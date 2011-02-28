
//=======================================================================
//			Copyright XashXT Group 2007 ©
//			utils.c - shared launcher utils
//=======================================================================

#include "launch.h"
#include "library.h"

#define MAX_QUED_EVENTS		256
#define MASK_QUED_EVENTS		(MAX_QUED_EVENTS - 1)
#define LOG_BUFSIZE			131072	// 128 kb

system_t		Sys;
stdlib_api_t	com;
sys_event_t	event_que[MAX_QUED_EVENTS];
int		event_head, event_tail;

dll_info_t engine_dll = { "engine.dll", NULL, "CreateAPI", NULL, NULL, 1, sizeof( launch_exp_t ), sizeof( stdlib_api_t ) };

static const char *show_credits = "\n\n\n\n\tCopyright XashXT Group %s ©\n\t\
          All Rights Reserved\n\n\t           Visit www.xash.ru\n";

// stubs
void NullInit( int argc, char **argv ) {}
void NullFunc( void ) {}
void NullPrint( const char *msg ) {}

void Sys_GetStdAPI( void )
{
	// interface validator
	com.api_size = sizeof( stdlib_api_t );
	com.com_size = sizeof( stdlib_api_t );

	// base events
	com.instance = Sys_NewInstance;
	com.printf = Sys_Msg;
	com.dprintf = Sys_MsgDev;
	com.error = Sys_Error;
	com.abort = Sys_Break;
	com.exit = Sys_Exit;
	com.print = Sys_Print;
	com.sleep = Sys_Sleep;
	com.clipboard = Sys_GetClipboardData;
	com.queevent = Sys_QueEvent;			// add event to queue
	com.getevent = Sys_GetEvent;			// get system events

	// memlib.c
	com.memcpy = _crt_mem_copy;			// first time using
	com.memset = _crt_mem_set;			// first time using
	com.realloc = _mem_realloc;
	com.move = _mem_move;
	com.malloc = _mem_alloc;
	com.free = _mem_free;
	com.is_allocated = _is_allocated;
	com.mallocpool = _mem_allocpool;
	com.freepool = _mem_freepool;
	com.clearpool = _mem_emptypool;
	com.memcheck = _mem_check;

	// network.c funcs
	com.NET_Init = NET_Init;
	com.NET_Shutdown = NET_Shutdown;
	com.NET_Sleep = NET_Sleep;
	com.NET_Config = NET_Config;
	com.NET_AdrToString = NET_AdrToString;
	com.NET_StringToAdr = NET_StringToAdr;
	com.NET_SendPacket = NET_SendPacket;
	com.NET_IsLocalAddress = NET_IsLocalAddress;
	com.NET_BaseAdrToString = NET_BaseAdrToString;
	com.NET_StringToAdr = NET_StringToAdr;
	com.NET_CompareAdr = NET_CompareAdr;
	com.NET_CompareBaseAdr = NET_CompareBaseAdr;
	com.NET_GetPacket = NET_GetPacket;
	com.NET_SendPacket = NET_SendPacket;

	com.Com_LoadGameInfo = FS_LoadGameInfo;		// gate game info from script file
	com.Com_AddGameHierarchy = FS_AddGameHierarchy;	// add base directory in search list
	com.Com_AddGameDirectory = FS_AddGameDirectory;	// add game directory in search list
	com.Com_AllowDirectPaths = FS_AllowDirectPaths;	// allow direct paths e.g. C:\windows
	com.Com_CheckParm = FS_CheckParm;		// get parm from cmdline
	com.Com_GetParm = FS_GetParmFromCmdLine;	// get filename without path & ext
	com.Com_FileBase = FS_FileBase;		// get filename without path & ext
	com.Com_FileExists = FS_FileExists;		// return true if file exist
	com.Com_FileSize = FS_FileSize;		// same as Com_FileExists but return filesize
	com.Com_FileTime = FS_FileTime;		// same as Com_FileExists but return filetime
	com.Com_FileExtension = FS_FileExtension;	// return extension of file
	com.Com_RemovePath = FS_FileWithoutPath;	// return file without path
	com.Com_DiskPath = FS_GetDiskPath;		// build fullpath for disk files
	com.Com_StripExtension = FS_StripExtension;	// remove extension if present
	com.Com_StripFilePath = FS_ExtractFilePath;	// get file path without filename.ext
	com.Com_DefaultExtension = FS_DefaultExtension;	// append extension if not present
	com.Com_ClearSearchPath = FS_ClearSearchPath;	// delete all search pathes

	com.LoadLibrary = Com_LoadLibrary;
	com.GetProcAddress = Com_GetProcAddress;
	com.NameForFunction = Com_NameForFunction;
	com.FunctionFromName = Com_FunctionFromName;
	com.FreeLibrary = Com_FreeLibrary;

	com.Com_LoadScript = PS_LoadScript;		// loading script into buffer
	com.Com_CloseScript = PS_FreeScript;		// release current script
	com.Com_ResetScript = PS_ResetScript;		// jump to start of scriptfile 
	com.Com_EndOfScript = PS_EndOfScript;		// returns true if end of script reached
	com.Com_SkipBracedSection = PS_SkipBracedSection;	// skip braced section with specified depth
	com.Com_SkipRestOfLine = PS_SkipRestOfLine;	// skip all tokene the rest of line
	com.Com_ReadToken = PS_ReadToken;		// generic reading
	com.Com_SaveToken = PS_SaveToken;		// save current token to get it again

	// script machine simple user interface
	com.Com_ReadString = PS_GetString;		// string
	com.Com_ReadFloat = PS_GetFloat;		// float value
	com.Com_ReadDword = PS_GetUnsigned;		// unsigned integer
	com.Com_ReadLong = PS_GetInteger;		// signed integer

	com.Com_Search = FS_Search;			// returned list of founded files

	// console variables
	com.Cvar_Get = Cvar_Get;
	com.Cvar_FullSet = Cvar_FullSet;
	com.Cvar_SetLatched = Cvar_SetLatched;
	com.Cvar_SetFloat = Cvar_SetFloat;
	com.Cvar_SetString = Cvar_Set;
	com.Cvar_GetInteger = Cvar_VariableInteger;
	com.Cvar_GetValue = Cvar_VariableValue;
	com.Cvar_GetString = Cvar_VariableString;
	com.Cvar_LookupVars = Cvar_LookupVars;
	com.Cvar_FindVar = Cvar_FindVar;
	com.Cvar_DirectSet = Cvar_DirectSet;
	com.Cvar_Register = Cvar_RegisterVariable;

	// console commands
	com.Cmd_Exec = Cbuf_ExecuteText;		// process cmd buffer
	com.Cmd_Argc = Cmd_Argc;
	com.Cmd_Args = Cmd_Args;
	com.Cmd_Argv = Cmd_Argv; 
	com.Cmd_LookupCmds = Cmd_LookupCmds;
	com.Cmd_AddCommand = Cmd_AddCommand;
	com.Cmd_AddGameCommand = Cmd_AddGameCommand;
	com.Cmd_DelCommand = Cmd_RemoveCommand;
	com.Cmd_TokenizeString = Cmd_TokenizeString;

	// real filesystem
	com.fopen = FS_Open;		// same as fopen
	com.fclose = FS_Close;		// same as fclose
	com.fwrite = FS_Write;		// same as fwrite
	com.fread = FS_Read;		// same as fread, can see trough pakfile
	com.fprint = FS_Print;		// printed message into file		
	com.fprintf = FS_Printf;		// same as fprintf
	com.fgetc = FS_Getc;		// same as fgetc
	com.fgets = FS_Gets;		// like a fgets, but can return EOF
	com.fseek = FS_Seek;		// fseek, can seek in packfiles too
	com.ftell = FS_Tell;		// like a ftell
	com.feof = FS_Eof;			// like a feof
	com.fremove = FS_Delete;		// like remove
	com.frename = FS_Rename;		// like rename
	com.flength = FS_FileLength;		// return length for current file

	// filesystem simply user interface
	com.Com_LoadFile = FS_LoadFile;		// load file into heap
	com.Com_FreeFile = FS_FreeFile;		// safe free file
	com.Com_WriteFile = FS_WriteFile;		// write file into disk
	com.Com_LoadLibrary = Sys_LoadLibrary;		// load library 
	com.Com_FreeLibrary = Sys_FreeLibrary;		// free library
	com.Com_GetProcAddress = Sys_GetProcAddress;	// gpa
	com.Com_ShellExecute = Sys_ShellExecute;	// shell execute
	com.Com_DoubleTime = Sys_DoubleTime;		// hi-res timer

	// stdlib.c funcs
	com.strnupr = com_strnupr;
	com.strnlwr = com_strnlwr;
	com.strupr = com_strupr;
	com.strlwr = com_strlwr;
	com.strlen = com_strlen;
	com.cstrlen = com_cstrlen;
	com.toupper = com_toupper;
	com.tolower = com_tolower;
	com.strncat = com_strncat;
	com.strcat = com_strcat;
	com.strncpy = com_strncpy;
	com.strcpy = com_strcpy;
	com.stralloc = com_stralloc;
	com.is_digit = com_isdigit;
	com.atoi = com_atoi;
	com.atof = com_atof;
	com.atov = com_atov;
	com.strchr = com_strchr;
	com.strrchr = com_strrchr;
	com.strnicmp = com_strnicmp;
	com.stricmp = com_stricmp;
	com.strncmp = com_strncmp;
	com.strcmp = com_strcmp;
	com.stristr = com_stristr;
	com.strstr = com_strstr;
	com.vsprintf = com_vsprintf;
	com.sprintf = com_sprintf;
	com.stricmpext = com_stricmpext;
	com.va = va;
	com.vsnprintf = com_vsnprintf;
	com.snprintf = com_snprintf;
	com.pretifymem = com_pretifymem;
	com.timestamp = com_timestamp;

	com.SysInfo = &SI;
}

