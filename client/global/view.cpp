//=======================================================================
//			Copyright XashXT Group 2008 �
//		     view.cpp - view/refresh setup functions
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "ref_params.h"
#include "triangle_api.h"
#include "pm_movevars.h"
#include "r_beams.h"
#include "studio.h"
#include "pm_defs.h"
#include "hud.h"

extern void PM_ParticleLine( float *start, float *end, int pcolor, float life, float vert);
extern int PM_GetVisEntInfo( int ent );
extern int	PM_GetPhysEntInfo( int ent );
extern void InterpolateAngles(  float * start, float * end, float * output, float frac );
extern void NormalizeAngles( float * angles );
extern float Distance(const float * v1, const float * v2);
extern float AngleBetweenVectors(  const float * v1,  const float * v2 );

extern float	vJumpOrigin[3];
extern float	vJumpAngles[3];

#define ORIGIN_BACKUP	64
#define ORIGIN_MASK		( ORIGIN_BACKUP - 1 )

// global view containers
Vector	v_origin, v_angles, v_cl_angles;	// base client vectors
float	v_idlescale, v_lastDistance;		// misc variables
Vector	ev_punchangle;			// client punchangles
cl_entity_t *spot;

typedef struct 
{
	Vector	Origins[ORIGIN_BACKUP];
	float	OriginTime[ORIGIN_BACKUP];
	Vector	Angles[ORIGIN_BACKUP];
	float	AngleTime[ORIGIN_BACKUP];
	int	CurrentOrigin;
	int	CurrentAngle;
} viewinterp_t;

typedef struct pitchdrift_s
{
	int  	nodrift;
	float	pitchvel;
	float	driftmove;
	float	laststop;
} pitchdrift_t;

static viewinterp_t ViewInterp;
static pitchdrift_t pd;

// view CVARS
cvar_t	*scr_ofsx;
cvar_t	*scr_ofsy;
cvar_t	*scr_ofsz;
cvar_t	*cl_vsmoothing;
cvar_t	*cl_stairsmooth;
cvar_t	*cl_weaponlag;

cvar_t	*cl_bobcycle;
cvar_t	*cl_bob;
cvar_t	*cl_bobup;
cvar_t	*cl_waterdist;
cvar_t	*cl_chasedist;

cvar_t	*r_studio_lerping;
cvar_t	*v_iyaw_cycle;
cvar_t	*v_iroll_cycle;
cvar_t	*v_ipitch_cycle;
cvar_t	*v_iyaw_level;
cvar_t	*v_iroll_level;
cvar_t	*v_ipitch_level;
cvar_t	*v_dark;

//============================================================================== 
//				VIEW RENDERING 
//============================================================================== 

//==========================
// V_ThirdPerson
//==========================
void V_ThirdPerson( void )
{
	// no thirdperson in multiplayer
	if( gEngfuncs.GetMaxClients() > 1 ) return;

	gHUD.m_iCameraMode = 1;
}

//==========================
// V_FirstPerson
//==========================
void V_FirstPerson( void )
{
	gHUD.m_iCameraMode = 0;
}

/*
=============
V_PunchAxis

Client side punch effect
=============
*/
void V_PunchAxis( int axis, float punch )
{
	ev_punchangle[axis] = punch;
}

