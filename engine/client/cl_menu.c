//=======================================================================
//			Copyright XashXT Group 2010 �
//		        cl_menu.c - gameui dlls interaction
//=======================================================================

#include "common.h"
#include "client.h"
#include "const.h"

gameui_static_t	gameui;

void UI_UpdateMenu( float realtime )
{
	if( !gameui.hInstance ) return;
	gameui.dllFuncs.pfnRedraw( realtime );
}

void UI_KeyEvent( int key, bool down )
{
	if( !gameui.hInstance ) return;
	gameui.dllFuncs.pfnKeyEvent( key, down );
}

void UI_MouseMove( int x, int y )
{
	if( !gameui.hInstance ) return;
	gameui.dllFuncs.pfnMouseMove( x, y );
}

void UI_SetActiveMenu( bool fActive )
{
	if( !gameui.hInstance ) return;
	gameui.dllFuncs.pfnSetActiveMenu( fActive );
}

void UI_AddServerToList( netadr_t adr, const char *info )
{
	if( !gameui.hInstance ) return;
	gameui.dllFuncs.pfnAddServerToList( adr, info );
}

void UI_GetCursorPos( int *pos_x, int *pos_y )
{
	if( !gameui.hInstance ) return;
	gameui.dllFuncs.pfnGetCursorPos( pos_x, pos_y );
}

void UI_SetCursorPos( int pos_x, int pos_y )
{
	if( !gameui.hInstance ) return;
	gameui.dllFuncs.pfnSetCursorPos( pos_x, pos_y );
}

void UI_ShowCursor( bool show )
{
	if( !gameui.hInstance ) return;
	gameui.dllFuncs.pfnShowCursor( show );
}

bool UI_CreditsActive( void )
{
	if( !gameui.hInstance ) return 0;
	return gameui.dllFuncs.pfnCreditsActive();
}

void UI_CharEvent( int key )
{
	if( !gameui.hInstance ) return;
	gameui.dllFuncs.pfnCharEvent( key );
}

bool UI_MouseInRect( void )
{
	if( !gameui.hInstance ) return 1;
	return gameui.dllFuncs.pfnMouseInRect();
}

bool UI_IsVisible( void )
{
	if( !gameui.hInstance ) return 0;
	return gameui.dllFuncs.pfnIsVisible();
}

void Host_Credits( void )
{
	if( !gameui.hInstance ) return;
	gameui.dllFuncs.pfnFinalCredits();
}

static void UI_ConvertGameInfo( GAMEINFO *out, gameinfo_t *in )
{
	com.strncpy( out->gamefolder, in->gamefolder, sizeof( out->gamefolder ));
	com.strncpy( out->startmap, in->startmap, sizeof( out->startmap ));
	com.strncpy( out->trainmap, in->trainmap, sizeof( out->trainmap ));
	com.strncpy( out->title, in->title, sizeof( out->title ));
	com.strncpy( out->version, va( "%g", in->version ), sizeof( out->version ));

	com.strncpy( out->game_url, in->game_url, sizeof( out->game_url ));
	com.strncpy( out->update_url, in->update_url, sizeof( out->update_url ));
	com.strncpy( out->size, com.pretifymem( in->size, 0 ), sizeof( out->size ));
	com.strncpy( out->type, in->type, sizeof( out->type ));
	com.strncpy( out->date, in->date, sizeof( out->date ));

	out->gamemode = in->gamemode;
}

/*
====================
PIC_AdjustSize

draw menupic routine
====================
*/
static void PIC_AdjustSize( float *x, float *y, float *w, float *h )
{
	float	xscale, yscale;

	if( !x && !y && !w && !h ) return;

	// scale for screen sizes
	xscale = gameui.scrInfo.iRealWidth / (float)gameui.scrInfo.iWidth;
	yscale = gameui.scrInfo.iRealHeight / (float)gameui.scrInfo.iHeight;

	if( x ) *x *= xscale;
	if( y ) *y *= yscale;
	if( w ) *w *= xscale;
	if( h ) *h *= yscale;
}

