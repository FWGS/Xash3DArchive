//=======================================================================
//			Copyright XashXT Group 2007 �
//			platform.c - game common dll
//=======================================================================

#include "platform.h"
#include "utils.h"
#include "bsplib.h"
#include "mdllib.h"

dll_info_t vprogs_dll = { "vprogs.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(vprogs_exp_t) };
vprogs_exp_t *PRVM;
stdlib_api_t com;
byte *basepool;
byte *zonepool;
byte *error_bmp;
size_t error_bmp_size;
static double start, end;
uint app_name = 0;

/*
==================
CommonInit

platform.dll needs for some setup operations
so do it manually
==================
*/
void InitPlatform ( int argc, char **argv )
{
	byte	bspflags = 0, qccflags = 0, roqflags = 0;
	char	source[64], gamedir[64];
	launch_t	CreateVprogs;

	basepool = Mem_AllocPool( "Temp" );
	// blamk image for missed resources
	error_bmp = FS_LoadInternal( "blank.bmp", &error_bmp_size );

	// for custom cmdline parsing
	com_argc = argc;
	com_argv = argv;
	app_name = g_Instance;

	switch( app_name )
	{
	case HOST_BSPLIB:
		if(!FS_GetParmFromCmdLine("-game", gamedir ))
			com.strncpy(gamedir, Cvar_VariableString( "fs_defaultdir" ), sizeof(gamedir));
		if(!FS_GetParmFromCmdLine("+map", source ))
			com.strncpy(source, "newmap", sizeof(source));
		if(FS_CheckParm("-vis")) bspflags |= BSP_ONLYVIS;
		if(FS_CheckParm("-rad")) bspflags |= BSP_ONLYRAD;
		if(FS_CheckParm("-full")) bspflags |= BSP_FULLCOMPILE;
		if(FS_CheckParm("-onlyents")) bspflags |= BSP_ONLYENTS;
		if(FS_CheckParm("-notjunc")) notjunc = true;

		PrepareBSPModel( gamedir, source, bspflags );
		break;
	case HOST_QCCLIB:
		Sys_LoadLibrary( &vprogs_dll ); // load qcclib
		CreateVprogs = (void *)vprogs_dll.main;
		PRVM = CreateVprogs( &com, NULL ); // second interface not allowed

		PRVM->Init( argc, argv );

		if(!FS_GetParmFromCmdLine("-dir", gamedir ))
			com.strncpy(gamedir, ".", sizeof(gamedir));
		if(!FS_GetParmFromCmdLine("+src", source ))
			com.strncpy(source, "progs.src", sizeof(source));

		start = Sys_DoubleTime();
		PRVM->PrepareDAT( gamedir, source );	
		break;
	case HOST_SPRITE:
	case HOST_STUDIO:
	case HOST_WADLIB:
		FS_InitRootDir(".");
		start = Sys_DoubleTime();
		break;
	case HOST_OFFLINE:
		break;
	}
}

void RunPlatform( void )
{
	search_t	*search;
	bool	(*CompileMod)( byte *mempool, const char *name, byte parms ) = NULL;
	char	filename[MAX_QPATH], typemod[16], searchmask[8][16], errorstring[256];
	byte	parms = 0; // future expansion
	int	i, j, numCompiledMods = 0;

	memset( searchmask, 0, 8 * 16 ); 
	memset( errorstring, 0, 256 ); 

	switch(app_name)
	{
	case HOST_SPRITE: 
		CompileMod = CompileSpriteModel;
		com.strcpy(typemod, "sprites" );
		com.strcpy(searchmask[0], "*.qc" );
		break;
	case HOST_STUDIO:
		CompileMod = CompileStudioModel;
		com.strcpy(typemod, "models" );
		com.strcpy(searchmask[0], "*.qc" );
		break;
	case HOST_BSPLIB: 
		com.strcpy(typemod, "maps" );
		com.strcpy(searchmask[0], "*.map" );
		CompileBSPModel(); 
		break;
	case HOST_WADLIB:
		CompileMod = CompileWad3Archive;
		com.strcpy(typemod, "wads" );
		com.strcpy(searchmask[0], "*.qc" );
		break;
	case HOST_QCCLIB: 
		com.strcpy(typemod, "progs" );
		com.strcpy(searchmask[0], "*.src" );
		PRVM->CompileDAT(); 
		break;
	case HOST_OFFLINE:
		com.strcpy(typemod, "things" );
		com.strcpy(searchmask[0], "*.*" );
		break;
	}
	if(!CompileMod) goto elapced_time; // jump to shutdown

	zonepool = Mem_AllocPool( "compiler" );
	if(!FS_GetParmFromCmdLine( "-file", filename ))
	{
		// search by mask		
		for( i = 0; i < 8; i++)
		{
			// skip blank mask
			if(!com.strlen(searchmask[i])) continue;
			search = FS_Search( searchmask[i], true );
			if(!search) continue; // try next mask

			for( j = 0; j < search->numfilenames; j++ )
			{
				if(CompileMod( zonepool, search->filenames[j], parms ))
					numCompiledMods++;
			}
		}
		if(numCompiledMods == 0) 
		{
			for(j = 0; j < 8; j++) 
			{
				if(!strlen(searchmask[j])) continue;
				strcat(errorstring, va("%s ", searchmask[j]));
			}
			Sys_Break("no %sfound in this folder!\n", errorstring );
		}
	}
	else CompileMod( zonepool, filename, parms );

elapced_time:
	end = Sys_DoubleTime();
	Msg ("%5.3f seconds elapsed\n", end - start);
	if(numCompiledMods > 1) Msg("total %d %s compiled\n", numCompiledMods, typemod );
}

void FreePlatform ( void )
{
	if( app_name == HOST_QCCLIB )
	{
		PRVM->Free();
		Sys_FreeLibrary( &vprogs_dll ); // free qcclib
	}

	Mem_Check(); // check for leaks
	Mem_FreePool( &basepool );
	Mem_FreePool( &zonepool );
}

launch_exp_t DLLEXPORT *CreateAPI( stdlib_api_t *input, void *unused )
{
	static launch_exp_t		Com;

	com = *input;

	// generic functions
	Com.api_size = sizeof(launch_exp_t);

	Com.Init = InitPlatform;
	Com.Main = RunPlatform;
	Com.Free = FreePlatform;

	return &Com;
}