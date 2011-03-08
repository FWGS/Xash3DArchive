//=======================================================================
//			Copyright XashXT Group 2007 �
//		       cl_cmds.c - client console commnds
//=======================================================================

#include "common.h"
#include "client.h"

/*
====================
CL_PlayVideo_f

movie <moviename>
====================
*/
void CL_PlayVideo_f( void )
{
	string	path;

	if( Cmd_Argc() != 2 && Cmd_Argc() != 3 )
	{
		Msg( "movie <moviename> [full]\n" );
		return;
	}

	if( cls.state == ca_active )
	{
		Msg( "Can't play movie while connected to a server.\nPlease disconnect first.\n" );
		return;
	}

	switch( Cmd_Argc( ))
	{
	case 2:	// simple user version
		com.snprintf( path, sizeof( path ), "media/%s.avi", Cmd_Argv( 1 ));
		SCR_PlayCinematic( path );
		break;
	case 3:	// sequenced cinematics used this
		SCR_PlayCinematic( Cmd_Argv( 1 ));
		break;
	}
}

/*
===============
CL_PlayCDTrack_f

Emulate cd-audio system
===============
*/
void CL_PlayCDTrack_f( void )
{
	const char	*command;
	static int	track = 0;
	static qboolean	paused = false;
	static qboolean	looped = false;

	if( Cmd_Argc() < 2 ) return;
	command = Cmd_Argv( 1 );

	if( !com.stricmp( command, "play" ))
	{
		track = bound( 1, com.atoi( Cmd_Argv( 2 )), MAX_CDTRACKS );
		S_StartBackgroundTrack( clgame.cdtracks[track-1], NULL );
		paused = false;
		looped = false;
	}
	else if( !com.stricmp( command, "loop" ))
	{
		track = bound( 1, com.atoi( Cmd_Argv( 2 )), MAX_CDTRACKS );
		S_StartBackgroundTrack( clgame.cdtracks[track-1], clgame.cdtracks[track-1] );
		paused = false;
		looped = true;
	}
	else if( !com.stricmp( command, "pause" ))
	{
		S_StreamSetPause( true );
		paused = true;
	}
	else if( !com.stricmp( command, "resume" ))
	{
		S_StreamSetPause( false );
		paused = false;
	}
	else if( !com.stricmp( command, "stop" ))
	{
		S_StopBackgroundTrack();
		paused = false;
		looped = false;
		track = 0;
	}
	else if( !com.stricmp( command, "info" ))
	{
		int	i, maxTrack;

		for( maxTrack = i = 0; i < MAX_CDTRACKS; i++ )
			if( com.strlen( clgame.cdtracks[i] )) maxTrack++;
			
		Msg( "%u tracks\n", maxTrack );
		if( track )
		{
			if( paused ) Msg( "Paused %s track %u\n", looped ? "looping" : "playing", track );
			else Msg( "Currently %s track %u\n", looped ? "looping" : "playing", track );
		}
		Msg( "Volume is %f\n", Cvar_VariableValue( "musicvolume" ));
		return;
	}
	else Msg( "cd: unknown command %s\n", command );
}

/*
===============
CL_Download_f

Request a download from the server
===============
*/
void CL_Download_f( void )
{
	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: download <filename>\n" );
		return;
	}
	CL_CheckOrDownloadFile( Cmd_Argv( 1 ));
}

/* 
================== 
CL_ScreenshotGetName
================== 
*/  
void CL_ScreenshotGetName( int lastnum, char *filename )
{
	int	a, b, c, d;

	if( !filename ) return;
	if( lastnum < 0 || lastnum > 9999 )
	{
		// bound
		com.sprintf( filename, "scrshots/%s/!error.bmp", clgame.mapname );
		return;
	}

	a = lastnum / 1000;
	lastnum -= a * 1000;
	b = lastnum / 100;
	lastnum -= b * 100;
	c = lastnum / 10;
	lastnum -= c * 10;
	d = lastnum;

	com.sprintf( filename, "scrshots/%s/shot%i%i%i%i.bmp", clgame.mapname, a, b, c, d );
}

/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/
/* 
================== 
CL_ScreenShot_f

normal screenshot
================== 
*/  
void CL_ScreenShot_f( void ) 
{
	int	i;
	string	checkname;

	// scan for a free filename
	for( i = 0; i <= 9999; i++ )
	{
		CL_ScreenshotGetName( i, checkname );
		if( !FS_FileExists( checkname, false ))
			break;
	}

	Con_ClearNotify();
	VID_ScreenShot( checkname, VID_SCREENSHOT );
}

