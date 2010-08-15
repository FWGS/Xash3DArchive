//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        cl_game.c - client dlls interaction
//=======================================================================

#include "common.h"
#include "client.h"
#include "byteorder.h"
#include "matrix_lib.h"
#include "const.h"
#include "protocol.h"
#include "triangle_api.h"
#include "effects_api.h"
#include "event_flags.h"
#include "pm_defs.h"

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

	if( index < 0 || index > clgame.globals->numEntities )
	{
		if( index == -1 )
			return NULL;

		MsgDev( D_ERROR, "CL_GetEntityByIndex: invalid entindex %i\n", index );
		return NULL;
	}

	if( EDICT_NUM( index )->index == -1 )
		return NULL;
	return EDICT_NUM( index );
}

/*
=============
CL_AllocString

=============
*/
string_t CL_AllocString( const char *szValue )
{
	return StringTable_SetString( clgame.hStringTable, szValue );
}		

/*
=============
CL_GetString

=============
*/
const char *CL_GetString( string_t iString )
{
	return StringTable_GetString( clgame.hStringTable, iString );
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
CL_GetPlayerInfo

get player info by render request
====================
*/
player_info_t *CL_GetPlayerInfo( int playerIndex )
{
	if( playerIndex < 0 || playerIndex >= clgame.globals->maxClients )
		return NULL;

	return &cl.players[playerIndex];
}

/*
====================
StudioEvent

Event callback for studio models
====================
*/
void CL_StudioEvent( mstudioevent_t *event, cl_entity_t *pEdict )
{
	clgame.dllFuncs.pfnStudioEvent( event, pEdict );
}

/*
====================
Studio_FxTransform

apply fxtransforms for each studio bone
====================
*/
void CL_StudioFxTransform( cl_entity_t *ent, float transform[4][4] )
{
	clgame.dllFuncs.pfnStudioFxTransform( ent, transform );
}

/*
=============
CL_GetLerpFrac

=============
*/
float CL_GetLerpFrac( void )
{
	return cl.lerpFrac;
}

/*
================
CL_FadeAlpha
================
*/
void CL_FadeAlpha( int starttime, int endtime, rgba_t color )
{
	int	time, fade_time;

	if( starttime == 0 )
	{
		MakeRGBA( color, 255, 255, 255, 255 );
		return;
	}

	time = cls.realtime - starttime;

	if( time >= endtime )
	{
		MakeRGBA( color, 255, 255, 255, 0 );
		return;
	}

	// fade time is 1/4 of endtime
	fade_time = endtime / 4;
	fade_time = bound( 300, fade_time, 10000 );

	color[0] = color[1] = color[2] = 255;

	// fade out
	if(( endtime - time ) < fade_time )
		color[3] = bound( 0, (( endtime - time ) * ( 1.0f / fade_time )) * 255, 255 );
	else color[3] = 255;
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
	clgame.centerPrint.time = cl.frame.servertime; // allow pause for centerprint
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
	xscale = clgame.scrInfo.iRealWidth / (float)clgame.scrInfo.iWidth;
	yscale = clgame.scrInfo.iRealHeight / (float)clgame.scrInfo.iHeight;

	if( x ) *x *= xscale;
	if( y ) *y *= yscale;
	if( w ) *w *= xscale;
	if( h ) *h *= yscale;
}

static bool SPR_Scissor( float *x, float *y, float *width, float *height, float *u0, float *v0, float *u1, float *v1 )
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

	if( !re ) return;

	if( width == -1 && height == -1 )
	{
		int	w, h;

		// assume we get sizes from image
		re->GetParms( &w, &h, NULL, frame, clgame.ds.hSprite );

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
	re->DrawStretchPic( x, y, width, height, s1, t1, s2, t2, clgame.ds.hSprite );
	re->SetColor( NULL );
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
	rgba_t	color;

	if( !clgame.centerPrint.time )
		return;

	CL_FadeAlpha( clgame.centerPrint.time, scr_centertime->value * 1000, color );

	if( *(int *)color == 0x00FFFFFF ) 
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
				re->SetColor( color );
				clgame.ds.hSprite = cls.creditsFont.hFontTexture;
				re->SetParms( clgame.ds.hSprite, kRenderTransAdd, 0 );
				SPR_DrawGeneric( 0, x, y, -1, -1, &cls.creditsFont.fontRc[ch] );
			}
			x = next;
		}
		y += clgame.scrInfo.iCharHeight;
	}
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
	if( cls.state == ca_active )
	{
		cl_entity_t	*player = EDICT_NUM( cl.playernum + 1 );

		if( player ) return player;
		Host_Error( "CL_GetLocalPlayer: invalid edict\n" );
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
	return clgame.globals->maxClients;
}

