//=======================================================================
//			Copyright XashXT Group 2007 ©
//			platform.c - tools common dll
//=======================================================================

#include "launch.h"
#include "utils.h"
#include "mdllib.h"
#include "engine_api.h"
#include "mathlib.h"
#include "badimage.h"

stdlib_api_t com;
char  **com_argv;

#define MAX_SEARCHMASK	256

string	searchmask[MAX_SEARCHMASK];
int	num_searchmask = 0;
string	gs_searchmask;
string	gs_gamedir;
string	gs_basedir;
string	gs_filename;
byte	*error_bmp;
size_t	error_bmp_size;
static	double start, end;
qboolean	enable_log = false;
file_t	*bsplog = NULL;

void ClrMask( void )
{
	num_searchmask = 0;
	Mem_Set( searchmask, 0,  MAX_STRING * MAX_SEARCHMASK ); 
}

void AddMask( const char *mask )
{
	if( num_searchmask >= MAX_SEARCHMASK )
	{
		MsgDev( D_WARN, "AddMask: searchlist is full\n" );
		return;
	}
	com.strncpy( searchmask[num_searchmask], mask, MAX_STRING );
	num_searchmask++;
}

/*
================
Com_ValidScript

validate qc-script for unexcpected keywords
================
*/
qboolean Com_ValidScript( const char *token, qctype_t scripttype )
{ 
	if( !com.stricmp( token, "$spritename") && scripttype != QC_SPRITEGEN )
	{
		Msg( "%s probably spritegen qc.script, skipping...\n", gs_filename );
		return false;
	}
	else if( !com.stricmp( token, "$resample" ) && scripttype != QC_SPRITEGEN )
	{
		Msg( "%s probably spritegen qc.script, skipping...\n", gs_filename );
		return false;
	}
	else if( !com.stricmp( token, "$modelname" ) && scripttype != QC_STUDIOMDL )
	{
		Msg( "%s probably studio qc.script, skipping...\n", gs_filename );
		return false;
	}	
	else if( !com.stricmp( token, "$body" ) && scripttype != QC_STUDIOMDL )
	{
		Msg( "%s probably studio qc.script, skipping...\n", gs_filename );
		return false;
	}
	else if( !com.stricmp( token, "$wadname" ) && scripttype != QC_WADLIB )
	{
		Msg( "%s probably wadlib qc.script, skipping...\n", gs_filename );
		return false;
	}
	else if( !com.stricmp( token, "$mipmap" ) && scripttype != QC_WADLIB )
	{
		Msg("%s probably wadlib qc.script, skipping...\n", gs_filename );
		return false;
	}
	return true;
}

/*
==================
Init_Tools

==================
*/
void Init_Tools( const int argc, const char **argv )
{
	int	imageflags = 0;

	enable_log = false;

	switch( Sys.app_name )
	{
	case HOST_BSPLIB:
		if( !FS_GetParmFromCmdLine( "-game", gs_basedir, sizeof( gs_basedir )))
			com.strncpy( gs_basedir, Cvar_VariableString( "fs_defaultdir" ), sizeof( gs_basedir ));
		if( !FS_GetParmFromCmdLine( "+map", gs_filename, sizeof( gs_filename )))
			com.strncpy( gs_filename, "newmap", sizeof( gs_filename ));
		// initialize ImageLibrary
		start = Sys_DoubleTime();
		PrepareBSPModel( (int)argc, (char **)argv );
		break;
	case HOST_XIMAGE:
		imageflags |= (IL_USE_LERPING|IL_ALLOW_OVERWRITE|IL_IGNORE_MIPS);
		com_argv = (char **)argv;
		if( !FS_GetParmFromCmdLine( "-to", gs_filename, sizeof( gs_filename )))
		{
			gs_filename[0] = '\0'; // will be set later
		}
		else
		{
			// if output an 8-bit formats so keep 8-bit textures with palette
			if( !com.stricmp( gs_filename, "pcx" ) || !com.stricmp( gs_filename, "bmp" ))
				imageflags |= IL_KEEP_8BIT;
		}
		Image_Setup( NULL, imageflags );
		FS_InitRootDir(".");

		start = Sys_DoubleTime();
		Msg( "\n\n" ); // tabulation
		break;
	case HOST_SPRITE:
	case HOST_STUDIO:
	case HOST_WADLIB:
		imageflags |= IL_KEEP_8BIT;
		Image_Setup( NULL, imageflags );
	case HOST_RIPPER:
		// blamk image for missed resources
		error_bmp = (byte *)blank_bmp;
		error_bmp_size = sizeof( blank_bmp );
		FS_InitRootDir(".");

		start = Sys_DoubleTime();
		Msg( "\n\n" ); // tabulation
		break;
	case HOST_OFFLINE:
		break;
	}
}

