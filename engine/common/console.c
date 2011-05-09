/*
console.c - developer console
Copyright (C) 2007 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"
#include "client.h"
#include "keydefs.h"
#include "protocol.h"		// get the protocol version
#include "con_nprint.h"
#include "gl_local.h"
#include "qfont.h"

convar_t	*con_notifytime;
convar_t	*scr_conspeed;
convar_t	*con_fontsize;

#define CON_TIMES		5	// need for 4 lines
#define COLOR_DEFAULT	'7'
#define CON_HISTORY		64
#define MAX_DBG_NOTIFY	128

#define CON_TEXTSIZE	131072	// 128 kb buffer

// console color typeing
rgba_t g_color_table[8] =
{
{  0,   0,   0, 255},	// black
{255,   0,   0, 255},	// red
{  0, 255,   0, 255},	// green
{255, 255,   0, 255},	// yellow
{  0,   0, 255, 255},	// blue
{  0, 255, 255, 255},	// cyan
{255,   0, 255, 255},	// magenta
{240, 180,  24, 255},	// default color (can be changed by user)
};

typedef struct
{
	string		buffer;
	int		cursor;
	int		scroll;
	int		widthInChars;
} field_t;

typedef struct
{
	string		szNotify;
	float		expire;
	rgba_t		color;
	int		key_dest;
} notify_t;

typedef struct
{
	qboolean		initialized;

	short		text[CON_TEXTSIZE];
	int		current;		// line where next message will be printed
	int		display;		// bottom of console displays this line
	int		x;		// offset in current line for next print

	int 		linewidth;	// characters across screen
	int		totallines;	// total lines in console scrollback

	float		displayFrac;	// aproaches finalFrac at scr_conspeed
	float		finalFrac;	// 0.0 to 1.0 lines of console to display

	int		vislines;		// in scanlines
	double		times[CON_TIMES];	// host.realtime the line was generated for transparent notify lines
	rgba_t		color;

	// console images
	int		background;	// console background

	// conchars
	cl_font_t		chars;		// fonts.wad/font1.fnt
	int		charHeight;
	byte		charWidths[256];
	
	// console input
	field_t		input;

	// console history
	field_t		historyLines[CON_HISTORY];
	int		historyLine;	// the line being displayed from history buffer will be <= nextHistoryLine
	int		nextHistoryLine;	// the last line in the history buffer, not masked

	notify_t		notify[MAX_DBG_NOTIFY]; // for Con_NXPrintf
	qboolean		draw_notify;	// true if we have NXPrint message

	// console auto-complete
	field_t		*completionField;
	char		*completionString;
	string		shortestMatch;
	int		matchCount;
} console_t;

static console_t		con;

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
		con.text[i] = ( ColorIndex( COLOR_DEFAULT ) << 8 ) | ' ';
	con.display = con.current; // go to end
}

/*
================
Con_SetColor_f
================
*/
void Con_SetColor_f( void )
{
	vec3_t	color;

	switch( Cmd_Argc() )
	{
	case 1:
		Msg( "\"con_color\" is %i %i %i\n", g_color_table[7][0], g_color_table[7][1], g_color_table[7][2] );
		break;
	case 2:
		VectorSet( color, g_color_table[7][0], g_color_table[7][1], g_color_table[7][2] );
		Q_atov( color, Cmd_Argv( 1 ), 3 );
		Con_DefaultColor( color[0], color[1], color[2] );
		break;
	default:
		Msg( "Usage: con_color \"r g b\"\n" );
		break;
	}
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
		con.times[i] = 0;
}

/*
================
Con_ClearField
================
*/
void Con_ClearField( field_t *edit )
{
	Q_memset( edit->buffer, 0, MAX_STRING );
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
	con.input.widthInChars = con.linewidth;
}