void CL_DrawCrosshair( void )
{
	int		x, y, width, height;
	cl_entity_t	*pPlayer;

	if( !re || clgame.ds.hCrosshair <= 0 || cl.refdef.crosshairangle[2] || !cl_crosshair->integer )
		return;

	pPlayer = CL_GetLocalPlayer();

	if( cl.frame.cd.deadflag != DEAD_NO || cl.frame.cd.flags & FL_FROZEN )
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
		re->WorldToScreen( point, screen );

		x += 0.5f * screen[0] * clgame.scrInfo.iRealWidth + 0.5f;
		y += 0.5f * screen[1] * clgame.scrInfo.iRealHeight + 0.5f;
	}

	clgame.ds.hSprite = clgame.ds.hCrosshair;
	re->SetColor( clgame.ds.rgbaCrosshair );
	re->SetParms( clgame.ds.hSprite, kRenderTransAlpha, 0 );
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

	re->GetParms( &width, &height, NULL, 0, cls.loadingBar );
	x = ( 640 - width ) >> 1;
	y = ( 480 - height) >> 1;

	xscale = scr_width->integer / 640.0f;
	yscale = scr_height->integer / 480.0f;

	x *= xscale;
	y *= yscale;
	width *= xscale;
	height *= yscale;

	MakeRGBA( color, 128, 128, 128, 255 );
	re->SetColor( color );
	re->SetParms( cls.loadingBar, kRenderTransTexture, 0 );
	re->DrawStretchPic( x, y, width, height, 0, 0, 1, 1, cls.loadingBar );

	step = (float)width / 100.0f;
	right = (int)ceil( percent * step );
	s2 = (float)right / width;
	width = right;
	
	MakeRGBA( color, 208, 152, 0, 255 );
	re->SetColor( color );
	re->SetParms( cls.loadingBar, kRenderTransTexture, 0 );
	re->DrawStretchPic( x, y, width, height, 0, 0, s2, 1, cls.loadingBar );
	re->SetColor( NULL );
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

	if( !re ) return;

	re->GetParms( &width, &height, NULL, 0, cls.pauseIcon );
	x = ( 640 - width ) >> 1;
	y = ( 480 - height) >> 1;

	xscale = scr_width->integer / 640.0f;
	yscale = scr_height->integer / 480.0f;

	x *= xscale;
	y *= yscale;
	width *= xscale;
	height *= yscale;

	re->SetColor( NULL );
	re->SetParms( cls.pauseIcon, kRenderTransTexture, 0 );
	re->DrawStretchPic( x, y, width, height, 0, 0, 1, 1, cls.pauseIcon );
}

void CL_DrawHUD( int state )
{
	if( state == CL_ACTIVE && !cl.video_prepped )
		state = CL_LOADING;

	if( state == CL_ACTIVE && cl.refdef.paused )
		state = CL_PAUSED;

	if( state == CL_ACTIVE )
	{
		clgame.dllFuncs.pfnRedraw( cl_time(), false );
	}

	switch( state )
	{
	case CL_PAUSED:
		CL_DrawPause();
		// intentionally fallthrough
	case CL_ACTIVE:
		CL_DrawCrosshair ();
		CL_DrawCenterPrint ();
		break;
	case CL_LOADING:
		CL_DrawLoading( scr_loading->value );
		break;
	case CL_CHANGELEVEL:
		CL_DrawLoading( 100.0f );
		break;
	}
}

void CL_LinkUserMessage( char *pszName, const int svc_num, int iSize )
{
	int	i, j;

	if( !pszName || !*pszName )
		return; // ignore blank names

	// see if already hooked
	for( i = 0; i < MAX_USER_MESSAGES && clgame.msg[i].name[0]; i++ )
	{
		// hooked
		if( !com.strcmp( clgame.msg[i].name, pszName ) && clgame.msg[i].func )
			return;
	}

	if( i == MAX_USER_MESSAGES ) 
	{
		MsgDev( D_ERROR, "LINK_USER_MSG: user messages limit exceeded\n" );
		return;
	}

	// search for hook
	for( j = 0; j < MAX_USER_MESSAGES && clgame.hook[j].name[0]; j++ )
	{
		// find the hook
		if( !com.strcmp( clgame.hook[j].name, pszName ))
			break;
	}

	// register new message
	com.strncpy( clgame.msg[i].name, pszName, sizeof( clgame.msg[i].name ));
	clgame.msg[i].number = svc_num;
	clgame.msg[i].size = iSize;
	clgame.msg[i].func = clgame.hook[j].func;
}

