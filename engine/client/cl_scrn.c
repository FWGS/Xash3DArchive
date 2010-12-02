//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cl_scrn.c - build client frame
//=======================================================================

#include "common.h"
#include "client.h"
#include "qfont.h"

convar_t *scr_viewsize;
convar_t *scr_centertime;
convar_t *scr_printspeed;
convar_t *scr_loading;
convar_t *scr_download;
convar_t *scr_width;
convar_t *scr_height;
convar_t *cl_testlights;
convar_t *cl_allow_levelshots;
convar_t *cl_levelshot_name;
convar_t *cl_envshot_size;
convar_t *scr_dark;

static qboolean scr_init = false;

/*
==============
SCR_DrawFPS
==============
*/
void SCR_DrawFPS( void )
{
	float		calc;
	rgba_t		color;
	static double	nexttime = 0, lasttime = 0;
	static double	framerate = 0;
	static int	framecount = 0;
	double		newtime;
	char		fpsstring[32];

	if( cls.state != ca_active ) return; 
	if( !cl_showfps->integer ) return;
	if( cls.scrshot_action != scrshot_inactive )
		return;
	
	newtime = Sys_DoubleTime();
	if( newtime >= nexttime )
	{
		framerate = framecount / (newtime - lasttime);
		lasttime = newtime;
		nexttime = max( nexttime + 1, lasttime - 1 );
		framecount = 0;
	}

	framecount++;
	calc = framerate;

	if( calc < 1.0f )
	{
		com.snprintf( fpsstring, sizeof( fpsstring ), "%4i spf", (int)(1.0f / calc + 0.5));
		MakeRGBA( color, 255, 0, 0, 255 );
	}
	else
	{
		com.snprintf( fpsstring, sizeof( fpsstring ), "%4i fps", (int)(calc + 0.5));
		MakeRGBA( color, 255, 255, 255, 255 );
          }
	Con_DrawString( scr_width->integer - 68, 4, fpsstring, color );
}

void SCR_NetSpeeds( void )
{
	static char	msg[MAX_SYSPATH];
	int		x, y, height;
	char		*p, *start, *end;
	float		time = sv_time();
	rgba_t		color;

	if( !net_speeds->integer ) return;
	if( cls.state != ca_active ) return; 

	switch( net_speeds->integer )
	{
	case 1:
		if( cls.netchan.compress )
		{
			com.snprintf( msg, sizeof( msg ), "Game Time: %02d:%02d\nTotal received from server:\n Huffman %s\nUncompressed %s\n",
			(int)(time / 60.0f ), (int)fmod( time, 60.0f ), memprint( cls.netchan.total_received ), memprint( cls.netchan.total_received_uncompressed ));
		}
		else
		{
			com.snprintf( msg, sizeof( msg ), "Game Time: %02d:%02d\nTotal received from server:\nUncompressed %s\n",
			(int)(time / 60.0f ), (int)fmod( time, 60.0f ), memprint( cls.netchan.total_received_uncompressed ));
		}
		break;
	case 2:
		if( cls.netchan.compress )
		{
			com.snprintf( msg, sizeof( msg ), "Game Time: %02d:%02d\nTotal sended to server:\nHuffman %s\nUncompressed %s\n",
			(int)(time / 60.0f ), (int)fmod( time, 60.0f ), memprint( cls.netchan.total_sended ), memprint( cls.netchan.total_sended_uncompressed ));
		}
		else
		{
			com.snprintf( msg, sizeof( msg ), "Game Time: %02d:%02d\nTotal sended to server:\nUncompressed %s\n",
			(int)(time / 60.0f ), (int)fmod( time, 60.0f ), memprint( cls.netchan.total_sended_uncompressed ));
		}
		break;
	default: return;
	}

	x = scr_width->integer - 320;
	y = 256;

	Con_DrawStringLen( NULL, NULL, &height );
	MakeRGBA( color, 255, 255, 255, 255 );

	p = start = msg;
	do
	{
		end = com.strchr( p, '\n' );
		if( end ) msg[end-start] = '\0';

		Con_DrawString( x, y, p, color );
		y += height;

		if( end ) p = end + 1;
		else break;
	} while( 1 );
}

/*
================
SCR_RSpeeds
================
*/
void SCR_RSpeeds( void )
{
	char	msg[MAX_SYSPATH];

	if( R_SpeedsMessage( msg, sizeof( msg )))
	{
		int	x, y, height;
		char	*p, *start, *end;
		rgba_t	color;

		x = scr_width->integer - 320;
		y = 64;

		Con_DrawStringLen( NULL, NULL, &height );
		MakeRGBA( color, 255, 255, 255, 255 );

		p = start = msg;
		do
		{
			end = com.strchr( p, '\n' );
			if( end ) msg[end-start] = '\0';

			Con_DrawString( x, y, p, color );
			y += height;

			if( end ) p = end + 1;
			else break;
		} while( 1 );
	}
}