static bool PIC_Scissor( float *x, float *y, float *width, float *height, float *u0, float *v0, float *u1, float *v1 )
{
	float	dudx, dvdy;

	// clip sub rect to sprite
	if(( width == 0 ) || ( height == 0 ))
		return false;

	if( *x + *width <= gameui.ds.scissor_x )
		return false;
	if( *x >= gameui.ds.scissor_x + gameui.ds.scissor_width )
		return false;
	if( *y + *height <= gameui.ds.scissor_y )
		return false;
	if( *y >= gameui.ds.scissor_y + gameui.ds.scissor_height )
		return false;

	dudx = (*u1 - *u0) / *width;
	dvdy = (*v1 - *v0) / *height;

	if( *x < gameui.ds.scissor_x )
	{
		*u0 += (gameui.ds.scissor_x - *x) * dudx;
		*width -= gameui.ds.scissor_x - *x;
		*x = gameui.ds.scissor_x;
	}

	if( *x + *width > gameui.ds.scissor_x + gameui.ds.scissor_width )
	{
		*u1 -= (*x + *width - (gameui.ds.scissor_x + gameui.ds.scissor_width)) * dudx;
		*width = gameui.ds.scissor_x + gameui.ds.scissor_width - *x;
	}

	if( *y < gameui.ds.scissor_y )
	{
		*v0 += (gameui.ds.scissor_y - *y) * dvdy;
		*height -= gameui.ds.scissor_y - *y;
		*y = gameui.ds.scissor_y;
	}

	if( *y + *height > gameui.ds.scissor_y + gameui.ds.scissor_height )
	{
		*v1 -= (*y + *height - (gameui.ds.scissor_y + gameui.ds.scissor_height)) * dvdy;
		*height = gameui.ds.scissor_y + gameui.ds.scissor_height - *y;
	}
	return true;
}

/*
====================
PIC_DrawGeneric

draw hudsprite routine
====================
*/
static void PIC_DrawGeneric( int frame, float x, float y, float width, float height, const wrect_t *prc )
{
	float	s1, s2, t1, t2;
	bool	hasCustomSize;

	if( !re ) return;

	if( width == -1 && height == -1 )
	{
		int	w, h;

		// assume we get sizes from image
		re->GetParms( &w, &h, NULL, frame, gameui.ds.hSprite );

		width = w;
		height = h;
		hasCustomSize = false;
	}
	else hasCustomSize = true;

	if( prc )
	{
		if( hasCustomSize )
		{
			// e.g. quake fonts 
			s1 = (float)(prc->left * 0.001f);
			t1 = (float)(prc->top * 0.001f);
			s2 = (float)(prc->right * 0.001f);
			t2 = (float)(prc->bottom * 0.001f);
		}
		else
		{
			// calc user-defined rectangle
			s1 = (float)prc->left / width;
			t1 = (float)prc->top / height;
			s2 = (float)prc->right / width;
			t2 = (float)prc->bottom / height;
			width = prc->right - prc->left;
			height = prc->bottom - prc->top;
		}
	}
	else
	{
		s1 = t1 = 0.0f;
		s2 = t2 = 1.0f;
	}

	// pass scissor test if supposed
	if( gameui.ds.scissor_test && !PIC_Scissor( &x, &y, &width, &height, &s1, &t1, &s2, &t2 ))
		return;

	// scale for screen sizes
	PIC_AdjustSize( &x, &y, &width, &height );
	re->DrawStretchPic( x, y, width, height, s1, t1, s2, t2, gameui.ds.hSprite );
	re->SetColor( NULL );
}

/*
===============================================================================
	GameUI Builtin Functions

===============================================================================
*/
/*
=========
pfnPIC_Load

=========
*/
static HIMAGE pfnPIC_Load( const char *szPicName )
{
	if( !re ) return 0; // render not initialized
	if( !szPicName || !*szPicName )
	{
		MsgDev( D_ERROR, "CL_LoadImage: bad name!\n" );
		return 0;
	}
	return re->RegisterShader( szPicName, SHADER_NOMIP );
}

