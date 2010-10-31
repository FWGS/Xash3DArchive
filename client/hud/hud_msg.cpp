/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
//  hud_msg.cpp
//
#include "extdll.h"
#include "utils.h"
#include "hud.h"
#include "aurora.h"
#include "ref_params.h"

// CHud message handlers
DECLARE_HUDMESSAGE( Logo );
DECLARE_HUDMESSAGE( HUDColor );
DECLARE_HUDMESSAGE( SetFog );
DECLARE_HUDMESSAGE( SetFOV );
DECLARE_HUDMESSAGE( RainData );
DECLARE_HUDMESSAGE( SetBody );
DECLARE_HUDMESSAGE( SetSkin );
DECLARE_HUDMESSAGE( ResetHUD );
DECLARE_HUDMESSAGE( InitHUD );
DECLARE_HUDMESSAGE( ViewMode );
DECLARE_HUDMESSAGE( Particle );
DECLARE_HUDMESSAGE( Concuss );
DECLARE_HUDMESSAGE( GameMode );
DECLARE_HUDMESSAGE( ServerName );
DECLARE_HUDCOMMAND( ChangeLevel );

cvar_t *cl_lw = NULL;
float g_lastFOV = 0.0f;

int CHud :: InitMessages( void )
{
	HOOK_MESSAGE( Logo );
	HOOK_MESSAGE( ResetHUD );
	HOOK_MESSAGE( GameMode );
	HOOK_MESSAGE( ServerName );
	HOOK_MESSAGE( InitHUD );
	HOOK_MESSAGE( ViewMode );
	HOOK_MESSAGE( Concuss );
	HOOK_MESSAGE( HUDColor );
	HOOK_MESSAGE( Particle );
	HOOK_MESSAGE( SetFog );
	HOOK_MESSAGE( SetFOV );
	HOOK_MESSAGE( RainData ); 
	HOOK_MESSAGE( SetBody );
	HOOK_MESSAGE( SetSkin );

	HOOK_COMMAND( "hud_changelevel", ChangeLevel );	// send by engine

	m_iLogo = 0;
	m_iFOV = 0;
	m_iHUDColor = RGB_YELLOWISH; // 255, 160, 0
	
	CVAR_REGISTER( "zoom_sensitivity_ratio", "1.2", 0 );
	default_fov = CVAR_REGISTER( "default_fov", "90", 0 );
	CVAR_REGISTER( "hud_draw", "1", 0 );
	CVAR_REGISTER( "hud_takesshots", "0", 0 );
	cl_lw = gEngfuncs.pfnGetCvarPointer( "cl_lw" );

	// clear any old HUD list
	if( m_pHudList )
	{
		HUDLIST *pList;
		while( m_pHudList )
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			free( pList );
		}
		m_pHudList = NULL;
	}

	m_flTime = 1.0;

	return 1;
}

void CHud :: UserCmd_ChangeLevel( void )
{
}

int CHud :: MsgFunc_Logo( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	// update Train data
	m_iLogo = READ_BYTE();

	END_READ();

	return 1;
}

int CHud :: MsgFunc_ResetHUD(const char *pszName, int iSize, void *pbuf )
{
	// clear all hud data
	HUDLIST *pList = m_pHudList;

	while( pList )
	{
		if( pList->p ) pList->p->Reset();
		pList = pList->pNext;
	}

	// reset sensitivity
	m_flMouseSensitivity = 0;

	// reset concussion effect
	m_iConcussionEffect = 0;

	m_iIntermission = 0;

	// reset windspeed
	m_vecWindVelocity = Vector( 0, 0, 0 );

	// reset fog
	m_flStartDist = 0;
	m_flEndDist = 0;
		
	return 1;
}

int CHud :: MsgFunc_ViewMode( const char *pszName, int iSize, void *pbuf )
{
	// CAM_ToFirstPerson();
	return 1;
}

int CHud :: MsgFunc_InitHUD( const char *pszName, int iSize, void *pbuf )
{
	m_flStartDist = 0;
	m_flEndDist = 0;
	m_iIntermission = 0;

	// prepare all hud data
	HUDLIST *pList = m_pHudList;

	while( pList )
	{
		if( pList->p )
			pList->p->InitHUDData();
		pList = pList->pNext;
	}
	return 1;
}

