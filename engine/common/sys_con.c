//=======================================================================
//			Copyright XashXT Group 2007 �
//		        sys_con.c - win32 dediacted console
//=======================================================================

#include "common.h"

/*
===============================================================================

WIN32 CONSOLE

===============================================================================
*/

// console defines
#define SUBMIT_ID		1	// "submit" button
#define QUIT_ON_ESCAPE_ID	2	// escape event
#define EDIT_ID		110
#define INPUT_ID		109
#define IDI_ICON1		101

#define SYSCONSOLE		"XashConsole"
#define COMMAND_HISTORY	64	// system console keep more commands than game console

typedef struct
{
	char		title[64];
	HWND		hWnd;
	HWND		hwndBuffer;
	HWND		hwndButtonSubmit;
	HBRUSH		hbrEditBackground;
	HFONT		hfBufferFont;
	HFONT		hfButtonFont;
	HWND		hwndInputLine;
	string		consoleText;
	string		returnedText;
	string		historyLines[COMMAND_HISTORY];
	int		nextHistoryLine;
	int		historyLine;
	int		status;
	int		windowWidth, windowHeight;
	WNDPROC		SysInputLineWndProc;
	size_t		outLen;

	// log stuff
	qboolean		log_active;
	char		log_path[MAX_SYSPATH];
	FILE		*logfile;
} WinConData;

static WinConData	s_wcd;

void Con_ShowConsole( qboolean show )
{
	if( !s_wcd.hWnd || show == s_wcd.status )
		return;

	s_wcd.status = show;
	if( show )
	{
		ShowWindow( s_wcd.hWnd, SW_SHOWNORMAL );
		SendMessage( s_wcd.hwndBuffer, EM_LINESCROLL, 0, 0xffff );
	}
	else ShowWindow( s_wcd.hWnd, SW_HIDE );
}

void Con_DisableInput( void )
{
	if( host.type != HOST_DEDICATED ) return;
	SendMessage( s_wcd.hwndButtonSubmit, WM_ENABLE, 0, 0 );
	SendMessage( s_wcd.hwndInputLine, WM_ENABLE, 0, 0 );
}

void Con_SetInputText( const char *inputText )
{
	if( host.type != HOST_DEDICATED ) return;
	SetWindowText( s_wcd.hwndInputLine, inputText );
	SendMessage( s_wcd.hwndInputLine, EM_SETSEL, Q_strlen( inputText ), -1 );
}

static int Con_KeyEvent( int key, qboolean down )
{
	char	inputBuffer[1024];

	if( !down )
		return 0;

	switch( key )
	{
	case VK_TAB:
		GetWindowText( s_wcd.hwndInputLine, inputBuffer, sizeof( inputBuffer ));
		Cmd_AutoComplete( inputBuffer );
		Con_SetInputText( inputBuffer );
		return 1;
	case VK_DOWN:
		if( s_wcd.historyLine == s_wcd.nextHistoryLine )
			return 0;
		s_wcd.historyLine++;
		Con_SetInputText( s_wcd.historyLines[s_wcd.historyLine % COMMAND_HISTORY] );
		return 1;
	case VK_UP:
		if( s_wcd.nextHistoryLine - s_wcd.historyLine < COMMAND_HISTORY && s_wcd.historyLine > 0 )
			s_wcd.historyLine--;
		Con_SetInputText( s_wcd.historyLines[s_wcd.historyLine % COMMAND_HISTORY] );
		return 1;
	}
	return 0;
}

static long _stdcall Con_WndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch( uMsg )
	{
	case WM_ACTIVATE:
		if( LOWORD( wParam ) != WA_INACTIVE )
			SetFocus( s_wcd.hwndInputLine );
		break;
	case WM_CLOSE:
		if( host.state == HOST_ERR_FATAL )
		{
			// send windows message
			PostQuitMessage( 0 );
		}
		else Sys_Quit(); // otherwise
		return 0;
	case WM_CTLCOLORSTATIC:
		if((HWND)lParam == s_wcd.hwndBuffer )
		{
			SetBkColor((HDC)wParam, RGB( 0x90, 0x90, 0x90 ));
			SetTextColor((HDC)wParam, RGB( 0xff, 0xff, 0xff ));
			return (long)s_wcd.hbrEditBackground;
		}
		break;
	case WM_COMMAND:
		if( wParam == SUBMIT_ID )
		{
			SendMessage( s_wcd.hwndInputLine, WM_CHAR, 13, 0L );
			SetFocus( s_wcd.hwndInputLine );
		}
		break;
	case WM_HOTKEY:
		switch( LOWORD( wParam ))
		{
		case QUIT_ON_ESCAPE_ID:
			PostQuitMessage( 0 );
			break;
		}
		break;
	case WM_CREATE:
		s_wcd.hbrEditBackground = CreateSolidBrush( RGB( 0x90, 0x90, 0x90 ));
		break;
	}
	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

