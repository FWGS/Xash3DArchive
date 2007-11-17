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
#include "client.h"
#include "qmenu.h"

extern cvar_t *r_fullscreen;
extern cvar_t *vid_gamma;

static cvar_t *gl_mode;
static cvar_t *gl_picmip;
static cvar_t *gl_ext_palettedtexture;
static cvar_t *gl_finish;

extern void M_ForceMenuOff( void );

/*
====================================================================

MENU INTERACTION

====================================================================
*/

static menuframework_s	s_video_menu;
static menulist_s		s_mode_list;
static menuslider_s		s_tq_slider;
static menuslider_s		s_brightness_slider;
static menulist_s  		s_fs_box;
static menulist_s  		s_stipple_box;
static menulist_s  		s_paletted_texture_box;
static menulist_s  		s_finish_box;
static menuaction_s		s_cancel_action;
static menuaction_s		s_defaults_action;

static void ScreenSizeCallback( void *s )
{
	menuslider_s *slider = (menuslider_s *)s;
	Cvar_SetValue( "viewsize", slider->curvalue * 10 );
}

static void BrightnessCallback( void *s )
{
	menuslider_s *slider = (menuslider_s *)s;
	float gamma = ( 0.8 - ( slider->curvalue / 10.0 - 0.5 ) ) + 0.5;

	Cvar_SetValue( "vid_gamma", gamma );
}

static void ResetDefaults( void *unused )
{
	VID_MenuInit();
}

static void ApplyChanges( void *unused )
{
	Cvar_SetValue( "gl_picmip", 3 - s_tq_slider.curvalue );
	Cvar_SetValue( "r_fullscreen", s_fs_box.curvalue );
	Cvar_SetValue( "gl_ext_palettedtexture", s_paletted_texture_box.curvalue );
	Cvar_SetValue( "gl_finish", s_finish_box.curvalue );
	Cvar_SetValue( "gl_mode", s_mode_list.curvalue );
	Cbuf_AddText("vid_restart\n"); // restart render

	M_ForceMenuOff();
}

static void CancelChanges( void *unused )
{
	extern void M_PopMenu( void );

	M_PopMenu();
}