//==========================
// V_Init
//==========================
void V_Init( void )
{
	scr_ofsx = gEngfuncs.pfnRegisterVariable( "scr_ofsx", "0", 0, "screen offset by X" );
	scr_ofsy = gEngfuncs.pfnRegisterVariable( "scr_ofsy", "0", 0, "screen offset by Y" );
	scr_ofsz = gEngfuncs.pfnRegisterVariable( "scr_ofsz", "0", 0, "screen offset by Z" );
	r_studio_lerping = gEngfuncs.pfnRegisterVariable( "r_studio_lerping", "1", FCVAR_ARCHIVE, "enables studio model animation lerping" ); // get copy of engine cvar

	cl_vsmoothing = gEngfuncs.pfnRegisterVariable( "cl_vsmoothing", "0.05", 0, "enables lepring in multiplayer" );
	cl_stairsmooth = gEngfuncs.pfnRegisterVariable( "cl_vstairsmooth", "100", FCVAR_ARCHIVE, "how fast your view moves upward/downward when running up/down stairs" );

	v_iyaw_cycle = gEngfuncs.pfnRegisterVariable( "v_iyaw_cycle", "2", 0, "viewing inverse yaw cycle" );
	v_iroll_cycle = gEngfuncs.pfnRegisterVariable( "v_iroll_cycle", "0.5", 0, "viewing inverse roll cycle" );
	v_ipitch_cycle = gEngfuncs.pfnRegisterVariable( "v_ipitch_cycle", "1", 0, "viewing inverse pitch cycle" );
	v_iyaw_level = gEngfuncs.pfnRegisterVariable( "v_iyaw_level", "0.3", 0, "viewing inverse yaw level" );
	v_iroll_level = gEngfuncs.pfnRegisterVariable( "v_iroll_level", "0.1", 0, "viewing inverse roll level" );
	v_ipitch_level = gEngfuncs.pfnRegisterVariable( "v_iyaw_level", "0.3", 0, "viewing inverse pitch level" );
	v_dark = gEngfuncs.pfnRegisterVariable( "v_dark", "0", 0, "gross hack to make first frame black. half-life legacy" );

	cl_weaponlag = gEngfuncs.pfnRegisterVariable( "v_viewmodel_lag", "0.0", FCVAR_ARCHIVE, "add some lag to viewmodel like in HL2" );
	cl_bobcycle = gEngfuncs.pfnRegisterVariable( "cl_bobcycle","0.8", 0, "bob full cycle" );
	cl_bob = gEngfuncs.pfnRegisterVariable( "cl_bob", "0.01", 0, "bob value" );
	cl_bobup = gEngfuncs.pfnRegisterVariable( "cl_bobup", "0.5", 0, "bob upper limit" );
	cl_waterdist = gEngfuncs.pfnRegisterVariable( "cl_waterdist", "4", 0, "distance between viewofs and water plane" );
	cl_chasedist = gEngfuncs.pfnRegisterVariable( "cl_chasedist", "112", 0, "max distance to chase camera" );
	gEngfuncs.pfnAddCommand( "thirdperson", V_ThirdPerson, "change camera to thirdperson" );
	gEngfuncs.pfnAddCommand( "firstperson", V_FirstPerson, "change camera to firstperson" );
}

//==========================
// V_StartPitchDrift
//==========================
void V_StartPitchDrift( void )
{
	if( pd.laststop == GetClientTime( ))
		return;

	if( pd.nodrift || !pd.pitchvel )
	{
		pd.pitchvel = v_centerspeed->value;
		pd.nodrift = 0;
		pd.driftmove = 0;
	}
}

//==========================
// V_StopPitchDrift
//==========================
void V_StopPitchDrift( void )
{
	pd.laststop = GetClientTime();
	pd.nodrift = 1;
	pd.pitchvel = 0;
}

//==========================
// V_DriftPitch
//==========================
void V_DriftPitch( ref_params_t *pparams )
{
	float	delta, move;

	if( gEngfuncs.IsNoClipping() || !pparams->onground || pparams->demoplayback )
	{
		pd.driftmove = 0;
		pd.pitchvel = 0;
		return;
	}

	if( pd.nodrift )
	{
		if( fabs( pparams->cmd->forwardmove ) < cl_forwardspeed->value )
			pd.driftmove = 0;
		else pd.driftmove += pparams->frametime;
		if( pd.driftmove > v_centermove->value ) V_StartPitchDrift();
		return;
	}
	
	delta = pparams->idealpitch - pparams->cl_viewangles[PITCH];

	if( !delta )
	{
		pd.pitchvel = 0;
		return;
	}

	move = pparams->frametime * pd.pitchvel;
	pd.pitchvel += pparams->frametime * v_centerspeed->value;
	
	if( delta > 0 )
	{
		if( move > delta )
		{
			pd.pitchvel = 0;
			move = delta;
		}
		pparams->cl_viewangles[PITCH] += move;
	}
	else if( delta < 0 )
	{
		if( move > -delta )
		{
			pd.pitchvel = 0;
			move = -delta;
		}
		pparams->cl_viewangles[PITCH] -= move;
	}
}

//==========================
// V_CalcFov
//==========================
float V_CalcFov( float fov_x, float width, float height )
{
	// check to avoid division by zero
	if( fov_x < 1 || fov_x > 179 )
	{
		Con_Printf( "V_CalcFov: invalid fov %g!\n", fov_x );
		fov_x = 90;
	}

	float x = width / tan( DEG2RAD( fov_x ) * 0.5f );
	float half_fov_y = atan( height / x );
	return RAD2DEG( half_fov_y ) * 2;
}

