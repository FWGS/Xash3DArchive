//=======================================================================
//			Copyright XashXT Group 2007 ©
//			host.c - dedicated and shared host
//=======================================================================

#include "common.h"
#include "netchan.h"
#include "cm_local.h"
#include "input.h"

#define MAX_SYSEVENTS	1024	// system events

render_exp_t	*re;
vsound_exp_t	*se;
host_parm_t	host;	// host parms
stdlib_api_t	com, newcom;

dll_info_t render_dll = { "", NULL, "CreateAPI", NULL, NULL, 0, sizeof(render_exp_t), sizeof(stdlib_api_t) };
dll_info_t vsound_dll = { "", NULL, "CreateAPI", NULL, NULL, 0, sizeof(vsound_exp_t), sizeof(stdlib_api_t) };

cvar_t	*host_serverstate;
cvar_t	*host_cheats;
cvar_t	*host_maxfps;
cvar_t	*host_framerate;
cvar_t	*host_video;
cvar_t	*host_audio;

// these cvars will be duplicated on each client across network
int Host_ServerState( void ) { return Cvar_VariableInteger( "host_serverstate" ); }

int Host_CompareFileTime( long ft1, long ft2 )
{
	if( ft1 < ft2 )
	{
		return -1;
	}
	else if( ft1 > ft2 )
	{
		return 1;
	}
	return 0;
}

void Host_ShutdownServer( void )
{
	if( !SV_Active()) return;
	com.strncpy( host.finalmsg, "Server was killed\n", MAX_STRING );
	SV_Shutdown( false );
}

/*
================
Host_NewGame
================
*/
bool Host_NewGame( const char *mapName, bool loadGame )
{
	bool	iRet;

	iRet = SV_NewGame( mapName, loadGame );

	return iRet;
}
/*
================
Host_EndGame
================
*/
void Host_EndGame( const char *message, ... )
{
	va_list		argptr;
	static char	string[MAX_SYSPATH];
	
	va_start( argptr, message );
	com.vsprintf( string, message, argptr );
	va_end( argptr );

	MsgDev( D_INFO, "Host_EndGame: %s\n", string );
	
	if( SV_Active())
	{
		com.snprintf( host.finalmsg, sizeof( host.finalmsg ), "Host_EndGame: %s\n", string );
		SV_Shutdown( false );
	}
	
	if( host.type == HOST_DEDICATED )
		Sys_Break( "Host_EndGame: %s\n", string ); // dedicated servers exit
	
	if( CL_NextDemo( ));
	else CL_Disconnect();

	Host_AbortCurrentFrame ();
}

void Host_FreeRender( void )
{
	if( render_dll.link )
	{
		SCR_Shutdown ();
		re->Shutdown( true );
		Mem_Set( &re, 0, sizeof( re ));
	}
	Sys_FreeLibrary( &render_dll );
}

bool Host_InitRender( void )
{
	static render_imp_t	ri;
	launch_t		CreateRender;
	bool		result = false;
	
	ri.api_size = sizeof( render_imp_t );

          // studio callbacks
	ri.UpdateScreen = SCR_UpdateScreen;
	ri.StudioEvent = CL_StudioEvent;
	ri.StudioFxTransform = CL_StudioFxTransform;
	ri.ShowCollision = CM_DrawCollision;
	ri.GetClientEdict = CL_GetEntityByIndex;
	ri.GetPlayerInfo = CL_GetPlayerInfo;
	ri.GetLocalPlayer = CL_GetLocalPlayer;
	ri.GetMaxClients = CL_GetMaxClients;
	ri.DrawTriangles = Tri_DrawTriangles;
	ri.WndProc = IN_WndProc;          

	Sys_LoadLibrary( host_video->string, &render_dll );

	if( render_dll.link )
	{
		CreateRender = (void *)render_dll.main;
		re = CreateRender( &newcom, &ri );

		if( re->Init( true )) result = true;
	} 

	// video system not started, shutdown refresh subsystem
	if( !result ) Host_FreeRender();

	return result;
}

void Host_FreeSound( void )
{
	if( vsound_dll.link )
	{
		se->Shutdown();
		Mem_Set( &se, 0, sizeof( se ));
	}
	Sys_FreeLibrary( &vsound_dll );
}

