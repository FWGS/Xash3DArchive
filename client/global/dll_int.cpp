//=======================================================================
//			Copyright XashXT Group 2008 �
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
	HUD_UpdateClientData,
	HUD_TxferLocalOverrides,
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

/*
==========================
	HUD_UpdateClientData

called every time shared client
dll/engine data gets changed,
and gives the cdll a chance
to modify the data.

returns 1 if anything has been changed, 0 otherwise.
==========================
*/

int HUD_UpdateClientData( client_data_t *pcldata, float flTime )
{
	return gHUD.UpdateClientData( pcldata, flTime );
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

/*
=========================
HUD_UpdateClientVars

The server sends us our origin with extra precision as part of the clientdata structure, not during the normal
playerstate update in entity_state_t.  In order for these overrides to eventually get to the appropriate playerstate
structure, we need to copy them into the state structure at this point.
=========================
*/
void HUD_TxferLocalOverrides( entity_state_t *state, const clientdata_t *client )
{
	state->origin = client->origin;

	// Spectator
	state->iuser1 = client->iuser1;
	state->iuser2 = client->iuser2;

	// Duck prevention
	state->iuser3 = client->iuser3;

	// Fire prevention
	state->iuser4 = client->iuser4;

	// FIXME: move this stuff into TxferPredictionData when it will be done

	// Spectating or not dead == get control over view angles.
	g_iAlive = ( client->iuser1 || ( client->deadflag == DEAD_NO ) ) ? 1 : 0;
}

int HUD_AddVisibleEntity( cl_entity_t *pEnt, int ed_type )
{
	float	oldScale, oldRenderAmt;
	float	shellScale = 1.0f;
	int	result;

	if ( pEnt->curstate.renderfx == kRenderFxGlowShell )
	{
		oldRenderAmt = pEnt->curstate.renderamt;
		oldScale = pEnt->curstate.scale;
		
		pEnt->curstate.renderamt = 255; // clear amount
	}

	result = CL_AddEntity( pEnt, ed_type, -1 );

	if ( pEnt->curstate.renderfx == kRenderFxGlowShell )
	{
		shellScale = (oldRenderAmt * 0.0015f);	// shellOffset
		pEnt->curstate.scale = oldScale + shellScale;	// sets new scale
		pEnt->curstate.renderamt = 128;

		// render glowshell
		result |= CL_AddEntity( pEnt, ed_type, g_pTempEnts->hSprGlowShell );

		// restore parms
		pEnt->curstate.scale = oldScale;
		pEnt->curstate.renderamt = oldRenderAmt;
	}	

	if ( pEnt->curstate.effects & EF_BRIGHTFIELD )
	{
		g_pParticles->EntityParticles( pEnt );
	}

	// add in muzzleflash effect
	if ( pEnt->curstate.effects & EF_MUZZLEFLASH )
	{
		if( ed_type == ED_VIEWMODEL )
			pEnt->curstate.effects &= ~EF_MUZZLEFLASH;
		g_pTempEnts->WeaponFlash( pEnt, 1 );
	}

	// add light effect
	if ( pEnt->curstate.effects & EF_LIGHT )
	{
		g_pTempEnts->AllocDLight( pEnt->curstate.origin, 100, 100, 100, 200, 0.001f );
		g_pTempEnts->RocketFlare( pEnt->curstate.origin );
	}

	// add dimlight
	if ( pEnt->curstate.effects & EF_DIMLIGHT )
	{
		if ( ed_type == ED_CLIENT )
		{
			EV_UpadteFlashlight( pEnt );
		}
		else
		{
			g_pTempEnts->AllocDLight( pEnt->curstate.origin, RANDOM_LONG( 200, 230 ), 0.001f );
		}
	}	

	// NOTE: Xash3D sent entities to client even if it has EF_NODRAW flags
	// run simple check here to prevent lighting invisible entity
	if ( pEnt->curstate.effects & EF_BRIGHTLIGHT && !( pEnt->curstate.effects & EF_NODRAW ))
	{			
		Vector pos( pEnt->curstate.origin.x, pEnt->curstate.origin.y, pEnt->curstate.origin.z + 16 );
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

void HUD_UpdateOnRemove( cl_entity_t *pEdict )
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