//==========================
// V_DropPunchAngle
//==========================
void V_DropPunchAngle( float frametime, Vector &ev_punchangle )
{
	float	len;

	len = ev_punchangle.Length();
	ev_punchangle = ev_punchangle.Normalize();

	len -= (10.0 + len * 0.5) * frametime;
	len = max( len, 0.0 );
	ev_punchangle *= len;
}

void V_CalcViewModelLag( ref_params_t *pparams, Vector& origin, Vector& angles, Vector& original_angles )
{
	static Vector m_vecLastFacing;
	Vector vOriginalOrigin = origin;
	Vector vOriginalAngles = angles;

	// Calculate our drift
	Vector	forward;
	AngleVectors( angles, forward, NULL, NULL );

	if ( pparams->frametime != 0.0f )
	{
		Vector vDifference;

		vDifference = forward - m_vecLastFacing;

		float flSpeed = 5.0f;

		// If we start to lag too far behind, we'll increase the "catch up" speed.
		// Solves the problem with fast cl_yawspeed, m_yaw or joysticks rotating quickly.
		// The old code would slam lastfacing with origin causing the viewmodel to pop to a new position
		float flDiff = vDifference.Length();
		if (( flDiff > cl_weaponlag->value ) && ( cl_weaponlag->value > 0.0f ))
		{
			float flScale = flDiff / cl_weaponlag->value;
			flSpeed *= flScale;
		}

		// FIXME:  Needs to be predictable?
		m_vecLastFacing = m_vecLastFacing + vDifference * ( flSpeed * pparams->frametime );
		// Make sure it doesn't grow out of control!!!
		m_vecLastFacing = m_vecLastFacing.Normalize();
		origin = origin + (vDifference * -1.0f) * 5.0f;
		ASSERT( m_vecLastFacing.IsValid() );
	}

	Vector right, up;
	AngleVectors( original_angles, forward, right, up );

	float pitch = original_angles[PITCH];

	if ( pitch > 180.0f )
	{
		pitch -= 360.0f;
	}
	else if ( pitch < -180.0f )
	{
		pitch += 360.0f;
	}

	if ( cl_weaponlag->value <= 0.0f )
	{
		origin = vOriginalOrigin;
		angles = vOriginalAngles;
	}
	else
	{
		// FIXME: These are the old settings that caused too many exposed polys on some models
		origin = origin + forward * ( -pitch * 0.035f );
		origin = origin + right * ( -pitch * 0.03f );
		origin = origin + up * ( -pitch * 0.02f );
	}
}

//==========================
// V_CalcGunAngle
//==========================
void V_CalcGunAngle( ref_params_t *pparams )
{	
	if( pparams->fov_x > 135 ) return;

	cl_entity_t *viewent = GetViewModel();

	if( !viewent->curstate.modelindex )
		return;

	viewent->curstate.effects |= EF_MINLIGHT;

	viewent->angles[YAW] = pparams->viewangles[YAW] + pparams->crosshairangle[YAW];
	viewent->angles[PITCH] = pparams->viewangles[PITCH] - pparams->crosshairangle[PITCH] * 0.25;
	viewent->angles[ROLL] = pparams->viewangles[ROLL];
	viewent->angles[ROLL] -= v_idlescale * sin(pparams->time * v_iroll_cycle->value) * v_iroll_level->value;
	
	// don't apply all of the v_ipitch to prevent normally unseen parts of viewmodel from coming into view.
	viewent->angles[PITCH] -= v_idlescale * sin(pparams->time * v_ipitch_cycle->value) * (v_ipitch_level->value * 0.5);
	viewent->angles[YAW]   -= v_idlescale * sin(pparams->time * v_iyaw_cycle->value) * v_iyaw_level->value;
}

//==========================
// V_PreRender
//==========================
void V_PreRender( ref_params_t *pparams )
{
	// pass into beam global vars
	g_pViewRenderBeams->SetViewParams( pparams );

	// input
	pparams->intermission = gHUD.m_iIntermission;
	if( gHUD.m_iCameraMode ) pparams->flags |= RDF_THIRDPERSON;
	else pparams->flags &= ~RDF_THIRDPERSON;

	pparams->fov_x = gHUD.m_flFOV; // this is a final fov value
	pparams->fov_y = V_CalcFov( pparams->fov_x, pparams->viewport[2], pparams->viewport[3] );

	memset( pparams->blend, 0, sizeof( pparams->blend ));
}