bool Host_InitSound( void )
{
	static vsound_imp_t	si;
	launch_t		CreateSound;  
	bool		result = false;

	if( FS_CheckParm( "-nosound" ))
		return result;

	si.api_size = sizeof( vsound_imp_t );

	// sound callbacks
	si.GetEntitySpatialization = CL_GetEntitySpatialization;
	si.AmbientLevels = CM_AmbientLevels;
	si.GetClientEdict = CL_GetEntityByIndex;
	si.GetServerTime = CL_GetServerTime;
	si.GetAudioChunk = SCR_GetAudioChunk;
	si.GetMovieInfo = SCR_GetMovieInfo;
	si.IsInMenu = CL_IsInMenu;
	si.IsActive = CL_IsInGame;

	Sys_LoadLibrary( host_audio->string, &vsound_dll );

	if( vsound_dll.link )
	{
		CreateSound = (void *)vsound_dll.main;
		se = CreateSound( &newcom, &si );
	
		if( se->Init( host.hWnd )) result = true;
	}

	// audio system not started, shutdown sound subsystem
	if( !result ) Host_FreeSound();

	return result;
}

void Host_CheckChanges( void )
{
	int	num_changes;
	bool	audio_disabled = false;

	if( FS_CheckParm( "-nosound" ))
	{
		if( host.state == HOST_INIT )
			audio_disabled = true;
		host_audio->modified = false;
	}

	if( host_video->modified || host_audio->modified )
	{
		S_StopAllSounds();	// don't let them loop during the restart

		if( host_video->modified ) CL_ForceVid();
		if( host_audio->modified ) CL_ForceSnd();
	}
	else return;

	num_changes = 0;

	if( host_video->modified && CL_Active( ))
	{
		// we're in game and want keep decals when renderer is changed
		host.decalList = (decallist_t *)Z_Malloc(sizeof( decallist_t ) * MAX_DECALS );
		host.numdecals = CL_CreateDecalList( host.decalList, false );
	}

	// restart or change renderer
	while( host_video->modified )
	{
		host_video->modified = false;		// predict state

		Host_FreeRender();			// release render.dll
		if( !Host_InitRender( ))		// load it again
		{
			if( num_changes > host.num_video_dlls )
			{
				Sys_NewInstance( va("#%s", GI->gamefolder ), "fallback to dedicated mode\n" );
				return;
			}
			if( !com.strcmp( host.video_dlls[num_changes], host_video->string ))
				num_changes++; // already trying - failed
			Cvar_FullSet( "host_video", host.video_dlls[num_changes], CVAR_INIT|CVAR_ARCHIVE );
			num_changes++;
		}
		else SCR_Init ();
	}

	if( audio_disabled ) MsgDev( D_INFO, "Audio: Disabled\n" );

	num_changes = 0;

	// restart or change sound engine
	while( host_audio->modified )
	{
		host_audio->modified = false;		// predict state

		Host_FreeSound();			// release sound.dll
		if( !Host_InitSound( ))		// load it again
		{
			if( num_changes > host.num_audio_dlls )
			{
				MsgDev( D_ERROR, "couldn't initialize sound system\n" );
				return;
			}
			if( !com.strcmp( host.audio_dlls[num_changes], host_audio->string ))
				num_changes++; // already trying - failed
			Cvar_FullSet( "host_audio", host.audio_dlls[num_changes], CVAR_INIT|CVAR_ARCHIVE );
			num_changes++;
		}
	}
}

/*
================
Host_AbortCurrentFrame

aborts the current host frame and goes on with the next one
================
*/
void Host_AbortCurrentFrame( void )
{
	longjmp( host.abortframe, 1 );
}

/*
==================
Host_SetServerState
==================
*/
void Host_SetServerState( int state )
{
	Cvar_SetValue( "host_serverstate", state );
}

/*
=================
Host_VidRestart_f

Restart the video subsystem
=================
*/
void Host_VidRestart_f( void )
{
	host_video->modified = true;
}

/*
=================
Host_SndRestart_f

Restart the audio subsystem
=================
*/
void Host_SndRestart_f( void )
{
	host_audio->modified = true;
}

/*
=================
Host_ChangeGame_f

Change game modification
=================
*/
void Host_ChangeGame_f( void )
{
	int	i;

	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: game <directory>\n" );
		return;
	}

	// validate gamedir
	for( i = 0; i < SI->numgames; i++ )
	{
		if( !com.stricmp( SI->games[i]->gamefolder, Cmd_Argv( 1 )))
			break;
	}

	if( i == SI->numgames ) Msg( "%s not exist\n", Cmd_Argv( 1 ));
	else if( !com.stricmp( GI->gamefolder, Cmd_Argv( 1 )))
		Msg( "%s already active\n", Cmd_Argv( 1 ));	
	else Sys_NewInstance( Cmd_Argv( 1 ), "Host_ChangeGame\n" );
}

void Host_Minimize_f( void )
{
	if( host.hWnd ) ShowWindow( host.hWnd, SW_MINIMIZE );
}

