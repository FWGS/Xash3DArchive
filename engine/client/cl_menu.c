//=======================================================================
//			Copyright XashXT Group 2010 ©
//		        cl_menu.c - menu dlls interaction
//=======================================================================

#include "common.h"
#include "client.h"
#include "const.h"

menu_static_t	menu;

void UI_UpdateMenu( float realtime )
{
	if( !menu.hInstance ) return;
	menu.dllFuncs.pfnRedraw( realtime );
}

void UI_KeyEvent( int key, qboolean down )
{
	if( !menu.hInstance ) return;
	menu.dllFuncs.pfnKeyEvent( key, down );
}

void UI_MouseMove( int x, int y )
{
	if( !menu.hInstance ) return;
	menu.dllFuncs.pfnMouseMove( x, y );
}

void UI_SetActiveMenu( qboolean fActive )
{
	movie_state_t	*cin_state;

	if( !menu.hInstance ) return;
	menu.drawLogo = fActive;
	menu.dllFuncs.pfnSetActiveMenu( fActive );

	if( !fActive )
	{
		// close logo when menu is shutdown
		cin_state = AVI_GetState( CIN_LOGO );
		AVI_CloseVideo( cin_state );
	}
}

void UI_AddServerToList( netadr_t adr, const char *info )
{
	if( !menu.hInstance ) return;
	menu.dllFuncs.pfnAddServerToList( adr, info );
}

void UI_GetCursorPos( int *pos_x, int *pos_y )
{
	if( !menu.hInstance ) return;
	menu.dllFuncs.pfnGetCursorPos( pos_x, pos_y );
}

void UI_SetCursorPos( int pos_x, int pos_y )
{
	if( !menu.hInstance ) return;
	menu.dllFuncs.pfnSetCursorPos( pos_x, pos_y );
}

void UI_ShowCursor( qboolean show )
{
	if( !menu.hInstance ) return;
	menu.dllFuncs.pfnShowCursor( show );
}

qboolean UI_CreditsActive( void )
{
	if( !menu.hInstance ) return 0;
	return menu.dllFuncs.pfnCreditsActive();
}

void UI_CharEvent( int key )
{
	if( !menu.hInstance ) return;
	menu.dllFuncs.pfnCharEvent( key );
}

qboolean UI_MouseInRect( void )
{
	if( !menu.hInstance ) return 1;
	return menu.dllFuncs.pfnMouseInRect();
}

qboolean UI_IsVisible( void )
{
	if( !menu.hInstance ) return 0;
	return menu.dllFuncs.pfnIsVisible();
}

static void UI_DrawLogo( const char *filename, float x, float y, float width, float height )
{
	static float	video_duration;
	static float	cin_time;
	static int	last_frame = -1;
	byte		*cin_data = NULL;
	movie_state_t	*cin_state;
	int		cin_frame;
	qboolean		redraw = false;

	if( !menu.drawLogo ) return;
	cin_state = AVI_GetState( CIN_LOGO );

	if( !AVI_IsActive( cin_state ))
	{
		string		path;
		const char	*fullpath;
	
		// run cinematic if not
		com.snprintf( path, sizeof( path ), "media/%s", filename );
		FS_DefaultExtension( path, ".avi" );
		fullpath = FS_GetDiskPath( path );

		if( FS_FileExists( path ) && !fullpath )
		{
			MsgDev( D_ERROR, "couldn't load %s from packfile. Please extract it\n", path );
			menu.drawLogo = false;
			return;
		}

		AVI_OpenVideo( cin_state, fullpath, false, false, true );
		if( !( AVI_GetVideoInfo( cin_state, &menu.logo_xres, &menu.logo_yres, &video_duration )))
		{
			AVI_CloseVideo( cin_state );
			menu.drawLogo = false;
			return;
		}

		cin_time = 0.0f;
		last_frame = -1;
	}

	if( width <= 0 || height <= 0 )
	{
		// precache call, don't draw
		cin_time = 0.0f;
		last_frame = -1;
		return;
	}

	// advances cinematic time (ignores maxfps and host_framerate settings)
	cin_time += host.realframetime;

	// restarts the cinematic
	if( cin_time > video_duration )
		cin_time = 0.0f;

	// read the next frame
	cin_frame = AVI_GetVideoFrameNumber( cin_state, cin_time );

	if( cin_frame != last_frame )
	{
		cin_data = AVI_GetVideoFrame( cin_state, cin_frame );
		last_frame = cin_frame;
		redraw = true;
	}

	re->DrawStretchRaw( x, y, width, height, menu.logo_xres, menu.logo_yres, cin_data, redraw );
}

