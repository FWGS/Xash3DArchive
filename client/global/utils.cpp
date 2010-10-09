//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     utils.cpp - client game utilities code
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "pm_defs.h"
#include "com_model.h"
#include "hud.h"

DLL_GLOBAL const Vector g_vecZero = Vector( 0.0f, 0.0f, 0.0f );

#ifdef _DEBUG
void DBG_AssertFunction( BOOL fExpr, const char* szExpr, const char* szFile, int szLine, const char* szMessage )
{
	if( fExpr ) return;

	char szOut[512];
	if( szMessage != NULL )
		sprintf( szOut, "ASSERT FAILED:\n %s \n(%s@%d)\n%s", szExpr, szFile, szLine, szMessage );
	else sprintf( szOut, "ASSERT FAILED:\n %s \n(%s@%d)", szExpr, szFile, szLine );
	Con_Printf( szOut );
}
#endif	// DEBUG

// NOTE: modify these functions with caution
typedef struct
{
	char	*name;
	byte	*buf;
	int	size;
	int	read;
	BOOL	badRead;
	char	string[2048];	// using for store strings
} user_message_t;

static user_message_t gMsg;

void BEGIN_READ( const char *pszName, int iSize, void *pBuf )
{
	memset( &gMsg, 0, sizeof( gMsg ));

	gMsg.size = iSize;
	gMsg.buf = (byte *)pBuf;
}

void END_READ( void )
{
	if( gMsg.badRead )
	{
		Con_Printf( "%s was received with errors\n", gMsg.name );
	}
}

int READ_CHAR( void )
{
	int     c;
	
	if( gMsg.read + 1 > gMsg.size )
	{
		gMsg.badRead = true;
		return -1;
	}
		
	c = (signed char)gMsg.buf[gMsg.read];
	gMsg.read++;
	
	return c;
}

int READ_BYTE( void )
{
	int     c;
	
	if( gMsg.read+1 > gMsg.size )
	{
		gMsg.badRead = true;
		return -1;
	}
		
	c = (unsigned char)gMsg.buf[gMsg.read];
	gMsg.read++;
	
	return c;
}

int READ_SHORT( void )
{
	int     c;
	
	if( gMsg.read + 2 > gMsg.size )
	{
		gMsg.badRead = true;
		return -1;
	}
		
	c = (short)( gMsg.buf[gMsg.read] + ( gMsg.buf[gMsg.read+1] << 8 ));
	gMsg.read += 2;
	
	return c;
}

int READ_WORD( void ) { return READ_SHORT(); }

int READ_LONG( void )
{
	int     c;
	
	if( gMsg.read + 4 > gMsg.size )
	{
		gMsg.badRead = true;
		return -1;
	}
 	c = gMsg.buf[gMsg.read]+(gMsg.buf[gMsg.read+1]<<8)+(gMsg.buf[gMsg.read+2]<<16)+(gMsg.buf[gMsg.read+3]<<24);
	gMsg.read += 4;
	
	return c;
}

float READ_FLOAT( void )
{
	union { float f; int l; } dat;
	
	dat.l = READ_LONG();
	return dat.f;   
}

char* READ_STRING( void )
{
	int	l, c;

	gMsg.string[0] = 0;

	l = 0;
	do
	{
		if( gMsg.read+1 > gMsg.size ) break; // no more characters

		c = READ_CHAR();
		if( c == -1 || c == '\0' )
			break;

		gMsg.string[l] = c;
		l++;
	} while( l < sizeof( gMsg.string ) - 1 );
	
	gMsg.string[l] = 0; // terminator

	return gMsg.string;
}

float READ_COORD( void )
{
	return (float)(READ_SHORT() * (1.0/8));
}

float READ_ANGLE( void )
{
	return (float)(READ_CHAR() * (360.0/256));
}

float READ_HIRESANGLE( void )
{
	return (float)(READ_SHORT() * (360.0/65536));
}

/*
==============================================================================

		DRAW HELPERS

==============================================================================
*/
void DrawImageBar( float percent, const char *szSpriteName )
{
	int m_loading = gHUD.GetSpriteIndex( szSpriteName );
	wrect_t rcSize = gHUD.GetSpriteRect( m_loading );

	int w = rcSize.right - rcSize.left;
	int h = rcSize.bottom - rcSize.top;
	DrawImageBar( percent, szSpriteName, (ScreenWidth - w)/2, (ScreenHeight - h)/2 );
}