void SCR_MakeLevelShot( void )
{
	if( cls.scrshot_request != scrshot_plaque )
		return;

	// make levelshot at nextframe()
	Cbuf_AddText( "wait 2\nlevelshot\n" );
}

void SCR_MakeScreenShot( void )
{
	qboolean	iRet = false;

	if( host.type == HOST_NORMAL )
		return;	// don't reset action - it will be wait until render initalization is done

	switch( cls.scrshot_action )
	{
	case scrshot_plaque:
		iRet = VID_ScreenShot( cls.shotname, VID_LEVELSHOT );
		break;
	case scrshot_savegame:
	case scrshot_demoshot:
		iRet = VID_ScreenShot( cls.shotname, VID_MINISHOT );
		break;
	case scrshot_envshot:
		iRet = VID_CubemapShot( cls.shotname, cl_envshot_size->integer, cls.envshot_vieworg, false );
		break;
	case scrshot_skyshot:
		iRet = VID_CubemapShot( cls.shotname, cl_envshot_size->integer, cls.envshot_vieworg, true );
		break;
	default: return; // does nothing
	}

	// report
	if( iRet ) MsgDev( D_INFO, "Write %s\n", cls.shotname );
	else MsgDev( D_ERROR, "Unable to write %s\n", cls.shotname );

	cls.envshot_vieworg = NULL;
	cls.scrshot_action = scrshot_inactive;
	cls.shotname[0] = '\0';
}

void SCR_DrawPlaque( void )
{
	shader_t	levelshot;

	if( cl_allow_levelshots->integer && !cls.changelevel )
	{
		levelshot = GL_LoadTexture( cl_levelshot_name->string, NULL, 0, TF_IMAGE );
		GL_SetRenderMode( kRenderNormal );
		R_DrawStretchPic( 0, 0, scr_width->integer, scr_height->integer, 0, 0, 1, 1, levelshot );

		CL_DrawHUD( CL_LOADING );
	}
}

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen( void )
{
	if( V_PreRender( ))
	{
		switch( cls.state )
		{
		case ca_disconnected:
			break;
		case ca_connecting:
		case ca_connected:
			SCR_DrawPlaque();
			break;
		case ca_active:
			V_RenderView();
			CL_DrawHUD( CL_ACTIVE );
			CL_DrawDemoRecording();
			break;
		case ca_cinematic:
			SCR_DrawCinematic();
			break;
		default:
			Host_Error( "SCR_UpdateScreen: bad cls.state\n" );
			break;
		}
		V_PostRender();
          }

	if( !clgame.hInstance ) return;

	clgame.dllFuncs.pfnFrame( cl.time );

	VGui_Paint ();
}

static void SCR_LoadCreditsFont( void )
{
	// setup creditsfont
	if( FS_FileExists( "gfx/creditsfont.fnt" ))
	{
		byte	*buffer;
		size_t	length;
		int	fontWidth;
		qfont_t	*src;
	
		// half-life font with variable chars witdh
		buffer = FS_LoadFile( "gfx/creditsfont.fnt", &length );
		R_GetTextureParms( &fontWidth, NULL, cls.creditsFont.hFontTexture );
	
		if( buffer && length >= sizeof( qfont_t ))
		{
			int	i;
	
			src = (qfont_t *)buffer;
			clgame.scrInfo.iCharHeight = src->rowheight;

			// build rectangles
			for( i = 0; i < 256; i++ )
			{
				cls.creditsFont.fontRc[i].left = (word)src->fontinfo[i].startoffset % fontWidth;
				cls.creditsFont.fontRc[i].right = cls.creditsFont.fontRc[i].left + src->fontinfo[i].charwidth;
				cls.creditsFont.fontRc[i].top = (word)src->fontinfo[i].startoffset / fontWidth;
				cls.creditsFont.fontRc[i].bottom = cls.creditsFont.fontRc[i].top + src->rowheight;
				clgame.scrInfo.charWidths[i] = src->fontinfo[i].charwidth;
			}
			cls.creditsFont.valid = true;
		}
		if( buffer ) Mem_Free( buffer );
	}
}

