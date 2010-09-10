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

#define ST_STATIC_ALLOCATE		// comment this to use realloc
// (all pointers to real strings will be invalid after another calling SetString, but it - memory economy mode )

typedef struct stringtable_s
{
	string	name;		// system name (for debug purposes)
	byte	*mempool;		// system mempool
	char	*data;		// buffer with strings
	size_t	datasize;		// current buffsize
	size_t	maxdatasize;	// dynamically resized

	int	*table;		// indexes at begining string
	size_t	numstrings;	// current strings count
	size_t	maxstrings;	// current system limit
} stringtable_t;

stringtable_t *dstring[MAX_STRING_TABLES];

bool StringTable_CheckHandle( int handle, bool silent )
{
	if( handle < 0 || handle > MAX_STRING_TABLES )
	{
		if( !silent )
			MsgDev( D_ERROR, "StringTable_CheckHandle: invalid system handle %d\n", handle );
		return false;
	}
	if( !dstring[handle] )
	{
		if( !silent )
			MsgDev( D_ERROR, "StringTable_CheckHandle: system with handle %d inactive\n", handle );
		return false;
	}
	return true;
}

bool StringTable_CheckString( int handle, string_t str )
{
	if(!StringTable_CheckHandle( handle, true ))
		return false;

	if( str < 0 || str >= dstring[handle]->numstrings )
	{
		MsgDev( D_ERROR, "StringTable( %s ): invalid string index %i\n", dstring[handle]->name, str );
		return false;
	}
	return true;
}

const char *StringTable_GetName( int handle )
{
	if( !StringTable_CheckHandle( handle, true ))
		return NULL;	
	return dstring[handle]->name;
}

int StringTable_CreateNewSystem( const char *name, size_t max_strings )
{
	int	i;

	// fisrt, find free stringtable system
	for( i = 0; i < MAX_STRING_TABLES; i++ )
	{
		if( !dstring[i] )
		{
			// found free slot
			dstring[i] = Mem_Alloc( Sys.basepool, sizeof( stringtable_t ));
			dstring[i]->mempool = Mem_AllocPool( va( "StringTable_%s", name ));
			com.strncpy( dstring[i]->name, name, sizeof( dstring[i]->name ));
			dstring[i]->maxdatasize = max_strings * 8;
			dstring[i]->maxstrings = max_strings;
#ifdef ST_STATIC_ALLOCATE
			// create static arrays
			dstring[i]->data = (char *)Mem_Alloc( dstring[i]->mempool, dstring[i]->maxdatasize );
			dstring[i]->table = (int *)Mem_Alloc( dstring[i]->mempool, dstring[i]->maxstrings );
#endif

			StringTable_SetString( i, "" ); // make iNullString
			return i;
		}
	}

	MsgDev( D_ERROR, "StringTable_CreateNewSystem: no free string systems\n" );
	return -1;
}

void StringTable_DeleteSystem( int handle )
{
	if( !StringTable_CheckHandle( handle, false ))
		return;

	// now free stringtable
	Mem_FreePool( &dstring[handle]->mempool );
	Mem_Free( dstring[handle] ); // free himself
	dstring[handle] = NULL;
}

void StringTable_ClearSystem( int handle )
{
	if( !StringTable_CheckHandle( handle, false ))
		return;

	Mem_EmptyPool( dstring[handle]->mempool );
	dstring[handle]->datasize = dstring[handle]->numstrings = 0;
#ifdef ST_STATIC_ALLOCATE
	dstring[handle]->data = (char *)Mem_Alloc( dstring[handle]->mempool, dstring[handle]->maxdatasize );
	dstring[handle]->table = (int *)Mem_Alloc( dstring[handle]->mempool, dstring[handle]->maxstrings );
#else
	dstring[handle]->table = NULL;
	dstring[handle]->data = NULL;
#endif
	StringTable_SetString( handle, "" ); // make iNullString
}

const char *StringTable_GetString( int handle, string_t index )
{
	if(!StringTable_CheckString( handle, index )) return "";
	return &dstring[handle]->data[dstring[handle]->table[index]];
}