long _stdcall Con_InputLineProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	char	inputBuffer[1024];

	switch( uMsg )
	{
	case WM_KILLFOCUS:
		if(( HWND )wParam == s_wcd.hWnd )
		{
			SetFocus( hWnd );
			return 0;
		}
		break;
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
		if( Con_KeyEvent( LOWORD( wParam ), true ))
			return 0;
		break;
	case WM_SYSKEYUP:
	case WM_KEYUP:
		if( Con_KeyEvent( LOWORD( wParam ), false ))
			return 0;
		break;
	case WM_CHAR:
		if( Con_KeyEvent( wParam, true ))
			return 0;
		if( wParam == 13 && host.state != HOST_ERR_FATAL )
		{
			GetWindowText( s_wcd.hwndInputLine, inputBuffer, sizeof( inputBuffer ));
			Q_strncat( s_wcd.consoleText, inputBuffer, sizeof( s_wcd.consoleText ) - Q_strlen( s_wcd.consoleText ) - 5 );
			Q_strcat( s_wcd.consoleText, "\n" );
			SetWindowText( s_wcd.hwndInputLine, "" );
			Msg( ">%s\n", inputBuffer );

			// copy line to history buffer
			Q_strncpy( s_wcd.historyLines[s_wcd.nextHistoryLine % COMMAND_HISTORY], inputBuffer, MAX_STRING );
			s_wcd.nextHistoryLine++;
			s_wcd.historyLine = s_wcd.nextHistoryLine;
			return 0;
		}
		break;
	}
	return CallWindowProc( s_wcd.SysInputLineWndProc, hWnd, uMsg, wParam, lParam );
}


/*
===============================================================================

WIN32 IO

===============================================================================
*/
/*
================
Con_WinPrint

print into window console
================
*/
void Con_WinPrint( const char *pMsg )
{
	size_t	len = Q_strlen( pMsg );

	// replace selection instead of appending if we're overflowing
	s_wcd.outLen += len;
	if( s_wcd.outLen >= 0x7fff )
	{
		SendMessage( s_wcd.hwndBuffer, EM_SETSEL, 0, -1 );
		s_wcd.outLen = len;
	} 

	SendMessage( s_wcd.hwndBuffer, EM_REPLACESEL, 0, (LPARAM)pMsg );

	// put this text into the windows console
	SendMessage( s_wcd.hwndBuffer, EM_LINESCROLL, 0, 0xffff );
	SendMessage( s_wcd.hwndBuffer, EM_SCROLLCARET, 0, 0 );
}


