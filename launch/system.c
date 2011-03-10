
//=======================================================================
//			Copyright XashXT Group 2007 ©
//			utils.c - shared launcher utils
//=======================================================================

#include "launch.h"


system_t		Sys;
stdlib_api_t	com;

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
	com.Com_CheckParm = Sys_CheckParm;		// get parm from cmdline
	com.Com_GetParm = Sys_GetParmFromCmdLine;	// get argument for specified parm
	com.input = Con_Input;

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
	com.memlist = _mem_printlist;
	com.memstats = _mem_printstats;

	com.Com_LoadLibrary = Sys_LoadLibrary;		// load library 
	com.Com_FreeLibrary = Sys_FreeLibrary;		// free library
	com.Com_GetProcAddress = Sys_GetProcAddress;	// gpa

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

	Sys.app_name = HOST_OFFLINE;
	// we can specified custom name, from Sys_NewInstance
	if( GetModuleFileName( NULL, szTemp, MAX_SYSPATH ) && Sys.app_state != SYS_RESTART )
		Com_FileBase( szTemp, SI.ModuleName );

	// determine host type
	if( SI.ModuleName[0] == '#' || SI.ModuleName[0] == '©' )
	{
		if( SI.ModuleName[0] == '#' ) dedicated = true;
		if( SI.ModuleName[0] == '©' ) com.strcpy( Sys.progname, "credits" );

		// cutoff hidden symbols
		com.strncpy( szTemp, SI.ModuleName + 1, MAX_SYSPATH );
		com.strncpy( SI.ModuleName, szTemp, MAX_SYSPATH );			
	}

	if( Sys.progname[0] == '$' )
	{
		// custom path came from executable, otherwise can't be modified 
		com.strncpy( SI.ModuleName, Sys.progname + 1, MAX_SYSPATH );
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
			com.sprintf( Sys.log_path, "dedicated.log", com.timestamp( TIME_FILENAME )); // logs folder
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
		com.strcpy( Sys.caption, "Xash3D" );
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
		Sys.CmdAuto = Host->CmdComplete;
		Sys.Crashed = Host->Crashed;
		break;
	case HOST_CREDITS:
		Sys_Break( show_credits, com.timestamp( TIME_YEAR_ONLY ));
		break;
	case HOST_OFFLINE:
		Sys_Break( "Host offline\n" );		
		break;
	}

	// init our host now!
	Sys.Init( Sys.argc, Sys.argv );

	// post initializations
	if( Sys.app_name == HOST_NORMAL ) Con_ShowConsole( false ); // hide console
	Sys.app_state = SYS_FRAME;	// system is now active
}

/*
==================
Sys_ParseCommandLine

==================
*/
void Sys_ParseCommandLine( LPSTR lpCmdLine )
{
	Sys.argc = 1;
	Sys.argv[0] = "exe";

	while( *lpCmdLine && ( Sys.argc < MAX_NUM_ARGVS ))
	{
		while( *lpCmdLine && *lpCmdLine <= ' ' )
			lpCmdLine++;
		if( !*lpCmdLine ) break;

		if( *lpCmdLine == '\"' )
		{
			// quoted string
			lpCmdLine++;
			Sys.argv[Sys.argc] = lpCmdLine;
			Sys.argc++;
			while( *lpCmdLine && ( *lpCmdLine != '\"' ))
				lpCmdLine++;
		}
		else
		{
			// unquoted word
			Sys.argv[Sys.argc] = lpCmdLine;
			Sys.argc++;
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
	
	for( i = 0; i < Sys.argc; i++ )
	{
		// we wan't return to first game
		if( !com.stricmp( "-game", Sys.argv[i] )) Sys.argv[i] = (char *)blank;
		// probably it's timewaster, because engine rejected second change
		if( !com.stricmp( "+game", Sys.argv[i] )) Sys.argv[i] = (char *)blank;
		// you sure what is map exists in new game?
		if( !com.stricmp( "+map", Sys.argv[i] )) Sys.argv[i] = (char *)blank;
		// just stupid action
		if( !com.stricmp( "+load", Sys.argv[i] )) Sys.argv[i] = (char *)blank;
		// changelevel beetwen games? wow it's great idea!
		if( !com.stricmp( "+changelevel", Sys.argv[i] )) Sys.argv[i] = (char *)blank;

		// second call
		if( Sys.app_name == HOST_DEDICATED && !com.strnicmp( "+menu_", Sys.argv[i], 6 ))
			Sys.argv[i] = (char *)blank;
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
	Con_Print( buffer );
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
Sys_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int Sys_CheckParm( const char *parm )
{
	int	i;

	for( i = 1; i < Sys.argc; i++ )
	{
		// NEXTSTEP sometimes clears appkit vars.
		if( !Sys.argv[i] ) continue;
		if( !com.stricmp( parm, Sys.argv[i] )) return i;
	}
	return 0;
}

/*
================
Sys_GetParmFromCmdLine

Returns the argument for specified parm
================
*/
qboolean Sys_GetParmFromCmdLine( char *parm, char *out, size_t size )
{
	int	argc = Sys_CheckParm( parm );

	if( !argc ) return false;
	if( !out ) return false;	
	if( !Sys.argv[argc + 1] ) return false;

	com.strncpy( out, Sys.argv[argc+1], size );
	return true;
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

long _stdcall Sys_Crash( PEXCEPTION_POINTERS pInfo )
{
	// save config
	if( Sys.app_state != SYS_CRASH )
	{
		// check to avoid recursive call
		Sys.error = true;
		Sys.app_state = SYS_CRASH;

		if( Sys.app_name == HOST_NORMAL && Sys.Crashed ) Sys.Crashed(); // tell client about crash
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

	Sys.oldFilter = SetUnhandledExceptionFilter( Sys_Crash );

	// some commands may turn engine into infinity loop,
	// e.g. xash.exe +game xash -game xash
	// so we clearing all cmd_args, but leave dbg states as well
	if( Sys.app_state != SYS_RESTART )
		Sys_ParseCommandLine( GetCommandLine());
	else Sys_MergeCommandLine( GetCommandLine());

	// parse and copy args into local array
	if( Sys_CheckParm( "-log" )) Sys.log_active = true;
	if( Sys_CheckParm( "-console" )) Sys.developer = 1;
	if( Sys_CheckParm( "-dev" ))
	{
		if( Sys_GetParmFromCmdLine( "-dev", dev_level, sizeof( dev_level )))
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
	Sys.con_showcredits = Sys.con_silentmode = false;

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

	SI.developer = Sys.developer;
	Sys_CreateInstance();
}

void Sys_Shutdown( void )
{
	// prepare host to close
	Sys.Free();
	Sys_FreeLibrary( Sys.linked_dll );
	Sys.CPrint = NullPrint;

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

	if( Sys.app_state != SYS_ERROR )
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
================
Sys_NewInstance

restarted engine with new instance
e.g. for change game or fallback to dedicated mode
================
*/
void Sys_NewInstance( const char *name, const char *fmsg )
{
	string	tmp;

	// save parms
	com.strncpy( tmp, name, sizeof( tmp ));
	com.strncpy( Sys.fmessage, fmsg, sizeof( Sys.fmessage ));
	Sys.app_state = SYS_RESTART;	// set right state
	Sys_Shutdown();		// shutdown current instance

	// restore parms here
	com.strncpy( SI.ModuleName, tmp, sizeof( SI.ModuleName ));

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