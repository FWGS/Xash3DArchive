//=======================================================================
//			Copyright XashXT Group 2007 ©
//			 utils.h - shared utilities
//=======================================================================
#ifndef UTILS_H
#define UTILS_H

#include <time.h>

// bsplib compile flags
#define ALIGN( a ) a = (byte *)((int)((byte *)a + 3) & ~ 3)

extern byte *basepool;
extern byte *zonepool;
extern qboolean enable_log;
extern stdlib_api_t com;
extern file_t *bsplog;

#define Sys_Error			com.error
#define Malloc( size )		Mem_Alloc( basepool, size )  
#define Realloc( ptr, size )		Mem_Realloc( basepool, ptr, size )

extern string gs_filename;
extern string gs_basedir;
extern byte *error_bmp;
extern size_t error_bmp_size;
extern char** com_argv;

typedef enum
{
	QC_SPRITEGEN = 1,
	QC_STUDIOMDL,
	QC_ROQLIB,
	QC_WADLIB
} qctype_t;

qboolean Com_ValidScript( const char *token, qctype_t script_type );
float ColorNormalize( const vec3_t in, vec3_t out );
void NormalToLatLong( const vec3_t normal, byte bytes[2] );

// misc
qboolean CompileStudioModel( byte *mempool, const char *name, byte parms );
qboolean CompileSpriteModel( byte *mempool, const char *name, byte parms );
qboolean CompileWad3Archive( byte *mempool, const char *name, byte parms );
qboolean ConvertImages( byte *mempool, const char *name, byte parms );
qboolean PrepareBSPModel( int argc, char **argv );
qboolean CompileBSPModel( void );

#endif//UTILS_H