bool Host_IsLocalGame( void )
{
	if( CL_Active() && SV_Active() && CL_GetMaxClients() == 1 )
		return true;
	return false;
}

/*
=================
Host_InitEvents
=================
*/
void Host_InitEvents( void )
{
	Mem_Set( host.events, 0, sizeof( host.events ));
	host.events_head = 0;
	host.events_tail = 0;
}

/*
=================
Host_PushEvent
=================
*/
void Host_PushEvent( sys_event_t *event )
{
	sys_event_t	*ev;
	static bool	overflow = false;

	ev = &host.events[host.events_head & (MAX_SYSEVENTS-1)];

	if( host.events_head - host.events_tail >= MAX_SYSEVENTS )
	{
		if( !overflow )
		{
			MsgDev( D_WARN, "Host_PushEvent overflow\n" );
			overflow = true;
		}
		if( ev->data ) Mem_Free( ev->data );
		host.events_tail++;
	}
	else overflow = false;

	*ev = *event;
	host.events_head++;
}

/*
=================
Host_GetEvent
=================
*/
sys_event_t Host_GetEvent( void )
{
	if( host.events_head > host.events_tail )
	{
		host.events_tail++;
		return host.events[(host.events_tail - 1) & (MAX_SYSEVENTS-1)];
	}
	return Sys_GetEvent();
}

/*
=================
Host_EventLoop

Returns last event time
=================
*/
int Host_EventLoop( void )
{
	sys_event_t	ev;

	while( 1 )
	{
		ev = Sys_GetEvent();
		switch( ev.type )
		{
		case SE_NONE:
			return ev.time;
		case SE_KEY:
			Key_Event( ev.value[0], ev.value[1], ev.time );
			break;
		case SE_CHAR:
			CL_CharEvent( ev.value[0] );
			break;
		case SE_MOUSE:
			CL_MouseEvent( ev.value[0], ev.value[1] );
			break;
		case SE_CONSOLE:
			Cbuf_AddText( va( "%s\n", ev.data ));
			break;
		default:
			MsgDev( D_ERROR, "Host_EventLoop: bad event type %i", ev.type );
			return false;
		}
		if( ev.data ) Mem_Free( ev.data );
	}
	return 0;	// never reached
}

/*
================
Host_ModifyTime
================
*/
int Host_ModifyTime( int msec )
{
	int	clamp_time;

	// modify time for debugging values
	if( host_framerate->value )
		msec = host_framerate->value * 1000;
	if( msec < 1 && host_framerate->value ) msec = 1;

	if( host.type == HOST_DEDICATED )
	{
		// dedicated servers don't want to clamp for a much longer
		// period, because it would mess up all the client's views of time.
		if( msec > 500 ) MsgDev( D_NOTE, "Host_ModifyTime: %i msec frame time\n", msec );
		clamp_time = 5000;
	}
	else if( SV_Active( ))
	{
		// for local single player gaming
		// we may want to clamp the time to prevent players from
		// flying off edges when something hitches.
		clamp_time = 200;
	}
	else
	{
		// clients of remote servers do not want to clamp time, because
		// it would skew their view of the server's time temporarily
		clamp_time = 5000;
	}

	if( msec > clamp_time )
		msec = clamp_time;

	return msec;
}