static int UI_GetLogoWidth( void )
{
	return menu.logo_xres;
}

static int UI_GetLogoHeight( void )
{
	return menu.logo_yres;
}
	
void Host_Credits( void )
{
	if( !menu.hInstance ) return;
	menu.dllFuncs.pfnFinalCredits();
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

static qboolean PIC_Scissor( float *x, float *y, float *width, float *height, float *u0, float *v0, float *u1, float *v1 )
{
	float	dudx, dvdy;

	// clip sub rect to sprite
	if(( width == 0 ) || ( height == 0 ))
		return false;

	if( *x + *width <= menu.ds.scissor_x )
		return false;
	if( *x >= menu.ds.scissor_x + menu.ds.scissor_width )
		return false;
	if( *y + *height <= menu.ds.scissor_y )
		return false;
	if( *y >= menu.ds.scissor_y + menu.ds.scissor_height )
		return false;

	dudx = (*u1 - *u0) / *width;
	dvdy = (*v1 - *v0) / *height;

	if( *x < menu.ds.scissor_x )
	{
		*u0 += (menu.ds.scissor_x - *x) * dudx;
		*width -= menu.ds.scissor_x - *x;
		*x = menu.ds.scissor_x;
	}

	if( *x + *width > menu.ds.scissor_x + menu.ds.scissor_width )
	{
		*u1 -= (*x + *width - (menu.ds.scissor_x + menu.ds.scissor_width)) * dudx;
		*width = menu.ds.scissor_x + menu.ds.scissor_width - *x;
	}

	if( *y < menu.ds.scissor_y )
	{
		*v0 += (menu.ds.scissor_y - *y) * dvdy;
		*height -= menu.ds.scissor_y - *y;
		*y = menu.ds.scissor_y;
	}

	if( *y + *height > menu.ds.scissor_y + menu.ds.scissor_height )
	{
		*v1 -= (*y + *height - (menu.ds.scissor_y + menu.ds.scissor_height)) * dvdy;
		*height = menu.ds.scissor_y + menu.ds.scissor_height - *y;
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

	if( !re ) return;

	if( width == -1 && height == -1 )
	{
		int	w, h;

		// assume we get sizes from image
		re->GetParms( &w, &h, NULL, frame, menu.ds.hSprite );

		width = w;
		height = h;
	}

	if( prc )
	{
		// calc user-defined rectangle
		s1 = (float)prc->left / width;
		t1 = (float)prc->top / height;
		s2 = (float)prc->right / width;
		t2 = (float)prc->bottom / height;
		width = prc->right - prc->left;
		height = prc->bottom - prc->top;
	}
	else
	{
		s1 = t1 = 0.0f;
		s2 = t2 = 1.0f;
	}

	// pass scissor test if supposed
	if( menu.ds.scissor_test && !PIC_Scissor( &x, &y, &width, &height, &s1, &t1, &s2, &t2 ))
		return;

	re->DrawStretchPic( x, y, width, height, s1, t1, s2, t2, menu.ds.hSprite );
	re->SetColor( NULL );
}

/*
===============================================================================
	MainUI Builtin Functions

===============================================================================
*/
/*
=========
pfnPIC_Load

=========
*/
static HIMAGE pfnPIC_Load( const char *szPicName, const byte *image_buf, long image_size )
{
	if( !re ) return 0; // render not initialized
	if( !szPicName || !*szPicName )
	{
		MsgDev( D_ERROR, "CL_LoadImage: bad name!\n" );
		return 0;
	}

	if( image_buf && image_size > 0 )
		return re->RegisterShaderInt( szPicName, image_buf, image_size );
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

	menu.ds.hSprite = hPic;
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

	re->SetParms( menu.ds.hSprite, kRenderNormal, frame );
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

	re->SetParms( menu.ds.hSprite, kRenderTransTexture, frame );
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

	re->SetParms( menu.ds.hSprite, kRenderTransAlpha, frame );
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

	re->SetParms( menu.ds.hSprite, kRenderTransAdd, frame );
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
	x = bound( 0, x, menu.globals->scrWidth );
	y = bound( 0, y, menu.globals->scrHeight );
	width = bound( 0, width, menu.globals->scrWidth - x );
	height = bound( 0, height, menu.globals->scrHeight - y );

	menu.ds.scissor_x = x;
	menu.ds.scissor_width = width;
	menu.ds.scissor_y = y;
	menu.ds.scissor_height = height;
	menu.ds.scissor_test = true;
}

/*
=========
pfnPIC_DisableScissor

=========
*/
static void pfnPIC_DisableScissor( void )
{
	menu.ds.scissor_x = 0;
	menu.ds.scissor_width = 0;
	menu.ds.scissor_y = 0;
	menu.ds.scissor_height = 0;
	menu.ds.scissor_test = false;
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
	re->SetParms( cls.fillShader, kRenderTransTexture, 0 );
	re->DrawStretchPic( x, y, width, height, 0, 0, 1, 1, cls.fillShader );
	re->SetColor( NULL );
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
	if( !szSound || !*szSound ) return;
	S_StartLocalSound( szSound );
}

/*
=============
pfnDrawCharacter

quakefont draw character
=============
*/
static void pfnDrawCharacter( int x, int y, int width, int height, int ch, int ulRGBA, HIMAGE hFont )
{
	rgba_t	color;
	float	row, col, size;

	ch &= 255;

	if( ch == ' ') return;
	if( y < -height ) return;

	color[3] = (ulRGBA & 0xFF000000) >> 24;
	color[0] = (ulRGBA & 0xFF0000) >> 16;
	color[1] = (ulRGBA & 0xFF00) >> 8;
	color[2] = (ulRGBA & 0xFF) >> 0;
	re->SetColor( color );

	col = (ch & 15) * 0.0625 + (0.5f / 256.0f);
	row = (ch >> 4) * 0.0625 + (0.5f / 256.0f);
	size = 0.0625f - (1.0f / 256.0f);

	re->SetParms( cls.creditsFont.hFontTexture, kRenderTransTexture, 0 );
	re->DrawStretchPic( x, y, width, height, col, row, col + size, row + size, hFont );
	re->SetColor( NULL );
}

/*
=============
pfnDrawConsoleString

drawing string like a console string 
=============
*/
static int pfnDrawConsoleString( int x, int y, const char *string )
{
	int	drawLen;

	if( !string || !*string ) return 0; // silent ignore
	drawLen = Con_DrawString( x, y, string, menu.ds.textColor );
	MakeRGBA( menu.ds.textColor, 255, 255, 255, 255 );

	return drawLen; // exclude color prexfixes
}

/*
=============
pfnDrawSetTextColor

set color for anything
=============
*/
static void pfnDrawSetTextColor( int r, int g, int b, int alpha )
{
	// bound color and convert to byte
	menu.ds.textColor[0] = r;
	menu.ds.textColor[1] = g;
	menu.ds.textColor[2] = b;
	menu.ds.textColor[3] = alpha;
}

/*
====================
pfnGetPlayerModel

for drawing playermodel previews
====================
*/
static cl_entity_t* pfnGetPlayerModel( void )
{
	return &menu.playermodel;
}

/*
====================
pfnSetPlayerModel

for drawing playermodel previews
====================
*/
static void pfnSetPlayerModel( cl_entity_t *ent, const char *path )
{
	re->RegisterModel( path, MAX_MODELS - 1 );
	ent->curstate.modelindex = MAX_MODELS - 1;
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
	if( fd->fov_x <= 0.0f || fd->fov_y <= 0.0f ) return;
	re->RenderFrame( fd, false );
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
	return com.malloc( menu.mempool, cb, filename, fileline );
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

	*pgameinfo = menu.gameInfo;
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
	return menu.modsInfo;
}

/*
=========
pfnGetFilesList

release prev search on a next call
=========
*/
static char **pfnGetFilesList( const char *pattern, int *numFiles, int gamedironly )
{
	static search_t	*t = NULL;

	if( t ) Mem_Free( t ); // release prev search

	t = FS_SearchExt( pattern, true, gamedironly );
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
	static const char	*sndlib = "default";

	if( numRenders ) *numRenders = 1;
	return &(char *)sndlib;
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
pfnCheckGameDll

=========
*/
int pfnCheckGameDll( void )
{
	void	*hInst;

	if( SV_Active( )) return true;

	if(( hInst = FS_LoadLibrary( GI->game_dll, true )) != NULL )
	{
		FS_FreeLibrary( hInst );
		return true;
	}
	return false;
}

/*
=========
pfnShellExecute

=========
*/
static void pfnShellExecute( const char *name, const char *args, qboolean closeEngine )
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
	Cvar_FullSet( "host_video", dllName, CVAR_INIT|CVAR_ARCHIVE );
	Cbuf_ExecuteText( EXEC_APPEND, "vid_restart\n" );
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
	pfnCvar_RegisterVariable,
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
	Con_Printf,
	Con_DPrintf,
	pfnPlaySound,
	UI_DrawLogo,
	UI_GetLogoWidth,
	UI_GetLogoHeight,
	pfnDrawCharacter,
	pfnDrawConsoleString,
	pfnDrawSetTextColor,
	Con_DrawStringLen,
	Con_DefaultColor,
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
	COM_ParseFile,
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
	pfnCheckGameDll,
	pfnGetClipboardData,
	pfnShellExecute,
	Host_WriteServerConfig,
	pfnChangeInstance,
	pfnChangeVideo,
	S_StartBackgroundTrack,
	pfnHostEndGame,
};

void UI_UnloadProgs( void )
{
	if( !menu.hInstance ) return;

	// deinitialize game
	menu.dllFuncs.pfnShutdown();

	FS_FreeLibrary( menu.hInstance );
	Mem_FreePool( &menu.mempool );
	Mem_Set( &menu, 0, sizeof( menu ));
}

qboolean UI_LoadProgs( const char *name )
{
	static MENUAPI		GetMenuAPI;
	static ui_globalvars_t	gpGlobals;
	int			i;

	if( menu.hInstance ) UI_UnloadProgs();

	// setup globals
	menu.globals = &gpGlobals;

	menu.mempool = Mem_AllocPool( "Menu Pool" );
	menu.hInstance = FS_LoadLibrary( name, false );
	if( !menu.hInstance ) return false;

	GetMenuAPI = (MENUAPI)FS_GetProcAddress( menu.hInstance, "GetMenuAPI" );

	if( !GetMenuAPI )
	{
		FS_FreeLibrary( menu.hInstance );
          	MsgDev( D_NOTE, "UI_LoadProgs: failed to get address of GetMenuAPI proc\n" );
		menu.hInstance = NULL;
		return false;
	}

	if( !GetMenuAPI( &menu.dllFuncs, &gEngfuncs, menu.globals ))
	{
		FS_FreeLibrary( menu.hInstance );
		MsgDev( D_NOTE, "UI_LoadProgs: can't init client API\n" );
		menu.hInstance = NULL;
		return false;
	}

	// setup gameinfo
	for( i = 0; i < SI->numgames; i++ )
	{
		menu.modsInfo[i] = Mem_Alloc( menu.mempool, sizeof( GAMEINFO ));
		UI_ConvertGameInfo( menu.modsInfo[i], SI->games[i] );
	}

	UI_ConvertGameInfo( &menu.gameInfo, SI->GameInfo ); // current gameinfo

	// setup globals
	menu.globals->developer = host.developer;
	com.strncpy( menu.globals->shotExt, SI->savshot_ext, sizeof( menu.globals->shotExt ));

	// initialize game
	menu.dllFuncs.pfnInit();

	return true;
}