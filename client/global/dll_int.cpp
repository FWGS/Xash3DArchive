//=======================================================================
//			Copyright XashXT Group 2008 ©
//		          dll_int.cpp - dll entry points 
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "ref_params.h"
#include "studio.h"
#include "hud.h"
#include "aurora.h"
#include "effects_api.h"
#include "r_particle.h"
#include "r_tempents.h"
#include "r_beams.h"
#include "ev_hldm.h"
#include "pm_shared.h"
#include "r_weather.h"

cl_enginefuncs_t	g_engfuncs;
cl_globalvars_t	*gpGlobals;
movevars_t	*gpMovevars = NULL;
ref_params_t	*gpViewParams = NULL;
CHud gHUD;

// main DLL entry point
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
	return TRUE;
}

static HUD_FUNCTIONS gFunctionTable = 
{
	HUD_VidInit,
	HUD_Init,
	HUD_Redraw,
	HUD_UpdateEntityVars,
	HUD_UpdateClientVars,
	HUD_UpdateOnRemove,
	HUD_Reset,
	HUD_StartFrame,
	HUD_Frame,
	HUD_Shutdown,
	HUD_RenderCallback,
	HUD_CreateEntities,
	HUD_AddVisibleEntity,
	HUD_StudioEvent,
	HUD_StudioFxTransform,
	V_CalcRefdef,
	PM_Move,			// pfnPM_Move
	PM_Init,			// pfnPM_Init
	PM_FindTextureType,		// pfnPM_FindTextureType
	HUD_CmdStart,
	HUD_CmdEnd,
	IN_CreateMove,
	IN_MouseEvent,
	IN_KeyEvent,
	VGui_ConsolePrint,
	HUD_ParticleEffect
};

//=======================================================================
//			GetApi
//=======================================================================
int CreateAPI( HUD_FUNCTIONS *pFunctionTable, cl_enginefuncs_t* pEngfuncsFromEngine, cl_globalvars_t *pGlobals )
{
	if( !pFunctionTable || !pEngfuncsFromEngine )
	{
		return FALSE;
	}

	// copy HUD_FUNCTIONS table to engine, copy engfuncs table from engine
	memcpy( pFunctionTable, &gFunctionTable, sizeof( HUD_FUNCTIONS ));
	memcpy( &g_engfuncs, pEngfuncsFromEngine, sizeof( cl_enginefuncs_t ));

	gpGlobals = pGlobals;
	gpViewParams = gpGlobals->pViewParms;
	gpMovevars = gpViewParams->movevars;

	return TRUE;
}

int HUD_VidInit( void )
{
	if ( g_pParticleSystems )
		g_pParticleSystems->ClearSystems();

	if ( g_pViewRenderBeams )
		g_pViewRenderBeams->ClearBeams();

	if ( g_pParticles )
		g_pParticles->Clear();

	if ( g_pTempEnts )
		g_pTempEnts->Clear();

 	ResetRain ();
	
	gHUD.VidInit();

	return 1;
}

void HUD_ShutdownEffects( void )
{
          if ( g_pParticleSystems )
          {
          	// init partsystem
		delete g_pParticleSystems;
		g_pParticleSystems = NULL;
	}

	if ( g_pViewRenderBeams )
	{
		// init render beams
		delete g_pViewRenderBeams;
		g_pViewRenderBeams = NULL;
	}

	if ( g_pParticles )
	{
		// init particles
		delete g_pParticles;
		g_pParticles = NULL;
	}

	if ( g_pTempEnts )
	{
		// init client tempents
		delete g_pTempEnts;
		g_pTempEnts = NULL;
	}
}

void HUD_Init( void )
{
	g_engfuncs.pfnAddCommand ("noclip", NULL, "enable or disable no clipping mode" );
	g_engfuncs.pfnAddCommand ("notarget", NULL, "notarget mode (monsters do not see you)" );
	g_engfuncs.pfnAddCommand ("fullupdate", NULL, "re-init HUD on start demo recording" );
	g_engfuncs.pfnAddCommand ("give", NULL, "give specified item or weapon" );
	g_engfuncs.pfnAddCommand ("drop", NULL, "drop current/specified item or weapon" );
	g_engfuncs.pfnAddCommand ("intermission", NULL, "go to intermission" );
	g_engfuncs.pfnAddCommand ("spectate", NULL, "enable spectator mode" );
	g_engfuncs.pfnAddCommand ("gametitle", NULL, "show game logo" );
	g_engfuncs.pfnAddCommand ("god", NULL, "classic cheat" );
	g_engfuncs.pfnAddCommand ("fov", NULL, "set client field of view" );

	HUD_ShutdownEffects ();

	g_pParticleSystems = new ParticleSystemManager();
	g_pViewRenderBeams = new CViewRenderBeams();
	g_pParticles = new CParticleSystem();
	g_pTempEnts = new CTempEnts();

	InitRain(); // init weather system

	gHUD.Init();

	IN_Init ();

	// link all events
	EV_HookEvents ();
}

