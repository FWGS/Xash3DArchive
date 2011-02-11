//=======================================================================
//			Copyright XashXT Group 2007 ©
//			host.c - dedicated and shared host
//=======================================================================

#include "common.h"
#include "netchan.h"
#include "protocol.h"
#include "cm_local.h"
#include "input.h"

host_parm_t	host;	// host parms
stdlib_api_t	com;

convar_t	*host_serverstate;
convar_t	*host_gameloaded;
convar_t	*host_limitlocal;
convar_t	*host_cheats;
convar_t	*host_maxfps;
convar_t	*host_framerate;
convar_t	*con_gamemaps;

// these cvars will be duplicated on each client across network
int Host_ServerState( void )
{
	return Cvar_VariableInteger( "host_serverstate" );
}

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

void Host_Null( void )
{
	// just a stub for some commands in dedicated-mode
}

/*
================
Host_NewGame
================
*/
qboolean Host_NewGame( const char *mapName, qboolean loadGame )
{
	qboolean	iRet;

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
	Cvar_FullSet( "host_serverstate", va( "%i", state ), CVAR_INIT );
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

qboolean Host_IsLocalGame( void )
{
	if( CL_Active() && SV_Active() && CL_GetMaxClients() == 1 )
		return true;
	return false;
}

static int num_decals;

/*
=================
Host_RegisterDecal
=================
*/
qboolean Host_RegisterDecal( const char *name )
{
	char	shortname[CS_SIZE];
	int	i;

	if( !name || !name[0] )
		return 0;

	FS_FileBase( name, shortname );

	for( i = 1; i < MAX_DECALS && host.draw_decals[i][0]; i++ )
	{
		if( !com.stricmp( host.draw_decals[i], shortname ))
			return true;
	}

	if( i == MAX_DECALS )
	{
		MsgDev( D_ERROR, "Host_RegisterDecal: MAX_DECALS limit exceeded\n" );
		return false;
	}

	// register new decal
	com.strncpy( host.draw_decals[i], shortname, sizeof( host.draw_decals[i] ));
	num_decals++;

	return true;
}

/*
=================
Host_InitDecals
=================
*/
void Host_InitDecals( void )
{
	search_t	*t;
	int	i;

	Mem_Set( host.draw_decals, 0, sizeof( host.draw_decals ));
	num_decals = 0;

	// lookup all decals in decals.wad
	t = FS_Search( "decals.wad/*.*", true );

	for( i = 0; t && i < t->numfilenames; i++ )
	{
		if( !Host_RegisterDecal( t->filenames[i] ))
			break;
	}

	if( t ) Mem_Free( t );
	MsgDev( D_NOTE, "InitDecals: %i decals\n", num_decals );
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
	static qboolean	overflow = false;

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

Returns false while events is out
=================
*/
void Host_EventLoop( void )
{
	sys_event_t	ev;

	while( 1 )
	{
		ev = Sys_GetEvent();
		switch( ev.type )
		{
		case SE_NONE:
			Cbuf_Execute();
			return;
		case SE_KEY:
			Key_Event( ev.value[0], ev.value[1] );
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
			return;
		}
		if( ev.data ) Mem_Free( ev.data );
	}
}

/*
===================
Host_RestartAmbientSounds

Restarts the sounds to let demo writing them
===================
*/
void Host_RestartAmbientSounds( void )
{
	soundlist_t	soundInfo[100];
	int		i, nSounds;

	if( !SV_Active( )) return;

	nSounds = S_GetCurrentStaticSounds( soundInfo, 100 );
	
	for( i = 0; i < nSounds; i++)
	{
		if( soundInfo[i].looping && soundInfo[i].entnum != -1 )
		{
			S_StopSound( soundInfo[i].entnum, CHAN_STATIC, soundInfo[i].name );

			// FIXME: replace with SV_StartAmbientSound
			SV_StartSound( pfnPEntityOfEntIndex( soundInfo[i].entnum ), CHAN_STATIC,
			soundInfo[i].name, soundInfo[i].volume, soundInfo[i].attenuation, 0, soundInfo[i].pitch );
		}
	}
}

/*
===================
Host_FilterTime

Returns false if the time is too short to run a frame
===================
*/
qboolean Host_FilterTime( float time )
{
	static double	oldtime;
	float		fps;

	host.realtime += time;

	// dedicated's tic_rate regulates server frame rate.  Don't apply fps filter here.
	fps = host_maxfps->value;

	if( fps != 0 )
	{
		float	minframetime;

		// limit fps to withing tolerable range
		fps = bound( MIN_FPS, fps, MAX_FPS );

		minframetime = 1.0f / fps;

		if(( host.realtime - oldtime ) < minframetime )
		{
			// framerate is too high
			return false;		
		}
	}

	host.frametime = host.realtime - oldtime;
	host.realframetime = bound( MIN_FRAMETIME, host.frametime, MAX_FRAMETIME );
	oldtime = host.realtime;

	if( host_framerate->value > 0 && ( Host_IsLocalGame() || CL_IsPlaybackDemo() ))
	{
		float fps = host_framerate->value;
		if( fps > 1 ) fps = 1.0f / fps;
		host.frametime = fps;
	}
	else
	{	// don't allow really long or short frames
		host.frametime = bound( MIN_FRAMETIME, host.frametime, MAX_FRAMETIME );
	}
	
	return true;
}

/*
=================
Host_Frame
=================
*/
void Host_Frame( float time )
{
	if( setjmp( host.abortframe ))
		return;

	Host_InputFrame ();	// input frame
	Host_EventLoop();

	// decide the simulation time
	if( !Host_FilterTime( time ))
		return;

	Host_ServerFrame (); // server frame
	Host_ClientFrame (); // client frame

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
	static qboolean	recursive = false;
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
	// get developer mode
	host.developer = SI->developer;

	// get current hInstance
	host.hInst = GetModuleHandle( NULL );

	FS_LoadGameInfo( NULL );
	Image_Init( GI->gameHint, -1 );
	Sound_Init( GI->gameHint, -1 );

	host.mempool = Mem_AllocPool( "Zone Engine" );

	Host_InitEvents();
	Host_InitDecals();

	IN_Init();
}

void Host_FreeCommon( void )
{
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

	// init commands and vars
	if( host.developer >= 3 )
	{
		Cmd_AddCommand ( "sys_error", Sys_Error_f, "just throw a fatal error to test shutdown procedures");
		Cmd_AddCommand ( "host_error", Host_Error_f, "just throw a host error to test shutdown procedures");
		Cmd_AddCommand ( "crash", Host_Crash_f, "a way to force a bus error for development reasons");
		Cmd_AddCommand ( "net_error", Net_Error_f, "send network bad message from random place");
          }

	host_cheats = Cvar_Get( "sv_cheats", "0", CVAR_LATCH, "allow cheat variables to enable" );
	host_maxfps = Cvar_Get( "fps_max", "72", CVAR_ARCHIVE, "host fps upper limit" );
	host_framerate = Cvar_Get( "host_framerate", "0", 0, "locks frame timing to this value in seconds" );  
	host_serverstate = Cvar_Get( "host_serverstate", "0", CVAR_INIT, "displays current server state" );
	host_gameloaded = Cvar_Get( "host_gameloaded", "0", CVAR_INIT, "inidcates a loaded game.dll" );
	host_limitlocal = Cvar_Get( "host_limitlocal", "0", 0, "apply cl_cmdrate and rate to loopback connection" );
	con_gamemaps = Cvar_Get( "con_gamemaps", "1", CVAR_ARCHIVE, "when true show only maps in game folder, ignore maps in a base folder" );

	// content control
	Cvar_Get( "violence_hgibs", "1", CVAR_ARCHIVE, "show human gib entities" );
	Cvar_Get( "violence_agibs", "1", CVAR_ARCHIVE, "show alien gib entities" );
	Cvar_Get( "violence_hblood", "1", CVAR_ARCHIVE, "draw human blood" );
	Cvar_Get( "violence_ablood", "1", CVAR_ARCHIVE, "draw alien blood" );

	if( host.type != HOST_DEDICATED )
	{
		// when we in developer-mode automatically turn cheats on
		if( host.developer > 1 ) Cvar_SetFloat( "sv_cheats", 1.0f );
		Cbuf_AddText( "exec video.cfg\n" );
	}

	Mod_Init();
	NET_Init();
	Netchan_Init();

	SV_Init();
	CL_Init();

	if( host.type == HOST_DEDICATED )
	{
		Cmd_AddCommand( "quit", Sys_Quit, "quit the game" );
		Cmd_AddCommand( "exit", Sys_Quit, "quit the game" );
		Cmd_AddCommand( "@crashed", Host_Null, "" );

		// dedicated servers using settings from server.cfg file
		Cbuf_AddText( va( "exec %s\n", Cvar_VariableString( "servercfgfile" )));
		Cbuf_Execute();

		Cbuf_AddText( va( "map %s\n", Cvar_VariableString( "defaultmap" )));
	}
	else
	{
		Cmd_AddCommand( "minimize", Host_Minimize_f, "minimize main window to tray" );
		Cbuf_AddText( "exec config.cfg\n" );
	}

	// allow to change game from the console
	Cmd_AddCommand( "game", Host_ChangeGame_f, "change game" );

	host.errorframe = 0;
	Cbuf_Execute();

	SCR_CheckStartupVids();	// must be last
}

/*
=================
Host_Main
=================
*/
void Host_Main( void )
{
	static double	oldtime, newtime;

	oldtime = Sys_DoubleTime();

	// main window message loop
	while( host.type != HOST_OFFLINE )
	{
		newtime = Sys_DoubleTime ();
		Host_Frame( newtime - oldtime );
		oldtime = newtime;
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

	SV_Shutdown( false );
	CL_Shutdown();

	Mod_Shutdown();
	S_Shutdown();
	R_Shutdown();
	NET_Shutdown();
	Host_FreeCommon();
}

// main DLL entry point
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
	return TRUE;
}

/*
=================
Engine entry point
=================
*/
launch_exp_t EXPORT *CreateAPI( stdlib_api_t *input, void *unused )
{
         	static launch_exp_t Host;

	com = *input;
	Host.api_size = sizeof( launch_exp_t );
	Host.com_size = sizeof( stdlib_api_t );

	Host.Init = Host_Init;
	Host.Main = Host_Main;
	Host.Free = Host_Free;
	Host.CPrint = Host_Print;
	Host.CmdForward = Cmd_ForwardToServer;
	Host.CmdComplete = Cmd_AutoComplete;

	return &Host;
}