int CHud::MsgFunc_HUDColor(const char *pszName,  int iSize, void *pbuf)
{
	BEGIN_READ( pszName, iSize, pbuf );

	m_iHUDColor = READ_LONG();

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_SetFog( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	m_vecFogColor.x = (float)(READ_BYTE() / 255.0f);
	m_vecFogColor.y = (float)(READ_BYTE() / 255.0f);
	m_vecFogColor.z = (float)(READ_BYTE() / 255.0f);

	m_flStartDist = READ_SHORT();
          m_flEndDist = READ_SHORT();

	END_READ();
	
	return 1;
}

int CHud::MsgFunc_SetFOV(const char *pszName,  int iSize, void *pbuf)
{
	BEGIN_READ( pszName, iSize, pbuf );

	int newfov = READ_BYTE();
	int def_fov = CVAR_GET_FLOAT( "default_fov" );

	//Weapon prediction already takes care of changing the fog. ( g_lastFOV ).
	if ( cl_lw && cl_lw->value )
	{
		END_READ();
		return 1;
	}

	g_lastFOV = newfov;

	if ( newfov == 0 )
	{
		m_iFOV = def_fov;
	}
	else
	{
		m_iFOV = newfov;
	}

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	if ( m_iFOV == def_fov )
	{  
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{  
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = m_sensitivity->value * ((float)newfov / (float)def_fov );
		m_flMouseSensitivity *= CVAR_GET_FLOAT( "zoom_sensitivity_ratio" );
	}

	END_READ();

	return 1;
}

int CHud :: MsgFunc_GameMode( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	m_Teamplay = READ_BYTE();

	END_READ();
	
	return 1;
}


int CHud :: MsgFunc_Damage( const char *pszName, int iSize, void *pbuf )
{
	int	armor, blood;
	int	i;
	float	count;

	BEGIN_READ( pszName, iSize, pbuf );
	armor = READ_BYTE();
	blood = READ_BYTE();

	float	from[3];
	for( i = 0; i < 3; i++ )
		from[i] = READ_COORD();

	count = (blood * 0.5) + (armor * 0.5);
	if( count < 10 ) count = 10;

	END_READ();

	// TODO: kick viewangles,  show damage visually

	return 1;
}

int CHud :: MsgFunc_Concuss( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	m_iConcussionEffect = READ_BYTE();
	if( m_iConcussionEffect )
		m_StatusIcons.EnableIcon( "dmg_concuss", 255, 160, 0 );
	else m_StatusIcons.DisableIcon( "dmg_concuss" );

	END_READ();

	return 1;
}

int CHud :: MsgFunc_RainData( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	Rain.dripsPerSecond = READ_SHORT();
	Rain.distFromPlayer = READ_COORD();
	Rain.windX = READ_COORD();
	Rain.windY = READ_COORD();
	Rain.randX = READ_COORD();
	Rain.randY = READ_COORD();
	Rain.weatherMode = READ_SHORT();
	Rain.globalHeight= READ_COORD();	// FIXME: calc on client side ?

	END_READ();	

	return 1;
}

int CHud :: MsgFunc_SetBody( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	cl_entity_t *viewmodel = GetViewModel();
	viewmodel->curstate.body = READ_BYTE();

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_SetSkin( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	cl_entity_t *viewmodel = GetViewModel();
	viewmodel->curstate.skin = READ_BYTE();

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_Particle( const char *pszName,  int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	int idx = READ_SHORT();
	char *sz = READ_STRING();

	CreateAurora( idx, sz );

	END_READ();
	
	return 1;
}

int CHud::MsgFunc_ServerName( const char *pszName, int iSize, void *pbuf )
{
	char	m_szServerName[32];

	BEGIN_READ( pszName, iSize, pbuf );
	strncpy( m_szServerName, READ_STRING(), 32 );
	END_READ();
	
 	return 1;
}

/*
=====================
HUD_GetFOV

Returns last FOV
=====================
*/
float HUD_GetFOV( void )
{
	return g_lastFOV;
}