/*
================
Con_CreateConsole

create win32 console
================
*/
void Con_CreateConsole( void )
{
	HDC	hDC;
	WNDCLASS	wc;
	RECT	rect;
	int	nHeight;
	int	swidth, sheight, fontsize;
	int	DEDSTYLE = WS_POPUPWINDOW | WS_CAPTION;
	int	CONSTYLE = WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_BORDER|WS_EX_CLIENTEDGE|ES_LEFT|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY;
	string	FontName;

	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC)Con_WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = host.hInst;
	wc.hIcon         = LoadIcon( host.hInst, MAKEINTRESOURCE( IDI_ICON1 ));
	wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (void *)COLOR_3DSHADOW;
	wc.lpszClassName = SYSCONSOLE;
	wc.lpszMenuName  = 0;

	if( Sys_CheckParm( "-log" ) && host.developer != 0 )
		s_wcd.log_active = true;

	if( host.type == HOST_CREDITS )
	{
		CONSTYLE &= ~WS_VSCROLL;
		rect.left = 0;
		rect.right = 536;
		rect.top = 0;
		rect.bottom = 280;
		Q_strncpy( FontName, "Arial", sizeof( FontName ));
		Q_strncpy( s_wcd.title, "About", sizeof( s_wcd.title ));
		s_wcd.log_active = false;
		fontsize = 16;
	}
	else if( host.type != HOST_DEDICATED )
	{
		rect.left = 0;
		rect.right = 536;
		rect.top = 0;
		rect.bottom = 364;
		Q_strncpy( FontName, "Fixedsys", sizeof( FontName ));
		Q_strncpy( s_wcd.title, va( "Xash3D %g", XASH_VERSION ), sizeof( s_wcd.title ));
		Q_strncpy( s_wcd.log_path, "engine.log", sizeof( s_wcd.log_path ));
		fontsize = 8;
	}
	else // dedicated console
	{
		rect.left = 0;
		rect.right = 640;
		rect.top = 0;
		rect.bottom = 392;
		Q_strncpy( FontName, "System", sizeof( FontName ));
		Q_strncpy( s_wcd.title, "Xash Dedicated Server", sizeof( s_wcd.title ));
		Q_strncpy( s_wcd.log_path, "dedicated.log", sizeof( s_wcd.log_path ));
		fontsize = 14;
	}

	Sys_InitLog();

	if( !RegisterClass( &wc ))
	{
		// print into log
		MsgDev( D_ERROR, "Can't register window class '%s'\n", SYSCONSOLE );
		return;
	} 

	AdjustWindowRect( &rect, DEDSTYLE, FALSE );

	hDC = GetDC( GetDesktopWindow() );
	swidth = GetDeviceCaps( hDC, HORZRES );
	sheight = GetDeviceCaps( hDC, VERTRES );
	ReleaseDC( GetDesktopWindow(), hDC );

	s_wcd.windowWidth = rect.right - rect.left;
	s_wcd.windowHeight = rect.bottom - rect.top;

	s_wcd.hWnd = CreateWindowEx( WS_EX_DLGMODALFRAME, SYSCONSOLE, s_wcd.title, DEDSTYLE, ( swidth - 600 ) / 2, ( sheight - 450 ) / 2 , rect.right - rect.left + 1, rect.bottom - rect.top + 1, NULL, NULL, host.hInst, NULL );
	if( s_wcd.hWnd == NULL )
	{
		MsgDev( D_ERROR, "Can't create window '%s'\n", s_wcd.title );
		return;
	}

	// create fonts
	hDC = GetDC( s_wcd.hWnd );
	nHeight = -MulDiv( fontsize, GetDeviceCaps( hDC, LOGPIXELSY ), 72 );
	s_wcd.hfBufferFont = CreateFont( nHeight, 0, 0, 0, FW_LIGHT, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_MODERN|FIXED_PITCH, FontName );
	ReleaseDC( s_wcd.hWnd, hDC );

	if( host.type == HOST_DEDICATED )
	{
		// create the input line
		s_wcd.hwndInputLine = CreateWindowEx( WS_EX_CLIENTEDGE, "edit", NULL, WS_CHILD|WS_VISIBLE|WS_BORDER|ES_LEFT|ES_AUTOHSCROLL, 0, 366, 550, 25, s_wcd.hWnd, (HMENU)INPUT_ID, host.hInst, NULL );

		s_wcd.hwndButtonSubmit = CreateWindow( "button", NULL, BS_PUSHBUTTON|WS_VISIBLE|WS_CHILD|BS_DEFPUSHBUTTON, 552, 367, 87, 25, s_wcd.hWnd, (HMENU)SUBMIT_ID, host.hInst, NULL );
		SendMessage( s_wcd.hwndButtonSubmit, WM_SETTEXT, 0, ( LPARAM ) "submit" );
          }
          
	// create the scrollbuffer
	GetClientRect( s_wcd.hWnd, &rect );

	s_wcd.hwndBuffer = CreateWindowEx( WS_EX_DLGMODALFRAME|WS_EX_CLIENTEDGE, "edit", NULL, CONSTYLE, 0, 0, rect.right - rect.left, min(365, rect.bottom), s_wcd.hWnd, (HMENU)EDIT_ID, host.hInst, NULL );
	SendMessage( s_wcd.hwndBuffer, WM_SETFONT, (WPARAM)s_wcd.hfBufferFont, 0 );

	if( host.type == HOST_DEDICATED )
	{
		s_wcd.SysInputLineWndProc = (WNDPROC)SetWindowLong( s_wcd.hwndInputLine, GWL_WNDPROC, (long)Con_InputLineProc );
		SendMessage( s_wcd.hwndInputLine, WM_SETFONT, ( WPARAM )s_wcd.hfBufferFont, 0 );
          }

	// show console if needed
	if( host.con_showalways )
	{          
		// make console visible
		ShowWindow( s_wcd.hWnd, SW_SHOWDEFAULT );
		UpdateWindow( s_wcd.hWnd );
		SetForegroundWindow( s_wcd.hWnd );

		if( host.type != HOST_DEDICATED )
			SetFocus( s_wcd.hWnd );
		else SetFocus( s_wcd.hwndInputLine );
		s_wcd.status = true;
          }
	else s_wcd.status = false;
}