int HUD_Redraw( float flTime, int state )
{
	switch( state )
	{
	case CL_ACTIVE:
	case CL_PAUSED:
		gHUD.Redraw( flTime );
		break;
	case CL_LOADING:
		// called while map is loading
		break;
	case CL_CHANGELEVEL:
		// called once when changelevel in-action
		break;
	}
	return 1;
}

void HUD_UpdateEntityVars( edict_t *ent, const entity_state_t *state, const entity_state_t *prev )
{
	float	m_fLerp;

	if( state->ed_type == ED_CLIENT && state->ed_flags & ESF_NO_PREDICTION )
		m_fLerp = 1.0f;
	else m_fLerp = GetLerpFrac();

	// copy state to progs
	ent->v.modelindex = state->modelindex;
	ent->v.weaponmodel = state->weaponmodel;
	ent->v.sequence = state->sequence;
	ent->v.gaitsequence = state->gaitsequence;
	ent->v.body = state->body;
	ent->v.skin = state->skin;
	ent->v.effects = state->effects;
	ent->v.velocity = state->velocity;
	ent->v.basevelocity = state->basevelocity;
	ent->v.oldorigin = ent->v.origin;		// previous origin holds
	ent->v.mins = state->mins;
	ent->v.maxs = state->maxs;
	ent->v.framerate = state->framerate;
	ent->v.colormap = state->colormap; 
	ent->v.rendermode = state->rendermode; 
	ent->v.renderfx = state->renderfx; 
//	ent->v.fov = state->fov; 
	ent->v.scale = state->scale; 
	ent->v.gravity = state->gravity;
	ent->v.solid = state->solid;
	ent->v.movetype = state->movetype;
	ent->v.animtime = state->animtime;

	if( ent != GetLocalPlayer())
	{
		ent->v.health = state->health;
	}

	if( state->aiment )
		ent->v.aiment = GetEntityByIndex( state->aiment );
	else ent->v.aiment = NULL;

	switch( ent->v.movetype )
	{
	case MOVETYPE_NONE:
	case MOVETYPE_STEP:
	case MOVETYPE_FLY:
		// monster's steps will be interpolated on render-side
		ent->v.origin = state->origin;
		ent->v.angles = state->angles;
		ent->v.oldorigin = prev->origin;	// used for lerp 'monster view'
		ent->v.vuser1 = prev->angles;		// used for lerp 'monster view'
		break;
	default:
		ent->v.angles = LerpAngle( prev->angles, state->angles, m_fLerp );
		ent->v.basevelocity = LerpPoint( prev->basevelocity, state->basevelocity, m_fLerp );
		if( ent != GetLocalPlayer( ))
		{
			ent->v.origin = LerpPoint( prev->origin, state->origin, m_fLerp );
		}
		break;
	}

	// interpolate scale, renderamount etc
	ent->v.scale = LerpPoint( prev->scale, state->scale, m_fLerp );
	ent->v.renderamt = LerpPoint( prev->renderamt, state->renderamt, m_fLerp );

	ent->v.rendercolor.x = state->rendercolor.r;
	ent->v.rendercolor.y = state->rendercolor.g;
	ent->v.rendercolor.z = state->rendercolor.b;

	if( ent->v.animtime )
	{
		// use normal studio lerping
		ent->v.frame = state->frame;
	}
	else
	{
		// round sprite and brushmodel frames
		ent->v.frame = Q_rint( state->frame );
	}

	switch( state->ed_type )
	{
	case ED_CLIENT:
		// restore viewangles from angles
		ent->v.v_angle[PITCH] = -ent->v.angles[PITCH] * 3;
		ent->v.v_angle[YAW] = ent->v.angles[YAW];

		if( ent != GetLocalPlayer( ))
		{
			ent->v.origin = LerpPoint( prev->origin, state->origin, m_fLerp );

			if( prev->fov != 90.0f && state->fov == 90.0f )
				ent->v.fov = state->fov; // fov is reset, so don't lerping
			else ent->v.fov = LerpPoint( prev->fov, state->fov, m_fLerp ); 
		}

		if( state->onground != -1 )
			ent->v.groundentity = GetEntityByIndex( state->onground );
		else ent->v.groundentity = NULL;

		ent->v.iStepLeft = state->iStepLeft;
		ent->v.flFallVelocity = state->flFallVelocity;
		break;
	case ED_PORTAL:
	case ED_MOVER:
	case ED_BSPBRUSH:
		ent->v.movedir = BitsToDir( state->body );
		ent->v.oldorigin = state->vuser1;	// used for portals and skyportals
		break;
	case ED_SKYPORTAL:
		{
			skyportal_t *sky = &gpViewParams->skyportal;

			// setup sky portal
			sky->vieworg = ent->v.origin; 
			sky->viewanglesOffset.x = sky->viewanglesOffset.z = 0.0f;
			sky->viewanglesOffset.y = gHUD.m_flTime * ent->v.angles[1];
			sky->scale = (ent->v.scale ? 1.0f / ent->v.scale : 0.0f );	// critical stuff
			sky->fov = ent->v.fov;
		}
		break;
	case ED_BEAM:
		ent->v.origin = state->origin;
		ent->v.angles = state->angles;	// beam endpoint

		// add server beam now
		g_pViewRenderBeams->AddServerBeam( ent );
		break;
	default:
		ent->v.movedir = Vector( 0, 0, 0 );
		break;
	}

	int	i;

	// copy blendings
	for( i = 0; i < 4; i++ )
		ent->v.blending[i] = state->blending[i];

	// copy controllers
	for( i = 0; i < 4; i++ )
		ent->v.controller[i] = state->controller[i];

	// g-cont. moved here because we may needs apply null scale to skyportal
	if( ent->v.scale == 0.0f && ent->v.skin >= 0 ) ent->v.scale = 1.0f;	

	switch( state->ed_type )
	{
	case ED_MOVER:
	case ED_NORMAL:
	case ED_BSPBRUSH:
		if( ent->v.scale >= 100.0f )
			ent->v.scale = 1.0f;	// original HL issues
		break;
	}
	ent->v.pContainingEntity = ent;
}