/*
=================
Host_Frame
=================
*/
void Host_Frame( void )
{
	static int	last_time;

	if( setjmp( host.abortframe ))
		return;

	rand(); // keep the random time dependent

	do {
		host.inputmsec = Host_EventLoop();
		if( last_time > host.inputmsec )
			last_time = host.inputmsec;
		host.frametime = host.inputmsec - last_time;
	} while( host.frametime < 1 );
	Cbuf_Execute();

	last_time = host.inputmsec;
	host.frametime = Host_ModifyTime( host.frametime );

	SV_Frame ( host.frametime ); // server frame

	if( host.type == HOST_DEDICATED )
	{
		// end frame of dedicated server
		host.framecount++;
		return;
	}

	// run event loop a second time to get server to client packets
	// without a frame of latency
	Host_EventLoop();
	Cbuf_Execute();

	CL_Frame ( host.frametime ); // client frame
	host.framecount++;
}
/*
================
Host_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void Host_Print( const char *txt )
{
	if( host.rd.target )
	{
		if(( com.strlen( txt ) + com.strlen( host.rd.buffer )) > ( host.rd.buffersize - 1 ))
		{
			if( host.rd.flush )
			{
				host.rd.flush( host.rd.address, host.rd.target, host.rd.buffer );
				*host.rd.buffer = 0;
			}
		}
		com.strcat( host.rd.buffer, txt );
		return;
	}
	Con_Print( txt ); // echo to client console
}

/*
=================
Host_Error
=================
*/
void Host_Error( const char *error, ... )
{
	static char	hosterror1[MAX_SYSPATH];
	static char	hosterror2[MAX_SYSPATH];
	static bool	recursive = false;
	va_list		argptr;

	va_start( argptr, error );
	com.vsprintf( hosterror1, error, argptr );
	va_end( argptr );

	CL_WriteMessageHistory (); // before com.error call

	if( host.framecount < 3 || host.state == HOST_SHUTDOWN )
	{
		SV_SysError( hosterror1 );
		com.error( "Host_InitError: %s", hosterror1 );
	}
	else if( host.framecount == host.errorframe )
	{
		SV_SysError( hosterror2 );
		com.error( "Host_MultiError: %s", hosterror2 );
		return;
	}
	else Msg( "Host_Error: %s", hosterror1 );

	if( recursive )
	{ 
		Msg( "Host_RecursiveError: %s", hosterror2 );
		SV_SysError( hosterror1 );
		com.error( hosterror1 );
		return; // don't multiple executes
	}

	recursive = true;
	com.strncpy( hosterror2, hosterror1, MAX_SYSPATH );
	host.errorframe = host.framecount; // to avoid multply calls per frame
	com.sprintf( host.finalmsg, "Server crashed: %s\n", hosterror1 );

	SV_Shutdown( false );
	CL_Drop(); // drop clients

	recursive = false;
	Host_AbortCurrentFrame();
	host.state = HOST_ERROR;
}

void Host_Error_f( void )
{
	const char *error = Cmd_Argv( 1 );

	if( !*error ) error = "Invoked host error";
	Host_Error( "%s\n", error );
}

void Sys_Error_f( void )
{
	const char *error = Cmd_Argv( 1 );

	if( !*error ) error = "Invoked sys error";
	SV_SysError( error );
	com.error( "%s\n", error );
}

void Net_Error_f( void )
{
	com.strncpy( host.finalmsg, Cmd_Argv( 1 ), sizeof( host.finalmsg ));
	SV_ForceError();
}

/*
=================
Host_Crash_f
=================
*/
static void Host_Crash_f( void )
{
	*(int *)0 = 0xffffffff;
}

void Host_InitCommon( const int argc, const char **argv )
{
	dll_info_t	check_vid, check_snd;
	search_t		*dlls;
	int		i;

	newcom = com;

	// overload some funcs
	newcom.error = Host_Error;

	// get developer mode
	host.developer = SI->developer;

	Host_InitEvents();

	FS_LoadGameInfo( NULL );
	Image_Init( GI->gameHint, -1 );
	Sound_Init( GI->gameHint, -1 );

	host.mempool = Mem_AllocPool( "Zone Engine" );

	IN_Init();

	// initialize audio\video multi-dlls system
	host.num_video_dlls = host.num_audio_dlls = 0;

	// make sure what global copy has no changed with any dll checking
	Mem_Copy( &check_vid, &render_dll, sizeof( dll_info_t ));
	Mem_Copy( &check_snd, &vsound_dll, sizeof( dll_info_t ));

	// checking dlls don't invoke crash!
	check_vid.crash = false;
	check_snd.crash = false;

	dlls = FS_Search( "*.dll", true );

	// couldn't find any dlls, render is missing (but i'm don't know how laucnher find engine :)
	// probably this should never happen
	if( !dlls ) Sys_NewInstance( "©", "" );

	for( i = 0; i < dlls->numfilenames; i++ )
	{
		if(!com.strnicmp( "vid_", dlls->filenames[i], 4 ))
		{
			// make sure what found library is valid
			if( Sys_LoadLibrary( dlls->filenames[i], &check_vid ))
			{
				MsgDev( D_NOTE, "Video[%i]: %s\n", host.num_video_dlls, dlls->filenames[i] );
				host.video_dlls[host.num_video_dlls] = copystring( dlls->filenames[i] );
				Sys_FreeLibrary( &check_vid );
				host.num_video_dlls++;
			}
		}
		else if(!com.strnicmp( "snd_", dlls->filenames[i], 4 ))
		{
			// make sure what found library is valid
			if( Sys_LoadLibrary( dlls->filenames[i], &check_snd ))
			{
				MsgDev( D_NOTE, "Audio[%i]: %s\n", host.num_audio_dlls, dlls->filenames[i] );
				host.audio_dlls[host.num_audio_dlls] = copystring( dlls->filenames[i] );
				Sys_FreeLibrary( &check_snd );
				host.num_audio_dlls++;
			}
		}
	}
	Mem_Free( dlls );
}

