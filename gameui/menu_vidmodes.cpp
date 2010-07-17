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

#define ART_BANNER		"gfx/shell/head_vidmodes"

#define ID_BACKGROUND	0
#define ID_BANNER		1
#define ID_OK		2
#define ID_CANCEL		3
#define ID_RENDERLIBRARY	4
#define ID_VIDMODELIST	5
#define ID_FULLSCREEN	6
#define ID_SOFTWARE		7
#define ID_TABLEHINT	8
#define ID_MSGHINT		9

#define MAX_VIDMODES	(sizeof( uiVideoModes ) / sizeof( uiVideoModes[0] )) + 1

static const char *uiVideoModes[] =
{
	"640 x 480",
	"800 x 600",
	"960 x 720",
	"1024 x 768",
	"1152 x 864",
	"1280 x 800",
	"1280 x 960",
	"1280 x 1024",
	"1600 x 1200",
	"2048 x 1536",
	"856 x 480 (wide)",
	"1024 x 576 (wide)",
	"1440 x 900 (wide)",
	"1680 x 1050 (wide)",
	"1920 x 1200 (wide)",
	"2560 x 1600 (wide)",
};

typedef struct
{
	const char	*videoModesPtr[MAX_VIDMODES];
	char		**videoList;
	int		numVideoDlls;

	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;
	menuAction_s	ok;
	menuAction_s	cancel;
	menuCheckBox_s	windowed;
	menuCheckBox_s	software;

	menuSpinControl_s	videoLibrary;
	menuScrollList_s	vidList;
	menuAction_s	listCaption;
	menuAction_s	vidlibCaption;
	menuAction_s	hintMessage;
	char		hintText[MAX_HINT_TEXT];
} uiVidModes_t;

static uiVidModes_t	uiVidModes;

/*
=================
UI_VidModes_GetModesList
=================
*/
static void UI_VidModes_GetConfig( void )
{
	uiVidModes.videoList = GET_VIDEO_LIST( &uiVidModes.numVideoDlls ); 
	
	for( int i = 0; i < uiVidModes.numVideoDlls; i++ )
	{
		if( !stricmp( uiVidModes.videoList[i], CVAR_GET_STRING( "host_video" )))
			uiVidModes.videoLibrary.curValue = i;
	}

	uiVidModes.videoLibrary.maxValue = uiVidModes.numVideoDlls - 1;
	uiVidModes.videoLibrary.generic.name = uiVidModes.videoList[(int)uiVidModes.videoLibrary.curValue];

	for( i = 0; i < MAX_VIDMODES-1; i++ )
		uiVidModes.videoModesPtr[i] = uiVideoModes[i];
	uiVidModes.videoModesPtr[i] = NULL;	// terminator

	uiVidModes.vidList.itemNames = uiVidModes.videoModesPtr;
	uiVidModes.vidList.curItem = CVAR_GET_FLOAT( "r_mode" );

	if( !CVAR_GET_FLOAT( "fullscreen" ))
		uiVidModes.windowed.enabled = 1;

	if( CVAR_GET_FLOAT( "r_allow_software" ))
		uiVidModes.software.enabled = 1;
}

/*
=================
UI_VidModes_SetConfig
=================
*/
static void UI_VidOptions_SetConfig( void )
{
	CVAR_SET_FLOAT( "r_mode", uiVidModes.vidList.curItem );
	CVAR_SET_FLOAT( "fullscreen", !uiVidModes.windowed.enabled );
	CVAR_SET_FLOAT( "r_allow_software", uiVidModes.software.enabled );

	CHANGE_VIDEO( uiVidModes.videoList[(int)uiVidModes.videoLibrary.curValue] );
}

/*
=================
UI_VidModes_UpdateConfig
=================
*/
static void UI_VidOptions_UpdateConfig( void )
{
	CVAR_SET_FLOAT( "r_allow_software", uiVidModes.software.enabled );
}

/*
=================
UI_VidModes_Callback
=================
*/
static void UI_VidModes_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	switch( item->id )
	{
	case ID_FULLSCREEN:
	case ID_SOFTWARE:
		if( event == QM_PRESSED )
			((menuCheckBox_s *)self)->focusPic = UI_CHECKBOX_PRESSED;
		else ((menuCheckBox_s *)self)->focusPic = UI_CHECKBOX_FOCUS;
		break;
	}

	if( event == QM_CHANGED )
	{
		UI_VidOptions_UpdateConfig();
		return;
	}

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_CANCEL:
		UI_PopMenu();
		break;
	case ID_OK:
		UI_VidOptions_SetConfig ();
		break;
	}
}

