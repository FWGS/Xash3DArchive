//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        cl_game.c - client dlls interaction
//=======================================================================

#include "common.h"
#include "client.h"
#include "matrix_lib.h"
#include "const.h"
#include "triangleapi.h"
#include "r_efx.h"
#include "net_api.h"
#include "demo_api.h"
#include "event_flags.h"
#include "ivoicetweak.h"
#include "pm_local.h"
#include "cl_tent.h"
#include "input.h"
#include "shake.h"
#include "sprite.h"
#include "gl_local.h"

#define MAX_TEXTCHANNELS	8		// must be power of two (GoldSrc uses 4 channel)
#define TEXT_MSGNAME	"TextMessage%i"

char			cl_textbuffer[MAX_TEXTCHANNELS][512];
client_textmessage_t	cl_textmessage[MAX_TEXTCHANNELS];

static dllfunc_t cdll_exports[] =
{
{ "Initialize", (void **)&clgame.dllFuncs.pfnInitialize },
{ "HUD_VidInit", (void **)&clgame.dllFuncs.pfnVidInit },
{ "HUD_Init", (void **)&clgame.dllFuncs.pfnInit },
{ "HUD_Shutdown", (void **)&clgame.dllFuncs.pfnShutdown },
{ "HUD_Redraw", (void **)&clgame.dllFuncs.pfnRedraw },
{ "HUD_UpdateClientData", (void **)&clgame.dllFuncs.pfnUpdateClientData },
{ "HUD_Reset", (void **)&clgame.dllFuncs.pfnReset },
{ "HUD_PlayerMove", (void **)&clgame.dllFuncs.pfnPlayerMove },
{ "HUD_PlayerMoveInit", (void **)&clgame.dllFuncs.pfnPlayerMoveInit },
{ "HUD_PlayerMoveTexture", (void **)&clgame.dllFuncs.pfnPlayerMoveTexture },
{ "HUD_ConnectionlessPacket", (void **)&clgame.dllFuncs.pfnConnectionlessPacket },
{ "HUD_GetHullBounds", (void **)&clgame.dllFuncs.pfnGetHullBounds },
{ "HUD_Frame", (void **)&clgame.dllFuncs.pfnFrame },
{ "HUD_PostRunCmd", (void **)&clgame.dllFuncs.pfnPostRunCmd },
{ "HUD_Key_Event", (void **)&clgame.dllFuncs.pfnKey_Event },
{ "HUD_AddEntity", (void **)&clgame.dllFuncs.pfnAddEntity },
{ "HUD_CreateEntities", (void **)&clgame.dllFuncs.pfnCreateEntities },
{ "HUD_StudioEvent", (void **)&clgame.dllFuncs.pfnStudioEvent },
{ "HUD_TxferLocalOverrides", (void **)&clgame.dllFuncs.pfnTxferLocalOverrides },
{ "HUD_ProcessPlayerState", (void **)&clgame.dllFuncs.pfnProcessPlayerState },
{ "HUD_TxferPredictionData", (void **)&clgame.dllFuncs.pfnTxferPredictionData },
{ "HUD_TempEntUpdate", (void **)&clgame.dllFuncs.pfnTempEntUpdate },
{ "HUD_DrawNormalTriangles", (void **)&clgame.dllFuncs.pfnDrawNormalTriangles },
{ "HUD_DrawTransparentTriangles", (void **)&clgame.dllFuncs.pfnDrawTransparentTriangles },
{ "HUD_GetUserEntity", (void **)&clgame.dllFuncs.pfnGetUserEntity },
{ "Demo_ReadBuffer", (void **)&clgame.dllFuncs.pfnDemo_ReadBuffer },
{ "CAM_Think", (void **)&clgame.dllFuncs.CAM_Think },
{ "CL_IsThirdPerson", (void **)&clgame.dllFuncs.CL_IsThirdPerson },
{ "CL_CreateMove", (void **)&clgame.dllFuncs.CL_CreateMove },
{ "IN_ActivateMouse", (void **)&clgame.dllFuncs.IN_ActivateMouse },
{ "IN_DeactivateMouse", (void **)&clgame.dllFuncs.IN_DeactivateMouse },
{ "IN_MouseEvent", (void **)&clgame.dllFuncs.IN_MouseEvent },
{ "IN_Accumulate", (void **)&clgame.dllFuncs.IN_Accumulate },
{ "IN_ClearStates", (void **)&clgame.dllFuncs.IN_ClearStates },
{ "V_CalcRefdef", (void **)&clgame.dllFuncs.pfnCalcRefdef },
{ "KB_Find", (void **)&clgame.dllFuncs.KB_Find },
{ NULL, NULL }
};

static dllfunc_t cdll_new_exports[] = 	// allowed only in SDK 2.3
{
{ "HUD_GetStudioModelInterface", (void **)&clgame.dllFuncs.pfnGetStudioModelInterface },
{ "HUD_DirectorMessage", (void **)&clgame.dllFuncs.pfnDirectorMessage },
{ "HUD_VoiceStatus", (void **)&clgame.dllFuncs.pfnVoiceStatus },
{ NULL, NULL }
};

/*
====================
CL_GetEntityByIndex

Render callback for studio models
====================
*/
cl_entity_t *CL_GetEntityByIndex( int index )
{
	if( !clgame.entities )
		return NULL;

	if( index < 0 )
		return clgame.dllFuncs.pfnGetUserEntity( abs( index ));

	if( index >= clgame.numEntities )
		return NULL;

	if( !EDICT_NUM( index )->index )
		return NULL;
	return EDICT_NUM( index );
}

/*
====================
CL_GetServerTime

don't clamped time that come from server
====================
*/
float CL_GetServerTime( void )
{
	return sv_time();
}

/*
====================
CL_GetLerpFrac

returns current lerp fraction
====================
*/
float CL_GetLerpFrac( void )
{
	return cl.lerpFrac;
}

/*
====================
CL_IsThirdPerson

returns true if thirdperson is enabled
====================
*/
qboolean CL_IsThirdPerson( void )
{
	return cl.thirdperson;
}

/*
====================
CL_GetPlayerInfo

get player info by render request
====================
*/
player_info_t *CL_GetPlayerInfo( int playerIndex )
{
	if( playerIndex < 0 || playerIndex >= cl.maxclients )
		return NULL;

	return &cl.players[playerIndex];
}

/*
====================
CL_CreatePlaylist

Create a default valve playlist
====================
*/
void CL_CreatePlaylist( const char *filename )
{
	file_t	*f;

	f = FS_Open( filename, "w" );
	if( !f ) return;

	// make standard cdaudio playlist
	FS_Print( f, "blank\n" );		// #1
	FS_Print( f, "Half-Life01.mp3\n" );	// #2
	FS_Print( f, "Prospero01.mp3\n" );	// #3
	FS_Print( f, "Half-Life12.mp3\n" );	// #4
	FS_Print( f, "Half-Life07.mp3\n" );	// #5
	FS_Print( f, "Half-Life10.mp3\n" );	// #6
	FS_Print( f, "Suspense01.mp3\n" );	// #7
	FS_Print( f, "Suspense03.mp3\n" );	// #8
	FS_Print( f, "Half-Life09.mp3\n" );	// #9
	FS_Print( f, "Half-Life02.mp3\n" );	// #10
	FS_Print( f, "Half-Life13.mp3\n" );	// #11
	FS_Print( f, "Half-Life04.mp3\n" );	// #12
	FS_Print( f, "Half-Life15.mp3\n" );	// #13
	FS_Print( f, "Half-Life14.mp3\n" );	// #14
	FS_Print( f, "Half-Life16.mp3\n" );	// #15
	FS_Print( f, "Suspense02.mp3\n" );	// #16
	FS_Print( f, "Half-Life03.mp3\n" );	// #17
	FS_Print( f, "Half-Life08.mp3\n" );	// #18
	FS_Print( f, "Prospero02.mp3\n" );	// #19
	FS_Print( f, "Half-Life05.mp3\n" );	// #20
	FS_Print( f, "Prospero04.mp3\n" );	// #21
	FS_Print( f, "Half-Life11.mp3\n" );	// #22
	FS_Print( f, "Half-Life06.mp3\n" );	// #23
	FS_Print( f, "Prospero03.mp3\n" );	// #24
	FS_Print( f, "Half-Life17.mp3\n" );	// #25
	FS_Print( f, "Prospero05.mp3\n" );	// #26
	FS_Print( f, "Suspense05.mp3\n" );	// #27
	FS_Print( f, "Suspense07.mp3\n" );	// #28
	FS_Close( f );
}

/*
====================
CL_InitCDAudio

Initialize CD playlist
====================
*/
void CL_InitCDAudio( const char *filename )
{
	script_t	*script;
	string	token;
	int	c = 0;

	if( !FS_FileExists( filename ))
	{
		// create a default playlist
		CL_CreatePlaylist( filename );
	}

	script = Com_OpenScript( filename, NULL, 0 );
	if( !script ) return;

	// format: trackname\n [num]
	while( Com_ReadString( script, SC_ALLOW_NEWLINES|SC_ALLOW_PATHNAMES2, token ))
	{
		if( !com.stricmp( token, "blank" )) token[0] = '\0';
		com.strncpy( clgame.cdtracks[c], token, sizeof( clgame.cdtracks[0] ));

		if( ++c > MAX_CDTRACKS - 1 )
		{
			MsgDev( D_WARN, "CD_Init: too many tracks %i in %s\n", filename, MAX_CDTRACKS );
			break;
		}
	}

	Com_CloseScript( script );
}

/*
====================
CL_PointContents

Return contents for point
====================
*/
int CL_PointContents( const vec3_t p )
{
	int cont = CL_TruePointContents( p );

	if( cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN )
		cont = CONTENTS_WATER;
	return cont;
}

/*
====================
StudioEvent

Event callback for studio models
====================
*/
void CL_StudioEvent( struct mstudioevent_s *event, cl_entity_t *pEdict )
{
	clgame.dllFuncs.pfnStudioEvent( event, pEdict );
}

/*
================
CL_FadeAlpha
================
*/
void CL_FadeAlpha( int starttime, int endtime, byte *alpha )
{
	int	time, fade_time;

	if( starttime == 0 )
	{
		*alpha = 255;
		return;
	}

	time = (host.realtime * 1000) - starttime;	// FIXME; convert it to float properly

	if( time >= endtime )
	{
		*alpha = 0;
		return;
	}

	// fade time is 1/4 of endtime
	fade_time = endtime / 4;
	fade_time = bound( 300, fade_time, 10000 );

	// fade out
	if(( endtime - time ) < fade_time )
		*alpha = bound( 0, (( endtime - time ) * ( 1.0f / fade_time )) * 255, 255 );
	else *alpha = 255;
}

/*
=============
CL_AdjustXPos

adjust text by x pos
=============
*/
static int CL_AdjustXPos( float x, int width, int totalWidth )
{
	int xPos;

	if( x == -1 )
	{
		xPos = ( clgame.scrInfo.iWidth - width ) * 0.5f;
	}
	else
	{
		if ( x < 0 )
			xPos = (1.0 + x) * clgame.scrInfo.iWidth - totalWidth;	// Alight right
		else // align left
			xPos = x * clgame.scrInfo.iWidth;
	}

	if( xPos + width > clgame.scrInfo.iWidth )
		xPos = clgame.scrInfo.iWidth - width;
	else if( xPos < 0 )
		xPos = 0;

	return xPos;
}

