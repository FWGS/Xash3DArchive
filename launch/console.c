//=======================================================================
//			Copyright XashXT Group 2007 �
//			console.c - win and dos console
//=======================================================================

#include "launcher.h"

HINSTANCE	base_hInstance;
FILE	*logfile;
char	log_path[256];
LPTOP_LEVEL_EXCEPTION_FILTER	oldFilter = 0;

/*
===============================================================================

WIN32 CONSOLE

===============================================================================
*/

//console defines
#define SUBMIT_ID		1	// "submit" button
#define QUIT_ON_ESCPE_ID	2	// escape event
#define QUIT_ON_SPACE_ID	3	// space event
#define EDIT_ID		100
#define INPUT_ID		101
#define IDI_ICON1		101
#define SYSCONSOLE		"SystemConsole"

typedef struct
{
	HWND		hWnd;
	HWND		hwndBuffer;
	HWND		hwndButtonSubmit;
	HWND		hwndErrorBox;
	HWND		hwndErrorText;
	HBITMAP		hbmLogo;
	HBITMAP		hbmClearBitmap;
	HBRUSH		hbrEditBackground;
	HBRUSH		hbrErrorBackground;
	HFONT		hfBufferFont;
	HFONT		hfButtonFont;
	HWND		hwndInputLine;
	char		errorString[80];
	char		consoleText[512], returnedText[512];
	int		status;
	int		windowWidth, windowHeight;
	WNDPROC		SysInputLineWndProc;

} WinConData;
static WinConData s_wcd;
extern char caption[MAX_QPATH];

// gdi32 export table 
static HDC (_stdcall *pGetDC)(HWND);
static int (_stdcall *pReleaseDC)(HWND,HDC);
static bool (_stdcall *pDeleteObject)(HGDIOBJ);
static int (_stdcall *pGetDeviceCaps)(HDC,int);
static HBRUSH(_stdcall *pCreateSolidBrush)(COLORREF);
static COLORREF (_stdcall *pSetBkColor)(HDC,COLORREF);
static COLORREF (_stdcall *pSetTextColor)(HDC,COLORREF);
static HFONT (_stdcall *pCreateFont)(int,int,int,int,int,dword,dword,dword,dword,dword,dword,dword,dword,LPCSTR);

static dllfunc_t gdi32_funcs[] =
{
	{"GetDC", (void **) &pGetDC },
	{"ReleaseDC", (void **) &pReleaseDC },		
	{"SetBkColor", (void **) &pSetBkColor },
	{"CreateFontA", (void **) &pCreateFont },
	{"DeleteObject", (void **) &pDeleteObject },
	{"SetTextColor", (void **) &pSetTextColor },
	{"GetDeviceCaps", (void **) &pGetDeviceCaps },
	{"CreateSolidBrush", (void **) &pCreateSolidBrush },
	{ NULL, NULL }
};

dll_info_t gdi32_dll = { /*"gdi32.dll"*/NULL, gdi32_funcs, NULL, NULL, NULL, true, 0 }; // FIXME

void Sys_InitLog( void )
{
	// create log if needed
	if(!log_active || !strlen(log_path)) return;
	logfile = fopen ( log_path, "w");
	if(!logfile) Sys_Error("Sys_InitLog: can't create log file %s\n", log_path );

	fprintf (logfile, "=======================================================================\n" );
	fprintf (logfile, "\t%s started at %s\n", caption, time_stamp(TIME_FULL));
	fprintf (logfile, "=======================================================================\n");
}

void Sys_CloseLog( void )
{
	if(!logfile) return;

	fprintf (logfile, "\n");
	fprintf (logfile, "=======================================================================");
	fprintf (logfile, "\n\t%s stopped at %s\n", caption, time_stamp(TIME_FULL));
	fprintf (logfile, "=======================================================================");

	fclose(logfile);
	logfile = NULL;
}

void Sys_PrintLog( const char *pMsg )
{
	if(!logfile) return;

	fprintf (logfile, "%s", pMsg );
	fflush (logfile); // force it to save every time
}