/*
=========
pfnPIC_Free

=========
*/
static void pfnPIC_Free( const char *szPicName )
{
	if( re ) re->FreeShader( szPicName );
}

/*
=========
pfnPIC_Frames

=========
*/
static int pfnPIC_Frames( HIMAGE hPic )
{
	int	numFrames;

	if( !re ) return 1;
	re->GetParms( NULL, NULL, &numFrames, 0, hPic );

	return numFrames;
}

/*
=========
pfnPIC_Height

=========
*/
static int pfnPIC_Height( HIMAGE hPic, int frame )
{
	int	picHeight;

	if( !re ) return 0;
	re->GetParms( NULL, &picHeight, NULL, frame, hPic );

	return picHeight;
}

/*
=========
pfnPIC_Width

=========
*/
static int pfnPIC_Width( HIMAGE hPic, int frame )
{
	int	picWidth;

	if( !re ) return 0;
	re->GetParms( &picWidth, NULL, NULL, frame, hPic );

	return picWidth;
}

/*
=========
pfnPIC_Set

=========
*/
static void pfnPIC_Set( HIMAGE hPic, int r, int g, int b, int a )
{
	rgba_t	color;

	if( !re ) return; // render not initialized

	gameui.ds.hSprite = hPic;
	MakeRGBA( color, r, g, b, a );
	re->SetColor( color );
}

/*
=========
pfnPIC_Draw

=========
*/
static void pfnPIC_Draw( int frame, int x, int y, int width, int height, const wrect_t *prc )
{
	if( !re ) return; // render not initialized

	re->SetParms( gameui.ds.hSprite, kRenderNormal, frame );
	PIC_DrawGeneric( frame, x, y, width, height, prc );
}

/*
=========
pfnPIC_DrawTrans

=========
*/
static void pfnPIC_DrawTrans( int frame, int x, int y, int width, int height, const wrect_t *prc )
{
	if( !re ) return; // render not initialized

	re->SetParms( gameui.ds.hSprite, kRenderTransTexture, frame );
	PIC_DrawGeneric( frame, x, y, width, height, prc );
}

/*
=========
pfnPIC_DrawHoles

=========
*/
static void pfnPIC_DrawHoles( int frame, int x, int y, int width, int height, const wrect_t *prc )
{
	if( !re ) return; // render not initialized

	re->SetParms( gameui.ds.hSprite, kRenderTransAlpha, frame );
	PIC_DrawGeneric( frame, x, y, width, height, prc );
}

/*
=========
pfnPIC_DrawAdditive

=========
*/
static void pfnPIC_DrawAdditive( int frame, int x, int y, int width, int height, const wrect_t *prc )
{
	if( !re ) return; // render not initialized

	re->SetParms( gameui.ds.hSprite, kRenderTransAdd, frame );
	PIC_DrawGeneric( frame, x, y, width, height, prc );
}

/*
=========
pfnPIC_EnableScissor

=========
*/
static void pfnPIC_EnableScissor( int x, int y, int width, int height )
{
	// check bounds
	x = bound( 0, x, gameui.scrInfo.iWidth );
	y = bound( 0, y, gameui.scrInfo.iHeight );
	width = bound( 0, width, gameui.scrInfo.iWidth - x );
	height = bound( 0, height, gameui.scrInfo.iHeight - y );

	gameui.ds.scissor_x = x;
	gameui.ds.scissor_width = width;
	gameui.ds.scissor_y = y;
	gameui.ds.scissor_height = height;
	gameui.ds.scissor_test = true;
}

/*
=========
pfnPIC_DisableScissor

=========
*/
static void pfnPIC_DisableScissor( void )
{
	gameui.ds.scissor_x = 0;
	gameui.ds.scissor_width = 0;
	gameui.ds.scissor_y = 0;
	gameui.ds.scissor_height = 0;
	gameui.ds.scissor_test = false;
}

