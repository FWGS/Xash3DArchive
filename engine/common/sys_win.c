//=======================================================================
//			Copyright XashXT Group 2011 ©
//		        sys_win.c - platform dependent code
//=======================================================================

#include "common.h"
#include "mathlib.h"

#define MAX_QUED_EVENTS	256
#define MASK_QUED_EVENTS	(MAX_QUED_EVENTS - 1)

qboolean		error_on_exit = false;	// arg for exit();
sys_event_t	event_que[MAX_QUED_EVENTS];
int		event_head, event_tail;

typedef struct register_s
{
	dword	eax;
	dword	ebx;
	dword	ecx;
	dword	edx;
	qboolean	retval;
} register_t;

static register_t Sys_CpuId( uint function )
{
	register_t	local;
          
          local.retval = true;

	_asm pushad;

	__try
	{
		_asm
		{
			xor edx, edx	// Clue the compiler that EDX is about to be used.
			mov eax, function   // set up CPUID to return processor version and features
					// 0 = vendor string, 1 = version info, 2 = cache info
			cpuid		// code bytes = 0fh,  0a2h
			mov local.eax, eax	// features returned in eax
			mov local.ebx, ebx	// features returned in ebx
			mov local.ecx, ecx	// features returned in ecx
			mov local.edx, edx	// features returned in edx
		}
	} 

	__except( EXCEPTION_EXECUTE_HANDLER ) 
	{ 
		local.retval = false; 
	}

	_asm popad

	return local;
}

qboolean Sys_CheckMMX( void )
{
	register_t mmx = Sys_CpuId( 1 );
	if( !mmx.retval ) return false;
	return ( mmx.edx & 0x800000 ) != 0;
}

qboolean Sys_CheckSSE( void )
{
	register_t sse = Sys_CpuId( 1 );
	if( !sse.retval ) return false;
	return ( sse.edx & 0x2000000L ) != 0;
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
		HANDLE	hClipboardData;

		if(( hClipboardData = GetClipboardData( CF_TEXT )) != 0 )
		{
			if(( cliptext = GlobalLock( hClipboardData )) != 0 ) 
			{
				data = Z_Malloc( GlobalSize( hClipboardData ) + 1 );
				Q_strcpy( data, cliptext );
				GlobalUnlock( hClipboardData );
			}
		}
		CloseClipboard();
	}
	return data;
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
Sys_GetCurrentUser

returns username for current profile
================
*/
char *Sys_GetCurrentUser( void )
{
	static string	s_userName;
	dword		size = sizeof( s_userName );

	if( !GetUserName( s_userName, &size ) || !s_userName[0] )
		Q_strcpy( s_userName, "player" );

	return s_userName;
}

/*
=================
Sys_ShellExecute
=================
*/
void Sys_ShellExecute( const char *path, const char *parms, qboolean exit )
{
	ShellExecute( NULL, "open", path, parms, NULL, SW_SHOW );

	if( exit ) Sys_Quit();
}

/*
==================
Sys_ParseCommandLine

==================
*/
void Sys_ParseCommandLine( LPSTR lpCmdLine )
{
	const char	*blank = "censored";
	int		i;

	host.argc = 1;
	host.argv[0] = "exe";

	while( *lpCmdLine && ( host.argc < MAX_NUM_ARGVS ))
	{
		while( *lpCmdLine && *lpCmdLine <= ' ' )
			lpCmdLine++;
		if( !*lpCmdLine ) break;

		if( *lpCmdLine == '\"' )
		{
			// quoted string
			lpCmdLine++;
			host.argv[host.argc] = lpCmdLine;
			host.argc++;
			while( *lpCmdLine && ( *lpCmdLine != '\"' ))
				lpCmdLine++;
		}
		else
		{
			// unquoted word
			host.argv[host.argc] = lpCmdLine;
			host.argc++;
			while( *lpCmdLine && *lpCmdLine > ' ')
				lpCmdLine++;
		}

		if( *lpCmdLine )
		{
			*lpCmdLine = 0;
			lpCmdLine++;
		}
	}

	if( !host.change_game ) return;

	for( i = 0; i < host.argc; i++ )
	{
		// we wan't return to first game
		if( !Q_stricmp( "-game", host.argv[i] )) host.argv[i] = (char *)blank;
		// probably it's timewaster, because engine rejected second change
		if( !Q_stricmp( "+game", host.argv[i] )) host.argv[i] = (char *)blank;
		// you sure what is map exists in new game?
		if( !Q_stricmp( "+map", host.argv[i] )) host.argv[i] = (char *)blank;
		// just stupid action
		if( !Q_stricmp( "+load", host.argv[i] )) host.argv[i] = (char *)blank;
		// changelevel beetwen games? wow it's great idea!
		if( !Q_stricmp( "+changelevel", host.argv[i] )) host.argv[i] = (char *)blank;
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

	if( !host.change_game ) return;

	for( i = 0; i < host.argc; i++ )
	{
		// second call
		if( host.type == HOST_DEDICATED && !Q_strnicmp( "+menu_", host.argv[i], 6 ))
			host.argv[i] = (char *)blank;
	}
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

	for( i = 1; i < host.argc; i++ )
	{
		if( !host.argv[i] ) continue;
		if( !Q_stricmp( parm, host.argv[i] ))
			return i;
	}
	return 0;
}

/*
================
Sys_GetParmFromCmdLine

Returns the argument for specified parm
================
*/
qboolean _Sys_GetParmFromCmdLine( char *parm, char *out, size_t size )
{
	int	argc = Sys_CheckParm( parm );

	if( !argc ) return false;
	if( !out ) return false;	
	if( !host.argv[argc + 1] ) return false;
	Q_strncpy( out, host.argv[argc+1], size );

	return true;
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
		if( Mem_IsAllocatedExt( host.mempool, ev->data ))
			Mem_Free( ev->data );
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
			// FIXME: set reason to quit
			Sys_Quit();
		}
		TranslateMessage(&msg );
      		DispatchMessage( &msg );
	}

	// check for console commands
	s = Con_Input();
	if( s )
	{
		char	*b;
		int	len;

		len = Q_strlen( s );
		b = Z_Malloc( len + 1 );
		Q_strcpy( b, s );
		Sys_QueEvent( SE_CONSOLE, 0, 0, len, b );
	}

	// return if we have data
	if( event_head > event_tail )
	{
		event_tail++;
		return event_que[(event_tail - 1) & MASK_QUED_EVENTS];
	}

	// create an empty event to return
	Q_memset( &ev, 0, sizeof( ev ));

	return ev;
}

