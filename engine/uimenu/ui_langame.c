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

#include "common.h"
#include "ui_local.h"
#include "client.h"

#define ART_BANNER		"gfx/shell/head_lan"

#define ID_BACKGROUND	0
#define ID_BANNER		1
#define ID_JOINGAME		2
#define ID_CREATEGAME	3
#define ID_GAMEINFO		4
#define ID_REFRESH		5
#define ID_DONE		6
#define ID_SERVERSLIST	7
#define ID_TABLEHINT	8

#define GAME_LENGTH		18
#define MAPNAME_LENGTH	20+GAME_LENGTH
#define TYPE_LENGTH		16+MAPNAME_LENGTH
#define MAXCL_LENGTH	15+TYPE_LENGTH

typedef struct
{
	string		gameDescription[UI_MAX_SERVERS];
	char		*gameDescriptionPtr[UI_MAX_SERVERS];

	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;
	menuAction_s	joinGame;
	menuAction_s	createGame;
	menuAction_s	gameInfo;
	menuAction_s	refresh;
	menuAction_s	done;

	menuScrollList_s	gameList;
	menuAction_s	hintMessage;
	char		hintText[MAX_SYSPATH];
	int		refreshTime;
} uiLanGame_t;

static uiLanGame_t	uiLanGame;

/*
=================
UI_LanGame_GetGamesList
=================
*/
static void UI_LanGame_GetGamesList( void )
{
	int		i;
	const char	*info;

	for( i = 0; i < uiStatic.numServers; i++ )
	{
		if( i >= UI_MAX_SERVERS ) break;
		info = uiStatic.serverNames[i];
 
		com.strncat( uiLanGame.gameDescription[i], Info_ValueForKey( info, "host" ), GAME_LENGTH );
		com.strncat( uiLanGame.gameDescription[i], uiEmptyString, GAME_LENGTH );
		com.strncat( uiLanGame.gameDescription[i], Info_ValueForKey( info, "map" ), MAPNAME_LENGTH );
		com.strncat( uiLanGame.gameDescription[i], uiEmptyString, MAPNAME_LENGTH );
		if( !com.strcmp( Info_ValueForKey( info, "dm" ), "1" ))
			com.strncat( uiLanGame.gameDescription[i], "deathmatch", TYPE_LENGTH );
		else if( !com.strcmp( Info_ValueForKey( info, "coop" ), "1" ))
			com.strncat( uiLanGame.gameDescription[i], "coop", TYPE_LENGTH );
		else if( !com.strcmp( Info_ValueForKey( info, "team" ), "1" ))
			com.strncat( uiLanGame.gameDescription[i], "teamplay", TYPE_LENGTH );
		com.strncat( uiLanGame.gameDescription[i], uiEmptyString, TYPE_LENGTH );
		com.strncat( uiLanGame.gameDescription[i], Info_ValueForKey( info, "numcl" ), MAXCL_LENGTH );
		com.strncat( uiLanGame.gameDescription[i], "\\", MAXCL_LENGTH );
		com.strncat( uiLanGame.gameDescription[i], Info_ValueForKey( info, "maxcl" ), MAXCL_LENGTH );
		com.strncat( uiLanGame.gameDescription[i], uiEmptyString, MAXCL_LENGTH );
		uiLanGame.gameDescriptionPtr[i] = uiLanGame.gameDescription[i];
	}

	for( ; i < UI_MAX_SERVERS; i++ ) uiLanGame.gameDescriptionPtr[i] = NULL;
	uiLanGame.gameList.itemNames = uiLanGame.gameDescriptionPtr;
	uiLanGame.gameList.numItems = 0; // reset it

	if( !uiLanGame.gameList.generic.charHeight ) return; // to avoid divide integer by zero

	// count number of items
	while( uiLanGame.gameList.itemNames[uiLanGame.gameList.numItems] )
		uiLanGame.gameList.numItems++;

	// calculate number of visible rows
	uiLanGame.gameList.numRows = (uiLanGame.gameList.generic.height2 / uiLanGame.gameList.generic.charHeight) - 2;
	if( uiLanGame.gameList.numRows > uiLanGame.gameList.numItems ) uiLanGame.gameList.numRows = uiLanGame.gameList.numItems;
}