//==========================
// V_CalcGlobalFog
//==========================
void V_CalcGlobalFog( ref_params_t *pparams )
{
	int bOn = (pparams->waterlevel < 2) && (gHUD.m_flStartDist > 0) && (gHUD.m_flEndDist > 0 && gHUD.m_flStartDist);
	gEngfuncs.pTriAPI->Fog( gHUD.m_vecFogColor, gHUD.m_flStartDist, gHUD.m_flEndDist, bOn );
}

//==========================
// V_CalcBob
//==========================
float V_CalcBob( ref_params_t *pparams )
{
	static double bobtime;
	static float bob;
	static float lasttime;
	float cycle;
	Vector vel;

	if( !pparams->onground || pparams->time == lasttime )
		return bob;	
	lasttime = pparams->time;

	bobtime += pparams->frametime;
	cycle = bobtime - (int)( bobtime / cl_bobcycle->value ) * cl_bobcycle->value;
	cycle /= cl_bobcycle->value;
	
	if( cycle < cl_bobup->value ) cycle = M_PI * cycle / cl_bobup->value;
	else cycle = M_PI + M_PI * ( cycle - cl_bobup->value )/( 1.0 - cl_bobup->value );

	vel = pparams->simvel;
	vel[2] = 0;

	bob = sqrt( vel[0] * vel[0] + vel[1] * vel[1] ) * cl_bob->value;
	bob = bob * 0.3 + bob * 0.7 * sin( cycle );
	bob = min( bob, 4 );
	bob = max( bob, -7 );
	return bob;
}

//==========================
// V_AddIdle
//==========================
void V_AddIdle( ref_params_t *pparams )
{
	pparams->viewangles[ROLL] += v_idlescale * sin(pparams->time*v_iroll_cycle->value) * v_iroll_level->value;
	pparams->viewangles[PITCH] += v_idlescale * sin(pparams->time*v_ipitch_cycle->value) * v_ipitch_level->value;
	pparams->viewangles[YAW] += v_idlescale * sin(pparams->time*v_iyaw_cycle->value) * v_iyaw_level->value;
}

//==========================
// V_CalcViewRoll
//==========================
void V_CalcViewRoll( ref_params_t *pparams )
{
	float   sign, side, value;
	cl_entity_t *viewentity;
	Vector  right;

	viewentity = GetEntityByIndex( pparams->viewentity );
	if( !viewentity ) return;

	if( pparams->health <= 0 && ( pparams->viewheight[2] != 0 ))
	{
		GetViewModel()->curstate.modelindex = 0;	// clear the viewmodel
		pparams->viewangles[ROLL] = 80;		// dead view angle
		return;
	}

	if( pparams->demoplayback ) return;

	AngleVectors( viewentity->angles, NULL, right, NULL );
	side = right.Dot( pparams->simvel );
	sign = side < 0 ? -1 : 1;
	side = fabs( side );
	value = pparams->movevars->rollangle;
	if( side < pparams->movevars->rollspeed )
		side = side * value / pparams->movevars->rollspeed;
	else side = value;
	side = side * sign;		
	pparams->viewangles[ROLL] += side;
}

//==========================
// V_SetViewport
//==========================
void V_SetViewport( ref_params_t *pparams )
{
	pparams->viewport[0] = 0;
	pparams->viewport[1] = 0;
	pparams->viewport[2] = VIEWPORT_SIZE;
	pparams->viewport[3] = VIEWPORT_SIZE;
}

//==========================
// V_GetChaseOrigin
//==========================
void V_GetChaseOrigin( Vector angles, Vector origin, float distance, Vector &result )
{
	Vector	vecEnd;
	Vector	forward;
	Vector	vecStart;
	pmtrace_t * trace;
	int maxLoops = 8;

	cl_entity_t *ent = NULL;
	int ignoreent = -1;	// first, ignore no entity
	
	// trace back from the target using the player's view angles
	AngleVectors( angles, forward, NULL, NULL );
	forward *= -1;
	vecStart = origin;
	vecEnd.MA( distance, vecStart, forward );

	while( maxLoops > 0 )
	{
		trace = gEngfuncs.PM_TraceLine( vecStart, vecEnd, PM_TRACELINE_PHYSENTSONLY, 2, ignoreent );
		if( trace->ent <= 0 ) break; // we hit the world or nothing, stop trace

		ent = GetEntityByIndex( PM_GetPhysEntInfo( trace->ent ));

		if ( ent == NULL )
			break;

		// hit non-player solid BSP, stop here
		if( ent->curstate.solid == SOLID_BSP && !ent->player )
			break;

		// if close enought to end pos, stop, otherwise continue trace
		if( trace->endpos.Distance( vecEnd ) < 1.0f )
			break;
		else
		{
			ignoreent = trace->ent; // ignore last hit entity
			vecStart = trace->endpos;
		}
		maxLoops--;
	}  

	result.MA( 4, trace->endpos, trace->plane.normal );
	v_lastDistance = trace->endpos.Distance( origin );	// real distance without offset
}