/*
==================
Parse program name to launch and determine work style

NOTE: at this day we have ten instances

0. "offline" - invalid instance
1. "credits" - show engine credits
2. "dedicated" - dedicated server
3. "normal" - normal or dedicated game launch
==================
*/
void Sys_LookupInstance( void )
{
	char	szTemp[4096];
	qboolean	dedicated = false;

	// NOTE: we want set "real" work directory
	// defined in environment variables, but in some reasons
	// we need make some additional checks before set current dir
	GetCurrentDirectory( MAX_SYSPATH, sys_rootdir );

	Sys.app_name = HOST_OFFLINE;
	// we can specified custom name, from Sys_NewInstance
	if( GetModuleFileName( NULL, szTemp, MAX_SYSPATH ) && Sys.app_state != SYS_RESTART )
		FS_FileBase( szTemp, Sys.ModuleName );

	// determine host type
	if( Sys.ModuleName[0] == '#' || Sys.ModuleName[0] == '©' )
	{
		if( Sys.ModuleName[0] == '#' ) dedicated = true;
		if( Sys.ModuleName[0] == '©' ) com.strcpy( Sys.progname, "credits" );

		// cutoff hidden symbols
		com.strncpy( szTemp, Sys.ModuleName + 1, MAX_SYSPATH );
		com.strncpy( Sys.ModuleName, szTemp, MAX_SYSPATH );			
	}

	if( Sys.progname[0] == '$' )
	{
		// custom path came from executable, otherwise can't be modified 
		com.strncpy( Sys.ModuleName, Sys.progname + 1, MAX_SYSPATH );
		com.strncpy( Sys.progname, "normal", MAX_SYSPATH ); // set as "normal"		
	}

	// lookup all instances
	if( !com.strcmp( Sys.progname, "credits" ))
	{
		Sys.app_name = HOST_CREDITS;		// easter egg
		Sys.linked_dll = NULL;		// no need to loading library
		Sys.log_active = Sys.developer = 0;	// clear all dbg states
		com.strcpy( Sys.caption, "About" );
		Sys.con_showcredits = true;
	}
	else if( !com.strcmp( Sys.progname, "normal" ))
	{
		if( dedicated )
		{
			Sys.app_name = HOST_DEDICATED;
			Sys.con_readonly = false;

			// check for duplicate dedicated server
			Sys.hMutex = CreateMutex( NULL, 0, "Xash Dedicated Server" );
			if( !Sys.hMutex )
			{
				MSGBOX( "Dedicated server already running" );
				Sys_Exit();
				return;
			}

			CloseHandle( Sys.hMutex );
			Sys.hMutex = CreateSemaphore( NULL, 0, 1, "Xash Dedicated Server" );
			if( !Sys.developer ) Sys.developer = 3;	// otherwise we see empty console
			com.sprintf( Sys.log_path, "engine.log", com.timestamp( TIME_FILENAME )); // logs folder
		}
		else
		{
			Sys.app_name = HOST_NORMAL;
			Sys.con_readonly = true;
			// don't show console as default
			if( Sys.developer < D_WARN )
				Sys.con_showalways = false;

			com.sprintf( Sys.log_path, "engine.log", com.timestamp( TIME_FILENAME )); // logs folder
		}

		Sys.linked_dll = &engine_dll;	// pointer to engine.dll info
		com.strcpy( Sys.caption, va( "Xash3D ver.%g", XASH_VERSION ));
	}

	// share instance over all system
	SI.instance = Sys.app_name;
}