/*
=================
UI_Background_Ownerdraw
=================
*/
static void UI_Background_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	UI_DrawPic(item->x, item->y, item->width, item->height, uiColorWhite, ((menuBitmap_s *)self)->pic);

	if( uiStatic.realTime > uiLanGame.refreshTime )
	{
		uiLanGame.refreshTime = uiStatic.realTime + 10000; // refresh every 10 secs
		UI_RefreshServerList();
	}

	// serverinfo has been changed update display
	if( uiStatic.updateServers )
	{
		UI_LanGame_GetGamesList ();
		uiStatic.updateServers = false;
	}
}


/*
=================
UI_LanGame_Callback
=================
*/
static void UI_LanGame_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_JOINGAME:
		if( com.strlen( uiLanGame.gameDescription[uiLanGame.gameList.curItem] ))
		{
			string	text;

			com.snprintf( text, sizeof( text ), "connect %s\n", NET_AdrToString( uiStatic.serverAddresses[uiLanGame.gameList.curItem] ));
			Cbuf_ExecuteText( EXEC_APPEND, text );
		}
		break;
	case ID_CREATEGAME:
		UI_CreateGame_Menu();
		break;
	case ID_GAMEINFO:
		// UNDONE: not implemented
		break;
	case ID_REFRESH:
		UI_RefreshServerList();
		break;
	case ID_DONE:
		UI_PopMenu();
		break;
	}
}