//==========================
// V_GetChasePos
//==========================
void V_GetChasePos( ref_params_t *pparams, cl_entity_t *ent, float *cl_angles, Vector &origin, Vector &angles )
{
	if ( !ent )
	{
		// just copy a save in-map position
		angles = Vector( 0, 0, 0 );
		origin = Vector( 0, 0, 0 );
		return;
	}

	if ( cl_angles == NULL )
	{
		// no mouse angles given, use entity angles ( locked mode )
		angles = ent->angles;
		angles.x *= -1;
	}
	else
	{
		angles = cl_angles;
	}

	// refresh the position
	origin = ent->origin;
	origin[2] += 28; // DEFAULT_VIEWHEIGHT - some offset
          V_GetChaseOrigin( angles, origin, cl_chasedist->value, origin );
}

//==========================
// V_CalcCameraRefdef
//==========================
void V_CalcCameraRefdef( ref_params_t *pparams )
{
	if( pparams->intermission ) return;	// disable in intermission mode

	if( GetEntityByIndex( pparams->viewentity ) != GetLocalPlayer( ))
	{
		// this is a viewentity which sets by SET_VIEW builtin
		cl_entity_t *viewentity = GetEntityByIndex( pparams->viewentity );
	 	if( viewentity )
		{		 
			studiohdr_t *viewmonster = (studiohdr_t *)Mod_Extradata( viewentity->curstate.modelindex );
			float m_fLerp = pparams->lerpfrac;

			if( viewentity->curstate.movetype == MOVETYPE_STEP )
				v_origin = LerpPoint( viewentity->prevstate.origin, viewentity->curstate.origin, m_fLerp );
			else v_origin = viewentity->origin;	// already interpolated

			// add view offset
			if( viewmonster ) v_origin += viewmonster->eyeposition;

			if( viewentity->curstate.movetype == MOVETYPE_STEP )
				v_angles = LerpAngle( viewentity->prevstate.angles, viewentity->curstate.angles, m_fLerp );
			else v_angles = viewentity->angles;	// already interpolated

			pparams->crosshairangle[ROLL] = 1;	// crosshair is hided

			// refresh position
			pparams->viewangles = v_angles;
			pparams->vieworg = v_origin;
		}		
	}
	else pparams->crosshairangle[ROLL] = 0; // show crosshair again
}

cl_entity_t *V_FindIntermisionSpot( ref_params_t *pparams )
{
	cl_entity_t *ent;
	int spotindex[16];	// max number of intermission spot
	int k = 0, j = 0;

	// found intermission points
	for( int i = 0; i < pparams->num_entities; i++ )
	{
		ent = GetEntityByIndex( i );
		if( ent && !stricmp( ent->curstate.classname, "info_intermission" ))
		{
			if( j > 15 ) break; // spotlist is full
			spotindex[j] = ent->index; // save entindex
			j++;
		}
	}	
	
	// ok, we have list of intermission spots
	if( j )
	{
		if( j > 1 ) k = RANDOM_LONG( 0, j - 1 );
		ent = GetEntityByIndex( spotindex[k] );
	}
	else ent = GetLocalPlayer(); // just get view from player
	
	return ent;
}

//==========================
// V_CalcIntermisionRefdef
//==========================
void V_CalcIntermisionRefdef( ref_params_t *pparams )
{
	if( !pparams->intermission ) return;

	cl_entity_t *view;
	float	  old;

          if( !spot ) spot = V_FindIntermisionSpot( pparams );
	view = GetViewModel();

	// need to lerping position ?
	pparams->vieworg = spot->origin;
	pparams->viewangles = spot->angles;
	view->curstate.modelindex = 0;

	// allways idle in intermission
	old = v_idlescale;
	v_idlescale = 1;
	V_AddIdle( pparams );

	v_idlescale = old;
	v_cl_angles = pparams->cl_viewangles;
	v_origin = pparams->vieworg;
	v_angles = pparams->viewangles;
}

