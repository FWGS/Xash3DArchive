//=======================================================================
//			Copyright XashXT Group 2011 ©
//		        sys_win.c - platform dependent code
//=======================================================================

#include "common.h"

#define MAX_QUED_EVENTS	256
#define MASK_QUED_EVENTS	(MAX_QUED_EVENTS - 1)

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
		if( Mem_IsAllocated( host.mempool, ev->data ))
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
	s = Sys_Input();
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