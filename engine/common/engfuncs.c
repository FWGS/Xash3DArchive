//=======================================================================
//			Copyright XashXT Group 2008 ©
//		engfuncs.c - misc functions used by dlls'
//=======================================================================

#include "common.h"
#include "byteorder.h"
#include "mathlib.h"
#include "const.h"
#include "client.h"
#include "cvardef.h"

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
==============
pfnParseToken

simple dlls version
==============
*/
char *pfnParseToken( const char **data_p )
{
	int		c, len = 0;
	const char	*data;
	static char	token[8192];
	
	token[0] = 0;
	data = *data_p;
	
	if( !data ) 
	{
		*data_p = NULL;
		return NULL;
	}		

	// skip whitespace
skipwhite:
	while(( c = *data) <= ' ' )
	{
		if( c == 0 )
		{
			*data_p = NULL;
			return NULL; // end of file;
		}
		data++;
	}
	
	// skip // comments
	if( c=='/' && data[1] == '/' )
	{
		while( *data && *data != '\n' )
			data++;
		goto skipwhite;
	}

	// skip /* comments
	if( c=='/' && data[1] == '*' )
	{
		while( data[1] && (data[0] != '*' || data[1] != '/' ))
			data++;
		data += 2;
		goto skipwhite;
	}
	

	// handle quoted strings specially
	if( *data == '\"' || *data == '\'' )
	{
		data++;
		while( 1 )
		{
			c = *data++;
			if( c=='\"' || c=='\0' )
			{
				token[len] = 0;
				*data_p = data;
				return token;
			}
			token[len] = c;
			len++;
		}
	}

	// parse single characters
	if( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':' || c == ',' )
	{
		token[len] = c;
		data++;
		len++;
		token[len] = 0;
		*data_p = data;
		return token;
	}

	// parse a regular word
	do
	{
		token[len] = c;
		data++;
		len++;
		c = *data;
		if( c == '{' || c == '}'|| c == ')'|| c == '(' || c == '\'' || c == ':' || c == ',' )
			break;
	} while( c > 32 );
	
	token[len] = 0;
	*data_p = data;
	return token;
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
=============
pfnLoadFile

=============
*/
byte* pfnLoadFile( const char *filename, int *pLength )
{
	return FS_LoadFile( filename, pLength );
}

void pfnFreeFile( void *buffer )
{
	if( buffer ) Mem_Free( buffer );
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
pfnCVarRegister

=============
*/
cvar_t *pfnCVarRegister( const char *szName, const char *szValue, int flags, const char *szDesc )
{
	int	real_flags = 0;

	if( flags & FCVAR_ARCHIVE ) real_flags |= CVAR_ARCHIVE;
	if( flags & FCVAR_USERINFO ) real_flags |= CVAR_USERINFO;
	if( flags & FCVAR_SERVER ) real_flags |= CVAR_SERVERINFO;
	if( flags & FCVAR_SPONLY ) real_flags |= CVAR_CHEAT;
	if( flags & FCVAR_PRINTABLEONLY ) real_flags |= CVAR_PRINTABLEONLY;

	return Cvar_Get( szName, szValue, real_flags, szDesc );
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
	Cvar_SetValue( szName, flValue );
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
	return Cvar_FindVar( szVarName );
}
	
/*
=============
pfnAddCommand

=============
*/
void pfnAddCommand( const char *cmd_name, xcommand_t func, const char *cmd_desc )
{
	if( !cmd_name || !*cmd_name ) return;
	if( !cmd_desc ) cmd_desc = ""; // hidden for makehelep system

	// NOTE: if( func == NULL ) cmd will be forwarded to a server
	Cmd_AddCommand( cmd_name, func, cmd_desc );
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
const char *pfnCmd_Args( void )
{
	return Cmd_Args();
}

/*
=============
pfnCmd_Argv

=============
*/
const char *pfnCmd_Argv( int argc )
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
pfnCon_Printf

=============
*/
void pfnCon_Printf( char *szFmt, ... )
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
pfnCon_DPrintf

=============
*/
void pfnCon_DPrintf( char *szFmt, ... )
{
	char	buffer[2048];	// must support > 1k messages
	va_list	args;

	if( !host.developer ) return;

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
	if( !szGetGameDir ) return;
	com.strcpy( szGetGameDir, GI->gamedir );
}