//==========================
// V_CalcShake
//==========================
void V_CalcShake( void )
{
	float	frametime;
	int	i;
	float	fraction, freq;

	if( gHUD.m_Shake.time == 0 )
		return;

	if(( gHUD.m_flTime > gHUD.m_Shake.time ) || gHUD.m_Shake.duration <= 0 || gHUD.m_Shake.amplitude <= 0 || gHUD.m_Shake.frequency <= 0 )
	{
		memset( &gHUD.m_Shake, 0, sizeof( gHUD.m_Shake ));
		return;
	}

	frametime = gHUD.m_flTimeDelta;

	if( gHUD.m_flTime > gHUD.m_Shake.nextShake )
	{
		// higher frequency means we recalc the extents more often and perturb the display again
		gHUD.m_Shake.nextShake = gHUD.m_flTime + (1.0f / gHUD.m_Shake.frequency);

		// Compute random shake extents (the shake will settle down from this)
		for( i = 0; i < 3; i++ )
		{
			gHUD.m_Shake.offset[i] = RANDOM_FLOAT( -gHUD.m_Shake.amplitude, gHUD.m_Shake.amplitude );
		}
		gHUD.m_Shake.angle = RANDOM_FLOAT( -gHUD.m_Shake.amplitude * 0.25, gHUD.m_Shake.amplitude * 0.25 );
	}

	// ramp down amplitude over duration (fraction goes from 1 to 0 linearly with slope 1/duration)
	fraction = ( gHUD.m_Shake.time - gHUD.m_flTime ) / gHUD.m_Shake.duration;

	// ramp up frequency over duration
	if( fraction )
	{
		freq = (gHUD.m_Shake.frequency / fraction);
	}
	else
	{
		freq = 0;
	}

	// square fraction to approach zero more quickly
	fraction *= fraction;

	// Sine wave that slowly settles to zero
	fraction = fraction * sin( gHUD.m_flTime * freq );
	
	// add to view origin
	gHUD.m_Shake.appliedOffset = gHUD.m_Shake.offset * fraction;
	
	// add to roll
	gHUD.m_Shake.appliedAngle = gHUD.m_Shake.angle * fraction;

	// drop amplitude a bit, less for higher frequency shakes
	float localAmp = gHUD.m_Shake.amplitude * ( frametime / ( gHUD.m_Shake.duration * gHUD.m_Shake.frequency ));
	gHUD.m_Shake.amplitude -= localAmp;
}

//==========================
// V_ApplyShake
//==========================
void V_ApplyShake( Vector& origin, Vector& angles, float factor )
{
	origin.MA( factor, origin, gHUD.m_Shake.appliedOffset );
	angles.z += gHUD.m_Shake.appliedAngle * factor;
}

//==========================
// V_CalcThirdPersonRefdef
//==========================
void V_CalcThirdPersonRefdef( ref_params_t *pparams )
{
	// passed only in third person
	if( gHUD.m_iCameraMode == 0 || pparams->intermission )
		return;

	// clear viewmodel for thirdperson
	cl_entity_t *viewent = GetViewModel();
	viewent->curstate.modelindex = 0;

	// get current values
	v_cl_angles = pparams->cl_viewangles;
	v_angles = pparams->viewangles;
	v_origin = pparams->vieworg;

	V_GetChasePos( pparams, GetLocalPlayer(), v_cl_angles, v_origin, v_angles );

	// write back new values
	pparams->cl_viewangles = v_cl_angles;
	pparams->viewangles = v_angles;
	pparams->vieworg = v_origin;

	// apply shake for thirdperson too
	V_CalcShake();
	V_ApplyShake( pparams->vieworg, pparams->viewangles, 1.0 );
}

//==========================
// V_CalcSendOrigin
//==========================
void V_CalcSendOrigin( ref_params_t *pparams )
{
	// never let view origin sit exactly on a node line, because a water plane can
	// dissapear when viewed with the eye exactly on it.
	pparams->vieworg[0] += 1.0 / 32;
	pparams->vieworg[1] += 1.0 / 32;
	pparams->vieworg[2] += 1.0 / 32;
}