static void CL_RegisterEvent( int lastnum, const char *szEvName, pfnEventHook func )
{
	user_event_t	*ev;

	if( lastnum == MAX_EVENTS )
	{
		MsgDev( D_ERROR, "CL_RegisterEvent: MAX_EVENTS hit!!!\n" );
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

void CL_InitEntity( cl_entity_t *pEdict )
{
	ASSERT( pEdict );

	pEdict->index = NUM_FOR_EDICT( pEdict );
}

void CL_FreeEntity( cl_entity_t *pEdict )
{
	ASSERT( pEdict );

	// already freed ?
	if( pEdict->index == -1 )
		return;

	clgame.dllFuncs.pfnUpdateOnRemove( pEdict );

	// clear curstate
	Mem_Set( &pEdict->curstate, 0, sizeof( pEdict->curstate ));
	pEdict->index = -1;	// freed
}

void CL_InitWorld( void )
{
	cl_entity_t	*ent;
	int	i;

	ent = EDICT_NUM( 0 );
	ent->index = NUM_FOR_EDICT( ent );
	ent->curstate.modelindex = 1;	// world model
	ent->curstate.solid = SOLID_BSP;
	ent->curstate.movetype = MOVETYPE_PUSH;
	clgame.globals->numEntities = 1;
	clgame.globals->serverflags = com.atoi( cl.configstrings[CS_SERVERFLAGS] );

	for( i = 0; i < clgame.globals->maxClients; i++ )
	{
		// setup all clients
		ent = EDICT_NUM( i + 1 );
		CL_InitEntity( ent );
	}
}

void CL_InitEdicts( void )
{
	cl_entity_t	*e;
	int	i;

	ASSERT( clgame.entities == NULL );

	CL_UPDATE_BACKUP = ( clgame.globals->maxClients == 1 ) ? SINGLEPLAYER_BACKUP : MULTIPLAYER_BACKUP;
	cl.frames = Mem_Alloc( clgame.mempool, sizeof( frame_t ) * CL_UPDATE_BACKUP );

	clgame.entities = Mem_Alloc( clgame.mempool, sizeof( cl_entity_t ) * clgame.globals->maxEntities );

	for( i = 0, e = clgame.entities; i < clgame.globals->maxEntities; i++, e++ )
		e->index = -1; // mark all entities as freed
}

void CL_FreeEdicts( void )
{
	int	i;
	cl_entity_t	*ent;

	if( clgame.entities )
	{
		for( i = 0; i < clgame.globals->maxEntities; i++ )
		{
			ent = EDICT_NUM( i );
			if( ent->index <= 0 ) continue;
			CL_FreeEntity( ent );
		}
		Mem_Free( clgame.entities );
	}

	if( cl.frames ) Mem_Free( cl.frames );

	// clear globals
	StringTable_Clear( clgame.hStringTable );

	clgame.globals->numEntities = 0;
	clgame.entities = NULL;
	cl.frames = NULL;
}

/*
===============================================================================
	CGame Builtin Functions

===============================================================================
*/
/*
=========
pfnSPR_Load

=========
*/
static HSPRITE pfnSPR_Load( const char *szPicName )
{
	if( !re ) return 0; // render not initialized
	if( !szPicName || !*szPicName )
	{
		MsgDev( D_ERROR, "CL_LoadSprite: bad name!\n" );
		return 0;
	}
	return re->RegisterShader( szPicName, SHADER_SPRITE );
}

/*
=========
pfnSPR_Frames

=========
*/
static int pfnSPR_Frames( HSPRITE hPic )
{
	int	numFrames;

	if( !re ) return 1;
	re->GetParms( NULL, NULL, &numFrames, 0, hPic );

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

	if( !re ) return 0;
	re->GetParms( NULL, &sprHeight, NULL, frame, hPic );

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

	if( !re ) return 0;
	re->GetParms( &sprWidth, NULL, NULL, frame, hPic );

	return sprWidth;
}

/*
=========
pfnSPR_Set

=========
*/
static void pfnSPR_Set( HSPRITE hPic, int r, int g, int b, int a )
{
	rgba_t	color;

	if( !re ) return; // render not initialized

	clgame.ds.hSprite = hPic;
	MakeRGBA( color, r, g, b, a );
	re->SetColor( color );
}

/*
=========
pfnSPR_Draw

=========
*/
static void pfnSPR_Draw( int frame, int x, int y, int width, int height, const wrect_t *prc )
{
	if( !re ) return; // render not initialized

	re->SetParms( clgame.ds.hSprite, kRenderNormal, frame );
	SPR_DrawGeneric( frame, x, y, width, height, prc );
}

/*
=========
pfnSPR_DrawTrans

=========
*/
static void pfnSPR_DrawTrans( int frame, int x, int y, int width, int height, const wrect_t *prc )
{
	if( !re ) return; // render not initialized

	re->SetParms( clgame.ds.hSprite, kRenderTransColor, frame );
	SPR_DrawGeneric( frame, x, y, width, height, prc );
}

/*
=========
pfnSPR_DrawHoles

=========
*/
static void pfnSPR_DrawHoles( int frame, int x, int y, int width, int height, const wrect_t *prc )
{
	if( !re ) return; // render not initialized

	re->SetParms( clgame.ds.hSprite, kRenderTransAlpha, frame );
	SPR_DrawGeneric( frame, x, y, width, height, prc );
}

/*
=========
pfnSPR_DrawAdditive

=========
*/
static void pfnSPR_DrawAdditive( int frame, int x, int y, int width, int height, const wrect_t *prc )
{
	if( !re ) return; // render not initialized

	re->SetParms( clgame.ds.hSprite, kRenderTransAdd, frame );
	SPR_DrawGeneric( frame, x, y, width, height, prc );
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
	pList = Mem_Alloc( clgame.mempool, sizeof( *pList ) * numSprites );

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
		MsgDev( D_WARN, "SPR_GetList: unexpected end of %s (%i should be %i)\n", psz, index, numSprites );

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

	if( !re ) return;

	MakeRGBA( color, r, g, b, a );
	re->SetColor( color );

	SPR_AdjustSize( (float *)&x, (float *)&y, (float *)&width, (float *)&height );

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
	clgame.scrInfo.iRealWidth = scr_width->integer;
	clgame.scrInfo.iRealHeight = scr_height->integer;

	if( pscrinfo && pscrinfo->iFlags & SCRINFO_VIRTUALSPACE )
	{
		// virtual screen space 640x480
		clgame.scrInfo.iWidth = 640;
		clgame.scrInfo.iHeight = 480;
		clgame.scrInfo.iFlags |= SCRINFO_VIRTUALSPACE;
	}
	else
	{
		clgame.scrInfo.iWidth = scr_width->integer;
		clgame.scrInfo.iHeight = scr_height->integer;
		clgame.scrInfo.iFlags &= ~SCRINFO_VIRTUALSPACE;
	}

	if( !pscrinfo ) return 0;
	*pscrinfo = clgame.scrInfo;	// copy screeninfo out

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
	clgame.ds.hCrosshair = hspr;
	clgame.ds.rcCrosshair = rc;
}

/*
=============
pfnHookUserMsg

=============
*/
static void pfnHookUserMsg( const char *pszName, pfnUserMsgHook pfn )
{
	int	i;

	// ignore blank names or null callbacks
	if( !pszName || !*pszName )
		return;	

	// message 0 is reserved for svc_bad
	for( i = 0; i < MAX_USER_MESSAGES && clgame.hook[i].name[0]; i++ )
	{
		// see if already hooked
		if( !com.strcmp( clgame.hook[i].name, pszName ))
			return;
	}

	if( i == MAX_USER_MESSAGES ) 
	{
		MsgDev( D_ERROR, "HOOK_USER_MSG: user messages limit exceeded\n" );
		return;
	}

	// hook new message
	com.strncpy( clgame.hook[i].name, pszName, sizeof( clgame.hook[i].name ));
	clgame.hook[i].func = pfn;
}

/*
=============
pfnServerCmd

=============
*/
static void pfnServerCmd( const char *szCmdString )
{
	// server command adding in cmds queue
	Cbuf_AddText( va( "cmd %s", szCmdString ));
}

/*
=============
pfnClientCmd

=============
*/
static void pfnClientCmd( const char *szCmdString )
{
	// client command executes immediately
	Cbuf_AddText( szCmdString );
}

/*
=============
pfnGetPlayerInfo

=============
*/
static void pfnGetPlayerInfo( int ent_num, hud_player_info_t *pinfo )
{
	player_info_t	*player;

	ent_num -= 1; // player list if offset by 1 from ents

	if( ent_num >= clgame.globals->maxClients || ent_num < 0 || !cl.players[ent_num].name[0] )
	{
		Mem_Set( pinfo, 0, sizeof( *pinfo ));
		return;
	}

	player = &cl.players[ent_num];
	pinfo->thisplayer = ( ent_num == cl.playernum ) ? true : false;

	pinfo->name = player->name;
	pinfo->model = player->model;
		
	pinfo->ping = com.atoi( Info_ValueForKey( player->userinfo, "ping" ));
	pinfo->spectator = com.atoi( Info_ValueForKey( player->userinfo, "spectator" ));
	pinfo->packetloss = com.atoi( Info_ValueForKey( player->userinfo, "loss" ));
	pinfo->topcolor = com.atoi( Info_ValueForKey( player->userinfo, "topcolor" ));
	pinfo->bottomcolor = com.atoi( Info_ValueForKey( player->userinfo, "bottomcolor" ));
}

/*
=============
pfnPlaySoundByName

=============
*/
static void pfnPlaySoundByName( const char *szSound, float volume, int pitch, const float *org )
{
	S_StartSound( org, cl.refdef.viewentity, CHAN_AUTO, S_RegisterSound( szSound ), volume, ATTN_NORM, pitch, 0 );
}

/*
=============
pfnPlaySoundByIndex

=============
*/
static void pfnPlaySoundByIndex( int iSound, float volume, int pitch, const float *org )
{
	// make sure what we in-bounds
	iSound = bound( 0, iSound, MAX_SOUNDS );

	if( cl.sound_precache[iSound] == 0 )
	{
		MsgDev( D_ERROR, "CL_PlaySoundByIndex: invalid sound handle %i\n", iSound );
		return;
	}
	S_StartSound( org, cl.refdef.viewentity, CHAN_AUTO, cl.sound_precache[iSound], volume, ATTN_NORM, pitch, 0 );
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
	number &= 255;

	if( !cls.creditsFont.valid )
		return 0;

	if( number < 32 ) return 0;
	if( y < -clgame.scrInfo.iCharHeight )
		return 0;

	pfnSPR_Set( cls.creditsFont.hFontTexture, r, g, b, 255 );
	pfnSPR_DrawAdditive( 0, x, y, -1, -1, &cls.creditsFont.fontRc[number] );
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
	if( *string != 1 ) Con_Print( string ); // show notify
	else Con_Print( va( "[skipnotify]%s", string + 1 )); // skip notify
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
pfnMemAlloc

=========
*/
static void *pfnMemAlloc( size_t cb, const char *filename, const int fileline )
{
	return com.malloc( clgame.private, cb, filename, fileline );
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
	return Info_ValueForKey( cl.frame.cd.physinfo, key );
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
	return cl.frame.cd.maxspeed;
}

/*
=============
pfnGetModelPtr

returns pointer to a studiomodel
=============
*/
static void *pfnGetModelPtr( cl_entity_t *pEdict )
{
	if( !pEdict || pEdict->index == -1 )
		return NULL;

	return Mod_Extradata( pEdict->curstate.modelindex );
}

/*
=============
pfnGetViewModel

can return NULL
=============
*/
static cl_entity_t* pfnGetViewModel( void )
{
	return &clgame.viewent;
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
pfnIsSpectateOnly

=============
*/
static int pfnIsSpectateOnly( void )
{
	return com.atoi( Info_ValueForKey( Cvar_Userinfo(), "spectator" )) ? true : false;
}

/*
=============
pfnPointContents

=============
*/
static int pfnPointContents( const float *rgflVector, int *truecont )
{
	return CM_PointContents( rgflVector );	// FIXME
}

/*
=============
pfnWaterEntity

=============
*/
static cl_entity_t *pfnWaterEntity( const float *rgflPos )
{
	cl_entity_t	*pWater, *touch[MAX_EDICTS];
	int	i, num;

	if( !rgflPos ) return NULL;

	// grab contents from all the water entities
	num = 0;// FIXME: CL_AreaEdicts( rgflPos, rgflPos, touch, MAX_EDICTS, AREA_CUSTOM );

	for( i = 0; i < num; i++ )
	{
		pWater = touch[i];

		if( !pWater || pWater->index == -1 )
			continue;

		if( pWater->curstate.solid != SOLID_NOT || pWater->curstate.skin == CONTENTS_NONE )
			continue; // invalid water ?

		// return first valid water entity
		return pWater;
	}
	return NULL;
}

/*
=============
pfnTraceLine

=============
*/
static pmtrace_t *pfnTraceLine( const float *start, const float *end, int flags, int usehull, int ignore_pe )
{
	static pmtrace_t	trace;

	// FIXME: implement traceline
	Mem_Set( &trace, 0, sizeof( pmtrace_t ));
	VectorCopy( end, trace.endpos );
	trace.fraction = 1.0f;
	trace.allsolid = true;
	trace.hitgroup = -1;

	return &trace;
}

/*
=============
pfnPrecacheEvent

=============
*/
static word pfnPrecacheEvent( int type, const char* psz )
{
	return CL_PrecacheEvent( psz );
}

/*
=============
pfnPlaybackEvent

=============
*/
static void pfnPlaybackEvent( int flags, const cl_entity_t *pInvoker, word eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
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
	if( !CL_PrecacheEvent( cl.configstrings[CS_EVENTS+eventindex] ))
	{
		MsgDev( D_ERROR, "CL_PlaybackEvent: event %i was not precached\n", eventindex );
		return;		
	}

	flags |= FEV_CLIENT; // it's a client event
	flags &= ~(FEV_NOTHOST|FEV_HOSTONLY|FEV_GLOBAL);

	if( delay < 0.0f )
		delay = 0.0f; // fixup negative delays

	if( pInvoker ) invokerIndex = NUM_FOR_EDICT( pInvoker );

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
		if( VectorIsNull( args.origin )) 
			VectorCopy( pInvoker->curstate.origin, args.origin );
		if( VectorIsNull( args.angles )) 
			VectorCopy( pInvoker->curstate.angles, args.angles );
		VectorCopy( pInvoker->curstate.velocity, args.velocity );
		args.ducking = pInvoker->curstate.usehull;
	}

	CL_QueueEvent( flags, eventindex, delay, &args );
}

/*
=============
pfnHookEvent

=============
*/
static void pfnHookEvent( const char *name, pfnEventHook pfn )
{
	word		event_index = CL_PrecacheEvent( name );
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
	int		eventIndex = CL_PrecacheEvent( eventname );

	if( eventIndex < 0 || eventIndex >= MAX_EVENTS )
		return;
	if( entnum < 0 || entnum > clgame.globals->maxEntities )
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
pfnFindModelIndex

=============
*/
int pfnFindModelIndex( const char *model )
{
	int	i;

	if( !model || !*model ) return 0;
	for( i = 1; i < MAX_MODELS; i++ )
	{
		if( !com.strcmp( model, cl.configstrings[CS_MODELS+i] ))
			return i;
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
	return cl.frame.cd.bInDuck;
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
	// FIXME: implement
	return -1;
}

/*
=============
pfnGetPhysent

=============
*/
physent_t *pfnGetPhysent( int idx )
{
	// FIXME: implement
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
	// FIXME: implement
}

/*
=============
pfnPopPMStates

=============
*/
void pfnPopPMStates( void )
{
	// FIXME: implement
}

/*
=============
pfnSetSolidPlayers

=============
*/
void pfnSetSolidPlayers( int playernum )
{
	// FIXME: implement
}

/*
=============
pfnSetTraceHull

=============
*/
void pfnSetTraceHull( int hull )
{
	// FIXME: implement
}

/*
=============
pfnPlayerTrace

=============
*/
static void pfnPlayerTrace( float *start, float *end, int traceFlags, int ignore_pe, pmtrace_t *tr )
{
	// FIXME: implement
}

/*
=============
pfnTraceTexture

=============
*/
static const char *pfnTraceTexture( int ground, const float *v1, const float *v2 )
{
	// FIXME: implement
	return NULL;
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
static bool pfnBoxVisible( const vec3_t mins, const vec3_t maxs )
{
	if( !re ) return false;
	return CM_BoxVisible( mins, maxs, re->GetCurrentVis());
}

/*
=============
pfnGetModFrames

=============
*/
int pfnGetModFrames( int modelIndex )
{
	int	numFrames = 1;

	Mod_GetFrames( modelIndex, &numFrames );

	return numFrames;
}
	
/*
=============
VGui_GetPanel

=============
*/
void *VGui_GetPanel( void )
{
	// UNDONE: wait for version 0.75
	return NULL;
}

/*
=============
VGui_ViewportPaintBackground

=============
*/
void VGui_ViewportPaintBackground( int extents[4] )
{
	// UNDONE: wait for version 0.75
}

/*
=============
pfnIsInGame

=============
*/
int pfnIsInGame( void )
{
	return ( cls.key_dest == key_game ) ? true : false;
}

/*
===============================================================================
	EffectsAPI Builtin Functions

===============================================================================
*/
/*
=================
pfnDecalIndexFromName

=================
*/
int pfnDecalIndexFromName( const char *szDecalName )
{
	int	i;

	// look through the loaded sprite name list for SpriteName
	for( i = 0; i < MAX_DECALNAMES && cl.configstrings[CS_DECALS+i+1][0]; i++ )
	{
		if( !com.stricmp( szDecalName, cl.configstrings[CS_DECALS+i+1] ))
			return cl.decal_shaders[i+1];
	}
	return 0; // invalid decal
}

/*
=================
pfnDecalIndex

=================
*/
static int pfnDecalIndex( int id )
{
	id = bound( 0, id, MAX_DECALNAMES - 1 );
	return cl.decal_shaders[id];
}

/*
=================
pfnCullBox

=================
*/
static int pfnCullBox( const vec3_t mins, const vec3_t maxs )
{
	if( !re ) return false;
	return re->CullBox( mins, maxs );
}

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
pfnGetPaletteColor

=================
*/
static void pfnGetPaletteColor( int colorIndex, vec3_t outColor )
{
	if( !outColor ) return;
	colorIndex = bound( 0, colorIndex, 255 );
	VectorCopy( clgame.palette[colorIndex], outColor );
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
	clgame.dllFuncs.pfnDrawTriangles( fTrans );
}

/*
=============
TriLoadShader

=============
*/
shader_t TriLoadShader( const char *szShaderName, int fShaderNoMip )
{
	if( !re ) return 0; // render not initialized
	if( !szShaderName || !*szShaderName )
	{
		MsgDev( D_ERROR, "Tri_LoadShader: invalid shadername (%s)\n", fShaderNoMip ? "nomip" : "generic" );
		return -1;
	}
	if( fShaderNoMip )
		return re->RegisterShader( szShaderName, SHADER_NOMIP );
	return re->RegisterShader( szShaderName, SHADER_GENERIC );
}

/*
=============
TriRenderMode

=============
*/
void TriRenderMode( kRenderMode_t mode )
{
	if( !re ) return;
	re->RenderMode( mode );
}

shader_t TriGetSpriteFrame( int spriteIndex, int spriteFrame )
{
	if( !re ) return 0;
	return re->GetSpriteTexture( spriteIndex, spriteFrame );
}
	
/*
=============
TriBind

bind current shader
=============
*/
void TriBind( shader_t shader, int frame )
{
	if( !re ) return;
	re->Bind( shader, frame );
}

/*
=============
TriBegin

begin triangle sequence
=============
*/
void TriBegin( TRI_DRAW mode )
{
	if( re ) re->Begin( mode );
}

/*
=============
TriEnd

draw triangle sequence
=============
*/
void TriEnd( void )
{
	if( re ) re->End();
}

/*
=============
TriEnable

=============
*/
void TriEnable( int cap )
{
	if( !re ) return;
	re->Enable( cap );
}

/*
=============
TriDisable

=============
*/
void TriDisable( int cap )
{
	if( !re ) return;
	re->Disable( cap );
}

/*
=============
TriVertex3f

=============
*/
void TriVertex3f( float x, float y, float z )
{
	if( !re ) return;
	re->Vertex3f( x, y, z );
}

/*
=============
TriVertex3fv

=============
*/
void TriVertex3fv( const float *v )
{
	if( !re || !v ) return;
	re->Vertex3f( v[0], v[1], v[2] );
}

/*
=============
TriNormal3f

=============
*/
void TriNormal3f( float x, float y, float z )
{
	if( !re ) return;
	re->Normal3f( x, y, z );
}

/*
=============
TriNormal3fv

=============
*/
void TriNormal3fv( const float *v )
{
	if( !re || !v ) return;
	re->Normal3f( v[0], v[1], v[2] );
}

/*
=============
TriColor4f

=============
*/
void TriColor4f( float r, float g, float b, float a )
{
	rgba_t	rgba;

	if( !re ) return;
	rgba[0] = (byte)bound( 0, (r * 255.0f), 255 );
	rgba[1] = (byte)bound( 0, (g * 255.0f), 255 );
	rgba[2] = (byte)bound( 0, (b * 255.0f), 255 );
	rgba[3] = (byte)bound( 0, (a * 255.0f), 255 );
	re->Color4ub( rgba[0], rgba[1], rgba[2], rgba[3] );
}

/*
=============
TriColor4ub

=============
*/
void TriColor4ub( byte r, byte g, byte b, byte a )
{
	if( !re ) return;
	re->Color4ub( r, g, b, a );
}

/*
=============
TriTexCoord2f

=============
*/
void TriTexCoord2f( float u, float v )
{
	if( !re ) return;
	re->TexCoord2f( u, v );
}

/*
=============
TriCullFace

=============
*/
void TriCullFace( TRI_CULL mode )
{
	if( !re ) return;
	re->CullFace( mode );
}

/*
=============
TriScreenToWorld

convert screen coordinates (x,y) into world (x, y, z)
=============
*/
void TriScreenToWorld( float *screen, float *world )
{
	if( !re ) return;
	re->ScreenToWorld( screen, world );
} 

/*
=============
TriWorldToScreen

convert world coordinates (x,y,z) into screen (x, y)
=============
*/
int TriWorldToScreen( float *world, float *screen )
{
	int retval = 0;

	if( !re || !world || !screen )
		return retval;

	retval = re->WorldToScreen( world, screen );

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
	if( re ) re->Fog( flFogColor, flStart, flEnd, bOn );
}

static triapi_t gTriApi =
{
	TRI_API_VERSION,	
	TriLoadShader,
	TriGetSpriteFrame,
	TriRenderMode,
	TriBegin,
	TriEnd,
	TriEnable,
	TriDisable,
	TriVertex3f,
	TriVertex3fv,
	TriNormal3f,
	TriNormal3fv,
	TriColor4f,
	TriColor4ub,
	TriTexCoord2f,
	TriBind,
	TriCullFace,
	TriScreenToWorld,
	TriWorldToScreen,
	TriFog
};

static efxapi_t gEfxApi =
{
	pfnGetPaletteColor,
	pfnDecalIndex,
	pfnDecalIndexFromName,
	CL_DecalShoot,
	CL_PlayerDecal,
	CL_AllocDlight,
	CL_AllocElight,
	CL_LightForPoint,
	pfnBoxVisible,
	pfnCullBox,
	CL_AddEntity,
	pfnEnvShot,
};

static event_api_t gEventApi =
{
	EVENT_API_VERSION,
	pfnPlaySound,
	pfnStopSound,
	pfnFindModelIndex,
	pfnIsLocal,
	pfnLocalPlayerDucking,
	pfnLocalPlayerViewheight,
	pfnLocalPlayerBounds,
	pfnIndexFromTrace,
	pfnGetPhysent,
	pfnSetUpPlayerPrediction,
	pfnPushPMStates,
	pfnPopPMStates,
	pfnSetSolidPlayers,
	pfnSetTraceHull,
	pfnPlayerTrace,
	CL_WeaponAnim,
	pfnPrecacheEvent,
	pfnPlaybackEvent,
	pfnTraceTexture,
	pfnStopAllSounds,
	pfnKillEvents,
};

// engine callbacks
static cl_enginefuncs_t gEngfuncs = 
{
	pfnSPR_Load,
	pfnSPR_Frames,
	pfnSPR_Height,
	pfnSPR_Width,
	pfnSPR_Set,
	pfnSPR_Draw,
	pfnSPR_DrawHoles,
	pfnSPR_DrawTrans,
	pfnSPR_DrawAdditive,
	pfnSPR_EnableScissor,
	pfnSPR_DisableScissor,
	pfnSPR_GetList,
	pfnFillRGBA,
	pfnGetScreenInfo,
	pfnSetCrosshair,
	pfnCVarRegister,
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
	pfnMemAlloc,
	pfnMemFree,
	pfnGetViewAngles,
	pfnSetViewAngles,
	pfnCVarSetString,
	pfnCVarSetValue,
	pfnCmd_Argc,
	pfnCmd_Argv,
	pfnCmd_Args,
	CL_GetLerpFrac,
	pfnDelCommand,
	pfnCon_Printf,
	pfnCon_DPrintf,
	pfnPhysInfo_ValueForKey,
	pfnServerInfo_ValueForKey,
	pfnGetClientMaxspeed,
	pfnGetModelPtr,
	CL_AllocString,
	CL_GetString,
	CL_GetLocalPlayer,
	pfnGetViewModel,
	CL_GetEntityByIndex,
	pfnGetClientTime,
	S_FadeClientVolume,
	pfnPointContents,
	pfnWaterEntity,
	pfnTraceLine,
	CM_GetModelType,
	Mod_GetBounds,
	pfnGetModFrames,
	pfnPrecacheEvent,
	pfnPlaybackEvent,
	CL_WeaponAnim,
	pfnRandomFloat,
	pfnRandomLong,
	pfnHookEvent,
	Con_Visible,
	pfnLoadLibrary,
	pfnGetProcAddress,
	pfnFreeLibrary,
	Host_Error,
	pfnFileExists,
	pfnGetGameDir,
	VGui_GetPanel,
	VGui_ViewportPaintBackground,
	pfnLoadFile,
	pfnParseToken,
	pfnFreeFile,
	&gTriApi,
	&gEfxApi,
	&gEventApi,
	pfnIsSpectateOnly,
	pfnIsInGame
};

void CL_UnloadProgs( void )
{
	if( !clgame.hInstance ) return;

	CL_FreeEdicts();

	// deinitialize game
	clgame.dllFuncs.pfnShutdown();
	StringTable_Delete( clgame.hStringTable );

	FS_FreeLibrary( clgame.hInstance );
	Mem_FreePool( &cls.mempool );
	Mem_FreePool( &clgame.mempool );
	Mem_FreePool( &clgame.private );
	Mem_Set( &clgame, 0, sizeof( clgame ));
}

bool CL_LoadProgs( const char *name )
{
	static CLIENTAPI		GetClientAPI;
	static cl_globalvars_t	gpGlobals;
	static playermove_t		gpMove;

	if( clgame.hInstance ) CL_UnloadProgs();

	// setup globals
	clgame.globals = &gpGlobals;
	cl.refdef.movevars = &clgame.movevars;
	clgame.globals->pViewParms = &cl.refdef;

	// initialize PlayerMove
	clgame.pmove = &gpMove;

	cls.mempool = Mem_AllocPool( "Client Static Pool" );
	clgame.mempool = Mem_AllocPool( "Client Edicts Zone" );
	clgame.private = Mem_AllocPool( "Client Private Zone" );
	clgame.entities = NULL;

	clgame.hInstance = FS_LoadLibrary( name, false );
	if( !clgame.hInstance ) return false;

	GetClientAPI = (CLIENTAPI)FS_GetProcAddress( clgame.hInstance, "CreateAPI" );

	if( !GetClientAPI )
	{
		FS_FreeLibrary( clgame.hInstance );
          	MsgDev( D_NOTE, "CL_LoadProgs: failed to get address of CreateAPI proc\n" );
		clgame.hInstance = NULL;
		return false;
	}

	if( !GetClientAPI( &clgame.dllFuncs, &gEngfuncs, clgame.globals ))
	{
		FS_FreeLibrary( clgame.hInstance );
		MsgDev( D_NOTE, "CL_LoadProgs: can't init client API\n" );
		clgame.hInstance = NULL;
		return false;
	}

	clgame.globals->pStringBase = "";	// setup string base

	// 65535 unique strings should be enough ...
	clgame.hStringTable = StringTable_Create( "Client", 0x10000 );

	clgame.globals->maxEntities = GI->max_edicts; // merge during loading

	// register svc_bad message
	pfnHookUserMsg( "svc_bad", CL_BadMessage );
	CL_LinkUserMessage( "svc_bad", svc_bad, 0 );

	CL_InitTitles( "titles.txt" );

	// initialize game
	clgame.dllFuncs.pfnInit();

	// initialize pm_shared
	CL_InitClientMove();

	return true;
}