/*
=============
CL_AdjustYPos

adjust text by y pos
=============
*/
static int CL_AdjustYPos( float y, int height )
{
	int yPos;

	if( y == -1 ) // centered?
	{
		yPos = ( clgame.scrInfo.iHeight - height ) * 0.5f;
	}
	else
	{
		// Alight bottom?
		if( y < 0 )
			yPos = (1.0 + y) * clgame.scrInfo.iHeight - height; // Alight bottom
		else // align top
			yPos = y * clgame.scrInfo.iHeight;
	}

	if( yPos + height > clgame.scrInfo.iHeight )
		yPos = clgame.scrInfo.iHeight - height;
	else if( yPos < 0 )
		yPos = 0;

	return yPos;
}

/*
=============
CL_CenterPrint

print centerscreen message
=============
*/
void CL_CenterPrint( const char *text, float y )
{
	char	*s;
	int	width = 0;
	int	length = 0;

	clgame.centerPrint.lines = 1;
	clgame.centerPrint.totalWidth = 0;
	clgame.centerPrint.time = cl.mtime[0] * 1000; // allow pause for centerprint
	com.strncpy( clgame.centerPrint.message, text, sizeof( clgame.centerPrint.message ));
	s = clgame.centerPrint.message;

	// count the number of lines for centering
	while( *s )
	{
		if( *s == '\n' )
		{
			clgame.centerPrint.lines++;
			if( width > clgame.centerPrint.totalWidth )
				clgame.centerPrint.totalWidth = width;
			width = 0;
		}
		else width += clgame.scrInfo.charWidths[*s];
		s++;
		length++;
	}

	clgame.centerPrint.totalHeight = ( clgame.centerPrint.lines * clgame.scrInfo.iCharHeight ); 
	clgame.centerPrint.y = CL_AdjustYPos( y, clgame.centerPrint.totalHeight );
}

/*
====================
SPR_AdjustSize

draw hudsprite routine
====================
*/
static void SPR_AdjustSize( float *x, float *y, float *w, float *h )
{
	float	xscale, yscale;

	if( !x && !y && !w && !h ) return;

	// scale for screen sizes
	xscale = scr_width->integer / (float)clgame.scrInfo.iWidth;
	yscale = scr_height->integer / (float)clgame.scrInfo.iHeight;

	if( x ) *x *= xscale;
	if( y ) *y *= yscale;
	if( w ) *w *= xscale;
	if( h ) *h *= yscale;
}

static qboolean SPR_Scissor( float *x, float *y, float *width, float *height, float *u0, float *v0, float *u1, float *v1 )
{
	float	dudx, dvdy;

	// clip sub rect to sprite
	if(( width == 0 ) || ( height == 0 ))
		return false;

	if( *x + *width <= clgame.ds.scissor_x )
		return false;
	if( *x >= clgame.ds.scissor_x + clgame.ds.scissor_width )
		return false;
	if( *y + *height <= clgame.ds.scissor_y )
		return false;
	if( *y >= clgame.ds.scissor_y + clgame.ds.scissor_height )
		return false;

	dudx = (*u1 - *u0) / *width;
	dvdy = (*v1 - *v0) / *height;

	if( *x < clgame.ds.scissor_x )
	{
		*u0 += (clgame.ds.scissor_x - *x) * dudx;
		*width -= clgame.ds.scissor_x - *x;
		*x = clgame.ds.scissor_x;
	}

	if( *x + *width > clgame.ds.scissor_x + clgame.ds.scissor_width )
	{
		*u1 -= (*x + *width - (clgame.ds.scissor_x + clgame.ds.scissor_width)) * dudx;
		*width = clgame.ds.scissor_x + clgame.ds.scissor_width - *x;
	}

	if( *y < clgame.ds.scissor_y )
	{
		*v0 += (clgame.ds.scissor_y - *y) * dvdy;
		*height -= clgame.ds.scissor_y - *y;
		*y = clgame.ds.scissor_y;
	}

	if( *y + *height > clgame.ds.scissor_y + clgame.ds.scissor_height )
	{
		*v1 -= (*y + *height - (clgame.ds.scissor_y + clgame.ds.scissor_height)) * dvdy;
		*height = clgame.ds.scissor_y + clgame.ds.scissor_height - *y;
	}
	return true;
}