void DrawImageBar( float percent, const char *szSpriteName, int x, int y )
{
	int	m_loading = gHUD.GetSpriteIndex( szSpriteName );
	HSPRITE	hLoading = gHUD.GetSprite( m_loading );
	wrect_t	rcBar, rcBack;
	float	step;

	rcBar = rcBack = gHUD.GetSpriteRect( m_loading );
	step = (float)(rcBack.right - rcBack.left) / 100;
	rcBar.right = rcBar.left + (int)ceil(percent * step);

	SPR_Set( hLoading, 128, 128, 128 );
	SPR_DrawAdditive( 0, x, y, &rcBack );	// background

	SPR_Set( hLoading, 255, 160, 0 );
	SPR_DrawAdditive( 0, x, y, &rcBar );	// progress bar
}

void AngleMatrix( const vec3_t angles, float (*matrix)[4] )
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	
	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin( angle );
	cy = cos( angle );
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin( angle );
	cp = cos( angle );
	angle = angles[ROLL] * (M_PI*2 / 360);
	sr = sin( angle );
	cr = cos( angle );

	// matrix = (YAW * PITCH) * ROLL
	matrix[0][0] = cp*cy;
	matrix[1][0] = cp*sy;
	matrix[2][0] = -sp;
	matrix[0][1] = sr*sp*cy+cr*-sy;
	matrix[1][1] = sr*sp*sy+cr*cy;
	matrix[2][1] = sr*cp;
	matrix[0][2] = (cr*sp*cy+-sr*-sy);
	matrix[1][2] = (cr*sp*sy+-sr*cy);
	matrix[2][2] = cr*cp;
	matrix[0][3] = 0.0;
	matrix[1][3] = 0.0;
	matrix[2][3] = 0.0;
}

