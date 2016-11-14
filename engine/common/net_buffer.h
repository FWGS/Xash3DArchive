/*
net_buffer.h - network message io functions
Copyright (C) 2010 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef NET_BUFFER_H
#define NET_BUFFER_H

#include "features.h"

/*
==============================================================================

			MESSAGE IO FUNCTIONS
	       Handles byte ordering and avoids alignment errors
==============================================================================
*/

// Pad a number so it lies on an N byte boundary.
// So PAD_NUMBER(0,4) is 0 and PAD_NUMBER(1,4) is 4
#define PAD_NUMBER( num, boundary )	((( num ) + (( boundary ) - 1 )) / ( boundary )) * ( boundary )

_inline int BitByte( int bits )
{
	return PAD_NUMBER( bits, 8 ) >> 3;
}

typedef struct sizebuf_s
{
	qboolean		bOverflow;	// overflow reading or writing
	const char	*pDebugName;	// buffer name (pointer to const name)

	byte		*pData;
	int		iCurBit;
	int		nDataBits;
} sizebuf_t;

#define MSG_WriteUBitLong( bf, data, bits )	MSG_WriteUBitLongExt( bf, data, bits, true );
#define MSG_StartReading			MSG_StartWriting
#define MSG_GetNumBytesRead			MSG_GetNumBytesWritten
#define MSG_GetRealBytesRead			MSG_GetRealBytesWritten
#define MSG_GetNumBitsRead			MSG_GetNumBitsWritten
#define MSG_ReadBitAngles			MSG_ReadBitVec3Coord
#define MSG_ReadString( bf )			MSG_ReadStringExt( bf, false )
#define MSG_ReadStringLine( bf )		MSG_ReadStringExt( bf, true )
#define MSG_ReadAngle( bf )			(float)(MSG_ReadChar( bf ) * ( 360.0f / 256.0f ))
#define MSG_Init( bf, name, data, bytes )	MSG_InitExt( bf, name, data, bytes, -1 )

// common functions
void MSG_InitExt( sizebuf_t *bf, const char *pDebugName, void *pData, int nBytes, int nMaxBits );
void MSG_InitMasks( void );	// called once at startup engine
void MSG_SeekToBit( sizebuf_t *bf, int bitPos );
void MSG_SeekToByte( sizebuf_t *bf, int bytePos );
void MSG_ExciseBits( sizebuf_t *bf, int startbit, int bitstoremove );
qboolean MSG_CheckOverflow( sizebuf_t *bf );
short MSG_BigShort( short swap );

// init writing
void MSG_StartWriting( sizebuf_t *bf, void *pData, int nBytes, int iStartBit, int nBits );
void MSG_Clear( sizebuf_t *bf );

// Bit-write functions
void MSG_WriteOneBit( sizebuf_t *bf, int nValue );
void MSG_WriteUBitLongExt( sizebuf_t *bf, uint curData, int numbits, qboolean bCheckRange );
void MSG_WriteSBitLong( sizebuf_t *bf, int data, int numbits );
void MSG_WriteBitLong( sizebuf_t *bf, uint data, int numbits, qboolean bSigned );
qboolean MSG_WriteBits( sizebuf_t *bf, const void *pData, int nBits );
void MSG_WriteBitAngle( sizebuf_t *bf, float fAngle, int numbits );
void MSG_WriteBitFloat( sizebuf_t *bf, float val );

// Byte-write functions
void MSG_WriteChar( sizebuf_t *bf, int val );
void MSG_WriteByte( sizebuf_t *bf, int val );
void MSG_WriteShort( sizebuf_t *bf, int val );
void MSG_WriteWord( sizebuf_t *bf, int val );
void MSG_WriteLong( sizebuf_t *bf, long val );
void MSG_WriteCoord( sizebuf_t *bf, float val );
void MSG_WriteFloat( sizebuf_t *bf, float val );
void MSG_WriteVec3Coord( sizebuf_t *bf, const float *fa );
qboolean MSG_WriteBytes( sizebuf_t *bf, const void *pBuf, int nBytes );	// same as MSG_WriteData
qboolean MSG_WriteString( sizebuf_t *bf, const char *pStr );		// returns false if it overflows the buffer.

// helper functions
_inline int MSG_GetNumBytesWritten( sizebuf_t *bf ) { return BitByte( bf->iCurBit ); }
_inline int MSG_GetRealBytesWritten( sizebuf_t *bf ) { return bf->iCurBit >> 3; }	// unpadded
_inline int MSG_GetNumBitsWritten( sizebuf_t *bf ) { return bf->iCurBit; }
_inline int MSG_GetMaxBits( sizebuf_t *bf ) { return bf->nDataBits; }
_inline int MSG_GetMaxBytes( sizebuf_t *bf ) { return bf->nDataBits >> 3; }
_inline int MSG_GetNumBitsLeft( sizebuf_t *bf ) { return bf->nDataBits - bf->iCurBit; }
_inline int MSG_GetNumBytesLeft( sizebuf_t *bf ) { return MSG_GetNumBitsLeft( bf ) >> 3; }
_inline byte *MSG_GetData( sizebuf_t *bf ) { return bf->pData; }

// Bit-read functions
int MSG_ReadOneBit( sizebuf_t *bf );
float MSG_ReadBitFloat( sizebuf_t *bf );
qboolean MSG_ReadBits( sizebuf_t *bf, void *pOutData, int nBits );
float MSG_ReadBitAngle( sizebuf_t *bf, int numbits );
int MSG_ReadSBitLong( sizebuf_t *bf, int numbits );
uint MSG_ReadUBitLong( sizebuf_t *bf, int numbits );
uint MSG_ReadBitLong( sizebuf_t *bf, int numbits, qboolean bSigned );

// Byte-read functions
int MSG_ReadChar( sizebuf_t *bf );
int MSG_ReadByte( sizebuf_t *bf );
int MSG_ReadShort( sizebuf_t *bf );
int MSG_ReadWord( sizebuf_t *bf );
long MSG_ReadLong( sizebuf_t *bf );
float MSG_ReadCoord( sizebuf_t *bf );
float MSG_ReadFloat( sizebuf_t *bf );
void MSG_ReadVec3Coord( sizebuf_t *bf, vec3_t fa );
qboolean MSG_ReadBytes( sizebuf_t *bf, void *pOut, int nBytes );
char *MSG_ReadStringExt( sizebuf_t *bf, qboolean bLine );
					
#endif//NET_BUFFER_H