//==========================
// V_CalcWaterLevel
//==========================
float V_CalcWaterLevel( ref_params_t *pparams )
{
	float waterOffset = 0;
	
	if( pparams->waterlevel >= 2 )
	{
		int	i, contents;
		float	waterDist = cl_waterdist->value;
		Vector	point;

		cl_entity_t *pwater = gEngfuncs.pfnWaterEntity( pparams->simorg );
		if( pwater ) waterDist += ( pwater->curstate.scale * 16 );

		point = pparams->vieworg;

		// eyes are above water, make sure we're above the waves
		if( pparams->waterlevel == 2 )	
		{
			point[2] -= waterDist;
			for( i = 0; i < waterDist; i++ )
			{
				contents = POINT_CONTENTS( point );
				if( contents > CONTENTS_WATER ) break;
				point[2] += 1;
			}
			waterOffset = (point[2] + waterDist) - pparams->vieworg[2];
		}
		else
		{
			// eyes are under water. Make sure we're far enough under
			point[2] += waterDist;
			for( i = 0; i < waterDist; i++ )
			{
				contents = POINT_CONTENTS( point );
				if( contents <= CONTENTS_WATER ) break;
				point[2] -= 1;
			}
			waterOffset = (point[2] - waterDist) - pparams->vieworg[2];
		}
	}

	// underwater refraction
	if( pparams->waterlevel == 3 )
	{
		float f = sin( pparams->time * 0.4 * (M_PI * 2.7));
		pparams->fov_x += f;
		pparams->fov_y -= f;
	}

	pparams->vieworg[2] += waterOffset;
	return waterOffset;
}

//==========================
// V_CalcScrOffset
//==========================
void V_CalcScrOffset( ref_params_t *pparams )
{
	// don't allow cheats in multiplayer
	if( pparams->maxclients > 1 ) return;

	for( int i = 0; i < 3; i++ )
	{
		pparams->vieworg[i] += scr_ofsx->value * pparams->forward[i];
		pparams->vieworg[i] += scr_ofsy->value * pparams->right[i];
		pparams->vieworg[i] += scr_ofsz->value * pparams->up[i];
	}
}

//==========================
// V_InterpolatePos
//==========================
void V_InterpolatePos( ref_params_t *pparams )
{
	cl_entity_t *view;

	// view is the weapon model (only visible from inside body )
	view = GetViewModel();

	if( cl_vsmoothing->value && ( pparams->smoothing && ( pparams->maxclients > 1 )))
	{
		int	i, foundidx;
		float	t;

		if( cl_vsmoothing->value < 0.0 ) CVAR_SET_FLOAT( "cl_vsmoothing", 0 );
		t = pparams->time - cl_vsmoothing->value;
		for( i = 1; i < ORIGIN_MASK; i++ )
		{
			foundidx = ViewInterp.CurrentOrigin - 1 - i;
			if( ViewInterp.OriginTime[foundidx & ORIGIN_MASK] <= t ) break;
		}

		if( i < ORIGIN_MASK && ViewInterp.OriginTime[foundidx & ORIGIN_MASK] != 0.0 )
		{
			// interpolate
			Vector delta;
			double frac;
			double dt;
			Vector neworg;

			dt = ViewInterp.OriginTime[(foundidx + 1) & ORIGIN_MASK] - ViewInterp.OriginTime[foundidx & ORIGIN_MASK];
			if ( dt > 0.0 )
			{
				frac = ( t - ViewInterp.OriginTime[foundidx & ORIGIN_MASK] ) / dt;
				frac = min( 1.0, frac );
				delta = ViewInterp.Origins[(foundidx + 1) & ORIGIN_MASK] - ViewInterp.Origins[foundidx & ORIGIN_MASK];
				neworg.MA( frac, ViewInterp.Origins[foundidx & ORIGIN_MASK], delta );

				// don't interpolate large changes
				if( delta.Length() < 64 )
				{
					delta = neworg - pparams->simorg;
					pparams->simorg += delta;
					pparams->vieworg += delta;
					view->origin += delta;

				}
			}
		}
	}
}

float V_CalcStairSmoothValue( float oldz, float newz, float smoothtime, float stepheight )
{
	if( oldz < newz )
		return bound( newz - stepheight, oldz + smoothtime * cl_stairsmooth->value, newz );
	else if( oldz > newz )
		return bound( newz, oldz - smoothtime * cl_stairsmooth->value, newz + stepheight );
	return 0.0;
}