//=======================================================================
//			DLL'S MANAGER SYSTEM
//=======================================================================
qboolean Sys_LoadLibrary( dll_info_t *dll )
{
	const dllfunc_t	*func;
	string		errorstring;

	// check errors
	if( !dll ) return false;	// invalid desc
	if( dll->link ) return true;	// already loaded

	if( !dll->name || !*dll->name )
		return false; // nothing to load

	MsgDev( D_NOTE, "Sys_LoadLibrary: Loading %s", dll->name );

	if( dll->fcts ) 
	{
		// lookup export table
		for( func = dll->fcts; func && func->name != NULL; func++ )
			*func->func = NULL;
	}

	if( !dll->link ) dll->link = LoadLibrary ( dll->name ); // environment pathes

	// no DLL found
	if( !dll->link ) 
	{
		Q_snprintf( errorstring, sizeof( errorstring ), "Sys_LoadLibrary: couldn't load %s\n", dll->name );
		goto error;
	}

	// Get the function adresses
	for( func = dll->fcts; func && func->name != NULL; func++ )
	{
		if( !( *func->func = Sys_GetProcAddress( dll, func->name )))
		{
			Q_snprintf( errorstring, sizeof( errorstring ), "Sys_LoadLibrary: %s missing or invalid function (%s)\n", dll->name, func->name );
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

	if( host.state == HOST_CRASHED )
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
Sys_WaitForQuit

wait for 'Esc' key will be hit
================
*/
void Sys_WaitForQuit( void )
{
	MSG	msg;

	Con_RegisterHotkeys();		

	msg.message = 0;

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

long _stdcall Sys_Crash( PEXCEPTION_POINTERS pInfo )
{
	// save config
	if( host.state != HOST_CRASHED )
	{
		// check to avoid recursive call
		error_on_exit = true;
		host.state = HOST_CRASHED;

		if( host.type == HOST_NORMAL ) CL_Crashed(); // tell client about crash
		Msg( "Sys_Crash: call %p at address %p\n", pInfo->ExceptionRecord->ExceptionAddress, pInfo->ExceptionRecord->ExceptionCode );

		if( host.developer <= 0 )
		{
			// no reason to call debugger in release build - just exit
			Sys_Quit();
			return EXCEPTION_CONTINUE_EXECUTION;
		}

		// all other states keep unchanged to let debugger find bug
		Con_DestroyConsole();
          }

	if( host.oldFilter )
		return host.oldFilter( pInfo );
	return EXCEPTION_CONTINUE_EXECUTION;
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
         
	if( host.state == HOST_ERR_FATAL )
		return; // don't multiple executes

	// make sure what console received last message
	if( host.change_game ) Sys_Sleep( 200 );

	error_on_exit = true;
	host.state = HOST_ERR_FATAL;	
	va_start( argptr, error );
	Q_vsprintf( text, error, argptr );
	va_end( argptr );

	if( host.type = HOST_NORMAL )
		CL_Shutdown(); // kill video

	if( host.developer > 0 )
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
	Sys_Quit();
}

/*
================
Sys_Break

same as Error
================
*/
void Sys_Break( const char *error, ... )
{
	va_list		argptr;
	char		text[MAX_SYSPATH];

	if( host.state == HOST_ERR_FATAL )
		return; // don't multiple executes
         
	va_start( argptr, error );
	Q_vsprintf( text, error, argptr );
	va_end( argptr );

	error_on_exit = true;	
	host.state = HOST_ERR_FATAL;

	if( host.type == HOST_NORMAL )
		CL_Shutdown(); // kill video

	if( host.type != HOST_NORMAL || host.developer > 0 )
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
	Sys_Quit();
}

/*
================
Sys_Quit
================
*/
void Sys_Quit( void )
{
	Host_Shutdown();
	exit( error_on_exit );
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

	if( host.type == HOST_NORMAL )
		Con_Print( pMsg );

	// if the message is REALLY long, use just the last portion of it
	if( Q_strlen( pMsg ) > sizeof( buffer ) - 1 )
		msg = pMsg + Q_strlen( pMsg ) - sizeof( buffer ) + 1;
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

	Sys_PrintLog( logbuf );
	Con_WinPrint( buffer );
}

/*
================
Msg

formatted message
================
*/
void Msg( const char *pMsg, ... )
{
	va_list	argptr;
	char	text[8192];
	
	va_start( argptr, pMsg );
	Q_vsnprintf( text, sizeof( text ), pMsg, argptr );
	va_end( argptr );

	Sys_Print( text );
}

/*
================
MsgDev

formatted developer message
================
*/
void MsgDev( int level, const char *pMsg, ... )
{
	va_list	argptr;
	char	text[8192];

	if( host.developer < level ) return;

	va_start( argptr, pMsg );
	Q_vsnprintf( text, sizeof( text ), pMsg, argptr );
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