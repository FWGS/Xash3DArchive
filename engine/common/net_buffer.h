//=======================================================================
//			Copyright XashXT Group 2010 ©
//		    net_buffer.h - network message io functions
//=======================================================================
#ifndef NET_BUFFER_H
#define NET_BUFFER_H

// Pad a number so it lies on an N byte boundary.
// So PAD_NUMBER(0,4) is 0 and PAD_NUMBER(1,4) is 4
#define PAD_NUMBER( num, boundary )	((( num ) + (( boundary ) - 1 )) / ( boundary )) * ( boundary )

_inline int BitByte( int bits )
{
	return PAD_NUMBER( bits, 8 ) >> 3;
}

typedef struct bitbuf_s
{
	bool		bOverflow;	// overflow reading or writing
	const char	*pDebugName;	// buffer name (pointer to const name)

	byte		*pData;
	int		iCurBit;
	int		nDataBytes;
	int		nDataBits;
	size_t		nMaxSize;		// buffer maxsize
} bitbuf_t;

#define BF_WriteUBitLong( bf, data, bits )	BF_WriteUBitLongExt( bf, data, bits, true );
#define BF_StartReading			BF_StartWriting
#define BF_GetNumBytesRead			BF_GetNumBytesWritten
#define BF_ReadBitAngles			BF_ReadBitVec3Coord
#define BF_ReadString( bf )			BF_ReadStringExt( bf, false )
#define BF_ReadStringLine( bf )		BF_ReadStringExt( bf, true )
#define BF_Init( bf, name, data, bytes )	BF_InitExt( bf, name, data, bytes, -1 )

// common functions
void BF_InitExt( bitbuf_t *bf, const char *pDebugName, void *pData, int nBytes, int nMaxBits );
void BF_InitMasks( void );	// called once at startup engine
void BF_SeekToBit( bitbuf_t *bf, int bitPos );
void BF_SeekToByte( bitbuf_t *bf, int bytePos );
bool BF_CheckOverflow( bitbuf_t *bf );

// init writing
void BF_StartWriting( bitbuf_t *bf, void *pData, int nBytes, int iStartBit, int nBits );
void BF_Clear( bitbuf_t *bf );

// Bit-write functions
void BF_WriteOneBit( bitbuf_t *bf, int nValue );
void BF_WriteUBitLongExt( bitbuf_t *bf, uint curData, int numbits, bool bCheckRange );
void BF_WriteSBitLong( bitbuf_t *bf, int data, int numbits );
void BF_WriteBitLong( bitbuf_t *bf, uint data, int numbits, bool bSigned );
bool BF_WriteBits( bitbuf_t *bf, const void *pData, int nBits );
void BF_WriteBitAngle( bitbuf_t *bf, float fAngle, int numbits );
void BF_WriteBitCoord( bitbuf_t *bf, const float f );
void BF_WriteBitFloat( bitbuf_t *bf, float val );
void BF_WriteBitVec3Coord( bitbuf_t *bf, const float *fa );
void BF_WriteBitNormal( bitbuf_t *bf, float f );
void BF_WriteBitVec3Normal( bitbuf_t *bf, const float *fa );
void BF_WriteBitAngles( bitbuf_t *bf, const float *fa );

// Byte-write functions
void BF_WriteChar( bitbuf_t *bf, int val );
void BF_WriteByte( bitbuf_t *bf, int val );
void BF_WriteShort( bitbuf_t *bf, int val );
void BF_WriteWord( bitbuf_t *bf, int val );
void BF_WriteLong( bitbuf_t *bf, long val );
void BF_WriteFloat( bitbuf_t *bf, float val );
bool BF_WriteBytes( bitbuf_t *bf, const void *pBuf, int nBytes );	// same as MSG_WriteData
bool BF_WriteString(  bitbuf_t *bf, const char *pStr );		// returns false if it overflows the buffer.

// delta-write functions
bool BF_WriteDeltaMovevars( bitbuf_t *sb, movevars_t *from, movevars_t *cmd );

// helper functions
_inline int BF_GetNumBytesWritten( bitbuf_t *bf )	{ return BitByte( bf->iCurBit ); }
_inline int BF_GetNumBitsWritten( bitbuf_t *bf ) { return bf->iCurBit; }
_inline int BF_GetMaxBits( bitbuf_t *bf ) { return bf->nDataBits; }
_inline int BF_GetMaxBytes( bitbuf_t *bf ) { return bf->nDataBytes; }
_inline int BF_GetNumBitsLeft( bitbuf_t *bf ) { return bf->nDataBits - bf->iCurBit; }
_inline int BF_GetNumBytesLeft( bitbuf_t *bf ) { return BF_GetNumBitsLeft( bf ) >> 3; }
_inline byte *BF_GetData( bitbuf_t *bf ) { return bf->pData; }

// Bit-read functions
int BF_ReadOneBit( bitbuf_t *bf );
float BF_ReadBitFloat( bitbuf_t *bf );
bool BF_ReadBits( bitbuf_t *bf, void *pOutData, int nBits );
float BF_ReadBitAngle( bitbuf_t *bf, int numbits );
int BF_ReadSBitLong( bitbuf_t *bf, int numbits );
uint BF_ReadUBitLong( bitbuf_t *bf, int numbits );
uint BF_ReadBitLong( bitbuf_t *bf, int numbits, bool bSigned );
float BF_ReadBitCoord( bitbuf_t *bf );
void BF_ReadBitVec3Coord( bitbuf_t *bf, vec3_t fa );
float BF_ReadBitNormal( bitbuf_t *bf );
void BF_ReadBitVec3Normal( bitbuf_t *bf, vec3_t fa );

// Byte-read functions
int BF_ReadChar( bitbuf_t *bf );
int BF_ReadByte( bitbuf_t *bf );
int BF_ReadShort( bitbuf_t *bf );
int BF_ReadWord( bitbuf_t *bf );
long BF_ReadLong( bitbuf_t *bf );
float BF_ReadFloat( bitbuf_t *bf );
bool BF_ReadBytes( bitbuf_t *bf, void *pOut, int nBytes );
char *BF_ReadStringExt( bitbuf_t *bf, bool bLine );

// delta-read functions
void BF_ReadDeltaMovevars( bitbuf_t *sb, movevars_t *from, movevars_t *cmd );
					
#endif//NET_BUFFER_H