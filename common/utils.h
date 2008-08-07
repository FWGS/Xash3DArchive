//=======================================================================
//			Copyright XashXT Group 2007 �
//			utils.h - shared engine utility
//=======================================================================
#ifndef UTILS_H
#define UTILS_H

#include <time.h>

/*
========================================================================

wavefile in memory representation

using with darkplaces video encoder-decoder
========================================================================
*/

typedef struct wavefile_s
{
	file_t *file;	// file this is reading from
	
	uint info_format;	// these settings are read directly from the wave format (1 is uncompressed PCM)
	uint info_rate;	// how many samples per second
	uint info_channels;	// how many channels (1 = mono, 2 = stereo, 6 = 5.1 audio?)
	uint info_bits;	// how many bits per channel (8 or 16)

			// these settings are generated from the wave format
			// how many bytes in a sample (which may be one or two channels, 
			// thus 1 or 2 or 2 or 4, depending on info_bytesperchannel)
	uint info_bytespersample;

			// how many bytes in channel (1 for 8bit, or 2 for 16bit)
	uint info_bytesperchannel;
	
	uint length;	// how many samples in the wave file
	uint datalength;	// how large the data chunk is
	uint dataposition;	// beginning of data in data chunk
	uint position;	// current position in stream (in samples)
	uint bufferlength;	// these are private to the wave file functions, just used for processing size of *buffer

	void *buffer;	// buffer is reallocated if caller asks for more than fits
} wavefile_t;

// bsplib compile flags
#define BSP_ONLYENTS	0x01
#define BSP_ONLYVIS		0x02
#define BSP_ONLYRAD		0x04
#define BSP_FULLCOMPILE	0x08

#define ALIGN( a ) a = (byte *)((int)((byte *)a + 3) & ~ 3)

extern unsigned __int64 __g_ProfilerStart;
extern unsigned __int64 __g_ProfilerEnd;
extern unsigned __int64 __g_ProfilerEnd2;
extern unsigned __int64 __g_ProfilerSpare;
extern unsigned __int64 __g_ProfilerTotalTicks;
extern double __g_ProfilerTotalMsec;
extern int com_argc;
extern char **com_argv;
extern byte *basepool;
extern byte *zonepool;

#define Profile_Start()\
{\
if (com.GameInfo->rdtsc) \
{ \
__asm pushad \
__asm rdtsc \
__asm mov DWORD PTR[__g_ProfilerStart+4], edx \
__asm mov DWORD PTR[__g_ProfilerStart], eax \
__asm popad \
} \
}

#define Profile_End()\
{\
if (GI.rdtsc) \
{ \
__asm pushad \
__asm rdtsc \
__asm mov DWORD PTR[__g_ProfilerEnd+4], edx \
__asm mov DWORD PTR[__g_ProfilerEnd], eax \
__asm popad \
__asm pushad \
__asm rdtsc \
__asm mov DWORD PTR[__g_ProfilerEnd2+4], edx \
__asm mov DWORD PTR[__g_ProfilerEnd2], eax \
__asm popad \
} \
}

void Profile_RatioResults( void );
void _Profile_Results( const char *function );
void Profile_Store( void );
void Profile_Time( void );	// total profile time
#define Profile_Results( name )  _Profile_Results( #name )

extern stdlib_api_t com;
extern vprogs_exp_t *PRVM;

#define Sys_Error			com.error
#define Malloc(size)		Mem_Alloc(basepool, size)  
#define BSP_Realloc(ptr, size)	Mem_Realloc(basepool, ptr, size)
#define Z_Malloc(size)		Mem_Alloc(zonepool, size)  

extern string gs_filename;
extern char gs_basedir[ MAX_SYSPATH ];
extern byte *error_bmp;
extern size_t error_bmp_size;

extern byte *studiopool;

enum
{
	QC_SPRITEGEN = 1,
	QC_STUDIOMDL,
	QC_ROQLIB,
	QC_WADLIB
};

bool Com_ValidScript( int scripttype );
void Com_CheckToken( const char *match );
void Com_Parse1DMatrix( int x, vec_t *m );
void Com_Parse2DMatrix( int y, int x, vec_t *m );
void Com_Parse3DMatrix( int z, int y, int x, vec_t *m );

// misc
bool CompileStudioModel ( byte *mempool, const char *name, byte parms );
bool CompileSpriteModel ( byte *mempool, const char *name, byte parms );
bool ConvertImagePixels ( byte *mempool, const char *name, byte parms );
bool CompileWad3Archive ( byte *mempool, const char *name, byte parms );
bool CompileROQVideo( byte *mempool, const char *name, byte parms );
bool PrepareBSPModel ( const char *dir, const char *name, byte params );
bool CompileBSPModel ( void );
#endif//UTILS_H