//==========================
// V_CalcFirstPersonRefdef
//==========================
void V_CalcFirstPersonRefdef( ref_params_t *pparams )
{
	// don't pass in thirdperson or intermission
	if( gHUD.m_iCameraMode || pparams->intermission )
		return;

	Vector angles;
	float bob, waterOffset;
	static float lasttime;
	cl_entity_t *view = GetViewModel();
	int i;

	V_DriftPitch( pparams );
	bob = V_CalcBob( pparams );

	// refresh the position
	pparams->vieworg = pparams->simorg; 
	pparams->vieworg[2] += ( bob );
	pparams->vieworg += pparams->viewheight;

	pparams->viewangles = pparams->cl_viewangles;

	V_CalcShake();
	V_ApplyShake( pparams->vieworg, pparams->viewangles, 1.0 );

	V_CalcSendOrigin( pparams );
	waterOffset = V_CalcWaterLevel( pparams );
	V_CalcViewRoll( pparams );
	V_AddIdle( pparams );

	// offsets
	angles = pparams->viewangles;
	AngleVectors( angles, pparams->forward, pparams->right, pparams->up );
	V_CalcScrOffset( pparams );

	Vector	lastAngles;

	lastAngles = view->angles = pparams->cl_viewangles;
	V_CalcGunAngle( pparams );

	// use predicted origin as view origin.
	view->origin = pparams->simorg;      
	view->origin[2] += ( waterOffset );
	view->origin += pparams->viewheight;

	// Let the viewmodel shake at about 10% of the amplitude
	V_ApplyShake( view->origin, view->angles, 0.9 );

	for( i = 0; i < 3; i++ )
		view->origin[i] += bob * 0.4 * pparams->forward[i];
	view->origin[2] += bob;

	view->angles[YAW] -= bob * 0.5;
	view->angles[ROLL] -= bob * 1;
	view->angles[PITCH] -= bob * 0.3;
	view->origin[2] -= 1;

	// add lag
	V_CalcViewModelLag( pparams, view->origin, view->angles, lastAngles );

	// fudge position around to keep amount of weapon visible
	// roughly equal with different FOV
	if( pparams->viewsize == 110 ) view->origin[2] += 1;
	else if( pparams->viewsize == 100 ) view->origin[2] += 2;
	else if( pparams->viewsize == 90 ) view->origin[2] += 1;
	else if( pparams->viewsize == 80 ) view->origin[2] += 0.5;

	pparams->viewangles += pparams->punchangle;
	pparams->viewangles += ev_punchangle;

	V_DropPunchAngle( pparams->frametime, ev_punchangle );

	static float stairoldtime = 0;
	static float old_client_z = 0;
	static float old_weapon_z = 0;

	// calculate how much time has passed since the last V_CalcRefdef
	float smoothtime = bound( 0.0, pparams->time - stairoldtime, 0.1 );
	stairoldtime = pparams->time;

	// smooth stair stepping, but only if onground and enabled
	if( !pparams->smoothing || !pparams->onground || cl_stairsmooth->value <= 0 )
	{
		old_client_z = pparams->vieworg[2];
		old_weapon_z = view->origin[2];
	}
	else
	{
		float	result;
		float	stepheight = pparams->movevars->stepsize;

		result = V_CalcStairSmoothValue( old_client_z, pparams->vieworg[2], smoothtime, stepheight );
		if( result ) pparams->vieworg[2] = old_client_z = result;
		result = V_CalcStairSmoothValue( old_weapon_z, view->origin[2], smoothtime, stepheight );
		if( result ) view->origin[2] = old_weapon_z = result;
	}

	static Vector lastorg;
	Vector delta;

	delta = pparams->simorg - lastorg;

	if( delta.Length() != 0.0 )
	{
		ViewInterp.Origins[ViewInterp.CurrentOrigin & ORIGIN_MASK] = pparams->simorg;
		ViewInterp.OriginTime[ViewInterp.CurrentOrigin & ORIGIN_MASK] = pparams->time;
		ViewInterp.CurrentOrigin++;

		lastorg = pparams->simorg;
	}

//	probably not needs in Xash3D
//	V_InterpolatePos( pparams ); // smooth predicting moving in multiplayer

	lasttime = pparams->time;
	v_origin = pparams->vieworg;	
}

//==========================
// V_CalcScreenBlend
//==========================
void V_CalcScreenBlend( ref_params_t *pparams )
{
#if 0
	// FIXME: get some code from q1
	pparams->blend[0] = 0.0f;
	pparams->blend[1] = 0.0f;
	pparams->blend[2] = 0.0f;
	pparams->blend[3] = 1.0f;
#endif
}

void V_CalcRefdef( ref_params_t *pparams )
{
	V_PreRender( pparams );
	V_CalcGlobalFog( pparams );
	V_CalcFirstPersonRefdef( pparams );
	V_CalcThirdPersonRefdef( pparams );
	V_CalcIntermisionRefdef( pparams );
	V_CalcCameraRefdef( pparams );
	V_CalcScreenBlend( pparams );
        
}