/*
============
Con_StringLength

skipped color prefixes
============
*/
int Con_StringLength( const char *string )
{
	int		len;
	const char	*p;

	if( !string ) return 0;

	len = 0;
	p = string;
	while( *p )
	{
		if( IsColorString( p ))
		{
			p += 2;
			continue;
		}
		p++;
		len++;
	}
	return len;
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f( void )
{
	if( !host.developer ) return;	// disabled

	if( UI_CreditsActive( )) return; // diabled by final credits

	// show console only in game or by special call from menu
	if( cls.state != ca_active || cls.key_dest == key_menu )
		return;

	Con_ClearTyping();
	Con_ClearNotify();

	if( cls.key_dest == key_console )
	{
		if( Cvar_VariableInteger( "sv_background" ))
			UI_SetActiveMenu( true );
		else UI_SetActiveMenu( false );
	}
	else
	{
		UI_SetActiveMenu( false );
		Key_SetKeyDest( key_console );
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
	int	i, j, width, numlines, numchars;
	int	oldwidth, oldtotallines;
	short	tbuf[CON_TEXTSIZE];
	int	charWidth = 8;

	if( con.chars.hFontTexture )
		charWidth = con.charWidths['M'];

	width = ( scr_width->integer / charWidth ) - 2;

	// FIXME: Con_CheckResize is totally wrong :-(
	// g-cont. i've just used fixed width on all resolutions
	width = 90;

	if( width == con.linewidth )
		return;

	if( !glw_state.initialized )
	{
		// video hasn't been initialized yet
		con.linewidth = width;
		con.totallines = CON_TEXTSIZE / con.linewidth;

		for( i = 0; i < CON_TEXTSIZE; i++ )
			con.text[i] = ( ColorIndex( COLOR_DEFAULT ) << 8 ) | ' ';
	}
	else
	{
		oldwidth = con.linewidth;
		con.linewidth = width;
		oldtotallines = con.totallines;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		numlines = oldtotallines;

		if( con.totallines < numlines )
			numlines = con.totallines;

		numchars = oldwidth;
	
		if( con.linewidth < numchars )
			numchars = con.linewidth;

		Q_memcpy( tbuf, con.text, CON_TEXTSIZE * sizeof( short ));

		for( i = 0; i < CON_TEXTSIZE; i++ )
			con.text[i] = ( ColorIndex( COLOR_DEFAULT ) << 8 ) | ' ';

		// FIXME: should we consider '\n' when counting the actual lines?
		for( i = 0; i < numlines; i++ )
		{
			for( j = 0; j < numchars; j++ )
			{
				con.text[(con.totallines - 1 - i) * con.linewidth + j] =
					tbuf[((con.current - i + oldtotallines) % oldtotallines) * oldwidth + j + con.x];
			}
		}
		Con_ClearNotify ();
	}

	con.current = con.totallines - 1;
	con.display = con.current;

	con.input.widthInChars = con.linewidth;

	for( i = 0; i < CON_HISTORY; i++ )
		con.historyLines[i].widthInChars = con.linewidth;
}

/*
================
Con_PageUp
================
*/
void Con_PageUp( void )
{
	con.display -= 2;

	if( con.current - con.display >= con.totallines )
		con.display = con.current - con.totallines + 1;
}

/*
================
Con_PageDown
================
*/
void Con_PageDown( void )
{
	con.display += 2;

	if( con.display > con.current )
		con.display = con.current;
}

/*
================
Con_Top
================
*/
void Con_Top( void )
{
	con.display = con.totallines;

	if( con.current - con.display >= con.totallines )
		con.display = con.current - con.totallines + 1;
}

/*
================
Con_Bottom
================
*/
void Con_Bottom( void )
{
	con.display = con.current;
}

/*
================
Con_Visible
================
*/
qboolean Con_Visible( void )
{
	return (con.displayFrac != 0.0f);
}

/*
================
Con_LoadConchars
================
*/
static void Con_LoadConchars( void )
{
	int	fontWidth, fontHeight;

	// make sure fontnum in-range
	if( con_fontsize->integer < 0 ) Cvar_SetFloat( "con_fontsize", 0 );
	if( con_fontsize->integer > 2 ) Cvar_SetFloat( "con_fontsize", 2 );

	// select properly fontsize
	if( scr_width->integer < 640 ) Cvar_SetFloat( "con_fontsize", 0 );
	else if( scr_width->integer >= 1280 ) Cvar_SetFloat( "con_fontsize", 2 );
	else Cvar_SetFloat( "con_fontsize", 1 );

	// loading conchars
	con.chars.hFontTexture = GL_LoadTexture( va( "fonts/font%i", con_fontsize->integer ), NULL, 0, TF_FONT );

	if( !con_fontsize->modified ) return; // font not changed

	R_GetTextureParms( &fontWidth, &fontHeight, con.chars.hFontTexture );
		
	// setup creditsfont
	if( FS_FileExists( va( "fonts/font%i.fnt", con_fontsize->integer ), false ))
	{
		byte	*buffer;
		size_t	length;
		qfont_t	*src;
	
		// half-life font with variable chars witdh
		buffer = FS_LoadFile( va( "fonts/font%i", con_fontsize->integer ), &length, false );

		if( buffer && length >= sizeof( qfont_t ))
		{
			int	i;
	
			src = (qfont_t *)buffer;
			con.charHeight = src->rowheight;

			// build rectangles
			for( i = 0; i < 256; i++ )
			{
				con.chars.fontRc[i].left = (word)src->fontinfo[i].startoffset % fontWidth;
				con.chars.fontRc[i].right = con.chars.fontRc[i].left + src->fontinfo[i].charwidth;
				con.chars.fontRc[i].top = (word)src->fontinfo[i].startoffset / fontWidth;
				con.chars.fontRc[i].bottom = con.chars.fontRc[i].top + src->rowheight;
				con.charWidths[i] = src->fontinfo[i].charwidth;
			}
			con.chars.valid = true;
		}
		if( buffer ) Mem_Free( buffer );
	}
}

static int Con_DrawGenericChar( int x, int y, int number, rgba_t color )
{
	int	width, height;
	float	s1, t1, s2, t2;
	wrect_t	*rc;

	number &= 255;

	if( !con.chars.valid )
		return 0;

	if( number < 32 ) return 0;
	if( y < -con.charHeight )
		return 0;

	rc = &con.chars.fontRc[number];

	pglColor4ubv( color );
	R_GetTextureParms( &width, &height, con.chars.hFontTexture );

	// calc rectangle
	s1 = (float)rc->left / width;
	t1 = (float)rc->top / height;
	s2 = (float)rc->right / width;
	t2 = (float)rc->bottom / height;
	width = rc->right - rc->left;
	height = rc->bottom - rc->top;

	R_DrawStretchPic( x, y, width, height, s1, t1, s2, t2, con.chars.hFontTexture );		
	pglColor4ub( 255, 255, 255, 255 ); // don't forget reset color

	return con.charWidths[number];
}

static int Con_DrawCharacter( int x, int y, int number, rgba_t color )
{
	GL_SetRenderMode( kRenderTransTexture );
	return Con_DrawGenericChar( x, y, number, color );
}

void Con_DrawStringLen( const char *pText, int *length, int *height )
{
	int	curLength = 0;

	if( height ) *height = con.charHeight;
	if( !length ) return;

	*length = 0;

	while( *pText )
	{
		byte	c = *pText;

		if( *pText == '\n' )
		{
			pText++;
			curLength = 0;
		}

		// skip color strings they are not drawing
		if( IsColorString( pText ))
		{
			pText += 2;
			continue;
		}

		curLength += con.charWidths[c];
		pText++;

		if( curLength > *length )
			*length = curLength;
	}
}

/*
==================
Con_DrawString

Draws a multi-colored string, optionally forcing
to a fixed color.
==================
*/
int Con_DrawGenericString( int x, int y, const char *string, rgba_t setColor, qboolean forceColor, int hideChar )
{
	rgba_t		color;
	int		drawLen = 0;
	int		numDraws = 0;
	const char	*s;

	// draw the colored text
	s = string;
	*(uint *)color = *(uint *)setColor;

	while ( *s )
	{
		if( *s == '\n' )
		{
			s++;
			drawLen = 0; // begin new row
			y += con.charHeight;
		}

		if( IsColorString( s ))
		{
			if( !forceColor )
			{
				Q_memcpy( color, g_color_table[ColorIndex(*(s+1))], sizeof( color ));
				color[3] = setColor[3];
			}

			s += 2;
			numDraws++;
			continue;
		}

		// hide char for overstrike mode
		if( hideChar == numDraws )
			drawLen += con.charWidths[*s];
		else drawLen += Con_DrawCharacter( x + drawLen, y, *s, color );

		numDraws++;
		s++;
	}
          
	pglColor4ub( 255, 255, 255, 255 );
	return drawLen;
}

int Con_DrawString( int x, int y, const char *string, rgba_t setColor )
{
	return Con_DrawGenericString( x, y, string, setColor, false, -1 );
}

/*
================
Con_Init
================
*/
void Con_Init( void )
{
	int	i;

	// must be init before startup video subsystem
	scr_width = Cvar_Get( "width", "640", 0, "screen width" );
	scr_height = Cvar_Get( "height", "480", 0, "screen height" );
	scr_conspeed = Cvar_Get( "scr_conspeed", "600", 0, "console moving speed" );
	con_notifytime = Cvar_Get( "con_notifytime", "3", 0, "notify time to live" );
	con_fontsize = Cvar_Get( "con_fontsize", "1", CVAR_ARCHIVE|CVAR_LATCH, "console font number (0, 1 or 2)" );

	Con_CheckResize();

	Con_ClearField( &con.input );
	con.input.widthInChars = con.linewidth;

	for( i = 0; i < CON_HISTORY; i++ )
	{
		Con_ClearField( &con.historyLines[i] );
		con.historyLines[i].widthInChars = con.linewidth;
	}

	Cmd_AddCommand( "toggleconsole", Con_ToggleConsole_f, "opens or closes the console" );
	Cmd_AddCommand( "con_color", Con_SetColor_f, "set a custom console color" );
	Cmd_AddCommand( "clear", Con_Clear_f, "clear console history" );

	MsgDev( D_NOTE, "Console initialized.\n" );
	con.initialized = true;
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed( void )
{
	int	i;

	// mark time for transparent overlay
	if( con.current >= 0 )
		con.times[con.current % CON_TIMES] = host.realtime;

	con.x = 0;
	if( con.display == con.current )
		con.display++;

	con.current++;

	for( i = 0; i < con.linewidth; i++ )
		con.text[(con.current % con.totallines) * con.linewidth+i] = ( ColorIndex( COLOR_DEFAULT ) << 8 ) | ' ';
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

	// client not running
	if( host.type == HOST_DEDICATED ) return;
          if( !con.initialized ) return;
	
	color = ColorIndex( COLOR_DEFAULT );

	while(( c = *txt ) != 0 )
	{
		if( IsColorString( txt ))
		{
			color = ColorIndex( *( txt + 1 ));
			txt += 2;
			continue;
		}

		// count word length
		for( l = 0; l < con.linewidth; l++ )
		{
			if( txt[l] <= ' ')
				break;
		}

		// word wrap
		if( l != con.linewidth && ( con.x + l >= con.linewidth ))
			Con_Linefeed();
		txt++;

		switch( c )
		{
		case '\n':
			Con_Linefeed();
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
				Con_Linefeed();
				con.x = 0;
			}
			break;
		}
	}
}

void Con_NPrintf( int idx, char *fmt, ... )
{
	va_list	args;

	if( idx < 0 || idx >= MAX_DBG_NOTIFY )
		return;

	Q_memset( con.notify[idx].szNotify, 0, MAX_STRING );

	va_start( args, fmt );
	Q_vsnprintf( con.notify[idx].szNotify, MAX_STRING, fmt, args );
	va_end( args );

	// reset values
	con.notify[idx].key_dest = key_game;
	con.notify[idx].expire = host.realtime + 4.0f;
	MakeRGBA( con.notify[idx].color, 255, 255, 255, 255 );
	con.draw_notify = true;
}

void Con_NXPrintf( con_nprint_t *info, char *fmt, ... )
{
	va_list	args;

	if( !info ) return;

	if( info->index < 0 || info->index >= MAX_DBG_NOTIFY )
		return;

	Q_memset( con.notify[info->index].szNotify, 0, MAX_STRING );

	va_start( args, fmt );
	Q_vsnprintf( con.notify[info->index].szNotify, MAX_STRING, fmt, args );
	va_end( args );

	// setup values
	con.notify[info->index].key_dest = key_game;
	con.notify[info->index].expire = host.realtime + info->time_to_live;
	MakeRGBA( con.notify[info->index].color, (byte)(info->color[0] * 255), (byte)(info->color[1] * 255), (byte)(info->color[2] * 255), 255 );
	con.draw_notify = true;
}

void UI_NPrintf( int idx, char *fmt, ... )
{
	va_list	args;

	if( idx < 0 || idx >= MAX_DBG_NOTIFY )
		return;

	Q_memset( con.notify[idx].szNotify, 0, MAX_STRING );

	va_start( args, fmt );
	Q_vsnprintf( con.notify[idx].szNotify, MAX_STRING, fmt, args );
	va_end( args );

	// reset values
	con.notify[idx].key_dest = key_menu;
	con.notify[idx].expire = host.realtime + 4.0f;
	MakeRGBA( con.notify[idx].color, 255, 255, 255, 255 );
	con.draw_notify = true;
}

void UI_NXPrintf( con_nprint_t *info, char *fmt, ... )
{
	va_list	args;

	if( !info ) return;

	if( info->index < 0 || info->index >= MAX_DBG_NOTIFY )
		return;

	Q_memset( con.notify[info->index].szNotify, 0, MAX_STRING );

	va_start( args, fmt );
	Q_vsnprintf( con.notify[info->index].szNotify, MAX_STRING, fmt, args );
	va_end( args );

	// setup values
	con.notify[info->index].key_dest = key_menu;
	con.notify[info->index].expire = host.realtime + info->time_to_live;
	MakeRGBA( con.notify[info->index].color, (byte)(info->color[0] * 255), (byte)(info->color[1] * 255), (byte)(info->color[2] * 255), 255 );
	con.draw_notify = true;
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
static qboolean Cmd_CheckName( const char *name )
{
	if( !Q_stricmp( Cmd_Argv( 0 ), name ))
		return true;
	if( !Q_stricmp( Cmd_Argv( 0 ), va( "\\%s", name )))
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
	if( Q_strnicmp( s, con.completionString, Q_strlen( con.completionString )))
		return;

	con.matchCount++;

	if( con.matchCount == 1 )
	{
		Q_strncpy( con.shortestMatch, s, sizeof( con.shortestMatch ));
		return;
	}

	// cut shortestMatch to the amount common with s
	for( i = 0; s[i]; i++ )
	{
		if( Q_tolower( con.shortestMatch[i] ) != Q_tolower( s[i] ))
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
	if( !Q_strnicmp( s, con.shortestMatch, Q_strlen( con.shortestMatch )))
	{
		if( m && *m ) Msg( "    %s ^3\"%s\"\n", s, m );
		else Msg( "    %s\n", s ); // variable or command without description
	}
}

static void ConcatRemaining( const char *src, const char *start )
{
	char	*arg;
	int	i;

	arg = Q_strstr( src, start );

	if( !arg )
	{
		for( i = 1; i < Cmd_Argc(); i++ )
		{
			Q_strncat( con.completionField->buffer, " ", sizeof( con.completionField->buffer ));
			arg = Cmd_Argv( i );
			while( *arg )
			{
				if( *arg == ' ' )
				{
					Q_strncat( con.completionField->buffer, "\"", sizeof( con.completionField->buffer ));
					break;
				}
				arg++;
			}

			Q_strncat( con.completionField->buffer, Cmd_Argv( i ), sizeof( con.completionField->buffer ));
			if( *arg == ' ' ) Q_strncat( con.completionField->buffer, "\"", sizeof( con.completionField->buffer ));
		}
		return;
	}

	arg += Q_strlen( start );
	Q_strncat( con.completionField->buffer, arg, sizeof( con.completionField->buffer ));
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

	if( !Q_strlen( con.completionString ))
		return;

	Cmd_LookupCmds( NULL, NULL, pfnFindMatches );
	Cvar_LookupVars( 0, NULL, NULL, pfnFindMatches );

	if( con.matchCount == 0 ) return; // no matches
	Q_memcpy( &temp, con.completionField, sizeof( field_t ));

	if( Cmd_Argc() == 2 )
	{
		qboolean result = false;

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
			Q_sprintf( con.completionField->buffer, "%s %s", Cmd_Argv( 0 ), filename ); 
			con.completionField->cursor = Q_strlen( con.completionField->buffer );
			return;
		}
	}  

	if( con.matchCount == 1 )
	{
		Q_sprintf( con.completionField->buffer, "\\%s", con.shortestMatch );
		if( Cmd_Argc() == 1 )
			Q_strncat( con.completionField->buffer, " ", sizeof( con.completionField->buffer ));
		else ConcatRemaining( temp.buffer, con.completionString );
		con.completionField->cursor = Q_strlen( con.completionField->buffer );
		return;
	}

	// multiple matches, complete to shortest
	Q_sprintf( con.completionField->buffer, "\\%s", con.shortestMatch );
	con.completionField->cursor = Q_strlen( con.completionField->buffer );
	ConcatRemaining( temp.buffer, con.completionString );

	Msg( "]%s\n", con.completionField->buffer );

	// run through again, printing matches
	Cmd_LookupCmds( NULL, NULL, pfnPrintMatches );
	Cvar_LookupVars( 0, NULL, NULL, pfnPrintMatches );
}

/*
================
Field_Paste
================
*/
void Field_Paste( field_t *edit )
{
	char	*cbd;
	int	i, pasteLen;

	cbd = Sys_GetClipboardData();
	if( !cbd ) return;

	// send as if typed, so insert / overstrike works properly
	pasteLen = Q_strlen( cbd );
	for( i = 0; i < pasteLen; i++ )
		Field_CharEvent( edit, cbd[i] );
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

	len = Q_strlen( edit->buffer );

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
	if ( key == K_HOME || ( Q_tolower(key) == 'a' && Key_IsDown( K_CTRL )))
	{
		edit->cursor = 0;
		return;
	}
	if ( key == K_END || ( Q_tolower(key) == 'e' && Key_IsDown( K_CTRL )))
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

	len = Q_strlen( edit->buffer );

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

			Q_strncpy( temp, con.input.buffer, sizeof( temp ));
			Q_sprintf( con.input.buffer, "\\%s", temp );
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
		con.input.widthInChars = con.linewidth;

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
	if(( key == K_MWHEELUP && Key_IsDown( K_SHIFT )) || ( key == K_UPARROW ) || (( Q_tolower(key) == 'p' ) && Key_IsDown( K_CTRL )))
	{
		if( con.nextHistoryLine - con.historyLine < CON_HISTORY && con.historyLine > 0 )
		{
			con.historyLine--;
		}
		con.input = con.historyLines[con.historyLine % CON_HISTORY];
		return;
	}

	if(( key == K_MWHEELDOWN && Key_IsDown( K_SHIFT )) || ( key == K_DOWNARROW ) || (( Q_tolower(key) == 'n' ) && Key_IsDown( K_CTRL )))
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
	int	len;
	int	drawLen, hideChar = -1;
	int	prestep, curPos;
	int	x, y, cursorChar;
	char	str[MAX_SYSPATH];
	byte	*colorDefault;

	// don't draw anything (always draw if not active)
	if( cls.key_dest != key_console ) return;

	x = QCHAR_WIDTH; // room for ']'
	y = con.vislines - ( con.charHeight * 2 );
	drawLen = con.input.widthInChars;
	len = Q_strlen( con.input.buffer ) + 1;
	colorDefault = g_color_table[ColorIndex( COLOR_DEFAULT )];

	// guarantee that cursor will be visible
	if( len <= drawLen )
	{
		prestep = 0;
	}
	else
	{
		if( con.input.scroll + drawLen > len )
		{
			con.input.scroll = len - drawLen;
			if( con.input.scroll < 0 )
				con.input.scroll = 0;
		}
		prestep = con.input.scroll;
	}

	if( prestep + drawLen > len )
		drawLen = len - prestep;

	// extract <drawLen> characters from the field at <prestep>
	ASSERT( drawLen < MAX_SYSPATH );

	Q_memcpy( str, con.input.buffer + prestep, drawLen );
	str[drawLen] = 0;

	// save char for overstrike
	cursorChar = str[con.input.cursor - prestep];

	if( host.key_overstrike && cursorChar && !((int)( host.realtime * 4 ) & 1 ))
		hideChar = con.input.cursor - prestep;	// skip this char
	
	// draw it
	Con_DrawGenericString( x, y, str, colorDefault, false, hideChar );
	Con_DrawCharacter( QCHAR_WIDTH >> 1, y, ']', colorDefault );

	// draw the cursor
	if((int)( host.realtime * 4 ) & 1 ) return; // off blink

	// calc cursor position
	str[con.input.cursor - prestep] = 0;
	Con_DrawStringLen( str, &curPos, NULL );

	if( host.key_overstrike && cursorChar )
	{
		// overstrike cursor
		pglEnable( GL_BLEND );
		pglDisable( GL_ALPHA_TEST );
		pglBlendFunc( GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		Con_DrawGenericChar( x + curPos, y, cursorChar, colorDefault );
	}
	else Con_DrawCharacter( x + curPos, y, '_', colorDefault );
}

/*
================
Con_DrawDebugLines

Custom debug messages
================
*/
int Con_DrawDebugLines( void )
{
	int	i, count = 0;
	int	y = 20;
	
	for( i = 0; i < MAX_DBG_NOTIFY; i++ )
	{
		if( host.realtime < con.notify[i].expire && con.notify[i].key_dest == cls.key_dest )
		{
			int	x, len;
			int	fontTall;

			Con_DrawStringLen( con.notify[i].szNotify, &len, &fontTall );
			x = scr_width->integer - 10 - len;
			fontTall += 1;

			if( y + fontTall > (int)scr_height->integer - 20 )
				return count;

			count++;
			y = 20 + fontTall * i;
			Con_DrawString( x, y, con.notify[i].szNotify, con.notify[i].color );
		}
	}
	return count;
}

/*
================
Con_DrawDebug

Draws the debug messages (not passed to console history)
================
*/
void Con_DrawDebug( void )
{
	if( !host.developer || Cvar_VariableInteger( "sv_background" ))
		return;

	if( con.draw_notify && !Con_Visible( ))
	{
		if( Con_DrawDebugLines() == 0 )
			con.draw_notify = false;
	}
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
	int	start, currentColor;
	short	*text;
	float	time;

	if( !host.developer || Cvar_VariableInteger( "sv_background" ))
		return;

	currentColor = 7;
	pglColor4ubv( g_color_table[currentColor] );

	for( i = con.current - CON_TIMES + 1; i <= con.current; i++ )
	{
		if( i < 0 ) continue;
		time = con.times[i % CON_TIMES];
		if( time == 0 ) continue;
		time = host.realtime - time;

		if( time > con_notifytime->value )
			continue;	// expired

		text = con.text + (i % con.totallines) * con.linewidth;
		start = con.charWidths[' ']; // offset one space at left screen side

		for( x = 0; x < con.linewidth; x++ )
		{
			if((( text[x] >> 8 ) & 7 ) != currentColor )
				currentColor = ( text[x] >> 8 ) & 7;
			start += Con_DrawCharacter( start, v, text[x] & 0xFF, g_color_table[currentColor] );
		}
		v += con.charHeight;
	}
	pglColor4ub( 255, 255, 255, 255 );
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
	int	lines, start;
	int	currentColor;
	string	curbuild;

	lines = scr_height->integer * frac;
	if( lines <= 0 ) return;
	if( lines > scr_height->integer )
		lines = scr_height->integer;

	// draw the background
	y = frac * scr_height->integer;

	if( y >= 1 )
	{
		GL_SetRenderMode( kRenderNormal );
		R_DrawStretchPic( 0, y - scr_height->integer, scr_width->integer, scr_height->integer, 0, 0, 1, 1, con.background );
	}
	else y = 0;

	if( host.developer )
	{
		// draw current version
		byte	*color = g_color_table[7];
		int	stringLen, width = 0, charH;

		Q_snprintf( curbuild, MAX_STRING, "Xash3D %i/%g (hw build %i)", PROTOCOL_VERSION, XASH_VERSION, Q_buildnum( ));
		Con_DrawStringLen( curbuild, &stringLen, &charH );
		start = scr_width->integer - stringLen;
		stringLen = Con_StringLength( curbuild );

		for( i = 0; i < stringLen; i++ )
			width += Con_DrawCharacter( start + width, 0, curbuild[i], color );
	}

	// draw the text
	con.vislines = lines;
	rows = ( lines - QCHAR_WIDTH ) / QCHAR_WIDTH; // rows of text to draw
	y = lines - ( con.charHeight * 3 );

	// draw from the bottom up
	if( con.display != con.current )
	{
		start = con.charWidths[' ']; // offset one space at left screen side

		// draw red arrows to show the buffer is backscrolled
		for( x = 0; x < con.linewidth; x += 4 )
			Con_DrawCharacter(( x + 1 ) * start, y, '^', g_color_table[1] );
		y -= con.charHeight;
		rows--;
	}
	
	row = con.display;
	if( con.x == 0 ) row--;

	currentColor = 7;
	pglColor4ubv( g_color_table[currentColor] );

	for( i = 0; i < rows; i++, y -= con.charHeight, row-- )
	{
		if( row < 0 ) break;
		if( con.current - row >= con.totallines )
		{
			// past scrollback wrap point
			continue;	
		}

		text = con.text + ( row % con.totallines ) * con.linewidth;
		start = con.charWidths[' ']; // offset one space at left screen side

		for( x = 0; x < con.linewidth; x++ )
		{
			if((( text[x] >> 8 ) & 7 ) != currentColor )
				currentColor = ( text[x] >> 8 ) & 7;
			start += Con_DrawCharacter( start, y, text[x] & 0xFF, g_color_table[currentColor] );
		}
	}

	// draw the input prompt, user text, and cursor if desired
	Con_DrawInput();
	pglColor4ub( 255, 255, 255, 255 );
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
			if( Cvar_VariableInteger( "sv_background" ) && cls.key_dest != key_console )
				con.displayFrac = con.finalFrac = 0.0f;
			else con.displayFrac = con.finalFrac = 1.0f;
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
			Key_SetKeyDest( key_console );
		}
		break;
	case ca_connected:
	case ca_connecting:
		// force to show console always for -dev 3 and higher 
		if( con.displayFrac ) Con_DrawSolidConsole( con.displayFrac );
		break;
	case ca_active:
	case ca_cinematic: 
		if( Cvar_VariableInteger( "sv_background" ))
		{
			if( cls.key_dest == key_console ) 
				Con_DrawSolidConsole( 1.0f );
		}
		else
		{
			if( con.displayFrac )
				Con_DrawSolidConsole( con.displayFrac );
			else if( cls.state == ca_active && cls.key_dest == key_game )
				Con_DrawNotify(); // draw notify lines
		}
		break;
	}
}

/*
==================
Con_DrawVersion

Used by menu
==================
*/
void Con_DrawVersion( void )
{
	// draws the current build
	byte	*color = g_color_table[7];
	int	i, stringLen, width = 0, charH;
	int	start, height = scr_height->integer;
	string	curbuild;

	if( cls.key_dest != key_menu ) return;

	Q_snprintf( curbuild, MAX_STRING, "v%i/%g (build %i)", PROTOCOL_VERSION, XASH_VERSION, Q_buildnum( ));
	Con_DrawStringLen( curbuild, &stringLen, &charH );
	start = scr_width->integer - stringLen * 1.05f;
	stringLen = Con_StringLength( curbuild );
	height -= charH * 1.5f;

	for( i = 0; i < stringLen; i++ )
		width += Con_DrawCharacter( start + width, height, curbuild[i], color );
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

	// when level is loading frametime may be is wrong
	if( cls.state == ca_connecting || cls.state == ca_connected )
		host.realframetime = ( MAX_FPS / host_maxfps->value ) * MIN_FRAMETIME;

	if( con.finalFrac < con.displayFrac )
	{
		con.displayFrac -= scr_conspeed->value * 0.002 * host.realframetime;
		if( con.finalFrac > con.displayFrac )
			con.displayFrac = con.finalFrac;
	}
	else if( con.finalFrac > con.displayFrac )
	{
		con.displayFrac += scr_conspeed->value * 0.002 * host.realframetime;
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
	Con_CheckResize();

	// loading console image
	if( host.developer )
	{
		if( scr_width->integer < 640 )
		{
			if( FS_FileExists( "cached/conback400", false ))
				con.background = GL_LoadTexture( "cached/conback400", NULL, 0, TF_IMAGE );
			else con.background = GL_LoadTexture( "cached/conback", NULL, 0, TF_IMAGE );
		}
		else
		{
			if( FS_FileExists( "cached/conback640", false ))
				con.background = GL_LoadTexture( "cached/conback640", NULL, 0, TF_IMAGE );
			else con.background = GL_LoadTexture( "cached/conback", NULL, 0, TF_IMAGE );
		}
	}
	else
	{
		if( scr_width->integer < 640 )
		{
			if( FS_FileExists( "cached/loading400", false ))
				con.background = GL_LoadTexture( "cached/loading400", NULL, 0, TF_IMAGE );
			else con.background = GL_LoadTexture( "cached/loading", NULL, 0, TF_IMAGE );
		}
		else
		{
			if( FS_FileExists( "cached/loading640", false ))
				con.background = GL_LoadTexture( "cached/loading640", NULL, 0, TF_IMAGE );
			else con.background = GL_LoadTexture( "cached/loading", NULL, 0, TF_IMAGE );
		}
	}

	// missed console image will be replaced as white (GoldSrc rules)
	if( con.background == tr.defaultTexture || con.background == 0 )
		con.background = tr.whiteTexture;

	Con_LoadConchars();
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
	Q_strncpy( input.buffer, complete_string, sizeof( input.buffer ));
	input.cursor = input.scroll = 0;

	Con_CompleteCommand( &input );

	// setup output
	if( input.buffer[0] == '\\' || input.buffer[0] == '/' )
		Q_strncpy( complete_string, input.buffer + 1, sizeof( input.buffer ));
	else Q_strncpy( complete_string, input.buffer, sizeof( input.buffer ));
}

void Con_Close( void )
{
	Con_ClearField( &con.input );
	Con_ClearNotify();
	con.finalFrac = 0; // none visible
	con.displayFrac = 0;
}

/*
=========
Con_DefaultColor

called from MainUI
=========
*/
void Con_DefaultColor( int r, int g, int b )
{
	r = bound( 0, r, 255 );
	g = bound( 0, g, 255 );
	b = bound( 0, b, 255 );
	MakeRGBA( g_color_table[7], r, g, b, 255 );
}