void Sys_ShowConsoleW( bool show )
{
	if ( show == s_wcd.status ) return;
	s_wcd.status = show;
	if ( !s_wcd.hWnd ) return;

	if( show )
	{
		ShowWindow( s_wcd.hWnd, SW_SHOWNORMAL );
		SendMessage( s_wcd.hwndBuffer, EM_LINESCROLL, 0, 0xffff );
	}
	else ShowWindow( s_wcd.hWnd, SW_HIDE );
}

static LONG WINAPI ConWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool	s_timePolarity;
	PAINTSTRUCT	ps;
	HDC		hdc;

	switch (uMsg)
	{
	case WM_ACTIVATE:
		if ( LOWORD( wParam ) != WA_INACTIVE ) SetFocus( s_wcd.hwndInputLine );
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, (LPPAINTSTRUCT)&ps);
		EndPaint(hWnd,(LPPAINTSTRUCT)&ps);
		return TRUE;
		break;
	case WM_CLOSE:
		if(sys_error)
		{
			// send windows message
			PostQuitMessage( 0 );
		}
		else Sys_Exit(); //otherwise
		return 0;
	case WM_CTLCOLORSTATIC:
		if (( HWND )lParam == s_wcd.hwndBuffer )
		{
			SetBkColor( ( HDC ) wParam, RGB( 0x90, 0x90, 0x90 ));
			SetTextColor( ( HDC ) wParam, RGB( 0xff, 0xff, 0xff ));
			return ( long ) s_wcd.hbrEditBackground;
		}
		else if (( HWND )lParam == s_wcd.hwndErrorBox )
		{
			if ( s_timePolarity & 1 )
			{
				SetBkColor(( HDC )wParam, RGB( 0x80, 0x80, 0x80 ));
				SetTextColor(( HDC )wParam, RGB( 0xff, 0x0, 0x00 ));
			}
			else
			{
				SetBkColor(( HDC )wParam, RGB( 0x80, 0x80, 0x80 ));
				SetTextColor(( HDC )wParam, RGB( 0x00, 0x0, 0x00 ));
			}
			return ( long )s_wcd.hbrErrorBackground;
		}
		break;
	case WM_COMMAND:
		if ( wParam == SUBMIT_ID )
		{
			SendMessage(s_wcd.hwndInputLine, WM_CHAR, 13, 0L);
			SetFocus( s_wcd.hwndInputLine );
		}
		break;
	case WM_HOTKEY:
		switch(LOWORD(wParam))
		{
		case QUIT_ON_ESCPE_ID:
		case QUIT_ON_SPACE_ID:
			PostQuitMessage( 0 );
			break;
		}
		break;
	case WM_CREATE:
		s_wcd.hbrEditBackground = CreateSolidBrush( RGB( 0x90, 0x90, 0x90 ) );
		s_wcd.hbrErrorBackground = CreateSolidBrush( RGB( 0x80, 0x80, 0x80 ) );
		SetTimer( hWnd, 1, 1000, NULL );
		break;
	case WM_ERASEBKGND:
		return DefWindowProc( hWnd, uMsg, wParam, lParam );
	case WM_TIMER:
		if ( wParam == 1 )
		{
			s_timePolarity = !s_timePolarity;
			if ( s_wcd.hwndErrorBox ) InvalidateRect( s_wcd.hwndErrorBox, NULL, FALSE );
		}
		break;
	}
	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