void Host_FreeCommon( void )
{
	IN_Shutdown();
	Netchan_Shutdown();
	Mem_FreePool( &host.mempool );
}

/*
=================
Host_Init
=================
*/
void Host_Init( const int argc, const char **argv )
{
	host.state = HOST_INIT;	// initialzation started
	host.type = g_Instance();

	Host_InitCommon( argc, argv );
	Key_Init();

	if( host.type != HOST_DEDICATED )
	{
		// get user configuration 
		Cbuf_AddText( "exec config.cfg\n" );
		Cbuf_Execute();
	}

	// init commands and vars
	if( host.developer >= 3 )
	{
		Cmd_AddCommand ( "sys_error", Sys_Error_f, "just throw a fatal error to test shutdown procedures");
		Cmd_AddCommand ( "host_error", Host_Error_f, "just throw a host error to test shutdown procedures");
		Cmd_AddCommand ( "crash", Host_Crash_f, "a way to force a bus error for development reasons");
		Cmd_AddCommand ( "net_error", Net_Error_f, "send network bad message from random place");
          }

	host_video = Cvar_Get( "host_video", "vid_gl.dll", CVAR_INIT|CVAR_ARCHIVE, "name of video rendering library");
	host_audio = Cvar_Get( "host_audio", "snd_dx.dll", CVAR_INIT|CVAR_ARCHIVE, "name of sound rendering library");
	host_cheats = Cvar_Get( "sv_cheats", "0", CVAR_LATCH, "allow cheat variables to enable" );
	host_maxfps = Cvar_Get( "fps_max", "72", CVAR_ARCHIVE, "host fps upper limit" );
	host_framerate = Cvar_Get( "host_framerate", "0", 0, "locks frame timing to this value in seconds" );  
	host_serverstate = Cvar_Get( "host_serverstate", "0", 0, "displays current server state" );

	// content control
	Cvar_Get( "violence_hgibs", "1", CVAR_INIT|CVAR_ARCHIVE, "content control disables human gibs" );
	Cvar_Get( "violence_agibs", "1", CVAR_INIT|CVAR_ARCHIVE, "content control disables alien gibs" );
	Cvar_Get( "violence_hblood", "1", CVAR_INIT|CVAR_ARCHIVE, "content control disables human blood" );
	Cvar_Get( "violence_ablood", "1", CVAR_INIT|CVAR_ARCHIVE, "content control disables alien blood" );

	if( host.type != HOST_DEDICATED )
	{
		// when we in developer-mode automatically turn cheats on
		if( host.developer > 1 ) Cvar_SetValue( "sv_cheats", 1.0f );
	}

	NET_Init();
	Netchan_Init();
	CM_InitPhysics();

	SV_Init();
	CL_Init();

	if( host.type == HOST_DEDICATED )
	{
		Cmd_AddCommand( "quit", Sys_Quit, "quit the game" );
		Cmd_AddCommand( "exit", Sys_Quit, "quit the game" );

		// dedicated servers using settings from server.cfg file
		Cbuf_AddText( va( "exec %s\n", Cvar_VariableString( "servercfgfile" )));
		Cbuf_Execute();

		Cbuf_AddText( va( "map %s\n", Cvar_VariableString( "defaultmap" )));
	}
	else
	{
		Cmd_AddCommand( "minimize", Host_Minimize_f, "minimize main window to tray" );
		Cmd_AddCommand( "vid_restart", Host_VidRestart_f, "restarts video system" );
		Cmd_AddCommand( "snd_restart", Host_SndRestart_f, "restarts audio system" );
	}

	// allow to change game from the console
	Cmd_AddCommand( "game", Host_ChangeGame_f, "change game" );

	host.errorframe = 0;
	Cbuf_Execute();
}

/*
=================
Host_Main
=================
*/
void Host_Main( void )
{
	// main window message loop
	while( host.type != HOST_OFFLINE )
	{
		IN_Frame();
		Host_Frame();
	}
}

/*
=================
Host_Shutdown
=================
*/
void Host_Free( void )
{
	if( host.state == HOST_SHUTDOWN )
		return;

	host.state = HOST_SHUTDOWN;	// prepare host to normal shutdown
	com.strncpy( host.finalmsg, "Server shutdown\n", MAX_STRING );

	CM_FreePhysics();
	Host_FreeRender();
	Host_FreeSound();

	SV_Shutdown( false );
	CL_Shutdown();
	NET_Shutdown();
	Host_FreeCommon();
}