/*
=================
UI_VidModes_Init
=================
*/
static void UI_VidModes_Init( void )
{
	memset( &uiVidModes, 0, sizeof( uiVidModes_t ));

	strcat( uiVidModes.hintText, "Change Xash3D rendering engine\n" );
	strcat( uiVidModes.hintText, "to get more compatibility and\n" );
	strcat( uiVidModes.hintText, "optimize FPS.\n" );
	strcat( uiVidModes.hintText, "Also enables more hardware\n" );
	strcat( uiVidModes.hintText, "features on top-computers." );
	
	uiVidModes.background.generic.id = ID_BACKGROUND;
	uiVidModes.background.generic.type = QMTYPE_BITMAP;
	uiVidModes.background.generic.flags = QMF_INACTIVE;
	uiVidModes.background.generic.x = 0;
	uiVidModes.background.generic.y = 0;
	uiVidModes.background.generic.width = 1024;
	uiVidModes.background.generic.height = 768;
	uiVidModes.background.pic = ART_BACKGROUND;

	uiVidModes.banner.generic.id = ID_BANNER;
	uiVidModes.banner.generic.type = QMTYPE_BITMAP;
	uiVidModes.banner.generic.flags = QMF_INACTIVE;
	uiVidModes.banner.generic.x = UI_BANNER_POSX;
	uiVidModes.banner.generic.y = UI_BANNER_POSY;
	uiVidModes.banner.generic.width = UI_BANNER_WIDTH;
	uiVidModes.banner.generic.height = UI_BANNER_HEIGHT;
	uiVidModes.banner.pic = ART_BANNER;

	uiVidModes.ok.generic.id = ID_OK;
	uiVidModes.ok.generic.type = QMTYPE_ACTION;
	uiVidModes.ok.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiVidModes.ok.generic.x = 72;
	uiVidModes.ok.generic.y = 230;
	uiVidModes.ok.generic.name = "Ok";
	uiVidModes.ok.generic.statusText = "Apply changes and return to the Main Menu";
	uiVidModes.ok.generic.callback = UI_VidModes_Callback;

	uiVidModes.cancel.generic.id = ID_CANCEL;
	uiVidModes.cancel.generic.type = QMTYPE_ACTION;
	uiVidModes.cancel.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiVidModes.cancel.generic.x = 72;
	uiVidModes.cancel.generic.y = 280;
	uiVidModes.cancel.generic.name = "Cancel";
	uiVidModes.cancel.generic.statusText = "Return back to previous menu";
	uiVidModes.cancel.generic.callback = UI_VidModes_Callback;

	uiVidModes.videoLibrary.generic.id = ID_RENDERLIBRARY;
	uiVidModes.videoLibrary.generic.type = QMTYPE_SPINCONTROL;
	uiVidModes.videoLibrary.generic.flags = QMF_CENTER_JUSTIFY|QMF_SMALLFONT|QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiVidModes.videoLibrary.generic.x = 112;
	uiVidModes.videoLibrary.generic.y = 390;
	uiVidModes.videoLibrary.generic.width = 168;
	uiVidModes.videoLibrary.generic.height = 26;
	uiVidModes.videoLibrary.generic.callback = UI_VidModes_Callback;
	uiVidModes.videoLibrary.generic.statusText = "Select renderer";
	uiVidModes.videoLibrary.minValue = 0;
	uiVidModes.videoLibrary.maxValue = 0;
	uiVidModes.videoLibrary.range = 1;

	uiVidModes.vidlibCaption.generic.id = ID_TABLEHINT;
	uiVidModes.vidlibCaption.generic.type = QMTYPE_ACTION;
	uiVidModes.vidlibCaption.generic.flags = QMF_INACTIVE|QMF_SMALLFONT;
	uiVidModes.vidlibCaption.generic.color = uiColorHelp;
	uiVidModes.vidlibCaption.generic.name = "Rendering module";
	uiVidModes.vidlibCaption.generic.x = 72;
	uiVidModes.vidlibCaption.generic.y = 350;

	uiVidModes.hintMessage.generic.id = ID_TABLEHINT;
	uiVidModes.hintMessage.generic.type = QMTYPE_ACTION;
	uiVidModes.hintMessage.generic.flags = QMF_INACTIVE|QMF_SMALLFONT;
	uiVidModes.hintMessage.generic.color = uiColorHelp;
	uiVidModes.hintMessage.generic.name = uiVidModes.hintText;
	uiVidModes.hintMessage.generic.x = 72;
	uiVidModes.hintMessage.generic.y = 450;

	uiVidModes.listCaption.generic.id = ID_TABLEHINT;
	uiVidModes.listCaption.generic.type = QMTYPE_ACTION;
	uiVidModes.listCaption.generic.flags = QMF_INACTIVE|QMF_SMALLFONT;
	uiVidModes.listCaption.generic.color = uiColorHelp;
	uiVidModes.listCaption.generic.name = "Display mode";
	uiVidModes.listCaption.generic.x = 400;
	uiVidModes.listCaption.generic.y = 270;

	uiVidModes.vidList.generic.id = ID_VIDMODELIST;
	uiVidModes.vidList.generic.type = QMTYPE_SCROLLLIST;
	uiVidModes.vidList.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_SMALLFONT;
	uiVidModes.vidList.generic.x = 400;
	uiVidModes.vidList.generic.y = 300;
	uiVidModes.vidList.generic.width = 560;
	uiVidModes.vidList.generic.height = 300;
	uiVidModes.vidList.generic.callback = UI_VidModes_Callback;

	uiVidModes.windowed.generic.id = ID_FULLSCREEN;
	uiVidModes.windowed.generic.type = QMTYPE_CHECKBOX;
	uiVidModes.windowed.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_ACT_ONRELEASE|QMF_MOUSEONLY|QMF_DROPSHADOW;
	uiVidModes.windowed.generic.name = "Run in a window";
	uiVidModes.windowed.generic.x = 400;
	uiVidModes.windowed.generic.y = 620;
	uiVidModes.windowed.generic.callback = UI_VidModes_Callback;
	uiVidModes.windowed.generic.statusText = "Run game in window mode";

	uiVidModes.software.generic.id = ID_SOFTWARE;
	uiVidModes.software.generic.type = QMTYPE_CHECKBOX;
	uiVidModes.software.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_ACT_ONRELEASE|QMF_MOUSEONLY|QMF_DROPSHADOW;
	uiVidModes.software.generic.name = "Allow software";
	uiVidModes.software.generic.x = 400;
	uiVidModes.software.generic.y = 670;
	uiVidModes.software.generic.callback = UI_VidModes_Callback;
	uiVidModes.software.generic.statusText = "allow software rendering when hardware support is missing";

	UI_VidModes_GetConfig();

	UI_AddItem( &uiVidModes.menu, (void *)&uiVidModes.background );
	UI_AddItem( &uiVidModes.menu, (void *)&uiVidModes.banner );
	UI_AddItem( &uiVidModes.menu, (void *)&uiVidModes.ok );
	UI_AddItem( &uiVidModes.menu, (void *)&uiVidModes.cancel );
	UI_AddItem( &uiVidModes.menu, (void *)&uiVidModes.windowed );
	UI_AddItem( &uiVidModes.menu, (void *)&uiVidModes.software );
	UI_AddItem( &uiVidModes.menu, (void *)&uiVidModes.listCaption );
	UI_AddItem( &uiVidModes.menu, (void *)&uiVidModes.vidlibCaption );
	UI_AddItem( &uiVidModes.menu, (void *)&uiVidModes.hintMessage );
	UI_AddItem( &uiVidModes.menu, (void *)&uiVidModes.videoLibrary );
	UI_AddItem( &uiVidModes.menu, (void *)&uiVidModes.vidList );
}

/*
=================
UI_VidModes_Precache
=================
*/
void UI_VidModes_Precache( void )
{
	PIC_Load( ART_BACKGROUND );
	PIC_Load( ART_BANNER );
}

/*
=================
UI_VidModes_Menu
=================
*/
void UI_VidModes_Menu( void )
{
	UI_VidModes_Precache();
	UI_VidModes_Init();

	UI_PushMenu( &uiVidModes.menu );
}