/*
==================
Find needed library, setup and run it
==================
*/
void Sys_CreateInstance( void )
{
	// export
	launch_t		CreateHost;
	launch_exp_t	*Host;			// callback to mainframe 

	srand( time( NULL ));			// init random generator
	Sys_LoadLibrary( NULL, Sys.linked_dll );	// loading library if need

	// pre initializations
	switch( Sys.app_name )
	{
	case HOST_NORMAL:
	case HOST_DEDICATED:
		CreateHost = (void *)Sys.linked_dll->main;
		Host = CreateHost( &com, NULL ); // second interface not allowed
		Sys.Init = Host->Init;
		Sys.Main = Host->Main;
		Sys.Free = Host->Free;
		Sys.CPrint = Host->CPrint;
		Sys.CmdFwd = Host->CmdForward;
		Sys.CmdAuto = Host->CmdComplete;
		break;
	case HOST_CREDITS:
		Sys_Break( show_credits, com.timestamp( TIME_YEAR_ONLY ));
		break;
	case HOST_OFFLINE:
		Sys_Break( "Host offline\n" );		
		break;
	}

	// init our host now!
	Sys.Init( fs_argc, fs_argv );

	// post initializations
	switch( Sys.app_name )
	{
	case HOST_NORMAL:
		Con_ShowConsole( false );				// hide console
		Cbuf_AddText( va( "exec %s.rc\n", Sys.ModuleName ));	// execute startup config and cmdline
	case HOST_DEDICATED:
		Cbuf_Execute();
		// if stuffcmds wasn't run, then init.rc is probably missing, use default
		if( !Sys.stuffcmdsrun ) Cbuf_ExecuteText( EXEC_NOW, "stuffcmds\n" );
		break;
	}

	Cmd_RemoveCommand( "setr" );	// remove potentially backdoor for change render settings
	Cmd_RemoveCommand( "setgl" );
	Sys.app_state = SYS_FRAME;	// system is now active
}

