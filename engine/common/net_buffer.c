//=======================================================================
//			Copyright XashXT Group 2010 ©
//		    net_buffer.c - network bitbuffer io functions
//=======================================================================

#include "common.h"
#include "net_buffer.h"

// precalculated bit masks for WriteUBitLong.
// Using these tables instead of doing the calculations
// gives a 33% speedup in WriteUBitLong.

static dword	BitWriteMasks[32][33];
static dword	ExtraMasks[32];

void BF_InitMasks( void )
{
	uint	startbit, endbit;
	uint	maskBit, nBitsLeft;

	for( startbit = 0; startbit < 32; startbit++ )
	{
		for( nBitsLeft = 0; nBitsLeft < 33; nBitsLeft++ )
		{
			endbit = startbit + nBitsLeft;

			BitWriteMasks[startbit][nBitsLeft] = BIT( startbit ) - 1;
			if( endbit < 32 ) BitWriteMasks[startbit][nBitsLeft] |= ~(BIT( endbit ) - 1 );
		}
	}

	for( maskBit = 0; maskBit < 32; maskBit++ )
		ExtraMasks[maskBit] = BIT( maskBit ) - 1;
}
 
void BF_Init( bitbuf_t *bf, const char *pDebugName, void *pData, int nBytes, int nMaxBits )
{
	bf->pDebugName = pDebugName;

	BF_StartWriting( bf, pData, nBytes, 0, nMaxBits );
}

void BF_StartWriting( bitbuf_t *bf, void *pData, int nBytes, int iStartBit, int nBits )
{
	// Make sure it's dword aligned and padded.
	ASSERT(( nBytes % 4 ) == 0 );
	ASSERT(((dword)pData & 3 ) == 0 );

	bf->pData = (byte *)pData;
	bf->nDataBytes = nBytes;

	if( nBits == -1 )
	{
		bf->nDataBits = nBytes << 3;
	}
	else
	{
		ASSERT( nBits <= nBytes * 8 );
		bf->nDataBits = nBits;
	}

	bf->iCurBit = iStartBit;
	bf->bOverflow = false;
}

void BF_Clear( bitbuf_t *bf )
{
	bf->iCurBit = 0;
	bf->bOverflow = false;
}

static bool BF_Overflow( bitbuf_t *bf, int nBits )
{
	if( bf->iCurBit + nBits > bf->nDataBits )
	{
		bf->bOverflow = true;
		MsgDev( D_ERROR, "Msg %s: overflow!\n", bf->pDebugName );
	}
	return bf->bOverflow;
}

void BF_SeekToBit( bitbuf_t *bf, int bitPos )
{
	bf->iCurBit = bitPos;
}

void BF_WriteOneBit( bitbuf_t *bf, int nValue )
{
	if( !BF_Overflow( bf, 1 ))
	{
		if( nValue ) bf->pData[bf->iCurBit>>3] |= (1 << ( bf->iCurBit & 7 ));
		else bf->pData[bf->iCurBit>>3] &= ~(1 << ( bf->iCurBit & 7 ));

		bf->iCurBit++;
	}
}

void BF_WriteUBitLongExt( bitbuf_t *bf, uint curData, int numbits, bool bCheckRange )
{
#ifdef _DEBUG
	// make sure it doesn't overflow.
	if( bCheckRange && numbits < 32 )
	{
		if( curData >= (unsigned long)BIT( numbits ))
			MsgDev( D_ERROR, "Msg %s: out of range value!\n", bf->pDebugName );
	}
	ASSERT( numbits >= 0 && numbits <= 32 );
#endif

	// bounds checking..
	if(( bf->iCurBit + numbits ) > bf->nDataBits )
	{
		bf->bOverflow = true;
		bf->iCurBit = bf->nDataBits;
		MsgDev( D_ERROR, "Msg %s: overflow!\n", bf->pDebugName );
	}
	else
	{
		int	nBitsLeft = numbits;
		int	iCurBit = bf->iCurBit;
		uint	iDWord = iCurBit >> 5;	// Mask in a dword.
		dword	iCurBitMasked;
		int	nBitsWritten;

		ASSERT(( iDWord * 4 + sizeof( long )) <= (uint)bf->nDataBytes );

		iCurBitMasked = iCurBit & 31;
		((dword *)bf->pData)[iDWord] &= BitWriteMasks[iCurBitMasked][nBitsLeft];
		((dword *)bf->pData)[iDWord] |= curData << iCurBitMasked;

		// did it span a dword?
		nBitsWritten = 32 - iCurBitMasked;

		if( nBitsWritten < nBitsLeft )
		{
			nBitsLeft -= nBitsWritten;
			iCurBit += nBitsWritten;
			curData >>= nBitsWritten;

			iCurBitMasked = iCurBit & 31;
			((dword *)bf->pData)[iDWord+1] &= BitWriteMasks[iCurBitMasked][nBitsLeft];
			((dword *)bf->pData)[iDWord+1] |= curData << iCurBitMasked;
		}
		bf->iCurBit += numbits;
	}
}