/*
=============
pfnFillRGBA

=============
*/
static void pfnFillRGBA( int x, int y, int width, int height, int r, int g, int b, int a )
{
	rgba_t	color;

	if( !re ) return;

	MakeRGBA( color, r, g, b, a );
	re->SetColor( color );

	PIC_AdjustSize( (float *)&x, (float *)&y, (float *)&width, (float *)&height );

	re->SetParms( cls.fillShader, kRenderTransTexture, 0 );
	re->DrawStretchPic( x, y, width, height, 0, 0, 1, 1, cls.fillShader );
	re->SetColor( NULL );
}

/*
=============
pfnGetScreenInfo

get actual screen info
=============
*/
static int pfnGetScreenInfo( SCREENINFO *pscrinfo )
{
	// setup screen info
	gameui.scrInfo.iRealWidth = scr_width->integer;
	gameui.scrInfo.iRealHeight = scr_height->integer;

	if( pscrinfo && pscrinfo->iFlags & SCRINFO_VIRTUALSPACE )
	{
		// virtual screen space 1024x768
		gameui.scrInfo.iWidth = 1024;
		gameui.scrInfo.iHeight = 768;
		gameui.scrInfo.iFlags |= SCRINFO_VIRTUALSPACE;
	}
	else
	{
		gameui.scrInfo.iWidth = scr_width->integer;
		gameui.scrInfo.iHeight = scr_height->integer;
		gameui.scrInfo.iFlags &= ~SCRINFO_VIRTUALSPACE;
	}

	if( !pscrinfo ) return 0;
	*pscrinfo = gameui.scrInfo;	// copy screeninfo out

	return 1;
}

/*
=============
pfnClientCmd

=============
*/
static void pfnClientCmd( int execute_now, const char *szCmdString )
{
	int	when;

	if( execute_now )
		when = EXEC_NOW;
	else when = EXEC_APPEND;

	// client command executes immediately
	Cbuf_ExecuteText( when, szCmdString );
}

/*
=============
pfnPlaySound

=============
*/
static void pfnPlaySound( const char *szSound )
{
	S_StartLocalSound( szSound );
}

/*
=============
pfnDrawCharacter

returns drawed chachter width (in real screen pixels)
=============
*/
static int pfnDrawCharacter( int x, int y, int number, int r, int g, int b )
{
	number &= 255;

	if( !re ) return 0;
	if( number < 32 ) return 0;
	if( y < -gameui.scrInfo.iCharHeight )
		return 0;

	if( gameui.creditsFont.use_qfont )
	{
		pfnPIC_Set( gameui.creditsFont.hFontTexture, r, g, b, 255 );
		pfnPIC_DrawAdditive( 0, x, y, -1, -1, &gameui.creditsFont.fontRc[number] );
	}
	else
	{
		float	size, frow, fcol;
		float	ax, ay, aw, ah;
		int	fontWidth, fontHeight;
		rgba_t	color;

		ax = x;
		ay = y;
		aw = gameui.scrInfo.charWidths[number];
		ah = gameui.scrInfo.iCharHeight;

		re->GetParms( &fontWidth, &fontHeight, NULL, 0, gameui.creditsFont.hFontTexture );
		PIC_AdjustSize( &ax, &ay, &aw, &ah );

		MakeRGBA( color, r, g, b, 255 );
		re->SetColor( color );
	
		frow = (number >> 4) * 0.0625f + (0.5f / (float)fontWidth);
		fcol = (number & 15) * 0.0625f + (0.5f / (float)fontHeight);
		size = 0.0625f - (1.0f / (float)fontWidth);

		re->SetParms( gameui.creditsFont.hFontTexture, kRenderTransAdd, 0 );
		re->DrawStretchPic( ax, ay, aw, ah, fcol, frow, fcol + size, frow + size, gameui.creditsFont.hFontTexture );
	}

	re->SetColor( NULL ); // don't forget reset color
	return gameui.scrInfo.charWidths[number];
}