/*
==================
Sys_ParseCommandLine

==================
*/
void Sys_ParseCommandLine( LPSTR lpCmdLine )
{
	fs_argc = 1;
	fs_argv[0] = "exe";

	while( *lpCmdLine && ( fs_argc < MAX_NUM_ARGVS ))
	{
		while( *lpCmdLine && *lpCmdLine <= ' ' )
			lpCmdLine++;
		if( !*lpCmdLine ) break;

		if( *lpCmdLine == '\"' )
		{
			// quoted string
			lpCmdLine++;
			fs_argv[fs_argc] = lpCmdLine;
			fs_argc++;
			while( *lpCmdLine && ( *lpCmdLine != '\"' ))
				lpCmdLine++;
		}
		else
		{
			// unquoted word
			fs_argv[fs_argc] = lpCmdLine;
			fs_argc++;
			while( *lpCmdLine && *lpCmdLine > ' ')
				lpCmdLine++;
		}

		if( *lpCmdLine )
		{
			*lpCmdLine = 0;
			lpCmdLine++;
		}
	}

}

/*
==================
Sys_MergeCommandLine

==================
*/
void Sys_MergeCommandLine( LPSTR lpCmdLine )
{
	const char	*blank = "censored";
	int		i;
	
	for( i = 0; i < fs_argc; i++ )
	{
		// we wan't return to first game
		if( !com.stricmp( "-game", fs_argv[i] )) fs_argv[i] = (char *)blank;
		// probably it's timewaster, because engine rejected second change
		if( !com.stricmp( "+game", fs_argv[i] )) fs_argv[i] = (char *)blank;
		// you sure what is map exists in new game?
		if( !com.stricmp( "+map", fs_argv[i] )) fs_argv[i] = (char *)blank;
		// just stupid action
		if( !com.stricmp( "+load", fs_argv[i] )) fs_argv[i] = (char *)blank;
		// changelevel beetwen games? wow it's great idea!
		if( !com.stricmp( "+changelevel", fs_argv[i] )) fs_argv[i] = (char *)blank;

		// second call
		if( Sys.app_name == HOST_DEDICATED && !com.strnicmp( "+menu_", fs_argv[i], 6 ))
			fs_argv[i] = (char *)blank;
	}
}

