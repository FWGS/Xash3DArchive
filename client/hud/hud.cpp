//=======================================================================
//			Copyright (C) XashXT Group 2007
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "hud.h"
#include "triangle_api.h"

#define MAX_LOGO_FRAMES 56

int grgLogoFrame[MAX_LOGO_FRAMES] = 
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13, 13, 13, 13, 12, 11, 10, 9, 8, 14, 15,
	16, 17, 18, 19, 20, 20, 20, 20, 20, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 
	29, 29, 29, 29, 29, 28, 27, 26, 25, 24, 30, 31 
};

void CHud :: Init( void )
{
	InitMessages();
	m_Ammo.Init();
	m_Health.Init();
	m_SayText.Init();
	m_Geiger.Init();
	m_Train.Init();
	m_Battery.Init();
	m_Flash.Init();
	m_Message.Init();
	m_Scoreboard.Init();
	m_StatusBar.Init();
	m_DeathNotice.Init();
	m_AmmoSecondary.Init();
	m_TextMessage.Init();
	m_StatusIcons.Init();
	m_Menu.Init();
	m_MOTD.Init();
		
	MsgFunc_ResetHUD( 0, 0, NULL );
}

CHud :: ~CHud( void )
{
	delete [] m_rghSprites;
	delete [] m_rgrcRects;
	delete [] m_rgszSpriteNames;

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

	// release hud.txt file
	gEngfuncs.COM_FreeFile( m_pSpriteList );
	m_pSpriteList = NULL;
}

int CHud :: GetSpriteIndex( const char *SpriteName )
{
	// look through the loaded sprite name list for SpriteName
	for( int i = 0; i < m_iSpriteCount; i++ )
	{
		if(!strncmp( SpriteName, m_rgszSpriteNames + (i * MAX_SPRITE_NAME_LENGTH), MAX_SPRITE_NAME_LENGTH ))
			return i;
	}
	return -1; // invalid sprite
}

void CHud :: VidInit( void )
{
	// ----------
	// Load Sprites
	// ---------

	m_hsprLogo = 0;
	m_hsprCursor = 0;
	m_hHudError = 0;
	spot = NULL; // clear intermission spot

	ClearAllFades ();

	// setup screen info
	m_scrinfo.iSize = sizeof(m_scrinfo);
	GetScreenInfo( &m_scrinfo );

	if( ScreenWidth < 640 )
		m_iRes = 320;
	else m_iRes = 640;

	// Only load this once
	if ( !m_pSpriteList )
	{
		// we need to load the hud.txt, and all sprites within
		m_pSpriteList = SPR_GetList( "sprites/hud.txt", &m_iSpriteCountAllRes );

		if( m_pSpriteList )
		{
			// count the number of sprites of the appropriate res
			m_iSpriteCount = 0;
			client_sprite_t *p = m_pSpriteList;
			for ( int j = 0; j < m_iSpriteCountAllRes; j++ )
			{
				if ( p->iRes == m_iRes )
					m_iSpriteCount++;
				p++;
			}

			// allocated memory for sprite handle arrays
 			m_rghSprites = new HSPRITE[m_iSpriteCount];
			m_rgrcRects = new wrect_t[m_iSpriteCount];
			m_rgszSpriteNames = new char[m_iSpriteCount * MAX_SPRITE_NAME_LENGTH];

			p = m_pSpriteList;
			int index = 0;
			for ( j = 0; j < m_iSpriteCountAllRes; j++ )
			{
				if ( p->iRes == m_iRes )
				{
					char sz[256];
					sprintf(sz, "sprites/%s.spr", p->szSprite);
					m_rghSprites[index] = SPR_Load(sz);
					m_rgrcRects[index] = p->rc;
					strncpy( &m_rgszSpriteNames[index * MAX_SPRITE_NAME_LENGTH], p->szName, MAX_SPRITE_NAME_LENGTH );

					index++;
				}
				p++;
			}
		}
		else
		{
			gEngfuncs.Con_Printf( "Warning: hud.txt couldn't load\n" );
			CVAR_SET_FLOAT( "hud_draw", 0 );
			return;
		}
	}
	else
	{
		// we have already have loaded the sprite reference from hud.txt, but
		// we need to make sure all the sprites have been loaded (we've gone through a transition, or loaded a save game)
		client_sprite_t *p = m_pSpriteList;
		int index = 0;
		for ( int j = 0; j < m_iSpriteCountAllRes; j++ )
		{
			if ( p->iRes == m_iRes )
			{
				char sz[256];
				sprintf( sz, "sprites/%s.spr", p->szSprite );
				m_rghSprites[index] = SPR_Load(sz);
				index++;
			}

			p++;
		}
	}

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex( "number_0" );
	m_iFontHeight = GetSpriteRect( m_HUD_number_0 ).bottom - GetSpriteRect( m_HUD_number_0 ).top;

	// loading error sprite
	m_HUD_error = GetSpriteIndex( "error" );
	m_hHudError = GetSprite( m_HUD_error );
	
	m_Ammo.VidInit();
	m_Health.VidInit();
	m_Geiger.VidInit();
	m_Train.VidInit();
	m_Battery.VidInit();
	m_Flash.VidInit();
	m_MOTD.VidInit();
	m_Message.VidInit();
	m_Scoreboard.VidInit();
	m_StatusBar.VidInit();
	m_DeathNotice.VidInit();
	m_SayText.VidInit();
	m_Menu.VidInit();
	m_AmmoSecondary.VidInit();
	m_TextMessage.VidInit();
	m_StatusIcons.VidInit();
}

