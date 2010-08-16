//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       ev_common.cpp - events common code
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "ev_hldm.h"
#include "r_tempents.h"
#include "ref_params.h"
#include "pm_defs.h"

extern ref_params_t		*gpViewParams;

extern "C"
{
	void EV_EjectBrass( event_args_t *args );
	void EV_FireNull( event_args_t *args );
	void EV_FireCrowbar( event_args_t *args );
	void EV_PlayEmptySound( event_args_t *args );
	void EV_FireGlock1( event_args_t *args  );
	void EV_FireMP5( event_args_t *args  );
	void EV_FirePython( event_args_t *args  );
	void EV_FireGauss( event_args_t *args  );
	void EV_SpinGauss( event_args_t *args  );
	void EV_EgonFire( event_args_t *args );
	void EV_EgonStop( event_args_t *args );
	void EV_FireShotGunSingle( event_args_t *args  );
	void EV_FireShotGunDouble( event_args_t *args  );
	void EV_SnarkFire( event_args_t *args );
	void EV_TrainPitchAdjust( event_args_t *args );
	void EV_Decals( event_args_t *args );
}

//======================
//    Game_HookEvents
//======================
void EV_HookEvents( void )
{
	g_engfuncs.pfnHookEvent( "events/crowbar.sc", EV_FireCrowbar );
	g_engfuncs.pfnHookEvent( "events/glock1.sc", EV_FireGlock1 );
	g_engfuncs.pfnHookEvent( "events/shotgun1.sc", EV_FireShotGunSingle );
	g_engfuncs.pfnHookEvent( "events/shotgun2.sc", EV_FireShotGunDouble );
	g_engfuncs.pfnHookEvent( "events/mp5.sc", EV_FireMP5 );
	g_engfuncs.pfnHookEvent( "events/python.sc", EV_FirePython );
	g_engfuncs.pfnHookEvent( "events/gauss.sc", EV_FireGauss );
	g_engfuncs.pfnHookEvent( "events/gaussspin.sc", EV_SpinGauss );
	g_engfuncs.pfnHookEvent( "events/egon_fire.sc", EV_EgonFire );
	g_engfuncs.pfnHookEvent( "events/egon_stop.sc", EV_EgonStop );
	g_engfuncs.pfnHookEvent( "events/train.sc", EV_TrainPitchAdjust );
	g_engfuncs.pfnHookEvent( "events/snarkfire.sc", EV_SnarkFire );

	// legacy. attempt to be removed
	g_engfuncs.pfnHookEvent( "evEjectBrass", EV_EjectBrass );
	g_engfuncs.pfnHookEvent( "evNull", EV_FireNull );
	g_engfuncs.pfnHookEvent( "evEmptySound", EV_PlayEmptySound );
	g_engfuncs.pfnHookEvent( "evDecals", EV_Decals );
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
	if ( idx >= 1 && idx <= gpGlobals->maxClients )
		return true;
	return false;
}


//=================
//     EV_IsLocal
//=================
int EV_IsLocal( int idx )
{
	return g_engfuncs.pEventAPI->EV_IsLocal( idx - 1 ) ? true : false;
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
			g_engfuncs.pEventAPI->EV_LocalPlayerViewheight( view_ofs );
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
			g_engfuncs.pEventAPI->EV_LocalPlayerViewheight( view_ofs );
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
		// get the predicted angles
		AngleVectors( gpViewParams->cl_viewangles, forward, NULL, NULL );
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
			g_engfuncs.pEventAPI->EV_LocalPlayerViewheight( view_ofs );
		}
		else if ( pEnt->curstate.usehull == 1 )	// NOTE: needs changes in delta.lst
		{
			view_ofs[2] = VEC_DUCK_VIEW;
		}
	}

	vecSrc = pEnt->origin + view_ofs;
	vecEnd = vecSrc + forward * 512;
	int ignore = PM_FindPhysEntByIndex( pEnt->index );

	trace = g_engfuncs.PM_TraceLine( vecSrc, vecEnd, PM_TRACELINE_PHYSENTSONLY, 2, ignore );

	if( trace->fraction != 1.0f )
		vecPos = trace->endpos + (trace->plane.normal * -16.0f);
	else vecPos = trace->endpos;

	// update flashlight endpos
	dlight_t	*dl = g_engfuncs.pEfxAPI->CL_AllocDLight( pEnt->index );
	
	dl->origin = vecPos;
	dl->die = gpGlobals->time + 0.001f;	// die on next frame
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
	if( cl_lw->integer )
	{
		previousorigin = gpViewParams->vieworg;	// FIXME: probably this is not correct
	}
}