string_t StringTable_SetString( int handle, const char *string )
{
	int	i, len, table_size, data_size;

	if( !StringTable_CheckHandle( handle, false ))
		return -1;

	for( i = 0; i < dstring[handle]->numstrings; i++ )
	{
		if(!com.strcmp( string, StringTable_GetString( handle, i )))
			return i; // already existing
	}

	// register new string
	len = com.strlen( string );
	table_size = sizeof(string_t) * (dstring[handle]->numstrings + 1);
	data_size = dstring[handle]->datasize + len + 1;

	if( table_size >= dstring[handle]->maxstrings || data_size >= dstring[handle]->maxdatasize )
	{
		MsgDev( D_ERROR, "StringTable_SetString: string table %s limit exeeded\n", dstring[handle]->name );
		return -1;
	}
#ifndef ST_STATIC_ALLOCATE
	dstring[handle]->table = Mem_Realloc( dstring[handle]->mempool, dstring[handle]->table, table_size );
	dstring[handle]->data = Mem_Realloc( dstring[handle]->mempool, dstring[handle]->data, data_size );  
#endif
	com.strcpy( &dstring[handle]->data[dstring[handle]->datasize], string );
	dstring[handle]->table[dstring[handle]->numstrings] = dstring[handle]->datasize;
	dstring[handle]->datasize += len + 1; // null terminator
	dstring[handle]->numstrings++;

	return dstring[handle]->numstrings - 1; // current index
}

bool StringTable_SaveSystem( int h, wfile_t *wad )
{
	int table_size;

	if(!StringTable_CheckHandle( h, false ))
		return false;
	if(!W_SaveLump( wad, "stringdata", dstring[h]->data, dstring[h]->datasize, TYP_RAW, CMP_LZSS ))
		return false;
	table_size = dstring[h]->numstrings * sizeof( string_t );
	if( !W_SaveLump( wad, "stringtable", dstring[h]->table, table_size, TYP_RAW, CMP_LZSS ))
		return false;
	return true;
}

int StringTable_LoadSystem( wfile_t *wad, const char *name )
{
	int datasize, table_size;
	int h = StringTable_CreateNewSystem( name, 0x10000 ); // 65535 unique strings
	char *data = (char *)W_LoadLump( wad, "stringdata", &datasize, TYP_RAW );
	int *table = (int *)W_LoadLump( wad, "stringtable", &table_size, TYP_RAW );

	if(( datasize > dstring[h]->maxdatasize ) || ((table_size / sizeof( int )) > dstring[h]->maxstrings ))
		Sys_Error( "Too small StringTable for loading\n" );

#ifndef ST_STATIC_ALLOCATE
	dstring[h]->data = Mem_Realloc( dstring[handle]->mempool, dstring[handle]->data, datasize );	
	dstring[h]->table = Mem_Realloc( dstring[handle]->mempool, dstring[handle]->table, table_size );
#endif
	Mem_Copy( dstring[h]->data, data, datasize );
	Mem_Copy( dstring[h]->table, table, table_size ); 
	dstring[h]->datasize = datasize;
	dstring[h]->numstrings = table_size / sizeof( int );
	return h;
}

void StringTable_Info_f( void )
{
	int	i, j;

	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: stringlist <name>\n" );
		return;
	}

	// print all strings in selected StringTable
	for( i = 0; i < MAX_STRING_TABLES; i++ )
	{
		if( !dstring[i] ) continue;

		if( !com.stricmp( dstring[i]->name, Cmd_Argv( 1 )))
		{
			Msg( "------------- %i strings -------------\n", dstring[i]->numstrings );
			for( j = 0; j < dstring[i]->numstrings; j++ )
				Msg( "%s ", StringTable_GetString( i, j ));
			Msg( "\n  ^3total %s used\n", com.pretifymem( dstring[i]->datasize, 3 ));
			break;
		}
	}
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

bool lzss_decompress( const byte *in, const byte *inend, byte *out, byte *outend )
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