LONG WINAPI InputLineWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char inputBuffer[1024];

	switch ( uMsg )
	{
	case WM_KILLFOCUS:
		if (( HWND ) wParam == s_wcd.hWnd || ( HWND ) wParam == s_wcd.hwndErrorBox )
		{
			SetFocus( hWnd );
			return 0;
		}
		break;
	case WM_CHAR:
		if ( wParam == 13 )
		{
			GetWindowText( s_wcd.hwndInputLine, inputBuffer, sizeof( inputBuffer ) );
			strncat( s_wcd.consoleText, inputBuffer, sizeof( s_wcd.consoleText ) - strlen( s_wcd.consoleText ) - 5 );
			strcat( s_wcd.consoleText, "\n" );
			SetWindowText( s_wcd.hwndInputLine, "" );
			Msg(">%s\n", inputBuffer );
			return 0;
		}
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
Sys_PrintW

print into window console
================
*/
void Sys_Print(const char *pMsg)
{
	const char	*msg;
	char		buffer[MAX_INPUTLINE * 2];
	char		logbuf[MAX_INPUTLINE * 2];
	char		*b = buffer;
	char		*c = logbuf;	
	int		i = 0;

	// if the message is REALLY long, use just the last portion of it
	if ( strlen( pMsg ) > MAX_INPUTLINE - 1 )
		msg = pMsg + strlen( pMsg ) - MAX_INPUTLINE + 1;
	else msg = pMsg;

	// copy into an intermediate buffer
	while ( msg[i] && (( b - buffer ) < sizeof( buffer ) - 1 ))
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
		else if(IsColorString( &msg[i])) i++; // skip color prefix
		else
		{
			*b = *c = msg[i];
			b++, c++;
		}
		i++;
	}
	*b = *c = 0; // cutoff garbage

	Sys_PrintLog( logbuf );
	Msg_Print( buffer );
}

/*
================
Msg_PrintA

print into cmd32 console
================
*/
void Msg_PrintA(const char *pMsg)
{
	DWORD	cbWritten;

	WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), pMsg, strlen(pMsg), &cbWritten, 0 );
	//write(1, pMsg, strlen(pMsg));
}

/*
================
Msg_PrintW

print into window console
================
*/
void Msg_PrintW(const char *pMsg)
{
	// replace selection instead of appending if we're overflowing
	if( strlen(pMsg) > 0x7fff )
	{
		SendMessage( s_wcd.hwndBuffer, EM_SETSEL, 0, -1 );
	} 

	// put this text into the windows console
	SendMessage( s_wcd.hwndBuffer, EM_LINESCROLL, 0, 0xffff );
	SendMessage( s_wcd.hwndBuffer, EM_SCROLLCARET, 0, 0 );
	SendMessage( s_wcd.hwndBuffer, EM_REPLACESEL, 0, (LPARAM)pMsg );
}

/*
================
Sys_Msg

formatted message
================
*/
void Sys_MsgW( const char *pMsg, ... )
{
	va_list		argptr;
	char text[MAX_INPUTLINE];
	
	va_start (argptr, pMsg);
	vsprintf (text, pMsg, argptr);
	va_end (argptr);

	Sys_Print( text );
}

void Sys_MsgDevW( int level, const char *pMsg, ... )
{
	va_list	argptr;
	char	text[MAX_INPUTLINE];
	
	if(dev_mode < level) return;

	va_start (argptr, pMsg);
	vsprintf (text, pMsg, argptr);
	va_end (argptr);
	Sys_Print( text );
}

void Sys_MsgWarnW( const char *pMsg, ... )
{
	va_list	argptr;
	char	text[MAX_INPUTLINE];
	
	if(!debug_mode) return;

	va_start (argptr, pMsg);
	vsprintf (text, pMsg, argptr);
	va_end (argptr);
	Sys_Print( text );
}

