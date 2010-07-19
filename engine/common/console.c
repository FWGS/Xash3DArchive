//=======================================================================
//			Copyright XashXT Group 2007 ©
//			con_main.c - client console
//=======================================================================

#include "common.h"
#include "client.h"
#include "keydefs.h"

cvar_t	*con_notifytime;
cvar_t	*con_speed;
cvar_t	*con_font;

#define DEFAULT_CONSOLE_WIDTH	78
#define COLOR_BLACK		'0'
#define COLOR_RED		'1'
#define COLOR_GREEN		'2'
#define COLOR_YELLOW	'3'
#define COLOR_BLUE		'4'
#define COLOR_CYAN		'5'
#define COLOR_MAGENTA	'6'
#define COLOR_WHITE		'7'

#define CON_TIMES		5		// need for 4 lines
#define CON_HISTORY		32
#define CON_TEXTSIZE	(MAX_MSGLEN * 4)	// 128 kb buffer

int g_console_field_width = 78;

// console color typeing
rgba_t g_color_table[8] =
{
{  0,   0,   0, 255},
{255,   0,   0, 255},
{  0, 255,   0, 255},
{255, 255,   0, 255},
{  0,   0, 255, 255},
{  0, 255, 255, 255},
{255,   0, 255, 255},
{255, 255, 255, 255},
};

typedef struct
{
	string	buffer;
	int	cursor;
	int	scroll;
	int	widthInChars;
} field_t;

typedef struct
{
	bool	initialized;

	short	text[CON_TEXTSIZE];
	int	current;		// line where next message will be printed
	int	x;		// offset in current line for next print
	int	display;		// bottom of console displays this line

	int 	linewidth;	// characters across screen
	int	totallines;	// total lines in console scrollback

	float	xadjust;		// for wide aspect screens

	float	displayFrac;	// aproaches finalFrac at con_speed
	float	finalFrac;	// 0.0 to 1.0 lines of console to display

	int	vislines;		// in scanlines
	double	times[CON_TIMES];	// host.realtime the line was generated for transparent notify lines
	rgba_t	color;

	// console input
	field_t	input;

	// console history
	field_t	historyLines[CON_HISTORY];
	int	historyLine;	// the line being displayed from history buffer will be <= nextHistoryLine
	int	nextHistoryLine;	// the last line in the history buffer, not masked

	// console auto-complete
	field_t	*completionField;
	char	*completionString;
	string	shortestMatch;
	int	matchCount;
} console_t;

static console_t con;

void Field_CharEvent( field_t *edit, int ch );

/*
================
Con_Clear_f
================
*/
void Con_Clear_f( void )
{
	int	i;

	for( i = 0; i < CON_TEXTSIZE; i++ )
		con.text[i] = ( ColorIndex( COLOR_WHITE ) << 8 ) | ' ';
	Con_Bottom(); // go to end
}
						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify( void )
{
	int	i;
	
	for( i = 0; i < CON_TIMES; i++ )
		con.times[i] = 0.0;
}

/*
================
Con_ClearField
================
*/
void Con_ClearField( field_t *edit )
{
	Mem_Set( edit->buffer, 0, MAX_STRING );
	edit->cursor = 0;
	edit->scroll = 0;
}