static void SCR_InstallParticlePalette( void )
{
	rgbdata_t	*pic;
	int	i;

	// NOTE: imagelib required this fakebuffer for loading internal palette
	pic = FS_LoadImage( "#valve.pal", ((byte *)&i), 768 );

	if( pic )
	{
		for( i = 0; i < 256; i++ )
		{
			clgame.palette[i][0] = pic->palette[i*4+0];
			clgame.palette[i][1] = pic->palette[i*4+1];
			clgame.palette[i][2] = pic->palette[i*4+2];
		}
		FS_FreeImage( pic );
	}
	else
	{
		for( i = 0; i < 256; i++ )
		{
			clgame.palette[i][0] = i;
			clgame.palette[i][1] = i;
			clgame.palette[i][2] = i;
		}
		MsgDev( D_WARN, "CL_InstallParticlePalette: failed. Force to grayscale\n" );
	}
}

void SCR_RegisterShaders( void )
{
	cls.fillImage = GL_LoadTexture( "*white", NULL, 0, TF_IMAGE ); // used for FillRGBA
	cls.particleImage = GL_LoadTexture( "*particle", NULL, 0, TF_IMAGE );

	// register gfx.wad images
	cls.pauseIcon = GL_LoadTexture( "gfx.wad/paused", NULL, 0, TF_IMAGE );
	cls.loadingBar = GL_LoadTexture( "gfx.wad/lambda", NULL, 0, TF_IMAGE|TF_LUMINANCE );
	cls.creditsFont.hFontTexture = GL_LoadTexture( "gfx.wad/creditsfont", NULL, 0, TF_IMAGE );
	cls.hChromeSprite = pfnSPR_Load( "sprites/shellchrome.spr" );
}


/*
==================
SCR_VidInit
==================
*/
void SCR_VidInit( void )
{
	Mem_Set( &clgame.ds, 0, sizeof( clgame.ds )); // reset a draw state
	Mem_Set( &menu.ds, 0, sizeof( menu.ds )); // reset a draw state
	Mem_Set( &clgame.centerPrint, 0, sizeof( clgame.centerPrint ));

	// update screen sizes for menu
	menu.globals->scrWidth = scr_width->integer;
	menu.globals->scrHeight = scr_height->integer;

	VGui_Startup ();

	// vid_state has changed
	if( clgame.hInstance ) clgame.dllFuncs.pfnVidInit();
	if( menu.hInstance ) menu.dllFuncs.pfnVidInit();

	// restart console size
	Con_VidInit ();
}

/*
==================
SCR_Init
==================
*/
void SCR_Init( void )
{
	if( scr_init ) return;

	scr_centertime = Cvar_Get( "scr_centertime", "2.5", 0, "centerprint hold time" );
	scr_printspeed = Cvar_Get( "scr_printspeed", "8", 0, "centerprint speed of print" );
	cl_levelshot_name = Cvar_Get( "cl_levelshot_name", MAP_DEFAULT_SHADER, 0, "contains path to current levelshot" );
	cl_allow_levelshots = Cvar_Get( "allow_levelshots", "0", CVAR_ARCHIVE, "allow engine to use indivdual levelshots instead of 'loading' image" );
	scr_loading = Cvar_Get( "scr_loading", "0", 0, "loading bar progress" );
	scr_download = Cvar_Get( "scr_download", "0", 0, "downloading bar progress" );
	cl_testlights = Cvar_Get( "cl_testlights", "0", 0, "test dynamic lights" );
	cl_envshot_size = Cvar_Get( "cl_envshot_size", "256", CVAR_ARCHIVE, "envshot size of cube side" );
	scr_dark = Cvar_Get( "v_dark", "0", 0, "starts level from dark screen" );
	
	// register our commands
	Cmd_AddCommand( "timerefresh", SCR_TimeRefresh_f, "turn quickly and print rendering statistcs" );
	Cmd_AddCommand( "skyname", CL_SetSky_f, "set new skybox by basename" );
	Cmd_AddCommand( "viewpos", SCR_Viewpos_f, "prints current player origin" );

	if( !UI_LoadProgs( va( "%s/MainUI.dll", GI->dll_path ) ))
	{
		Msg( "^1Error: ^7can't initialize MainUI.dll\n" ); // there is non fatal for us
		if( !host.developer ) host.developer = 1; // we need console, because menu is missing
	}

	SCR_RegisterShaders ();
	SCR_LoadCreditsFont ();
	SCR_InstallParticlePalette ();
	SCR_InitCinematic();
	SCR_VidInit();

	if( host.developer && FS_CheckParm( "-toconsole" ))
		Cbuf_AddText( "toggleconsole\n" );
	else UI_SetActiveMenu( true );

	scr_init = true;
}

void SCR_Shutdown( void )
{
	if( !scr_init ) return;

	Cmd_RemoveCommand( "timerefresh" );
	Cmd_RemoveCommand( "skyname" );
	Cmd_RemoveCommand( "viewpos" );

	UI_SetActiveMenu( false );
	SCR_FreeCinematic();
	UI_UnloadProgs();
	scr_init = false;
}