/*
================
Sys_Print

print into window console
================
*/
void Sys_Print( const char *pMsg )
{
	const char	*msg;
	char		buffer[32768];
	char		logbuf[32768];
	char		*b = buffer;
	char		*c = logbuf;	
	int		i = 0;

	if( Sys.con_silentmode ) return;
	if( Sys.CPrint && Sys.app_name == HOST_NORMAL )
		Sys.CPrint( pMsg );

	// if the message is REALLY long, use just the last portion of it
	if( com.strlen( pMsg ) > sizeof( buffer ) - 1 )
		msg = pMsg + com.strlen( pMsg ) - sizeof( buffer ) + 1;
	else msg = pMsg;

	// copy into an intermediate buffer
	while( msg[i] && (( b - buffer ) < sizeof( buffer ) - 1 ))
	{
		if( msg[i] == '\n' && msg[i+1] == '\r' )
		{
			b[0] = '\r';
			b[1] = c[0] = '\n';
			b += 2, c++;
			i++;
		}
		else if( msg[i] == '\r' )
		{
			b[0] = c[0] = '\r';
			b[1] = '\n';
			b += 2, c++;
		}
		else if( msg[i] == '\n' )
		{
			b[0] = '\r';
			b[1] = c[0] = '\n';
			b += 2, c++;
		}
		else if( msg[i] == '\35' || msg[i] == '\36' || msg[i] == '\37' )
		{
			i++; // skip console pseudo graph
		}
		else if( IsColorString( &msg[i] ))
		{
			i++; // skip color prefix
		}
		else
		{
			*b = *c = msg[i];
			b++, c++;
		}
		i++;
	}

	*b = *c = 0; // cutoff garbage

	// because we needs to kill any psedo graph symbols
	// and color strings for other instances
	if( Sys.CPrint && Sys.app_name != HOST_NORMAL )
		Sys.CPrint( logbuf );

	Sys_PrintLog( logbuf );

	Sys.Con_Print( buffer );
}

/*
================
Sys_Msg

formatted message
================
*/
void Sys_Msg( const char *pMsg, ... )
{
	va_list	argptr;
	char	text[8192];
	
	va_start( argptr, pMsg );
	com.vsnprintf( text, sizeof( text ), pMsg, argptr );
	va_end( argptr );

	Sys_Print( text );
}

void Sys_MsgDev( int level, const char *pMsg, ... )
{
	va_list	argptr;
	char	text[8192];

	if( Sys.developer < level ) return;

	va_start( argptr, pMsg );
	com.vsnprintf( text, sizeof( text ), pMsg, argptr );
	va_end( argptr );

	switch( level )
	{
	case D_WARN:
		Sys_Print( va( "^3Warning:^7 %s", text ));
		break;
	case D_ERROR:
		Sys_Print( va( "^1Error:^7 %s", text ));
		break;
	case D_INFO:
	case D_NOTE:
	case D_AICONSOLE:
		Sys_Print( text );
		break;
	}
}

/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime( void )
{
	static LARGE_INTEGER	g_PerformanceFrequency;
	static LARGE_INTEGER	g_ClockStart;
	LARGE_INTEGER		CurrentTime;

	if( !g_PerformanceFrequency.QuadPart )
	{
		QueryPerformanceFrequency( &g_PerformanceFrequency );
		QueryPerformanceCounter( &g_ClockStart );
	}
	QueryPerformanceCounter( &CurrentTime );

	return (double)( CurrentTime.QuadPart - g_ClockStart.QuadPart ) / (double)( g_PerformanceFrequency.QuadPart );
}

/*
================
Sys_GetClipboardData

create buffer, that contain clipboard
================
*/
char *Sys_GetClipboardData( void )
{
	char	*data = NULL;
	char	*cliptext;

	if( OpenClipboard( NULL ) != 0 )
	{
		HANDLE hClipboardData;

		if(( hClipboardData = GetClipboardData( CF_TEXT )) != 0 )
		{
			if(( cliptext = GlobalLock( hClipboardData )) != 0 ) 
			{
				data = Malloc( GlobalSize( hClipboardData ) + 1 );
				com.strcpy( data, cliptext );
				GlobalUnlock( hClipboardData );
			}
		}
		CloseClipboard();
	}
	return data;
}

/*
================
Sys_GetCurrentUser

returns username for current profile
================
*/
char *Sys_GetCurrentUser( void )
{
	static string	s_userName;
	dword		size = sizeof( s_userName );

	if( !GetUserName( s_userName, &size ) || !s_userName[0] )
		com.strcpy( s_userName, "player" );

	return s_userName;
}

/*
================
Sys_Sleep

freeze application for some time
================
*/
void Sys_Sleep( int msec )
{
	msec = bound( 1, msec, 1000 );
	Sleep( msec );
}

/*
================
Sys_WaitForQuit

wait for 'Esc' key will be hit
================
*/
void Sys_WaitForQuit( void )
{
	MSG	msg;

	Con_RegisterHotkeys();		
	Mem_Set( &msg, 0, sizeof( msg ));

	// wait for the user to quit
	while( msg.message != WM_QUIT )
	{
		if( PeekMessage( &msg, 0, 0, 0, PM_REMOVE ))
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		} 
		else Sys_Sleep( 20 );
	}
}

