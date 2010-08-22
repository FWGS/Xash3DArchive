//=======================================================================
//			Copyright XashXT Group 2010 �
//		    studio.cpp - client side studio callbacks
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "studio.h"
#include "r_efx.h"
#include "pm_movevars.h"
#include "r_tempents.h"
#include "ev_hldm.h"
#include "r_beams.h"
#include "hud.h"

//======================
//	DRAW BEAM EVENT
//======================
void EV_DrawBeam ( void )
{
	// special effect for displacer
	cl_entity_t *view = GetViewModel();

	float life = 1.05;	// animtime
	int m_iBeam = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/plasma.spr" );
	int idx = GetLocalPlayer()->index; // link with client

	g_pViewRenderBeams->CreateBeamEnts( idx | 0x1000, idx | 0x2000, m_iBeam, life, 0.8, 0.5, 127, 0.6, 0, 10, 20, 100, 0 );
	g_pViewRenderBeams->CreateBeamEnts( idx | 0x1000, idx | 0x3000, m_iBeam, life, 0.8, 0.5, 127, 0.6, 0, 10, 20, 100, 0 );
	g_pViewRenderBeams->CreateBeamEnts( idx | 0x1000, idx | 0x4000, m_iBeam, life, 0.8, 0.5, 127, 0.6, 0, 10, 20, 100, 0 );
}

//======================
//	Eject Shell
//======================
void EV_EjectShell( const mstudioevent_t *event, cl_entity_t *entity )
{
	vec3_t view_ofs, ShellOrigin, ShellVelocity, forward, right, up;
	vec3_t origin = entity->origin;
	vec3_t angles = entity->angles;
	vec3_t velocity = entity->curstate.velocity;	// needs to change delta.lst to get it work
	
	float fR, fU;

          int shell = gEngfuncs.pEventAPI->EV_FindModelIndex( event->options );
	origin.z = origin.z - entity->curstate.usehull ? 12 : 28;

	for( int j = 0; j < 3; j++ )
	{
		if( angles[j] < -180 )
			angles[j] += 360; 
		else if( angles[j] > 180 )
			angles[j] -= 360;
          }

	angles.x = -angles.x;
	AngleVectors( angles, forward, right, up );
          
	fR = RANDOM_FLOAT( 50, 70 );
	fU = RANDOM_FLOAT( 100, 150 );

	for ( int i = 0; i < 3; i++ )
	{
		ShellVelocity[i] = velocity[i] + right[i] * fR + up[i] * fU + forward[i] * 25;
		ShellOrigin[i]   = origin[i] + view_ofs[i] + up[i] * -12 + forward[i] * 20 + right[i] * 4;
	}
	EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL );
}

void HUD_StudioEvent( const mstudioevent_t *event, cl_entity_t *entity )
{
	int	pitch;
	float	fvol;
	Vector	pos;

	switch( event->event )
	{
	case 5001:
		// MullzeFlash at attachment 1
		g_pTempEnts->MuzzleFlash( entity, 1, atoi( event->options ));
		break;
	case 5011:
		// MullzeFlash at attachment 2
		g_pTempEnts->MuzzleFlash( entity, 2, atoi( event->options ));
		break;
	case 5021:
		// MullzeFlash at attachment 3
		g_pTempEnts->MuzzleFlash( entity, 3, atoi( event->options ));
		break;
	case 5031:
		// MullzeFlash at attachment 4
		g_pTempEnts->MuzzleFlash( entity, 4, atoi( event->options ));
		break;
	case 5002:
		// SparkEffect at attachment 1
		pos = entity->curstate.origin + entity->attachment_origin[0];
		g_pTempEnts->SparkEffect( pos, 8, -200, 200 );
		break;
	case 5004:		
		// Client side sound
		pos = entity->origin + entity->attachment_origin[0];
		gEngfuncs.pEventAPI->EV_PlaySound( 0, pos, CHAN_AUTO, event->options, VOL_NORM, ATTN_NORM, 0, PITCH_NORM );
		break;
	case 5005:		
		// Client side sound with random pitch (most useful for reload sounds)
		pitch = 85 + RANDOM_LONG( 0, 0x1F );
		pos = entity->origin + entity->attachment_origin[0];
		fvol = RANDOM_FLOAT( 0.7f, 0.9f );
		gEngfuncs.pEventAPI->EV_PlaySound( 0, pos, CHAN_AUTO, event->options, fvol, ATTN_NORM, 0, pitch );
		break;
	case 5050:
		// Special event for displacer ( Xash 0.1, XDM 3.3 )
		EV_DrawBeam ();
		break;
	case 5060:
		EV_EjectShell( event, entity );
		break;
	default:
		Con_Printf( "Unhandled client-side attachment %i ( %s )\n", event->event, event->options );
		break;
	}
}

void HUD_StudioFxTransform( cl_entity_t *ent, float transform[4][4] )
{
	switch( ent->curstate.renderfx )
	{
	case kRenderFxDistort:
	case kRenderFxHologram:
		if(!RANDOM_LONG( 0, 49 ))
		{
			int	axis = RANDOM_LONG( 0, 1 );
			float	scale = RANDOM_FLOAT( 1, 1.484 );

			if( axis == 1 ) axis = 2; // choose between x & z
			transform[axis][0] *= scale;
			transform[axis][1] *= scale;
			transform[axis][2] *= scale;
		}
		else if(!RANDOM_LONG( 0, 49 ))
		{
			float offset;
			int axis = RANDOM_LONG( 0, 1 );
			if( axis == 1 ) axis = 2; // choose between x & z
			offset = RANDOM_FLOAT( -10, 10 );
			transform[RANDOM_LONG( 0, 2 )][3] += offset;
		}
		break;
	case kRenderFxExplode:
		float	scale;

		scale = 1.0f + (gHUD.m_flTime - ent->curstate.animtime) * 10.0;
		if( scale > 2 ) scale = 2; // don't blow up more than 200%
		
		transform[0][1] *= scale;
		transform[1][1] *= scale;
		transform[2][1] *= scale;
		break;
	}
}

int StudioBodyVariations( int modelIndex )
{
	studiohdr_t	*pstudiohdr;
	mstudiobodyparts_t	*pbodypart;
	int		i, count;

	pstudiohdr = (studiohdr_t *)Mod_Extradata( modelIndex );
	if( !pstudiohdr ) return 0;

	count = 1;
	pbodypart = (mstudiobodyparts_t *)((byte *)pstudiohdr + pstudiohdr->bodypartindex);

	// each body part has nummodels variations so there are as many total variations as there
	// are in a matrix of each part by each other part
	for( i = 0; i < pstudiohdr->numbodyparts; i++ )
	{
		count = count * pbodypart[i].nummodels;
	}
	return count;
}

// an example how to renderer determines interpolation methods
int HUD_StudioDoInterp( cl_entity_t *e )
{
	if( r_studio_lerping->integer )
	{
		return (e->curstate.effects & EF_NOINTERP) ? false : true;
	}
	return false;
}