void CHud :: Think( void )
{
	HUDLIST *pList = m_pHudList;

	while( pList )
	{
		if (pList->p->m_iFlags & HUD_ACTIVE)
			pList->p->Think();
		pList = pList->pNext;
	}

	int newfov = HUD_GetFOV();

	if ( newfov == 0 )
	{
		m_iFOV = default_fov->value;
	}
	else
	{
		m_iFOV = newfov;
	}

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	if ( m_iFOV == default_fov->value )
	{  
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{  
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = m_sensitivity->value * ((float)newfov / (float)default_fov->value );
		m_flMouseSensitivity *= CVAR_GET_FLOAT( "zoom_sensitivity_ratio" );
	}

	// think about default fov
	if ( m_iFOV == 0 )
	{  
		// only let players adjust up in fov,  and only if they are not overriden by something else
		m_iFOV = max( default_fov->value, 90 );  
	}
}

int CHud :: UpdateClientData( client_data_t *cdata, float time )
{
	cl_entity_t *pClient = GetLocalPlayer ();

	memcpy( m_vecOrigin, cdata->origin, sizeof( vec3_t ));
	memcpy( m_vecAngles, cdata->viewangles, sizeof( vec3_t ));

	if( pClient )
	{
		// TEMPORARY HACK
		m_iNoClip = (pClient->curstate.movetype == MOVETYPE_NOCLIP) ? 1 : 0;
	}

	m_iKeyBits = CL_ButtonBits( 0 );
	m_iWeaponBits = cdata->iWeaponBits;

	float in_fov = cdata->fov;

	Think();

	cdata->fov = m_iFOV;

	v_idlescale = m_iConcussionEffect;

	CL_ResetButtonBits( m_iKeyBits );

	return 1;
}

int CHud :: Redraw( float flTime )
{
	m_fOldTime = m_flTime;	// save time of previous redraw
	m_flTime = flTime;
	m_flTimeDelta = (double)m_flTime - m_fOldTime;
	static float m_flShotTime;

	// clock was reset, reset delta
	if( m_flTimeDelta < 0 ) m_flTimeDelta = 0;

	if( v_dark )
	{
		SetScreenFade( Vector( 0, 0, 0 ), 255, 4, 4, FFADE_IN );
		v_dark = FALSE;
	}

	// draw screen fade before hud
	DrawScreenFade();

	// take a screenshot if the client's got the cvar set
	if( m_flShotTime && m_flShotTime < flTime )
	{
		CLIENT_COMMAND( "screenshot\n" );
		m_flShotTime = 0;
	}
	
	if( CVAR_GET_FLOAT( "hud_draw" ))
	{
		HUDLIST *pList = m_pHudList;

		while( pList )
		{
			if( !m_iIntermission )
			{
				if(( pList->p->m_iFlags & HUD_ACTIVE ) && !(m_iHideHUDDisplay & HIDEHUD_ALL ))
					pList->p->Draw( flTime );
			}
			else
			{
				// it's an intermission, so only draw hud elements
				// that are set to draw during intermissions
				if( pList->p->m_iFlags & HUD_INTERMISSION )
					pList->p->Draw( flTime );
			}
			pList = pList->pNext;
		}
	}

	// are we in demo mode? do we need to draw the logo in the top corner?
	if (m_iLogo)
	{
		int x, y, i;

		if (m_hsprLogo == 0)
			m_hsprLogo = LoadSprite("sprites/%d_logo.spr");

		SPR_Set(m_hsprLogo, 250, 250, 250 );
		
		x = SPR_Width(m_hsprLogo, 0);
		x = ScreenWidth - x;
		y = SPR_Height(m_hsprLogo, 0)/2;

		// Draw the logo at 20 fps
		int iFrame = (int)(flTime * 20) % MAX_LOGO_FRAMES;
		i = grgLogoFrame[iFrame] - 1;

		SPR_DrawAdditive(i, x, y, NULL);
	}

	return 1;
}

int CHud :: DrawHudString( int xpos, int ypos, int iMaxX, char *szIt, int r, int g, int b )
{
	// draw the string until we hit the null character or a newline character
	for( ; *szIt != 0 && *szIt != '\n'; szIt++ )
	{
		int next = xpos + gHUD.m_scrinfo.charWidths[*szIt]; // variable-width fonts look cool
		if ( next > iMaxX )
			return xpos;

		TextMessageDrawChar( xpos, ypos, *szIt, r, g, b );
		xpos = next;		
	}

	return xpos;
}

int CHud :: DrawHudNumberString( int xpos, int ypos, int iMinX, int iNumber, int r, int g, int b )
{
	char szString[32];
	sprintf( szString, "%d", iNumber );
	return DrawHudStringReverse( xpos, ypos, iMinX, szString, r, g, b );

}

int CHud :: DrawHudStringReverse( int xpos, int ypos, int iMinX, char *szString, int r, int g, int b )
{
	// find the end of the string
	for ( char *szIt = szString; *szIt != 0; szIt++ )
	{ // we should count the length?		
	}

	// iterate throug the string in reverse
	for( szIt--;  szIt != (szString-1);  szIt-- )	
	{
		int next = xpos - gHUD.m_scrinfo.charWidths[ *szIt ]; // variable-width fonts look cool
		if( next < iMinX )
			return xpos;
		xpos = next;

		TextMessageDrawChar( xpos, ypos, *szIt, r, g, b );
	}

	return xpos;
}

int CHud :: DrawHudNumber( int x, int y, int iFlags, int iNumber, int r, int g, int b )
{
	int iWidth = GetSpriteRect( m_HUD_number_0 ).right - GetSpriteRect( m_HUD_number_0 ).left;
	int k;
	
	if( iNumber > 0 )
	{
		// SPR_Draw 100's
		if( iNumber >= 100 )
		{
			k = iNumber / 100;
			SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect(m_HUD_number_0 + k));
			x += iWidth;
		}
		else if( iFlags & DHN_3DIGITS )
		{
			x += iWidth;
		}

		// SPR_Draw 10's
		if( iNumber >= 10 )
		{
			k = (iNumber % 100)/10;
			SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect(m_HUD_number_0 + k));
			x += iWidth;
		}
		else if( iFlags & (DHN_3DIGITS|DHN_2DIGITS))
		{
			x += iWidth;
		}

		// SPR_Draw ones
		k = iNumber % 10;
		SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
		SPR_DrawAdditive(0,  x, y, &GetSpriteRect(m_HUD_number_0 + k));
		x += iWidth;
	} 
	else if( iFlags & DHN_DRAWZERO ) 
	{
		SPR_Set(GetSprite(m_HUD_number_0), r, g, b );

		// SPR_Draw 100's
		if( iFlags & DHN_3DIGITS )
		{
			x += iWidth;
		}

		if( iFlags & (DHN_3DIGITS|DHN_2DIGITS)) x += iWidth;
		SPR_DrawAdditive( 0,  x, y, &GetSpriteRect(m_HUD_number_0));
		x += iWidth;
	}
	return x;
}

int CHud::GetNumWidth( int iNumber, int iFlags )
{
	if( iFlags & (DHN_3DIGITS)) return 3;
	if( iFlags & (DHN_2DIGITS)) return 2;

	if( iNumber <= 0 )
	{
		if( iFlags & (DHN_DRAWZERO))
			return 1;
		else return 0;
	}

	if( iNumber < 10  ) return 1;
	if( iNumber < 100 ) return 2;

	return 3;

}

void CHud::AddHudElem( CHudBase *phudelem )
{
	HUDLIST	*pdl, *ptemp;

	if( !phudelem ) return;

	pdl = (HUDLIST *)malloc( sizeof( HUDLIST ));
	if (!pdl)
		return;

	memset(pdl, 0, sizeof(HUDLIST));
	pdl->p = phudelem;

	if( !m_pHudList )
	{
		m_pHudList = pdl;
		return;
	}

	ptemp = m_pHudList;

	while( ptemp->pNext )
		ptemp = ptemp->pNext;
	ptemp->pNext = pdl;
}