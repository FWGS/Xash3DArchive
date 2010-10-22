//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       ev_common.cpp - events common code
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "ev_hldm.h"
#include "r_tempents.h"
#include "pm_defs.h"
#include "hud.h"

void EV_FireGlock1( struct event_args_s *args  );
void EV_FireGlock2( struct event_args_s *args  );
void EV_FireShotGunSingle( struct event_args_s *args  );
void EV_FireShotGunDouble( struct event_args_s *args  );
void EV_FireMP5( struct event_args_s *args  );
void EV_FireMP52( struct event_args_s *args  );
void EV_FirePython( struct event_args_s *args  );
void EV_FireGauss( struct event_args_s *args  );
void EV_SpinGauss( struct event_args_s *args  );
void EV_Crowbar( struct event_args_s *args );
void EV_FireCrossbow( struct event_args_s *args );
void EV_FireCrossbow2( struct event_args_s *args );
void EV_FireRpg( struct event_args_s *args );
void EV_EgonFire( struct event_args_s *args );
void EV_EgonStop( struct event_args_s *args );
void EV_HornetGunFire( struct event_args_s *args );
void EV_TripmineFire( struct event_args_s *args );
void EV_SnarkFire( struct event_args_s *args );
void EV_TrainPitchAdjust( struct event_args_s *args );

//======================
//    Game_HookEvents
//======================
void EV_HookEvents( void )
{
	gEngfuncs.pfnHookEvent( "events/glock1.sc",	EV_FireGlock1 );
	gEngfuncs.pfnHookEvent( "events/glock2.sc",	EV_FireGlock2 );
	gEngfuncs.pfnHookEvent( "events/shotgun1.sc",	EV_FireShotGunSingle );
	gEngfuncs.pfnHookEvent( "events/shotgun2.sc",	EV_FireShotGunDouble );
	gEngfuncs.pfnHookEvent( "events/mp5.sc",	EV_FireMP5 );
	gEngfuncs.pfnHookEvent( "events/mp52.sc",	EV_FireMP52 );
	gEngfuncs.pfnHookEvent( "events/python.sc",	EV_FirePython );
	gEngfuncs.pfnHookEvent( "events/gauss.sc",	EV_FireGauss );
	gEngfuncs.pfnHookEvent( "events/gaussspin.sc",	EV_SpinGauss );
	gEngfuncs.pfnHookEvent( "events/train.sc",	EV_TrainPitchAdjust );
	gEngfuncs.pfnHookEvent( "events/crowbar.sc",	EV_Crowbar );
	gEngfuncs.pfnHookEvent( "events/crossbow1.sc",	EV_FireCrossbow );
	gEngfuncs.pfnHookEvent( "events/crossbow2.sc",	EV_FireCrossbow2 );
	gEngfuncs.pfnHookEvent( "events/rpg.sc",	EV_FireRpg );
	gEngfuncs.pfnHookEvent( "events/egon_fire.sc",	EV_EgonFire );
	gEngfuncs.pfnHookEvent( "events/egon_stop.sc",	EV_EgonStop );
	gEngfuncs.pfnHookEvent( "events/firehornet.sc",	EV_HornetGunFire );
	gEngfuncs.pfnHookEvent( "events/tripfire.sc",	EV_TripmineFire );
	gEngfuncs.pfnHookEvent( "events/snarkfire.sc",	EV_SnarkFire );
}

/*
=================
GetEntity

Return's the requested cl_entity_t
=================
*/
cl_entity_t *GetEntity( int idx )
{
	return GetEntityByIndex( idx );
}

/*
=================
GetViewEntity

Return's the current weapon/view model
=================
*/
cl_entity_t *GetViewEntity( void )
{
	return GetViewModel();
}

//=================
// EV_CreateTracer
//=================
void EV_CreateTracer( float *start, float *end )
{
	g_pTempEnts->TracerEffect( start, end );
}


//=================
//   EV_IsPlayer
//=================
int EV_IsPlayer( int idx )
{
	if ( idx >= 1 && idx <= gEngfuncs.GetMaxClients() )
		return true;
	return false;
}


//=================
//     EV_IsLocal
//=================
int EV_IsLocal( int idx )
{
	return gEngfuncs.pEventAPI->EV_IsLocal( idx - 1 ) ? true : false;
}


//=================
//  EV_GetGunPosition
//=================
void EV_GetGunPosition( event_args_t *args, float *pos, float *origin )
{
	int	idx;
	Vector	view_ofs;

	idx = args->entindex;

	view_ofs = Vector( 0, 0, 0 );
	view_ofs.z = DEFAULT_VIEWHEIGHT;

	if ( EV_IsPlayer( idx ))
	{
		// in spec mode use entity viewheigh, not own
		if ( EV_IsLocal( idx ))
		{
			// Grab predicted result for local player
			gEngfuncs.pEventAPI->EV_LocalPlayerViewheight( view_ofs );
		}
		else if ( args->ducking == 1 )
		{
			view_ofs[2] = VEC_DUCK_VIEW;
		}
	}

	pos[0] = origin[0] + view_ofs.x;
	pos[1] = origin[1] + view_ofs.y;
	pos[2] = origin[2] + view_ofs.z;
}


