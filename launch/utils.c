//=======================================================================
//			Copyright XashXT Group 2007 ©
//			utils.c - global system utils
//=======================================================================

#include "launch.h"
#include "wadfile.h"
#include "mathlib.h"
#include "const.h"

static long idum = 0;

#define MAX_RANDOM_RANGE	0x7FFFFFFFUL
#define IA		16807
#define IM		2147483647
#define IQ		127773
#define IR		2836
#define NTAB		32
#define NDIV		(1+(IM-1)/NTAB)
#define AM		(1.0/IM)
#define EPS		1.2e-7
#define RNMX		(1.0 - EPS)

void SeedRandomNumberGenerator( long lSeed )
{
	if( lSeed ) idum = lSeed;
	else idum = -time( NULL );

	if( 1000 < idum ) idum = -idum;
	else if( -1000 < idum ) idum -= 22261048;
}

long lran1( void )
{
	int	j;
	long	k;

	static long iy = 0;
	static long iv[NTAB];
	
	if( idum <= 0 || !iy )
	{
		if(-(idum) < 1) idum=1;
		else idum = -(idum);

		for( j = NTAB + 7; j >= 0; j-- )
		{
			k = (idum) / IQ;
			idum = IA * (idum - k * IQ) - IR * k;
			if( idum < 0 ) idum += IM;
			if( j < NTAB ) iv[j] = idum;
		}
		iy = iv[0];
	}
	k = (idum)/IQ;
	idum = IA * (idum - k * IQ) - IR * k;
	if( idum < 0 ) idum += IM;
	j = iy / NDIV;
	iy = iv[j];
	iv[j] = idum;

	return iy;
}

// fran1 -- return a random floating-point number on the interval [0,1)
float fran1( void )
{
	float temp = (float)AM * lran1();
	if( temp > RNMX ) return (float)RNMX;
	else return temp;
}

float Com_RandomFloat( float flLow, float flHigh )
{
	float	fl;

	if( idum == 0 ) SeedRandomNumberGenerator(0);

	fl = fran1(); // float in [0, 1)
	return (fl * (flHigh - flLow)) + flLow; // float in [low, high)
}

long Com_RandomLong( long lLow, long lHigh )
{
	dword	maxAcceptable;
	dword	n, x = lHigh-lLow + 1; 	

	if( idum == 0 ) SeedRandomNumberGenerator(0);

	if( x <= 0 || MAX_RANDOM_RANGE < x-1 )
		return lLow;

	// The following maps a uniform distribution on the interval [0, MAX_RANDOM_RANGE]
	// to a smaller, client-specified range of [0,x-1] in a way that doesn't bias
	// the uniform distribution unfavorably. Even for a worst case x, the loop is
	// guaranteed to be taken no more than half the time, so for that worst case x,
	// the average number of times through the loop is 2. For cases where x is
	// much smaller than MAX_RANDOM_RANGE, the average number of times through the
	// loop is very close to 1.
	maxAcceptable = MAX_RANDOM_RANGE - ((MAX_RANDOM_RANGE+1) % x );
	do
	{
		n = lran1();
	} while( n > maxAcceptable );

	return lLow + (n % x);
}

/*
=================
Com_HashKey

returns hash key for string
=================
*/
uint Com_HashKey( const char *string, uint hashSize )
{
	uint	i, hashKey = 0;

	for( i = 0; string[i]; i++ )
		hashKey = (hashKey + i) * 37 + com.tolower( string[i] );

	return (hashKey % hashSize);
}

void SinCos( float radians, float *sine, float *cosine )
{
	_asm
	{
		fld	dword ptr [radians]
		fsincos

		mov edx, dword ptr [cosine]
		mov eax, dword ptr [sine]

		fstp dword ptr [edx]
		fstp dword ptr [eax]
	}
}

float sse_sqrt( float x )
{
	float	root = 0.f;
	_asm
	{
		sqrtss	xmm0, x
		movss	root, xmm0
	}
	return root;
}

/*
=============================================================================

	LZSS COMPRESSION

=============================================================================
*/
#define REFERENCEMAXDIST		4096
#define REFERENCEMAXSIZE		18
#define PACKETMAXSYMBOLS		8

