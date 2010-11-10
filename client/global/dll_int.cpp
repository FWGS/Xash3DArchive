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
#include "r_efx.h"
#include "r_particle.h"
#include "r_tempents.h"
#include "r_beams.h"
#include "ev_hldm.h"
#include "r_weather.h"
#include "pm_shared.h"
#include "pm_movevars.h"
#include "entity_types.h"

cl_enginefunc_t	gEngfuncs;
int		g_iPlayerClass;
int		g_iTeamNumber;
int		g_iUser1;
int		g_iUser2;
int		g_iUser3;
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
	HUD_GetHullBounds,
	HUD_TxferLocalOverrides,
	HUD_ProcessPlayerState,
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
	HUD_ParticleEffect,
	HUD_TempEntityMessage,
	HUD_DirectorMessage,
};

//=======================================================================
//			GetApi
//=======================================================================
int CreateAPI( HUD_FUNCTIONS *pFunctionTable, cl_enginefunc_t* pEngfuncsFromEngine )
{
	if( !pFunctionTable || !pEngfuncsFromEngine )
	{
		return FALSE;
	}

	// copy HUD_FUNCTIONS table to engine, copy engfuncs table from engine
	memcpy( pFunctionTable, &gFunctionTable, sizeof( HUD_FUNCTIONS ));
	memcpy( &gEngfuncs, pEngfuncsFromEngine, sizeof( cl_enginefunc_t ));

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
================================
HUD_GetHullBounds

Engine calls this to enumerate player collision hulls, for prediction.
Return 0 if the hullnumber doesn't exist.
================================
*/
int HUD_GetHullBounds( int hullnumber, float *mins, float *maxs )
{
	int iret = 0;

	switch( hullnumber )
	{
	case 0:	// Normal player
		VEC_HULL_MIN.CopyToArray( mins );
		VEC_HULL_MAX.CopyToArray( maxs );
		iret = 1;
		break;
	case 1:	// Crouched player
		VEC_DUCK_HULL_MIN.CopyToArray( mins );
		VEC_DUCK_HULL_MAX.CopyToArray( maxs );
		iret = 1;
		break;
	case 2:	// Point based hull
		g_vecZero.CopyToArray( mins );
		g_vecZero.CopyToArray( maxs );
		iret = 1;
		break;
	}
	return iret;
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

int HUD_Redraw( float flTime, int intermission )
{
	gHUD.Redraw( flTime );

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

	// FIXME: temporary hack so gaitsequence it's works properly
	state->velocity = client->velocity;
}

/*
=========================
HUD_ProcessPlayerState

We have received entity_state_t for this player over the network.  We need to copy appropriate fields to the
playerstate structure
=========================
*/
void HUD_ProcessPlayerState( struct entity_state_s *dst, const struct entity_state_s *src )
{
	// Copy in network data
	dst->origin = src->origin;
	dst->angles = src->angles;

	dst->velocity = src->velocity;

	dst->frame	= src->frame;
	dst->modelindex	= src->modelindex;
	dst->skin	     	= src->skin;
	dst->effects   	= src->effects;
	dst->weaponmodel	= src->weaponmodel;
	dst->movetype   	= src->movetype;
	dst->sequence   	= src->sequence;
	dst->animtime   	= src->animtime;
	
	dst->solid      	= src->solid;
	
	dst->rendermode	= src->rendermode;
	dst->renderamt	= src->renderamt;	
	dst->rendercolor.r	= src->rendercolor.r;
	dst->rendercolor.g	= src->rendercolor.g;
	dst->rendercolor.b	= src->rendercolor.b;
	dst->renderfx	= src->renderfx;

	dst->framerate	= src->framerate;
	dst->body		= src->body;

	memcpy( &dst->controller[0], &src->controller[0], 4 * sizeof( byte ) );
	memcpy( &dst->blending[0], &src->blending[0], 2 * sizeof( byte ) );

	dst->basevelocity = src->basevelocity;

	dst->friction	= src->friction;
	dst->gravity	= src->gravity;
	dst->gaitsequence	= src->gaitsequence;
	dst->spectator	= src->spectator;
	dst->usehull	= src->usehull;
	dst->playerclass	= src->playerclass;
	dst->team		= src->team;
	dst->colormap	= src->colormap;

	// Save off some data so other areas of the Client DLL can get to it
	cl_entity_t *player = GetLocalPlayer();	// Get the local player's index
	if ( dst->number == player->index )
	{
		g_iPlayerClass = dst->playerclass;
		g_iTeamNumber = dst->team;

		g_iUser1 = src->iuser1;
		g_iUser2 = src->iuser2;
		g_iUser3 = src->iuser3;
	}
}

int HUD_AddVisibleEntity( cl_entity_t *pEnt, int entityType )
{
	int	result;

	result = gEngfuncs.CL_CreateVisibleEntity( entityType, pEnt );

	if ( pEnt->curstate.effects & EF_BRIGHTFIELD )
	{
		g_pParticles->EntityParticles( pEnt );
	}

	// add in muzzleflash effect
	if ( pEnt->curstate.effects & EF_MUZZLEFLASH )
	{
		if( entityType == ET_VIEWENTITY )
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
		if ( entityType == ET_PLAYER )
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

void HUD_TempEntityMessage( int iSize, void *pbuf )
{
	BEGIN_READ( "TempEntity", iSize, pbuf );
	HUD_ParseTempEntity();
	END_READ();
}

void HUD_DirectorMessage( int iSize, void *pbuf )
{
	// FIXME: implement
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
}