void Tools_Main( void )
{
	search_t	*search;
	qboolean	(*CompileMod)( byte *mempool, const char *name, byte parms ) = NULL;
	byte	parms = 0; // future expansion
	int	i, j, numCompiledMods = 0;
	string	errorstring;

	Mem_Set( errorstring, 0, MAX_STRING ); 
	ClrMask();

	switch( Sys.app_name )
	{
	case HOST_SPRITE: 
		CompileMod = CompileSpriteModel;
		AddMask( "*.qc" );
		break;
	case HOST_STUDIO:
		CompileMod = CompileStudioModel;
		AddMask( "*.qc" );
		break;
	case HOST_BSPLIB: 
		AddMask( "*.map" );
		CompileBSPModel(); 
		break;
	case HOST_WADLIB:
		CompileMod = CompileWad3Archive;
		AddMask( "*.qc" );
		break;
	case HOST_XIMAGE:
		CompileMod = ConvertImages;
		break;
	case HOST_RIPPER:
		CompileMod = ConvertResource;
		Conv_RunSearch();
		break;
	case HOST_OFFLINE:
		break;
	}
	if( !CompileMod ) goto elapced_time; // jump to shutdown

	// using custom mask
	if( FS_GetParmFromCmdLine( "-file", gs_searchmask, sizeof( gs_searchmask )))
	{
		ClrMask(); // clear all previous masks
		AddMask( gs_searchmask ); // custom mask

		if( Sys.app_name == HOST_XIMAGE && !gs_filename[0] )
		{
			const char	*ext = FS_FileExtension( gs_searchmask );
                              
			if( !com.strcmp( ext, "" ) || !com.strcmp( ext, "*" )); // blend mode
			else com.strncpy( gs_filename, ext, sizeof( gs_filename ));
		}
	}

	Msg( "Converting ...\n\n" );

	// search by mask		
	for( i = 0; i < num_searchmask; i++ )
	{
		// skip blank mask
		if( !com.strlen( searchmask[i] )) continue;
		search = FS_Search( searchmask[i], true, false );
		if( !search ) continue; // try next mask

		for( j = 0; j < search->numfilenames; j++ )
		{
			if( CompileMod( Sys.basepool, search->filenames[j], parms ))
				numCompiledMods++;
		}
		Mem_Free( search );
	}
	if( numCompiledMods == 0 ) 
	{
		if( !num_searchmask ) com.strncpy( errorstring, "files", MAX_STRING );
		for( j = 0; j < num_searchmask; j++ ) 
		{
			if( !com.strlen( searchmask[j] )) continue;
			com.strncat( errorstring, va("%s ", searchmask[j]), MAX_STRING );
		}
		Sys_Break( "no %s found in this folder!\n", errorstring );
	}
elapced_time:
	end = Sys_DoubleTime();
	Msg( "%5.3f seconds elapsed\n", end - start );
	if( numCompiledMods > 1 ) Msg( "total %d files proceed\n", numCompiledMods );
}

void Free_Tools( void )
{
	if( Sys.app_name == HOST_RIPPER )
	{
		// finalize qc-script
		Skin_FinalizeScript();
	}
	else if( Sys.app_name == HOST_BSPLIB )
	{
		if( bsplog ) FS_Close( bsplog );
	}
}