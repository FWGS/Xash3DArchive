/*
filesystem.c - game filesystem based on DP fs
Copyright (C) 2007 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <fcntl.h>
#include <direct.h>
#include <sys/stat.h>
#include <io.h>
#include <time.h>
#include "common.h"
#include "wadfile.h"
#include "filesystem.h"
#include "library.h"
#include "mathlib.h"

#define FILE_COPY_SIZE		(1024 * 1024)
#define FILE_BUFF_SIZE		(65535)

// PAK errors
#define PAK_LOAD_OK			0
#define PAK_LOAD_COULDNT_OPEN		1
#define PAK_LOAD_BAD_HEADER		2
#define PAK_LOAD_BAD_FOLDERS		3
#define PAK_LOAD_TOO_MANY_FILES	4
#define PAK_LOAD_NO_FILES		5
#define PAK_LOAD_CORRUPTED		6

// WAD errors
#define WAD_LOAD_OK			0
#define WAD_LOAD_COULDNT_OPEN		1
#define WAD_LOAD_BAD_HEADER		2
#define WAD_LOAD_BAD_FOLDERS		3
#define WAD_LOAD_TOO_MANY_FILES	4
#define WAD_LOAD_NO_FILES		5
#define WAD_LOAD_CORRUPTED		6

typedef struct stringlist_s
{
	// maxstrings changes as needed, causing reallocation of strings[] array
	int		maxstrings;
	int		numstrings;
	char		**strings;
} stringlist_t;

typedef struct wadtype_s
{
	char		*ext;
	char		type;
} wadtype_t;

typedef struct file_s
{
	int		handle;			// file descriptor
	long		real_length;		// uncompressed file size (for files opened in "read" mode)
	long		position;			// current position in the file
	long		offset;			// offset into the package (0 if external file)
	int		ungetc;			// single stored character from ungetc, cleared to EOF when read
	time_t		filetime;			// pak, wad or real filetime
						// contents buffer
	long		buff_ind, buff_len;		// buffer current index and length
	byte		buff[FILE_BUFF_SIZE];	// intermediate buffer
};

typedef struct wfile_s
{
	string		filename;
	int		infotableofs;
	byte		*mempool;	// W_ReadLump temp buffers
	int		numlumps;
	int		mode;
	file_t		*handle;
	dlumpinfo_t	*lumps;
	time_t		filetime;
};

typedef struct pack_s
{
	string		filename;
	int		handle;
	int		numfiles;
	time_t		filetime;	// common for all packed files
	dpackfile_t	*files;
} pack_t;

typedef struct searchpath_s
{
	string		filename;
	pack_t		*pack;
	wfile_t		*wad;
	int		flags;
	struct searchpath_s *next;
} searchpath_t;

byte			*fs_mempool;
searchpath_t		*fs_searchpaths = NULL;	// chain
searchpath_t		fs_directpath;		// static direct path
char			fs_rootdir[MAX_SYSPATH];	// engine root directory
char			fs_basedir[MAX_SYSPATH];	// base directory of game
char			fs_falldir[MAX_SYSPATH];	// game falling directory
char			fs_gamedir[MAX_SYSPATH];	// game current directory
char			gs_basedir[MAX_SYSPATH];	// initial dir before loading gameinfo.txt (used for compilers too)
qboolean			fs_ext_path = false;	// attempt to read\write from ./ or ../ pathes 
static const wadtype_t	wad_hints[10];

static void FS_InitMemory( void );
const char *FS_FileExtension( const char *in );
static searchpath_t *FS_FindFile( const char *name, int *index, qboolean gamedironly );
static dlumpinfo_t *W_FindLump( wfile_t *wad, const char *name, const char matchtype );
static dpackfile_t* FS_AddFileToPack( const char* name, pack_t *pack, long offset, long size );
static byte *W_LoadFile( const char *path, long *filesizeptr, qboolean gamedironly );
static qboolean FS_SysFileExists( const char *path );
static qboolean FS_SysFolderExists( const char *path );
static long FS_SysFileTime( const char *filename );
static char W_TypeFromExt( const char *lumpname );
static const char *W_ExtFromType( char lumptype );

/*
=============================================================================

FILEMATCH COMMON SYSTEM

=============================================================================
*/
int matchpattern( const char *str, const char *cmp, qboolean caseinsensitive )
{
	int	c1, c2;

	while( *cmp )
	{
		switch( *cmp )
		{
		case 0:   return 1; // end of pattern
		case '?': // match any single character
			if( *str == 0 || *str == '/' || *str == '\\' || *str == ':' )
				return 0; // no match
			str++;
			cmp++;
			break;
		case '*': // match anything until following string
			if( !*str ) return 1; // match
			cmp++;
			while( *str )
			{
				if( *str == '/' || *str == '\\' || *str == ':' )
					break;
				// see if pattern matches at this offset
				if( matchpattern( str, cmp, caseinsensitive ))
					return 1;
				// nope, advance to next offset
				str++;
			}
			break;
		default:
			if( *str != *cmp )
			{
				if( !caseinsensitive )
					return 0; // no match
				c1 = Q_tolower( *str );
				c2 = Q_tolower( *cmp );
				if( c1 != c2 ) return 0; // no match
			}

			str++;
			cmp++;
			break;
		}
	}

	// reached end of pattern but not end of input?
	return (*str) ? 0 : 1;
}

void stringlistinit( stringlist_t *list )
{
	memset( list, 0, sizeof( *list ));
}

void stringlistfreecontents( stringlist_t *list )
{
	int	i;

	for( i = 0; i < list->numstrings; i++ )
	{
		if( list->strings[i] )
			Mem_Free( list->strings[i] );
		list->strings[i] = NULL;
	}

	if( list->strings )
		Mem_Free( list->strings );

	list->numstrings = 0;
	list->maxstrings = 0;
	list->strings = NULL;
}

void stringlistappend( stringlist_t *list, char *text )
{
	size_t	textlen;

	if( !Q_stricmp( text, "." ) || !Q_stricmp( text, ".." ))
		return; // ignore the virtual directories

	if( list->numstrings >= list->maxstrings )
	{
		list->maxstrings += 4096;
		list->strings = Mem_Realloc( fs_mempool, list->strings, list->maxstrings * sizeof( *list->strings ));
	}

	textlen = Q_strlen( text ) + 1;
	list->strings[list->numstrings] = Mem_Alloc( fs_mempool, textlen );
	memcpy( list->strings[list->numstrings], text, textlen );
	list->numstrings++;
}

void stringlistsort( stringlist_t *list )
{
	char	*temp;
	int	i, j;

	// this is a selection sort (finds the best entry for each slot)
	for( i = 0; i < list->numstrings - 1; i++ )
	{
		for( j = i + 1; j < list->numstrings; j++ )
		{
			if( Q_strcmp( list->strings[i], list->strings[j] ) > 0 )
			{
				temp = list->strings[i];
				list->strings[i] = list->strings[j];
				list->strings[j] = temp;
			}
		}
	}
}

void listdirectory( stringlist_t *list, const char *path )
{
	char		pattern[4096];
	struct _finddata_t	n_file;
	long		hFile;
	char		*c;
	int		i;

	Q_strncpy( pattern, path, sizeof( pattern ));
	Q_strncat( pattern, "*", sizeof( pattern ));

	// ask for the directory listing handle
	hFile = _findfirst( pattern, &n_file );
	if( hFile == -1 ) return;

	// start a new chain with the the first name
	stringlistappend( list, n_file.name );
	// iterate through the directory
	while( _findnext( hFile, &n_file ) == 0 )
		stringlistappend( list, n_file.name );
	_findclose( hFile );

	// convert names to lowercase because windows doesn't care, but pattern matching code often does
	for( i = 0; i < list->numstrings; i++ )
	{
		for( c = list->strings[i]; *c; c++ )
			*c = Q_tolower( *c );
	}
}

/*
=============================================================================

OTHER PRIVATE FUNCTIONS

=============================================================================
*/
/*
====================
FS_AddFileToPack

Add a file to the list of files contained into a package
====================
*/
static dpackfile_t* FS_AddFileToPack( const char* name, pack_t* pack, long offset, long size )
{
	int		left, right, middle;
	dpackfile_t	*pfile;

	// look for the slot we should put that file into (binary search)
	left = 0;
	right = pack->numfiles - 1;
	while( left <= right )
	{
		int diff;

		middle = (left + right) / 2;
		diff = Q_stricmp( pack->files[middle].name, name );

		// If we found the file, there's a problem
		if( !diff ) MsgDev( D_WARN, "Package %s contains the file %s several times\n", pack->filename, name );

		// If we're too far in the list
		if( diff > 0 ) right = middle - 1;
		else left = middle + 1;
	}

	// We have to move the right of the list by one slot to free the one we need
	pfile = &pack->files[left];
	memmove( pfile + 1, pfile, (pack->numfiles - left) * sizeof( *pfile ));
	pack->numfiles++;

	Q_strncpy( pfile->name, name, sizeof( pfile->name ));
	pfile->filepos = offset;
	pfile->filelen = size;

	return pfile;
}

/*
============
FS_CreatePath

Only used for FS_Open.
============
*/
void FS_CreatePath( char *path )
{
	char *ofs, save;

	for( ofs = path+1; *ofs; ofs++ )
	{
		if( *ofs == '/' || *ofs == '\\' )
		{
			// create the directory
			save = *ofs;
			*ofs = 0;
			_mkdir( path );
			*ofs = save;
		}
	}
}

/*
============
FS_Path_f

debug info
============
*/
void FS_Path_f( void )
{
	searchpath_t	*s;

	Msg( "Current search path:\n" );

	for( s = fs_searchpaths; s; s = s->next )
	{
		if( s->pack ) Msg( "%s (%i files)", s->pack->filename, s->pack->numfiles );
		else if( s->wad ) Msg( "%s (%i files)", s->wad->filename, s->wad->numlumps );
		else Msg( "%s", s->filename );

		if( FBitSet( s->flags, FS_GAMEDIR_PATH ))
			Msg( " ^2gamedir^7\n" );
		else Msg( "\n" );
	}
}

/*
============
FS_ClearPath_f

only for debug targets
============
*/
void FS_ClearPaths_f( void )
{
	FS_ClearSearchPath();
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

	len = Q_strlen( in );
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
	Q_strncpy( out, &in[start], len + 1 );
	out[len] = 0;
}