/*
================
Sys_Error

NOTE: we must prepare engine to shutdown
before call this
================
*/
void Sys_Error( const char *error, ... )
{
	va_list	argptr;
	char	text[MAX_SYSPATH];
         
	if( Sys.app_state == SYS_ERROR )
		return; // don't multiple executes

	// make sure what console received last message
	// stupid windows bug :( 
	if( Sys.app_state == SYS_RESTART )
		Sys_Sleep( 200 );

	Sys.error = true;
	Sys.app_state = SYS_ERROR;	
	va_start( argptr, error );
	com.vsprintf( text, error, argptr );
	va_end( argptr );

	if( Sys.app_name == HOST_NORMAL )
		Sys.Free(); // kill video

	if( Sys.developer > 0 )
	{
		Con_ShowConsole( true );
		Con_DisableInput();	// disable input line for dedicated server
		Sys_Print( text );	// print error message
		Sys_WaitForQuit();
	}
	else
	{
		Con_ShowConsole( false );
		MSGBOX( text );
	}
	Sys_Exit();
}

void Sys_Break( const char *error, ... )
{
	va_list		argptr;
	char		text[MAX_SYSPATH];

	if( Sys.app_state == SYS_ERROR )
		return; // don't multiple executes
         
	va_start( argptr, error );
	com.vsprintf( text, error, argptr );
	va_end( argptr );

	Sys.error = true;	
	Sys.app_state = SYS_ERROR;

	if( Sys.app_name == HOST_NORMAL )
		Sys.Free(); // kill video

	if( Sys.con_readonly && ( Sys.developer > 0 || Sys.app_name != HOST_NORMAL ))
	{
		Con_ShowConsole( true );
		Sys_Print( text );
		Sys_WaitForQuit();
	}
	else
	{
		Con_ShowConsole( false );
		MSGBOX( text );
	}
	Sys_Exit();
}

void Sys_Abort( void )
{
	// aborting by user, run normal shutdown procedure
	Sys.app_state = SYS_ABORT;
	Sys_Exit();
}

long _stdcall Sys_Crash( PEXCEPTION_POINTERS pInfo )
{
	// save config
	if( Sys.app_state != SYS_CRASH )
	{
		// check to avoid recursive call
		Sys.error = true;
		Sys.app_state = SYS_CRASH;

		if( Sys.app_name == HOST_NORMAL || Sys.app_name == HOST_DEDICATED )
			Cmd_ExecuteString( "@crashed\n" ); // tell server about crash
		Msg( "Sys_Crash: call %p at address %p\n", pInfo->ExceptionRecord->ExceptionAddress, pInfo->ExceptionRecord->ExceptionCode );

		if( Sys.developer <= 0 )
		{
			// no reason to call debugger in release build - just exit
			Sys_Exit();
			return EXCEPTION_CONTINUE_EXECUTION;
		}

		// all other states keep unchanged to let debugger find bug
		Con_DestroyConsole();
          }

	if( Sys.oldFilter )
		return Sys.oldFilter( pInfo );
	return EXCEPTION_CONTINUE_EXECUTION;
}

void Sys_Init( void )
{
	MEMORYSTATUS	lpBuffer;
	char		dev_level[4];

	lpBuffer.dwLength = sizeof( MEMORYSTATUS );
	GlobalMemoryStatus( &lpBuffer );
	Sys.logfile = NULL;

	// get current hInstance
	Sys.hInstance = (HINSTANCE)GetModuleHandle( NULL );
	Sys.developer = 0;

	Sys_GetStdAPI();
	Sys.Init = NullInit;
	Sys.Main = NullFunc;
	Sys.Free = NullFunc;
	Sys.CPrint = NullPrint;
	Sys.Con_Print = NullPrint;

	Sys.oldFilter = SetUnhandledExceptionFilter( Sys_Crash );

	// some commands may turn engine into infinity loop,
	// e.g. xash.exe +game xash -game xash
	// so we clearing all cmd_args, but leave dbg states as well
	if( Sys.app_state != SYS_RESTART )
		Sys_ParseCommandLine( GetCommandLine());
	else Sys_MergeCommandLine( GetCommandLine());

	// parse and copy args into local array
	if( FS_CheckParm( "-log" )) Sys.log_active = true;
	if( FS_CheckParm( "-console" )) Sys.developer = 1;
	if( FS_CheckParm( "-dev" ))
	{
		if( FS_GetParmFromCmdLine( "-dev", dev_level, sizeof( dev_level )))
		{
			if( com.is_digit( dev_level ))
				Sys.developer = abs( com.atoi( dev_level ));
			else Sys.developer++; // -dev == 1, -dev -console == 2
		}
		else Sys.developer++; // -dev == 1, -dev -console == 2
	}

	if( Sys.log_active && !Sys.developer )
		Sys.log_active = false;	// nothing to logging :)
          
	SetErrorMode( SEM_FAILCRITICALERRORS );	// no abort/retry/fail errors

	// set default state 
	Sys.con_showalways = Sys.con_readonly = true;
	Sys.con_showcredits = Sys.con_silentmode = Sys.stuffcmdsrun = false;

	Sys_LookupInstance();		// init launcher
	Con_CreateConsole();

	// second pass (known state)
	if( Sys.app_state == SYS_RESTART )
		Sys_MergeCommandLine( GetCommandLine());

	// first text message into console or log 
	MsgDev( D_NOTE, "Sys_LoadLibrary: Loading launch.dll - ok\n" );

	if( com.strlen( Sys.fmessage ) && !Sys.con_showcredits )
	{
		Sys_Print( Sys.fmessage );
		Sys.fmessage[0] = '\0';
	}

	Memory_Init();
	Sys_InitCPU();
	Cmd_Init();
	Cvar_Init();
	FS_Init();

	com.snprintf( dev_level, sizeof( dev_level ), "%i", Sys.developer );
	Cvar_Get( "developer", dev_level, CVAR_INIT, "current developer level" );

	Sys_CreateInstance();
}

