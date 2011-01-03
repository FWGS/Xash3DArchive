//=======================================================================
//			Copyright XashXT Group 2008 ©
//		      common.h - shared header for all tools
//=======================================================================
#ifndef COMMON_H
#define COMMON_H

#include <windows.h>

typedef unsigned char	byte;
typedef unsigned short	word;

//
// filesystem.c
//
byte *FS_LoadFile( const char *filepath, size_t *filesize );
BOOL FS_SaveFile( const char *filepath, void *buffer, size_t filesize );
void FS_DefaultExtension( char *path, const char *extension );
void FS_FileBase( const char *in, char *out );
long FS_FileTime( const char *filename );
int FS_FileExists( const char *path );
void FS_StripExtension( char *path );

#endif//COMMON_H