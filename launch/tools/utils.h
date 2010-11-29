//=======================================================================
//			Copyright XashXT Group 2007 ©
//			 utils.h - shared utilities
//=======================================================================
#ifndef UTILS_H
#define UTILS_H

#include <time.h>
#include <windows.h>
#include "launch_api.h"

extern qboolean enable_log;
extern stdlib_api_t com;
extern file_t *bsplog;

#define Realloc( ptr, size )		Mem_Realloc( Sys.basepool, ptr, size )

extern string gs_filename;
extern string gs_basedir;
extern byte *error_bmp;
extern size_t error_bmp_size;
extern char** com_argv;

typedef enum
{
	QC_SPRITEGEN = 1,
	QC_STUDIOMDL,
	QC_WADLIB
} qctype_t;

// misc
qboolean Com_ValidScript( const char *token, qctype_t script_type );
qboolean CompileStudioModel( byte *mempool, const char *name, byte parms );
qboolean CompileSpriteModel( byte *mempool, const char *name, byte parms );
qboolean CompileWad3Archive( byte *mempool, const char *name, byte parms );
qboolean PrepareBSPModel( int argc, char **argv );
qboolean CompileBSPModel( void );

//=====================================
//	extragen export
//=====================================
qboolean ConvertResource( byte *mempool, const char *filename, byte parms );
void Bsp_PrintLog( const char *pMsg );
void Skin_FinalizeScript( void );
void Conv_RunSearch( void );

// shared tools
void ClrMask( void );
void AddMask( const char *mask );
extern string searchmask[];
extern int num_searchmask;

#endif//UTILS_H