/*
================
Con_DestroyConsole

destroy win32 console
================
*/
void Con_DestroyConsole( void )
{
	// last text message into console or log 
	MsgDev( D_NOTE, "Sys_FreeLibrary: Unloading xash.dll\n" );

	Sys_CloseLog();

	if( s_wcd.hWnd )
	{
		DeleteObject( s_wcd.hbrEditBackground );
                    DeleteObject( s_wcd.hfBufferFont );
		
		ShowWindow( s_wcd.hWnd, SW_HIDE );
		DestroyWindow( s_wcd.hWnd );
		s_wcd.hWnd = 0;
	}

	UnregisterClass( SYSCONSOLE, host.hInst );

	// place it here in case Sys_Crash working properly
	if( host.hMutex ) CloseHandle( host.hMutex );
}

/*
================
Con_Input

returned input text 
================
*/
char *Con_Input( void )
{
	if( s_wcd.consoleText[0] == 0 )
		return NULL;
		
	Q_strncpy( s_wcd.returnedText, s_wcd.consoleText, sizeof( s_wcd.returnedText ));
	s_wcd.consoleText[0] = 0;
	
	return s_wcd.returnedText;
}

/*
================
Con_SetFocus

change focus to console hwnd 
================
*/
void Con_RegisterHotkeys( void )
{
	SetFocus( s_wcd.hWnd );

	// user can hit escape for quit
	RegisterHotKey( s_wcd.hWnd, QUIT_ON_ESCAPE_ID, 0, VK_ESCAPE );
}

/*
===============================================================================

SYSTEM LOG

===============================================================================
*/
void Sys_InitLog( void )
{
	const char	*mode;

	if( host.change_game )
		mode = "a";
	else mode = "w";

	// create log if needed
	if( s_wcd.log_active )
	{
		s_wcd.logfile = fopen( s_wcd.log_path, mode );
		if( !s_wcd.logfile ) MsgDev( D_ERROR, "Sys_InitLog: can't create log file %s\n", s_wcd.log_path );

		fprintf( s_wcd.logfile, "=======================================================================\n" );
		fprintf( s_wcd.logfile, "\t%s started at %s\n", s_wcd.title, Q_timestamp( TIME_FULL ));
		fprintf( s_wcd.logfile, "=======================================================================\n");
	}
}

void Sys_CloseLog( void )
{
	char	event_name[64];

	// continue logged
	switch( host.state )
	{
	case HOST_CRASHED:
		Q_strncpy( event_name, "crashed", sizeof( event_name ));
		break;
	case HOST_ERR_FATAL:
		Q_strncpy( event_name, "stopped with error", sizeof( event_name ));
		break;
	default:
		if( !host.change_game ) Q_strncpy( event_name, "stopped", sizeof( event_name ));
		else Q_strncpy( event_name, host.finalmsg, sizeof( event_name ));
		break;
	}

	if( s_wcd.logfile )
	{
		fprintf( s_wcd.logfile, "\n");
		fprintf( s_wcd.logfile, "=======================================================================");
		fprintf( s_wcd.logfile, "\n\t%s %s at %s\n", s_wcd.title, event_name, Q_timestamp( TIME_FULL ));
		fprintf( s_wcd.logfile, "=======================================================================\n");
		if( host.change_game ) fprintf( s_wcd.logfile, "\n" ); // just for tabulate

		fclose( s_wcd.logfile );
		s_wcd.logfile = NULL;
	}
}

void Sys_PrintLog( const char *pMsg )
{
	if( !s_wcd.logfile ) return;
	fprintf( s_wcd.logfile, pMsg );
	fflush( s_wcd.logfile );
}