/*
================
Sys_CreateConsoleW

create win32 console
================
*/
void Sys_CreateConsoleW( void )
{
	HDC hDC;
	WNDCLASS wc;
	RECT rect;
	int nHeight;
	int swidth, sheight, fontsize;
	int DEDSTYLE = WS_POPUPWINDOW | WS_CAPTION;
	int CONSTYLE = WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | WS_EX_CLIENTEDGE | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY;
	char Title[MAX_QPATH], FontName[MAX_QPATH];

	memset( &wc, 0, sizeof( wc ) );

	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC)ConWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = base_hInstance;
	wc.hIcon         = LoadIcon( base_hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = (void *)COLOR_3DSHADOW;
	wc.lpszMenuName  = 0;
	wc.lpszClassName = SYSCONSOLE;

	Sys_LoadLibrary( &gdi32_dll );
	if (!RegisterClass (&wc)) Sys_ErrorFatal( ERR_CONSOLE_FAIL );
 
	if(about_mode)
	{
		CONSTYLE &= ~WS_VSCROLL;
		rect.left = 0;
		rect.right = 536;
		rect.top = 0;
		rect.bottom = 280;
		strncpy(FontName, "Arial", MAX_QPATH );
		fontsize = 16;
	}
	else if(console_read_only)
	{
		rect.left = 0;
		rect.right = 536;
		rect.top = 0;
		rect.bottom = 364;
		strncpy(FontName, "Fixedsys", MAX_QPATH );
		fontsize = 8;
	}
	else // dedicated console
	{
		rect.left = 0;
		rect.right = 540;
		rect.top = 0;
		rect.bottom = 392;
		strncpy(FontName, "System", MAX_QPATH );
		fontsize = 14;
	}

	strncpy( Title, caption, MAX_QPATH );
	AdjustWindowRect( &rect, DEDSTYLE, FALSE );

	hDC = GetDC( GetDesktopWindow() );
	swidth = GetDeviceCaps( hDC, HORZRES );
	sheight = GetDeviceCaps( hDC, VERTRES );
	ReleaseDC( GetDesktopWindow(), hDC );

	s_wcd.windowWidth = rect.right - rect.left;
	s_wcd.windowHeight = rect.bottom - rect.top;

	s_wcd.hWnd = CreateWindowEx( WS_EX_DLGMODALFRAME, SYSCONSOLE, Title, DEDSTYLE, ( swidth - 600 ) / 2, ( sheight - 450 ) / 2 , rect.right - rect.left + 1, rect.bottom - rect.top + 1, NULL, NULL, base_hInstance, NULL );
	if ( s_wcd.hWnd == NULL ) Sys_ErrorFatal( ERR_CONSOLE_FAIL );

	// create fonts
	hDC = GetDC( s_wcd.hWnd );
	nHeight = -MulDiv( fontsize, GetDeviceCaps( hDC, LOGPIXELSY), 72);
	s_wcd.hfBufferFont = CreateFont( nHeight, 0, 0, 0, FW_LIGHT, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_MODERN | FIXED_PITCH, FontName );
	ReleaseDC( s_wcd.hWnd, hDC );

	if(!console_read_only)
	{
		// create the input line
		s_wcd.hwndInputLine = CreateWindowEx( WS_EX_CLIENTEDGE, "edit", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL, 0, 366, 450, 25, s_wcd.hWnd, ( HMENU ) INPUT_ID, base_hInstance, NULL );

		s_wcd.hwndButtonSubmit = CreateWindow( "button", NULL, BS_PUSHBUTTON | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 452, 367, 87, 25, s_wcd.hWnd, ( HMENU ) SUBMIT_ID, base_hInstance, NULL );
		SendMessage( s_wcd.hwndButtonSubmit, WM_SETTEXT, 0, ( LPARAM ) "submit" );
          }
          
	// create the scrollbuffer
	GetClientRect(s_wcd.hWnd, &rect);

	s_wcd.hwndBuffer = CreateWindowEx(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE, "edit", NULL, CONSTYLE, 0, 0, rect.right - rect.left, min(365, rect.bottom), s_wcd.hWnd, ( HMENU )EDIT_ID, base_hInstance, NULL );
	SendMessage( s_wcd.hwndBuffer, WM_SETFONT, ( WPARAM ) s_wcd.hfBufferFont, 0 );

	if(!console_read_only)
	{
		s_wcd.SysInputLineWndProc = ( WNDPROC ) SetWindowLong( s_wcd.hwndInputLine, GWL_WNDPROC, ( long ) InputLineWndProc );
		SendMessage( s_wcd.hwndInputLine, WM_SETFONT, ( WPARAM ) s_wcd.hfBufferFont, 0 );
          }

	Sys_InitLog();

	//show console if needed
	if( show_always )
	{          
		//make console visible
		ShowWindow( s_wcd.hWnd, SW_SHOWDEFAULT);
		UpdateWindow( s_wcd.hWnd );
		SetForegroundWindow( s_wcd.hWnd );

		if(console_read_only) SetFocus( s_wcd.hWnd );
		else SetFocus( s_wcd.hwndInputLine );
		s_wcd.status = true;
          }
	else s_wcd.status = false;
}

