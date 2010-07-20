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

#include "extdll.h"
#include "basemenu.h"
#include "utils.h"

#define ART_MAIN_CREDITS		"gfx/shell/credits"
#define UI_CREDITS_PATH		"scripts/credits.txt"
#define UI_CREDITS_MAXLINES		2048

static const char *uiCreditsDefault[] = 
{
	"^3Xash3D",
	"",
	"^3PROGRAMMING",
	"Uncle Mike",
	"",
	"^3ART",
	"Chorus",
	"Small Link",
	"The FiEctro",
	"",
	"^3LEVEL DESIGN",
	"Scrama",
	"Mitoh",
	"",
	"",
	"^3SPECIAL THANKS",
	"Chain Studios et al.",
	"",
	"",
	"",
	"",
	"",
	"",
	"^3MUSIC",
	"Dj Existence",
	"",
	"",
	"^3THANKS TO",
	"ID Software at all",
	"Georg Destroy for icon graphics",
	"LokiMb for fixed console font",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"^3Xash3D using some parts of:",
	"Doom 1 (Id Software)",
	"Quake 1 (Id Software)",
	"Quake 2 (Id Software)",
	"Quake 3 (Id Software)",
	"Half-Life (Valve Software)",
	"Darkplaces (Darkplaces Team)",
	"Quake 2 Evolved (Team Blur)",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"Copyright XashXT Group 2010 (C)",
	0
};

typedef struct
{
	const char	**credits;
	int		startTime;
	int		showTime;
	int		fadeTime;
	int		numLines;
	int		active;
	int		finalCredits;
	char		*index[UI_CREDITS_MAXLINES];
	char		*buffer;

	menuFramework_s	menu;
} uiCredits_t;

static uiCredits_t		uiCredits;


/*
=================
UI_Credits_DrawFunc
=================
*/
static void UI_Credits_DrawFunc( void )
{
	int	i, y;
	int	w = UI_MED_CHAR_WIDTH;
	int	h = UI_MED_CHAR_HEIGHT;
	int	color = 0;

	// draw the background first
	if( !uiCredits.finalCredits )
		UI_DrawPic( 0, 0, 1024 * uiStatic.scaleX, 768 * uiStatic.scaleY, uiColorWhite, ART_MAIN_CREDITS );
	// otherwise running on cutscene

	// now draw the credits
	UI_ScaleCoords( NULL, NULL, &w, &h );

	y = ScreenHeight - (( uiStatic.realTime - uiCredits.startTime ) / 40.0f );

	// draw the credits
	for ( i = 0; i < uiCredits.numLines && uiCredits.credits[i]; i++, y += 20 )
	{
		// skip not visible lines, but always draw end line
		if( y <= -16 && i != uiCredits.numLines - 1 ) continue;

		if(( y < ( ScreenHeight - h ) / 2 ) && i == uiCredits.numLines - 1 )
		{
			if( !uiCredits.fadeTime ) uiCredits.fadeTime = uiStatic.realTime;
			color = UI_FadeAlpha( uiCredits.fadeTime, uiCredits.showTime );
			if( UnpackAlpha( color ))
				UI_DrawString( 0, ( ScreenHeight - h ) / 2, 1024 * uiStatic.scaleX, h, uiCredits.credits[i], color, true, w, h, 1, true );
		}
		else UI_DrawString( 0, y, 1024 * uiStatic.scaleX, h, uiCredits.credits[i], uiColorWhite, false, w, h, 1, true );
	}

	if( y < 0 && UnpackAlpha( color ) == 0 )
	{
		uiCredits.active = false; // end of credits
		if( uiCredits.finalCredits )
			HOST_ENDGAME( gMenu.m_gameinfo.title );
	}

	if( !uiCredits.active )
		UI_PopMenu();
}

/*
=================
UI_Credits_KeyFunc
=================
*/
static const char *UI_Credits_KeyFunc( int key, int down )
{
	if( !down ) return uiSoundNull;

	// final credits can't be intterupted
	if( uiCredits.finalCredits )
		return uiSoundNull;

	uiCredits.active = false;
	return uiSoundNull;
}

/*
=================
UI_Credits_Init
=================
*/
static void UI_Credits_Init( void )
{
	uiCredits.menu.drawFunc = UI_Credits_DrawFunc;
	uiCredits.menu.keyFunc = UI_Credits_KeyFunc;

	if( !uiCredits.buffer )
	{
		int	count;
		char	*p;

		// load credits if needed
		uiCredits.buffer = (char *)LOAD_FILE( UI_CREDITS_PATH, &count );
		if( count )
		{
			p = uiCredits.buffer;

			// convert customs credits to 'ideal' strings array
			for ( uiCredits.numLines = 0; uiCredits.numLines < UI_CREDITS_MAXLINES; uiCredits.numLines++ )
			{
				uiCredits.index[uiCredits.numLines] = p;
				while ( *p != '\r' && *p != '\n' )
				{
					p++;
					if ( --count == 0 )
						break;
				}

				if ( *p == '\r' )
				{
					*p++ = 0;
					if( --count == 0 ) break;
				}

				*p++ = 0;
				if( --count == 0 ) break;
			}
			uiCredits.index[++uiCredits.numLines] = 0;
			uiCredits.credits = (const char **)uiCredits.index;
		}
		else
		{
			// use built-in credits
			uiCredits.credits =  uiCreditsDefault;
			uiCredits.numLines = ( sizeof( uiCreditsDefault ) / sizeof( uiCreditsDefault[0] )) - 1; // skip term
		}
	}

	// run credits
	uiCredits.startTime = uiStatic.realTime + 500; // make half-seconds delay
	uiCredits.showTime = bound( 100, strlen( uiCredits.credits[uiCredits.numLines - 1]) * 1000, 12000 );
	uiCredits.fadeTime = 0; // will be determined later
	uiCredits.active = true;
}

int UI_CreditsActive( void )
{
	return uiCredits.active;
}

/*
=================
UI_Credits_Precache
=================
*/
void UI_Credits_Precache( void )
{
	PIC_Load( ART_MAIN_CREDITS );
}

/*
=================
UI_Credits_Menu
=================
*/
void UI_Credits_Menu( void )
{
	UI_Credits_Precache();
	UI_Credits_Init();

	UI_PushMenu( &uiCredits.menu );
}

void UI_FinalCredits( void )
{
	UI_Credits_Menu();
	uiCredits.finalCredits = true;
}