void Sys_Shutdown( void )
{
	// prepare host to close
	Sys.Free();
	Sys_FreeLibrary( Sys.linked_dll );
	Sys.CPrint = NullPrint;

	FS_Shutdown();
	Memory_Shutdown();
	Con_DestroyConsole();

	// restore filter	
	if( Sys.oldFilter )
	{
		SetUnhandledExceptionFilter( Sys.oldFilter );
	}
}

/*
================
Sys_Exit

NOTE: we must prepare engine to shutdown
before call this
================
*/
void Sys_Exit( void )
{
	if( Sys.shutdown_issued ) return;
	Sys.shutdown_issued = true;
	Sys.app_state = SYS_SHUTDOWN;

	Sys_Shutdown();
	exit( Sys.error );
}

//=======================================================================
//			DLL'S MANAGER SYSTEM
//=======================================================================
qboolean Sys_LoadLibrary( const char *dll_name, dll_info_t *dll )
{
	const dllfunc_t	*func;
	qboolean		native_lib = false;
	string		errorstring;

	// check errors
	if( !dll ) return false;	// invalid desc
	if( dll->link ) return true;	// already loaded

	// check and replace names
	if( dll_name && *dll_name ) dll->name = dll_name;
	if( !dll->name || !*dll->name ) return false; // nothing to load

	MsgDev( D_NOTE, "Sys_LoadLibrary: Loading %s", dll->name );

	if( dll->fcts ) 
	{
		// lookup export table
		for( func = dll->fcts; func && func->name != NULL; func++ )
			*func->func = NULL;
	}
	else if( dll->entry ) native_lib = true;

	if( !dll->link ) dll->link = LoadLibrary ( dll->name ); // environment pathes

	// no DLL found
	if( !dll->link ) 
	{
		com.sprintf( errorstring, "Sys_LoadLibrary: couldn't load %s\n", dll->name );
		goto error;
	}

	if( native_lib )
	{
		if(( dll->main = Sys_GetProcAddress( dll, dll->entry )) == 0 )
		{
			com.sprintf( errorstring, "Sys_LoadLibrary: %s has no valid entry point\n", dll->name );
			goto error;
		}
	}
	else
	{
		// Get the function adresses
		for( func = dll->fcts; func && func->name != NULL; func++ )
		{
			if( !( *func->func = Sys_GetProcAddress( dll, func->name )))
			{
				com.sprintf( errorstring, "Sys_LoadLibrary: %s missing or invalid function (%s)\n", dll->name, func->name );
				goto error;
			}
		}
	}

	if( native_lib )
	{
		generic_api_t *check = NULL;

		// NOTE: native dlls must support null import!
		// e.g. see ..\engine\engine.c for details
		check = (void *)dll->main( &com, NULL ); // first iface always stdlib_api_t

		if( !check ) 
		{
			com.sprintf( errorstring, "Sys_LoadLibrary: \"%s\" have no export\n", dll->name );
			goto error;
		}
		if( check->api_size != dll->api_size )
		{
			com.sprintf( errorstring, "Sys_LoadLibrary: \"%s\" mismatch interface size (%i should be %i)\n", dll->name, check->api_size, dll->api_size );
			goto error;
		}	
		if( check->com_size != dll->com_size )
		{
			com.sprintf( errorstring, "Sys_LoadLibrary: \"%s\" mismatch stdlib api size (%i should be %i)\n", dll->name, check->com_size, dll->com_size);
			goto error;
		}
	}
          MsgDev( D_NOTE, " - ok\n" );

	return true;
error:
	MsgDev( D_NOTE, " - failed\n" );
	Sys_FreeLibrary( dll ); // trying to free 
	if( dll->crash ) Sys_Error( errorstring );
	else MsgDev( D_ERROR, errorstring );			

	return false;
}