/*
================
Sys_DestroyConsoleW

destroy win32 console
================
*/
void Sys_DestroyConsoleW( void )
{
	// last text message into console or log 
	MsgDev(D_ERROR, "Sys_FreeLibrary: Unloading launch.dll\n");

	if ( s_wcd.hWnd )
	{

		DeleteObject(s_wcd.hbrEditBackground);
		DeleteObject( s_wcd.hbrErrorBackground);
                    DeleteObject( s_wcd.hfBufferFont );
		
		ShowWindow( s_wcd.hWnd, SW_HIDE );
		DestroyWindow( s_wcd.hWnd );
		s_wcd.hWnd = 0;
	}

	UnregisterClass (SYSCONSOLE, base_hInstance);
	Sys_FreeLibrary( &gdi32_dll );
	Sys_CloseLog();
}

/*
================
Sys_InputW

returned input text 
================
*/
char *Sys_InputW( void )
{
	if ( s_wcd.consoleText[0] == 0 ) return NULL;
		
	strcpy( s_wcd.returnedText, s_wcd.consoleText );
	s_wcd.consoleText[0] = 0;
	
	return s_wcd.returnedText;
}

/*
================
Sys_ErrorW

NOTE: we must prepare engine to shutdown
before call this
================
*/
void Sys_ErrorW(char *error, ...)
{
	va_list		argptr;
	char		text[MAX_INPUTLINE];
         
	if(sys_error) return; //don't multiple executes
	
	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);
         
	sys_error = true;
	
	Sys_ShowConsole( true );
	Sys_Print( text ); //print error message
	
	SetFocus( s_wcd.hWnd );

	Sys_WaitForQuit();
	Sys_Exit();
}

/*
================
Sys_ErrorA

NOTE: we must prepare engine to shutdown
before call this
================
*/
void Sys_ErrorA(char *error, ...)
{
	va_list		argptr;
	char		text[MAX_INPUTLINE];
         
	if(sys_error) return; //don't multiple executes
	
	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);
         
	sys_error = true;
	
	Sys_ShowConsole( true );
	Sys_Print( text ); //print error message

	Sys_Print("press any key to quit\n");
	getchar();  //wait for quit
	Sys_Exit();
}

/*
================
Sys_FatalError

called while internal debugging tools 
are failed to initialize.
use generic msgbox
================
*/
void _Sys_ErrorFatal( int type, const char *filename, int fileline )
{
	char errorstring[64];

	switch( type )
	{
		case ERR_INVALID_ROOT:
			strncpy(errorstring, "Invalid root directory!", sizeof(errorstring));
			break;
		case ERR_CONSOLE_FAIL:
			strncpy(errorstring, "Can't create console window", sizeof(errorstring));
			break;
		case ERR_OSINFO_FAIL:
			strncpy(errorstring, "Couldn't get OS info", sizeof(errorstring));
			break;
		case ERR_INVALID_VER:
			strncpy(errorstring, "Requries Win95 or later", sizeof(errorstring));
			break;
		case ERR_WINDOWS_32S:
			strncpy(errorstring, "Win32s is not supported", sizeof(errorstring));
			break;
		default:
			sprintf(errorstring, "Internal engine error at %s:%i", filename, fileline );
			break;
	}
	MessageBox( 0, errorstring, "Error", MB_OK );
	exit(1);
}

void Sys_WaitForQuit( void )
{
	MSG		msg;

	// user can hit escape or space for quit
	RegisterHotKey(s_wcd.hWnd, QUIT_ON_ESCPE_ID, 0, VK_ESCAPE );
	RegisterHotKey(s_wcd.hWnd, QUIT_ON_SPACE_ID, 0, VK_SPACE  );
	ZeroMemory(&msg, sizeof(msg));

	// wait for the user to quit
	while(!hooked_out && msg.message != WM_QUIT)
	{
		if(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} 
		else Sys_Sleep( 20 );
	}
}

long WINAPI Sys_ExecptionFilter( PEXCEPTION_POINTERS pExceptionInfo )
{
	// save config
	Sys_Print("Engine crashed\n");
	Host_Free(); // prepare host to close
	Sys_FreeLibrary( linked_dll );
	Sys_FreeConsole();	

	if( oldFilter ) return oldFilter( pExceptionInfo );
	return EXCEPTION_CONTINUE_SEARCH;
	//return EXCEPTION_CONTINUE_EXECUTION;
}