/*
=======================
BF_WriteSBitLong

sign bit comes first
=======================
*/
void BF_WriteSBitLong( bitbuf_t *bf, int data, int numbits )
{
	// do we have a valid # of bits to encode with?
	ASSERT( numbits >= 1 );

	// NOTE: it does this wierdness here so it's bit-compatible with regular integer data in the buffer.
	// (Some old code writes direct integers right into the buffer).
	if( data < 0 )
	{
#ifdef _DEBUG
	if( numbits < 32 )
	{
		// Make sure it doesn't overflow.
		if( data < 0 )
		{
			ASSERT( data >= -BIT( numbits - 1 ));
		}
		else
		{
			ASSERT( data < BIT( numbits - 1 ));
		}
	}
#endif

		BF_WriteUBitLongExt( bf, (uint)( 0x80000000 + data ), numbits - 1, false );
		BF_WriteOneBit( bf, 1 );
	}
	else
	{
		BF_WriteUBitLong( bf, (uint)data, numbits - 1 );
		BF_WriteOneBit( bf, 0 );
	}
}

void BF_WriteBitLong( bitbuf_t *bf, uint data, int numbits, bool bSigned )
{
	if( bSigned )
		BF_WriteSBitLong( bf, (int)data, numbits );
	else BF_WriteUBitLong( bf, data, numbits );
}

bool BF_WriteBits( bitbuf_t *bf, const void *pData, int nBits )
{
	byte	*pOut = (byte *)pData;
	int	nBitsLeft = nBits;

	// get output dword-aligned.
	while((( dword )pOut & 3 ) != 0 && nBitsLeft >= 8 )
	{
		BF_WriteUBitLongExt( bf, *pOut, 8, false );

		nBitsLeft -= 8;
		++pOut;
	}

	// read dwords.
	while( nBitsLeft >= 32 )
	{
		BF_WriteUBitLongExt( bf, *(( dword *)pOut ), 32, false );

		pOut += sizeof( dword );
		nBitsLeft -= 32;
	}

	// read the remaining bytes.
	while( nBitsLeft >= 8 )
	{
		BF_WriteUBitLongExt( bf, *pOut, 8, false );

		nBitsLeft -= 8;
		++pOut;
	}
	
	// Read the remaining bits.
	if( nBitsLeft )
	{
		BF_WriteUBitLongExt( bf, *pOut, nBitsLeft, false );
	}

	return !bf->bOverflow;
}


void BF_WriteBitAngle( bitbuf_t *bf, float fAngle, int numbits )
{
	uint	mask, shift;
	int	d;

	shift = ( 1 << numbits );
	mask = shift - 1;

	d = (int)( fAngle * shift ) / 360;
	d &= mask;

	BF_WriteUBitLong( bf, (uint)d, numbits );
}

void BF_WriteBitCoord( bitbuf_t *bf, const float f )
{
	int	signbit = ( f <= -COORD_RESOLUTION );
	int	fractval = abs(( int )( f * COORD_DENOMINATOR )) & ( COORD_DENOMINATOR - 1 );
	int	intval = (int)abs( f );

	// Send the bit flags that indicate whether we have an integer part and/or a fraction part.
	BF_WriteOneBit( bf, intval );
	BF_WriteOneBit( bf, fractval );

	if( intval || fractval )
	{
		// send the sign bit
		BF_WriteOneBit( bf, signbit );

		// send the integer if we have one.
		if( intval )
		{
			// adjust the integers from [1..MAX_COORD_VALUE] to [0..MAX_COORD_VALUE-1]
			intval--;
			BF_WriteUBitLong( bf, (uint)intval, COORD_INTEGER_BITS );
		}
		
		// send the fraction if we have one
		if( fractval )
		{
			BF_WriteUBitLong( bf, (uint)fractval, COORD_FRACTIONAL_BITS );
		}
	}
}