/*
=========================
HUD_UpdateClientVars

The server sends us our origin with extra precision as part of the clientdata structure, not during the normal
playerstate update in entity_state_t.  In order for these overrides to eventually get to the appropriate playerstate
structure, we need to copy them into the state structure at this point.
=========================
*/
void HUD_UpdateClientVars( edict_t *ent, const clientdata_t *state, const clientdata_t *prev )
{
	float	m_fLerp;

	m_fLerp = GetLerpFrac();

	// copy origin
	ent->v.origin = LerpPoint( prev->origin, state->origin, m_fLerp );
	ent->v.velocity = state->velocity;
	ent->v.flags = state->flags;
	ent->v.health = state->health;

	ent->v.punchangle = LerpAngle( prev->punchangle, state->punchangle, m_fLerp );
	ent->v.view_ofs = LerpPoint( prev->view_ofs, state->view_ofs, m_fLerp );

	if( prev->fov != 90.0f && state->fov == 90.0f )
		ent->v.fov = state->fov; // fov is reset, so don't lerping
	else ent->v.fov = LerpPoint( prev->fov, state->fov, m_fLerp ); 

	// Water state
	ent->v.watertype = state->watertype;
	ent->v.waterlevel = state->waterlevel;
	
	// suit and weapon bits
	ent->v.weapons = state->weapons;

	ent->v.bInDuck = state->bInDuck;
	ent->v.flTimeStepSound = state->flTimeStepSound;
	ent->v.flDuckTime = state->flDuckTime;
	ent->v.flSwimTime = state->flSwimTime;
	ent->v.teleport_time = state->waterjumptime;

	// Viewmodel code
	edict_t	*viewent = GetViewModel();

	// if viewmodel has changed update sequence here
	if( viewent->v.modelindex != state->viewmodel )
	{
//		ALERT( at_console, "Viewmodel changed\n" );
		SendWeaponAnim( viewent->v.sequence, viewent->v.body, viewent->v.framerate );
	}

	// setup player viewmodel (only for local player!)
	viewent->v.modelindex = state->viewmodel;
	gHUD.m_flFOV = ent->v.fov; // keep client fov an actual

	ent->v.maxspeed = state->maxspeed;
	ent->v.weaponanim = state->weaponanim;	// g-cont. hmm. we can grab weaponanim from here ?
	ent->v.pushmsec = state->pushmsec;
	
	// Spectator
	ent->v.iuser1 = state->iuser1;
	ent->v.iuser2 = state->iuser2;

	// Duck prevention
	ent->v.iuser3 = state->iuser3;

	// Fire prevention
	ent->v.iuser4 = state->iuser4;
}