/*
=================
FS_LoadPackPAK

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
pack_t *FS_LoadPackPAK( const char *packfile, int *error )
{
	dpackheader_t	header;
	int		packhandle;
	int		i, numpackfiles;
	pack_t		*pack;
	dpackfile_t	*info;

	packhandle = open( packfile, O_RDONLY|O_BINARY );

	if( packhandle < 0 )
	{
		MsgDev( D_NOTE, "%s couldn't open\n", packfile );
		if( error ) *error = PAK_LOAD_COULDNT_OPEN;
		return NULL;
	}

	read( packhandle, (void *)&header, sizeof( header ));

	if( header.ident != IDPACKV1HEADER )
	{
		MsgDev( D_NOTE, "%s is not a packfile. Ignored.\n", packfile );
		if( error ) *error = PAK_LOAD_BAD_HEADER;
		close( packhandle );
		return NULL;
	}

	if( header.dirlen % sizeof( dpackfile_t ))
	{
		MsgDev( D_ERROR, "%s has an invalid directory size. Ignored.\n", packfile );
		if( error ) *error = PAK_LOAD_BAD_FOLDERS;
		close( packhandle );
		return NULL;
	}

	numpackfiles = header.dirlen / sizeof( dpackfile_t );

	if( numpackfiles > MAX_FILES_IN_PACK )
	{
		MsgDev( D_ERROR, "%s has too many files ( %i ). Ignored.\n", packfile, numpackfiles );
		if( error ) *error = PAK_LOAD_TOO_MANY_FILES;
		close( packhandle );
		return NULL;
	}

	if( numpackfiles <= 0 )
	{
		MsgDev( D_NOTE, "%s has no files. Ignored.\n", packfile );
		if( error ) *error = PAK_LOAD_NO_FILES;
		close( packhandle );
		return NULL;
	}

	info = (dpackfile_t *)Mem_Alloc( fs_mempool, sizeof( *info ) * numpackfiles );
	lseek( packhandle, header.dirofs, SEEK_SET );

	if( header.dirlen != read( packhandle, (void *)info, header.dirlen ))
	{
		MsgDev( D_NOTE, "%s is an incomplete PAK, not loading\n", packfile );
		if( error ) *error = PAK_LOAD_CORRUPTED;
		close( packhandle );
		Mem_Free( info );
		return NULL;
	}

	pack = (pack_t *)Mem_Alloc( fs_mempool, sizeof( pack_t ));
	Q_strncpy( pack->filename, packfile, sizeof( pack->filename ));
	pack->files = (dpackfile_t *)Mem_Alloc( fs_mempool, numpackfiles * sizeof( dpackfile_t ));
	pack->filetime = FS_SysFileTime( packfile );
	pack->handle = packhandle;
	pack->numfiles = 0;

	// parse the directory
	for( i = 0; i < numpackfiles; i++ )
		FS_AddFileToPack( info[i].name, pack, info[i].filepos, info[i].filelen );

	if( error ) *error = PAK_LOAD_OK;
	Mem_Free( info );

	return pack;
}

/*
====================
FS_AddWad_Fullpath
====================
*/
static qboolean FS_AddWad_Fullpath( const char *wadfile, qboolean *already_loaded, int flags )
{
	searchpath_t	*search;
	wfile_t		*wad = NULL;
	const char	*ext = FS_FileExtension( wadfile );
	int		errorcode = WAD_LOAD_COULDNT_OPEN;

	for( search = fs_searchpaths; search; search = search->next )
	{
		if( search->wad && !Q_stricmp( search->wad->filename, wadfile ))
		{
			if( already_loaded ) *already_loaded = true;
			return true; // already loaded
		}
	}
          
	if( already_loaded ) *already_loaded = false;
	if( !Q_stricmp( ext, "wad" )) wad = W_Open( wadfile, "rb", &errorcode );
	else MsgDev( D_ERROR, "\"%s\" doesn't have a wad extension\n", wadfile );

	if( wad )
	{
		search = (searchpath_t *)Mem_Alloc( fs_mempool, sizeof( searchpath_t ));
		search->wad = wad;
		search->next = fs_searchpaths;
		search->flags |= flags;
		fs_searchpaths = search;

		MsgDev( D_REPORT, "Adding wadfile: %s (%i files)\n", wadfile, wad->numlumps );
		return true;
	}
	else
	{
		if( errorcode != WAD_LOAD_NO_FILES )
			MsgDev( D_ERROR, "FS_AddWad_Fullpath: unable to load wad \"%s\"\n", wadfile );
		return false;
	}
}

/*
================
FS_AddPak_Fullpath

Adds the given pack to the search path.
The pack type is autodetected by the file extension.

Returns true if the file was successfully added to the
search path or if it was already included.

If keep_plain_dirs is set, the pack will be added AFTER the first sequence of
plain directories.
================
*/
static qboolean FS_AddPak_Fullpath( const char *pakfile, qboolean *already_loaded, int flags )
{
	searchpath_t	*search;
	pack_t		*pak = NULL;
	const char	*ext = FS_FileExtension( pakfile );
	int		i, errorcode = PAK_LOAD_COULDNT_OPEN;
	
	for( search = fs_searchpaths; search; search = search->next )
	{
		if( search->pack && !Q_stricmp( search->pack->filename, pakfile ))
		{
			if( already_loaded ) *already_loaded = true;
			return true; // already loaded
		}
	}

	if( already_loaded ) *already_loaded = false;

	if( !Q_stricmp( ext, "pak" )) pak = FS_LoadPackPAK( pakfile, &errorcode );
	else MsgDev( D_ERROR, "\"%s\" does not have a pack extension\n", pakfile );

	if( pak )
	{
		string	fullpath;

		search = (searchpath_t *)Mem_Alloc( fs_mempool, sizeof( searchpath_t ));
		search->pack = pak;
		search->next = fs_searchpaths;
		search->flags |= flags;
		fs_searchpaths = search;

		MsgDev( D_REPORT, "Adding pakfile: %s (%i files)\n", pakfile, pak->numfiles );

		// time to add in search list all the wads that contains in current pakfile (if do)
		for( i = 0; i < pak->numfiles; i++ )
		{
			if( !Q_stricmp( FS_FileExtension( pak->files[i].name ), "wad" ))
			{
				Q_sprintf( fullpath, "%s/%s", pakfile, pak->files[i].name );
				FS_AddWad_Fullpath( fullpath, NULL, flags );
			}
		}

		return true;
	}
	else
	{
		if( errorcode != PAK_LOAD_NO_FILES )
			MsgDev( D_ERROR, "FS_AddPak_Fullpath: unable to load pak \"%s\"\n", pakfile );
		return false;
	}
}

/*
================
FS_AddGameDirectory

Sets fs_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ...
================
*/
void FS_AddGameDirectory( const char *dir, int flags )
{
	stringlist_t	list;
	searchpath_t	*search;
	string		fullpath;
	int		i;

	if( !FBitSet( flags, FS_NOWRITE_PATH ))
		Q_strncpy( fs_gamedir, dir, sizeof( fs_gamedir ));

	stringlistinit( &list );
	listdirectory( &list, dir );
	stringlistsort( &list );

	// add any PAK package in the directory
	for( i = 0; i < list.numstrings; i++ )
	{
		if( !Q_stricmp( FS_FileExtension( list.strings[i] ), "pak" ))
		{
			Q_sprintf( fullpath, "%s%s", dir, list.strings[i] );
			FS_AddPak_Fullpath( fullpath, NULL, flags );
		}
	}

	FS_AllowDirectPaths( true );

	// add any WAD package in the directory
	for( i = 0; i < list.numstrings; i++ )
	{
		if( !Q_stricmp( FS_FileExtension( list.strings[i] ), "wad" ))
		{
			Q_sprintf( fullpath, "%s%s", dir, list.strings[i] );
			FS_AddWad_Fullpath( fullpath, NULL, flags );
		}
	}

	stringlistfreecontents( &list );
	FS_AllowDirectPaths( false );

	// add the directory to the search path
	// (unpacked files have the priority over packed files)
	search = (searchpath_t *)Mem_Alloc( fs_mempool, sizeof( searchpath_t ));
	Q_strncpy( search->filename, dir, sizeof ( search->filename ));
	search->next = fs_searchpaths;
	search->flags = flags;
	fs_searchpaths = search;
}

/*
================
FS_AddGameHierarchy
================
*/
void FS_AddGameHierarchy( const char *dir, int flags )
{
	// Add the common game directory
	if( dir && *dir ) FS_AddGameDirectory( va( "%s%s/", fs_basedir, dir ), flags );
}

/*
============
FS_FileExtension
============
*/
const char *FS_FileExtension( const char *in )
{
	const char *separator, *backslash, *colon, *dot;

	separator = Q_strrchr( in, '/' );
	backslash = Q_strrchr( in, '\\' );

	if( !separator || separator < backslash )
		separator = backslash;

	colon = Q_strrchr( in, ':' );

	if( !separator || separator < colon )
		separator = colon;

	dot = Q_strrchr( in, '.' );

	if( dot == NULL || ( separator && ( dot < separator )))
		return "";

	return dot + 1;
}

/*
============
FS_FileWithoutPath
============
*/
const char *FS_FileWithoutPath( const char *in )
{
	const char *separator, *backslash, *colon;

	separator = Q_strrchr( in, '/' );
	backslash = Q_strrchr( in, '\\' );

	if( !separator || separator < backslash )
		separator = backslash;

	colon = Q_strrchr( in, ':' );

	if( !separator || separator < colon )
		separator = colon;

	return separator ? separator + 1 : in;
}

/*
============
FS_ExtractFilePath
============
*/
void FS_ExtractFilePath( const char *path, char *dest )
{
	const char *src = path + Q_strlen( path ) - 1;

	// back up until a \ or the start
	while( src != path && !(*(src - 1) == '\\' || *(src - 1) == '/' ))
		src--;

	if( src != path )
	{
		memcpy( dest, path, src - path );
		dest[src - path - 1] = 0; // cutoff backslash
	}
	else Q_strcpy( dest, "" ); // file without path
}

/*
================
FS_ClearSearchPath
================
*/
void FS_ClearSearchPath( void )
{
	while( fs_searchpaths )
	{
		searchpath_t	*search = fs_searchpaths;

		if( !search ) break;

		if( FBitSet( search->flags, FS_STATIC_PATH ))
		{
			// skip read-only pathes
			if( search->next )
				fs_searchpaths = search->next->next;
			else break;
		}
		else fs_searchpaths = search->next;

		if( search->pack )
		{
			if( search->pack->files ) 
				Mem_Free( search->pack->files );
			Mem_Free( search->pack );
		}
		if( search->wad )
		{
			W_Close( search->wad );
		}

		Mem_Free( search );
	}
}

/*
====================
FS_CheckNastyPath

Return true if the path should be rejected due to one of the following:
1: path elements that are non-portable
2: path elements that would allow access to files outside the game directory,
   or are just not a good idea for a mod to be using.
====================
*/
int FS_CheckNastyPath( const char *path, qboolean isgamedir )
{
	// all: never allow an empty path, as for gamedir it would access the parent directory and a non-gamedir path it is just useless
	if( !path[0] ) return 2;

	// Mac: don't allow Mac-only filenames - : is a directory separator
	// instead of /, but we rely on / working already, so there's no reason to
	// support a Mac-only path
	// Amiga and Windows: : tries to go to root of drive
	if( Q_strstr( path, ":" ) && !fs_ext_path ) return 1; // non-portable attempt to go to root of drive

	// Amiga: // is parent directory
	if( Q_strstr( path, "//" )  && !fs_ext_path ) return 1; // non-portable attempt to go to parent directory

	// all: don't allow going to parent directory (../ or /../)
	if( Q_strstr( path, ".." ) && !fs_ext_path ) return 2; // attempt to go outside the game directory

	// Windows and UNIXes: don't allow absolute paths
	if( path[0] == '/' && !fs_ext_path ) return 2; // attempt to go outside the game directory

	// all: forbid trailing slash on gamedir
	if( isgamedir && !fs_ext_path && path[Q_strlen( path )-1] == '/' ) return 2;

	// all: forbid leading dot on any filename for any reason
	if( Q_strstr( path, "/." ) && !fs_ext_path ) return 2; // attempt to go outside the game directory

	// after all these checks we're pretty sure it's a / separated filename
	// and won't do much if any harm
	return 0;
}

/*
================
FS_Rescan
================
*/
void FS_Rescan( void )
{
	MsgDev( D_NOTE, "FS_Rescan( %s )\n", GI->title );

	FS_ClearSearchPath();

	if( Q_stricmp( GI->basedir, GI->gamedir ))
		FS_AddGameHierarchy( GI->basedir, 0 );
	if( Q_stricmp( GI->basedir, GI->falldir ) && Q_stricmp( GI->gamedir, GI->falldir ))
		FS_AddGameHierarchy( GI->falldir, 0 );
	FS_AddGameHierarchy( GI->gamedir, FS_GAMEDIR_PATH );

	if( FS_FileExists( va( "%s.rc", SI.basedirName ), false ))
		Q_strncpy( SI.rcName, SI.basedirName, sizeof( SI.rcName ));	// e.g. valve.rc
	else Q_strncpy( SI.rcName, SI.exeName, sizeof( SI.rcName ));	// e.g. quake.rc
}

/*
================
FS_Rescan_f
================
*/
void FS_Rescan_f( void )
{
	FS_Rescan();
}