/*
=================
UI_LanGame_Init
=================
*/
static void UI_LanGame_Init( void )
{
	string	libpath;

	Mem_Set( &uiLanGame, 0, sizeof( uiLanGame_t ));

	com.strncat( uiLanGame.hintText, "Game", GAME_LENGTH );
	com.strncat( uiLanGame.hintText, uiEmptyString, GAME_LENGTH );
	com.strncat( uiLanGame.hintText, "Map", MAPNAME_LENGTH );
	com.strncat( uiLanGame.hintText, uiEmptyString, MAPNAME_LENGTH );
	com.strncat( uiLanGame.hintText, "Type", TYPE_LENGTH );
	com.strncat( uiLanGame.hintText, uiEmptyString, TYPE_LENGTH );
	com.strncat( uiLanGame.hintText, "Num/Max Clients", MAXCL_LENGTH );
	com.strncat( uiLanGame.hintText, uiEmptyString, MAXCL_LENGTH );

	uiLanGame.background.generic.id = ID_BACKGROUND;
	uiLanGame.background.generic.type = QMTYPE_BITMAP;
	uiLanGame.background.generic.flags = QMF_INACTIVE;
	uiLanGame.background.generic.x = 0;
	uiLanGame.background.generic.y = 0;
	uiLanGame.background.generic.width = 1024;
	uiLanGame.background.generic.height = 768;
	uiLanGame.background.pic = ART_BACKGROUND;
	uiLanGame.background.generic.ownerdraw = UI_Background_Ownerdraw;

	uiLanGame.banner.generic.id = ID_BANNER;
	uiLanGame.banner.generic.type = QMTYPE_BITMAP;
	uiLanGame.banner.generic.flags = QMF_INACTIVE;
	uiLanGame.banner.generic.x = UI_BANNER_POSX;
	uiLanGame.banner.generic.y = UI_BANNER_POSY;
	uiLanGame.banner.generic.width = UI_BANNER_WIDTH;
	uiLanGame.banner.generic.height = UI_BANNER_HEIGHT;
	uiLanGame.banner.pic = ART_BANNER;

	uiLanGame.joinGame.generic.id = ID_JOINGAME;
	uiLanGame.joinGame.generic.type = QMTYPE_ACTION;
	uiLanGame.joinGame.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiLanGame.joinGame.generic.x = 72;
	uiLanGame.joinGame.generic.y = 230;
	uiLanGame.joinGame.generic.name = "Join game";
	uiLanGame.joinGame.generic.statusText = "Join to selected game";
	uiLanGame.joinGame.generic.callback = UI_LanGame_Callback;

	uiLanGame.createGame.generic.id = ID_CREATEGAME;
	uiLanGame.createGame.generic.type = QMTYPE_ACTION;
	uiLanGame.createGame.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiLanGame.createGame.generic.x = 72;
	uiLanGame.createGame.generic.y = 280;
	uiLanGame.createGame.generic.name = "Create game";
	uiLanGame.createGame.generic.statusText = "Create new LAN game";
	uiLanGame.createGame.generic.callback = UI_LanGame_Callback;

	uiLanGame.gameInfo.generic.id = ID_GAMEINFO;
	uiLanGame.gameInfo.generic.type = QMTYPE_ACTION;
	uiLanGame.gameInfo.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_GRAYED;
	uiLanGame.gameInfo.generic.x = 72;
	uiLanGame.gameInfo.generic.y = 330;
	uiLanGame.gameInfo.generic.name = "View game info";
	uiLanGame.gameInfo.generic.statusText = "Get detail game info";
	uiLanGame.gameInfo.generic.callback = UI_LanGame_Callback;

	uiLanGame.refresh.generic.id = ID_REFRESH;
	uiLanGame.refresh.generic.type = QMTYPE_ACTION;
	uiLanGame.refresh.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiLanGame.refresh.generic.x = 72;
	uiLanGame.refresh.generic.y = 380;
	uiLanGame.refresh.generic.name = "Refresh";
	uiLanGame.refresh.generic.statusText = "Refresh servers list";
	uiLanGame.refresh.generic.callback = UI_LanGame_Callback;

	uiLanGame.done.generic.id = ID_DONE;
	uiLanGame.done.generic.type = QMTYPE_ACTION;
	uiLanGame.done.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiLanGame.done.generic.x = 72;
	uiLanGame.done.generic.y = 430;
	uiLanGame.done.generic.name = "Done";
	uiLanGame.done.generic.statusText = "Return to main menu";
	uiLanGame.done.generic.callback = UI_LanGame_Callback;

	uiLanGame.hintMessage.generic.id = ID_TABLEHINT;
	uiLanGame.hintMessage.generic.type = QMTYPE_ACTION;
	uiLanGame.hintMessage.generic.flags = QMF_INACTIVE|QMF_SMALLFONT;
	uiLanGame.hintMessage.generic.color = uiColorHelp;
	uiLanGame.hintMessage.generic.name = uiLanGame.hintText;
	uiLanGame.hintMessage.generic.x = 360;
	uiLanGame.hintMessage.generic.y = 225;

	uiLanGame.gameList.generic.id = ID_SERVERSLIST;
	uiLanGame.gameList.generic.type = QMTYPE_SCROLLLIST;
	uiLanGame.gameList.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_SMALLFONT;
	uiLanGame.gameList.generic.x = 360;
	uiLanGame.gameList.generic.y = 255;
	uiLanGame.gameList.generic.width = 640;
	uiLanGame.gameList.generic.height = 440;
	uiLanGame.gameList.generic.callback = UI_LanGame_Callback;
	uiLanGame.gameList.itemNames = uiLanGame.gameDescriptionPtr;

	// server.dll needs for reading savefiles or startup newgame
	UI_BuildPath( "server", libpath );
	if( !FS_FileExists( libpath ))
		uiLanGame.createGame.generic.flags |= QMF_GRAYED;	// server.dll is missed - remote servers only

	UI_AddItem( &uiLanGame.menu, (void *)&uiLanGame.background );
	UI_AddItem( &uiLanGame.menu, (void *)&uiLanGame.banner );
	UI_AddItem( &uiLanGame.menu, (void *)&uiLanGame.joinGame );
	UI_AddItem( &uiLanGame.menu, (void *)&uiLanGame.createGame );
	UI_AddItem( &uiLanGame.menu, (void *)&uiLanGame.gameInfo );
	UI_AddItem( &uiLanGame.menu, (void *)&uiLanGame.refresh );
	UI_AddItem( &uiLanGame.menu, (void *)&uiLanGame.done );
	UI_AddItem( &uiLanGame.menu, (void *)&uiLanGame.hintMessage );
	UI_AddItem( &uiLanGame.menu, (void *)&uiLanGame.gameList );

	uiLanGame.refreshTime = uiStatic.realTime + 500; // delay before update 0.5 sec
}

/*
=================
UI_LanGame_Precache
=================
*/
void UI_LanGame_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
}

/*
=================
UI_LanGame_Menu
=================
*/
void UI_LanGame_Menu( void )
{
	UI_LanGame_Precache();
	UI_LanGame_Init();

	UI_PushMenu( &uiLanGame.menu );
}