// g-cont. copied here from pm_math.cpp
void VectorAngles( const Vector &forward, Vector &angles )
{
	float	tmp, yaw, pitch;
	
	if (forward[1] == 0 && forward[0] == 0)
	{
		yaw = 0;
		if (forward[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		yaw = (atan2(forward[1], forward[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		tmp = sqrt (forward[0]*forward[0] + forward[1]*forward[1]);
		pitch = (atan2(forward[2], tmp) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}
	
	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = 0;
}

//
// hl2 fade - this supports multiple fading
// FIXME: make Class CHudFade instead of C-style code?
//
void SetScreenFade( Vector fadeColor, float alpha, float duration, float holdTime, int fadeFlags )
{
	CL_ScreenFade *sf = NULL;

	for( int i = 0; i < HUD_MAX_FADES; i++ )
	{
		// search for free spot
		if( !gHUD.m_FadeList[i].bActive )
		{
			sf = &gHUD.m_FadeList[i];
			break;
		}
	}

	if( !sf ) return; // no free fades found 

	sf->bActive = true;
	sf->fadeEnd = duration;
	sf->fadeReset = holdTime;	
	sf->fadeColor = fadeColor;
	sf->fadeAlpha = alpha;
	sf->fadeFlags = fadeFlags;
	sf->fadeSpeed = 0;

	// calc fade speed
	if( duration > 0 )
	{
		if( fadeFlags & FFADE_OUT )
		{
			if( sf->fadeEnd )
			{
				sf->fadeSpeed = -(float)sf->fadeAlpha / sf->fadeEnd;
			}

			sf->fadeEnd += gHUD.m_flTime;
			sf->fadeReset += sf->fadeEnd;
		}
		else
		{
			if( sf->fadeEnd )
			{
				sf->fadeSpeed = (float)sf->fadeAlpha / sf->fadeEnd;
			}

			sf->fadeReset += gHUD.m_flTime;
			sf->fadeEnd += sf->fadeReset;
		}
	}
}

void DrawScreenFade( void )
{
	int	i, numFades = 0;
	
	// Cycle through all fades and remove any that have finished (work backwards)
	for( i = HUD_MAX_FADES - 1; i >= 0; i-- )
	{
		CL_ScreenFade *pFade = &gHUD.m_FadeList[i];

		if( pFade->bActive == 0 ) continue;	// free slot

		// Keep pushing reset time out indefinitely
		if( pFade->fadeFlags & FFADE_STAYOUT )
		{
			pFade->fadeReset = gHUD.m_flTime + 0.1f;
		}

		// All done?
		if(( gHUD.m_flTime > pFade->fadeReset ) && ( gHUD.m_flTime > pFade->fadeEnd ))
		{
			// remove this Fade from the list
			memset( pFade, 0, sizeof( CL_ScreenFade ));
		}
	}

	gHUD.m_bModulate = false;
	gHUD.m_vecFadeColor = Vector( 0, 0, 0 );
	gHUD.m_flFadeAlpha = 0.0f;

	// Cycle through all fades in the list and calculate the overall color/alpha
	for ( i = 0; i < HUD_MAX_FADES; i++ )
	{
		CL_ScreenFade *pFade = &gHUD.m_FadeList[i];

		if( pFade->bActive == 0 ) continue;	// free slot

		// Color
		gHUD.m_vecFadeColor += pFade->fadeColor;

		// Fading...
		int iFadeAlpha;
		if( pFade->fadeFlags == FFADE_STAYOUT )
		{
			iFadeAlpha = pFade->fadeAlpha;
		}
		else
		{
			iFadeAlpha = pFade->fadeSpeed * ( pFade->fadeEnd - gHUD.m_flTime );
			if( pFade->fadeFlags & FFADE_OUT )
			{
				iFadeAlpha += pFade->fadeAlpha;
			}
			iFadeAlpha = min( iFadeAlpha, pFade->fadeAlpha );
			iFadeAlpha = max( 0, iFadeAlpha );
		}

		// Use highest alpha
		if( iFadeAlpha > gHUD.m_flFadeAlpha )
		{
			gHUD.m_flFadeAlpha = iFadeAlpha;
		}

		// Modulate?
		if( pFade->fadeFlags & FFADE_MODULATE )
		{
			gHUD.m_bModulate = true;
		}
		numFades++;
	}

	// divide colors
	if( numFades ) gHUD.m_vecFadeColor /= numFades;
	else return;

	const float *RGB = gHUD.m_vecFadeColor;
	FillRGBA( 0, 0, ScreenWidth, ScreenHeight, RGB[0], RGB[1], RGB[2], gHUD.m_flFadeAlpha );
}

void ClearPermanentFades( void )
{
	for( int i = HUD_MAX_FADES - 1; i >= 0; i-- )
	{
		CL_ScreenFade *pFade = &gHUD.m_FadeList[i];
		if( pFade->fadeFlags == 0 ) continue;	// free slot

		if( pFade->fadeFlags & FFADE_STAYOUT )
		{
			// remove this Fade from the list
			memset( pFade, 0, sizeof( CL_ScreenFade ));
		}
	}
}

void ClearAllFades( void )
{
	memset( gHUD.m_FadeList, 0, sizeof( gHUD.m_FadeList ));
}

/*
====================
RotatePointAroundVector
====================
*/
void RotatePointAroundVector( Vector &dst, const Vector &dir, const Vector &point, float degrees )
{
	float	t0, t1;
	float	angle, c, s, d;
	Vector	vr, vu, vf;

	angle = DEG2RAD( degrees );
	s = sin( angle );
	c = cos( angle );

	vf.Init( dir.x, dir.y, dir.z );
	vr.Init( vf.z, -vf.x, vf.y );

	// find a three vectors
	d = DotProduct( vf, vr );
	vr = (vr + ( vf * -d )).Normalize();
	vu = CrossProduct( vr, vf );

	t0 = vr[0] *  c + vu[0] * -s;
	t1 = vr[0] *  s + vu[0] *  c;
	dst[0] = (t0 * vr[0] + t1 * vu[0] + vf[0] * vf[0]) * point[0]
	       + (t0 * vr[1] + t1 * vu[1] + vf[0] * vf[1]) * point[1]
	       + (t0 * vr[2] + t1 * vu[2] + vf[0] * vf[2]) * point[2];

	t0 = vr[1] *  c + vu[1] * -s;
	t1 = vr[1] *  s + vu[1] *  c;
	dst[1] = (t0 * vr[0] + t1 * vu[0] + vf[1] * vf[0]) * point[0]
	       + (t0 * vr[1] + t1 * vu[1] + vf[1] * vf[1]) * point[1]
	       + (t0 * vr[2] + t1 * vu[2] + vf[1] * vf[2]) * point[2];

	t0 = vr[2] *  c + vu[2] * -s;
	t1 = vr[2] *  s + vu[2] *  c;
	dst[2] = (t0 * vr[0] + t1 * vu[0] + vf[2] * vf[0]) * point[0]
	       + (t0 * vr[1] + t1 * vu[1] + vf[2] * vf[1]) * point[1]
	       + (t0 * vr[2] + t1 * vu[2] + vf[2] * vf[2]) * point[2];
}

/*
====================
UTIL_Probe

client explosion utility
====================
*/
float UTIL_Probe( Vector &origin, Vector *vecDirection, float strength )
{
	// press out
	Vector endpos = origin + (( *vecDirection ) * strength );

	// Trace into the world
	pmtrace_t *trace = gEngfuncs.PM_TraceLine( origin, endpos, PM_TRACELINE_PHYSENTSONLY, 2, -1 );

	// push back a proportional amount to the probe
	(*vecDirection) = -(*vecDirection) * (1.0f - trace->fraction);

	ASSERT(( 1.0f - trace->fraction ) >= 0.0f );

	// return the impacted proportion of the probe
	return (1.0f - trace->fraction);
}

void UTIL_GetForceDirection( Vector &origin, float magnitude, Vector *resultDirection, float *resultForce )
{
	Vector	d[6];

	// all cardinal directions
	d[0] = Vector(  1,  0,  0 );
	d[1] = Vector( -1,  0,  0 );
	d[2] = Vector(  0,  1,  0 );
	d[3] = Vector(  0, -1,  0 );
	d[4] = Vector(  0,  0,  1 );
	d[5] = Vector(  0,  0, -1 );

	//Init the results
	(*resultDirection).Init();
	(*resultForce) = 1.0f;
	
	// Get the aggregate force vector
	for ( int i = 0; i < 6; i++ )
	{
		(*resultForce) += UTIL_Probe( origin, &d[i], magnitude );
		(*resultDirection) += d[i];
	}

	// If we've hit nothing, then point up
	if (( *resultDirection ) == g_vecZero )
	{
		(*resultDirection) = Vector( 0, 0, 1 );
		(*resultForce) = 2.0f; // default value
	}

	// Just return the direction
	(*resultDirection) = (*resultDirection).Normalize();
}

modtype_t	Mod_GetModelType( int modelIndex )
{
	model_t *mod = GetModelPtr( modelIndex );

	if( mod ) return mod->type;
	return mod_bad;
}

void Mod_GetBounds( int modelIndex, Vector &mins, Vector &maxs )
{
	model_t	*mod = GetModelPtr( modelIndex );

	if( mod )
	{
		mins = mod->mins;
		maxs = mod->maxs;
	}
	else
	{
		Con_DPrintf( "Mod_GetBounds: NULL model %i\n", modelIndex );
		mins = g_vecZero;
		maxs = g_vecZero;
	}
}

void *Mod_Extradata( int modelIndex )
{
	model_t	*mod = GetModelPtr( modelIndex );

	if( mod && mod->type == mod_studio )
		return mod->extradata;
	return NULL;
}

int Mod_GetFrames( int modelIndex )
{
	model_t	*mod = GetModelPtr( modelIndex );

	if( !mod ) return 1;

	int numFrames;

	if( mod->type == mod_sprite )
		numFrames =  mod->numframes;
	else if( mod->type == mod_studio )
		numFrames = StudioBodyVariations( modelIndex );		
	if( numFrames < 1 ) numFrames = 1;

	return numFrames;
}

//============
// UTIL_FileExtension
// returns file extension
//============
const char *UTIL_FileExtension( const char *in )
{
	const char *separator, *backslash, *colon, *dot;

	separator = strrchr( in, '/' );
	backslash = strrchr( in, '\\' );
	if( !separator || separator < backslash )
		separator = backslash;
	colon = strrchr( in, ':' );
	if( !separator || separator < colon )
		separator = colon;
	dot = strrchr( in, '.' );
	if( dot == NULL || (separator && ( dot < separator )))
		return "";
	return dot + 1;
}

HSPRITE LoadSprite( const char *pszName )
{
	int i;
	char sz[256]; 

	if ( ScreenWidth < 640 )
		i = 320;
	else
		i = 640;

	sprintf( sz, pszName, i );

	return SPR_Load( sz );
}

/*
====================
VGui_ConsolePrint

VGUI not implemented, wait for version 0.75
====================
*/
void VGui_ConsolePrint( const char *text )
{
}