/*
================
FS_WriteGameInfo

assume GameInfo is valid
================
*/
static qboolean FS_WriteGameInfo( const char *filepath, gameinfo_t *GameInfo )
{
	file_t	*f;

	if( !GameInfo ) return false;
	f = FS_Open( filepath, "w", false );	// we in binary-mode
	if( !f ) return false;

	FS_Print( f, "// generated by Xash3D\n\n\n" );

	if( Q_strlen( GameInfo->basedir ))
		FS_Printf( f, "basedir\t\t\"%s\"\n", GameInfo->basedir );

	if( Q_strlen( GameInfo->gamedir ))
		FS_Printf( f, "gamedir\t\t\"%s\"\n", GameInfo->gamedir );

	if( Q_strlen( GameInfo->falldir ))
		FS_Printf( f, "fallback_dir\t\"%s\"\n", GameInfo->falldir );

	if( Q_strlen( GameInfo->title ))
		FS_Printf( f, "title\t\t\"%s\"\n", GameInfo->title );

	if( Q_strlen( GameInfo->startmap ))
		FS_Printf( f, "startmap\t\t\"%s\"\n", GameInfo->startmap );

	if( Q_strlen( GameInfo->trainmap ))
		FS_Printf( f, "trainmap\t\t\"%s\"\n", GameInfo->trainmap );

	if( GameInfo->version != 0.0f )
		FS_Printf( f, "version\t\t%g\n", GameInfo->version );

	if( GameInfo->size != 0 )
		FS_Printf( f, "size\t\t%i\n", GameInfo->size );

	if( Q_strlen( GameInfo->game_url ))
		FS_Printf( f, "url_info\t\t\"%s\"\n", GameInfo->game_url );

	if( Q_strlen( GameInfo->update_url ))
		FS_Printf( f, "url_update\t\t\"%s\"\n", GameInfo->update_url );

	if( Q_strlen( GameInfo->type ))
		FS_Printf( f, "type\t\t\"%s\"\n", GameInfo->type );

	if( Q_strlen( GameInfo->date ))
		FS_Printf( f, "date\t\t\"%s\"\n", GameInfo->date );

	if( Q_strlen( GameInfo->dll_path ))
		FS_Printf( f, "dllpath\t\t\"%s\"\n", GameInfo->dll_path );	
	if( Q_strlen( GameInfo->game_dll ))
		FS_Printf( f, "gamedll\t\t\"%s\"\n", GameInfo->game_dll );

	if( Q_strlen( GameInfo->iconpath ))
		FS_Printf( f, "icon\t\t\"%s\"\n", GameInfo->iconpath );

	switch( GameInfo->gamemode )
	{
	case 1: FS_Print( f, "gamemode\t\t\"singleplayer_only\"\n" ); break;
	case 2: FS_Print( f, "gamemode\t\t\"multiplayer_only\"\n" ); break;
	}

	if( Q_strlen( GameInfo->sp_entity ))
		FS_Printf( f, "sp_entity\t\t\"%s\"\n", GameInfo->sp_entity );
	if( Q_strlen( GameInfo->mp_entity ))
		FS_Printf( f, "mp_entity\t\t\"%s\"\n", GameInfo->mp_entity );

	if( GameInfo->secure )
		FS_Printf( f, "secure\t\t\"%i\"\n", GameInfo->secure );

	if( GameInfo->nomodels )
		FS_Printf( f, "nomodels\t\t\"%i\"\n", GameInfo->nomodels );

	if( GameInfo->max_edicts > 0 )
		FS_Printf( f, "max_edicts\t%i\n", GameInfo->max_edicts );
	if( GameInfo->max_tents > 0 )
		FS_Printf( f, "max_tempents\t%i\n", GameInfo->max_tents );
	if( GameInfo->max_beams > 0 )
		FS_Printf( f, "max_beams\t\t%i\n", GameInfo->max_beams );
	if( GameInfo->max_particles > 0 )
		FS_Printf( f, "max_particles\t%i\n", GameInfo->max_particles );

	FS_Print( f, "\n\n\n" );
	FS_Close( f );	// all done

	return true;
}

/*
================
FS_CreateDefaultGameInfo
================
*/
void FS_CreateDefaultGameInfo( const char *filename )
{
	gameinfo_t	defGI;

	memset( &defGI, 0, sizeof( defGI ));

	// setup default values
	defGI.max_edicts = 900;	// default value if not specified
	defGI.max_tents = 500;
	defGI.max_beams = 128;
	defGI.max_particles = 4096;
	defGI.version = 1.0;
	defGI.falldir[0] = '\0';

	Q_strncpy( defGI.title, "New Game", sizeof( defGI.title ));
	Q_strncpy( defGI.gamedir, gs_basedir, sizeof( defGI.gamedir ));
	Q_strncpy( defGI.basedir, SI.basedirName, sizeof( defGI.basedir ));
	Q_strncpy( defGI.sp_entity, "info_player_start", sizeof( defGI.sp_entity ));
	Q_strncpy( defGI.mp_entity, "info_player_deathmatch", sizeof( defGI.mp_entity ));
	Q_strncpy( defGI.dll_path, "cl_dlls", sizeof( defGI.dll_path ));
	Q_strncpy( defGI.game_dll, "dlls/hl.dll", sizeof( defGI.game_dll ));
	Q_strncpy( defGI.startmap, "newmap", sizeof( defGI.startmap ));
	Q_strncpy( defGI.iconpath, "game.ico", sizeof( defGI.iconpath ));

	// make simple gameinfo.txt
	FS_WriteGameInfo( filename, &defGI );
} 

/*
================
FS_ParseLiblistGam
================
*/
static qboolean FS_ParseLiblistGam( const char *filename, const char *gamedir, gameinfo_t *GameInfo )
{
	char	*afile, *pfile;
	string	token;

	if( !GameInfo ) return false;	
	afile = FS_LoadFile( filename, NULL, false );
	if( !afile ) return false;

	// setup default values
	GameInfo->max_edicts = 900;	// default value if not specified
	GameInfo->max_tents = 500;
	GameInfo->max_beams = 128;
	GameInfo->max_particles = 4096;
	GameInfo->version = 1.0f;
	GameInfo->falldir[0] = '\0';
	
	Q_strncpy( GameInfo->title, "New Game", sizeof( GameInfo->title ));
	Q_strncpy( GameInfo->gamedir, gamedir, sizeof( GameInfo->gamedir ));
	Q_strncpy( GameInfo->basedir, SI.basedirName, sizeof( GameInfo->basedir ));
	Q_strncpy( GameInfo->sp_entity, "info_player_start", sizeof( GameInfo->sp_entity ));
	Q_strncpy( GameInfo->mp_entity, "info_player_deathmatch", sizeof( GameInfo->mp_entity ));
	Q_strncpy( GameInfo->game_dll, "dlls/hl.dll", sizeof( GameInfo->game_dll ));
	Q_strncpy( GameInfo->startmap, "newmap", sizeof( GameInfo->startmap ));
	Q_strncpy( GameInfo->dll_path, "cl_dlls", sizeof( GameInfo->dll_path ));
	Q_strncpy( GameInfo->iconpath, "game.ico", sizeof( GameInfo->iconpath ));

	pfile = afile;

	while(( pfile = COM_ParseFile( pfile, token )) != NULL )
	{
		if( !Q_stricmp( token, "game" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->title );
		}
		if( !Q_stricmp( token, "gamedir" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->gamedir );
		}
		if( !Q_stricmp( token, "fallback_dir" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->falldir );
		}
		else if( !Q_stricmp( token, "startmap" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->startmap );
			FS_StripExtension( GameInfo->startmap ); // HQ2:Amen has extension .bsp
		}
		else if( !Q_stricmp( token, "trainmap" ) || !Q_stricmp( token, "trainingmap" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->trainmap );
			FS_StripExtension( GameInfo->trainmap ); // HQ2:Amen has extension .bsp
		}
		else if( !Q_stricmp( token, "url_info" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->game_url );
		}
		else if( !Q_stricmp( token, "url_dl" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->update_url );
		}
		else if( !Q_stricmp( token, "gamedll" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->game_dll );
			COM_FixSlashes( GameInfo->game_dll );
		}
		else if( !Q_stricmp( token, "icon" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->iconpath );
			COM_FixSlashes( GameInfo->iconpath );
			FS_DefaultExtension( GameInfo->iconpath, ".ico" );
		}
		else if( !Q_stricmp( token, "type" ))
		{
			pfile = COM_ParseFile( pfile, token );

			if( !Q_stricmp( token, "singleplayer_only" ))
			{
				GameInfo->gamemode = 1;
				Q_strncpy( GameInfo->type, "Single", sizeof( GameInfo->type ));
			}
			else if( !Q_stricmp( token, "multiplayer_only" ))
			{
				GameInfo->gamemode = 2;
				Q_strncpy( GameInfo->type, "Multiplayer", sizeof( GameInfo->type ));
			}
			else
			{
				// pass type without changes
				GameInfo->gamemode = 0;
				Q_strncpy( GameInfo->type, token, sizeof( GameInfo->type ));
			}
		}
		else if( !Q_stricmp( token, "version" ))
		{
			pfile = COM_ParseFile( pfile, token );
			GameInfo->version = Q_atof( token );
		}
		else if( !Q_stricmp( token, "size" ))
		{
			pfile = COM_ParseFile( pfile, token );
			GameInfo->size = Q_atoi( token );
		}
		else if( !Q_stricmp( token, "mpentity" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->mp_entity );
		}
		else if( !Q_stricmp( token, "secure" ))
		{
			pfile = COM_ParseFile( pfile, token );
			GameInfo->secure = Q_atoi( token );
		}
		else if( !Q_stricmp( token, "nomodels" ))
		{
			pfile = COM_ParseFile( pfile, token );
			GameInfo->nomodels = Q_atoi( token );
		}
	}

	if( !FS_SysFolderExists( va( "%s\\%s", host.rootdir, GameInfo->gamedir )))
		Q_strncpy( GameInfo->gamedir, gamedir, sizeof( GameInfo->gamedir ));

	if( !FS_SysFolderExists( va( "%s\\%s", host.rootdir, GameInfo->falldir )))
		GameInfo->falldir[0] = '\0';

	Mem_Free( afile );

	return true;
}

/*
================
FS_ConvertGameInfo
================
*/
void FS_ConvertGameInfo( const char *gamedir, const char *gameinfo_path, const char *liblist_path )
{
	gameinfo_t	GameInfo;

	memset( &GameInfo, 0, sizeof( GameInfo ));

	if( FS_ParseLiblistGam( liblist_path, gamedir, &GameInfo ))
	{
		if( FS_WriteGameInfo( gameinfo_path, &GameInfo ))
			MsgDev( D_INFO, "Convert %s to %s\n", liblist_path, gameinfo_path );
	}
}