/*
================
Con_ClearTyping
================
*/
void Con_ClearTyping( void )
{
	Con_ClearField( &con.input );
	con.input.widthInChars = g_console_field_width;
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f( void )
{
	if( !host.developer ) return;	// disabled

	// show console only in game or by special call from menu
	if( cls.state != ca_active || cls.key_dest == key_menu )
		return;

	Con_ClearTyping();
	Con_ClearNotify();

	if( cls.key_dest == key_console )
	{
		UI_SetActiveMenu( false );
		cls.key_dest = key_game;
	}
	else
	{
		UI_SetActiveMenu( false );
		cls.key_dest = key_console;	
	}
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize( void )
{
	int	i, j, width, oldwidth, oldtotallines, numlines, numchars;
	short	tbuf[CON_TEXTSIZE];

	width = SCREEN_WIDTH / SMALLCHAR_WIDTH - 2;

	if( width == con.linewidth )
		return;

	if( re == NULL ) // video hasn't been initialized yet
	{
		width = DEFAULT_CONSOLE_WIDTH;
		con.linewidth = width;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		for(i = 0; i < CON_TEXTSIZE; i++)
			con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
	}
	else
	{
		oldwidth = con.linewidth;
		con.linewidth = g_console_field_width;
		oldtotallines = con.totallines;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		numlines = oldtotallines;

		if( con.totallines < numlines )
			numlines = con.totallines;

		numchars = oldwidth;
	
		if( con.linewidth < numchars )
			numchars = con.linewidth;

		Mem_Copy( tbuf, con.text, CON_TEXTSIZE * sizeof( short ));
		for( i = 0; i < CON_TEXTSIZE; i++ )
			con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';

		for (i = 0; i < numlines; i++)
		{
			for (j = 0; j < numchars; j++)
			{
				con.text[(con.totallines - 1 - i) * con.linewidth + j] =
						tbuf[((con.current - i + oldtotallines) % oldtotallines) * oldwidth + j];
			}
		}
		Con_ClearNotify ();
	}

	con.current = con.totallines - 1;
	con.display = con.current;
}


/*
================
Con_Init
================
*/
void Con_Init( void )
{
	int	i;

	Con_CheckResize();

	// register our commands
	con_notifytime = Cvar_Get( "con_notifytime", "3", 0, "notify time to live" );
	con_speed = Cvar_Get( "scr_conspeed", "600", 0, "console moving speed" );
	con_font = Cvar_Get( "con_font", "default", CVAR_ARCHIVE, "path to console charset" );

	Con_ClearField( &con.input );
	con.input.widthInChars = g_console_field_width;

	for( i = 0; i < CON_HISTORY; i++ )
	{
		Con_ClearField( &con.historyLines[i] );
		con.historyLines[i].widthInChars = g_console_field_width;
	}

	Cmd_AddCommand( "toggleconsole", Con_ToggleConsole_f, "opens or closes the console" );
	Cmd_AddCommand( "clear", Con_Clear_f, "clear console history" );

	MsgDev( D_NOTE, "Console initialized.\n" );
	con.initialized = true;
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed( bool skipnotify )
{
	int	i;

	// mark time for transparent overlay
	if( con.current >= 0 )
	{
		if( skipnotify ) con.times[con.current % CON_TIMES] = 0;
		else con.times[con.current % CON_TIMES] = host.realtime;
	}

	con.x = 0;
	if( con.display == con.current ) con.display++;
	con.current++;
	for( i = 0; i < con.linewidth; i++ )
		con.text[(con.current % con.totallines) * con.linewidth+i] = (ColorIndex(COLOR_WHITE)<<8)|' ';
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void Con_Print( const char *txt )
{
	int	y, c, l, color;
	bool	skipnotify = false;
	int	prev;

	// client not running
	if( host.type == HOST_DEDICATED ) return;
          if( !con.initialized ) return;

	if(!com.strncmp( txt, "[skipnotify]", 12 ))
	{
		skipnotify = true;
		txt += 12;
	}
	
	color = ColorIndex( COLOR_WHITE );

	while((c = *txt) != 0 )
	{
		if(IsColorString( txt ))
		{
			color = ColorIndex(*(txt+1));
			txt += 2;
			continue;
		}

		// count word length
		for( l = 0; l < con.linewidth; l++ )
		{
			if( txt[l] <= ' ') break;
		}

		// word wrap
		if( l != con.linewidth && (con.x + l >= con.linewidth ))
			Con_Linefeed( skipnotify);
		txt++;

		switch(c)
		{
		case '\n':
			Con_Linefeed( skipnotify );
			break;
		case '\r':
			con.x = 0;
			break;
		default:	// display character and advance
			y = con.current % con.totallines;
			con.text[y*con.linewidth+con.x] = (color << 8) | c;
			con.x++;
			if( con.x >= con.linewidth )
			{
				Con_Linefeed( skipnotify );
				con.x = 0;
			}
			break;
		}
	}

	// mark time for transparent overlay
	if( con.current >= 0 )
	{
		if( skipnotify )
		{
			prev = con.current % CON_TIMES - 1;
			if( prev < 0 ) prev = CON_TIMES - 1;
			con.times[prev] = 0;
		}
		else con.times[con.current % CON_TIMES] = host.realtime;
	}
}

/*
=============================================================================

EDIT FIELDS

=============================================================================
*/
/*
===============
Cmd_CheckName

compare first argument with string
===============
*/
static bool Cmd_CheckName( const char *name )
{
	if( !com.stricmp( Cmd_Argv( 0 ), name ))
		return true;
	if( !com.stricmp( Cmd_Argv( 0 ), va( "\\%s", name )))
		return true;
	return false;
}

/*
===============
pfnFindMatches

===============
*/
static void pfnFindMatches( const char *s, const char *unused1, const char *unused2, void *unused3 )
{
	int		i;

	if( *s == '@' ) return; // never show system cvars or cmds
	if( com.strnicmp( s, con.completionString, com.strlen( con.completionString )))
		return;

	con.matchCount++;

	if( con.matchCount == 1 )
	{
		com.strncpy( con.shortestMatch, s, sizeof( con.shortestMatch ));
		return;
	}

	// cut shortestMatch to the amount common with s
	for( i = 0; s[i]; i++ )
	{
		if( com.tolower( con.shortestMatch[i] ) != com.tolower( s[i] ))
			con.shortestMatch[i] = 0;
	}
}

/*
===============
pfnPrintMatches
===============
*/
static void pfnPrintMatches( const char *s, const char *unused1, const char *m, void *unused2 )
{
	if( !com.strnicmp( s, con.shortestMatch, com.strlen( con.shortestMatch )))
	{
		if( m && *m ) Msg( "    %s ^3\"%s\"\n", s, m );
		else Msg( "    %s\n", s ); // variable or command without description
	}
}

static void ConcatRemaining( const char *src, const char *start )
{
	char	*arg;
	int	i;

	arg = com.strstr( src, start );

	if( !arg )
	{
		for( i = 1; i < Cmd_Argc(); i++ )
		{
			com.strncat( con.completionField->buffer, " ", sizeof( con.completionField->buffer ));
			arg = Cmd_Argv( i );
			while( *arg )
			{
				if( *arg == ' ' )
				{
					com.strncat( con.completionField->buffer, "\"", sizeof( con.completionField->buffer ));
					break;
				}
				arg++;
			}

			com.strncat( con.completionField->buffer, Cmd_Argv( i ), sizeof( con.completionField->buffer ));
			if( *arg == ' ' ) com.strncat( con.completionField->buffer, "\"", sizeof( con.completionField->buffer ));
		}
		return;
	}

	arg += com.strlen( start );
	com.strncat( con.completionField->buffer, arg, sizeof( con.completionField->buffer ));
}

/*
===============
Con_CompleteCommand

perform Tab expansion
===============
*/
void Con_CompleteCommand( field_t *field )
{
	field_t		temp;
	string		filename;
	autocomplete_list_t	*list;

	con.completionField = field;

	// only look at the first token for completion purposes
	Cmd_TokenizeString( con.completionField->buffer );

	con.completionString = Cmd_Argv( 0 );
	if( con.completionString[0] == '\\' || con.completionString[0] == '/' )
		con.completionString++;
	
	con.matchCount = 0;
	con.shortestMatch[0] = 0;

	if( !com.strlen( con.completionString ))
		return;

	Cmd_LookupCmds( NULL, NULL, pfnFindMatches );
	Cvar_LookupVars( 0, NULL, NULL, pfnFindMatches );

	if( con.matchCount == 0 ) return; // no matches
	Mem_Copy( &temp, con.completionField, sizeof( field_t ));

	if( Cmd_Argc() == 2 )
	{
		bool result = false;

		// autocomplete second arg
		for( list = cmd_list; list->name; list++ )
		{
			if( Cmd_CheckName( list->name ))
			{
				result = list->func( Cmd_Argv(1), filename, MAX_STRING ); 
				break;
			}
		}

		if( result )
		{         
			com.sprintf( con.completionField->buffer, "%s %s", Cmd_Argv( 0 ), filename ); 
			con.completionField->cursor = com.strlen( con.completionField->buffer );
			return;
		}
	}  

	if( con.matchCount == 1 )
	{
		com.sprintf( con.completionField->buffer, "\\%s", con.shortestMatch );
		if( Cmd_Argc() == 1 )
			com.strncat( con.completionField->buffer, " ", sizeof( con.completionField->buffer ));
		else ConcatRemaining( temp.buffer, con.completionString );
		con.completionField->cursor = com.strlen( con.completionField->buffer );
		return;
	}

	// multiple matches, complete to shortest
	com.sprintf( con.completionField->buffer, "\\%s", con.shortestMatch );
	con.completionField->cursor = com.strlen( con.completionField->buffer );
	ConcatRemaining( temp.buffer, con.completionString );

	Msg( "]%s\n", con.completionField->buffer );

	// run through again, printing matches
	Cmd_LookupCmds( NULL, NULL, pfnPrintMatches );
	Cvar_LookupVars( 0, NULL, NULL, pfnPrintMatches );
}

/*
===================
Field_Draw

Handles horizontal scrolling and cursor blinking
x, y, amd width are in pixels
===================
*/
void Field_VariableSizeDraw( field_t *edit, int x, int y, int width, int size, bool showCursor )
{
	int	len;
	int	drawLen;
	int	prestep;
	int	i, cursorChar;
	char	str[MAX_SYSPATH];

	drawLen = edit->widthInChars;
	len = com.strlen( edit->buffer ) + 1;

	// guarantee that cursor will be visible
	if ( len <= drawLen ) prestep = 0;
	else
	{
		if ( edit->scroll + drawLen > len )
		{
			edit->scroll = len - drawLen;
			if ( edit->scroll < 0 ) edit->scroll = 0;
		}
		prestep = edit->scroll;
	}

	if ( prestep + drawLen > len ) drawLen = len - prestep;

	// extract <drawLen> characters from the field at <prestep>
	if ( drawLen >= MAX_SYSPATH ) Host_Error("drawLen >= MAX_SYSPATH\n" );

	Mem_Copy( str, edit->buffer + prestep, drawLen );
	str[drawLen] = 0;

	// draw it
	if ( size == SMALLCHAR_WIDTH )
	{
		rgba_t	color = { 255, 255, 255, 255 };

		SCR_DrawSmallStringExt( x, y, str, color, false );
	}
	else SCR_DrawBigString( x, y, str, 255 ); // draw big string with drop shadow

	// draw the cursor
	if( !showCursor ) return;
	if((int)( host.realtime * 4 ) & 1 )
		return; // off blink

	if( host.key_overstrike ) cursorChar = 11;
	else cursorChar = 95;

	i = drawLen - (com.cstrlen(str) + 1);

	if( size == SMALLCHAR_WIDTH )
		SCR_DrawSmallChar( x + (edit->cursor - prestep - i) * size, y, cursorChar );
	else
	{
		str[0] = cursorChar;
		str[1] = 0;
		SCR_DrawBigString( x + ( edit->cursor - prestep - i ) * size, y, str, 255 );
	}
}

void Field_Draw( field_t *edit, int x, int y, int width, bool showCursor ) 
{
	Field_VariableSizeDraw( edit, x, y, width, SMALLCHAR_WIDTH, showCursor );
}

void Field_BigDraw( field_t *edit, int x, int y, int width, bool showCursor ) 
{
	Field_VariableSizeDraw( edit, x, y, width, BIGCHAR_WIDTH, showCursor );
}

/*
================
Field_Paste
================
*/
void Field_Paste( field_t *edit )
{
	char	*cbd;
	int	pasteLen, i;

	cbd = Sys_GetClipboardData();

	if ( !cbd ) return;

	// send as if typed, so insert / overstrike works properly
	pasteLen = com.strlen( cbd );
	for ( i = 0 ; i < pasteLen ; i++ ) Field_CharEvent( edit, cbd[i] );
	Mem_Free( cbd );
}

/*
=================
Field_KeyDownEvent

Performs the basic line editing functions for the console,
in-game talk, and menu fields

Key events are used for non-printable characters, others are gotten from char events.
=================
*/
void Field_KeyDownEvent( field_t *edit, int key )
{
	int		len;

	// shift-insert is paste
	if((( key == K_INS ) || ( key == K_KP_INS )) && Key_IsDown( K_SHIFT ))
	{
		Field_Paste( edit );
		return;
	}

	len = com.strlen( edit->buffer );

	if ( key == K_DEL )
	{
		if ( edit->cursor < len )
		{
			memmove( edit->buffer + edit->cursor, edit->buffer + edit->cursor + 1, len - edit->cursor );
		}
		return;
	}
	if ( key == K_BACKSPACE )
	{
		if ( edit->cursor > 0 )
		{
			memmove( edit->buffer + edit->cursor - 1, edit->buffer + edit->cursor, len - edit->cursor + 1 );
			edit->cursor--;
			if ( edit->scroll ) edit->scroll--;
		}
		return;
	}
	if ( key == K_RIGHTARROW ) 
	{
		if ( edit->cursor < len ) edit->cursor++;
		if ( edit->cursor >= edit->scroll + edit->widthInChars && edit->cursor <= len )
			edit->scroll++;
		return;
	}
	if ( key == K_LEFTARROW ) 
	{
		if ( edit->cursor > 0 ) edit->cursor--;
		if ( edit->cursor < edit->scroll ) edit->scroll--;
		return;
	}
	if ( key == K_HOME || ( com.tolower(key) == 'a' && Key_IsDown( K_CTRL )))
	{
		edit->cursor = 0;
		return;
	}
	if ( key == K_END || ( com.tolower(key) == 'e' && Key_IsDown( K_CTRL )))
	{
		edit->cursor = len;
		return;
	}
	if ( key == K_INS )
	{
		host.key_overstrike = !host.key_overstrike;
		return;
	}
}

/*
==================
Field_CharEvent
==================
*/
void Field_CharEvent( field_t *edit, int ch )
{
	int		len;

	if( ch == 'v' - 'a' + 1 )
	{
		// ctrl-v is paste
		Field_Paste( edit );
		return;
	}
	if( ch == 'c' - 'a' + 1 )
	{
		// ctrl-c clears the field
		Con_ClearField( edit );
		return;
	}

	len = com.strlen( edit->buffer );

	if( ch == 'a' - 'a' + 1 )
	{
		// ctrl-a is home
		edit->cursor = 0;
		edit->scroll = 0;
		return;
	}
	if( ch == 'e' - 'a' + 1 )
	{
		// ctrl-e is end
		edit->cursor = len;
		edit->scroll = edit->cursor - edit->widthInChars;
		return;
	}

	// ignore any other non printable chars
	if ( ch < 32 ) return;

	if ( host.key_overstrike )
	{	
		if ( edit->cursor == MAX_STRING - 1 ) return;
		edit->buffer[edit->cursor] = ch;
		edit->cursor++;
	}
	else
	{
		// insert mode
		if ( len == MAX_STRING - 1 ) return; // all full
		memmove( edit->buffer + edit->cursor + 1, edit->buffer + edit->cursor, len + 1 - edit->cursor );
		edit->buffer[edit->cursor] = ch;
		edit->cursor++;
	}

	if( edit->cursor >= edit->widthInChars ) edit->scroll++;
	if( edit->cursor == len + 1) edit->buffer[edit->cursor] = 0;
}

/*
=============================================================================

CONSOLE LINE EDITING

==============================================================================
*/

/*
====================
Key_Console

Handles history and console scrollback
====================
*/
void Key_Console( int key )
{
	// ctrl-L clears screen
	if( key == 'l' && Key_IsDown( K_CTRL ))
	{
		Cbuf_AddText( "clear\n" );
		return;
	}

	// enter finishes the line
	if ( key == K_ENTER || key == K_KP_ENTER )
	{
		// if not in the game explicitly prepent a slash if needed
		if( cls.state != ca_active && con.input.buffer[0] != '\\' && con.input.buffer[0] != '/' )
		{
			char	temp[MAX_SYSPATH];

			com.strncpy( temp, con.input.buffer, sizeof( temp ));
			com.sprintf( con.input.buffer, "\\%s", temp );
			con.input.cursor++;
		}

		// backslash text are commands, else chat
		if( con.input.buffer[0] == '\\' || con.input.buffer[0] == '/' )
			Cbuf_AddText( con.input.buffer + 1 ); // skip backslash
		else Cbuf_AddText( con.input.buffer ); // valid command
		Cbuf_AddText( "\n" );

		// echo to console
		Msg( ">%s\n", con.input.buffer );

		// copy line to history buffer
		con.historyLines[con.nextHistoryLine % CON_HISTORY] = con.input;
		con.nextHistoryLine++;
		con.historyLine = con.nextHistoryLine;

		Con_ClearField( &con.input );
		con.input.widthInChars = g_console_field_width;

		if( cls.state == ca_disconnected )
		{
			// force an update, because the command may take some time
			SCR_UpdateScreen ();
		}
		return;
	}

	// command completion
	if( key == K_TAB )
	{
		Con_CompleteCommand( &con.input );
		return;
	}

	// command history (ctrl-p ctrl-n for unix style)
	if(( key == K_MWHEELUP && Key_IsDown( K_SHIFT )) || ( key == K_UPARROW ) || ( key == K_KP_UPARROW ) || (( com.tolower(key) == 'p' ) && Key_IsDown( K_CTRL )))
	{
		if( con.nextHistoryLine - con.historyLine < CON_HISTORY && con.historyLine > 0 )
		{
			con.historyLine--;
		}
		con.input = con.historyLines[con.historyLine % CON_HISTORY];
		return;
	}

	if(( key == K_MWHEELDOWN && Key_IsDown( K_SHIFT )) || ( key == K_DOWNARROW ) || ( key == K_KP_DOWNARROW ) || (( com.tolower(key) == 'n' ) && Key_IsDown( K_CTRL )))
	{
		if( con.historyLine == con.nextHistoryLine ) return;
		con.historyLine++;
		con.input = con.historyLines[con.historyLine % CON_HISTORY];
		return;
	}

	// console scrolling
	if( key == K_PGUP )
	{
		Con_PageUp();
		return;
	}

	if( key == K_PGDN )
	{
		Con_PageDown();
		return;
	}

	if( key == K_MWHEELUP )
	{
		Con_PageUp();
		if( Key_IsDown( K_CTRL ))
		{
			Con_PageUp();
			Con_PageUp();
		}
		return;
	}

	if( key == K_MWHEELDOWN )
	{	
		Con_PageDown();
		if( Key_IsDown( K_CTRL ))
		{	
			Con_PageDown();
			Con_PageDown();
		}
		return;
	}

	// ctrl-home = top of console
	if( key == K_HOME && Key_IsDown( K_CTRL ))
	{
		Con_Top();
		return;
	}

	// ctrl-end = bottom of console
	if( key == K_END && Key_IsDown( K_CTRL ))
	{
		Con_Bottom();
		return;
	}

	// pass to the normal editline routine
	Field_KeyDownEvent( &con.input, key );
}

/*
==============================================================================

DRAWING

==============================================================================
*/
/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
void Con_DrawInput( void )
{
	int	y;

	if( cls.key_dest != key_console )
		return; // don't draw anything (always draw if not active)

	y = con.vislines - ( SMALLCHAR_HEIGHT * 2 );
	re->SetColor( con.color );
	Field_Draw( &con.input, con.xadjust + 2 * SMALLCHAR_WIDTH, y, SCREEN_WIDTH - 3 * SMALLCHAR_WIDTH, true );
	SCR_DrawSmallChar( con.xadjust + 1 * SMALLCHAR_WIDTH, y, '>' );
}

/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify( void )
{
	int	i, x, v = 0;
	int	currentColor;
	short	*text;
	float	time;

	if( !host.developer ) return;

	currentColor = 7;
	re->SetColor( g_color_table[currentColor] );

	for( i = con.current - CON_TIMES + 1; i <= con.current; i++ )
	{
		if( i < 0 ) continue;
		time = con.times[i % CON_TIMES];
		if( time == 0.0 ) continue;
		time = host.realtime - time;
		if( time > con_notifytime->value ) continue;
		text = con.text + (i % con.totallines) * con.linewidth;

		for( x = 0; x < con.linewidth; x++ )
		{
			if((text[x] & 0xff ) == ' ' ) continue;
			if(( ( text[x] >> 8 ) & 7 ) != currentColor )
			{
				currentColor = ( text[x] >> 8 ) & 7;
				re->SetColor(g_color_table[currentColor]);
			}
			SCR_DrawSmallChar( con.xadjust + (x+1) * SMALLCHAR_WIDTH, v, text[x] & 0xff );
		}
		v += SMALLCHAR_HEIGHT;
	}
	re->SetColor( NULL );
}

/*
================
Con_DrawConsole

Draws the console with the solid background
================
*/
void Con_DrawSolidConsole( float frac )
{
	int	i, x, y;
	int	rows;
	short	*text;
	int	row;
	int	lines;
	int	currentColor;
	string	curbuild;

	lines = scr_height->integer * frac;
	if( lines <= 0 ) return;
	if( lines > scr_height->integer ) lines = scr_height->integer;

	con.xadjust = 0; // on wide screens, we will center the text
	SCR_AdjustSize( &con.xadjust, NULL, NULL, NULL );

	// draw the background
	y = frac * SCREEN_HEIGHT;
	if( y < 1 ) y = 0;
	else SCR_DrawPic( 0, y - SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT, cls.consoleBack );

	if( host.developer )
	{
		// draw current version
		re->SetColor( g_color_table[ColorIndex(COLOR_YELLOW)] );
		com.snprintf( curbuild, MAX_STRING, "Xash3D %g (build %i)", SI->version, com_buildnum( ));
		i = com.strlen( curbuild );
		for( x = 0; x < i; x++ )
			SCR_DrawSmallChar( scr_width->integer - ( i - x ) * SMALLCHAR_WIDTH, (lines - (SMALLCHAR_HEIGHT+SMALLCHAR_HEIGHT/2)), curbuild[x] );
		re->SetColor( NULL );
	}

	// draw the text
	con.vislines = lines;
	rows = ( lines - SMALLCHAR_WIDTH ) / SMALLCHAR_WIDTH; // rows of text to draw
	y = lines - ( SMALLCHAR_HEIGHT * 3 );

	// draw from the bottom up
	if( con.display != con.current )
	{
		// draw arrows to show the buffer is backscrolled
		re->SetColor( g_color_table[ColorIndex(COLOR_RED)] );
		for( x = 0; x < con.linewidth; x += 4 )
			SCR_DrawSmallChar( con.xadjust + (x+1) * SMALLCHAR_WIDTH, y, '^' );
		y -= SMALLCHAR_HEIGHT;
		rows--;
	}
	
	row = con.display;
	if( con.x == 0 ) row--;

	currentColor = 7;
	re->SetColor( g_color_table[currentColor] );

	for( i = 0; i < rows; i++, y -= SMALLCHAR_HEIGHT, row-- )
	{
		if( row < 0 ) break;
		if( con.current - row >= con.totallines )
		{
			// past scrollback wrap point
			continue;	
		}

		text = con.text + (row % con.totallines) * con.linewidth;

		for( x = 0; x < con.linewidth; x++ )
		{
			if(( text[x] & 0xff ) == ' ' ) continue;
			if((( text[x]>>8) & 7 ) != currentColor )
			{
				currentColor = (text[x]>>8) & 7;
				re->SetColor(g_color_table[currentColor]);
			}
			SCR_DrawSmallChar( con.xadjust + (x+1) * SMALLCHAR_WIDTH, y, text[x] & 0xff );
		}
	}

	// draw the input prompt, user text, and cursor if desired
	Con_DrawInput();
	re->SetColor( NULL );
}

/*
==================
Con_DrawConsole
==================
*/
void Con_DrawConsole( void )
{
	// never draw console whel changelevel in-progress
	if( cls.changelevel ) return;

	// check for console width changes from a vid mode change
	Con_CheckResize ();

	if( cls.state == ca_connecting || cls.state == ca_connected )
	{
		if( !cl_allow_levelshots->integer )
		{
			con.displayFrac = con.finalFrac = 1.0f;
		}
		else
		{
			if( host.developer >= 4 )
			{
				con.displayFrac = 0.5f;	// keep console open
			}
			else
			{
				con.finalFrac = 0.0f;
				Con_RunConsole();

				if( host.developer >= 2 )
					Con_DrawNotify(); // draw notify lines
			}
		}
	}

	// if disconnected, render console full screen
	switch( cls.state )
	{
	case ca_uninitialized:
		break;
	case ca_disconnected:
		if( cls.key_dest != key_menu && host.developer )
		{
			Con_DrawSolidConsole( 1.0f );
			cls.key_dest = key_console;
		}
		break;
	case ca_connected:
	case ca_connecting:
		// force to show console always for -dev 3 and higher 
		if( con.displayFrac ) Con_DrawSolidConsole( con.displayFrac );
		break;
	case ca_active:
	case ca_cinematic: 
		if( con.displayFrac ) Con_DrawSolidConsole( con.displayFrac );
		else if( cls.state == ca_active && cls.key_dest == key_game )
			Con_DrawNotify(); // draw notify lines
		break;
	}
}

/*
==================
Con_RunConsole

Scroll it up or down
==================
*/
void Con_RunConsole( void )
{
	// decide on the destination height of the console
	if( host.developer && cls.key_dest == key_console )
	{
		if( cls.state == ca_disconnected )
			con.finalFrac = 1.0f;// full screen
		else con.finalFrac = 0.5f;	// half screen	
	}
	else con.finalFrac = 0; // none visible

	// whel level is loading frametime may be is wrong
	if( cls.state == ca_connecting || cls.state == ca_connected )
		host.realframetime = ( MAX_FPS / host_maxfps->value ) * MIN_FRAMETIME;

	if( con.finalFrac < con.displayFrac )
	{
		con.displayFrac -= con_speed->value * 0.002 * host.realframetime;
		if( con.finalFrac > con.displayFrac )
			con.displayFrac = con.finalFrac;
	}
	else if( con.finalFrac > con.displayFrac )
	{
		con.displayFrac += con_speed->value * 0.002 * host.realframetime;
		if( con.finalFrac < con.displayFrac )
			con.displayFrac = con.finalFrac;
	}
}

/*
==============================================================================

CONSOLE INTERFACE

==============================================================================
*/
void Con_CharEvent( int key )
{
	Field_CharEvent( &con.input, key );
}

void Con_VidInit( void )
{
	g_console_field_width = scr_width->integer / SMALLCHAR_WIDTH - 2;
	con.input.widthInChars = g_console_field_width;
}

/*
=========
Cmd_AutoComplete

NOTE: input string must be equal or longer than MAX_STRING
=========
*/
void Cmd_AutoComplete( char *complete_string )
{
	field_t	input;

	if( !complete_string || !*complete_string )
		return;

	// setup input
	com.strncpy( input.buffer, complete_string, sizeof( input.buffer ));
	input.cursor = input.scroll = 0;

	Con_CompleteCommand( &input );

	// setup output
	if( input.buffer[0] == '\\' || input.buffer[0] == '/' )
		com.strncpy( complete_string, input.buffer + 1, sizeof( input.buffer ));
	else com.strncpy( complete_string, input.buffer, sizeof( input.buffer ));
}

void Con_PageUp( void )
{
	con.display -= 2;
	if( con.current - con.display >= con.totallines )
		con.display = con.current - con.totallines + 1;
}

void Con_PageDown( void )
{
	con.display += 2;
	if( con.display > con.current) con.display = con.current;
}

void Con_Top( void )
{
	con.display = con.totallines;
	if( con.current - con.display >= con.totallines )
		con.display = con.current - con.totallines + 1;
}

void Con_Bottom( void )
{
	con.display = con.current;
}

void Con_Close( void )
{
	Con_ClearField( &con.input );
	Con_ClearNotify();
	con.finalFrac = 0; // none visible
	con.displayFrac = 0;
}

bool Con_Visible( void )
{
	return (con.finalFrac != 0.0f);
}