/*
=============
pfnDrawConsoleString

drawing string like a console string 
=============
*/
static int pfnDrawConsoleString( int x, int y, char *string )
{
	if( !string || !*string ) return 0; // silent ignore
	SCR_DrawSmallStringExt( x, y, string, NULL, false );
	return com.cstrlen( string ) * SMALLCHAR_WIDTH; // not includes color prexfixes
}

/*
=============
pfnDrawSetTextColor

set color for anything
=============
*/
static void pfnDrawSetTextColor( float r, float g, float b )
{
	rgba_t	color;

	// bound color and convert to byte
	color[0] = (byte)bound( 0, r * 255, 255 );
	color[1] = (byte)bound( 0, g * 255, 255 );
	color[2] = (byte)bound( 0, b * 255, 255 );
	color[3] = (byte)0xFF;
	if( re ) re->SetColor( color );
}

/*
=============
pfnDrawConsoleStringLen

returns width and height (in real pixels)
for specified string
=============
*/
static void pfnDrawConsoleStringLen(  const char *string, int *length, int *height )
{
	// console used fixed font size
	if( length ) *length = com.cstrlen( string ) * SMALLCHAR_WIDTH;
	if( height ) *height = SMALLCHAR_HEIGHT;
}

/*
====================
pfnGetPlayerModel

for drawing playermodel previews
====================
*/
static edict_t* pfnGetPlayerModel( void )
{
	return CL_GetEdictByIndex( MAX_EDICTS - 1 );
}

/*
====================
pfnSetPlayerModel

for drawing playermodel previews
====================
*/
static void pfnSetPlayerModel( edict_t *ent, const char *path )
{
	if( !CL_IsValidEdict( ent )) Host_Error( "UI_SetPlayerModel: bad edict\n" );

	ent->v.model = re->RegisterModel( path, MAX_MODELS - 1 );
	ent->v.modelindex = MAX_MODELS - 1;

	// setup latched controllers state to avoid unexpected poses :)
	ent->pvClientData->frame.curstate.controller[0] = 127;
	ent->pvClientData->frame.curstate.controller[1] = 127;
	ent->pvClientData->frame.curstate.controller[2] = 127;
	ent->pvClientData->frame.curstate.controller[3] = 127;
	ent->pvClientData->frame.latched.controller[0] = 127;
	ent->pvClientData->frame.latched.controller[1] = 127;
	ent->pvClientData->frame.latched.controller[2] = 127;
	ent->pvClientData->frame.latched.controller[3] = 127;
}

/*
====================
pfnRenderScene

for drawing playermodel previews
====================
*/
static void pfnRenderScene( const ref_params_t *fd )
{
	if( !re || !fd ) return;
	re->RenderFrame( fd );
}

/*
====================
pfnClientJoin

send client connect
====================
*/
static void pfnClientJoin( const netadr_t adr )
{
	Cbuf_ExecuteText( EXEC_APPEND, va( "connect %s\n", NET_AdrToString( adr )));
}

/*
====================
pfnKeyGetOverstrikeMode

get global key overstrike state
====================
*/
static int pfnKeyGetOverstrikeMode( void )
{
	return host.key_overstrike;
}

/*
====================
pfnKeySetOverstrikeMode

set global key overstrike mode
====================
*/
static void pfnKeySetOverstrikeMode( int fActive )
{
	host.key_overstrike = fActive;
}

/*
=========
pfnMemAlloc

=========
*/
static void *pfnMemAlloc( size_t cb, const char *filename, const int fileline )
{
	return com.malloc( gameui.mempool, cb, filename, fileline );
}

/*
=========
pfnMemFree

=========
*/
static void pfnMemFree( void *mem, const char *filename, const int fileline )
{
	com.free( mem, filename, fileline );
}