void BF_WriteBitFloat( bitbuf_t *bf, float val )
{
	long	intVal;

	ASSERT( sizeof( long ) == sizeof( float ));
	ASSERT( sizeof( float ) == 4 );

	intVal = *((long *)&val );
	BF_WriteUBitLong( bf, intVal, 32 );
}

void BF_WriteBitVec3Coord( bitbuf_t *bf, const float *fa )
{
	int	xflag, yflag, zflag;

	xflag = ( fa[0] >= COORD_RESOLUTION ) || ( fa[0] <= -COORD_RESOLUTION );
	yflag = ( fa[1] >= COORD_RESOLUTION ) || ( fa[1] <= -COORD_RESOLUTION );
	zflag = ( fa[2] >= COORD_RESOLUTION ) || ( fa[2] <= -COORD_RESOLUTION );

	BF_WriteOneBit( bf, xflag );
	BF_WriteOneBit( bf, yflag );
	BF_WriteOneBit( bf, zflag );

	if( xflag ) BF_WriteBitCoord( bf, fa[0] );
	if( yflag ) BF_WriteBitCoord( bf, fa[1] );
	if( zflag ) BF_WriteBitCoord( bf, fa[2] );
}

void BF_WriteBitNormal( bitbuf_t *bf, float f )
{
	int	signbit = ( f <= -NORMAL_RESOLUTION );
	uint	fractval = abs(( int )(f * NORMAL_DENOMINATOR ));

	if( fractval > NORMAL_DENOMINATOR )
		fractval = NORMAL_DENOMINATOR;

	// send the sign bit
	BF_WriteOneBit( bf, signbit );

	// send the fractional component
	BF_WriteUBitLong( bf, fractval, NORMAL_FRACTIONAL_BITS );
}

void BF_WriteBitVec3Normal( bitbuf_t *bf, const float *fa )
{
	int	xflag, yflag;
	int	signbit;

	xflag = ( fa[0] >= NORMAL_RESOLUTION ) || ( fa[0] <= -NORMAL_RESOLUTION );
	yflag = ( fa[1] >= NORMAL_RESOLUTION ) || ( fa[1] <= -NORMAL_RESOLUTION );

	BF_WriteOneBit( bf, xflag );
	BF_WriteOneBit( bf, yflag );

	if( xflag ) BF_WriteBitNormal( bf, fa[0] );
	if( yflag ) BF_WriteBitNormal( bf, fa[1] );
	
	// Write z sign bit
	signbit = ( fa[2] <= -NORMAL_RESOLUTION );
	BF_WriteOneBit( bf, signbit );
}

void BF_WriteBitAngles( bitbuf_t *bf, const float *fa )
{
	// FIXME: uses WriteBitAngle instead ?
	BF_WriteBitVec3Coord( bf, fa );
}

void BF_WriteChar( bitbuf_t *bf, int val )
{
	BF_WriteSBitLong( bf, val, sizeof( char ) << 3 );
}

void BF_WriteByte( bitbuf_t *bf, int val )
{
	BF_WriteUBitLong( bf, val, sizeof( byte ) << 3 );
}

void BF_WriteShort( bitbuf_t *bf, int val )
{
	BF_WriteSBitLong( bf, val, sizeof(short ) << 3 );
}

void BF_WriteWord( bitbuf_t *bf, int val )
{
	BF_WriteUBitLong( bf, val, sizeof( word ) << 3 );
}

void BF_WriteLong( bitbuf_t *bf, long val )
{
	BF_WriteSBitLong( bf, val, sizeof( long ) << 3 );
}

void BF_WriteFloat( bitbuf_t *bf, float val )
{
	BF_WriteBits( bf, &val, sizeof( val ) << 3 );
}

bool BF_WriteBytes( bitbuf_t *bf, const void *pBuf, int nBytes )
{
	return BF_WriteBits( bf, pBuf, nBytes << 3 );
}

bool BF_WriteString( bitbuf_t *bf, const char *pStr )
{
	if( pStr )
	{
		do
		{
			BF_WriteChar( bf, *pStr );
			++pStr;
		} while(*( pStr - 1 ) != 0 );
	}
	else BF_WriteChar( bf, 0 );

	return !bf->bOverflow;
}