//=======================================================================
//			Copyright XashXT Group 2010 ©
//			filesystem.c - tools filesystem
//=======================================================================

#include <io.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "common.h"

/*
=============================================================================

MAIN FILESYSTEM ROUTINES

=============================================================================
*/
/*
============
FS_LoadFile
============
*/
byte *FS_LoadFile( const char *filepath, size_t *filesize )
{
	long		handle;
	size_t		size;
	unsigned char	*buf;

	handle = open( filepath, O_RDONLY|O_BINARY, 0666 );

	if( filesize ) *filesize = 0;
	if( handle < 0 ) return NULL;

	size = lseek( handle, 0, SEEK_END );
	lseek( handle, 0, SEEK_SET );

	buf = (unsigned char *)malloc( size + 1 );
	buf[size] = '\0';
	read( handle, (unsigned char *)buf, size );
	close( handle );

	if( filesize ) *filesize = size;
	return buf;	
}

/*
============
FS_SaveFile
============
*/
BOOL FS_SaveFile( const char *filepath, void *buffer, size_t filesize )
{
	long		handle;
	size_t		size;

	if( buffer == NULL || filesize <= 0 ) return FALSE;
	handle = open( filepath, O_WRONLY|O_BINARY|O_CREAT|O_TRUNC, 0666 );
	if( handle < 0 ) return FALSE;

	size = write( handle, buffer, filesize );
	close( handle );

	if( size < 0 ) return FALSE;
	return TRUE;
}

/*
==================
FS_FileExists
==================
*/
int FS_FileExists( const char *path )
{
	int	desc;
     
	desc = open( path, O_RDONLY|O_BINARY );
	if( desc < 0 ) return 0;
	close( desc );

	return 1;
}

/*
====================
FS_ileTime
====================
*/
long FS_FileTime( const char *filename )
{
	struct stat	buf;
	
	if( stat( filename, &buf ) == -1 )
		return -1;

	return buf.st_mtime;
}

/*
=============================================================================

OTHERS PUBLIC FUNCTIONS

=============================================================================
*/
/*
============
FS_StripExtension
============
*/
void FS_StripExtension( char *path )
{
	size_t	length;

	length = strlen( path ) - 1;
	while( length > 0 && path[length] != '.' )
	{
		length--;
		if( path[length] == '/' || path[length] == '\\' || path[length] == ':' )
			return; // no extension
	}
	if( length ) path[length] = 0;
}

/*
==================
FS_DefaultExtension
==================
*/
void FS_DefaultExtension( char *path, const char *extension )
{
	const char *src;

	// if path doesn't have a .EXT, append extension
	// (extension should include the .)
	src = path + strlen( path ) - 1;

	while( *src != '/' && src != path )
	{
		// it has an extension
		if( *src == '.' ) return;                 
		src--;
	}
	strcat( path, extension );
}

/*
============
FS_FileBase

Extracts the base name of a file (no path, no extension, assumes '/' as path separator)
============
*/
void FS_FileBase( const char *in, char *out )
{
	int	len, start, end;

	len = strlen( in );
	if( !len ) return;
	
	// scan backward for '.'
	end = len - 1;

	while( end && in[end] != '.' && in[end] != '/' && in[end] != '\\' )
		end--;
	
	if( in[end] != '.' )
		end = len-1; // no '.', copy to end
	else end--; // found ',', copy to left of '.'

	// scan backward for '/'
	start = len - 1;

	while( start >= 0 && in[start] != '/' && in[start] != '\\' )
		start--;

	if( start < 0 || ( in[start] != '/' && in[start] != '\\' ))
		start = 0;
	else start++;

	// length of new sting
	len = end - start + 1;

	// Copy partial string
	strncpy( out, &in[start], len + 1 );
	out[len] = 0;
}