/*
====================
SPR_DrawGeneric

draw hudsprite routine
====================
*/
static void SPR_DrawGeneric( int frame, float x, float y, float width, float height, const wrect_t *prc )
{
	float	s1, s2, t1, t2;
	int	texnum;

	if( width == -1 && height == -1 )
	{
		int	w, h;

		// assume we get sizes from image
		R_GetSpriteParms( &w, &h, NULL, frame, clgame.ds.pSprite );

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
	if( clgame.ds.scissor_test && !SPR_Scissor( &x, &y, &width, &height, &s1, &t1, &s2, &t2 ))
		return;

	// scale for screen sizes
	SPR_AdjustSize( &x, &y, &width, &height );
	texnum = R_GetSpriteTexture( clgame.ds.pSprite, frame );
	R_DrawStretchPic( x, y, width, height, s1, t1, s2, t2, texnum );
}

/*
=============
CL_DrawCenterPrint

called each frame
=============
*/
static void CL_DrawCenterPrint( void )
{
	char	*pText;
	int	i, j, x, y;
	int	width, lineLength;
	byte	line[80];
	byte	alpha;

	if( !clgame.centerPrint.time )
		return;

	CL_FadeAlpha( clgame.centerPrint.time, scr_centertime->value * 1000, &alpha );

	if( !alpha ) 
	{
		// faded out
		clgame.centerPrint.time = 0;
		return;
	}

	pText = clgame.centerPrint.message;
	y = clgame.centerPrint.y; // start y
	
	for( i = 0; i < clgame.centerPrint.lines; i++ )
	{
		lineLength = 0;
		width = 0;

		while( *pText && *pText != '\n' )
		{
			byte c = *pText;
			line[lineLength] = c;
			width += clgame.scrInfo.charWidths[c];
			lineLength++;
			pText++;
		}
		pText++; // Skip LineFeed
		line[lineLength] = 0;

		x = CL_AdjustXPos( -1, width, clgame.centerPrint.totalWidth );

		for( j = 0; j < lineLength; j++ )
		{
			int ch = line[j];
			int next = x + clgame.scrInfo.charWidths[ch];

			if( x >= 0 && y >= 0 && next <= clgame.scrInfo.iWidth )
			{
				pfnPIC_Set( cls.creditsFont.hFontTexture, 255, 255, 255, alpha );
				pfnPIC_DrawAdditive( x, y, -1, -1, &cls.creditsFont.fontRc[ch] );
			}
			x = next;
		}
		y += clgame.scrInfo.iCharHeight;
	}
}

/*
=============
CL_DrawScreenFade

fill screen with specfied color
can be modulated
=============
*/
void CL_DrawScreenFade( void )
{
	rgba_t		color;
	screenfade_t	*sf = &clgame.fade;
	int		iFadeAlpha;
		
	if( sf->fadeReset == 0.0f && sf->fadeEnd == 0.0f )
		return;	// inactive

	// keep pushing reset time out indefinitely
	if( sf->fadeFlags & FFADE_STAYOUT )
	{
		sf->fadeReset = cl.time + 0.1f;
	}

	// all done?
	if(( cl.time > sf->fadeReset ) && ( cl.time > sf->fadeEnd ))
	{
		Mem_Set( &clgame.fade, 0, sizeof( clgame.fade ));
		return;
	}

	// fading...
	if( sf->fadeFlags == FFADE_STAYOUT )
	{
		iFadeAlpha = sf->fadealpha;
	}
	else
	{
		iFadeAlpha = sf->fadeSpeed * ( sf->fadeEnd - cl.time );
		if( sf->fadeFlags & FFADE_OUT ) iFadeAlpha += sf->fadealpha;
		iFadeAlpha = bound( 0, iFadeAlpha, sf->fadealpha );
	}

	MakeRGBA( color, sf->fader, sf->fadeg, sf->fadeb, iFadeAlpha );
	R_DrawSetColor( color );

	GL_SetRenderMode( kRenderTransTexture );
	R_DrawStretchPic( 0, 0, scr_width->integer, scr_height->integer, 0, 0, 1, 1, cls.fillImage );
	R_DrawSetColor( NULL );
}

/*
====================
CL_InitTitles

parse all messages that declared in titles.txt
and hold them into permament memory pool 
====================
*/
static void CL_InitTitles( const char *filename )
{
	size_t	fileSize;
	byte	*pMemFile;
	int	i;

	// initialize text messages (game_text)
	for( i = 0; i < MAX_TEXTCHANNELS; i++ )
	{
		cl_textmessage[i].pName = com.stralloc( clgame.mempool, va( TEXT_MSGNAME, i ), __FILE__, __LINE__ );
		cl_textmessage[i].pMessage = cl_textbuffer[i];
	}

	// clear out any old data that's sitting around.
	if( clgame.titles ) Mem_Free( clgame.titles );

	clgame.titles = NULL;
	clgame.numTitles = 0;

	pMemFile = FS_LoadFile( filename, &fileSize );
	if( !pMemFile ) return;

	CL_TextMessageParse( pMemFile, fileSize );
	Mem_Free( pMemFile );
}

/*
====================
CL_ParseTextMessage

Parse TE_TEXTMESSAGE
====================
*/
void CL_ParseTextMessage( sizebuf_t *msg )
{
	static int		msgindex = 0;
	client_textmessage_t	*text;
	int			channel;

	// read channel ( 0 - auto)
	channel = BF_ReadByte( msg );

	if( channel <= 0 || channel > ( MAX_TEXTCHANNELS - 1 ))
	{
		// invalid channel specified, use internal counter		
		if( channel != 0 ) MsgDev( D_ERROR, "HudText: invalid channel %i\n", channel );
		channel = msgindex;
		msgindex = (msgindex + 1) & (MAX_TEXTCHANNELS - 1);
	}	

	// grab message channel
	text = &cl_textmessage[channel];

	text->x = (float)(BF_ReadShort( msg ) / 8192.0f);
	text->y = (float)(BF_ReadShort( msg ) / 8192.0f);
	text->effect = BF_ReadByte( msg );
	text->r1 = BF_ReadByte( msg );
	text->g1 = BF_ReadByte( msg );
	text->b1 = BF_ReadByte( msg );
	text->a1 = BF_ReadByte( msg );
	text->r2 = BF_ReadByte( msg );
	text->g2 = BF_ReadByte( msg );
	text->b2 = BF_ReadByte( msg );
	text->a2 = BF_ReadByte( msg );
	text->fadein = (float)(BF_ReadShort( msg ) / 256.0f );
	text->fadeout = (float)(BF_ReadShort( msg ) / 256.0f );
	text->holdtime = (float)(BF_ReadShort( msg ) / 256.0f );

	if( text->effect == 2 )
		text->fxtime = (float)(BF_ReadShort( msg ) / 256.0f );
	else text->fxtime = 0.0f;

	// to prevent grab too long messages
	com.strncpy( (char *)text->pMessage, BF_ReadString( msg ), 512 ); 		

	// NOTE: a "HudText" message contain only 'string' with message name, so we
	// don't needs to use MSG_ routines here, just directly write msgname into netbuffer
	CL_DispatchUserMessage( "HudText", com.strlen( text->pName ) + 1, (void *)text->pName );
}

/*
====================
CL_BadMessage

Default method to invoke host error
====================
*/
int CL_BadMessage( const char *pszName, int iSize, void *pbuf )
{
	Host_Error( "svc_bad\n" );
	return 0;
}

/*
====================
CL_GetLocalPlayer

Render callback for studio models
====================
*/
cl_entity_t *CL_GetLocalPlayer( void )
{
	if( cls.state >= ca_connected )
	{
		cl_entity_t *player;

		player = EDICT_NUM( cl.playernum + 1 );
		ASSERT( player != NULL );
		return player;
	}
	return NULL;
}

/*
====================
CL_GetMaxlients

Render callback for studio models
====================
*/
int CL_GetMaxClients( void )
{
	return cl.maxclients;
}

void CL_DrawCrosshair( void )
{
	int		x, y, width, height;
	cl_entity_t	*pPlayer;

	if( !clgame.ds.pCrosshair || cl.refdef.crosshairangle[2] || !cl_crosshair->integer )
		return;

	pPlayer = CL_GetLocalPlayer();

	if( cl.frame.local.client.deadflag != DEAD_NO || cl.frame.local.client.flags & FL_FROZEN )
		return;

	// any camera on
	if( cl.refdef.viewentity != pPlayer->index )
		return;

	// get crosshair dimension
	width = clgame.ds.rcCrosshair.right - clgame.ds.rcCrosshair.left;
	height = clgame.ds.rcCrosshair.bottom - clgame.ds.rcCrosshair.top;

	x = clgame.scrInfo.iWidth / 2; 
	y = clgame.scrInfo.iHeight / 2;

	// g-cont - cl.refdef.crosshairangle is the autoaim angle.
	// if we're not using autoaim, just draw in the middle of the screen
	if( !VectorIsNull( cl.refdef.crosshairangle ))
	{
		vec3_t	angles;
		vec3_t	forward;
		vec3_t	point, screen;

		VectorAdd( cl.refdef.viewangles, cl.refdef.crosshairangle, angles );
		AngleVectors( angles, forward, NULL, NULL );
		VectorAdd( cl.refdef.vieworg, forward, point );
		R_WorldToScreen( point, screen );

		x += 0.5f * screen[0] * scr_width->integer + 0.5f;
		y += 0.5f * screen[1] * scr_height->integer + 0.5f;
	}

	clgame.ds.pSprite = clgame.ds.pCrosshair;

	GL_SetRenderMode( kRenderTransAlpha );
	R_DrawSetColor( clgame.ds.rgbaCrosshair );
	SPR_DrawGeneric( 0, x - 0.5f * width, y - 0.5f * height, -1, -1, &clgame.ds.rcCrosshair );
}

/*
=============
CL_DrawLoading

draw loading progress bar
=============
*/
static void CL_DrawLoading( float percent )
{
	int	x, y, width, height, right;
	float	xscale, yscale, step, s2;
	rgba_t	color;

	R_GetTextureParms( &width, &height, cls.loadingBar );
	x = ( 640 - width ) >> 1;
	y = ( 480 - height) >> 1;

	xscale = scr_width->integer / 640.0f;
	yscale = scr_height->integer / 480.0f;

	x *= xscale;
	y *= yscale;
	width *= xscale;
	height *= yscale;

	MakeRGBA( color, 128, 128, 128, 255 );
	R_DrawSetColor( color );
	GL_SetRenderMode( kRenderTransTexture );
	R_DrawStretchPic( x, y, width, height, 0, 0, 1, 1, cls.loadingBar );

	step = (float)width / 100.0f;
	right = (int)ceil( percent * step );
	s2 = (float)right / width;
	width = right;
	
	MakeRGBA( color, 208, 152, 0, 255 );
	R_DrawSetColor( color );
	GL_SetRenderMode( kRenderTransTexture );
	R_DrawStretchPic( x, y, width, height, 0, 0, s2, 1, cls.loadingBar );
	R_DrawSetColor( NULL );
}

/*
=============
CL_DrawPause

draw pause sign
=============
*/
static void CL_DrawPause( void )
{
	int	x, y, width, height;
	float	xscale, yscale;

	R_GetTextureParms( &width, &height, cls.pauseIcon );
	x = ( 640 - width ) >> 1;
	y = ( 480 - height) >> 1;

	xscale = scr_width->integer / 640.0f;
	yscale = scr_height->integer / 480.0f;

	x *= xscale;
	y *= yscale;
	width *= xscale;
	height *= yscale;

	R_DrawSetColor( NULL );
	GL_SetRenderMode( kRenderTransTexture );
	R_DrawStretchPic( x, y, width, height, 0, 0, 1, 1, cls.pauseIcon );
}

void CL_DrawHUD( int state )
{
	if( state == CL_ACTIVE && !cl.video_prepped )
		state = CL_LOADING;

	if( state == CL_ACTIVE && cl.refdef.paused )
		state = CL_PAUSED;

	switch( state )
	{
	case CL_ACTIVE:
		CL_DrawScreenFade ();
		CL_DrawCrosshair ();
		CL_DrawCenterPrint ();
		clgame.dllFuncs.pfnRedraw( cl_time(), false );
		break;
	case CL_PAUSED:
		CL_DrawScreenFade ();
		CL_DrawCrosshair ();
		CL_DrawCenterPrint ();
		clgame.dllFuncs.pfnRedraw( cl_time(), false );
		CL_DrawPause();
		break;
	case CL_LOADING:
		CL_DrawLoading( scr_loading->value );
		break;
	case CL_CHANGELEVEL:
		if( cls.draw_changelevel )
		{
			CL_DrawLoading( 100.0f );
			cls.draw_changelevel = false;
		}
		break;
	}
}

void CL_LinkUserMessage( char *pszName, const int svc_num, int iSize )
{
	int	i;

	if( !pszName || !*pszName )
		Host_Error( "CL_LinkUserMessage: bad message name\n" );

	if( svc_num < svc_lastmsg )
		Host_Error( "CL_LinkUserMessage: tired to hook a system message \"%s\"\n", svc_strings[svc_num] );	

	// see if already hooked
	for( i = 0; i < MAX_USER_MESSAGES && clgame.msg[i].name[0]; i++ )
	{
		// NOTE: no check for DispatchFunc, check only name
		if( !com.strcmp( clgame.msg[i].name, pszName ))
		{
			clgame.msg[i].number = svc_num;
			clgame.msg[i].size = iSize;
			return;
		}
	}

	if( i == MAX_USER_MESSAGES ) 
	{
		Host_Error( "CL_LinkUserMessage: MAX_USER_MESSAGES hit!\n" );
		return;
	}

	// register new message without DispatchFunc, so we should parse it properly
	com.strncpy( clgame.msg[i].name, pszName, sizeof( clgame.msg[i].name ));
	clgame.msg[i].number = svc_num;
	clgame.msg[i].size = iSize;
}

static void CL_RegisterEvent( int lastnum, const char *szEvName, pfnEventHook func )
{
	user_event_t	*ev;

	if( lastnum == MAX_EVENTS )
	{
		MsgDev( D_ERROR, "CL_RegisterEvent: MAX_EVENTS hit!\n" );
		return;
	}

	ev = clgame.events[lastnum];

	// clear existing or allocate new one
	if( ev ) Mem_Set( ev, 0, sizeof( *ev ));
	else ev = clgame.events[lastnum] = Mem_Alloc( cls.mempool, sizeof( *ev ));

	com.strncpy( ev->name, szEvName, CS_SIZE );
	ev->func = func;
	// ev->index will be set later
}

void CL_SetEventIndex( const char *szEvName, int ev_index )
{
	user_event_t	*ev;
	int		i;

	if( !szEvName || !*szEvName ) return; // ignore blank names

	// search event by name to link with
	for( i = 0; i < MAX_EVENTS; i++ )
	{
		ev = clgame.events[i];
		if( !ev ) break;

		if( !com.strcmp( ev->name, szEvName ))
		{
			ev->index = ev_index;
			return;
		}
	}
}

/*
===============
CL_ResetEvent

===============
*/
void CL_ResetEvent( event_info_t *ei )
{
	Mem_Set( ei, 0, sizeof( *ei ));
}

/*
=============
CL_EventIndex

=============
*/
word CL_EventIndex( const char *name )
{
	int	i;
	
	if( !name || !name[0] )
		return 0;

	for( i = 1; i < MAX_EVENTS && cl.event_precache[i][0]; i++ )
		if( !com.stricmp( cl.event_precache[i], name ))
			return i;
	return 0;
}

/*
=============
CL_FireEvent

=============
*/
qboolean CL_FireEvent( event_info_t *ei )
{
	user_event_t	*ev;
	const char	*name;
	int		i, idx;

	if( !ei || !ei->index )
		return false;

	// get the func pointer
	for( i = 0; i < MAX_EVENTS; i++ )
	{
		ev = clgame.events[i];		
		if( !ev )
		{
			idx = bound( 1, ei->index, MAX_EVENTS );
			MsgDev( D_ERROR, "CL_FireEvent: %s not precached\n", cl.event_precache[idx] );
			break;
		}

		if( ev->index == ei->index )
		{
			if( ev->func )
			{
				ev->func( &ei->args );
				return true;
			}

			name = cl.event_precache[ei->index];
			MsgDev( D_ERROR, "CL_FireEvent: %s not hooked\n", name );
			break;			
		}
	}
	return false;
}

/*
=============
CL_FireEvents

called right before draw frame
=============
*/
void CL_FireEvents( void )
{
	int		i;
	event_state_t	*es;
	event_info_t	*ei;
	qboolean		success;

	es = &cl.events;

	for( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		ei = &es->ei[i];

		if( ei->index == 0 )
			continue;

		if( cls.state == ca_disconnected )
		{
			CL_ResetEvent( ei );
			continue;
		}

		// delayed event!
		if( ei->fire_time && ( ei->fire_time > cl.time ))
			continue;

		success = CL_FireEvent( ei );

		// zero out the remaining fields
		CL_ResetEvent( ei );
	}
}

/*
=============
CL_FindEvent

find first empty event
=============
*/
event_info_t *CL_FindEmptyEvent( void )
{
	int		i;
	event_state_t	*es;
	event_info_t	*ei;

	es = &cl.events;

	// look for first slot where index is != 0
	for( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		ei = &es->ei[i];
		if( ei->index != 0 )
			continue;
		return ei;
	}

	// no slots available
	return NULL;
}

/*
=============
CL_FindEvent

replace only unreliable events
=============
*/
event_info_t *CL_FindUnreliableEvent( void )
{
	int		i;
	event_state_t	*es;
	event_info_t	*ei;

	es = &cl.events;
	for ( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		ei = &es->ei[i];
		if( ei->index != 0 )
		{
			// it's reliable, so skip it
			if( ei->flags & FEV_RELIABLE )
				continue;
		}
		return ei;
	}

	// this should never happen
	return NULL;
}

/*
=============
CL_QueueEvent

=============
*/
void CL_QueueEvent( int flags, int index, float delay, event_args_t *args )
{
	qboolean		unreliable = (flags & FEV_RELIABLE) ? false : true;
	event_info_t	*ei;

	// find a normal slot
	ei = CL_FindEmptyEvent();
	if( !ei && unreliable )
	{
		return;
	}

	// okay, so find any old unreliable slot
	if( !ei )
	{
		ei = CL_FindUnreliableEvent();
		if( !ei ) return;
	}

	ei->index	= index;
	ei->fire_time = delay ? (cl.time + delay) : 0.0f;
	ei->flags	= flags;
	
	// copy in args event data
	Mem_Copy( &ei->args, args, sizeof( ei->args ));
}

/*
=============
CL_PlaybackEvent

=============
*/
void CL_PlaybackEvent( int flags, const edict_t *pInvoker, word eventindex, float delay, float *origin,
	float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	event_args_t	args;
	int		invokerIndex = 0;

	// first check event for out of bounds
	if( eventindex < 1 || eventindex > MAX_EVENTS )
	{
		MsgDev( D_ERROR, "CL_PlaybackEvent: invalid eventindex %i\n", eventindex );
		return;
	}
	// check event for precached
	if( !CL_EventIndex( cl.event_precache[eventindex] ))
	{
		MsgDev( D_ERROR, "CL_PlaybackEvent: event %i was not precached\n", eventindex );
		return;		
	}

	flags |= FEV_CLIENT; // it's a client event
	flags &= ~(FEV_NOTHOST|FEV_HOSTONLY|FEV_GLOBAL);

	if( delay < 0.0f )
		delay = 0.0f; // fixup negative delays

	invokerIndex = cl.playernum + 1;

	args.flags = 0;
	args.entindex = invokerIndex;
	VectorCopy( origin, args.origin );
	VectorCopy( angles, args.angles );

	args.fparam1 = fparam1;
	args.fparam2 = fparam2;
	args.iparam1 = iparam1;
	args.iparam2 = iparam2;
	args.bparam1 = bparam1;
	args.bparam2 = bparam2;

	if( flags & FEV_RELIABLE )
	{
		args.ducking = 0;
		VectorClear( args.velocity );
	}
	else if( invokerIndex )
	{
		// get up some info from invoker
		VectorCopy( cl.data.origin, args.origin );
		VectorCopy( cl.data.viewangles, args.angles );
		VectorCopy( cl.frame.local.playerstate.velocity, args.velocity );
		args.ducking = cl.frame.local.playerstate.usehull;
	}

	CL_QueueEvent( flags, eventindex, delay, &args );
}

void CL_InitEntity( cl_entity_t *pEdict )
{
	ASSERT( pEdict );

	pEdict->index = NUM_FOR_EDICT( pEdict );
}

void CL_FreeEntity( cl_entity_t *pEdict )
{
	ASSERT( pEdict );

	// already freed ?
	if( !pEdict->index ) return;

	R_RemoveEfrags( pEdict );
	CL_KillDeadBeams( pEdict );
	pEdict->index = 0;	// freed
}

void CL_ClearWorld( void )
{
	cl_entity_t	*ent;

	ent = EDICT_NUM( 0 );
	ent->index = NUM_FOR_EDICT( ent );
	ent->curstate.modelindex = 1;	// world model
	ent->curstate.solid = SOLID_BSP;
	ent->curstate.movetype = MOVETYPE_PUSH;
	ent->model = cl.worldmodel;
	clgame.numEntities = 1;
}

void CL_InitEdicts( void )
{
	ASSERT( clgame.entities == NULL );

	CL_UPDATE_BACKUP = ( cl.maxclients == 1 ) ? SINGLEPLAYER_BACKUP : MULTIPLAYER_BACKUP;
	cls.num_client_entities = CL_UPDATE_BACKUP * 64;
	cls.packet_entities = Z_Realloc( cls.packet_entities, sizeof( entity_state_t ) * cls.num_client_entities );
	clgame.entities = Mem_Alloc( clgame.mempool, sizeof( cl_entity_t ) * clgame.maxEntities );
}

void CL_FreeEdicts( void )
{
	int	i;
	cl_entity_t	*ent;

	if( clgame.entities )
	{
		for( i = 0; i < clgame.maxEntities; i++ )
		{
			ent = EDICT_NUM( i );
			if( ent->index <= 0 ) continue;
			CL_FreeEntity( ent );
		}
		Mem_Free( clgame.entities );
	}

	if( cls.packet_entities )
	{
		Z_Free( cls.packet_entities );
		cls.packet_entities = NULL;
		cls.num_client_entities = 0;
		cls.next_client_entities = 0;
	}

	clgame.numEntities = 0;
	clgame.entities = NULL;
}

/*
===============================================================================
	CGame Builtin Functions

===============================================================================
*/
static qboolean CL_LoadHudSprite( const char *szSpriteName, model_t *m_pSprite, qboolean mapSprite )
{
	byte	*buf;
	size_t	size;

	ASSERT( m_pSprite != NULL );

	buf = FS_LoadFile( szSpriteName, &size );
	if( !buf ) return false;

	com.strncpy( m_pSprite->name, szSpriteName, sizeof( m_pSprite->name ));

	if( mapSprite ) Mod_LoadMapSprite( m_pSprite, buf, size );
	else Mod_LoadSpriteModel( m_pSprite, buf );		

	Mem_Free( buf );

	if( m_pSprite->type != mod_sprite )
	{
		Mem_Set( m_pSprite, 0, sizeof( *m_pSprite ));
		return false;
	}
	return true;
}

/*
=========
pfnSPR_Load

=========
*/
HSPRITE pfnSPR_Load( const char *szPicName )
{
	int	i;

	if( !szPicName || !*szPicName )
	{
		MsgDev( D_ERROR, "CL_LoadSprite: bad name!\n" );
		return 0;
	}

	// slot 0 isn't used
	for( i = 1; i < MAX_IMAGES; i++ )
	{
		if( !com.strcmp( clgame.sprites[i].name, szPicName ))
		{
			// prolonge registration
			clgame.sprites[i].needload = world.load_sequence;
			return i;
		}
	}

	// find a free model slot spot
	for( i = 1; i < MAX_IMAGES; i++ )
		if( !clgame.sprites[i].name[0] ) break; // this is a valid spot

	if( i == MAX_IMAGES ) 
	{
		MsgDev( D_ERROR, "SPR_Load: can't load %s, MAX_HSPRITES limit exceeded\n", szPicName );
		return 0;
	}

	// load new model
	if( CL_LoadHudSprite( szPicName, &clgame.sprites[i], false ))
		return i;
	return 0;
}

/*
=============
CL_GetSpritePointer

=============
*/
const model_t *CL_GetSpritePointer( HSPRITE hSprite )
{
	if( hSprite <= 0 || hSprite > ( MAX_IMAGES - 1 ))
		return 0;	// bad image
	return &clgame.sprites[hSprite];
}

/*
=========
pfnSPR_Frames

=========
*/
static int pfnSPR_Frames( HSPRITE hPic )
{
	int	numFrames;

	R_GetSpriteParms( NULL, NULL, &numFrames, 0, CL_GetSpritePointer( hPic ));

	return numFrames;
}

/*
=========
pfnSPR_Height

=========
*/
static int pfnSPR_Height( HSPRITE hPic, int frame )
{
	int	sprHeight;

	R_GetSpriteParms( NULL, &sprHeight, NULL, frame, CL_GetSpritePointer( hPic ));

	return sprHeight;
}

/*
=========
pfnSPR_Width

=========
*/
static int pfnSPR_Width( HSPRITE hPic, int frame )
{
	int	sprWidth;

	R_GetSpriteParms( &sprWidth, NULL, NULL, frame, CL_GetSpritePointer( hPic ));

	return sprWidth;
}

/*
=========
pfnSPR_Set

=========
*/
static void pfnSPR_Set( HSPRITE hPic, int r, int g, int b )
{
	rgba_t	color;

	clgame.ds.pSprite = CL_GetSpritePointer( hPic );
	color[0] = bound( 0, r, 255 );
	color[1] = bound( 0, g, 255 );
	color[2] = bound( 0, b, 255 );
	color[3] = 255;
	R_DrawSetColor( color );
}

/*
=========
pfnSPR_Draw

=========
*/
static void pfnSPR_Draw( int frame, int x, int y, const wrect_t *prc )
{
	GL_SetRenderMode( kRenderNormal );
	SPR_DrawGeneric( frame, x, y, -1, -1, prc );
}

/*
=========
pfnSPR_DrawHoles

=========
*/
static void pfnSPR_DrawHoles( int frame, int x, int y, const wrect_t *prc )
{
	GL_SetRenderMode( kRenderTransAlpha );
	SPR_DrawGeneric( frame, x, y, -1, -1, prc );
}

/*
=========
pfnSPR_DrawAdditive

=========
*/
static void pfnSPR_DrawAdditive( int frame, int x, int y, const wrect_t *prc )
{
	GL_SetRenderMode( kRenderTransAdd );
	SPR_DrawGeneric( frame, x, y, -1, -1, prc );
}

/*
=========
pfnSPR_EnableScissor

=========
*/
static void pfnSPR_EnableScissor( int x, int y, int width, int height )
{
	// check bounds
	x = bound( 0, x, clgame.scrInfo.iWidth );
	y = bound( 0, y, clgame.scrInfo.iHeight );
	width = bound( 0, width, clgame.scrInfo.iWidth - x );
	height = bound( 0, height, clgame.scrInfo.iHeight - y );

	clgame.ds.scissor_x = x;
	clgame.ds.scissor_width = width;
	clgame.ds.scissor_y = y;
	clgame.ds.scissor_height = height;
	clgame.ds.scissor_test = true;
}

/*
=========
pfnSPR_DisableScissor

=========
*/
static void pfnSPR_DisableScissor( void )
{
	clgame.ds.scissor_x = 0;
	clgame.ds.scissor_width = 0;
	clgame.ds.scissor_y = 0;
	clgame.ds.scissor_height = 0;
	clgame.ds.scissor_test = false;
}

/*
=========
pfnSPR_GetList

for parsing half-life scripts - hud.txt etc
=========
*/
static client_sprite_t *pfnSPR_GetList( char *psz, int *piCount )
{
	client_sprite_t	*pList;
	int		numSprites = 0;
	int		index, iTemp;
	script_t		*script;
	token_t		token;

	if( piCount ) *piCount = 0;

	if( !clgame.itemspath[0] )
		FS_ExtractFilePath( psz, clgame.itemspath );

	script = Com_OpenScript( psz, NULL, 0 );
	if( !script ) return NULL;
          
	Com_ReadUlong( script, SC_ALLOW_NEWLINES, &numSprites );

	// name, res, pic, x, y, w, h
	// NOTE: we must use com_studiocache because it will be purge on next restart or change map
	pList = Mem_Alloc( com_studiocache, sizeof( client_sprite_t ) * numSprites );

	for( index = 0; index < numSprites; index++ )
	{
		if( !Com_ReadToken( script, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token ))
			break;

		com.strncpy( pList[index].szName, token.string, sizeof( pList[index].szName ));

		// read resolution
		Com_ReadUlong( script, false, &iTemp );
		pList[index].iRes = iTemp;

		// read spritename
		Com_ReadToken( script, SC_PARSE_GENERIC, &token );
		com.strncpy( pList[index].szSprite, token.string, sizeof( pList[index].szSprite ));

		// parse rectangle
		Com_ReadUlong( script, false, &iTemp );
		pList[index].rc.left = iTemp;

		Com_ReadUlong( script, false, &iTemp );
		pList[index].rc.top = iTemp;

		Com_ReadUlong( script, false, &iTemp );
		pList[index].rc.right = pList[index].rc.left + iTemp;

		Com_ReadUlong( script, false, &iTemp );
		pList[index].rc.bottom = pList[index].rc.top + iTemp;

		if( piCount ) (*piCount)++;
	}

	if( index < numSprites )
		MsgDev( D_WARN, "SPR_GetList: unexpected end of %s (%i should be %i)\n", psz, numSprites, index );

	Com_CloseScript( script );

	return pList;
}

/*
=============
pfnFillRGBA

=============
*/
static void pfnFillRGBA( int x, int y, int width, int height, int r, int g, int b, int a )
{
	rgba_t	color;

	MakeRGBA( color, r, g, b, a );
	R_DrawSetColor( color );

	SPR_AdjustSize( (float *)&x, (float *)&y, (float *)&width, (float *)&height );

	GL_SetRenderMode( kRenderTransTexture );
	R_DrawStretchPic( x, y, width, height, 0, 0, 1, 1, cls.fillImage );
	R_DrawSetColor( NULL );
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
	clgame.scrInfo.iSize = sizeof( clgame.scrInfo );

	if( Cvar_VariableInteger( "hud_scale" ))
	{
		if( scr_width->integer < 640 )
		{
			// virtual screen space 320x200
			clgame.scrInfo.iWidth = 320;
			clgame.scrInfo.iHeight = 200;
		}
		else
		{
			// virtual screen space 640x480
			clgame.scrInfo.iWidth = 640;
			clgame.scrInfo.iHeight = 480;
		}
		clgame.scrInfo.iFlags |= SCRINFO_STRETCHED;
	}
	else
	{
		clgame.scrInfo.iWidth = scr_width->integer;
		clgame.scrInfo.iHeight = scr_height->integer;
		clgame.scrInfo.iFlags &= ~SCRINFO_STRETCHED;
	}

	if( !pscrinfo ) return 0;

	if( pscrinfo->iSize != clgame.scrInfo.iSize )
		clgame.scrInfo.iSize = pscrinfo->iSize;

	// copy screeninfo out
	Mem_Copy( pscrinfo, &clgame.scrInfo, clgame.scrInfo.iSize );

	return 1;
}

/*
=============
pfnSetCrosshair

setup auto-aim crosshair
=============
*/
static void pfnSetCrosshair( HSPRITE hspr, wrect_t rc, int r, int g, int b )
{
	clgame.ds.rgbaCrosshair[0] = (byte)r;
	clgame.ds.rgbaCrosshair[1] = (byte)g;
	clgame.ds.rgbaCrosshair[2] = (byte)b;
	clgame.ds.rgbaCrosshair[3] = (byte)0xFF;
	clgame.ds.pCrosshair = CL_GetSpritePointer( hspr );
	clgame.ds.rcCrosshair = rc;
}

/*
=============
pfnHookUserMsg

=============
*/
static int pfnHookUserMsg( const char *pszName, pfnUserMsgHook pfn )
{
	int	i;

	// ignore blank names or invalid callbacks
	if( !pszName || !*pszName || !pfn )
		return 0;	

	for( i = 0; i < MAX_USER_MESSAGES && clgame.msg[i].name[0]; i++ )
	{
		// see if already hooked
		if( !com.strcmp( clgame.msg[i].name, pszName ))
			return 1;
	}

	if( i == MAX_USER_MESSAGES ) 
	{
		Host_Error( "HookUserMsg: MAX_USER_MESSAGES hit!\n" );
		return 0;
	}

	// hook new message
	com.strncpy( clgame.msg[i].name, pszName, sizeof( clgame.msg[i].name ));
	clgame.msg[i].func = pfn;

	return 1;
}

/*
=============
pfnServerCmd

=============
*/
static int pfnServerCmd( const char *szCmdString )
{
	// server command adding in cmds queue
	Cbuf_AddText( va( "cmd %s", szCmdString ));
	return 1;
}

/*
=============
pfnClientCmd

=============
*/
static int pfnClientCmd( const char *szCmdString )
{
	// client command executes immediately
	Cbuf_AddText( szCmdString );
	return 1;
}

/*
=============
pfnGetPlayerInfo

=============
*/
static void pfnGetPlayerInfo( int ent_num, hud_player_info_t *pinfo )
{
	player_info_t	*player;
	cl_entity_t	*ent;
	qboolean		spec = false;

	ent = CL_GetEntityByIndex( ent_num );
	ent_num -= 1; // player list if offset by 1 from ents

	if( ent_num >= cl.maxclients || ent_num < 0 || !cl.players[ent_num].name[0] )
	{
		Mem_Set( pinfo, 0, sizeof( *pinfo ));
		return;
	}

	player = &cl.players[ent_num];
	pinfo->thisplayer = ( ent_num == cl.playernum ) ? true : false;
	if( ent ) spec = ent->curstate.spectator;

	pinfo->name = player->name;
	pinfo->model = player->model;

	pinfo->spectator = spec;		
	pinfo->ping = player->ping;
	pinfo->packetloss = player->packet_loss;
	pinfo->topcolor = com.atoi( Info_ValueForKey( player->userinfo, "topcolor" ));
	pinfo->bottomcolor = com.atoi( Info_ValueForKey( player->userinfo, "bottomcolor" ));
}

/*
=============
pfnPlaySoundByName

=============
*/
static void pfnPlaySoundByName( const char *szSound, float volume )
{
	int hSound = S_RegisterSound( szSound );
	S_StartSound( NULL, cl.refdef.viewentity, CHAN_AUTO, hSound, volume, ATTN_NORM, PITCH_NORM, 0 );
}

/*
=============
pfnPlaySoundByIndex

=============
*/
static void pfnPlaySoundByIndex( int iSound, float volume )
{
	int hSound;

	// make sure what we in-bounds
	iSound = bound( 0, iSound, MAX_SOUNDS );
	hSound = cl.sound_index[iSound];

	if( !hSound )
	{
		MsgDev( D_ERROR, "CL_PlaySoundByIndex: invalid sound handle %i\n", iSound );
		return;
	}
	S_StartSound( NULL, cl.refdef.viewentity, CHAN_AUTO, hSound, volume, ATTN_NORM, PITCH_NORM, 0 );
}

/*
=============
pfnTextMessageGet

returns specified message from titles.txt
=============
*/
static client_textmessage_t *pfnTextMessageGet( const char *pName )
{
	int	i;

	// first check internal messages
	for( i = 0; i < MAX_TEXTCHANNELS; i++ )
	{
		if( !com.strcmp( pName, va( TEXT_MSGNAME, i )))
			return cl_textmessage + i;
	}

	// find desired message
	for( i = 0; i < clgame.numTitles; i++ )
	{
		if( !com.strcmp( pName, clgame.titles[i].pName ))
			return clgame.titles + i;
	}
	return NULL; // found nothing
}

/*
=============
pfnDrawCharacter

returns drawed chachter width (in real screen pixels)
=============
*/
static int pfnDrawCharacter( int x, int y, int number, int r, int g, int b )
{
	if( !cls.creditsFont.valid )
		return 0;

	number &= 255;

	if( number < 32 ) return 0;
	if( y < -clgame.scrInfo.iCharHeight )
		return 0;

	pfnPIC_Set( cls.creditsFont.hFontTexture, r, g, b, 255 );
	pfnPIC_DrawAdditive( x, y, -1, -1, &cls.creditsFont.fontRc[number] );

	return clgame.scrInfo.charWidths[number];
}

/*
=============
pfnDrawConsoleString

drawing string like a console string 
=============
*/
static int pfnDrawConsoleString( int x, int y, char *string )
{
	int	drawLen;

	if( !string || !*string ) return 0; // silent ignore
	drawLen = Con_DrawString( x, y, string, clgame.ds.textColor );
	MakeRGBA( clgame.ds.textColor, 255, 255, 255, 255 );

	return drawLen; // exclude color prexfixes
}

/*
=============
pfnDrawSetTextColor

set color for anything
=============
*/
static void pfnDrawSetTextColor( float r, float g, float b )
{
	// bound color and convert to byte
	clgame.ds.textColor[0] = (byte)bound( 0, r * 255, 255 );
	clgame.ds.textColor[1] = (byte)bound( 0, g * 255, 255 );
	clgame.ds.textColor[2] = (byte)bound( 0, b * 255, 255 );
	clgame.ds.textColor[3] = (byte)0xFF;
}

/*
=============
pfnConsolePrint

prints dirctly into console (can skip notify)
=============
*/
static void pfnConsolePrint( const char *string )
{
	if( !string || !*string ) return;
	if( *string != 1 ) Msg( string ); // show notify
	else Msg( "[skipnotify]%s", string + 1 ); // skip notify
}

/*
=============
pfnCenterPrint

holds and fade message at center of screen
like trigger_multiple message in q1
=============
*/
static void pfnCenterPrint( const char *string )
{
	if( !string || !*string ) return; // someone stupid joke
	CL_CenterPrint( string, -1 );
}

/*
=========
GetWindowCenterX

=========
*/
static int pfnGetWindowCenterX( void )
{
	return host.window_center_x;
}

/*
=========
GetWindowCenterY

=========
*/
static int pfnGetWindowCenterY( void )
{
	return host.window_center_y;
}

/*
=============
pfnGetViewAngles

return interpolated angles from previous frame
=============
*/
static void pfnGetViewAngles( float *angles )
{
	if( angles ) VectorCopy( cl.refdef.cl_viewangles, angles );
}

/*
=============
pfnSetViewAngles

return interpolated angles from previous frame
=============
*/
static void pfnSetViewAngles( float *angles )
{
	if( angles ) VectorCopy( angles, cl.refdef.cl_viewangles );
}

/*
=============
pfnPhysInfo_ValueForKey

=============
*/
static const char* pfnPhysInfo_ValueForKey( const char *key )
{
	return Info_ValueForKey( cl.frame.local.client.physinfo, key );
}

/*
=============
pfnServerInfo_ValueForKey

=============
*/
static const char* pfnServerInfo_ValueForKey( const char *key )
{
	return Info_ValueForKey( cl.serverinfo, key );
}

/*
=============
pfnGetClientMaxspeed

value that come from server
=============
*/
static float pfnGetClientMaxspeed( void )
{
	return cl.frame.local.client.maxspeed;
}

/*
=============
pfnCheckParm

=============
*/
static int pfnCheckParm( char *parm, char **ppnext )
{
	static char	str[64];

	if( FS_GetParmFromCmdLine( parm, str ))
	{
		// get the pointer on cmdline param
		if( ppnext ) *ppnext = str;
		return 1;
	}
	return 0;
}

/*
=============
pfnKey_Event

=============
*/
static void pfnKey_Event( int key, int down )
{
	// add event to queue
	Sys_QueEvent( SE_KEY, key, down, 0, NULL );
}


/*
=============
pfnGetMousePosition

=============
*/
static void pfnGetMousePosition( int *mx, int *my )
{
	POINT	curpos;

	GetCursorPos( &curpos );
	if( mx ) *mx = curpos.x;
	if( my ) *my = curpos.y;
}

/*
=============
pfnIsNoClipping

=============
*/
int pfnIsNoClipping( void )
{
	cl_entity_t *pl = CL_GetLocalPlayer();

	if( !pl ) return false;

	return pl->curstate.movetype == MOVETYPE_NOCLIP;
}

/*
=============
pfnGetViewModel

=============
*/
static cl_entity_t* pfnGetViewModel( void )
{
	clgame.viewent.index = cl.playernum + 1;
	return &clgame.viewent;
}

/*
=============
pfnGetEntityByIndex

Client.dll safe version
=============
*/
static cl_entity_t *pfnGetEntityByIndex( int index )
{
	if( !clgame.entities )
		return NULL;

	if( index < 0 )
		return clgame.dllFuncs.pfnGetUserEntity( abs( index ));

	if( index >= clgame.maxEntities )
		return NULL;

	return EDICT_NUM( index );
}

/*
=============
pfnGetClientTime

=============
*/
static float pfnGetClientTime( void )
{
	return cl_time();
}

/*
=============
pfnCalcShake

=============
*/
void pfnCalcShake( void )
{
	int	i;
	float	fraction, freq;
	float	localAmp;

	if( clgame.shake.time == 0 )
		return;

	if(( cl.time > clgame.shake.time ) || clgame.shake.amplitude <= 0 || clgame.shake.frequency <= 0 )
	{
		Mem_Set( &clgame.shake, 0, sizeof( clgame.shake ));
		return;
	}

	if( cl.time > clgame.shake.next_shake )
	{
		// higher frequency means we recalc the extents more often and perturb the display again
		clgame.shake.next_shake = cl.time + ( 1.0f / clgame.shake.frequency );

		// compute random shake extents (the shake will settle down from this)
		for( i = 0; i < 3; i++ )
			clgame.shake.offset[i] = Com_RandomFloat( -clgame.shake.amplitude, clgame.shake.amplitude );
		clgame.shake.angle = Com_RandomFloat( -clgame.shake.amplitude * 0.25, clgame.shake.amplitude * 0.25 );
	}

	// ramp down amplitude over duration (fraction goes from 1 to 0 linearly with slope 1/duration)
	fraction = ( clgame.shake.time - cl.time ) / clgame.shake.duration;

	// ramp up frequency over duration
	if( fraction )
	{
		freq = ( clgame.shake.frequency / fraction );
	}
	else
	{
		freq = 0;
	}

	// square fraction to approach zero more quickly
	fraction *= fraction;

	// Sine wave that slowly settles to zero
	fraction = fraction * com.sin( cl.time * freq );
	
	// add to view origin
	VectorScale( clgame.shake.offset, fraction, clgame.shake.applied_offset );

	// add to roll
	clgame.shake.applied_angle = clgame.shake.angle * fraction;

	// drop amplitude a bit, less for higher frequency shakes
	localAmp = clgame.shake.amplitude * ( host.frametime / ( clgame.shake.duration * clgame.shake.frequency ));
	clgame.shake.amplitude -= localAmp;
}

/*
=============
pfnApplyShake

=============
*/
void pfnApplyShake( float *origin, float *angles, float factor )
{
	if( origin ) VectorMA( origin, factor, clgame.shake.applied_offset, origin );
	if( angles ) angles[ROLL] += clgame.shake.applied_angle * factor;
}
	
/*
=============
pfnIsSpectateOnly

=============
*/
static int pfnIsSpectateOnly( void )
{
	cl_entity_t *pl = CL_GetLocalPlayer();

	if( !pl ) return false;

	return pl->curstate.spectator;
}

/*
=============
pfnPointContents

=============
*/
static int pfnPointContents( const float *p, int *truecontents )
{
	int	cont, truecont;

	truecont = cont = CL_TruePointContents( p );
	if( truecontents ) *truecontents = truecont;

	if( cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN )
		cont = CONTENTS_WATER;
	return cont;
}

/*
=============
pfnWaterEntity

=============
*/
static int pfnWaterEntity( const float *rgflPos )
{
	physent_t		*pe;
	hull_t		*hull;
	vec3_t		test;
	int		i;

	if( !rgflPos ) return -1;

	for( i = 0; i < clgame.pmove->nummoveent; i++ )
	{
		pe = &clgame.pmove->moveents[i];

		if( pe->solid != SOLID_NOT ) // disabled ?
			continue;

		// only brushes can have special contents
		if( pe->model->type != mod_brush )
			continue;

		// check water brushes accuracy
		hull = PM_HullForBsp( pe, vec3_origin, vec3_origin, test );

		// offset the test point appropriately for this hull.
		VectorSubtract( rgflPos, test, test );

		// test hull for intersection with this model
		if( PM_HullPointContents( hull, hull->firstclipnode, test ) == CONTENTS_EMPTY )
			continue;

		// found water entity
		return pe->info;
	}
	return -1;
}

/*
=============
pfnTraceLine

=============
*/
static pmtrace_t *pfnTraceLine( float *start, float *end, int flags, int usehull, int ignore_pe )
{
	static pmtrace_t	tr;

	tr = PM_PlayerTrace( clgame.pmove, start, end, flags, usehull, ignore_pe, NULL );
	return &tr;
}

static void pfnPlaySoundByNameAtLocation( char *szSound, float volume, float *origin )
{
	int hSound = S_RegisterSound( szSound );
	S_StartSound( origin, 0, CHAN_AUTO, hSound, volume, ATTN_NORM, PITCH_NORM, 0 );
}

/*
=============
pfnPrecacheEvent

=============
*/
static word pfnPrecacheEvent( int type, const char* psz )
{
	return CL_EventIndex( psz );
}

/*
=============
pfnHookEvent

=============
*/
static void pfnHookEvent( const char *name, pfnEventHook pfn )
{
	word		event_index = CL_EventIndex( name );
	user_event_t	*ev;
	int		i;

	// ignore blank names
	if( !name || !*name ) return;	

	// second call can change EventFunc
	for( i = 0; i < MAX_EVENTS; i++ )
	{
		ev = clgame.events[i];		
		if( !ev ) break;

		if( !com.strcmp( name, ev->name ))
		{
			if( ev->func != pfn )
				ev->func = pfn;
			return;
		}
	}

	CL_RegisterEvent( i, name, pfn );
}

/*
=============
pfnKillEvent

=============
*/
static void pfnKillEvents( int entnum, const char *eventname )
{
	int		i;
	event_state_t	*es;
	event_info_t	*ei;
	int		eventIndex = CL_EventIndex( eventname );

	if( eventIndex < 0 || eventIndex >= MAX_EVENTS )
		return;

	if( entnum < 0 || entnum > clgame.maxEntities )
		return;

	es = &cl.events;

	// find all events with specified index and kill it
	for( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		ei = &es->ei[i];

		if( ei->index != eventIndex || ei->entity_index != entnum )
			continue;
		CL_ResetEvent( ei );
	}
}

/*
=============
pfnPlaySound

=============
*/
void pfnPlaySound( int ent, float *org, int chan, const char *samp, float vol, float attn, int flags, int pitch )
{
	S_StartSound( org, ent, chan, S_RegisterSound( samp ), vol, attn, pitch, flags );
}

/*
=============
pfnStopSound

=============
*/
void pfnStopSound( int ent, int channel, const char *sample )
{
	S_StopSound( ent, channel, sample );
}

/*
=============
CL_FindModelIndex

=============
*/
int CL_FindModelIndex( const char *m )
{
	int	i;

	if( !m || !m[0] )
		return 0;

	for( i = 1; i < MAX_MODELS && cl.model_precache[i][0]; i++ )
	{
		if( !com.strcmp( cl.model_precache[i], m ))
			return i;
	}

	if( cls.state == ca_active )
	{
		// tell user about problem
		MsgDev( D_ERROR, "CL_ModelIndex: %s not precached\n", m );
	}
	return 0;
}

/*
=============
pfnIsLocal

=============
*/
int pfnIsLocal( int playernum )
{
	if( playernum == cl.playernum )
		return true;
	return false;
}

/*
=============
pfnLocalPlayerDucking

=============
*/
int pfnLocalPlayerDucking( void )
{
	return cl.frame.local.client.bInDuck;
}

/*
=============
pfnLocalPlayerViewheight

=============
*/
void pfnLocalPlayerViewheight( float *view_ofs )
{
	// predicted or smoothed
	if( view_ofs ) VectorCopy( cl.predicted_viewofs, view_ofs );
}

/*
=============
pfnLocalPlayerBounds

=============
*/
void pfnLocalPlayerBounds( int hull, float *mins, float *maxs )
{
	if( hull >= 0 && hull < 4 )
	{
		if( mins ) VectorCopy( clgame.pmove->player_mins[hull], mins );
		if( maxs ) VectorCopy( clgame.pmove->player_maxs[hull], maxs );
	}
}

/*
=============
pfnIndexFromTrace

=============
*/
int pfnIndexFromTrace( struct pmtrace_s *pTrace )
{
	if( pTrace->ent >= 0 && pTrace->ent < clgame.pmove->numphysent )
	{
		// return cl.entities number
		return clgame.pmove->physents[pTrace->ent].info;
	}
	return -1;
}

/*
=============
pfnGetPhysent

=============
*/
physent_t *pfnGetPhysent( int idx )
{
	if( idx >= 0 && idx < clgame.pmove->numphysent )
	{
		// return physent
		return &clgame.pmove->physents[idx];
	}
	return NULL;
}

/*
=============
pfnSetUpPlayerPrediction

=============
*/
void pfnSetUpPlayerPrediction( int dopred, int bIncludeLocalClient )
{
	// FIXME: implement
}

/*
=============
pfnPushPMStates

=============
*/
void pfnPushPMStates( void )
{
	clgame.oldcount = clgame.pmove->numphysent;
}

/*
=============
pfnPopPMStates

=============
*/
void pfnPopPMStates( void )
{
	clgame.pmove->numphysent = clgame.oldcount;
}

/*
=============
pfnSetTraceHull

=============
*/
void pfnSetTraceHull( int hull )
{
	clgame.trace_hull = bound( 0, hull, 3 );
}

/*
=============
pfnPlayerTrace

=============
*/
static void pfnPlayerTrace( float *start, float *end, int traceFlags, int ignore_pe, pmtrace_t *tr )
{
	if( !tr ) return;

	*tr = PM_PlayerTrace( clgame.pmove, start, end, traceFlags, clgame.trace_hull, ignore_pe, NULL );
}

/*
=============
pfnTraceTexture

=============
*/
static const char *pfnTraceTexture( int ground, float *vstart, float *vend )
{
	physent_t *pe;

	if( ground < 0 || ground >= clgame.pmove->numphysent )
		return NULL; // bad ground

	pe = &clgame.pmove->physents[ground];
	return PM_TraceTexture( pe, vstart, vend );
}
	
/*
=============
pfnStopAllSounds

=============
*/
void pfnStopAllSounds( int ent, int entchannel )
{
	S_StopSound( ent, entchannel, NULL );
}

/*
=============
pfnBoxVisible

=============
*/
static qboolean pfnBoxVisible( const vec3_t mins, const vec3_t maxs )
{
	return Mod_BoxVisible( mins, maxs, Mod_GetCurrentVis( ));
}

/*
=============
CL_LoadModel

=============
*/
model_t *CL_LoadModel( const char *modelname, int *index )
{
	int	idx;

	idx = CL_FindModelIndex( modelname );
	if( !idx ) return NULL;
	if( index ) *index = idx;
	
	return CM_ClipHandleToModel( idx );
}

int CL_AddEntity( int entityType, cl_entity_t *pEnt )
{
	if( !pEnt || !pEnt->index )
		return false;

	// let the render reject entity without model
	return CL_AddVisibleEntity( pEnt, entityType );
}

/*
=============
pfnGetGameDirectory

FIXME: use Info_ValueForKey( cl.serverinfo, "*gamedir" ) instead ?
=============
*/
const char *pfnGetGameDirectory( void )
{
	return GI->gamedir;
}

/*
=============
Key_LookupBinding

=============
*/
const char *Key_LookupBinding( const char *pBinding )
{
	return Key_KeynumToString( Key_GetKey( pBinding ));
}

/*
=============
pfnGetLevelName

=============
*/
static const char *pfnGetLevelName( void )
{
	static char	mapname[64];

	if( cls.state >= ca_connected )
		com.snprintf( mapname, sizeof( mapname ), "maps/%s.bsp", clgame.mapname );
	else mapname[0] = '\0'; // not in game

	return mapname;
}

/*
=============
pfnGetScreenFade

=============
*/
static void pfnGetScreenFade( struct screenfade_s *fade )
{
	if( fade ) *fade = clgame.fade;
}

/*
=============
pfnSetScreenFade

=============
*/
static void pfnSetScreenFade( struct screenfade_s *fade )
{
	if( fade ) clgame.fade = *fade;
}

/*
=============
pfnLoadMapSprite

=============
*/
model_t *pfnLoadMapSprite( const char *filename )
{
	int	i;

	if( !filename || !*filename )
	{
		MsgDev( D_ERROR, "CL_LoadMapSprite: bad name!\n" );
		return NULL;
	}

	// slot 0 isn't used
	for( i = 1; i < MAX_IMAGES; i++ )
	{
		if( !com.strcmp( clgame.sprites[i].name, filename ))
		{
			// prolonge registration
			clgame.sprites[i].needload = world.load_sequence;
			return &clgame.sprites[i];
		}
	}

	// find a free model slot spot
	for( i = 1; i < MAX_IMAGES; i++ )
		if( !clgame.sprites[i].name[0] ) break; // this is a valid spot

	if( i == MAX_IMAGES ) 
	{
		MsgDev( D_ERROR, "LoadMapSprite: can't load %s, MAX_HSPRITES limit exceeded\n", filename );
		return NULL;
	}

	// load new map sprite
	if( CL_LoadHudSprite( filename, &clgame.sprites[i], true ))
		return &clgame.sprites[i];
	return NULL;
}

/*
=============
PlayerInfo_ValueForKey

=============
*/
const char *PlayerInfo_ValueForKey( int playerNum, const char *key )
{
	// find the player
	if(( playerNum > cl.maxclients ) || ( playerNum < 1 ))
		return NULL;

	if(( cl.players[playerNum-1].name == NULL ) || (*(cl.players[playerNum-1].name) == 0 ))
		return NULL;

	return Info_ValueForKey( cl.players[playerNum-1].userinfo, key );
}

/*
=============
PlayerInfo_SetValueForKey

=============
*/
void PlayerInfo_SetValueForKey( const char *key, const char *value )
{
	// FIXME: implement
}

/*
=============
pfnGetPlayerUniqueID

=============
*/
qboolean pfnGetPlayerUniqueID( int iPlayer, char playerID[16] )
{
	// FIXME: implement
	return false;
}

/*
=============
pfnGetTrackerIDForPlayer

=============
*/
int pfnGetTrackerIDForPlayer( int playerSlot )
{
	playerSlot -= 1;	// make into a client index

	if( !cl.players[playerSlot].userinfo[0] || !cl.players[playerSlot].name[0] )
			return 0;
	return com.atoi( Info_ValueForKey( cl.players[playerSlot].userinfo, "*tracker" ));
}

/*
=============
pfnGetPlayerForTrackerID

=============
*/
int pfnGetPlayerForTrackerID( int trackerID )
{
	int	i;

	for( i = 0; i < MAX_CLIENTS; i++ )
	{
		if( !cl.players[i].userinfo[0] || !cl.players[i].name[0] )
			continue;

		if( com.atoi( Info_ValueForKey( cl.players[i].userinfo, "*tracker" )) == trackerID )
		{
			// make into a player slot
			return (i+1);
		}
	}
	return 0;
}

/*
=============
pfnServerCmdUnreliable

=============
*/
int pfnServerCmdUnreliable( char *szCmdString )
{
	if( !szCmdString || !*szCmdString )
		return 0;

	BF_WriteByte( &cls.netchan.message, clc_stringcmd );
	BF_WriteString( &cls.netchan.message, szCmdString );

	return 1;
}

/*
=============
pfnGetMousePos

=============
*/
void pfnGetMousePos( struct tagPOINT *ppt )
{
	GetCursorPos( ppt );
}

/*
=============
pfnSetMousePos

=============
*/
void pfnSetMousePos( int mx, int my )
{
	if( !mx && !my ) return;
	Sys_QueEvent( SE_MOUSE, mx, my, 0, NULL );
}

/*
=============
pfnSetMouseEnable

=============
*/
void pfnSetMouseEnable( qboolean fEnable )
{
	if( fEnable ) IN_ActivateMouse();
	else IN_DeactivateMouse();
}

/*
===============================================================================
	EffectsAPI Builtin Functions

===============================================================================
*/
/*
=================
pfnEnvShot

=================
*/
static void pfnEnvShot( const float *vieworg, const char *name, int skyshot )
{
	static vec3_t viewPoint;

	if( !name )
	{
		MsgDev( D_ERROR, "R_%sShot: bad name\n", skyshot ? "Sky" : "Env" );
		return; 
	}

	if( cls.scrshot_action != scrshot_inactive )
	{
		if( cls.scrshot_action != scrshot_skyshot && cls.scrshot_action != scrshot_envshot )
			MsgDev( D_ERROR, "R_%sShot: subsystem is busy, try later.\n", skyshot ? "Sky" : "Env" );
		return;
	}

	cls.envshot_vieworg = NULL; // use client view
	com.strncpy( cls.shotname, name, sizeof( cls.shotname ));

	if( vieworg )
	{
		// make sure what viewpoint don't temporare
		VectorCopy( vieworg, viewPoint );
		cls.envshot_vieworg = viewPoint;
	}

	// make request for envshot
	if( skyshot ) cls.scrshot_action = scrshot_skyshot;
	else cls.scrshot_action = scrshot_envshot;
}

/*
=================
TriApi implementation

=================
*/
/*
=================
Tri_DrawTriangles

callback from renderer
=================
*/
void Tri_DrawTriangles( int fTrans )
{
	if( fTrans )
	{
		if( !RI.refdef.onlyClientDraw )
		{
			CL_DrawBeams( true );
			CL_DrawParticles();
		}
		clgame.dllFuncs.pfnDrawTransparentTriangles ();
	}
	else
	{
		if( !RI.refdef.onlyClientDraw )
		{
			CL_DrawBeams( false );
		}
		clgame.dllFuncs.pfnDrawNormalTriangles ();
	}
}

/*
=============
TriBegin

begin triangle sequence
=============
*/
void TriBegin( int mode )
{
	switch( mode )
	{
	case TRI_TRIANGLES:
		mode = GL_TRIANGLES;
		break;
	case TRI_TRIANGLE_FAN:
		mode = GL_TRIANGLE_FAN;
		break;
	case TRI_QUADS:
		mode = GL_QUADS;
		break;
	case TRI_LINES:
		mode = GL_LINES;
		break;
	case TRI_TRIANGLE_STRIP:
		mode = GL_TRIANGLE_STRIP;
		break;
	case TRI_QUAD_STRIP:
		mode = GL_QUAD_STRIP;
		break;
	case TRI_POLYGON:
	default:	mode = GL_POLYGON;
		break;
	}

	pglBegin( mode );
}

/*
=============
TriEnd

draw triangle sequence
=============
*/
void TriEnd( void )
{
	pglEnd();
}

/*
=============
TriColor4f

=============
*/
void TriColor4f( float r, float g, float b, float a )
{
	rgba_t	rgba;

	rgba[0] = (byte)bound( 0, (r * 255.0f), 255 );
	rgba[1] = (byte)bound( 0, (g * 255.0f), 255 );
	rgba[2] = (byte)bound( 0, (b * 255.0f), 255 );
	rgba[3] = (byte)bound( 0, (a * 255.0f), 255 );
	pglColor4ub( rgba[0], rgba[1], rgba[2], rgba[3] );
}

/*
=============
TriColor4ub

=============
*/
void TriColor4ub( byte r, byte g, byte b, byte a )
{
	pglColor4ub( r, g, b, a );
}

/*
=============
TriTexCoord2f

=============
*/
void TriTexCoord2f( float u, float v )
{
	pglTexCoord2f( u, v );
}

/*
=============
TriVertex3fv

=============
*/
void TriVertex3fv( const float *v )
{
	pglVertex3fv( v );
}

/*
=============
TriVertex3f

=============
*/
void TriVertex3f( float x, float y, float z )
{
	pglVertex3f( x, y, z );
}

/*
=============
TriBrightness

=============
*/
void TriBrightness( float brightness )
{
	int	color;

	color = brightness * 255;
	pglColor4ub( color, color, color, 255 );
}

/*
=============
TriCullFace

=============
*/
void TriCullFace( TRICULLSTYLE mode )
{
	switch( mode )
	{
	case TRI_FRONT:
		mode = GL_FRONT;
		break;
	default:
		mode = GL_NONE;
		break;
	}
	GL_Cull( mode );
}

/*
=============
TriSpriteTexture

bind current texture
=============
*/
int TriSpriteTexture( model_t *pSpriteModel, int frame )
{
	int	gl_texturenum;

	if(( gl_texturenum = R_GetSpriteTexture( pSpriteModel, frame )) == 0 )
		return 0;

	GL_Bind( GL_TEXTURE0, gl_texturenum );

	return 1;
}

/*
=============
TriWorldToScreen

convert world coordinates (x,y,z) into screen (x, y)
=============
*/
int TriWorldToScreen( float *world, float *screen )
{
	int	retval = 0;

	if( !world || !screen )
		return retval;

	retval = R_WorldToScreen( world, screen );

	screen[0] =  0.5f * screen[0] * (float)cl.refdef.viewport[2];
	screen[1] = -0.5f * screen[1] * (float)cl.refdef.viewport[3];
	screen[0] += 0.5f * (float)cl.refdef.viewport[2];
	screen[1] += 0.5f * (float)cl.refdef.viewport[3];

	return retval;
}

/*
=============
TriFog

enables global fog on the level
=============
*/
void TriFog( float flFogColor[3], float flStart, float flEnd, int bOn )
{
//	re->Fog( flFogColor, flStart, flEnd, bOn );
}

/*
=============
TriScreenToWorld

convert screen coordinates (x,y) into world (x, y, z)
=============
*/
void TriScreenToWorld( float *screen, float *world )
{
	R_ScreenToWorld( screen, world );
} 

/*
=================
DemoApi implementation

=================
*/
/*
=================
Demo_IsRecording

=================
*/
static int Demo_IsRecording( void )
{
	return cls.demorecording;
}

/*
=================
Demo_IsPlayingback

=================
*/
static int Demo_IsPlayingback( void )
{
	return cls.demoplayback;
}

/*
=================
Demo_IsTimeDemo

=================
*/
static int Demo_IsTimeDemo( void )
{
	return cls.timedemo;
}

/*
=================
Demo_WriteBuffer

=================
*/
static void Demo_WriteBuffer( int size, byte *buffer )
{
	// FIXME: implement
}

/*
=================
NetworkApi implementation

=================
*/
/*
=================
NetAPI_InitNetworking

=================
*/
void NetAPI_InitNetworking( void )
{
	// FIXME: implement
}

/*
=================
NetAPI_InitNetworking

=================
*/
void NetAPI_Status( net_status_t *status )
{
	// FIXME: implement
}

/*
=================
NetAPI_SendRequest

=================
*/
void NetAPI_SendRequest( int context, int request, int flags, double timeout, netadr_t *remote_address, net_api_response_func_t response )
{
	// FIXME: implement
}

/*
=================
NetAPI_CancelRequest

=================
*/
void NetAPI_CancelRequest( int context )
{
	// FIXME: implement
}

/*
=================
NetAPI_CancelAllRequests

=================
*/
void NetAPI_CancelAllRequests( void )
{
	// FIXME: implement
}

/*
=================
NetAPI_AdrToString

=================
*/
char *NetAPI_AdrToString( netadr_t *a )
{
	return NET_AdrToString( *a );
}

/*
=================
NetAPI_CompareAdr

=================
*/
int NetAPI_CompareAdr( netadr_t *a, netadr_t *b )
{
	return NET_CompareAdr( *a, *b );
}

/*
=================
NetAPI_StringToAdr

=================
*/
int NetAPI_StringToAdr( char *s, netadr_t *a )
{
	return NET_StringToAdr( s, a );
}

/*
=================
NetAPI_ValueForKey

=================
*/
const char *NetAPI_ValueForKey( const char *s, const char *key )
{
	return Info_ValueForKey( s, key );
}

/*
=================
NetAPI_RemoveKey

=================
*/
void NetAPI_RemoveKey( char *s, const char *key )
{
	Info_RemoveKey( s, key );
}

/*
=================
NetAPI_SetValueForKey

=================
*/
void NetAPI_SetValueForKey( char *s, const char *key, const char *value, int maxsize )
{
	if( maxsize > MAX_INFO_STRING ) return;
	Info_SetValueForKey( s, key, value );
}


/*
=================
IVoiceTweak implementation

=================
*/
/*
=================
Voice_StartVoiceTweakMode

=================
*/
int Voice_StartVoiceTweakMode( void )
{
	// UNDONE: wait for voice implementation in snd_dx.dll
	// g-cont. may be move snd_dx.dll back into the engine ?
	return 0;
}

/*
=================
Voice_EndVoiceTweakMode

=================
*/
void Voice_EndVoiceTweakMode( void )
{
	// FIXME: implement
}

/*
=================
Voice_SetControlFloat

=================
*/	
void Voice_SetControlFloat( VoiceTweakControl iControl, float value )
{
	// FIXME: implement
}

/*
=================
Voice_GetControlFloat

=================
*/
float Voice_GetControlFloat( VoiceTweakControl iControl )
{
	// FIXME: implement
	return 1.0f;
}
			
static triangleapi_t gTriApi =
{
	TRI_API_VERSION,	
	GL_SetRenderMode,
	TriBegin,
	TriEnd,
	TriColor4f,
	TriColor4ub,
	TriTexCoord2f,
	TriVertex3fv,
	TriVertex3f,
	TriBrightness,
	TriCullFace,
	TriSpriteTexture,
	TriWorldToScreen,
	TriFog,
	R_ScreenToWorld,
};

static efx_api_t gEfxApi =
{
	CL_AllocParticle,
	CL_BlobExplosion,
	CL_Blood,
	CL_BloodSprite,
	CL_BloodStream,
	CL_BreakModel,
	CL_Bubbles,
	CL_BubbleTrail,
	CL_BulletImpactParticles,
	CL_EntityParticles,
	CL_Explosion,
	CL_FizzEffect,
	CL_FireField,
	CL_FlickerParticles,
	CL_FunnelSprite,
	CL_Implosion,
	CL_Large_Funnel,
	CL_LavaSplash,
	CL_MultiGunshot,
	CL_MuzzleFlash,
	CL_ParticleBox,
	CL_ParticleBurst,
	CL_ParticleExplosion,
	CL_ParticleExplosion2,
	CL_ParticleLine,
	CL_PlayerSprites,
	CL_Projectile,
	CL_RicochetSound,
	CL_RicochetSprite,
	CL_RocketFlare,
	CL_RocketTrail,
	CL_RunParticleEffect,
	CL_ShowLine,
	CL_SparkEffect,
	CL_SparkShower,
	CL_SparkStreaks,
	CL_Spray,
	CL_Sprite_Explode,
	CL_Sprite_Smoke,
	CL_Sprite_Spray,
	CL_Sprite_Trail,
	CL_Sprite_WallPuff,
	CL_StreakSplash,
	CL_TracerEffect,
	CL_UserTracerParticle,
	CL_TracerParticles,
	CL_TeleportSplash,
	CL_TempSphereModel,
	CL_TempModel,
	CL_DefaultSprite,
	CL_TempSprite,
	CL_DecalIndex,
	CL_DecalIndexFromName,
	CL_DecalShoot,
	CL_AttachTentToPlayer,
	CL_KillAttachedTents,
	CL_BeamCirclePoints,
	CL_BeamEntPoint,
	CL_BeamEnts,
	CL_BeamFollow,
	CL_BeamKill,
	CL_BeamLightning,
	CL_BeamPoints,
	CL_BeamRing,
	CL_AllocDlight,
	CL_AllocElight,
	CL_TempEntAlloc,
	CL_TempEntAllocNoModel,
	CL_TempEntAllocHigh,
	CL_TempEntAllocCustom,
	CL_GetPackedColor,
	CL_LookupColor,
	CL_DecalRemoveAll,
};

static event_api_t gEventApi =
{
	EVENT_API_VERSION,
	pfnPlaySound,
	pfnStopSound,
	CL_FindModelIndex,
	pfnIsLocal,
	pfnLocalPlayerDucking,
	pfnLocalPlayerViewheight,
	pfnLocalPlayerBounds,
	pfnIndexFromTrace,
	pfnGetPhysent,
	pfnSetUpPlayerPrediction,
	pfnPushPMStates,
	pfnPopPMStates,
	CL_SetSolidPlayers,
	pfnSetTraceHull,
	pfnPlayerTrace,
	CL_WeaponAnim,
	pfnPrecacheEvent,
	CL_PlaybackEvent,
	pfnTraceTexture,
	pfnStopAllSounds,
	pfnKillEvents,
};

static demo_api_t gDemoApi =
{
	Demo_IsRecording,
	Demo_IsPlayingback,
	Demo_IsTimeDemo,
	Demo_WriteBuffer,
};

static net_api_t gNetApi =
{
	NetAPI_InitNetworking,
	NetAPI_Status,
	NetAPI_SendRequest,
	NetAPI_CancelRequest,
	NetAPI_CancelAllRequests,
	NetAPI_AdrToString,
	NetAPI_CompareAdr,
	NetAPI_StringToAdr,
	NetAPI_ValueForKey,
	NetAPI_RemoveKey,
	NetAPI_SetValueForKey,
};

static IVoiceTweak gVoiceApi =
{
	Voice_StartVoiceTweakMode,
	Voice_EndVoiceTweakMode,
	Voice_SetControlFloat,
	Voice_GetControlFloat,
};

// engine callbacks
static cl_enginefunc_t gEngfuncs = 
{
	pfnSPR_Load,
	pfnSPR_Frames,
	pfnSPR_Height,
	pfnSPR_Width,
	pfnSPR_Set,
	pfnSPR_Draw,
	pfnSPR_DrawHoles,
	pfnSPR_DrawAdditive,
	pfnSPR_EnableScissor,
	pfnSPR_DisableScissor,
	pfnSPR_GetList,
	pfnFillRGBA,
	pfnGetScreenInfo,
	pfnSetCrosshair,
	pfnCvar_RegisterVariable,
	pfnCVarGetValue,
	pfnCVarGetString,
	pfnAddCommand,
	pfnHookUserMsg,
	pfnServerCmd,
	pfnClientCmd,
	pfnGetPlayerInfo,
	pfnPlaySoundByName,
	pfnPlaySoundByIndex,
	AngleVectors,
	pfnTextMessageGet,
	pfnDrawCharacter,
	pfnDrawConsoleString,
	pfnDrawSetTextColor,
	Con_DrawStringLen,
	pfnConsolePrint,
	pfnCenterPrint,
	pfnGetWindowCenterX,
	pfnGetWindowCenterY,
	pfnGetViewAngles,
	pfnSetViewAngles,
	CL_GetMaxClients,
	pfnCVarSetValue,
	pfnCmd_Argc,
	pfnCmd_Argv,
	Con_Printf,
	Con_DPrintf,
	Con_NPrintf,
	Con_NXPrintf,
	pfnPhysInfo_ValueForKey,
	pfnServerInfo_ValueForKey,
	pfnGetClientMaxspeed,
	pfnCheckParm,
	pfnKey_Event,
	pfnGetMousePosition,
	pfnIsNoClipping,
	CL_GetLocalPlayer,
	pfnGetViewModel,
	pfnGetEntityByIndex,
	pfnGetClientTime,
	pfnCalcShake,
	pfnApplyShake,
	pfnPointContents,
	pfnWaterEntity,
	pfnTraceLine,
	CL_LoadModel,
	CL_AddEntity,
	CL_GetSpritePointer,
	pfnPlaySoundByNameAtLocation,
	pfnPrecacheEvent,
	CL_PlaybackEvent,
	CL_WeaponAnim,
	pfnRandomFloat,
	pfnRandomLong,
	pfnHookEvent,
	Con_Visible,
	pfnGetGameDirectory,
	pfnCVarGetPointer,
	Key_LookupBinding,
	pfnGetLevelName,
	pfnGetScreenFade,
	pfnSetScreenFade,
	VGui_GetPanel,
	VGui_ViewportPaintBackground,
	COM_LoadFile,
	COM_ParseFile,
	pfnFreeFile,
	&gTriApi,
	&gEfxApi,
	&gEventApi,
	&gDemoApi,
	&gNetApi,
	&gVoiceApi,
	pfnIsSpectateOnly,
	pfnLoadMapSprite,
	COM_AddAppDirectoryToSearchPath,
	COM_ExpandFilename,
	PlayerInfo_ValueForKey,
	PlayerInfo_SetValueForKey,
	pfnGetPlayerUniqueID,
	pfnGetTrackerIDForPlayer,
	pfnGetPlayerForTrackerID,
	pfnServerCmdUnreliable,
	pfnGetMousePos,
	pfnSetMousePos,
	pfnSetMouseEnable,
};

void CL_UnloadProgs( void )
{
	if( !clgame.hInstance ) return;

	CL_FreeEdicts();
	CL_FreeTempEnts();
	CL_FreeViewBeams();
	CL_FreeParticles();
	VGui_Shutdown();

	clgame.dllFuncs.pfnShutdown();

	FS_FreeLibrary( clgame.hInstance );
	Mem_FreePool( &cls.mempool );
	Mem_FreePool( &clgame.mempool );
	Mem_Set( &clgame, 0, sizeof( clgame ));
}

qboolean CL_LoadProgs( const char *name )
{
	static playermove_t		gpMove;
	const dllfunc_t		*func;

	if( clgame.hInstance ) CL_UnloadProgs();

	// setup globals
	cl.refdef.movevars = &clgame.movevars;

	// initialize PlayerMove
	clgame.pmove = &gpMove;

	cls.mempool = Mem_AllocPool( "Client Static Pool" );
	clgame.mempool = Mem_AllocPool( "Client Edicts Zone" );
	clgame.entities = NULL;

	// NOTE: important stuff!
	// vgui must startup BEFORE loading client.dll to avoid get error ERROR_NOACESS
	// during LoadLibrary
	VGui_Startup ();
	
	clgame.hInstance = FS_LoadLibrary( name, false );
	if( !clgame.hInstance ) return false;

	// clear exports
	for( func = cdll_exports; func && func->name; func++ )
		*func->func = NULL;

	for( func = cdll_exports; func && func->name != NULL; func++ )
	{
		// functions are cleared before all the extensions are evaluated
		if(!( *func->func = (void *)FS_GetProcAddress( clgame.hInstance, func->name )))
		{
          		MsgDev( D_NOTE, "CL_LoadProgs: failed to get address of %s proc\n", func->name );
			FS_FreeLibrary( clgame.hInstance );
			clgame.hInstance = NULL;
			return false;
		}
	}

	// clear new exports
	for( func = cdll_new_exports; func && func->name; func++ )
		*func->func = NULL;

	for( func = cdll_new_exports; func && func->name != NULL; func++ )
	{
		// functions are cleared before all the extensions are evaluated
		// NOTE: new exports can be missed without stop the engine
		if(!( *func->func = (void *)FS_GetProcAddress( clgame.hInstance, func->name )))
          		MsgDev( D_NOTE, "CL_LoadProgs: failed to get address of %s proc\n", func->name );
	}

	if( !clgame.dllFuncs.pfnInitialize( &gEngfuncs, CLDLL_INTERFACE_VERSION ))
	{
		FS_FreeLibrary( clgame.hInstance );
		MsgDev( D_NOTE, "CL_LoadProgs: can't init client API\n" );
		clgame.hInstance = NULL;
		return false;
	}

	if( !CL_InitStudioAPI( ))
	{
		FS_FreeLibrary( clgame.hInstance );
		MsgDev( D_NOTE, "CL_LoadProgs: can't init studio API\n" );
		clgame.hInstance = NULL;
		return false;
	}

	Cvar_Get( "cl_lw", "1", CVAR_ARCHIVE|CVAR_USERINFO, "enable client weapon predicting" );

	clgame.maxEntities = GI->max_edicts; // merge during loading
	CL_InitCDAudio( "media/cdaudio.txt" );
	CL_InitTitles( "titles.txt" );
	CL_InitParticles ();
	CL_InitViewBeams ();
	CL_InitTempEnts ();

	// initialize game
	clgame.dllFuncs.pfnInit();

	// initialize pm_shared
	CL_InitClientMove();

	return true;
}