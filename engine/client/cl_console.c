/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// console.c

#include "client.h"

cvar_t		*con_notifytime;

vec4_t console_color = {1.0, 1.0, 1.0, 1.0};
extern cvar_t *scr_conspeed;		
int g_console_field_width = 78;

#define NUM_CON_TIMES	4
#define CON_TEXTSIZE	MAX_INPUTLINE * 32 // 512 kb buffer

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

	float	displayFrac;	// aproaches finalFrac at scr_conspeed
	float	finalFrac;	// 0.0 to 1.0 lines of console to display

	int	vislines;		// in scanlines
	float	times[NUM_CON_TIMES]; // cls.realtime time the line was generated for transparent notify lines
	vec4_t	color;

} console_t;

static console_t con;

void DrawString (int x, int y, char *s)
{
	while (*s)
	{
		re->DrawChar (x, y, *s);
		x+=8;
		s++;
	}
}

void DrawAltString (int x, int y, char *s)
{
	while (*s)
	{
		re->DrawChar (x, y, *s ^ 0x80);
		x+=8;
		s++;
	}
}

// strlen that discounts color sequences
int Con_PrintStrlen( const char *string )
{
	int		len;
	const char	*p;

	if( !string ) return 0;

	len = 0;
	p = string;
	while( *p )
	{
		if(IsColorString( p ))
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
void Con_ToggleConsole_f (void)
{
	SCR_EndLoadingPlaque();	// get rid of loading plaque
	Field_Clear( &g_consoleField );
	g_consoleField.widthInChars = g_console_field_width;

	Con_ClearNotify ();

	if (cls.key_dest == key_console)
	{
		M_ForceMenuOff();
		Cvar_Set ("paused", "0");
	}
	else
	{
		M_ForceMenuOff();
		cls.key_dest = key_console;	

		if(Cvar_VariableValue ("maxclients") == 1  && Com_ServerState())
			Cvar_Set ("paused", "1");
	}
}

/*
================
Con_ToggleChat_f
================
*/
void Con_ToggleChat_f (void)
{
	Field_Clear( &g_consoleField );
	g_consoleField.widthInChars = g_console_field_width;

	if (cls.key_dest == key_console)
	{
		if (cls.state == ca_active)
		{
			M_ForceMenuOff ();
			cls.key_dest = key_game;
		}
	}
	else
		cls.key_dest = key_console;
	
	Con_ClearNotify ();
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	int		i;

	for ( i = 0; i < CON_TEXTSIZE; i++ )
		con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
	Con_Bottom(); // go to end
}

						
/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
void Con_Dump_f (void)
{
	int		l, x, i;
	short		*line;
	file_t		*f;
	char		buffer[1024];
	char		name[MAX_OSPATH];

	if (Cmd_Argc() != 2)
	{
		Msg ("usage: condump <filename>\n");
		return;
	}

	sprintf (name, "%s.txt", Cmd_Argv(1));

	Msg ("Dumped console text to %s.\n", name);
	f = FS_Open (name, "w");
	if (!f)
	{
		Msg ("ERROR: couldn't open.\n");
		return;
	}

	// skip empty lines
	for (l = con.current - con.totallines + 1; l <= con.current; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		for (x = 0; x < con.linewidth; x++)
		{
			if ((line[x] & 0xff) != ' ')
				break;
		}
		if (x != con.linewidth) break;
	}

	// write the remaining lines
	buffer[con.linewidth] = 0;
	for ( ; l <= con.current; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		for(i = 0; i < con.linewidth; i++)
			buffer[i] = line[i] & 0xff;
		for (x = con.linewidth - 1; x >= 0; x--)
		{
			if (buffer[x] == ' ') buffer[x] = 0;
			else break;
		}
		strcat( buffer, "\n" );
		FS_Printf (f, "%s\n", buffer);
	}
	FS_Close (f);
}

						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	int		i;
	
	for (i=0 ; i<NUM_CON_TIMES ; i++)
		con.times[i] = 0;
}

						
/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void)
{
	chat_team = false;
	cls.key_dest = key_message;
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	chat_team = true;
	cls.key_dest = key_message;
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char	tbuf[CON_TEXTSIZE];

	width = (viddef.width >> 3) - 2;

	if (width == con.linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = 38;
		con.linewidth = width;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		memset (con.text, ' ', CON_TEXTSIZE);
	}
	else
	{
		oldwidth = con.linewidth;
		con.linewidth = width;
		oldtotallines = con.totallines;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		numlines = oldtotallines;

		if (con.totallines < numlines)
			numlines = con.totallines;

		numchars = oldwidth;
	
		if (con.linewidth < numchars)
			numchars = con.linewidth;

		memcpy (tbuf, con.text, CON_TEXTSIZE);
		memset (con.text, ' ', CON_TEXTSIZE);

		for (i=0 ; i<numlines ; i++)
		{
			for (j=0 ; j<numchars ; j++)
			{
				con.text[(con.totallines - 1 - i) * con.linewidth + j] =
						tbuf[((con.current - i + oldtotallines) %
							  oldtotallines) * oldwidth + j];
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
void Con_Init (void)
{
	con.linewidth = -1;

	Con_CheckResize ();
	
	MsgDev(D_INFO, "Console initialized.\n");

	// register our commands
	con_notifytime = Cvar_Get ("con_notifytime", "3", 0);

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("togglechat", Con_ToggleChat_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	Cmd_AddCommand ("condump", Con_Dump_f);
	con.initialized = true;
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (bool skipnotify)
{
	int	i;

	// mark time for transparent overlay
	if (con.current >= 0)
	{
		if(skipnotify) con.times[con.current % NUM_CON_TIMES] = 0.0f;
		else con.times[con.current % NUM_CON_TIMES] = cls.realtime;
	}

	con.x = 0;
	if (con.display == con.current) con.display++;
	con.current++;
	for(i = 0; i < con.linewidth; i++)
		con.text[(con.current%con.totallines)*con.linewidth+i] = (ColorIndex(COLOR_WHITE)<<8)|' ';
}

/*
================
CL_ConsolePrint

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void Con_Print( char *txt )
{
	int	y, c, l, color;
	bool	skipnotify = false;
	int	prev;

	// client not running
	if(host.type == HOST_DEDICATED) return;
          if(!con.initialized) return;

	if(!strncmp( txt, "[skipnotify]", 12 ))
	{
		skipnotify = true;
		txt += 12;
	}
	
	color = ColorIndex(COLOR_WHITE);

	while((c = *txt) != 0 )
	{
		if(IsColorString( txt ))
		{
			color = ColorIndex(*(txt+1));
			txt += 2;
			continue;
		}

		// count word length
		for (l = 0; l < con.linewidth; l++)
		{
			if ( txt[l] <= ' ') break;
		}

		// word wrap
		if (l != con.linewidth && (con.x + l >= con.linewidth))
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
			if (con.x >= con.linewidth)
			{
				Con_Linefeed(skipnotify);
				con.x = 0;
			}
			break;
		}
	}

	// mark time for transparent overlay
	if (con.current >= 0)
	{
		if ( skipnotify )
		{
			prev = con.current % NUM_CON_TIMES - 1;
			if ( prev < 0 ) prev = NUM_CON_TIMES - 1;
			con.times[prev] = 0.0f;
		}
		else con.times[con.current % NUM_CON_TIMES] = cls.realtime;
	}
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
void Con_DrawInput (void)
{
	int		y;

	if (cls.key_dest == key_menu) return;
	if (cls.key_dest != key_console && cls.state == ca_active)
		return; // don't draw anything (always draw if not active)

	y = con.vislines - ( SMALLCHAR_HEIGHT * 2 );
	re->SetColor( con.color );
	SCR_DrawSmallChar( con.xadjust + 1 * SMALLCHAR_WIDTH, y, '>' );
	Field_Draw( &g_consoleField, con.xadjust + 2 * SMALLCHAR_WIDTH, y, SCREEN_WIDTH - 3 * SMALLCHAR_WIDTH, true );
}

/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	int		x, v;
	short		*text;
	int		i;
	float		time;
	int		skip;
	int		currentColor;

	currentColor = 7;
	re->SetColor( g_color_table[currentColor] );

	v = 0;
	for (i = con.current-NUM_CON_TIMES + 1; i <= con.current; i++)
	{
		if (i < 0) continue;
		time = con.times[i % NUM_CON_TIMES];
		if (time == 0) continue;
		time = cls.realtime - time;
		if (time > con_notifytime->value) continue;
		text = con.text + (i % con.totallines)*con.linewidth;

		for (x = 0 ; x < con.linewidth ; x++)
		{
			if ( ( text[x] & 0xff ) == ' ' ) continue;
			if ( ( (text[x]>>8)&7 ) != currentColor )
			{
				currentColor = (text[x]>>8)&7;
				re->SetColor( g_color_table[currentColor] );
			}
			SCR_DrawSmallChar( con.xadjust + (x+1)*SMALLCHAR_WIDTH, v, text[x] & 0xff );
		}
		v += SMALLCHAR_HEIGHT;
	}

	re->SetColor( NULL );

	// draw the chat line
	if ( cls.key_dest == key_message )
	{
		if (chat_team)
		{
			SCR_DrawBigString (8, v, "say_team:", 1.0f );
			skip = 11;
		}
		else
		{
			SCR_DrawBigString (8, v, "say:", 1.0f );
			skip = 5;
		}
		Field_BigDraw( &chatField, skip * BIGCHAR_WIDTH, v, SCREEN_WIDTH - ( skip + 1 ) * BIGCHAR_WIDTH, true );
		v += BIGCHAR_HEIGHT;
	}

}

/*
================
Con_DrawConsole

Draws the console with the solid background
================
*/
void Con_DrawSolidConsole (float frac)
{
	int	i, x, y;
	int	rows;
	short	*text;
	int	row;
	int	lines;
	int	currentColor;
	vec4_t	color;

	lines = viddef.height * frac;
	if (lines <= 0) return;
	if (lines > viddef.height) lines = viddef.height;

	con.xadjust = 0; // on wide screens, we will center the text
	SCR_AdjustSize( &con.xadjust, NULL, NULL, NULL );

	// draw the background
	y = frac * SCREEN_HEIGHT - 2;
	if ( y < 1 ) y = 0;
	else SCR_DrawPic( 0, -SCREEN_HEIGHT + lines, SCREEN_WIDTH, SCREEN_HEIGHT, "conback" );

	Vector4Set( color, 1, 0, 0, 1 );
	SCR_FillRect( 0, y, SCREEN_WIDTH, 2, color );

	// draw the version number
	re->SetColor(g_color_table[ColorIndex(COLOR_RED)]);
	i = strlen( VERSION );
	for (x = 0; x < i; x++)
		SCR_DrawSmallChar( viddef.width - ( i - x ) * SMALLCHAR_WIDTH, (lines - (SMALLCHAR_HEIGHT+SMALLCHAR_HEIGHT/2)), VERSION[x]);

	// draw the text
	con.vislines = lines;
	rows = (lines - SMALLCHAR_WIDTH)/SMALLCHAR_WIDTH; // rows of text to draw
	y = lines - (SMALLCHAR_HEIGHT * 3);

	// draw from the bottom up
	if (con.display != con.current)
	{
		// draw arrows to show the buffer is backscrolled
		re->SetColor(g_color_table[ColorIndex(COLOR_RED)]);
		for (x = 0; x < con.linewidth; x += 4)
			SCR_DrawSmallChar( con.xadjust + (x+1) * SMALLCHAR_WIDTH, y, '^' );
		y -= SMALLCHAR_HEIGHT;
		rows--;
	}
	
	row = con.display;
	if( con.x == 0 ) row--;

	currentColor = 7;
	re->SetColor( g_color_table[currentColor] );

	for (i = 0; i < rows; i++, y -= SMALLCHAR_HEIGHT, row--)
	{
		if (row < 0) break;
		if (con.current - row >= con.totallines)
		{
			// past scrollback wrap point
			continue;	
		}

		text = con.text + (row % con.totallines)*con.linewidth;

		for (x = 0; x < con.linewidth; x++)
		{
			if((text[x] & 0xff ) == ' ')continue;
			if(((text[x]>>8)&7 ) != currentColor )
			{
				currentColor = (text[x]>>8)&7;
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
	// check for console width changes from a vid mode change
	Con_CheckResize ();

	// if disconnected, render console full screen
	if ( cls.state == ca_disconnected  || cls.state == ca_connecting)
	{
		Con_DrawSolidConsole( 1.0 );
	}

	if (cls.state != ca_active || !cl.refresh_prepped)
	{	
		// connected, but can't render
		if(cl.cinematictime > 0) SCR_DrawCinematic();
		else SCR_FillRect( 0, SCREEN_HEIGHT/2, SCREEN_WIDTH, SCREEN_HEIGHT/2, COLOR_0 );
		Con_DrawSolidConsole (0.5);
		return;
	}

	if ( con.displayFrac )
	{
		Con_DrawSolidConsole( con.displayFrac );
	}
	else
	{
		// draw notify lines
		if ( cls.state == ca_active )
		{
			Con_DrawNotify ();
		}
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
	if (cls.key_dest == key_console)
		con.finalFrac = 0.5; // half screen
	else con.finalFrac = 0; // none visible

	if (con.finalFrac < con.displayFrac)
	{
		con.displayFrac -= scr_conspeed->value*cls.frametime;
		if (con.finalFrac > con.displayFrac)
			con.displayFrac = con.finalFrac;
	}
	else if (con.finalFrac > con.displayFrac)
	{
		con.displayFrac += scr_conspeed->value*cls.frametime;
		if (con.finalFrac < con.displayFrac)
			con.displayFrac = con.finalFrac;
	}
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
	if (con.display > con.current) con.display = con.current;
}

void Con_Top( void )
{
	con.display = con.totallines;
	if ( con.current - con.display >= con.totallines )
		con.display = con.current - con.totallines + 1;
}

void Con_Bottom( void )
{
	con.display = con.current;
}

void Con_Close( void )
{
	Field_Clear( &g_consoleField );
	Con_ClearNotify ();
	con.finalFrac = 0; // none visible
	con.displayFrac = 0;
}

bool Con_Active( void )
{
	return con.initialized;
}