/*
** VID_MenuInit
*/
void VID_MenuInit( void )
{
	static const char *resolutions[] = 
	{
		"[640 480  ]",
		"[800 600  ]",
		"[1024 768 ]",
		"[1280 960 ]",
		"[1280 1024]",
		"[1600 1200]",
		"[2048 1536]",
		0,
	};
	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0,
	};

	if ( !gl_picmip ) gl_picmip = Cvar_Get( "gl_picmip", "0", 0 );
	if ( !gl_mode ) gl_mode = Cvar_Get( "gl_mode", "3", 0 );
	if ( !gl_finish ) gl_finish = Cvar_Get( "gl_finish", "0", CVAR_ARCHIVE );
	if ( !gl_ext_palettedtexture ) gl_ext_palettedtexture = Cvar_Get( "gl_ext_palettedtexture", "1", CVAR_ARCHIVE );

	s_mode_list.curvalue = gl_mode->value;

	s_video_menu.x = viddef.width * 0.50;
	s_video_menu.y = viddef.height / 2 - 58;
	s_video_menu.nitems = 0;

	s_mode_list.generic.type = MTYPE_SPINCONTROL;
	s_mode_list.generic.name = "video mode";
	s_mode_list.generic.x = 0;
	s_mode_list.generic.y = 0;
	s_mode_list.itemnames = resolutions;

	s_brightness_slider.generic.type = MTYPE_SLIDER;
	s_brightness_slider.generic.x	= 0;
	s_brightness_slider.generic.y	= 10;
	s_brightness_slider.generic.name = "brightness";
	s_brightness_slider.generic.callback = BrightnessCallback;
	s_brightness_slider.minvalue = 5;
	s_brightness_slider.maxvalue = 13;
	s_brightness_slider.curvalue = ( 1.3 - vid_gamma->value + 0.5 ) * 10;

	s_fs_box.generic.type = MTYPE_SPINCONTROL;
	s_fs_box.generic.x	= 0;
	s_fs_box.generic.y	= 20;
	s_fs_box.generic.name = "fullscreen";
	s_fs_box.itemnames = yesno_names;
	s_fs_box.curvalue = r_fullscreen->value;

	s_tq_slider.generic.type = MTYPE_SLIDER;
	s_tq_slider.generic.x = 0;
	s_tq_slider.generic.y = 30;
	s_tq_slider.generic.name = "texture quality";
	s_tq_slider.minvalue = 0;
	s_tq_slider.maxvalue = 3;
	s_tq_slider.curvalue = 3 - gl_picmip->value;

	s_paletted_texture_box.generic.type = MTYPE_SPINCONTROL;
	s_paletted_texture_box.generic.x = 0;
	s_paletted_texture_box.generic.y = 40;
	s_paletted_texture_box.generic.name = "8-bit textures";
	s_paletted_texture_box.itemnames = yesno_names;
	s_paletted_texture_box.curvalue = gl_ext_palettedtexture->value;

	s_finish_box.generic.type = MTYPE_SPINCONTROL;
	s_finish_box.generic.x = 0;
	s_finish_box.generic.y = 50;
	s_finish_box.generic.name = "sync every frame";
	s_finish_box.curvalue = gl_finish->value;
	s_finish_box.itemnames = yesno_names;

	s_defaults_action.generic.type = MTYPE_ACTION;
	s_defaults_action.generic.name = "reset to defaults";
	s_defaults_action.generic.x    = 0;
	s_defaults_action.generic.y    = 70;
	s_defaults_action.generic.callback = ResetDefaults;

	s_cancel_action.generic.type = MTYPE_ACTION;
	s_cancel_action.generic.name = "cancel";
	s_cancel_action.generic.x = 0;
	s_cancel_action.generic.y = 80;
	s_cancel_action.generic.callback = CancelChanges;

	Menu_AddItem( &s_video_menu, ( void * ) &s_mode_list );
	Menu_AddItem( &s_video_menu, ( void * ) &s_brightness_slider );
	Menu_AddItem( &s_video_menu, ( void * ) &s_fs_box );
	Menu_AddItem( &s_video_menu, ( void * ) &s_tq_slider );
	Menu_AddItem( &s_video_menu, ( void * ) &s_paletted_texture_box );
	Menu_AddItem( &s_video_menu, ( void * ) &s_finish_box );
	Menu_AddItem( &s_video_menu, ( void * ) &s_defaults_action );
	Menu_AddItem( &s_video_menu, ( void * ) &s_cancel_action );

	Menu_Center( &s_video_menu );
	s_video_menu.x -= 8;
}

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
	int w, h;

	// draw the banner
	re->DrawGetPicSize( &w, &h, "menu/m_banner_video" );
	SCR_DrawPic( SCREEN_WIDTH / 2 - w / 2, SCREEN_HEIGHT /2 - 110, -1, -1, "menu/m_banner_video" );

	// move cursor to a reasonable starting position
	Menu_AdjustCursor( &s_video_menu, 1 );

	// draw the menu
	Menu_Draw( &s_video_menu );
}

/*
================
VID_MenuKey
================
*/
const char *VID_MenuKey( int key )
{
	menuframework_s *m = &s_video_menu;
	static const char *sound = "misc/menu1.wav";

	switch ( key )
	{
	case K_ESCAPE:
		ApplyChanges( 0 );
		return NULL;
	case K_KP_UPARROW:
	case K_UPARROW:
		m->cursor--;
		Menu_AdjustCursor( m, -1 );
		break;
	case K_KP_DOWNARROW:
	case K_DOWNARROW:
		m->cursor++;
		Menu_AdjustCursor( m, 1 );
		break;
	case K_KP_LEFTARROW:
	case K_LEFTARROW:
		Menu_SlideItem( m, -1 );
		break;
	case K_KP_RIGHTARROW:
	case K_RIGHTARROW:
		Menu_SlideItem( m, 1 );
		break;
	case K_KP_ENTER:
	case K_ENTER:
		if( !Menu_SelectItem( m ))
			ApplyChanges( NULL );
		break;
	}

	return sound;
}