void CL_EnvShot_f( void )
{
	if( Cmd_Argc() < 2 )
	{
		Msg( "Usage: envshot <shotname>\n" );
		return;
	}

	com.sprintf( cls.shotname, "gfx/env/%s", Cmd_Argv( 1 ));
	cls.scrshot_action = scrshot_envshot;	// build new frame for envshot
	cls.envshot_vieworg = NULL; // no custom view
}

void CL_SkyShot_f( void )
{
	if( Cmd_Argc() < 2 )
	{
		Msg( "Usage: skyshot <shotname>\n" );
		return;
	}

	com.sprintf( cls.shotname, "gfx/env/%s", Cmd_Argv( 1 ));
	cls.scrshot_action = scrshot_skyshot;	// build new frame for skyshot
	cls.envshot_vieworg = NULL; // no custom view
}

/* 
================== 
CL_LevelShot_f

splash logo while map is loading
================== 
*/ 
void CL_LevelShot_f( void )
{
	if( cls.scrshot_request != scrshot_plaque ) return;
	cls.scrshot_request = scrshot_inactive;

	// check for exist
	com.sprintf( cls.shotname, "levelshots/%s.bmp", clgame.mapname );
	if( !FS_FileExists( cls.shotname, true ))
		cls.scrshot_action = scrshot_plaque;	// build new frame for levelshot
	else cls.scrshot_action = scrshot_inactive;	// disable - not needs
}

/* 
================== 
CL_SaveShot_f

mini-pic in loadgame menu
================== 
*/ 
void CL_SaveShot_f( void )
{
	if( Cmd_Argc() < 2 )
	{
		Msg( "Usage: saveshot <savename>\n" );
		return;
	}

	com.sprintf( cls.shotname, "save/%s.bmp", Cmd_Argv( 1 ));
	cls.scrshot_action = scrshot_savegame;	// build new frame for saveshot
}

/* 
================== 
CL_DemoShot_f

mini-pic in playdemo menu
================== 
*/ 
void CL_DemoShot_f( void )
{
	if( Cmd_Argc() < 2 )
	{
		Msg( "Usage: demoshot <demoname>\n" );
		return;
	}

	com.sprintf( cls.shotname, "demos/%s.bmp", Cmd_Argv( 1 ));
	cls.scrshot_action = scrshot_demoshot; // build new frame for demoshot
}

/*
==============
CL_DeleteDemo_f

==============
*/
void CL_DeleteDemo_f( void )
{
	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: killdemo <name>\n" );
		return;
	}

	if( cls.demorecording && !com.stricmp( cls.demoname, Cmd_Argv( 1 )))
	{
		Msg( "Can't delete %s - recording\n", Cmd_Argv( 1 ));
		return;
	}

	// delete save and saveshot
	FS_Delete( va( "demos/%s.dem", Cmd_Argv( 1 )));
	FS_Delete( va( "demos/%s.bmp", Cmd_Argv( 1 )));
}

/*
=================
CL_SetSky_f

Set a specific sky and rotation speed
=================
*/
void CL_SetSky_f( void )
{
	if( Cmd_Argc() < 2 )
	{
		Msg( "Usage: skyname <shadername>\n" );
		return;
	}

	R_SetupSky( Cmd_Argv( 1 ));
}

/*
================
SCR_TimeRefresh_f

timerefresh [noflip]
================
*/
void SCR_TimeRefresh_f( void )
{
	int	i;
	double	start, stop;
	double	time;

	if( cls.state != ca_active )
		return;

	start = Sys_DoubleTime();

	if( Cmd_Argc() == 2 )
	{	
		// run without page flipping
		R_BeginFrame( false );
		for( i = 0; i < 128; i++ )
		{
			cl.refdef.viewangles[1] = i / 128.0 * 360.0f;
			R_RenderFrame( &cl.refdef, true );
		}
		R_EndFrame();
	}
	else
	{
		for( i = 0; i < 128; i++ )
		{
			cl.refdef.viewangles[1] = i / 128.0 * 360.0f;

			R_BeginFrame( true );
			R_RenderFrame( &cl.refdef, true );
			R_EndFrame();
		}
	}

	stop = Sys_DoubleTime ();
	time = (stop - start);
	Msg( "%f seconds (%f fps)\n", time, 128 / time );
}

/*
=============
SCR_Viewpos_f

viewpos
=============
*/
void SCR_Viewpos_f( void )
{
	Msg( "org ( %g %g %g )\n", cl.refdef.vieworg[0], cl.refdef.vieworg[1], cl.refdef.vieworg[2] );
	Msg( "ang ( %g %g %g )\n", cl.refdef.viewangles[0], cl.refdef.viewangles[1], cl.refdef.viewangles[2] );
}