/*
================
FS_ParseGameInfo
================
*/
static qboolean FS_ParseGameInfo( const char *gamedir, gameinfo_t *GameInfo )
{
	char	*afile, *pfile;
	string	fs_path, filepath;
	string	liblist, token;

	Q_snprintf( filepath, sizeof( filepath ), "%s/gameinfo.txt", gamedir );
	Q_snprintf( liblist, sizeof( liblist ), "%s/liblist.gam", gamedir );

	// if user change liblist.gam update the gameinfo.txt
	if( FS_FileTime( liblist, false ) > FS_FileTime( filepath, false ))
		FS_ConvertGameInfo( gamedir, filepath, liblist );

	// force to create gameinfo for specified game if missing
	if( !Q_stricmp( gs_basedir, gamedir ) && !FS_FileExists( filepath, false ))
		FS_CreateDefaultGameInfo( filepath );

	if( !GameInfo ) return false;	// no dest

	afile = FS_LoadFile( filepath, NULL, false );
	if( !afile ) return false;

	// setup default values
	Q_strncpy( GameInfo->gamefolder, gamedir, sizeof( GameInfo->gamefolder ));
	GameInfo->max_edicts = 900;	// default value if not specified
	GameInfo->max_tents = 500;
	GameInfo->max_beams = 128;
	GameInfo->max_particles = 4096;
	GameInfo->version = 1.0f;
	GameInfo->falldir[0] = '\0';
	
	Q_strncpy( GameInfo->title, "New Game", sizeof( GameInfo->title ));
	Q_strncpy( GameInfo->sp_entity, "info_player_start", sizeof( GameInfo->sp_entity ));
	Q_strncpy( GameInfo->mp_entity, "info_player_deathmatch", sizeof( GameInfo->mp_entity ));
	Q_strncpy( GameInfo->dll_path, "cl_dlls", sizeof( GameInfo->dll_path ));
	Q_strncpy( GameInfo->game_dll, "dlls/hl.dll", sizeof( GameInfo->game_dll ));
	Q_strncpy( GameInfo->startmap, "", sizeof( GameInfo->startmap ));
	Q_strncpy( GameInfo->iconpath, "game.ico", sizeof( GameInfo->iconpath ));

	pfile = afile;

	while(( pfile = COM_ParseFile( pfile, token )) != NULL )
	{
		if( !Q_stricmp( token, "basedir" ))
		{
			pfile = COM_ParseFile( pfile, fs_path );
			if( Q_stricmp( fs_path, GameInfo->basedir ) || Q_stricmp( fs_path, GameInfo->gamedir ))
				Q_strncpy( GameInfo->basedir, fs_path, sizeof( GameInfo->basedir ));
		}
		else if( !Q_stricmp( token, "fallback_dir" ))
		{
			pfile = COM_ParseFile( pfile, fs_path );
			if( Q_stricmp( fs_path, GameInfo->basedir ) || Q_stricmp( fs_path, GameInfo->falldir ))
				Q_strncpy( GameInfo->falldir, fs_path, sizeof( GameInfo->falldir ));
		}
		else if( !Q_stricmp( token, "gamedir" ))
		{
			pfile = COM_ParseFile( pfile, fs_path );
			if( Q_stricmp( fs_path, GameInfo->basedir ) || Q_stricmp( fs_path, GameInfo->gamedir ))
				Q_strncpy( GameInfo->gamedir, fs_path, sizeof( GameInfo->gamedir ));
		}
		else if( !Q_stricmp( token, "title" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->title );
		}
		else if( !Q_stricmp( token, "sp_entity" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->sp_entity );
		}
		else if( !Q_stricmp( token, "mp_entity" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->mp_entity );
		}
		else if( !Q_stricmp( token, "gamedll" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->game_dll );
		}
		else if( !Q_stricmp( token, "dllpath" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->dll_path );
		}
		else if( !Q_stricmp( token, "startmap" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->startmap );
			FS_StripExtension( GameInfo->startmap ); // HQ2:Amen has extension .bsp
		}
		else if( !Q_stricmp( token, "trainmap" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->trainmap );
			FS_StripExtension( GameInfo->trainmap ); // HQ2:Amen has extension .bsp
		}
		else if( !Q_stricmp( token, "icon" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->iconpath );
			FS_DefaultExtension( GameInfo->iconpath, ".ico" );
		}
		else if( !Q_stricmp( token, "url_info" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->game_url );
		}
		else if( !Q_stricmp( token, "url_update" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->update_url );
		}
		else if( !Q_stricmp( token, "date" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->date );
		}
		else if( !Q_stricmp( token, "type" ))
		{
			pfile = COM_ParseFile( pfile, GameInfo->type );
		}
		else if( !Q_stricmp( token, "version" ))
		{
			pfile = COM_ParseFile( pfile, token );
			GameInfo->version = Q_atof( token );
		}
		else if( !Q_stricmp( token, "size" ))
		{
			pfile = COM_ParseFile( pfile, token );
			GameInfo->size = Q_atoi( token );
		}
		else if( !Q_stricmp( token, "max_edicts" ))
		{
			pfile = COM_ParseFile( pfile, token );
			GameInfo->max_edicts = bound( 600, Q_atoi( token ), 4096 );
		}
		else if( !Q_stricmp( token, "max_tempents" ))
		{
			pfile = COM_ParseFile( pfile, token );
			GameInfo->max_tents = bound( 300, Q_atoi( token ), 2048 );
		}
		else if( !Q_stricmp( token, "max_beams" ))
		{
			pfile = COM_ParseFile( pfile, token );
			GameInfo->max_beams = bound( 64, Q_atoi( token ), 512 );
		}
		else if( !Q_stricmp( token, "max_particles" ))
		{
			pfile = COM_ParseFile( pfile, token );
			GameInfo->max_particles = bound( 1024, Q_atoi( token ), 131072 );
		}
		else if( !Q_stricmp( token, "gamemode" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( !Q_stricmp( token, "singleplayer_only" ))
				GameInfo->gamemode = 1;
			else if( !Q_stricmp( token, "multiplayer_only" ))
				GameInfo->gamemode = 2;
		}
		else if( !Q_stricmp( token, "secure" ))
		{
			pfile = COM_ParseFile( pfile, token );
			GameInfo->secure = Q_atoi( token );
		}
		else if( !Q_stricmp( token, "nomodels" ))
		{
			pfile = COM_ParseFile( pfile, token );
			GameInfo->nomodels = Q_atoi( token );
		}
		else if( !Q_strnicmp( token, "ambient", 7 ))
		{
			int	ambientNum = Q_atoi( token + 7 );

			if( ambientNum < 0 || ambientNum > ( NUM_AMBIENTS - 1 ))
			{
				MsgDev( D_ERROR, "FS_ParseGameInfo: Invalid ambient number %i. Ignored.\n", ambientNum );
			}
			else
			{
				pfile = COM_ParseFile( pfile, GameInfo->ambientsound[ambientNum] );
			}
		}
	}

	// make sure what gamedir is really exist
	if( !FS_SysFolderExists( va( "%s\\%s", host.rootdir, GameInfo->gamedir )))
		Q_strncpy( GameInfo->gamedir, gamedir, sizeof( GameInfo->gamedir ));

	if( !FS_SysFolderExists( va( "%s\\%s", host.rootdir, GameInfo->falldir )))
		GameInfo->falldir[0] = '\0';

	if( afile != NULL )
		Mem_Free( afile );

	return true;
}

/*
================
FS_LoadGameInfo

can be passed null arg
================
*/
void FS_LoadGameInfo( const char *rootfolder )
{
	int	i;

	// lock uplevel of gamedir for read\write
	fs_ext_path = false;

	if( rootfolder ) Q_strcpy( gs_basedir, rootfolder );
	MsgDev( D_NOTE, "FS_LoadGameInfo( %s )\n", gs_basedir );

	// clear any old pathes
	FS_ClearSearchPath();

	// validate gamedir
	for( i = 0; i < SI.numgames; i++ )
	{
		if( !Q_stricmp( SI.games[i]->gamefolder, gs_basedir ))
			break;
	}

	if( i == SI.numgames )
		Sys_Error( "Couldn't find game directory '%s'\n", gs_basedir );

	SI.GameInfo = SI.games[i];
	FS_Rescan(); // create new filesystem

	Host_InitDecals ();	// reload decals
}

/*
================
FS_Init
================
*/
void FS_Init( void )
{
	stringlist_t	dirs;
	qboolean		hasDefaultDir = false;
	int		i;
	
	FS_InitMemory();

	Cmd_AddCommand( "fs_rescan", FS_Rescan_f, "rescan filesystem search pathes" );
	Cmd_AddCommand( "fs_path", FS_Path_f, "show filesystem search pathes" );
	Cmd_AddCommand( "fs_clearpaths", FS_ClearPaths_f, "clear filesystem search pathes" );

	// ignore commandlineoption "-game" for other stuff
	if( host.type == HOST_NORMAL || host.type == HOST_DEDICATED )
	{
		stringlistinit( &dirs );
		listdirectory( &dirs, "./" );
		stringlistsort( &dirs );
		SI.numgames = 0;
	
		if( !Sys_GetParmFromCmdLine( "-game", gs_basedir ))
			Q_strcpy( gs_basedir, SI.basedirName ); // default dir

		if( FS_CheckNastyPath( gs_basedir, true ))
		{
			MsgDev( D_ERROR, "FS_Init: invalid game directory \"%s\"\n", gs_basedir );		
			Q_strcpy( gs_basedir, SI.basedirName ); // default dir
		}

		// validate directories
		for( i = 0; i < dirs.numstrings; i++ )
		{
			if( !Q_stricmp( SI.basedirName, dirs.strings[i] ))
				hasDefaultDir = true;

			if( !Q_stricmp( gs_basedir, dirs.strings[i] ))
				break;
		}

		if( i == dirs.numstrings )
		{ 
			MsgDev( D_INFO, "FS_Init: game directory \"%s\" not exist\n", gs_basedir );		
			if( hasDefaultDir ) Q_strncpy( gs_basedir, SI.basedirName, sizeof( gs_basedir )); // default dir
		}

		// build list of game directories here
		FS_AddGameDirectory( "./", 0 );

		for( i = 0; i < dirs.numstrings; i++ )
		{
			if( !FS_SysFolderExists( dirs.strings[i] ) || (!Q_stricmp( dirs.strings[i], ".." ) && !fs_ext_path ))
				continue;

			if( !SI.games[SI.numgames] )
				SI.games[SI.numgames] = (gameinfo_t *)Mem_Alloc( fs_mempool, sizeof( gameinfo_t ));

			if( FS_ParseGameInfo( dirs.strings[i], SI.games[SI.numgames] ))
				SI.numgames++; // added
		}

		stringlistfreecontents( &dirs );
	}	

	MsgDev( D_NOTE, "FS_Init: done\n" );
}

void FS_AllowDirectPaths( qboolean enable )
{
	fs_ext_path = enable;
}

/*
================
FS_Shutdown
================
*/
void FS_Shutdown( void )
{
	int	i;

	// release gamedirs
	for( i = 0; i < SI.numgames; i++ )
		if( SI.games[i] ) Mem_Free( SI.games[i] );

	memset( &SI, 0, sizeof( sysinfo_t ));

	FS_ClearSearchPath(); // release all wad files too
	Mem_FreePool( &fs_mempool );
}

/*
====================
FS_SysFileTime

Internal function used to determine filetime
====================
*/
static long FS_SysFileTime( const char *filename )
{
	struct stat buf;
	
	if( stat( filename, &buf ) == -1 )
		return -1;

	return buf.st_mtime;
}

/*
====================
FS_SysOpen

Internal function used to create a file_t and open the relevant non-packed file on disk
====================
*/
static file_t* FS_SysOpen( const char* filepath, const char* mode )
{
	file_t	*file;
	int	mod, opt;
	uint	ind;

	// Parse the mode string
	switch( mode[0] )
	{
	case 'r':	// read
		mod = O_RDONLY;
		opt = 0;
		break;
	case 'w': // write
		mod = O_WRONLY;
		opt = O_CREAT | O_TRUNC;
		break;
	case 'a': // append
		mod = O_WRONLY;
		opt = O_CREAT | O_APPEND;
		break;
	case 'e': // edit
		mod = O_WRONLY;
		opt = O_CREAT;
		break;
	default:
		MsgDev( D_ERROR, "FS_SysOpen(%s, %s): invalid mode\n", filepath, mode );
		return NULL;
	}

	for( ind = 1; mode[ind] != '\0'; ind++ )
	{
		switch( mode[ind] )
		{
		case '+':
			mod = O_RDWR;
			break;
		case 'b':
			opt |= O_BINARY;
			break;
		default:
			MsgDev( D_ERROR, "FS_SysOpen: %s: unknown char in mode (%c)\n", filepath, mode, mode[ind] );
			break;
		}
	}

	file = (file_t *)Mem_Alloc( fs_mempool, sizeof( *file ));
	file->filetime = FS_SysFileTime( filepath );
	file->ungetc = EOF;

	file->handle = open( filepath, mod|opt, 0666 );
	if( file->handle < 0 )
	{
		Mem_Free( file );
		return NULL;
	}

	file->real_length = lseek( file->handle, 0, SEEK_END );

	// For files opened in append mode, we start at the end of the file
	if( mod & O_APPEND ) file->position = file->real_length;
	else lseek( file->handle, 0, SEEK_SET );

	return file;
}


/*
===========
FS_OpenPackedFile

Open a packed file using its package file descriptor
===========
*/
file_t *FS_OpenPackedFile( pack_t *pack, int pack_ind )
{
	dpackfile_t	*pfile;
	int		dup_handle;
	file_t		*file;

	pfile = &pack->files[pack_ind];

	if( lseek( pack->handle, pfile->filepos, SEEK_SET ) == -1 )
		return NULL;

	dup_handle = dup( pack->handle );

	if( dup_handle < 0 )
		return NULL;

	file = (file_t *)Mem_Alloc( fs_mempool, sizeof( *file ));
	file->handle = dup_handle;
	file->real_length = pfile->filelen;
	file->offset = pfile->filepos;
	file->position = 0;
	file->ungetc = EOF;

	return file;
}

/*
==================
FS_SysFileExists

Look for a file in the filesystem only
==================
*/
qboolean FS_SysFileExists( const char *path )
{
	int desc;

	if(( desc = open( path, O_RDONLY|O_BINARY )) < 0 )
		return false;

	close( desc );
	return true;
}

/*
==================
FS_SysFolderExists

Look for a existing folder
==================
*/
qboolean FS_SysFolderExists( const char *path )
{
	DWORD	dwFlags = GetFileAttributes( path );

	return ( dwFlags != -1 ) && FBitSet( dwFlags, FILE_ATTRIBUTE_DIRECTORY );
}

/*
====================
FS_FindFile

Look for a file in the packages and in the filesystem

Return the searchpath where the file was found (or NULL)
and the file index in the package if relevant
====================
*/
static searchpath_t *FS_FindFile( const char *name, int* index, qboolean gamedironly )
{
	searchpath_t	*search;
	char		*pEnvPath;
	pack_t		*pak;

	// search through the path, one element at a time
	for( search = fs_searchpaths; search; search = search->next )
	{
		if( gamedironly & !FBitSet( search->flags, FS_GAMEDIR_PATH ))
			continue;

		// is the element a pak file?
		if( search->pack )
		{
			int left, right, middle;

			pak = search->pack;

			// look for the file (binary search)
			left = 0;
			right = pak->numfiles - 1;
			while( left <= right )
			{
				int diff;

				middle = (left + right) / 2;
				diff = Q_stricmp( pak->files[middle].name, name );

				// Found it
				if( !diff )
				{
					if( index ) *index = middle;
					return search;
				}

				// if we're too far in the list
				if( diff > 0 )
					right = middle - 1;
				else left = middle + 1;
			}
		}
		else if( search->wad )
		{
			dlumpinfo_t	*lump;	
			char		type = W_TypeFromExt( name );
			qboolean		anywadname = true;
			string		wadname, wadfolder;
			string		shortname;

			// quick reject by filetype
			if( type == TYP_NONE ) continue;
			FS_ExtractFilePath( name, wadname );
			wadfolder[0] = '\0';

			if( Q_strlen( wadname ))
			{
				FS_FileBase( wadname, wadname );
				Q_strncpy( wadfolder, wadname, sizeof( wadfolder ));
				FS_DefaultExtension( wadname, ".wad" );
				anywadname = false;
			}

			// make wadname from wad fullpath
			FS_FileBase( search->wad->filename, shortname );
			FS_DefaultExtension( shortname, ".wad" );

			// quick reject by wadname
			if( !anywadname && Q_stricmp( wadname, shortname ))
				continue;

			// NOTE: we can't using long names for wad,
			// because we using original wad names[16];
			FS_FileBase( name, shortname );

			lump = W_FindLump( search->wad, shortname, type );
			if( lump )
			{
				if( index )
					*index = lump - search->wad->lumps;
				return search;
			}
		}
		else
		{
			char	netpath[MAX_SYSPATH];
			Q_sprintf( netpath, "%s%s", search->filename, name );
			if( FS_SysFileExists( netpath ))
			{
				if( index != NULL ) *index = -1;
				return search;
			}
		}
	}

	if( fs_ext_path && ( pEnvPath = getenv( "Path" )))
	{
		char	netpath[MAX_SYSPATH];

		// clear searchpath
		search = &fs_directpath;
		memset( search, 0, sizeof( searchpath_t ));

		// root folder has a more priority than netpath
		Q_strncpy( search->filename, host.rootdir, sizeof( search->filename ));
		Q_strcat( search->filename, "\\" );
		Q_snprintf( netpath, MAX_SYSPATH, "%s%s", search->filename, name );

		if( FS_SysFileExists( netpath ))
		{
			if( index != NULL )
				*index = -1;
			return search;
		}

		// search for environment path
		while( pEnvPath )
		{
			char *end = Q_strchr( pEnvPath, ';' );
			if( !end ) break;
			Q_strncpy( search->filename, pEnvPath, (end - pEnvPath) + 1 );
			Q_strcat( search->filename, "\\" );
			Q_snprintf( netpath, MAX_SYSPATH, "%s%s", search->filename, name );

			if( FS_SysFileExists( netpath ))
			{
				if( index != NULL )
					*index = -1;
				return search;
			}
			pEnvPath += (end - pEnvPath) + 1; // move pointer
		}
	}

	if( index != NULL )
		*index = -1;

	return NULL;
}


/*
===========
FS_OpenReadFile

Look for a file in the search paths and open it in read-only mode
===========
*/
file_t *FS_OpenReadFile( const char *filename, const char *mode, qboolean gamedironly )
{
	searchpath_t	*search;
	int		pack_ind;

	search = FS_FindFile( filename, &pack_ind, gamedironly );

	// not found?
	if( search == NULL )
		return NULL; 

	if( search->pack )
		return FS_OpenPackedFile( search->pack, pack_ind );
	else if( search->wad )
		return NULL; // let W_LoadFile get lump correctly
	else if( pack_ind < 0 )
	{
		// found in the filesystem?
		char	path [MAX_SYSPATH];
		Q_sprintf( path, "%s%s", search->filename, filename );
		return FS_SysOpen( path, mode );
	} 
	return NULL;
}

/*
=============================================================================

MAIN PUBLIC FUNCTIONS

=============================================================================
*/
/*
====================
FS_Open

Open a file. The syntax is the same as fopen
====================
*/
file_t *FS_Open( const char *filepath, const char *mode, qboolean gamedironly )
{
	if( host.type == HOST_NORMAL || host.type == HOST_DEDICATED )
          {
		// some stupid mappers used leading '/' or '\' in path to models or sounds
		if( filepath[0] == '/' || filepath[0] == '\\' ) filepath++;
		if( filepath[0] == '/' || filepath[0] == '\\' ) filepath++;
          }

	if( FS_CheckNastyPath( filepath, false ))
		return NULL;

	// if the file is opened in "write", "append", or "read/write" mode
	if( mode[0] == 'w' || mode[0] == 'a'|| mode[0] == 'e' || Q_strchr( mode, '+' ))
	{
		char	real_path[MAX_SYSPATH];

		// open the file on disk directly
		Q_sprintf( real_path, "%s/%s", fs_gamedir, filepath );
		FS_CreatePath( real_path );// Create directories up to the file
		return FS_SysOpen( real_path, mode );
	}
	
	// else, we look at the various search paths and open the file in read-only mode
	return FS_OpenReadFile( filepath, mode, gamedironly );
}

/*
====================
FS_Close

Close a file
====================
*/
int FS_Close( file_t *file )
{
	if( !file ) return 0;

	if( close( file->handle ))
		return EOF;

	Mem_Free( file );
	return 0;
}

/*
====================
FS_Write

Write "datasize" bytes into a file
====================
*/
long FS_Write( file_t *file, const void *data, size_t datasize )
{
	long	result;

	if( !file ) return 0;

	// if necessary, seek to the exact file position we're supposed to be
	if( file->buff_ind != file->buff_len )
		lseek( file->handle, file->buff_ind - file->buff_len, SEEK_CUR );

	// purge cached data
	FS_Purge( file );

	// write the buffer and update the position
	result = write( file->handle, data, (long)datasize );
	file->position = lseek( file->handle, 0, SEEK_CUR );
	if( file->real_length < file->position )
		file->real_length = file->position;

	if( result < 0 )
		return 0;
	return result;
}

/*
====================
FS_Read

Read up to "buffersize" bytes from a file
====================
*/
long FS_Read( file_t *file, void *buffer, size_t buffersize )
{
	long	count, done;
	long	nb;

	// nothing to copy
	if( buffersize == 0 ) return 1;

	// Get rid of the ungetc character
	if( file->ungetc != EOF )
	{
		((char*)buffer)[0] = file->ungetc;
		buffersize--;
		file->ungetc = EOF;
		done = 1;
	}
	else done = 0;

	// first, we copy as many bytes as we can from "buff"
	if( file->buff_ind < file->buff_len )
	{
		count = file->buff_len - file->buff_ind;

		done += ((long)buffersize > count ) ? count : (long)buffersize;
		memcpy( buffer, &file->buff[file->buff_ind], done );
		file->buff_ind += done;

		buffersize -= done;
		if( buffersize == 0 )
			return done;
	}

	// NOTE: at this point, the read buffer is always empty

	// we must take care to not read after the end of the file
	count = file->real_length - file->position;

	// if we have a lot of data to get, put them directly into "buffer"
	if( buffersize > sizeof( file->buff ) / 2 )
	{
		if( count > (long)buffersize )
			count = (long)buffersize;
		lseek( file->handle, file->offset + file->position, SEEK_SET );
		nb = read (file->handle, &((byte *)buffer)[done], count );

		if( nb > 0 )
		{
			done += nb;
			file->position += nb;
			// purge cached data
			FS_Purge( file );
		}
	}
	else
	{
		if( count > (long)sizeof( file->buff ))
			count = (long)sizeof( file->buff );
		lseek( file->handle, file->offset + file->position, SEEK_SET );
		nb = read( file->handle, file->buff, count );

		if( nb > 0 )
		{
			file->buff_len = nb;
			file->position += nb;

			// copy the requested data in "buffer" (as much as we can)
			count = (long)buffersize > file->buff_len ? file->buff_len : (long)buffersize;
			memcpy( &((byte *)buffer)[done], file->buff, count );
			file->buff_ind = count;
			done += count;
		}
	}

	return done;
}

/*
====================
FS_Print

Print a string into a file
====================
*/
int FS_Print( file_t *file, const char *msg )
{
	return FS_Write( file, msg, Q_strlen( msg ));
}

/*
====================
FS_Printf

Print a string into a file
====================
*/
int FS_Printf( file_t *file, const char* format, ... )
{
	int	result;
	va_list	args;

	va_start( args, format );
	result = FS_VPrintf( file, format, args );
	va_end( args );

	return result;
}

/*
====================
FS_VPrintf

Print a string into a file
====================
*/
int FS_VPrintf( file_t *file, const char* format, va_list ap )
{
	int	len;
	long	buff_size = MAX_SYSPATH;
	char	*tempbuff;

	if( !file ) return 0;

	while( 1 )
	{
		tempbuff = (char *)Mem_Alloc( fs_mempool, buff_size );
		len = Q_vsprintf( tempbuff, format, ap );

		if( len >= 0 && len < buff_size )
			break;

		Mem_Free( tempbuff );
		buff_size *= 2;
	}

	len = write( file->handle, tempbuff, len );
	Mem_Free( tempbuff );

	return len;
}

/*
====================
FS_Getc

Get the next character of a file
====================
*/
int FS_Getc( file_t *file )
{
	char	c;

	if( FS_Read( file, &c, 1) != 1 )
		return EOF;

	return c;
}

/*
====================
FS_UnGetc

Put a character back into the read buffer (only supports one character!)
====================
*/
int FS_UnGetc( file_t *file, byte c )
{
	// If there's already a character waiting to be read
	if( file->ungetc != EOF )
		return EOF;

	file->ungetc = c;
	return c;
}

/*
====================
FS_Gets

Same as fgets
====================
*/
int FS_Gets( file_t *file, byte *string, size_t bufsize )
{
	int	c, end = 0;

	while( 1 )
	{
		c = FS_Getc( file );
		if( c == '\r' || c == '\n' || c < 0 )
			break;
		if( end < bufsize - 1 )
			string[end++] = c;
	}
	string[end] = 0;

	// remove \n following \r
	if( c == '\r' )
	{
		c = FS_Getc( file );
		if( c != '\n' )
			FS_UnGetc( file, (byte)c );
	}

	return c;
}

/*
====================
FS_Seek

Move the position index in a file
====================
*/
int FS_Seek( file_t *file, long offset, int whence )
{
	// compute the file offset
	switch( whence )
	{
	case SEEK_CUR:
		offset += file->position - file->buff_len + file->buff_ind;
		break;
	case SEEK_SET:
		break;
	case SEEK_END:
		offset += file->real_length;
		break;
	default: 
		return -1;
	}
	
	if( offset < 0 || offset > file->real_length )
		return -1;

	// if we have the data in our read buffer, we don't need to actually seek
	if( file->position - file->buff_len <= offset && offset <= file->position )
	{
		file->buff_ind = offset + file->buff_len - file->position;
		return 0;
	}

	// Purge cached data
	FS_Purge( file );

	if( lseek( file->handle, file->offset + offset, SEEK_SET ) == -1 )
		return -1;
	file->position = offset;

	return 0;
}

/*
====================
FS_Tell

Give the current position in a file
====================
*/
long FS_Tell( file_t* file )
{
	if( !file ) return 0;
	return file->position - file->buff_len + file->buff_ind;
}

/*
====================
FS_Eof

indicates at reached end of file
====================
*/
qboolean FS_Eof( file_t* file )
{
	if( !file ) return true;
	return (( file->position - file->buff_len + file->buff_ind ) == file->real_length ) ? true : false;
}

/*
====================
FS_Purge

Erases any buffered input or output data
====================
*/
void FS_Purge( file_t* file )
{
	file->buff_len = 0;
	file->buff_ind = 0;
	file->ungetc = EOF;
}

/*
============
FS_LoadFile

Filename are relative to the xash directory.
Always appends a 0 byte.
============
*/
byte *FS_LoadFile( const char *path, long *filesizeptr, qboolean gamedironly )
{
	file_t	*file;
	byte	*buf = NULL;
	long	filesize = 0;

	file = FS_Open( path, "rb", gamedironly );

	if( file )
	{
		filesize = file->real_length;
		buf = (byte *)Mem_Alloc( fs_mempool, filesize + 1 );
		buf[filesize] = '\0';
		FS_Read( file, buf, filesize );
		FS_Close( file );
	}
	else
	{
		buf = W_LoadFile( path, &filesize, gamedironly );
	}

	if( filesizeptr )
		*filesizeptr = filesize;

	return buf;
}

/*
============
FS_OpenFile

Simply version of FS_Open
============
*/
file_t *FS_OpenFile( const char *path, long *filesizeptr, qboolean gamedironly )
{
	file_t	*file = FS_Open( path, "rb", gamedironly );

	if( filesizeptr )
	{
		if( file ) *filesizeptr = file->real_length;
		else *filesizeptr = 0;
	}
	return file;
}

/*
============
FS_WriteFile

The filename will be prefixed by the current game directory
============
*/
qboolean FS_WriteFile( const char *filename, const void *data, long len )
{
	file_t *file;

	file = FS_Open( filename, "wb", false );

	if( !file )
	{
		MsgDev( D_ERROR, "FS_WriteFile: failed on %s\n", filename);
		return false;
	}

	FS_Write( file, data, len );
	FS_Close( file );

	return true;
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

	length = Q_strlen( path ) - 1;
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
	src = path + Q_strlen( path ) - 1;

	while( *src != '/' && src != path )
	{
		// it has an extension
		if( *src == '.' ) return;                 
		src--;
	}
	Q_strcat( path, extension );
}

/*
==================
FS_FileExists

Look for a file in the packages and in the filesystem
==================
*/
qboolean FS_FileExists( const char *filename, qboolean gamedironly )
{
	if( FS_FindFile( filename, NULL, gamedironly ))
		return true;
	return false;
}

/*
==================
FS_GetDiskPath

Build direct path for file in the filesystem
return NULL for file in pack
==================
*/
const char *FS_GetDiskPath( const char *name, qboolean gamedironly )
{
	int		index;
	searchpath_t	*search;

	search = FS_FindFile( name, &index, gamedironly );

	if( search )
	{
		if( index != -1 ) // file in pack or wad
			return NULL;
		return va( "%s%s", search->filename, name );
	}

	return NULL;
}

/*
==================
FS_CheckForCrypt

return true if library is crypted
==================
*/
qboolean FS_CheckForCrypt( const char *dllname )
{
	file_t	*f;
	int	key;

	f = FS_Open( dllname, "rb", false );
	if( !f ) return false;

	FS_Seek( f, 64, SEEK_SET );	// skip first 64 bytes
	FS_Read( f, &key, sizeof( key ));
	FS_Close( f );

	return ( key == 0x12345678 ) ? true : false;
}

/*
==================
FS_FindLibrary

search for library, assume index is valid
only for internal use
==================
*/
dll_user_t *FS_FindLibrary( const char *dllname, qboolean directpath )
{
	string		dllpath;
	searchpath_t	*search;
	dll_user_t	*hInst;
	int		i, index;
	int		start = 0;

	// check for bad exports
	if( !dllname || !*dllname )
		return NULL;

	fs_ext_path = directpath;

	// HACKHACK: remove absoulte path to valve folder
	if( !Q_strnicmp( dllname, "..\\valve\\", 9 ) || !Q_strnicmp( dllname, "../valve/", 9 ))
		start += 9;

	// replace all backward slashes
	for( i = 0; i < Q_strlen( dllname ); i++ )
	{
		if( dllname[i+start] == '\\' ) dllpath[i] = '/';
		else dllpath[i] = Q_tolower( dllname[i+start] );
	}
	dllpath[i] = '\0';

	FS_DefaultExtension( dllpath, ".dll" );	// apply ext if forget
	search = FS_FindFile( dllpath, &index, false );

	if( !search )
	{
		fs_ext_path = false;
		if( directpath ) return NULL;	// direct paths fails here

		// trying check also 'bin' folder for indirect paths
		Q_strncpy( dllpath, dllname, sizeof( dllpath ));
		search = FS_FindFile( dllpath, &index, false );
		if( !search ) return NULL;	// unable to find
	}

	// all done, create dll_user_t struct
	hInst = Mem_Alloc( host.mempool, sizeof( dll_user_t ));	

	// save dllname for debug purposes
	Q_strncpy( hInst->dllName, dllname, sizeof( hInst->dllName ));

	// shortPath is used for LibraryLoadSymbols only
	Q_strncpy( hInst->shortPath, dllpath, sizeof( hInst->shortPath ));

	hInst->encrypted = FS_CheckForCrypt( dllpath );

	if( index < 0 && !hInst->encrypted )
	{
		Q_snprintf( hInst->fullPath, sizeof( hInst->fullPath ), "%s%s", search->filename, dllpath );
		hInst->custom_loader = false;	// we can loading from disk and use normal debugging
	}
	else
	{
		Q_strncpy( hInst->fullPath, dllpath, sizeof( hInst->fullPath ));
		hInst->custom_loader = true;	// loading from pack or wad - for release, debug don't working
	}
	fs_ext_path = false; // always reset direct paths
		
	return hInst;
}

/*
==================
FS_FileSize

return size of file in bytes
==================
*/
long FS_FileSize( const char *filename, qboolean gamedironly )
{
	file_t	*fp;
	int	length = 0;
	
	fp = FS_Open( filename, "rb", gamedironly );

	if( fp )
	{
		// it exists
		FS_Seek( fp, 0, SEEK_END );
		length = FS_Tell( fp );
		FS_Close( fp );
	}
	return length;
}

/*
==================
FS_FileLength

return size of file in bytes
==================
*/
long FS_FileLength( file_t *f )
{
	if( !f ) return 0;
	return f->real_length;
}

/*
==================
FS_FileTime

return time of creation file in seconds
==================
*/
long FS_FileTime( const char *filename, qboolean gamedironly )
{
	searchpath_t	*search;
	int		pack_ind;
	
	search = FS_FindFile( filename, &pack_ind, gamedironly );
	if( !search ) return -1; // doesn't exist

	if( search->pack ) // grab pack filetime
		return search->pack->filetime;
	else if( search->wad ) // grab wad filetime
		return search->wad->filetime;
	else if( pack_ind < 0 )
	{
		// found in the filesystem?
		char	path [MAX_SYSPATH];

		Q_sprintf( path, "%s%s", search->filename, filename );
		return FS_SysFileTime( path );
	}
	return -1; // doesn't exist
}

/*
==================
FS_Rename

rename specified file from gamefolder
==================
*/
qboolean FS_Rename( const char *oldname, const char *newname )
{
	char	oldpath[MAX_SYSPATH], newpath[MAX_SYSPATH];
	qboolean	iRet;

	if( !oldname || !newname || !*oldname || !*newname )
		return false;

	Q_snprintf( oldpath, sizeof( oldpath ), "%s%s", fs_gamedir, oldname );
	Q_snprintf( newpath, sizeof( newpath ), "%s%s", fs_gamedir, newname );

	COM_FixSlashes( oldpath );
	COM_FixSlashes( newpath );

	iRet = rename( oldpath, newpath );

	return (iRet == 0);
}

/*
==================
FS_Delete

delete specified file from gamefolder
==================
*/
qboolean FS_Delete( const char *path )
{
	char	real_path[MAX_SYSPATH];
	qboolean	iRet;

	if( !path || !*path )
		return false;

	Q_snprintf( real_path, sizeof( real_path ), "%s%s", fs_gamedir, path );
	COM_FixSlashes( real_path );
	iRet = remove( real_path );

	return (iRet == 0);
}

/*
==================
FS_FileCopy

==================
*/
qboolean FS_FileCopy( file_t *pOutput, file_t *pInput, int fileSize )
{
	char	*buf = Mem_Alloc( fs_mempool, FILE_COPY_SIZE );
	int	size, readSize;
	qboolean	done = true;

	while( fileSize > 0 )
	{
		if( fileSize > FILE_COPY_SIZE )
			size = FILE_COPY_SIZE;
		else size = fileSize;

		if(( readSize = FS_Read( pInput, buf, size )) < size )
		{
			MsgDev( D_ERROR, "FS_FileCopy: unexpected end of input file (%d < %d)\n", readSize, size );
			fileSize = 0;
			done = false;
			break;
		}

		FS_Write( pOutput, buf, readSize );
		fileSize -= size;
	}

	Mem_Free( buf );
	return done;
}

/*
===========
FS_Search

Allocate and fill a search structure with information on matching filenames.
===========
*/
search_t *FS_Search( const char *pattern, int caseinsensitive, int gamedironly )
{
	search_t		*search = NULL;
	searchpath_t	*searchpath;
	pack_t		*pak;
	wfile_t		*wad;
	int		i, basepathlength, numfiles, numchars;
	int		resultlistindex, dirlistindex;
	const char	*slash, *backslash, *colon, *separator;
	string		netpath, temp;
	stringlist_t	resultlist;
	stringlist_t	dirlist;
	char		*basepath;

	for( i = 0; pattern[i] == '.' || pattern[i] == ':' || pattern[i] == '/' || pattern[i] == '\\'; i++ );

	if( i > 0 )
	{
		MsgDev( D_INFO, "FS_Search: don't use punctuation at the beginning of a search pattern!\n");
		return NULL;
	}

	stringlistinit( &resultlist );
	stringlistinit( &dirlist );
	slash = Q_strrchr( pattern, '/' );
	backslash = Q_strrchr( pattern, '\\' );
	colon = Q_strrchr( pattern, ':' );
	separator = max( slash, backslash );
	separator = max( separator, colon );
	basepathlength = separator ? (separator + 1 - pattern) : 0;
	basepath = Mem_Alloc( fs_mempool, basepathlength + 1 );
	if( basepathlength ) memcpy( basepath, pattern, basepathlength );
	basepath[basepathlength] = 0;

	// search through the path, one element at a time
	for( searchpath = fs_searchpaths; searchpath; searchpath = searchpath->next )
	{
		if( gamedironly && !FBitSet( searchpath->flags, FS_GAMEDIR_PATH ))
			continue;

		// is the element a pak file?
		if( searchpath->pack )
		{
			// look through all the pak file elements
			pak = searchpath->pack;
			for( i = 0; i < pak->numfiles; i++ )
			{
				Q_strncpy( temp, pak->files[i].name, sizeof( temp ));
				while( temp[0] )
				{
					if( matchpattern( temp, (char *)pattern, true ))
					{
						for( resultlistindex = 0; resultlistindex < resultlist.numstrings; resultlistindex++ )
						{
							if( !Q_strcmp( resultlist.strings[resultlistindex], temp ))
								break;
						}

						if( resultlistindex == resultlist.numstrings )
							stringlistappend( &resultlist, temp );
					}
					// strip off one path element at a time until empty
					// this way directories are added to the listing if they match the pattern
					slash = Q_strrchr( temp, '/' );
					backslash = Q_strrchr( temp, '\\' );
					colon = Q_strrchr( temp, ':' );
					separator = temp;
					if( separator < slash )
						separator = slash;
					if( separator < backslash )
						separator = backslash;
					if( separator < colon )
						separator = colon;
					*((char *)separator) = 0;
				}
			}
		}
		else if( searchpath->wad )
		{
			string	wadpattern, wadname, temp2;
			char	type = W_TypeFromExt( pattern );
			qboolean	anywadname = true;
			string	wadfolder;

			// quick reject by filetype
			if( type == TYP_NONE ) continue;
			FS_ExtractFilePath( pattern, wadname );
			FS_FileBase( pattern, wadpattern );
			wadfolder[0] = '\0';

			if( Q_strlen( wadname ))
			{
				FS_FileBase( wadname, wadname );
				Q_strncpy( wadfolder, wadname, sizeof( wadfolder ));
				FS_DefaultExtension( wadname, ".wad" );
				anywadname = false;
			}

			// make wadname from wad fullpath
			FS_FileBase( searchpath->wad->filename, temp2 );
			FS_DefaultExtension( temp2, ".wad" );

			// quick reject by wadname
			if( !anywadname && Q_stricmp( wadname, temp2 ))
				continue;

			// look through all the wad file elements
			wad = searchpath->wad;
			for( i = 0; i < wad->numlumps; i++ )
			{
				// if type not matching, we already have no chance ...
				if( type != TYP_ANY && wad->lumps[i].type != type )
					continue;

				// build the lumpname with image suffix (if present)
				Q_snprintf( temp, sizeof( temp ), "%s%s", wad->lumps[i].name, wad_hints[wad->lumps[i].img_type].ext );

				while( temp[0] )
				{
					if( matchpattern( temp, wadpattern, true ))
					{
						for( resultlistindex = 0; resultlistindex < resultlist.numstrings; resultlistindex++ )
						{
							if( !Q_strcmp( resultlist.strings[resultlistindex], temp ))
								break;
						}

						if( resultlistindex == resultlist.numstrings )
						{
							// build path: wadname/lumpname.ext
							Q_snprintf( temp2, sizeof(temp2), "%s/%s", wadfolder, temp );
							FS_DefaultExtension( temp2, va(".%s", W_ExtFromType( wad->lumps[i].type )));
							stringlistappend( &resultlist, temp2 );
						}
					}

					// strip off one path element at a time until empty
					// this way directories are added to the listing if they match the pattern
					slash = Q_strrchr( temp, '/' );
					backslash = Q_strrchr( temp, '\\' );
					colon = Q_strrchr( temp, ':' );
					separator = temp;
					if( separator < slash )
						separator = slash;
					if( separator < backslash )
						separator = backslash;
					if( separator < colon )
						separator = colon;
					*((char *)separator) = 0;
				}
			}
		}
		else
		{
			// get a directory listing and look at each name
			Q_sprintf( netpath, "%s%s", searchpath->filename, basepath );
			stringlistinit( &dirlist );
			listdirectory( &dirlist, netpath );
			for( dirlistindex = 0; dirlistindex < dirlist.numstrings; dirlistindex++ )
			{
				Q_sprintf( temp, "%s%s", basepath, dirlist.strings[dirlistindex] );
				if( matchpattern( temp, (char *)pattern, true ))
				{
					for( resultlistindex = 0; resultlistindex < resultlist.numstrings; resultlistindex++ )
					{
						if( !Q_strcmp( resultlist.strings[resultlistindex], temp ))
							break;
					}

					if( resultlistindex == resultlist.numstrings )
						stringlistappend( &resultlist, temp );
				}
			}

			stringlistfreecontents( &dirlist );
		}
	}

	if( resultlist.numstrings )
	{
		stringlistsort( &resultlist );
		numfiles = resultlist.numstrings;
		numchars = 0;

		for( resultlistindex = 0; resultlistindex < resultlist.numstrings; resultlistindex++ )
			numchars += (int)Q_strlen( resultlist.strings[resultlistindex]) + 1;
		search = Mem_Alloc( fs_mempool, sizeof(search_t) + numchars + numfiles * sizeof( char* ));
		search->filenames = (char **)((char *)search + sizeof( search_t ));
		search->filenamesbuffer = (char *)((char *)search + sizeof( search_t ) + numfiles * sizeof( char* ));
		search->numfilenames = (int)numfiles;
		numfiles = numchars = 0;

		for( resultlistindex = 0; resultlistindex < resultlist.numstrings; resultlistindex++ )
		{
			size_t	textlen;

			search->filenames[numfiles] = search->filenamesbuffer + numchars;
			textlen = Q_strlen(resultlist.strings[resultlistindex]) + 1;
			memcpy( search->filenames[numfiles], resultlist.strings[resultlistindex], textlen );
			numfiles++;
			numchars += (int)textlen;
		}
	}

	stringlistfreecontents( &resultlist );

	Mem_Free( basepath );

	return search;
}

void FS_InitMemory( void )
{
	fs_mempool = Mem_AllocPool( "FileSystem Pool" );	

	// add a path separator to the end of the basedir if it lacks one
	if( fs_basedir[0] && fs_basedir[Q_strlen(fs_basedir) - 1] != '/' && fs_basedir[Q_strlen(fs_basedir) - 1] != '\\' )
		Q_strncat( fs_basedir, "/", sizeof( fs_basedir ));

	fs_searchpaths = NULL;
}

/*
=============================================================================

WADSYSTEM PRIVATE COMMON FUNCTIONS

=============================================================================
*/
// associate extension with wad type
static const wadtype_t wad_types[7] =
{
{ "pal", TYP_PALETTE	}, // palette
{ "dds", TYP_DDSTEX 	}, // DDS image
{ "lmp", TYP_GFXPIC		}, // quake1, hl pic
{ "fnt", TYP_QFONT		}, // hl qfonts
{ "mip", TYP_MIPTEX		}, // hl/q1 mip
{ "txt", TYP_SCRIPT		}, // scripts
{ NULL,  TYP_NONE		}
};

// suffix converts to img_type and back
static const wadtype_t wad_hints[10] =
{
{ "",	 IMG_DIFFUSE	}, // no suffix
{ "_mask", IMG_ALPHAMASK	}, // alpha-channel stored to another lump
{ "_norm", IMG_NORMALMAP	}, // indexed normalmap
{ "_spec", IMG_GLOSSMAP	}, // grayscale\color specular
{ "_gpow", IMG_GLOSSPOWER	}, // grayscale gloss power
{ "_hmap", IMG_HEIGHTMAP	}, // heightmap (can be converted to normalmap)
{ "_luma", IMG_LUMA		}, // self-illuminate parts on the diffuse
{ "_adec", IMG_DECAL_ALPHA	}, // classic HL-decal (with alpha-channel)
{ "_cdec", IMG_DECAL_COLOR	}, // paranoia decal (base 127 127 127)
{ NULL,    0		}  // terminator
};

/*
===========
W_TypeFromExt

Extracts file type from extension
===========
*/
static char W_TypeFromExt( const char *lumpname )
{
	const char	*ext = FS_FileExtension( lumpname );
	const wadtype_t	*type;

	// we not known about filetype, so match only by filename
	if( !Q_strcmp( ext, "*" ) || !Q_strcmp( ext, "" ))
		return TYP_ANY;
	
	for( type = wad_types; type->ext; type++ )
	{
		if( !Q_stricmp( ext, type->ext ))
			return type->type;
	}
	return TYP_NONE;
}

/*
===========
W_ExtFromType

Convert type to extension
===========
*/
static const char *W_ExtFromType( char lumptype )
{
	const wadtype_t	*type;

	// we not known aboyt filetype, so match only by filename
	if( lumptype == TYP_NONE || lumptype == TYP_ANY )
		return "";

	for( type = wad_types; type->ext; type++ )
	{
		if( lumptype == type->type )
			return type->ext;
	}
	return "";
}

/*
===========
W_HintFromSuf

Convert name suffix into image type
===========
*/
char W_HintFromSuf( const char *lumpname )
{
	char		barename[64];
	char		suffix[8];
	size_t		namelen;
	const wadtype_t	*hint;

	// trying to extract hint from the name
	FS_FileBase( lumpname, barename );
	namelen = Q_strlen( barename );

	if( namelen <= HINT_NAMELEN )
		return IMG_DIFFUSE;

	Q_strncpy( suffix, barename + namelen - HINT_NAMELEN, sizeof( suffix ));

	// we not known about filetype, so match only by filename
	for( hint = wad_hints; hint->ext; hint++ )
	{
		if( !Q_stricmp( suffix, hint->ext ))
			return hint->type;
	}

	// no any special type was found
	return IMG_DIFFUSE;
}

/*
===========
W_FindLump

Serach for already existed lump
===========
*/
static dlumpinfo_t *W_FindLump( wfile_t *wad, const char *name, const char matchtype )
{
	char		img_type = IMG_DIFFUSE;
	char		barename[64], suffix[8];
	int		left, right;
	size_t		namelen;
	const wadtype_t	*hint;

	if( !wad || !wad->lumps || matchtype == TYP_NONE )
		return NULL;

	// trying to extract hint from the name
	FS_FileBase( name, barename );
	namelen = Q_strlen( barename );

	if( namelen > HINT_NAMELEN )
	{
		Q_strncpy( suffix, barename + namelen - HINT_NAMELEN, sizeof( suffix ));

		// we not known about filetype, so match only by filename
		for( hint = wad_hints; hint->ext; hint++ )
		{
			if( !Q_stricmp( suffix, hint->ext ))
			{
				img_type = hint->type;
				break;
			}
		}

		if( img_type != IMG_DIFFUSE )
			barename[namelen - HINT_NAMELEN] = '\0'; // kill the suffix
	}

	// look for the file (binary search)
	left = 0;
	right = wad->numlumps - 1;
	
	while( left <= right )
	{
		int	middle = (left + right) / 2;
		int	diff = Q_stricmp( wad->lumps[middle].name, barename );

		if( !diff )
		{
			if( wad->lumps[middle].img_type > img_type )
				diff = 1;
			else if( wad->lumps[middle].img_type < img_type )
				diff = -1;
			else if(( matchtype == TYP_ANY ) || ( matchtype == wad->lumps[middle].type ))
				return &wad->lumps[middle]; // found
			else if( wad->lumps[middle].type < matchtype )
				diff = 1;
			else if( wad->lumps[middle].type > matchtype )
				diff = -1;
			else break; // not found
		}

		// if we're too far in the list
		if( diff > 0 ) right = middle - 1;
		else left = middle + 1;
	}

	return NULL;
}

/*
====================
W_AddFileToWad

Add a file to the list of files contained into a package
and sort LAT in alpha-bethical order
====================
*/
static dlumpinfo_t *W_AddFileToWad( const char *name, wfile_t *wad, dlumpinfo_t *newlump )
{
	int		left, right;
	dlumpinfo_t	*plump;

	// look for the slot we should put that file into (binary search)
	left = 0;
	right = wad->numlumps - 1;

	while( left <= right )
	{
		int	middle = ( left + right ) / 2;
		int	diff = Q_stricmp( wad->lumps[middle].name, name );

		if( !diff )
		{
			if( wad->lumps[middle].img_type > newlump->img_type )
				diff = 1;
			else if( wad->lumps[middle].img_type < newlump->img_type )			
				diff = -1;
			else if( wad->lumps[middle].type < newlump->type )
				diff = 1;
			else if( wad->lumps[middle].type > newlump->type )
				diff = -1;
			else MsgDev( D_WARN, "Wad %s contains the file %s several times\n", wad->filename, name );
		}

		// If we're too far in the list
		if( diff > 0 ) right = middle - 1;
		else left = middle + 1;
	}

	// we have to move the right of the list by one slot to free the one we need
	plump = &wad->lumps[left];
	memmove( plump + 1, plump, ( wad->numlumps - left ) * sizeof( *plump ));
	wad->numlumps++;

	*plump = *newlump;
	memcpy( plump->name, name, sizeof( plump->name ));

	return plump;
}

/*
===========
W_ReadLump

reading lump into temp buffer
===========
*/
byte *W_ReadLump( wfile_t *wad, dlumpinfo_t *lump, long *lumpsizeptr )
{
	size_t	oldpos, size = 0;
	byte	*buf;

	// assume error
	if( lumpsizeptr ) *lumpsizeptr = 0;

	// no wads loaded
	if( !wad || !lump ) return NULL;

	oldpos = FS_Tell( wad->handle ); // don't forget restore original position

	if( FS_Seek( wad->handle, lump->filepos, SEEK_SET ) == -1 )
	{
		MsgDev( D_ERROR, "W_ReadLump: %s is corrupted\n", lump->name );
		FS_Seek( wad->handle, oldpos, SEEK_SET );
		return NULL;
	}

	buf = (byte *)Mem_Alloc( wad->mempool, lump->disksize );
	size = FS_Read( wad->handle, buf, lump->disksize );
	if( size < lump->disksize )
	{
		MsgDev( D_WARN, "W_ReadLump: %s is probably corrupted\n", lump->name );
		FS_Seek( wad->handle, oldpos, SEEK_SET );
		Mem_Free( buf );
		return NULL;
	}


	if( lumpsizeptr ) *lumpsizeptr = lump->size;
	FS_Seek( wad->handle, oldpos, SEEK_SET );

	return buf;
}

/*
===========
W_WriteLump

compress and write lump
===========
*/
qboolean W_WriteLump( wfile_t *wad, dlumpinfo_t *lump, const void *data, size_t datasize )
{
	if( !wad || !lump ) return false;

	if( !data || !datasize )
	{
		MsgDev( D_WARN, "W_WriteLump: ignore blank lump %s - nothing to save\n", lump->name );
		return false;
	}

	if( wad->mode == O_RDONLY )
	{
		MsgDev( D_ERROR, "W_WriteLump: %s opened in readonly mode\n", wad->filename ); 
		return false;
	}

	lump->size = lump->disksize = datasize;

	if( FS_Write( wad->handle, data, datasize ) == datasize )
		return true;		
	return false;
}

/*
=============================================================================

WADSYSTEM PUBLIC BASE FUNCTIONS

=============================================================================
*/
/*
===========
W_Open

open the wad for reading & writing
===========
*/
wfile_t *W_Open( const char *filename, const char *mode, int *error )
{
	dwadinfo_t	header;
	wfile_t		*wad = (wfile_t *)Mem_Alloc( fs_mempool, sizeof( wfile_t ));
	const char	*comment = "Created by Xash3D Engine.\0";
	int		i, ind, mod, opt, lumpcount;
	size_t		wadsize, lat_size;
	dlumpinfo_t	*srclumps;

	// parse the mode string
	switch( mode[0] )
	{
	case 'r':
		mod = O_RDONLY;
		opt = 0;
		break;
	case 'w':
		mod = O_WRONLY;
		opt = O_CREAT|O_TRUNC;
		break;
	case 'a':
		mod = O_WRONLY;
		opt = O_CREAT;
		break;
	default:
		MsgDev( D_ERROR, "W_Open(%s, %s): invalid mode\n", filename, mode );
		return NULL;
	}

	for( ind = 1; mode[ind] != '\0'; ind++ )
	{
		switch( mode[ind] )
		{
		case '+':
			mod = O_RDWR;
			break;
		case 'b':
			opt |= O_BINARY;
			break;
		default:
			MsgDev( D_ERROR, "W_Open: %s: unknown char in mode (%c)\n", filename, mode, mode[ind] );
			break;
		}
	}

	// NOTE: FS_Open is load wad file from the first pak in the list (while fs_ext_path is false)
	if( fs_ext_path ) wad->handle = FS_Open( filename, mode, false );
	else wad->handle = FS_Open( FS_FileWithoutPath( filename ), mode, false );

	if( wad->handle == NULL )
	{
		MsgDev( D_ERROR, "W_Open: couldn't open %s\n", filename );
		if( error ) *error = WAD_LOAD_COULDNT_OPEN;
		W_Close( wad );
		return NULL;
	}

	// copy wad name
	Q_strncpy( wad->filename, filename, sizeof( wad->filename ));
	wad->filetime = FS_SysFileTime( filename );
	wad->mempool = Mem_AllocPool( filename );

	wadsize = FS_FileLength( wad->handle );

	// if the file is opened in "write", "append", or "read/write" mode
	if( mod == O_WRONLY || !wadsize )
	{
		dwadinfo_t hdr;

		wad->numlumps = 0;		// blank wad
		wad->lumps = NULL;		//
		wad->mode = O_WRONLY;

		// save space for header
		hdr.ident = IDWAD3HEADER;
		hdr.numlumps = wad->numlumps;
		hdr.infotableofs = sizeof( dwadinfo_t );
		FS_Write( wad->handle, &hdr, sizeof( hdr ));
		FS_Write( wad->handle, comment, Q_strlen( comment ) + 1 );
		wad->infotableofs = FS_Tell( wad->handle );
	}
	else if( mod == O_RDWR || mod == O_RDONLY )
	{
		if( mod == O_RDWR )
			wad->mode = O_APPEND;
		else wad->mode = O_RDONLY;

		if( FS_Read( wad->handle, &header, sizeof( dwadinfo_t )) != sizeof( dwadinfo_t ))
		{
			MsgDev( D_ERROR, "W_Open: %s can't read header\n", filename );
			if( error ) *error = WAD_LOAD_BAD_HEADER;
			W_Close( wad );
			return NULL;
		}

		if( header.ident != IDWAD2HEADER && header.ident != IDWAD3HEADER )
		{
			MsgDev( D_ERROR, "W_Open: %s is not a WAD2 or WAD3 file\n", filename );
			if( error ) *error = WAD_LOAD_BAD_HEADER;
			W_Close( wad );
			return NULL;
		}

		lumpcount = header.numlumps;

		if( lumpcount >= MAX_FILES_IN_WAD && wad->mode == O_APPEND )
		{
			MsgDev( D_WARN, "W_Open: %s is full (%i lumps)\n", filename, lumpcount );
			if( error ) *error = WAD_LOAD_TOO_MANY_FILES;
			wad->mode = O_RDONLY; // set read-only mode
		}
		else if( lumpcount <= 0 && wad->mode == O_RDONLY )
		{
			MsgDev( D_ERROR, "W_Open: %s has no lumps\n", filename );
			if( error ) *error = WAD_LOAD_NO_FILES;
			W_Close( wad );
			return NULL;
		}
		else if( error ) *error = WAD_LOAD_OK;

		wad->infotableofs = header.infotableofs; // save infotableofs position

		if( FS_Seek( wad->handle, wad->infotableofs, SEEK_SET ) == -1 )
		{
			MsgDev( D_ERROR, "W_Open: %s can't find lump allocation table\n", filename );
			if( error ) *error = WAD_LOAD_BAD_FOLDERS;
			W_Close( wad );
			return NULL;
		}

		lat_size = lumpcount * sizeof( dlumpinfo_t );

		// NOTE: lumps table can be reallocated for O_APPEND mode
		srclumps = (dlumpinfo_t *)Mem_Alloc( wad->mempool, lat_size );

		if( FS_Read( wad->handle, srclumps, lat_size ) != lat_size )
		{
			MsgDev( D_ERROR, "W_ReadLumpTable: %s has corrupted lump allocation table\n", wad->filename );
			if( error ) *error = WAD_LOAD_CORRUPTED;
			Mem_Free( srclumps );
			W_Close( wad );
			return NULL;
		}

		// starting to add lumps
		wad->lumps = (dlumpinfo_t *)Mem_Alloc( wad->mempool, lat_size );
		wad->numlumps = 0;

		// sort lumps for binary search
		for( i = 0; i < lumpcount; i++ )
		{
			char	name[16];
			int	k;

			// cleanup lumpname
			Q_strnlwr( srclumps[i].name, name, sizeof( srclumps[i].name ));

			// check for '*' symbol issues (quake1)
			k = Q_strlen( Q_strrchr( name, '*' ));
			if( k ) name[Q_strlen( name ) - k] = '!';

			// check for Quake 'conchars' issues (only lmp loader really allows to read this lame pic)
			if( srclumps[i].type == 68 && !Q_stricmp( srclumps[i].name, "conchars" ))
				srclumps[i].type = TYP_GFXPIC; 

			W_AddFileToWad( name, wad, &srclumps[i] );
		}

		// release source lumps
		Mem_Free( srclumps );

		// if we are in append mode - we need started from infotableofs poisition
		// overwrite lumptable as well, we have her copy in wad->lumps
		if( wad->mode == O_APPEND )
			FS_Seek( wad->handle, wad->infotableofs, SEEK_SET );
	}

	// and leave the file open
	return wad;
}

/*
===========
W_Close

finalize wad or just close
===========
*/
void W_Close( wfile_t *wad )
{
	if( !wad ) return;

	if( wad->handle != NULL && ( wad->mode == O_APPEND || wad->mode == O_WRONLY ))
	{
		dwadinfo_t	hdr;
		long		ofs;

		// write the lumpinfo
		ofs = FS_Tell( wad->handle );
		FS_Write( wad->handle, wad->lumps, wad->numlumps * sizeof( dlumpinfo_t ));

		// write the header
		hdr.ident = IDWAD3HEADER;
		hdr.numlumps = wad->numlumps;
		hdr.infotableofs = ofs;

		FS_Seek( wad->handle, 0, SEEK_SET );
		FS_Write( wad->handle, &hdr, sizeof( hdr ));
	}

	Mem_FreePool( &wad->mempool );
	if( wad->handle != NULL )
		FS_Close( wad->handle );	

	Mem_Free( wad ); // free himself
}

/*
=============================================================================

FILESYSTEM IMPLEMENTATION

=============================================================================
*/
/*
===========
W_SaveLump

write new or replace existed lump
===========
*/
size_t W_SaveFile( wfile_t *wad, const char *lump, const void *data, size_t datasize, char type, qboolean replace )
{
	dlumpinfo_t	*find, newlump;
	size_t		lat_size, oldpos;
	char		hint, lumpname[64];

	if( !wad || !lump ) return -1;

	if( !data || !datasize )
	{
		MsgDev( D_WARN, "W_SaveLump: ignore blank lump %s - nothing to save\n", lump );
		return -1;
	}

	if( wad->mode == O_RDONLY )
	{
		MsgDev( D_ERROR, "W_SaveLump: %s opened in readonly mode\n", wad->filename ); 
		return -1;
	}

	if( wad->numlumps >= MAX_FILES_IN_WAD )
	{
		MsgDev( D_ERROR, "W_SaveLump: %s is full\n", wad->filename ); 
		return -1;
	}

	find = W_FindLump( wad, lump, type );

	if( find != NULL && replace )
	{
		if( FBitSet( find->attribs, ATTR_READONLY ))
		{
			// g-cont. i left this limitation as a protect of the replacement of compressed lumps
			MsgDev( D_ERROR, "W_ReplaceLump: %s is read-only\n", find->name );
			return -1;
		}

		if( datasize != find->size )
		{
			MsgDev( D_ERROR, "W_ReplaceLump: %s [%s] should be [%s]\n",
			lumpname, Q_memprint( datasize ), Q_memprint( find->size )); 
			return -1;
		}

		oldpos = FS_Tell( wad->handle ); // don't forget restore original position

		if( FS_Seek( wad->handle, find->filepos, SEEK_SET ) == -1 )
		{
			MsgDev( D_ERROR, "W_ReplaceLump: %s is corrupted\n", find->name );
			FS_Seek( wad->handle, oldpos, SEEK_SET );
			return -1;
		}

		if( FS_Write( wad->handle, data, datasize ) != find->disksize )
			MsgDev( D_WARN, "W_ReplaceLump: %s probably replaced with errors\n", find->name );

		// restore old position
		FS_Seek( wad->handle, oldpos, SEEK_SET );

		return wad->numlumps;
	}
	else
	{
		MsgDev( D_ERROR, "W_SaveLump: %s already exist\n", lump ); 
		return -1;
	}

	// prepare lump name
	Q_strncpy( lumpname, lump, sizeof( lumpname ));

	// extract image hint
	hint = W_HintFromSuf( lumpname );

	if( hint != IMG_DIFFUSE )
		lumpname[Q_strlen( lumpname ) - HINT_NAMELEN] = '\0'; // kill the suffix

	if( Q_strlen( lumpname ) >= WAD3_NAMELEN )
	{
		// name is too long
		MsgDev( D_ERROR, "W_SaveLump: %s more than %i symbols\n", lumpname, WAD3_NAMELEN ); 
		return -1;
	}

	lat_size = sizeof( dlumpinfo_t ) * (wad->numlumps + 1);

	// reallocate lumptable
	wad->lumps = (dlumpinfo_t *)Mem_Realloc( wad->mempool, wad->lumps, lat_size );

	memset( &newlump, 0, sizeof( newlump ));

	// write header
	Q_strnupr( lumpname, newlump.name, WAD3_NAMELEN );
	newlump.filepos = FS_Tell( wad->handle );
	newlump.attribs = ATTR_NONE;
	newlump.img_type = hint;
	newlump.type = type;

	if( !W_WriteLump( wad, &newlump, data, datasize ))
		return -1;

	// record entry and re-sort table
	W_AddFileToWad( lumpname, wad, &newlump );

	return wad->numlumps;
}

/*
===========
W_LoadFile

loading lump into the tmp buffer
===========
*/
static byte *W_LoadFile( const char *path, long *lumpsizeptr, qboolean gamedironly )
{
	searchpath_t	*search;
	int		index;

	search = FS_FindFile( path, &index, gamedironly );
	if( search && search->wad )
		return W_ReadLump( search->wad, &search->wad->lumps[index], lumpsizeptr ); 
	return NULL;
}