// this only needs 17 bytes (1+symbols*2) but is padded to a multiple of 8
#define PACKETMAXBYTES		24 
#define REFERENCEHASHBITS		12
#define REFERENCEHASHSIZE		(1 << REFERENCEHASHBITS)
#define MINWINDOWBUFFERSIZE		(REFERENCEMAXDIST + REFERENCEMAXSIZE * PACKETMAXSYMBOLS)

// WINDOWBUFFERSIZE must be >= REFERENCEMAXDIST+REFERENCEMAXSIZE*PACKETMAXSYMBOLS
#define WINDOWBUFFERSIZE		(REFERENCEMAXDIST * 2)
#define WINDOWBUFFERSIZE2		(WINDOWBUFFERSIZE*2)
#define HASHSIZE			(4096)

typedef struct lzss_state_s
{
	int	hashindex[HASHSIZE];	// contains hash indexes
	int	hashnext[WINDOWBUFFERSIZE2];	// contains hash indexes
	int	packetbit;
	int	packetsize;
	int	windowstart;
	int	windowposition;
	int	windowend;
	byte	packetbytes[PACKETMAXBYTES];
	byte	window[WINDOWBUFFERSIZE2];
} lzss_state_t;

void lzss_state_packetreset( lzss_state_t *state )
{
	state->packetbit = 0x80;	// current bit to set if encoding a reference
	state->packetsize = 1;	// size of packet
	state->packetbytes[0] = 0;	// command byte indicating contents of packet
}

void lzss_state_start( lzss_state_t *state )
{
	int	i;

	lzss_state_packetreset( state );
	state->windowstart = 0; // start of search window
	state->windowposition = 0; // current position in search window
	state->windowend = 0; // end of search window

	for( i = 0; i < HASHSIZE; i++ )
		state->hashindex[i] = -1;
}

// returns number of bytes needed to fill the buffer
uint lzss_state_wantbytes( lzss_state_t *state )
{
	return WINDOWBUFFERSIZE - ( state->windowend - state->windowstart );
}

// appends supplied bytes to buffer
// do not feed more bytes than lzss_state_wantbytes returned! (less is fine)
void lzss_state_feedbytes( lzss_state_t *state, const byte *in, uint inlength )
{
	int	i, pos;
	
	if( (int)inlength > WINDOWBUFFERSIZE - ( state->windowend - state->windowstart ))
		return; // error!

	while( inlength-- )
	{
		if( state->windowstart >= WINDOWBUFFERSIZE )
		{
			for( i = 0; i < HASHSIZE; i++ )
			{
				if( state->hashindex[i] >= state->windowstart )
				{
					state->hashindex[i] -= WINDOWBUFFERSIZE;
					pos = state->hashindex[i];
					state->hashnext[pos] = state->hashnext[pos + WINDOWBUFFERSIZE];

					while( state->hashnext[pos] >= state->windowstart )
					{
						state->hashnext[pos] -= WINDOWBUFFERSIZE;
						pos = state->hashnext[pos];
						state->hashnext[pos] = state->hashnext[pos + WINDOWBUFFERSIZE];
					}
					state->hashnext[pos] = -1;
				}
				else state->hashindex[i] = -1;
			}

			for( i = state->windowstart; i < state->windowend; i++ )
				state->window[i - WINDOWBUFFERSIZE] = state->window[i];

			state->windowstart -= WINDOWBUFFERSIZE;
			state->windowposition -= WINDOWBUFFERSIZE;
			state->windowend -= WINDOWBUFFERSIZE;
		}

		state->window[state->windowend] = *in;
		state->windowend++;
		in++;
	}
}