/*
=========
pfnGetGameInfo

=========
*/
static int pfnGetGameInfo( GAMEINFO *pgameinfo )
{
	if( !pgameinfo ) return 0;

	*pgameinfo = gameui.gameInfo;
	return 1;
}

/*
=========
pfnGetGamesList

=========
*/
static GAMEINFO **pfnGetGamesList( int *numGames )
{
	if( numGames ) *numGames = SI->numgames;
	return gameui.modsInfo;
}

/*
=========
pfnGetFilesList

release prev search on a next call
=========
*/
static char **pfnGetFilesList( const char *pattern, int *numFiles )
{
	static search_t	*t = NULL;

	if( t ) Mem_Free( t ); // release prev search

	t = FS_Search( pattern, true );
	if( !t )
	{
		if( numFiles ) *numFiles = 0;
		return NULL;
	}

	if( numFiles ) *numFiles = t->numfilenames;
	return t->filenames;
}

/*
=========
pfnGetVideoList

retuns list of valid video renderers
=========
*/
static char **pfnGetVideoList( int *numRenders )
{
	if( numRenders )
		*numRenders = host.num_video_dlls;
	return host.video_dlls;
}

/*
=========
pfnGetAudioList

retuns list of valid audio renderers
=========
*/
static char **pfnGetAudioList( int *numRenders )
{
	if( numRenders )
		*numRenders = host.num_audio_dlls;
	return host.audio_dlls;
}

/*
=========
pfnGetClipboardData

pointer must be released in call place
=========
*/
static char *pfnGetClipboardData( void )
{
	return Sys_GetClipboardData();
}

/*
=========
pfnShellExecute

=========
*/
static void pfnShellExecute( const char *name, const char *args, bool closeEngine )
{
	Sys_ShellExecute( name, args, closeEngine );
}

/*
=========
pfnChangeInstance

=========
*/
static void pfnChangeInstance( const char *newInstance, const char *szFinalMessage )
{
	if( !szFinalMessage ) szFinalMessage = "";
	if( !newInstance || !*newInstance ) return;

	Sys_NewInstance( newInstance, szFinalMessage );
}

/*
=========
pfnChangeVideo

=========
*/
static void pfnChangeVideo( const char *dllName )
{
	if( !dllName || !*dllName ) return;

	// video subsystem will be automatically restarted on nextframe
	Cvar_FullSet( "host_video", dllName, CVAR_SYSTEMINFO );
	Cbuf_ExecuteText( EXEC_APPEND, "vid_restart\n" );
}

/*
=========
pfnChangeAudio

=========
*/
static void pfnChangeAudio( const char *dllName )
{
	if( !dllName || !*dllName ) return;

	// sound subsystem will be automatically restarted on nextframe
	Cvar_FullSet( "host_audio", dllName, CVAR_SYSTEMINFO );
	Cbuf_ExecuteText( EXEC_APPEND, "snd_restart\n" );
}

/*
=========
pfnHostNewGame

=========
*/
static void pfnHostNewGame( const char *mapName )
{
	if( !mapName || !*mapName ) return;
	Host_NewGame( mapName, false );
}

/*
=========
pfnHostEndGame

=========
*/
static void pfnHostEndGame( const char *szFinalMessage )
{
	if( !szFinalMessage ) szFinalMessage = "";
	Host_EndGame( szFinalMessage );
}