int HUD_AddVisibleEntity( edict_t *pEnt, int ed_type )
{
	float	oldScale, oldRenderAmt;
	float	shellScale = 1.0f;
	int	result;

	if ( pEnt->v.renderfx == kRenderFxGlowShell )
	{
		oldRenderAmt = pEnt->v.renderamt;
		oldScale = pEnt->v.scale;
		
		pEnt->v.renderamt = 255; // clear amount
	}

	result = CL_AddEntity( pEnt, ed_type, -1 );

	if ( pEnt->v.renderfx == kRenderFxGlowShell )
	{
		shellScale = (oldRenderAmt * 0.0015f);	// shellOffset
		pEnt->v.scale = oldScale + shellScale;	// sets new scale
		pEnt->v.renderamt = 128;

		// render glowshell
		result |= CL_AddEntity( pEnt, ed_type, g_pTempEnts->hSprGlowShell );

		// restore parms
		pEnt->v.scale = oldScale;
		pEnt->v.renderamt = oldRenderAmt;
	}	

	if ( pEnt->v.effects & EF_BRIGHTFIELD )
	{
		g_pParticles->EntityParticles( pEnt );
	}

	// add in muzzleflash effect
	if ( pEnt->v.effects & EF_MUZZLEFLASH )
	{
		if( ed_type == ED_VIEWMODEL )
			pEnt->v.effects &= ~EF_MUZZLEFLASH;
		g_pTempEnts->WeaponFlash( pEnt, 1 );
	}

	// add light effect
	if ( pEnt->v.effects & EF_LIGHT )
	{
		g_pTempEnts->AllocDLight( pEnt->v.origin, 100, 100, 100, 200, 0.001f );
		g_pTempEnts->RocketFlare( pEnt->v.origin );
	}

	// add dimlight
	if ( pEnt->v.effects & EF_DIMLIGHT )
	{
		if ( ed_type == ED_CLIENT )
		{
			EV_UpadteFlashlight( pEnt );
		}
		else
		{
			g_pTempEnts->AllocDLight( pEnt->v.origin, RANDOM_LONG( 200, 230 ), 0.001f );
		}
	}	

	// NOTE: Xash3D sent entities to client even if it has EF_NODRAW flags
	// run simple check here to prevent lighting invisible entity
	if ( pEnt->v.effects & EF_BRIGHTLIGHT && !( pEnt->v.effects & EF_NODRAW ))
	{			
		Vector pos( pEnt->v.origin.x, pEnt->v.origin.y, pEnt->v.origin.z + 16 );
		g_pTempEnts->AllocDLight( pos, RANDOM_LONG( 400, 430 ), 0.001f );
	}

	return result;
}

void HUD_CreateEntities( void )
{
	EV_UpdateBeams ();		// egon use this
	EV_UpdateLaserSpot ();	// predictable laserspot

	// add in any game specific objects here
	g_pViewRenderBeams->UpdateTempEntBeams( );

	g_pTempEnts->Update();
}

void HUD_UpdateOnRemove( edict_t *pEdict )
{
	// move TE_BEAMTRAIL, kill other beams
	g_pViewRenderBeams->KillDeadBeams( pEdict );
}

void HUD_Reset( void )
{
	HUD_VidInit ();
}

void HUD_StartFrame( void )
{
	// clear list of server beams after each frame
	g_pViewRenderBeams->ClearServerBeams( );
}

void HUD_ParticleEffect( const float *org, const float *dir, int color, int count )
{
	g_pParticles->ParticleEffect( org, dir, color, count );
}

void HUD_Frame( double time )
{
	// place to call vgui_frame
	// VGUI not implemented, wait for version 0.75
}

void HUD_Shutdown( void )
{
	HUD_ShutdownEffects ();

	IN_Shutdown ();

	// perform shutdown operations
	g_engfuncs.pfnDelCommand ("noclip" );
	g_engfuncs.pfnDelCommand ("notarget" );
	g_engfuncs.pfnDelCommand ("fullupdate" );
	g_engfuncs.pfnDelCommand ("give" );
	g_engfuncs.pfnDelCommand ("drop" );
	g_engfuncs.pfnDelCommand ("intermission" );
	g_engfuncs.pfnDelCommand ("spectate" );
	g_engfuncs.pfnDelCommand ("gametitle" );
	g_engfuncs.pfnDelCommand ("god" );
	g_engfuncs.pfnDelCommand ("fov" );
}