void* Sys_GetProcAddress( dll_info_t *dll, const char* name )
{
	if( !dll || !dll->link ) // invalid desc
		return NULL;

	return (void *)GetProcAddress( dll->link, name );
}

qboolean Sys_FreeLibrary( dll_info_t *dll )
{
	// invalid desc or alredy freed
	if( !dll || !dll->link )
		return false;

	if( Sys.app_state == SYS_CRASH )
	{
		// we need to hold down all modules, while MSVC can find error
		MsgDev( D_NOTE, "Sys_FreeLibrary: hold %s for debugging\n", dll->name );
		return false;
	}
	else MsgDev( D_NOTE, "Sys_FreeLibrary: Unloading %s\n", dll->name );
	FreeLibrary( dll->link );
	dll->link = NULL;

	return true;
}

/*
=================
Sys_ShellExecute
=================
*/
void Sys_ShellExecute( const char *path, const char *parms, qboolean exit )
{
	ShellExecute( NULL, "open", path, parms, NULL, SW_SHOW );

	if( exit ) Sys_Exit();
}

/*
================
Sys_QueEvent

A time of 0 will get the current time
Ptr should either be null, or point to a block of data that can
be freed by the game later.
================
*/
void Sys_QueEvent( ev_type_t type, int value, int value2, int length, void *ptr )
{
	sys_event_t	*ev;

	ev = &event_que[event_head & MASK_QUED_EVENTS];
	if( event_head - event_tail >= MAX_QUED_EVENTS )
	{
		MsgDev( D_ERROR, "Sys_QueEvent: overflow\n");
		// make sure what memory is allocated by engine
		if( Mem_IsAllocated( ev->data )) Mem_Free( ev->data );
		event_tail++;
	}
	event_head++;

	ev->type = type;
	ev->value[0] = value;
	ev->value[1] = value2;
	ev->length = length;
	ev->data = ptr;
}

/*
================
Sys_GetEvent

================
*/
sys_event_t Sys_GetEvent( void )
{
	MSG		msg;
	sys_event_t	ev;
	char		*s;
	
	// return if we have data
	if( event_head > event_tail )
	{
		event_tail++;
		return event_que[(event_tail - 1) & MASK_QUED_EVENTS];
	}

	// pump the message loop
	while( PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ))
	{
		if( !GetMessage( &msg, NULL, 0, 0 ))
		{
			Sys.error = true;
			Sys_Exit();
		}
		TranslateMessage(&msg );
      		DispatchMessage( &msg );
	}

	// check for console commands
	s = Sys_Input();
	if( s )
	{
		char	*b;
		int	len;

		len = com.strlen( s ) + 1;
		b = Malloc( len );
		com.strncpy( b, s, len - 1 );
		Sys_QueEvent( SE_CONSOLE, 0, 0, len, b );
	}

	// return if we have data
	if( event_head > event_tail )
	{
		event_tail++;
		return event_que[(event_tail - 1) & MASK_QUED_EVENTS];
	}

	// create an empty event to return
	Mem_Set( &ev, 0, sizeof( ev ));

	return ev;
}

qboolean Sys_GetModuleName( char *buffer, size_t length )
{
	if( Sys.ModuleName[0] == '\0' )
		return false;

	com.strncpy( buffer, Sys.ModuleName, length + 1 );
	return true;
}

/*
================
Sys_NewInstance

restarted engine with new instance
e.g. for change game or fallback to dedicated mode
================
*/
void Sys_NewInstance( const char *name, const char *fmsg )
{
	// save parms
	com.strncpy( Sys.ModuleName, name, sizeof( Sys.ModuleName ));
	com.strncpy( Sys.fmessage, fmsg, sizeof( Sys.fmessage ));
	Sys.app_state = SYS_RESTART;	// set right state
	Sys_Shutdown();		// shutdown current instance

	// NOTE: we never return to old instance,
	// because Sys_Exit call exit(0); and terminate program
	Sys_Init();
	Sys.Main();
	Sys_Exit();
}

// main DLL entry point
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
	return TRUE;
}

/*
=================
Main Entry Point
=================
*/
EXPORT int CreateAPI( const char *hostname, qboolean console )
{
	com_strncpy( Sys.progname, hostname, sizeof( Sys.progname ));

	Sys_Init();
	Sys.Main();
	Sys_Exit();

	return 0;
}