// engine callbacks
static ui_enginefuncs_t gEngfuncs = 
{
	pfnPIC_Load,
	pfnPIC_Free,
	pfnPIC_Frames,
	pfnPIC_Height,
	pfnPIC_Width,
	pfnPIC_Set,
	pfnPIC_Draw,
	pfnPIC_DrawHoles,
	pfnPIC_DrawTrans,
	pfnPIC_DrawAdditive,
	pfnPIC_EnableScissor,
	pfnPIC_DisableScissor,
	pfnFillRGBA,
	pfnGetScreenInfo,
	pfnCVarRegister,
	pfnCVarGetValue,
	pfnCVarGetString,
	pfnCVarSetString,
	pfnCVarSetValue,
	pfnAddCommand,
	pfnClientCmd,
	pfnDelCommand,
	pfnCmd_Argc,
	pfnCmd_Argv,
	pfnCmd_Args,
	pfnAlertMessage,
	pfnPlaySound,
	pfnDrawCharacter,
	pfnDrawConsoleString,
	pfnDrawSetTextColor,
	pfnDrawConsoleStringLen,
	pfnGetPlayerModel,
	pfnSetPlayerModel,
	V_ClearScene,	
	pfnRenderScene,
	CL_AddEntity,
	pfnLoadLibrary,
	pfnGetProcAddress,
	pfnFreeLibrary,
	Host_Error,
	pfnFileExists,
	pfnGetGameDir,
	VGui_GetPanel,
	VGui_ViewportPaintBackground,
	Cmd_CheckMapsList,
	CL_Active,
	pfnClientJoin,
	pfnLoadFile,
	pfnParseToken,
	pfnFreeFile,
	Key_ClearStates,
	Key_SetKeyDest,
	Key_KeynumToString,
	Key_GetBinding,
	Key_SetBinding,
	Key_IsDown,
	pfnKeyGetOverstrikeMode,
	pfnKeySetOverstrikeMode,
	pfnMemAlloc,
	pfnMemFree,
	pfnGetGameInfo,
	pfnGetGamesList,
	pfnGetFilesList,
	pfnGetVideoList,
	pfnGetAudioList,
	SV_GetComment,
	CL_GetComment,
	pfnGetClipboardData,
	pfnShellExecute,
	Host_WriteServerConfig,
	pfnChangeInstance,
	pfnChangeVideo,
	pfnChangeAudio,
	pfnHostNewGame,
	pfnHostEndGame,
};

void UI_UnloadProgs( void )
{
	if( !gameui.hInstance ) return;

	// deinitialize game
	gameui.dllFuncs.pfnShutdown();

	FS_FreeLibrary( gameui.hInstance );
	Mem_FreePool( &gameui.mempool );
	Mem_Set( &gameui, 0, sizeof( gameui ));
}

bool UI_LoadProgs( const char *name )
{
	static GAMEUIAPI		GetMenuAPI;
	static ui_globalvars_t	gpGlobals;
	int			i;

	if( gameui.hInstance ) UI_UnloadProgs();

	// setup globals
	gameui.globals = &gpGlobals;

	gameui.mempool = Mem_AllocPool( "GameUI Pool" );
	gameui.hInstance = FS_LoadLibrary( name, false );
	if( !gameui.hInstance ) return false;

	GetMenuAPI = (GAMEUIAPI)FS_GetProcAddress( gameui.hInstance, "CreateAPI" );

	if( !GetMenuAPI )
	{
		FS_FreeLibrary( gameui.hInstance );
          	MsgDev( D_NOTE, "UI_LoadProgs: failed to get address of CreateAPI proc\n" );
		gameui.hInstance = NULL;
		return false;
	}

	if( !GetMenuAPI( &gameui.dllFuncs, &gEngfuncs, gameui.globals ))
	{
		FS_FreeLibrary( gameui.hInstance );
		MsgDev( D_NOTE, "UI_LoadProgs: can't init client API\n" );
		gameui.hInstance = NULL;
		return false;
	}

	// setup gameinfo
	for( i = 0; i < SI->numgames; i++ )
	{
		gameui.modsInfo[i] = Mem_Alloc( gameui.mempool, sizeof( GAMEINFO ));
		UI_ConvertGameInfo( gameui.modsInfo[i], SI->games[i] );
	}

	UI_ConvertGameInfo( &gameui.gameInfo, SI->GameInfo ); // current gameinfo

	// setup globals
	gameui.globals->developer = host.developer;
	com.strncpy( gameui.globals->shot_ext, SI->savshot_ext, sizeof( gameui.globals->shot_ext ));

	// initialize game
	gameui.dllFuncs.pfnInit();

	return true;
}