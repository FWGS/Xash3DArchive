//=======================================================================
//			Copyright XashXT Group 2008 ©
//		engfuncs.c - misc functions used by dlls'
//=======================================================================

#include "common.h"
#include "studio.h"
#include "mathlib.h"
#include "const.h"
#include "client.h"

/*
=============
COM_LoadFile

=============
*/
byte *COM_LoadFile( const char *filename, int usehunk, int *pLength )
{
	if( !filename || !*filename )
	{
		if( pLength ) *pLength = 0;
		return NULL;
	}

	return FS_LoadFile( filename, pLength );
}

/*
==============
COM_ParseFile

simple dlls version
==============
*/
char *COM_ParseFile( char *data, char *token )
{
	int	c, len;

	if( !token )
		return NULL;
	
	len = 0;
	token[0] = 0;
	
	if( !data )
		return NULL;
		
// skip whitespace
skipwhite:
	while(( c = *data ) <= ' ' )
	{
		if( c == 0 )
			return NULL;	// end of file;
		data++;
	}
	
	// skip // comments
	if( c=='/' && data[1] == '/' )
	{
		while( *data && *data != '\n' )
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if( c == '\"' )
	{
		data++;
		while( 1 )
		{
			c = *data++;
			if( c == '\"' || !c )
			{
				token[len] = 0;
				return data;
			}
			token[len] = c;
			len++;
		}
	}

	// parse single characters
	if( c == '{' || c == '}'|| c == ')'|| c == '(' || c == '\'' || c == ',' )
	{
		token[len] = c;
		len++;
		token[len] = 0;
		return data + 1;
	}

	// parse a regular word
	do
	{
		token[len] = c;
		data++;
		len++;
		c = *data;

		if( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ',' )
			break;
	} while( c > 32 );
	
	token[len] = 0;

	return data;
}

/*
=============
COM_AddAppDirectoryToSearchPath

=============
*/
void COM_AddAppDirectoryToSearchPath( const char *pszBaseDir, const char *appName )
{
	string	dir;

	if( !pszBaseDir || !appName )
	{
		MsgDev( D_ERROR, "COM_AddDirectorySearchPath: bad directory or appname\n" );
		return;
	}

	com.snprintf( dir, sizeof( dir ), "%s/%s", pszBaseDir, appName );
	FS_AddGameDirectory( dir, FS_GAMEDIR_PATH );
}

/*
===========
COM_ExpandFilename

Finds the file in the search path, copies over the name with the full path name.
This doesn't search in the pak file.
===========
*/
int COM_ExpandFilename( const char *fileName, char *nameOutBuffer, int nameOutBufferSize )
{
	const char	*path;
	char		rootdir[MAX_SYSPATH];
	char		result[MAX_SYSPATH];

	if( !fileName || !*fileName || !nameOutBuffer || nameOutBufferSize <= 0 )
		return 0;

	// filename examples:
	// media\sierra.avi - D:\Xash3D\valve\media\sierra.avi
	// models\barney.mdl - D:\Xash3D\bshift\models\barney.mdl
	if(( path = FS_GetDiskPath( fileName )) != NULL )
	{
		GetCurrentDirectory( MAX_SYSPATH, rootdir );
		com.sprintf( result, "%s/%s", rootdir, path );		

		// check for enough room
		if( com.strlen( result ) > nameOutBufferSize )
			return 0;

		com.strncpy( nameOutBuffer, result, nameOutBufferSize );
		return 1;
	}
	return 0;
}

/*
=============
pfnMemFgets

=============
*/
char *pfnMemFgets( byte *pMemFile, int fileSize, int *filePos, char *pBuffer, int bufferSize )
{
	int	i, last, stop;

	if( !pMemFile || !pBuffer || !filePos )
		return NULL;

	if( *filePos >= fileSize )
		return NULL;

	i = *filePos;
	last = fileSize;

	// fgets always NULL terminates, so only read bufferSize-1 characters
	if( last - *filePos > ( bufferSize - 1 ))
		last = *filePos + ( bufferSize - 1);

	stop = 0;

	// stop at the next newline (inclusive) or end of buffer
	while( i < last && !stop )
	{
		if( pMemFile[i] == '\n' )
			stop = 1;
		i++;
	}


	// if we actually advanced the pointer, copy it over
	if( i != *filePos )
	{
		// we read in size bytes
		int	size = i - *filePos;

		// copy it out
		Mem_Copy( pBuffer, pMemFile + *filePos, size );
		
		// If the buffer isn't full, terminate (this is always true)
		if( size < bufferSize ) pBuffer[size] = 0;

		// update file pointer
		*filePos = i;
		return pBuffer;
	}
	return NULL;
}

/*
====================
Cache_Check

consistency check
====================
*/
void *Cache_Check( byte *mempool, cache_user_t *c )
{
	if( !c->data )
		return NULL;

	if( !Mem_IsAllocated( mempool, c->data ))
		return NULL;

	return c->data;
}

/*
=============
pfnLoadFile

=============
*/
byte* pfnLoadFile( const char *filename, int *pLength )
{
	if( !filename || !*filename )
	{
		if( pLength ) *pLength = 0;
		return NULL;
	}

	return FS_LoadFile( filename, pLength );
}

void pfnFreeFile( void *buffer )
{
	FS_FreeFile( buffer );
}

/*
=============
pfnFileExists

=============
*/
int pfnFileExists( const char *filename )
{
	return FS_FileExists( filename );
}

/*
=============
pfnRandomLong

=============
*/
long pfnRandomLong( long lLow, long lHigh )
{
	return Com_RandomLong( lLow, lHigh );
}

/*
=============
pfnRandomFloat

=============
*/
float pfnRandomFloat( float flLow, float flHigh )
{
	return Com_RandomFloat( flLow, flHigh );
}

/*
=============
pfnTime

=============
*/
float pfnTime( void )
{
	return Sys_DoubleTime();
}
	
/*
=============
pfnCvar_RegisterVariable

=============
*/
cvar_t *pfnCvar_RegisterVariable( const char *szName, const char *szValue, int flags )
{
	return (cvar_t *)Cvar_Get( szName, szValue, flags|CVAR_CLIENTDLL, "" );
}

/*
=============
pfnCVarSetString

=============
*/
void pfnCVarSetString( const char *szName, const char *szValue )
{
	Cvar_Set( szName, szValue );
}

/*
=============
pfnCVarSetValue

=============
*/
void pfnCVarSetValue( const char *szName, float flValue )
{
	Cvar_SetFloat( szName, flValue );
}

/*
=============
pfnCVarGetString

=============
*/
char* pfnCVarGetString( const char *szName )
{
	return Cvar_VariableString( szName );
}

/*
=============
pfnCVarGetValue

=============
*/
float pfnCVarGetValue( const char *szName )
{
	return Cvar_VariableValue( szName );
}

/*
=============
pfnCVarGetPointer

can return NULL
=============
*/
cvar_t *pfnCVarGetPointer( const char *szVarName )
{
	return (cvar_t *)Cvar_FindVar( szVarName );
}
	
/*
=============
pfnAddCommand

=============
*/
int pfnAddCommand( const char *cmd_name, xcommand_t func )
{
	if( !cmd_name || !*cmd_name )
		return 0;

	// NOTE: if( func == NULL ) cmd will be forwarded to a server
	Cmd_AddCommand( cmd_name, func, "" );

	return 1;
}

/*
=============
pfnDelCommand

=============
*/
void pfnDelCommand( const char *cmd_name )
{
	if( !cmd_name || !*cmd_name ) return;

	Cmd_RemoveCommand( cmd_name );
}

/*
=============
pfnCmd_Args

=============
*/
char *pfnCmd_Args( void )
{
	return Cmd_Args();
}

/*
=============
pfnCmd_Argv

=============
*/
char *pfnCmd_Argv( int argc )
{
	if( argc >= 0 && argc < Cmd_Argc())
		return Cmd_Argv( argc );
	return "";
}

/*
=============
pfnCmd_Argc

=============
*/
int pfnCmd_Argc( void )
{
	return Cmd_Argc();
}

/*
=============
Con_Printf

=============
*/
void Con_Printf( char *szFmt, ... )
{
	char	buffer[2048];	// must support > 1k messages
	va_list	args;

	va_start( args, szFmt );
	com.vsnprintf( buffer, 2048, szFmt, args );
	va_end( args );

	com.print( buffer );
}

/*
=============
Con_DPrintf

=============
*/
void Con_DPrintf( char *szFmt, ... )
{
	char	buffer[2048];	// must support > 1k messages
	va_list	args;

	if( host.developer < D_AICONSOLE )
		return;

	va_start( args, szFmt );
	com.vsnprintf( buffer, 2048, szFmt, args );
	va_end( args );

	com.print( buffer );
}

/*
=============
pfnLoadLibrary

=============
*/
void *pfnLoadLibrary( const char *name )
{
	return FS_LoadLibrary( name, false );
}

/*
=============
pfnGetProcAddress

=============
*/
void *pfnGetProcAddress( void *hInstance, const char *name )
{
	return FS_GetProcAddress( hInstance, name );
}

/*
=============
pfnFreeLibrary

=============
*/
void pfnFreeLibrary( void *hInstance )
{
	FS_FreeLibrary( hInstance );
}

/*
=============
pfnGetGameDir

=============
*/
void pfnGetGameDir( char *szGetGameDir )
{
	char	rootdir[MAX_SYSPATH];

	if( !szGetGameDir ) return;
	GetCurrentDirectory( MAX_SYSPATH, rootdir );
	com.sprintf( szGetGameDir, "%s/%s", rootdir, GI->gamedir );
}