//=================
//  EV_EjectBrass
//=================
void EV_EjectBrass( float *origin, float *velocity, float rotation, int model, int soundtype )
{
	Vector endpos = Vector( 0.0f, rotation, 0.0f );

	g_pTempEnts->TempModel( origin, velocity, endpos, RANDOM_LONG( 30, 50 ), model, soundtype );
}


//=================
// EV_GetDefaultShellInfo
//=================
void EV_GetDefaultShellInfo( event_args_t *args, float *origin, float *velocity, float *ShellVelocity, float *ShellOrigin, float *forward, float *right, float *up, float forwardScale, float upScale, float rightScale )
{
	int idx;
	Vector view_ofs;
	float fR, fU;

	idx = args->entindex;
	view_ofs = Vector( 0, 0, 0 );
	view_ofs.z = DEFAULT_VIEWHEIGHT;

	if ( EV_IsPlayer( idx ) )
	{
		if ( EV_IsLocal( idx ) )
		{
			gEngfuncs.pEventAPI->EV_LocalPlayerViewheight( view_ofs );
		}
		else if ( args->ducking == 1 )
		{
			view_ofs[2] = VEC_DUCK_VIEW;
		}
	}

	fR = RANDOM_FLOAT( 50, 70 );
	fU = RANDOM_FLOAT( 100, 150 );

	for ( int i = 0; i < 3; i++ )
	{
		ShellVelocity[i] = velocity[i] + right[i] * fR + up[i] * fU + forward[i] * 25;
		ShellOrigin[i] = origin[i] + view_ofs[i] + up[i] * upScale + forward[i] * forwardScale + right[i] * rightScale;
	}
}

//=================
//  EV_MuzzleFlash
//=================
void EV_MuzzleFlash( void )
{
	// Add muzzle flash to current weapon model
	cl_entity_t *ent = GetViewEntity();
	if ( !ent ) return;

	// Or in the muzzle flash
	ent->curstate.effects |= EF_MUZZLEFLASH;
}

//=================
//  EV_FlashLight
//=================
void EV_UpadteFlashlight( cl_entity_t *pEnt )
{
	Vector vecSrc, vecEnd, vecPos, forward;
	float rgba[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	pmtrace_t *trace;

	if ( EV_IsLocal( pEnt->index ) )
	{
		float	viewangles[3];

		// get the predicted angles
		GetViewAngles( viewangles );
		AngleVectors( viewangles, forward, NULL, NULL );
	}
	else
	{
		Vector v_angle;

		// restore viewangles from angles
		v_angle[PITCH] = -pEnt->angles[PITCH] * 3;
		v_angle[YAW] = pEnt->angles[YAW];
		v_angle[ROLL] = 0; 	// no roll

		AngleVectors( v_angle, forward, NULL, NULL );
	}

	Vector view_ofs = Vector( 0, 0, 0 );
	view_ofs.z = DEFAULT_VIEWHEIGHT;

	if ( EV_IsPlayer( pEnt->index ) )
	{
		if ( EV_IsLocal( pEnt->index ) )
		{
			gEngfuncs.pEventAPI->EV_LocalPlayerViewheight( view_ofs );
		}
		else if ( pEnt->curstate.usehull == 1 )	// NOTE: needs changes in delta.lst
		{
			view_ofs[2] = VEC_DUCK_VIEW;
		}
	}

	vecSrc = pEnt->origin + view_ofs;
	vecEnd = vecSrc + forward * 512;
	int ignore = PM_FindPhysEntByIndex( pEnt->index );

	trace = gEngfuncs.PM_TraceLine( vecSrc, vecEnd, PM_TRACELINE_PHYSENTSONLY, 2, ignore );

	if( trace->fraction != 1.0f )
		vecPos = trace->endpos + (trace->plane.normal * -16.0f);
	else vecPos = trace->endpos;

	// update flashlight endpos
	dlight_t	*dl = gEngfuncs.pEfxAPI->CL_AllocDLight( pEnt->index );
	
	dl->origin = vecPos;
	dl->die = GetClientTime() + 0.001f;	// die on next frame
	dl->color[0] = 255;
	dl->color[1] = 255;
	dl->color[2] = 255;
	dl->radius = 96;
}

void HUD_CmdStart( const cl_entity_t *player, int runfuncs )
{
}

void HUD_CmdEnd( const cl_entity_t *player, const usercmd_t *cmd, unsigned int random_seed )
{
	// Offset final origin by view_offset
	if( cl_lw && cl_lw->value )
	{
		// FIXME: probably this is not correct
		previousorigin = gHUD.m_vecOrigin;
	}
}