// compress some data if the buffer is sufficiently full or flush is true
void lzss_state_compress( lzss_state_t *state, int flush )
{
	int	w, l, maxl, bestl, bestcode, hash;
	byte	c, c1, c2;

	while( state->packetbit && ( maxl = ( state->windowend - state->windowposition )) >= ( flush ? 1 : REFERENCEMAXSIZE ))
	{
		if( maxl > REFERENCEMAXSIZE )
			maxl = REFERENCEMAXSIZE;

		c = state->window[state->windowposition];
		bestl = 1;

		if( maxl >= 3 && state->windowposition > state->windowstart )
		{
			c1 = state->window[state->windowposition+1];
			c2 = state->window[state->windowposition+2];

			for( w = state->hashindex[(c + c1 * 16 + c2 * 256) % HASHSIZE]; w >= state->windowstart; w = state->hashnext[w] )
			{
				if( w < state->windowposition && state->window[w] == c && state->window[w+1] == c1 && state->window[w+2] == c2 )
				{
					for( l = 3; l < maxl && state->window[w+l] == state->window[state->windowposition+l]; l++ );

					if( bestl < l )
					{
						bestl = l;
						bestcode = ((bestl - 3) << 12) | ( state->windowposition - w - 1 );

						if( bestl == maxl )
							break;
					}
				}
			}
		}

		if( bestl >= 3 )
		{
			state->packetbytes[0] |= state->packetbit;
			state->packetbytes[state->packetsize++] = (byte)(bestcode >> 8);
			state->packetbytes[state->packetsize++] = (byte)bestcode;
		}
		else state->packetbytes[state->packetsize++] = c;

		state->packetbit >>= 1;

		while( bestl-- )
		{
			// add hash entry
			if( state->windowposition + 3 <= state->windowend )
			{
				hash = (state->window[state->windowposition] + state->window[state->windowposition + 1] * 16 + state->window[state->windowposition + 2] * 256) % HASHSIZE;
				state->hashnext[state->windowposition] = state->hashindex[hash];
				state->hashindex[hash] = state->windowposition;
			}
			state->windowposition++;
		}

		if( state->windowstart < state->windowposition - REFERENCEMAXDIST )
			state->windowstart = state->windowposition - REFERENCEMAXDIST;
	}
}

uint lzss_state_packetfull( lzss_state_t *state )
{
	return !state->packetbit; 
}

uint lzss_state_getpacketsize( lzss_state_t *state )
{
	return state->packetsize >= 2 ? state->packetsize : 0;
}

void lzss_state_getpacketbytes( lzss_state_t *state, byte *out )
{
	int	i;

	// copy the bytes to output
	for( i = 0; i < state->packetsize; i++ )
		out[i] = state->packetbytes[i];

	// reset the packet
	lzss_state_packetreset( state );
}

uint lzss_compress( const byte *in, const byte *inend, byte *out, byte *outend )
{
	byte		*outstart = out;
	uint		b;
	lzss_state_t	state;

	lzss_state_start( &state );

	// this code is a little complex because it implements the flush stage as
	// just a few checks (otherwise it would take two copies of this code)

	// while the buffer is not empty, or there is more input
	while( state.windowposition != state.windowend || in != inend )
	{
		// keep compressing until it stops making new packets
		// (this means the buffer is not full enough anymore)
		// in == inend is setting the flush flag, which will finish the file,
		// and a packet is written if the packet is full or flush is true
		lzss_state_compress( &state, in == inend );

		b = lzss_state_getpacketsize( &state );
		if( in == inend ? b : lzss_state_packetfull( &state ))
		{
			// write a packet
			if( out + b > outend )
				return 0; // error: made file bigger

			lzss_state_getpacketbytes( &state, out );
			out += b;
		}
		else
		{
			// fill up buffer if needed
			if( in < inend && ( b = lzss_state_wantbytes( &state )))
			{
				if( b > (uint)( inend - in ))
					b = (uint)( inend - in );
				lzss_state_feedbytes( &state, in, b );
				in += b;
			}
		}
	}

	return out - outstart;
}

qboolean lzss_decompress( const byte *in, const byte *inend, byte *out, byte *outend )
{
	int		i, commandbyte, code;
	const byte	*copy;
	byte		*outcopyend, *outstart = out;

	// input file should not end with a command byte so make sure there are
	// at least two remaining bytes
	while( in + 2 <= inend && out < outend )
	{
		commandbyte = *in++;

		for( i = 0x80; i && in < inend && out < outend; i >>= 1 )
		{
			if( commandbyte & i )
			{
				code = (*in++) * 0x100;
				if( in == inend )
					return true; // corrupt

				code += *in++;
				outcopyend = out + ((code >> 12) & 15) + 3;
				copy = out - ((code & 0xFFF) + 1);

				if( out < outstart || outcopyend > outend )
					return true; // corrupt
				while( out < outcopyend )
					*out++ = *copy++;
			}
			else *out++ = *in++;
		}
	}

	// corrupt if non-zero
	return in != inend || out != outend;
}