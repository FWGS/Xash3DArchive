/*
cl_tent.c - temp entity effects management
Copyright (C) 2009 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"
#include "client.h"
#include "r_efx.h"
#include "entity_types.h"
#include "triangleapi.h"
#include "cl_tent.h"
#include "pm_local.h"
#include "gl_local.h"
#include "studio.h"
#include "wadfile.h"	// acess decal size
#include "sound.h"

/*
==============================================================

TEMPENTS MANAGEMENT

==============================================================
*/
#define FLASHLIGHT_DISTANCE		2000	// in units
#define SHARD_VOLUME		12.0f	// on shard ever n^3 units
#define MAX_MUZZLEFLASH		3
#define SF_FUNNEL_REVERSE		1

TEMPENTITY	*cl_active_tents;
TEMPENTITY	*cl_free_tents;
TEMPENTITY	*cl_tempents = NULL;		// entities pool
int		cl_muzzleflash[MAX_MUZZLEFLASH];	// muzzle flashes

/*
================
CL_RegisterMuzzleFlashes

================
*/
void CL_RegisterMuzzleFlashes( void )
{
	// update muzzleflash indexes
	cl_muzzleflash[0] = CL_FindModelIndex( "sprites/muzzleflash1.spr" );
	cl_muzzleflash[1] = CL_FindModelIndex( "sprites/muzzleflash2.spr" );
	cl_muzzleflash[2] = CL_FindModelIndex( "sprites/muzzleflash3.spr" );

	// update registration for shellchrome
	cls.hChromeSprite = pfnSPR_Load( "sprites/shellchrome.spr" );
}

/*
===============
CL_FxBlend
===============
*/
int CL_FxBlend( cl_entity_t *e )
{
	int	blend = 0;
	float	offset, dist;
	vec3_t	tmp;

	offset = ((int)e->index ) * 363.0f; // Use ent index to de-sync these fx

	switch( e->curstate.renderfx ) 
	{
	case kRenderFxPulseSlowWide:
		blend = e->curstate.renderamt + 0x40 * sin( cl.time * 2 + offset );	
		break;
	case kRenderFxPulseFastWide:
		blend = e->curstate.renderamt + 0x40 * sin( cl.time * 8 + offset );
		break;
	case kRenderFxPulseSlow:
		blend = e->curstate.renderamt + 0x10 * sin( cl.time * 2 + offset );
		break;
	case kRenderFxPulseFast:
		blend = e->curstate.renderamt + 0x10 * sin( cl.time * 8 + offset );
		break;
	// JAY: HACK for now -- not time based
	case kRenderFxFadeSlow:			
		if( e->curstate.renderamt > 0 ) 
			e->curstate.renderamt -= 1;
		else e->curstate.renderamt = 0;
		blend = e->curstate.renderamt;
		break;
	case kRenderFxFadeFast:
		if( e->curstate.renderamt > 3 ) 
			e->curstate.renderamt -= 4;
		else e->curstate.renderamt = 0;
		blend = e->curstate.renderamt;
		break;
	case kRenderFxSolidSlow:
		if( e->curstate.renderamt < 255 ) 
			e->curstate.renderamt += 1;
		else e->curstate.renderamt = 255;
		blend = e->curstate.renderamt;
		break;
	case kRenderFxSolidFast:
		if( e->curstate.renderamt < 252 ) 
			e->curstate.renderamt += 4;
		else e->curstate.renderamt = 255;
		blend = e->curstate.renderamt;
		break;
	case kRenderFxStrobeSlow:
		blend = 20 * sin( cl.time * 4 + offset );
		if( blend < 0 ) blend = 0;
		else blend = e->curstate.renderamt;
		break;
	case kRenderFxStrobeFast:
		blend = 20 * sin( cl.time * 16 + offset );
		if( blend < 0 ) blend = 0;
		else blend = e->curstate.renderamt;
		break;
	case kRenderFxStrobeFaster:
		blend = 20 * sin( cl.time * 36 + offset );
		if( blend < 0 ) blend = 0;
		else blend = e->curstate.renderamt;
		break;
	case kRenderFxFlickerSlow:
		blend = 20 * (sin( cl.time * 2 ) + sin( cl.time * 17 + offset ));
		if( blend < 0 ) blend = 0;
		else blend = e->curstate.renderamt;
		break;
	case kRenderFxFlickerFast:
		blend = 20 * (sin( cl.time * 16 ) + sin( cl.time * 23 + offset ));
		if( blend < 0 ) blend = 0;
		else blend = e->curstate.renderamt;
		break;
	case kRenderFxHologram:
	case kRenderFxDistort:
		VectorCopy( e->origin, tmp );
		VectorSubtract( tmp, RI.vieworg, tmp );
		dist = DotProduct( tmp, RI.vforward );
			
		// turn off distance fade
		if( e->curstate.renderfx == kRenderFxDistort )
			dist = 1;

		if( dist <= 0 )
		{
			blend = 0;
		}
		else 
		{
			e->curstate.renderamt = 180;
			if( dist <= 100 ) blend = e->curstate.renderamt;
			else blend = (int) ((1.0f - ( dist - 100 ) * ( 1.0f / 400.0f )) * e->curstate.renderamt );
			blend += COM_RandomLong( -32, 31 );
		}
		break;
	default:
		blend = e->curstate.renderamt;
		break;
	}

	blend = bound( 0, blend, 255 );

	return blend;
}

/*
================
CL_InitTempents

================
*/
void CL_InitTempEnts( void )
{
	cl_tempents = Mem_Alloc( cls.mempool, sizeof( TEMPENTITY ) * GI->max_tents );
	CL_ClearTempEnts();
}

/*
================
CL_ClearTempEnts

================
*/
void CL_ClearTempEnts( void )
{
	int	i;

	if( !cl_tempents ) return;

	for( i = 0; i < GI->max_tents - 1; i++ )
	{
		cl_tempents[i].next = &cl_tempents[i+1];
		cl_tempents[i].entity.trivial_accept = INVALID_HANDLE;
          }

	cl_tempents[GI->max_tents-1].next = NULL;
	cl_free_tents = cl_tempents;
	cl_active_tents = NULL;
}

/*
================
CL_FreeTempEnts

================
*/
void CL_FreeTempEnts( void )
{
	if( cl_tempents )
		Mem_Free( cl_tempents );
	cl_tempents = NULL;
}

/*
==============
CL_PrepareTEnt

set default values
==============
*/
void CL_PrepareTEnt( TEMPENTITY *pTemp, model_t *pmodel )
{
	int	frameCount = 0;
	int	modelIndex = 0;
	int	modelHandle = pTemp->entity.trivial_accept;

	memset( pTemp, 0, sizeof( *pTemp ));

	// use these to set per-frame and termination conditions / actions
	pTemp->entity.trivial_accept = modelHandle; // keep unchanged
	pTemp->flags = FTENT_NONE;		
	pTemp->die = cl.time + 0.75f;

	if( pmodel )
	{
		modelIndex = CL_FindModelIndex( pmodel->name );
		Mod_GetFrames( modelIndex, &frameCount );
	}
	else
	{
		pTemp->flags |= FTENT_NOMODEL;
	}

	pTemp->entity.curstate.modelindex = modelIndex;
	pTemp->entity.curstate.rendermode = kRenderNormal;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.curstate.rendercolor.r = 255;
	pTemp->entity.curstate.rendercolor.g = 255;
	pTemp->entity.curstate.rendercolor.b = 255;
	pTemp->frameMax = max( 0, frameCount - 1 );
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.body = 0;
	pTemp->entity.curstate.skin = 0;
	pTemp->entity.model = pmodel;
	pTemp->fadeSpeed = 0.5f;
	pTemp->hitSound = 0;
	pTemp->clientIndex = 0;
	pTemp->bounceFactor = 1;
	pTemp->entity.curstate.scale = 1.0f;
}

/*
==============
CL_TempEntPlaySound

play collide sound
==============
*/
void CL_TempEntPlaySound( TEMPENTITY *pTemp, float damp )
{
	float	fvol;
	char	soundname[32];
	qboolean	isshellcasing = false;
	int	zvel;

	ASSERT( pTemp != NULL );

	switch( pTemp->hitSound )
	{
	case BOUNCE_GLASS:
		Q_snprintf( soundname, sizeof( soundname ), "debris/glass%i.wav", COM_RandomLong( 1, 4 ));
		break;
	case BOUNCE_METAL:
		Q_snprintf( soundname, sizeof( soundname ), "debris/metal%i.wav", COM_RandomLong( 1, 6 ));
		break;
	case BOUNCE_FLESH:
		Q_snprintf( soundname, sizeof( soundname ), "debris/flesh%i.wav", COM_RandomLong( 1, 7 ));
		break;
	case BOUNCE_WOOD:
		Q_snprintf( soundname, sizeof( soundname ), "debris/wood%i.wav", COM_RandomLong( 1, 4 ));
		break;
	case BOUNCE_SHRAP:
		Q_snprintf( soundname, sizeof( soundname ), "weapons/ric%i.wav", COM_RandomLong( 1, 5 ));
		break;
	case BOUNCE_SHOTSHELL:
		Q_snprintf( soundname, sizeof( soundname ), "weapons/sshell%i.wav", COM_RandomLong( 1, 3 ));
		isshellcasing = true; // shell casings have different playback parameters
		break;
	case BOUNCE_SHELL:
		Q_snprintf( soundname, sizeof( soundname ), "player/pl_shell%i.wav", COM_RandomLong( 1, 3 ));
		isshellcasing = true; // shell casings have different playback parameters
		break;
	case BOUNCE_CONCRETE:
		Q_snprintf( soundname, sizeof( soundname ), "debris/concrete%i.wav", COM_RandomLong( 1, 3 ));
		break;
	default:	// null sound
		return;
	}

	zvel = abs( pTemp->entity.baseline.origin[2] );
		
	// only play one out of every n
	if( isshellcasing )
	{	
		// play first bounce, then 1 out of 3		
		if( zvel < 200 && COM_RandomLong( 0, 3 ))
			return;
	}
	else
	{
		if( COM_RandomLong( 0, 5 )) 
			return;
	}

	fvol = 1.0f;

	if( damp > 0.0f )
	{
		int	pitch;
		sound_t	handle;
		
		if( isshellcasing )
			fvol *= min ( 1.0f, ((float)zvel) / 350.0f ); 
		else fvol *= min ( 1.0f, ((float)zvel) / 450.0f ); 
		
		if( !COM_RandomLong( 0, 3 ) && !isshellcasing )
			pitch = COM_RandomLong( 95, 105 );
		else pitch = PITCH_NORM;

		handle = S_RegisterSound( soundname );
		S_StartSound( pTemp->entity.origin, -(pTemp - cl_tempents), CHAN_BODY, handle, fvol, ATTN_NORM, pitch, SND_STOP_LOOPING );
	}
}

/*
==============
CL_TEntAddEntity

add entity to renderlist
==============
*/
int CL_TempEntAddEntity( cl_entity_t *pEntity )
{
	vec3_t mins, maxs;

	ASSERT( pEntity != NULL );

	if( !pEntity->model )
		return 0;

	VectorAdd( pEntity->origin, pEntity->model->mins, mins );
	VectorAdd( pEntity->origin, pEntity->model->maxs, maxs );

	// g-cont. just use PVS from previous frame
	if( TriBoxInPVS( mins, maxs ))
	{
		VectorCopy( pEntity->angles, pEntity->curstate.angles );
		VectorCopy( pEntity->origin, pEntity->curstate.origin );
		VectorCopy( pEntity->angles, pEntity->latched.prevangles );
		VectorCopy( pEntity->origin, pEntity->latched.prevorigin );
	
		// add to list
		if( CL_AddVisibleEntity( pEntity, ET_TEMPENTITY ))
			r_stats.c_active_tents_count++;

		return 1;
	}

	return 0;
}

/*
==============
CL_AddTempEnts

temp-entities will be added on a user-side
setup client callback
==============
*/
void CL_TempEntUpdate( void )
{
	double	ft = cl.time - cl.oldtime;
	float	gravity = clgame.movevars.gravity;

	clgame.dllFuncs.pfnTempEntUpdate( ft, cl.time, gravity, &cl_free_tents, &cl_active_tents, CL_TempEntAddEntity, CL_TempEntPlaySound );
}

/*
==============
CL_TEntAddEntity

free the first low priority tempent it finds.
==============
*/
qboolean CL_FreeLowPriorityTempEnt( void )
{
	TEMPENTITY	*pActive = cl_active_tents;
	TEMPENTITY	*pPrev = NULL;

	while( pActive )
	{
		if( pActive->priority == TENTPRIORITY_LOW )
		{
			// remove from the active list.
			if( pPrev ) pPrev->next = pActive->next;
			else cl_active_tents = pActive->next;

			// add to the free list.
			pActive->next = cl_free_tents;
			cl_free_tents = pActive;

			return true;
		}

		pPrev = pActive;
		pActive = pActive->next;
	}

	return false;
}

/*
==============
CL_TempEntAlloc

alloc normal\low priority tempentity
==============
*/
TEMPENTITY *CL_TempEntAlloc( const vec3_t org, model_t *pmodel )
{
	TEMPENTITY	*pTemp;

	if( !cl_free_tents )
	{
		MsgDev( D_INFO, "Overflow %d temporary ents!\n", GI->max_tents );
		return NULL;
	}

	pTemp = cl_free_tents;
	cl_free_tents = pTemp->next;

	CL_PrepareTEnt( pTemp, pmodel );

	pTemp->priority = TENTPRIORITY_LOW;
	if( org ) VectorCopy( org, pTemp->entity.origin );

	pTemp->next = cl_active_tents;
	cl_active_tents = pTemp;

	return pTemp;
}

/*
==============
CL_TempEntAllocHigh

alloc high priority tempentity
==============
*/
TEMPENTITY *CL_TempEntAllocHigh( const vec3_t org, model_t *pmodel )
{
	TEMPENTITY	*pTemp;

	if( !cl_free_tents )
	{
		// no temporary ents free, so find the first active low-priority temp ent 
		// and overwrite it.
		CL_FreeLowPriorityTempEnt();
	}

	if( !cl_free_tents )
	{
		// didn't find anything? The tent list is either full of high-priority tents
		// or all tents in the list are still due to live for > 10 seconds. 
		MsgDev( D_INFO, "Couldn't alloc a high priority TENT!\n" );
		return NULL;
	}

	// Move out of the free list and into the active list.
	pTemp = cl_free_tents;
	cl_free_tents = pTemp->next;

	CL_PrepareTEnt( pTemp, pmodel );

	pTemp->priority = TENTPRIORITY_HIGH;
	if( org ) VectorCopy( org, pTemp->entity.origin );

	pTemp->next = cl_active_tents;
	cl_active_tents = pTemp;

	return pTemp;
}

/*
==============
CL_TempEntAlloc

alloc normal priority tempentity with no model
==============
*/
TEMPENTITY *CL_TempEntAllocNoModel( const vec3_t org )
{
	return CL_TempEntAlloc( org, NULL );
}

/*
==============
CL_TempEntAlloc

custom tempentity allocation
==============
*/                                                   
TEMPENTITY *CL_TempEntAllocCustom( const vec3_t org, model_t *model, int high, void (*pfn)( TEMPENTITY*, float, float ))
{
	TEMPENTITY	*pTemp;

	if( high )
	{
		pTemp = CL_TempEntAllocHigh( org, model );
	}
	else
	{
		pTemp = CL_TempEntAlloc( org, model );
	}

	if( pTemp && pfn )
	{
		pTemp->flags |= FTENT_CLIENTCUSTOM;
		pTemp->callback = pfn;
	}

	return pTemp;
}

/*
==============================================================

	EFFECTS BASED ON TEMPENTS (presets)

==============================================================
*/
/*
==============
CL_FizzEffect

Create a fizz effect
==============
*/
void CL_FizzEffect( cl_entity_t *pent, int modelIndex, int density )
{
	TEMPENTITY	*pTemp;
	int		i, width, depth, count, frameCount;
	float		angle, maxHeight, speed;
	float		xspeed, yspeed, zspeed;
	vec3_t		origin, mins, maxs;

	if( !pent || Mod_GetType( modelIndex ) == mod_bad )
		return;

	if( pent->curstate.modelindex <= 0 )
		return;

	count = density + 1;
	density = count * 3 + 6;

	Mod_GetBounds( pent->curstate.modelindex, mins, maxs );

	maxHeight = maxs[2] - mins[2];
	width = maxs[0] - mins[0];
	depth = maxs[1] - mins[1];
	speed = ( pent->curstate.rendercolor.r<<8 | pent->curstate.rendercolor.g );
	if( pent->curstate.rendercolor.b ) speed = -speed;
	if( speed == 0.0f ) speed = 100.0f;	// apply default value

	if( pent->angles[YAW] != 0.0f )
	{
		angle = pent->angles[YAW] * M_PI / 180;
		yspeed = sin( angle );
		xspeed = cos( angle );

		xspeed *= speed;
		yspeed *= speed;
	}
	else xspeed = yspeed = 0.0f;	// z only

	Mod_GetFrames( modelIndex, &frameCount );

	for( i = 0; i < count; i++ )
	{
		origin[0] = mins[0] + COM_RandomLong( 0, width - 1 );
		origin[1] = mins[1] + COM_RandomLong( 0, depth - 1 );
		origin[2] = mins[2];
		pTemp = CL_TempEntAlloc( origin, Mod_Handle( modelIndex ));

		if ( !pTemp ) return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];

		zspeed = COM_RandomLong( 80, 140 );
		VectorSet( pTemp->entity.baseline.origin, xspeed, yspeed, zspeed );
		pTemp->die = cl.time + ( maxHeight / zspeed ) - 0.1f;
		pTemp->entity.curstate.frame = COM_RandomLong( 0, frameCount - 1 );
		// Set sprite scale
		pTemp->entity.curstate.scale = 1.0f / COM_RandomFloat( 2.0f, 5.0f );
		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 255;
	}
}

/*
==============
CL_Bubbles

Create bubbles
==============
*/
void CL_Bubbles( const vec3_t mins, const vec3_t maxs, float height, int modelIndex, int count, float speed )
{
	TEMPENTITY	*pTemp;
	int		i, frameCount;
	float		sine, cosine, zspeed;
	float		angle;
	vec3_t		origin;

	if( Mod_GetType( modelIndex ) == mod_bad )
		return;

	Mod_GetFrames( modelIndex, &frameCount );

	for ( i = 0; i < count; i++ )
	{
		origin[0] = COM_RandomLong( mins[0], maxs[0] );
		origin[1] = COM_RandomLong( mins[1], maxs[1] );
		origin[2] = COM_RandomLong( mins[2], maxs[2] );
		pTemp = CL_TempEntAlloc( origin, Mod_Handle( modelIndex ));
		if( !pTemp ) return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];
		angle = COM_RandomFloat( -M_PI, M_PI );
		SinCos( angle, &sine, &cosine );
		
		zspeed = COM_RandomLong( 80, 140 );
		VectorSet( pTemp->entity.baseline.origin, speed * cosine, speed * sine, zspeed );
		pTemp->die = cl.time + ((height - (origin[2] - mins[2])) / zspeed) - 0.1f;
		pTemp->entity.curstate.frame = COM_RandomLong( 0, frameCount - 1 );
		
		// Set sprite scale
		pTemp->entity.curstate.scale = 1.0f / COM_RandomFloat( 4.0f, 16.0f );
		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 192; // g-cont. why difference with FizzEffect ???		
	}
}

/*
==============
CL_BubbleTrail

Create bubble trail
==============
*/
void CL_BubbleTrail( const vec3_t start, const vec3_t end, float flWaterZ, int modelIndex, int count, float speed )
{
	TEMPENTITY	*pTemp;
	int		i, frameCount;
	float		dist, angle, zspeed;
	vec3_t		origin;

	if( Mod_GetType( modelIndex ) == mod_bad )
		return;

	Mod_GetFrames( modelIndex, &frameCount );

	for( i = 0; i < count; i++ )
	{
		dist = COM_RandomFloat( 0, 1.0 );
		VectorLerp( start, dist, end, origin );
		pTemp = CL_TempEntAlloc( origin, Mod_Handle( modelIndex ));
		if( !pTemp ) return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];
		angle = COM_RandomFloat( -M_PI, M_PI );

		zspeed = COM_RandomLong( 80, 140 );
		VectorSet( pTemp->entity.baseline.origin, speed * cos( angle ), speed * sin( angle ), zspeed );
		pTemp->die = cl.time + ((flWaterZ - (origin[2] - start[2])) / zspeed) - 0.1f;
		pTemp->entity.curstate.frame = COM_RandomLong( 0, frameCount - 1 );
		// Set sprite scale
		pTemp->entity.curstate.scale = 1.0 / COM_RandomFloat( 4, 8 );
		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 192;
	}
}

/*
==============
CL_AttachTentToPlayer

Attaches entity to player
==============
*/
void CL_AttachTentToPlayer( int client, int modelIndex, float zoffset, float life )
{
	TEMPENTITY	*pTemp;
	vec3_t		position;
	cl_entity_t	*pClient;

	if( client <= 0 || client > cl.maxclients )
	{
		MsgDev( D_ERROR, "Bad client %i in AttachTentToPlayer()!\n", client );
		return;
	}

	pClient = CL_GetEntityByIndex( client );
	if( !pClient )
	{
		MsgDev( D_INFO, "Couldn't get ClientEntity for %i\n", client );
		return;
	}

	if( Mod_GetType( modelIndex ) == mod_bad )
	{
		MsgDev( D_INFO, "No model %d!\n", modelIndex );
		return;
	}

	VectorCopy( pClient->origin, position );
	position[2] += zoffset;

	pTemp = CL_TempEntAllocHigh( position, Mod_Handle( modelIndex ));

	if( !pTemp )
	{
		MsgDev( D_INFO, "No temp ent.\n" );
		return;
	}

	pTemp->entity.curstate.rendermode = kRenderNormal;
	pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 192;
	pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
	
	pTemp->clientIndex = client;
	pTemp->tentOffset[0] = 0;
	pTemp->tentOffset[1] = 0;
	pTemp->tentOffset[2] = zoffset;
	pTemp->die = cl.time + life;
	pTemp->flags |= FTENT_PLYRATTACHMENT|FTENT_PERSIST;

	// is the model a sprite?
	if( Mod_GetType( pTemp->entity.curstate.modelindex ) == mod_sprite )
	{
		pTemp->flags |= FTENT_SPRANIMATE|FTENT_SPRANIMATELOOP;
		pTemp->entity.curstate.framerate = 10;
	}
	else
	{
		// no animation support for attached clientside studio models.
		pTemp->frameMax = 0;
	}

	pTemp->entity.curstate.frame = 0;
}

/*
==============
CL_KillAttachedTents

Detach entity from player
==============
*/
void CL_KillAttachedTents( int client )
{
	int	i;

	if( client <= 0 || client > cl.maxclients )
	{
		MsgDev( D_ERROR, "Bad client %i in KillAttachedTents()!\n", client );
		return;
	}

	for( i = 0; i < GI->max_tents; i++ )
	{
		TEMPENTITY *pTemp = &cl_tempents[i];

		if( pTemp->flags & FTENT_PLYRATTACHMENT )
		{
			// this TEMPENTITY is player attached.
			// if it is attached to this client, set it to die instantly.
			if( pTemp->clientIndex == client )
			{
				pTemp->die = cl.time; // good enough, it will die on next tent update. 
			}
		}
	}
}

/*
==============
CL_RicochetSprite

Create ricochet sprite
==============
*/
void CL_RicochetSprite( const vec3_t pos, model_t *pmodel, float duration, float scale )
{
	TEMPENTITY	*pTemp;

	pTemp = CL_TempEntAlloc( pos, pmodel );
	if( !pTemp ) return;

	pTemp->entity.curstate.rendermode = kRenderGlow;
	pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 200;
	pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
	pTemp->entity.curstate.scale = scale;
	pTemp->die = cl.time + duration;
	pTemp->flags = FTENT_FADEOUT;
	pTemp->fadeSpeed = 8;

	pTemp->entity.curstate.frame = 0;
	pTemp->entity.angles[ROLL] = 45 * COM_RandomLong( 0, 7 );
}

/*
==============
CL_RocketFlare

Create rocket flare
==============
*/
void CL_RocketFlare( const vec3_t pos )
{
	TEMPENTITY	*pTemp;
	model_t		*pmodel;
	int		modelIndex;
	int		nframeCount;

	modelIndex = CL_FindModelIndex( "sprites/animglow01.spr" );
	pmodel = Mod_Handle( modelIndex );
	if( !pmodel ) return;

	Mod_GetFrames( modelIndex, &nframeCount );

	pTemp = CL_TempEntAlloc( pos, pmodel );
	if ( !pTemp ) return;

	pTemp->frameMax = nframeCount - 1;
	pTemp->entity.curstate.rendermode = kRenderGlow;
	pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
	pTemp->entity.curstate.renderamt = 200;
	pTemp->entity.curstate.framerate = 1.0;
	pTemp->entity.curstate.frame = COM_RandomLong( 0, nframeCount - 1 );
	pTemp->entity.curstate.scale = 1.0;
	pTemp->die = cl.time + 0.01f;	// when 100 fps die at next frame
	pTemp->flags |= FTENT_SPRANIMATE;
}

/*
==============
CL_MuzzleFlash

Do muzzleflash
==============
*/
void CL_MuzzleFlash( const vec3_t pos, int type )
{
	TEMPENTITY	*pTemp;
	int		index, modelIndex, frameCount;
	float		scale;

	index = ( type % 10 ) % MAX_MUZZLEFLASH;
	scale = ( type / 10 ) * 0.1f;
	if( scale == 0.0f ) scale = 0.5f;

	modelIndex = cl_muzzleflash[index];
	if( !modelIndex ) return;

	Mod_GetFrames( modelIndex, &frameCount );

	// must set position for right culling on render
	pTemp = CL_TempEntAllocHigh( pos, Mod_Handle( modelIndex ));
	if( !pTemp ) return;
	pTemp->entity.curstate.rendermode = kRenderTransAdd;
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.framerate = 10;
	pTemp->entity.curstate.renderfx = 0;
	pTemp->die = cl.time + 0.01; // die at next frame
	pTemp->entity.curstate.frame = COM_RandomLong( 0, frameCount - 1 );
	pTemp->flags |= FTENT_SPRANIMATE|FTENT_SPRANIMATELOOP;
	pTemp->frameMax = frameCount - 1;

	if( index == 0 )
	{
		// Rifle flash
		pTemp->entity.curstate.scale = scale * COM_RandomFloat( 0.5f, 0.6f );
		pTemp->entity.angles[2] = 90 * COM_RandomLong( 0, 3 );
	}
	else
	{
		pTemp->entity.curstate.scale = scale;
		pTemp->entity.angles[2] = COM_RandomLong( 0, 359 );
	}

	// play playermodel muzzleflashes only for mirror pass
	if( RP_LOCALCLIENT( RI.currententity ) && !RI.thirdPerson && ( RI.params & RP_MIRRORVIEW ))
		pTemp->entity.curstate.effects |= EF_REFLECTONLY;

	CL_TempEntAddEntity( &pTemp->entity );
}

/*
==============
CL_BloodSprite

Create a high priority blood sprite
and some blood drops. This is high-priority tent
==============
*/
void CL_BloodSprite( const vec3_t org, int colorIndex, int modelIndex, int modelIndex2, float size )
{
	TEMPENTITY	*pTemp;

	if( Mod_GetType( modelIndex ) == mod_bad )
		return;

	// large, single blood sprite is a high-priority tent
	if(( pTemp = CL_TempEntAllocHigh( org, Mod_Handle( modelIndex ))) != NULL )
	{
		int	i, frameCount;
		vec3_t	offset, dir;
		vec3_t	forward, right, up;

		colorIndex = bound( 0, colorIndex, 256 );

		Mod_GetFrames( modelIndex, &frameCount );
		pTemp->entity.curstate.rendermode = kRenderTransTexture;
		pTemp->entity.curstate.renderfx = kRenderFxClampMinScale;
		pTemp->entity.curstate.scale = COM_RandomFloat(( size / 25.0f ), ( size / 35.0f ));
		pTemp->flags = FTENT_SPRANIMATE;

		pTemp->entity.curstate.rendercolor.r = clgame.palette[colorIndex].r;
		pTemp->entity.curstate.rendercolor.g = clgame.palette[colorIndex].g;
		pTemp->entity.curstate.rendercolor.b = clgame.palette[colorIndex].b;
		pTemp->entity.curstate.framerate = frameCount * 4; // Finish in 0.250 seconds
		// play the whole thing once
		pTemp->die = cl.time + (frameCount / pTemp->entity.curstate.framerate);

		pTemp->entity.angles[2] = COM_RandomLong( 0, 360 );
		pTemp->bounceFactor = 0;

		VectorSet( forward, 0.0f, 0.0f, 1.0f );	// up-vector
		VectorVectors( forward, right, up );

		Mod_GetFrames( modelIndex2, &frameCount );

		// create blood drops
		for( i = 0; i < 14; i++ )
		{
			// originate from within a circle 'scale' inches in diameter.
			VectorCopy( org, offset );
			VectorMA( offset, COM_RandomFloat( -0.5f, 0.5f ) * size, right, offset ); 
			VectorMA( offset, COM_RandomFloat( -0.5f, 0.5f ) * size, up, offset ); 

			pTemp = CL_TempEntAlloc( org, Mod_Handle( modelIndex2 ));
			if( !pTemp ) return;

			pTemp->flags = FTENT_COLLIDEWORLD|FTENT_SLOWGRAVITY;

			pTemp->entity.curstate.rendermode = kRenderTransTexture;
			pTemp->entity.curstate.renderfx = kRenderFxClampMinScale; 
			pTemp->entity.curstate.scale = COM_RandomFloat(( size / 25.0f), ( size / 35.0f ));
			pTemp->entity.curstate.rendercolor.r = clgame.palette[colorIndex].r;
			pTemp->entity.curstate.rendercolor.g = clgame.palette[colorIndex].g;
			pTemp->entity.curstate.rendercolor.b = clgame.palette[colorIndex].b;
			pTemp->entity.curstate.frame = COM_RandomLong( 0, frameCount - 1 );
			pTemp->die = cl.time + COM_RandomFloat( 1.0f, 3.0f );

			pTemp->entity.angles[2] = COM_RandomLong( 0, 360 );
			pTemp->bounceFactor = 0;

			dir[0] = forward[0] + COM_RandomFloat( -0.8f, 0.8f );
			dir[1] = forward[1] + COM_RandomFloat( -0.8f, 0.8f );
			dir[2] = forward[2];

			VectorScale( dir, COM_RandomFloat( 8.0f * size, 20.0f * size ), pTemp->entity.baseline.origin );
			pTemp->entity.baseline.origin[2] += COM_RandomFloat( 4.0f, 16.0f ) * size;
		}
	}
}

/*
==============
CL_BreakModel

Create a shards
==============
*/
void CL_BreakModel( const vec3_t pos, const vec3_t size, const vec3_t direction, float random, float life, int count, int modelIndex, char flags )
{
	int		i, frameCount;
	int		numtries = 0;
	TEMPENTITY	*pTemp;
	char		type;
	vec3_t		dir;

	if( !modelIndex ) return;
	type = flags & BREAK_TYPEMASK;

	if( Mod_GetType( modelIndex ) == mod_bad )
		return;

	Mod_GetFrames( modelIndex, &frameCount );
		
	if( count == 0 )
	{
		// assume surface (not volume)
		count = (size[0] * size[1] + size[1] * size[2] + size[2] * size[0]) / (3 * SHARD_VOLUME * SHARD_VOLUME);
	}

	VectorCopy( direction, dir );

	// limit to 100 pieces
	if( count > 100 ) count = 100;

	if( VectorIsNull( direction ))
		random *= 10;

	for( i = 0; i < count; i++ ) 
	{
		vec3_t	vecSpot;

		if( VectorIsNull( direction ))
		{
			// random direction for each piece
			dir[0] = COM_RandomFloat( -1.0f, 1.0f );
			dir[1] = COM_RandomFloat( -1.0f, 1.0f );
			dir[2] = COM_RandomFloat( -1.0f, 1.0f );

			VectorNormalize( dir );
		}
		numtries = 0;
tryagain:
		// fill up the box with stuff
		vecSpot[0] = pos[0] + COM_RandomFloat( -0.5f, 0.5f ) * size[0];
		vecSpot[1] = pos[1] + COM_RandomFloat( -0.5f, 0.5f ) * size[1];
		vecSpot[2] = pos[2] + COM_RandomFloat( -0.5f, 0.5f ) * size[2];

		if( CL_PointContents( vecSpot ) == CONTENTS_SOLID )
		{
			if( ++numtries < 32 )
				goto tryagain;
			continue;	// a piece completely stuck in the wall, ignore it
                    }

		pTemp = CL_TempEntAlloc( vecSpot, Mod_Handle( modelIndex ));
		if( !pTemp ) return;

		// keep track of break_type, so we know how to play sound on collision
		pTemp->hitSound = type;
		
		if( Mod_GetType( modelIndex ) == mod_sprite )
			pTemp->entity.curstate.frame = COM_RandomLong( 0, frameCount - 1 );
		else if( Mod_GetType( modelIndex ) == mod_studio )
			pTemp->entity.curstate.body = COM_RandomLong( 0, frameCount - 1 );

		pTemp->flags |= FTENT_COLLIDEWORLD | FTENT_FADEOUT | FTENT_SLOWGRAVITY;

		if ( COM_RandomLong( 0, 255 ) < 200 ) 
		{
			pTemp->flags |= FTENT_ROTATE;
			pTemp->entity.baseline.angles[0] = COM_RandomFloat( -256, 255 );
			pTemp->entity.baseline.angles[1] = COM_RandomFloat( -256, 255 );
			pTemp->entity.baseline.angles[2] = COM_RandomFloat( -256, 255 );
		}

		if (( COM_RandomLong( 0, 255 ) < 100 ) && ( flags & BREAK_SMOKE ))
			pTemp->flags |= FTENT_SMOKETRAIL;

		if (( type == BREAK_GLASS ) || ( flags & BREAK_TRANS ))
		{
			pTemp->entity.curstate.rendermode = kRenderTransTexture;
			pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 128;
		}
		else
		{
			pTemp->entity.curstate.rendermode = kRenderNormal;
			pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 255; // set this for fadeout
		}

		pTemp->entity.baseline.origin[0] = dir[0] + COM_RandomFloat( -random, random );
		pTemp->entity.baseline.origin[1] = dir[1] + COM_RandomFloat( -random, random );
		pTemp->entity.baseline.origin[2] = dir[2] + COM_RandomFloat( 0, random );

		pTemp->die = cl.time + life + COM_RandomFloat( 0.0f, 1.0f ); // Add an extra 0-1 secs of life
	}
}

/*
==============
CL_TempModel

Create a temp model with gravity, sounds and fadeout
==============
*/
TEMPENTITY *CL_TempModel( const vec3_t pos, const vec3_t dir, const vec3_t angles, float life, int modelIndex, int soundtype )
{
	// alloc a new tempent
	TEMPENTITY	*pTemp;

	pTemp = CL_TempEntAlloc( pos, Mod_Handle( modelIndex ));
	if( !pTemp ) return NULL;

	// keep track of shell type
	switch( soundtype )
	{
	case TE_BOUNCE_SHELL:
		pTemp->hitSound = BOUNCE_SHELL;
		break;
	case TE_BOUNCE_SHOTSHELL:
		pTemp->hitSound = BOUNCE_SHOTSHELL;
		break;
	}

	VectorCopy( pos, pTemp->entity.origin );
	VectorCopy( angles, pTemp->entity.angles );
	VectorCopy( dir, pTemp->entity.baseline.origin );

	pTemp->entity.curstate.body = 0;
	pTemp->flags = (FTENT_COLLIDEWORLD|FTENT_GRAVITY|FTENT_ROTATE);
	pTemp->entity.baseline.angles[0] = COM_RandomFloat( -255, 255 );
	pTemp->entity.baseline.angles[1] = COM_RandomFloat( -255, 255 );
	pTemp->entity.baseline.angles[2] = COM_RandomFloat( -255, 255 );
	pTemp->entity.curstate.rendermode = kRenderNormal;
	pTemp->entity.baseline.renderamt = 255;
	pTemp->die = cl.time + life;

	return pTemp;
}

/*
==============
CL_DefaultSprite

Create an animated sprite
==============
*/
TEMPENTITY *CL_DefaultSprite( const vec3_t pos, int spriteIndex, float framerate )
{
	TEMPENTITY	*pTemp;
	int		frameCount;

	if( !spriteIndex || Mod_GetType( spriteIndex ) != mod_sprite )
	{
		MsgDev( D_INFO, "No Sprite %d!\n", spriteIndex );
		return NULL;
	}

	Mod_GetFrames( spriteIndex, &frameCount );

	pTemp = CL_TempEntAlloc( pos, Mod_Handle( spriteIndex ));
	if( !pTemp ) return NULL;

	pTemp->frameMax = frameCount - 1;
	pTemp->entity.curstate.scale = 1.0f;
	pTemp->flags |= FTENT_SPRANIMATE;
	if( framerate == 0 ) framerate = 10;

	pTemp->entity.curstate.framerate = framerate;
	pTemp->die = cl.time + (float)frameCount / framerate;
	pTemp->entity.curstate.frame = 0;

	return pTemp;
}

/*
===============
R_SparkShower

Create an animated moving sprite 
===============
*/
void R_SparkShower( const vec3_t pos )
{
	TEMPENTITY	*pTemp;

	pTemp = CL_TempEntAllocNoModel( pos );
	if( !pTemp ) return;

	pTemp->entity.baseline.origin[0] = COM_RandomFloat( -300.0f, 300.0f );
	pTemp->entity.baseline.origin[1] = COM_RandomFloat( -300.0f, 300.0f );
	pTemp->entity.baseline.origin[2] = COM_RandomFloat( -200.0f, 200.0f );

	pTemp->flags |= FTENT_SLOWGRAVITY | FTENT_COLLIDEWORLD | FTENT_SPARKSHOWER;

	pTemp->entity.curstate.framerate = COM_RandomFloat( 0.5f, 1.5f );
	pTemp->entity.curstate.scale = cl.time;
	pTemp->die = cl.time + 0.5;
}

/*
===============
CL_TempSprite

Create an animated moving sprite 
===============
*/
TEMPENTITY *CL_TempSprite( const vec3_t pos, const vec3_t dir, float scale, int modelIndex, int rendermode, int renderfx, float a, float life, int flags )
{
	TEMPENTITY	*pTemp;
	int		frameCount;

	if( !modelIndex ) 
		return NULL;

	if( Mod_GetType( modelIndex ) == mod_bad )
	{
		MsgDev( D_INFO, "No model %d!\n", modelIndex );
		return NULL;
	}

	Mod_GetFrames( modelIndex, &frameCount );

	pTemp = CL_TempEntAlloc( pos, Mod_Handle( modelIndex ));
	if( !pTemp ) return NULL;

	pTemp->frameMax = frameCount - 1;
	pTemp->entity.curstate.framerate = 10;
	pTemp->entity.curstate.rendermode = rendermode;
	pTemp->entity.curstate.renderfx = renderfx;
	pTemp->entity.curstate.scale = scale;
	pTemp->entity.baseline.renderamt = a * 255;
	pTemp->entity.curstate.renderamt = a * 255;
	pTemp->flags |= flags;

	VectorCopy( pos, pTemp->entity.origin );
	VectorCopy( dir, pTemp->entity.baseline.origin );

	if( life ) pTemp->die = cl.time + life;
	else pTemp->die = cl.time + ( frameCount * 0.1f ) + 1.0f;
	pTemp->entity.curstate.frame = 0;

	return pTemp;
}

/*
===============
CL_Sprite_Explode

apply params for exploding sprite
===============
*/
void CL_Sprite_Explode( TEMPENTITY *pTemp, float scale, int flags )
{
	if( !pTemp ) return;

	if( flags & TE_EXPLFLAG_NOADDITIVE )
	{
		// solid sprite
		pTemp->entity.curstate.rendermode = kRenderNormal;
		pTemp->entity.curstate.renderamt = 255; 
	}
	else if( flags & TE_EXPLFLAG_DRAWALPHA )
	{
		// alpha sprite (came from hl2)
		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.curstate.renderamt = 180;
	}
	else
	{
		// additive sprite
		pTemp->entity.curstate.rendermode = kRenderTransAdd;
		pTemp->entity.curstate.renderamt = 120;
	}

	if( flags & TE_EXPLFLAG_ROTATE )
	{
		// came from hl2
		pTemp->entity.angles[2] = COM_RandomLong( 0, 360 );
	}

	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.baseline.origin[2] = 8;
	pTemp->entity.origin[2] += 10;
	pTemp->entity.curstate.scale = scale;
}

/*
===============
CL_Sprite_Smoke

apply params for smoke sprite
===============
*/
void CL_Sprite_Smoke( TEMPENTITY *pTemp, float scale )
{
	int	iColor;

	if( !pTemp ) return;

	iColor = COM_RandomLong( 20, 35 );
	pTemp->entity.curstate.rendermode = kRenderTransAlpha;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.baseline.origin[2] = 30;
	pTemp->entity.curstate.rendercolor.r = iColor;
	pTemp->entity.curstate.rendercolor.g = iColor;
	pTemp->entity.curstate.rendercolor.b = iColor;
	pTemp->entity.origin[2] += 20;
	pTemp->entity.curstate.scale = scale;
}

/*
===============
CL_Spray

Throws a shower of sprites or models
===============
*/
void CL_Spray( const vec3_t pos, const vec3_t dir, int modelIndex, int count, int speed, int iRand, int renderMode )
{
	TEMPENTITY	*pTemp;
	float		noise;
	float		znoise;
	int		i, frameCount;

	noise = (float)iRand / 100;

	// more vertical displacement
	znoise = noise * 1.5f;
	if( znoise > 1 ) znoise = 1;

	if( Mod_GetType( modelIndex ) == mod_bad )
	{
		MsgDev( D_INFO, "No model %d!\n", modelIndex );
		return;
	}

	Mod_GetFrames( modelIndex, &frameCount );

	for( i = 0; i < count; i++ )
	{
		vec3_t	velocity;
		float	scale;

		pTemp = CL_TempEntAlloc( pos, Mod_Handle( modelIndex ));
		if( !pTemp ) return;

		pTemp->entity.curstate.rendermode = renderMode;
		pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
		pTemp->entity.curstate.scale = 0.5f;
		pTemp->entity.baseline.renderamt = 255;
		pTemp->flags |= FTENT_FADEOUT|FTENT_SLOWGRAVITY;
		pTemp->fadeSpeed = 2.0f;

		// make the spittle fly the direction indicated, but mix in some noise.
		velocity[0] = dir[0] + COM_RandomFloat( -noise, noise );
		velocity[1] = dir[1] + COM_RandomFloat( -noise, noise );
		velocity[2] = dir[2] + COM_RandomFloat( 0, znoise );
		scale = COM_RandomFloat(( speed * 0.8f ), ( speed * 1.2f ));
		VectorScale( velocity, scale, pTemp->entity.baseline.origin );
		pTemp->die = cl.time + 0.35f;
		pTemp->entity.curstate.frame = COM_RandomLong( 0, frameCount - 1 );
	}
}

/*
===============
CL_Sprite_Spray

Spray of alpha sprites
===============
*/
void CL_Sprite_Spray( const vec3_t pos, const vec3_t dir, int modelIndex, int count, int speed, int iRand )
{
	CL_Spray( pos, dir, modelIndex, count, speed, iRand, kRenderTransAlpha );
}

/*
===============
CL_Sprite_Trail

Line of moving glow sprites with gravity,
fadeout, and collisions
===============
*/
void CL_Sprite_Trail( int type, const vec3_t vecStart, const vec3_t vecEnd, int modelIndex, int nCount, float flLife, float flSize, float flAmplitude, int nRenderamt, float flSpeed )
{
	TEMPENTITY	*pTemp;
	vec3_t		vecDelta, vecDir;
	int		i, flFrameCount;

	if( Mod_GetType( modelIndex ) == mod_bad )
	{
		MsgDev( D_INFO, "No model %d!\n", modelIndex );
		return;
	}	

	Mod_GetFrames( modelIndex, &flFrameCount );

	VectorSubtract( vecEnd, vecStart, vecDelta );
	VectorNormalize2( vecDelta, vecDir );

	flAmplitude /= 256.0f;

	for( i = 0; i < nCount; i++ )
	{
		vec3_t	vecPos, vecVel;

		// Be careful of divide by 0 when using 'count' here...
		if( i == 0 ) VectorCopy( vecStart, vecPos );
		else VectorMA( vecStart, ( i / ( nCount - 1.0f )), vecDelta, vecPos );

		pTemp = CL_TempEntAlloc( vecPos, Mod_Handle( modelIndex ));
		if( !pTemp ) return;

		pTemp->flags = (FTENT_COLLIDEWORLD|FTENT_SPRCYCLE|FTENT_FADEOUT|FTENT_SLOWGRAVITY);

		VectorScale( vecDir, flSpeed, vecVel );
		vecVel[0] += COM_RandomFloat( -127.0f, 128.0f ) * flAmplitude;
		vecVel[1] += COM_RandomFloat( -127.0f, 128.0f ) * flAmplitude;
		vecVel[2] += COM_RandomFloat( -127.0f, 128.0f ) * flAmplitude;
		VectorCopy( vecVel, pTemp->entity.baseline.origin );
		VectorCopy( vecPos, pTemp->entity.origin );

		pTemp->entity.curstate.scale = flSize;
		pTemp->entity.curstate.rendermode = kRenderGlow;
		pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
		pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = nRenderamt;

		pTemp->entity.curstate.frame = COM_RandomLong( 0, flFrameCount - 1 );
		pTemp->frameMax = flFrameCount - 1;
		pTemp->die = cl.time + flLife + COM_RandomFloat( 0.0f, 4.0f );
	}
}

/*
===============
CL_Large_Funnel

Create a funnel effect (particles only)
===============
*/
void CL_Large_Funnel( const vec3_t pos, int flags )
{
	CL_FunnelSprite( pos, 0, flags );
}

/*
===============
CL_FunnelSprite

Create a funnel effect with custom sprite
===============
*/
void CL_FunnelSprite( const vec3_t pos, int spriteIndex, int flags )
{
	TEMPENTITY	*pTemp = NULL;
	particle_t	*pPart = NULL;
	vec3_t		dir, dest;
	vec3_t		m_vecPos;
	float		flDist, life, vel;
	int		i, j, colorIndex;

	colorIndex = CL_LookupColor( 0, 255, 0 ); // green color

	for( i = -256; i <= 256; i += 32 )
	{
		for( j = -256; j <= 256; j += 32 )
		{
			if( flags & SF_FUNNEL_REVERSE )
			{
				VectorCopy( pos, m_vecPos );

				dest[0] = pos[0] + i;
				dest[1] = pos[1] + j;
				dest[2] = pos[2] + COM_RandomFloat( 100, 800 );

				// send particle heading to dest at a random speed
				VectorSubtract( dest, m_vecPos, dir );

				// velocity based on how far particle has to travel away from org
				vel = dest[2] / 8;
			}
			else
			{
				m_vecPos[0] = pos[0] + i;
				m_vecPos[1] = pos[1] + j;
				m_vecPos[2] = pos[2] + COM_RandomFloat( 100, 800 );

				// send particle heading to org at a random speed
				VectorSubtract( pos, m_vecPos, dir );

				// velocity based on how far particle starts from org
				vel = m_vecPos[2] / 8;
			}

			if( pPart && spriteIndex && CL_PointContents( m_vecPos ) == CONTENTS_EMPTY )
			{
				pTemp = CL_TempEntAlloc( pos, Mod_Handle( spriteIndex ));
				pPart = NULL;
			}
			else
			{
				pPart = R_AllocParticle( NULL );
				pTemp = NULL;
			}

			if( pTemp || pPart )
			{
				flDist = VectorNormalizeLength( dir );	// save the distance
				if( vel < 64 ) vel = 64;
				
				vel += COM_RandomFloat( 64, 128  );
				life = ( flDist / vel );

				if( pTemp )
				{
					VectorCopy( m_vecPos, pTemp->entity.origin );
					VectorScale( dir, vel, pTemp->entity.baseline.origin );
					pTemp->entity.curstate.rendermode = kRenderTransAdd;
					pTemp->flags |= FTENT_FADEOUT;
					pTemp->fadeSpeed = 3.0f;
					pTemp->die = cl.time + life - COM_RandomFloat( 0.5f, 0.6f );
					pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 255;
					pTemp->entity.curstate.scale = 0.75f;
				}
				
				if( pPart )
				{
					VectorCopy( m_vecPos, pPart->org );
					pPart->color = colorIndex;
					pPart->type = pt_static;

					VectorScale( dir, vel, pPart->vel );
					// die right when you get there
					pPart->die += life;
				}
			}
		}
	}
}

/*
===============
R_SparkEffect

Create a streaks + richochet sprite
===============
*/
void R_SparkEffect( const vec3_t pos, int count, int velocityMin, int velocityMax )
{
	model_t	*pmodel;

	pmodel = Mod_Handle( CL_FindModelIndex( "sprites/richo1.spr" ));
	CL_RicochetSprite( pos, pmodel, 0.1f, COM_RandomFloat( 0.5f, 1.0f ));
	R_SparkStreaks( pos, count, velocityMin, velocityMax );
}

/*
==============
CL_RicochetSound

Make a random ricochet sound
==============
*/
void CL_RicochetSound( const vec3_t pos )
{
	char	soundpath[32];
	int	iPitch = COM_RandomLong( 90, 105 );
	float	fvol = COM_RandomFloat( 0.7f, 0.9f );
	sound_t	handle;

	Q_snprintf( soundpath, sizeof( soundpath ), "weapons/ric%i.wav", COM_RandomLong( 1, 5 ));
	handle = S_RegisterSound( soundpath );

	S_StartSound( pos, 0, CHAN_AUTO, handle, fvol, ATTN_NORM, iPitch, 0 );
}

/*
==============
CL_Projectile

Create an projectile entity
==============
*/
void CL_Projectile( const vec3_t origin, const vec3_t velocity, int modelIndex, int life, int owner, void (*hitcallback)( TEMPENTITY*, pmtrace_t* ))
{
	// alloc a new tempent
	TEMPENTITY	*pTemp;

	pTemp = CL_TempEntAlloc( origin, Mod_Handle( modelIndex ));
	if( !pTemp ) return;

	VectorAngles( velocity, pTemp->entity.angles );
	VectorCopy( velocity, pTemp->entity.baseline.origin );

	pTemp->entity.curstate.body = 0;
	pTemp->flags = FTENT_COLLIDEALL|FTENT_COLLIDEKILL;
	pTemp->entity.curstate.rendermode = kRenderNormal;
	pTemp->entity.baseline.renderamt = 255;
	pTemp->hitcallback = hitcallback;
	pTemp->die = cl.time + life;
	pTemp->clientIndex = owner;
}

/*
==============
CL_TempSphereModel

Spherical shower of models, picks from set
==============
*/
void CL_TempSphereModel( const vec3_t pos, float speed, float life, int count, int modelIndex )
{
	vec3_t		forward, dir;
	TEMPENTITY	*pTemp;
	int		i;
	
	VectorSet( forward, 0.0f, 0.0f, -1.0f ); // down-vector

	// create temp models
	for( i = 0; i < count; i++ )
	{
		pTemp = CL_TempEntAlloc( pos, Mod_Handle( modelIndex ));
		if( !pTemp ) return;

		dir[0] = forward[0] + COM_RandomFloat( -0.3f, 0.3f );
		dir[1] = forward[1] + COM_RandomFloat( -0.3f, 0.3f );
		dir[2] = forward[2] + COM_RandomFloat( -0.3f, 0.3f );

		VectorScale( dir, COM_RandomFloat( 30.0f, 40.0f ), pTemp->entity.baseline.origin );
		pTemp->entity.curstate.body = 0;
		pTemp->flags = (FTENT_COLLIDEWORLD|FTENT_SMOKETRAIL|FTENT_FLICKER|FTENT_GRAVITY|FTENT_ROTATE);
		pTemp->entity.baseline.angles[0] = COM_RandomFloat( -255, 255 );
		pTemp->entity.baseline.angles[1] = COM_RandomFloat( -255, 255 );
		pTemp->entity.baseline.angles[2] = COM_RandomFloat( -255, 255 );
		pTemp->entity.curstate.rendermode = kRenderNormal;
		pTemp->entity.baseline.renderamt = 255;
		pTemp->die = cl.time + life;
	}
}

/*
==============
CL_Explosion

Create an explosion (scale is magnitude)
==============
*/
void CL_Explosion( vec3_t pos, int model, float scale, float framerate, int flags )
{
	TEMPENTITY	*pTemp;
	sound_t		hSound;

	if( scale != 0.0f )
	{
		// create explosion sprite
		pTemp = CL_DefaultSprite( pos, model, framerate );
		CL_Sprite_Explode( pTemp, scale, flags );

		if( !( flags & TE_EXPLFLAG_NODLIGHTS ))
		{
			dlight_t	*dl;

			// big flash
			dl = CL_AllocDlight( 0 );
			VectorCopy( pos, dl->origin );
			dl->radius = 200;
			dl->color.r = dl->color.g = 250;
			dl->color.b = 150;
			dl->die = cl.time + 0.25f;
			dl->decay = 800;

			// red glow
			dl = CL_AllocDlight( 0 );
			VectorCopy( pos, dl->origin );
			dl->radius = 150;
			dl->color.r = 255;
			dl->color.g= 190;
			dl->color.b = 40;
			dl->die = cl.time + 1.0f;
			dl->decay = 200;
		}
	}

	if(!( flags & TE_EXPLFLAG_NOPARTICLES ))
		CL_FlickerParticles( pos );

	if( flags & TE_EXPLFLAG_NOSOUND ) return;

	hSound = S_RegisterSound( va( "weapons/explode%i.wav", COM_RandomLong( 3, 5 )));
	S_StartSound( pos, 0, CHAN_AUTO, hSound, VOL_NORM, 0.3f, PITCH_NORM, 0 );
}

/*
==============
CL_PlayerSprites

Create a particle smoke around player
==============
*/
void CL_PlayerSprites( int client, int modelIndex, int count, int size )
{
	TEMPENTITY	*pTemp;
	cl_entity_t	*pEnt;
	float		vel;
	int		i;

	pEnt = CL_GetEntityByIndex( client );

	if( !pEnt || !pEnt->player )
	{
		MsgDev( D_INFO, "Bad ent %i in R_PlayerSprites()!\n", client );
		return;
	}

	vel = 128;

	for( i = 0; i < count; i++ )
	{
		pTemp = CL_DefaultSprite( pEnt->origin, modelIndex, 15 );
		if( !pTemp ) return;

		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.curstate.renderfx = kRenderFxNone;
		pTemp->entity.baseline.origin[0] = COM_RandomFloat( -1.0f, 1.0f ) * vel;
		pTemp->entity.baseline.origin[1] = COM_RandomFloat( -1.0f, 1.0f ) * vel;
		pTemp->entity.baseline.origin[2] = COM_RandomFloat( 0.0f, 1.0f ) * vel;
		pTemp->entity.curstate.rendercolor.r = 192;
		pTemp->entity.curstate.rendercolor.g = 192;
		pTemp->entity.curstate.rendercolor.b = 192;
		pTemp->entity.curstate.renderamt = 64;
		pTemp->entity.curstate.scale = size;
	}
}

/*
==============
CL_FireField

Makes a field of fire
==============
*/
void CL_FireField( float *org, int radius, int modelIndex, int count, int flags, float life )
{
	TEMPENTITY	*pTemp;
	float		radius2;
	vec3_t		dir, m_vecPos;
	int		i;

	for( i = 0; i < count; i++ )
	{
		dir[0] = COM_RandomFloat( -1.0f, 1.0f );
		dir[1] = COM_RandomFloat( -1.0f, 1.0f );

		if( flags & TEFIRE_FLAG_PLANAR ) dir[2] = 0.0f;
		else dir[2] = COM_RandomFloat( -1.0f, 1.0f );
		VectorNormalize( dir );

		radius2 = COM_RandomFloat( 0.0f, radius );
		VectorMA( org, -radius2, dir, m_vecPos );

		pTemp = CL_DefaultSprite( m_vecPos, modelIndex, 0 );
		if( !pTemp ) return;

		if( flags & TEFIRE_FLAG_ALLFLOAT )
			pTemp->entity.baseline.origin[2] = 30; // drift sprite upward
		else if( flags & TEFIRE_FLAG_SOMEFLOAT && COM_RandomLong( 0, 1 ))
			pTemp->entity.baseline.origin[2] = 30; // drift sprite upward

		if( flags & TEFIRE_FLAG_LOOP )
		{
			pTemp->entity.curstate.framerate = 15;
			pTemp->flags |= FTENT_SPRANIMATELOOP;
		}

		if( flags & TEFIRE_FLAG_ALPHA )
		{
			pTemp->entity.curstate.rendermode = kRenderTransTexture;
			pTemp->entity.curstate.renderamt = 128;
		}

		pTemp->die += life;
	}
}

/*
==============
CL_MultiGunshot

Client version of shotgun shot
==============
*/
void CL_MultiGunshot( const vec3_t org, const vec3_t dir, const vec3_t noise, int count, int decalCount, int *decalIndices )
{
	pmtrace_t	trace;
	vec3_t	right, up;
	vec3_t	vecSrc, vecDir, vecEnd;
	int	i, j, decalIndex;

	VectorVectors( dir, right, up );
	VectorCopy( org, vecSrc );

	for( i = 1; i <= count; i++ )
	{
		// get circular gaussian spread
		float x, y, z;
		do {
			x = COM_RandomFloat( -0.5f, 0.5f ) + COM_RandomFloat( -0.5f, 0.5f );
			y = COM_RandomFloat( -0.5f, 0.5f ) + COM_RandomFloat( -0.5f, 0.5f );
			z = x * x + y * y;
		} while( z > 1.0f );

		for( j = 0; j < 3; j++ )
		{
			vecDir[j] = dir[i] + x * noise[0] * right[j] + y * noise[1] * up[j];
			vecEnd[j] = vecSrc[j] + 2048.0f * vecDir[j];
		}

		trace = CL_TraceLine( vecSrc, vecEnd, PM_STUDIO_BOX );

		// paint decals
		if( trace.fraction != 1.0f )
		{
			physent_t	*pe = NULL;

			if( trace.ent >= 0 && trace.ent < clgame.pmove->numphysent )
				pe = &clgame.pmove->physents[trace.ent];

			if( pe && ( pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP ))
			{
				cl_entity_t *e = CL_GetEntityByIndex( pe->info );
				decalIndex = CL_DecalIndex( decalIndices[COM_RandomLong( 0, decalCount-1 )] );
				CL_DecalShoot( decalIndex, e->index, 0, trace.endpos, 0 );
			}
		}
	}
}

/*
==============
CL_Sprite_WallPuff

Create a wallpuff
==============
*/
void CL_Sprite_WallPuff( TEMPENTITY *pTemp, float scale )
{
	if( !pTemp ) return;

	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.rendermode = kRenderTransAlpha;
	pTemp->entity.angles[ROLL] = COM_RandomLong( 0, 359 );
	pTemp->entity.curstate.scale = scale;
	pTemp->die = cl.time + 0.01f;
}

/*
==============
CL_ParseTempEntity

handle temp-entity messages
==============
*/
void CL_ParseTempEntity( sizebuf_t *msg )
{
	sizebuf_t		buf;
	byte		pbuf[256];
	int		iSize = MSG_ReadByte( msg );
	int		type, color, count, flags;
	int		decalIndex, modelIndex, entityIndex;
	float		scale, life, frameRate, vel, random;
	float		brightness, r, g, b;
	vec3_t		pos, pos2, ang;
	int		decalIndices[1];	// just stub
	TEMPENTITY	*pTemp;
	cl_entity_t	*pEnt;
	dlight_t		*dl;

	decalIndex = modelIndex = entityIndex = 0;

	// parse user message into buffer
	MSG_ReadBytes( msg, pbuf, iSize );

	// init a safe tempbuffer
	MSG_Init( &buf, "TempEntity", pbuf, iSize );

	type = MSG_ReadByte( &buf );

	switch( type )
	{
	case TE_BEAMPOINTS:
	case TE_BEAMENTPOINT:
	case TE_LIGHTNING:
	case TE_BEAMENTS:
	case TE_BEAM:
	case TE_BEAMSPRITE:
	case TE_BEAMTORUS:
	case TE_BEAMDISK:
	case TE_BEAMCYLINDER:
	case TE_BEAMFOLLOW:
	case TE_BEAMRING:
	case TE_BEAMHOSE:
	case TE_KILLBEAM:
		CL_ParseViewBeam( &buf, type );
		break;
	case TE_GUNSHOT:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		CL_RicochetSound( pos );
		R_RunParticleEffect( pos, vec3_origin, 0, 20 );
		break;
	case TE_EXPLOSION:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		modelIndex = MSG_ReadShort( &buf );
		scale = (float)(MSG_ReadByte( &buf ) * 0.1f);
		frameRate = MSG_ReadByte( &buf );
		flags = MSG_ReadByte( &buf );
		CL_Explosion( pos, modelIndex, scale, frameRate, flags );
		break;
	case TE_TAREXPLOSION:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		CL_BlobExplosion( pos );
		break;
	case TE_SMOKE:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		modelIndex = MSG_ReadShort( &buf );
		scale = (float)(MSG_ReadByte( &buf ) * 0.1f);
		frameRate = MSG_ReadByte( &buf );
		pTemp = CL_DefaultSprite( pos, modelIndex, frameRate );
		CL_Sprite_Smoke( pTemp, scale );
		break;
	case TE_TRACER:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		pos2[0] = MSG_ReadCoord( &buf );
		pos2[1] = MSG_ReadCoord( &buf );
		pos2[2] = MSG_ReadCoord( &buf );
		R_TracerEffect( pos, pos2 );
		break;
	case TE_SPARKS:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		R_SparkShower( pos );
		break;
	case TE_LAVASPLASH:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		CL_LavaSplash( pos );
		break;
	case TE_TELEPORT:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		CL_TeleportSplash( pos );
		break;
	case TE_EXPLOSION2:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		color = MSG_ReadByte( &buf );
		count = MSG_ReadByte( &buf );
		CL_ParticleExplosion2( pos, color, count );
		break;
	case TE_BSPDECAL:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		decalIndex = MSG_ReadShort( &buf );
		entityIndex = MSG_ReadShort( &buf );
		if( entityIndex ) modelIndex = MSG_ReadShort( &buf );
		CL_DecalShoot( CL_DecalIndex( decalIndex ), entityIndex, modelIndex, pos, FDECAL_PERMANENT );
		break;
	case TE_IMPLOSION:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		scale = MSG_ReadByte( &buf );
		count = MSG_ReadByte( &buf );
		life = (float)(MSG_ReadByte( &buf ) * 0.1f);
		R_Implosion( pos, scale, count, life );
		break;
	case TE_SPRITETRAIL:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		pos2[0] = MSG_ReadCoord( &buf );
		pos2[1] = MSG_ReadCoord( &buf );
		pos2[2] = MSG_ReadCoord( &buf );
		modelIndex = MSG_ReadShort( &buf );
		count = MSG_ReadByte( &buf );
		life = (float)(MSG_ReadByte( &buf ) * 0.1f);
		scale = (float)(MSG_ReadByte( &buf ) * 0.1f);
		vel = (float)MSG_ReadByte( &buf );
		random = (float)MSG_ReadByte( &buf );
		CL_Sprite_Trail( type, pos, pos2, modelIndex, count, life, scale, random, 255, vel );
		break;
	case TE_SPRITE:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		modelIndex = MSG_ReadShort( &buf );
		scale = (float)(MSG_ReadByte( &buf ) * 0.1f);
		brightness = (float)MSG_ReadByte( &buf );

		if(( pTemp = CL_DefaultSprite( pos, modelIndex, 0 )) != NULL )
		{
			pTemp->entity.curstate.scale = scale;
			pTemp->entity.baseline.renderamt = brightness;
			pTemp->entity.curstate.renderamt = brightness;
			pTemp->entity.curstate.rendermode = kRenderTransAdd;
		}
		break;
	case TE_GLOWSPRITE:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		modelIndex = MSG_ReadShort( &buf );
		life = (float)(MSG_ReadByte( &buf ) * 0.1f);
		scale = (float)(MSG_ReadByte( &buf ) * 0.1f);
		brightness = (float)MSG_ReadByte( &buf );

		if(( pTemp = CL_DefaultSprite( pos, modelIndex, 0 )) != NULL )
		{
			pTemp->entity.curstate.scale = scale;
			pTemp->entity.curstate.rendermode = kRenderGlow;
			pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
			pTemp->entity.baseline.renderamt = brightness;
			pTemp->entity.curstate.renderamt = brightness;
			pTemp->flags = FTENT_FADEOUT;
			pTemp->die = cl.time + life;
		}
		break;
	case TE_STREAK_SPLASH:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		pos2[0] = MSG_ReadCoord( &buf );
		pos2[1] = MSG_ReadCoord( &buf );
		pos2[2] = MSG_ReadCoord( &buf );
		color = MSG_ReadByte( &buf );
		count = MSG_ReadShort( &buf );
		vel = (float)MSG_ReadShort( &buf );
		random = (float)MSG_ReadShort( &buf );
		R_StreakSplash( pos, pos2, color, count, vel, -random, random );
		break;
	case TE_DLIGHT:
		dl = CL_AllocDlight( 0 );
		dl->origin[0] = MSG_ReadCoord( &buf );
		dl->origin[1] = MSG_ReadCoord( &buf );
		dl->origin[2] = MSG_ReadCoord( &buf );
		dl->radius = (float)(MSG_ReadByte( &buf ) * 10.0f);
		dl->color.r = MSG_ReadByte( &buf );
		dl->color.g = MSG_ReadByte( &buf );
		dl->color.b = MSG_ReadByte( &buf );
		dl->die = cl.time + (float)(MSG_ReadByte( &buf ) * 0.1f);
		dl->decay = (float)(MSG_ReadByte( &buf ) * 10.0f);
		break;
	case TE_ELIGHT:
		dl = CL_AllocElight( 0 );
		entityIndex = MSG_ReadShort( &buf );
		dl->origin[0] = MSG_ReadCoord( &buf );
		dl->origin[1] = MSG_ReadCoord( &buf );
		dl->origin[2] = MSG_ReadCoord( &buf );
		dl->radius = MSG_ReadCoord( &buf );
		dl->color.r = MSG_ReadByte( &buf );
		dl->color.g = MSG_ReadByte( &buf );
		dl->color.b = MSG_ReadByte( &buf );
		dl->die = cl.time + (float)(MSG_ReadByte( &buf ) * 0.1f);
		dl->decay = MSG_ReadCoord( &buf );
		break;
	case TE_TEXTMESSAGE:
		CL_ParseTextMessage( &buf );
		break;
	case TE_LINE:
	case TE_BOX:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		pos2[0] = MSG_ReadCoord( &buf );
		pos2[1] = MSG_ReadCoord( &buf );
		pos2[2] = MSG_ReadCoord( &buf );
		life = (float)(MSG_ReadShort( &buf ) * 0.1f);
		r = MSG_ReadByte( &buf );
		g = MSG_ReadByte( &buf );
		b = MSG_ReadByte( &buf );
		if( type == TE_LINE ) CL_ParticleLine( pos, pos2, r, g, b, life );
		else CL_ParticleBox( pos, pos2, r, g, b, life );
		break;
	case TE_LARGEFUNNEL:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		modelIndex = MSG_ReadShort( &buf );
		flags = MSG_ReadShort( &buf );
		CL_FunnelSprite( pos, modelIndex, flags );
		break;
	case TE_BLOODSTREAM:
	case TE_BLOOD:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		pos2[0] = MSG_ReadCoord( &buf );
		pos2[1] = MSG_ReadCoord( &buf );
		pos2[2] = MSG_ReadCoord( &buf );
		color = MSG_ReadByte( &buf );
		vel = (float)MSG_ReadByte( &buf );
		if( type == TE_BLOOD ) CL_Blood( pos, pos2, color, vel );
		else CL_BloodStream( pos, pos2, color, vel );
		break;
	case TE_SHOWLINE:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		pos2[0] = MSG_ReadCoord( &buf );
		pos2[1] = MSG_ReadCoord( &buf );
		pos2[2] = MSG_ReadCoord( &buf );
		CL_ShowLine( pos, pos2 );
		break;
	case TE_DECAL:
	case TE_DECALHIGH:
	case TE_WORLDDECAL:
	case TE_WORLDDECALHIGH:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		decalIndex = MSG_ReadByte( &buf );
		if( type == TE_DECAL || type == TE_DECALHIGH )
			entityIndex = MSG_ReadShort( &buf );
		else entityIndex = 0;
		if( type == TE_DECALHIGH || type == TE_WORLDDECALHIGH )
			decalIndex += 256;
		pEnt = CL_GetEntityByIndex( entityIndex );
		if( pEnt ) modelIndex = pEnt->curstate.modelindex;
		CL_DecalShoot( CL_DecalIndex( decalIndex ), entityIndex, modelIndex, pos, 0 );
		break;
	case TE_FIZZ:
		entityIndex = MSG_ReadShort( &buf );
		modelIndex = MSG_ReadShort( &buf );
		scale = MSG_ReadByte( &buf );	// same as density
		pEnt = CL_GetEntityByIndex( entityIndex );
		CL_FizzEffect( pEnt, modelIndex, scale );
		break;
	case TE_MODEL:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		pos2[0] = MSG_ReadCoord( &buf );
		pos2[1] = MSG_ReadCoord( &buf );
		pos2[2] = MSG_ReadCoord( &buf );
		VectorSet( ang, 0.0f, MSG_ReadAngle( &buf ), 0.0f ); // yaw angle
		modelIndex = MSG_ReadShort( &buf );
		flags = MSG_ReadByte( &buf );	// sound flags
		life = (float)(MSG_ReadByte( &buf ) * 0.1f);
		CL_TempModel( pos, pos2, ang, life, modelIndex, flags );
		break;
	case TE_EXPLODEMODEL:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		vel = MSG_ReadCoord( &buf );
		modelIndex = MSG_ReadShort( &buf );
		count = MSG_ReadShort( &buf );
		life = (float)(MSG_ReadByte( &buf ) * 0.1f);
		CL_TempSphereModel( pos, vel, life, count, modelIndex );
		break;
	case TE_BREAKMODEL:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		pos2[0] = MSG_ReadCoord( &buf );
		pos2[1] = MSG_ReadCoord( &buf );
		pos2[2] = MSG_ReadCoord( &buf );
		ang[0] = MSG_ReadCoord( &buf );
		ang[1] = MSG_ReadCoord( &buf );
		ang[2] = MSG_ReadCoord( &buf );
		random = (float)MSG_ReadByte( &buf );
		modelIndex = MSG_ReadShort( &buf );
		count = MSG_ReadByte( &buf );
		life = (float)(MSG_ReadByte( &buf ) * 0.1f);
		flags = MSG_ReadByte( &buf );
		CL_BreakModel( pos, pos2, ang, random, life, count, modelIndex, (char)flags );
		break;
	case TE_GUNSHOTDECAL:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		entityIndex = MSG_ReadShort( &buf );
		decalIndex = MSG_ReadByte( &buf );
		pEnt = CL_GetEntityByIndex( entityIndex );
		CL_DecalShoot( CL_DecalIndex( decalIndex ), entityIndex, 0, pos, 0 );
		R_BulletImpactParticles( pos );
		CL_RicochetSound( pos );
		break;
	case TE_SPRAY:
	case TE_SPRITE_SPRAY:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		pos2[0] = MSG_ReadCoord( &buf );
		pos2[1] = MSG_ReadCoord( &buf );
		pos2[2] = MSG_ReadCoord( &buf );
		modelIndex = MSG_ReadShort( &buf );
		count = MSG_ReadByte( &buf );
		vel = (float)MSG_ReadByte( &buf );
		random = (float)MSG_ReadByte( &buf );
		if( type == TE_SPRAY )
		{
			flags = MSG_ReadByte( &buf );	// rendermode
			CL_Spray( pos, pos2, modelIndex, count, vel, random, flags );
		}
		else CL_Sprite_Spray( pos, pos2, modelIndex, count, vel, random );
		break;
	case TE_ARMOR_RICOCHET:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		scale = (float)(MSG_ReadByte( &buf ) * 0.1f);
		modelIndex = CL_FindModelIndex( "sprites/richo1.spr" );
		CL_RicochetSprite( pos, Mod_Handle( modelIndex ), 0.0f, scale );
		CL_RicochetSound( pos );
		break;
	case TE_PLAYERDECAL:
		color = MSG_ReadByte( &buf );	// playernum
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		entityIndex = MSG_ReadShort( &buf );
		decalIndex = MSG_ReadByte( &buf );
		CL_PlayerDecal( CL_DecalIndex( decalIndex ), entityIndex, pos );
		break;
	case TE_BUBBLES:
	case TE_BUBBLETRAIL:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		pos2[0] = MSG_ReadCoord( &buf );
		pos2[1] = MSG_ReadCoord( &buf );
		pos2[2] = MSG_ReadCoord( &buf );
		scale = MSG_ReadCoord( &buf );	// water height
		modelIndex = MSG_ReadShort( &buf );
		count = MSG_ReadByte( &buf );
		vel = MSG_ReadCoord( &buf );
		if( type == TE_BUBBLES ) CL_Bubbles( pos, pos2, scale, modelIndex, count, vel );
		else CL_BubbleTrail( pos, pos2, scale, modelIndex, count, vel );
		break;
	case TE_BLOODSPRITE:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		modelIndex = MSG_ReadShort( &buf );	// sprite #1
		decalIndex = MSG_ReadShort( &buf );	// sprite #2
		color = MSG_ReadByte( &buf );
		scale = (float)MSG_ReadByte( &buf );
		CL_BloodSprite( pos, color, modelIndex, decalIndex, scale );
		break;
	case TE_PROJECTILE:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		pos2[0] = MSG_ReadCoord( &buf );
		pos2[1] = MSG_ReadCoord( &buf );
		pos2[2] = MSG_ReadCoord( &buf );
		modelIndex = MSG_ReadShort( &buf );
		life = (float)(MSG_ReadByte( &buf ) * 0.1f);
		color = MSG_ReadByte( &buf );	// playernum
		CL_Projectile( pos, pos2, modelIndex, life, color, NULL );
		break;
	case TE_PLAYERSPRITES:
		color = MSG_ReadShort( &buf );	// entitynum
		modelIndex = MSG_ReadShort( &buf );
		count = MSG_ReadByte( &buf );
		random = (float)MSG_ReadByte( &buf );
		CL_PlayerSprites( color, modelIndex, count, random );
		break;
	case TE_PARTICLEBURST:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		scale = (float)MSG_ReadShort( &buf );
		color = MSG_ReadByte( &buf );
		life = (float)(MSG_ReadByte( &buf ) * 0.1f);
		CL_ParticleBurst( pos, scale, color, life );
		break;
	case TE_FIREFIELD:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		scale = (float)MSG_ReadShort( &buf );
		modelIndex = MSG_ReadShort( &buf );
		count = MSG_ReadByte( &buf );
		flags = MSG_ReadByte( &buf );
		life = (float)(MSG_ReadByte( &buf ) * 0.1f);
		CL_FireField( pos, scale, modelIndex, count, flags, life );
		break;
	case TE_PLAYERATTACHMENT:
		color = MSG_ReadByte( &buf );	// playernum
		scale = MSG_ReadCoord( &buf );	// height
		modelIndex = MSG_ReadShort( &buf );
		life = (float)(MSG_ReadShort( &buf ) * 0.1f);
		CL_AttachTentToPlayer( color, modelIndex, scale, life );
		break;
	case TE_KILLPLAYERATTACHMENTS:
		color = MSG_ReadByte( &buf );	// playernum
		CL_KillAttachedTents( color );
		break;
	case TE_MULTIGUNSHOT:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		pos2[0] = MSG_ReadCoord( &buf );
		pos2[1] = MSG_ReadCoord( &buf );
		pos2[2] = MSG_ReadCoord( &buf );
		ang[0] = MSG_ReadCoord( &buf ) * 0.01f;
		ang[1] = MSG_ReadCoord( &buf ) * 0.01f;
		ang[2] = 0.0f;
		count = MSG_ReadByte( &buf );
		decalIndices[0] = MSG_ReadByte( &buf );
		CL_MultiGunshot( pos, pos2, ang, count, 1, decalIndices );
		break;
	case TE_USERTRACER:
		pos[0] = MSG_ReadCoord( &buf );
		pos[1] = MSG_ReadCoord( &buf );
		pos[2] = MSG_ReadCoord( &buf );
		pos2[0] = MSG_ReadCoord( &buf );
		pos2[1] = MSG_ReadCoord( &buf );
		pos2[2] = MSG_ReadCoord( &buf );
		life = (float)(MSG_ReadByte( &buf ) * 0.1f);
		color = MSG_ReadByte( &buf );
		scale = (float)(MSG_ReadByte( &buf ) * 0.1f);
		R_UserTracerParticle( pos, pos2, life, color, scale, 0, NULL );
		break;
	default:
		MsgDev( D_ERROR, "ParseTempEntity: illegible TE message %i\n", type );
		break;
	}

	// throw warning
	if( MSG_CheckOverflow( &buf )) MsgDev( D_WARN, "ParseTempEntity: overflow TE message\n" );
}


/*
==============================================================

LIGHT STYLE MANAGEMENT

==============================================================
*/
#define STYLE_LERPING_THRESHOLD	3.0f // because we wan't interpolate fast sequences (like on\off)
		
/*
================
CL_ClearLightStyles
================
*/
void CL_ClearLightStyles( void )
{
	memset( cl.lightstyles, 0, sizeof( cl.lightstyles ));
}

void CL_SetLightstyle( int style, const char *s, float f )
{
	int		i, k;
	lightstyle_t	*ls;
	float		val1, val2;

	ASSERT( s );
	ASSERT( style >= 0 && style < MAX_LIGHTSTYLES );

	ls = &cl.lightstyles[style];

	Q_strncpy( ls->pattern, s, sizeof( ls->pattern ));

	ls->length = Q_strlen( s );
	ls->time = f; // set local time

	for( i = 0; i < ls->length; i++ )
		ls->map[i] = (float)(s[i] - 'a');

	ls->interp = (ls->length <= 1) ? false : true;

	// check for allow interpolate
	// NOTE: fast flickering styles looks ugly when interpolation is running
	for( k = 0; k < (ls->length - 1); k++ )
	{
		val1 = ls->map[(k+0) % ls->length];
		val2 = ls->map[(k+1) % ls->length];

		if( fabs( val1 - val2 ) > STYLE_LERPING_THRESHOLD )
		{
			ls->interp = false;
			break;
		}
	}
	MsgDev( D_REPORT, "Lightstyle %i (%s), interp %s\n", style, ls->pattern, ls->interp ? "Yes" : "No" );
}

/*
==============================================================

DLIGHT MANAGEMENT

==============================================================
*/
dlight_t	cl_dlights[MAX_DLIGHTS];
dlight_t	cl_elights[MAX_ELIGHTS];

/*
================
CL_ClearDlights
================
*/
void CL_ClearDlights( void )
{
	memset( cl_dlights, 0, sizeof( cl_dlights ));
	memset( cl_elights, 0, sizeof( cl_elights ));
}

/*
===============
CL_AllocDlight

===============
*/
dlight_t *CL_AllocDlight( int key )
{
	dlight_t	*dl;
	int	i;

	// first look for an exact key match
	if( key )
	{
		for( i = 0, dl = cl_dlights; i < MAX_DLIGHTS; i++, dl++ )
		{
			if( dl->key == key )
			{
				// reuse this light
				memset( dl, 0, sizeof( *dl ));
				dl->key = key;
				return dl;
			}
		}
	}

	// then look for anything else
	for( i = 0, dl = cl_dlights; i < MAX_DLIGHTS; i++, dl++ )
	{
		if( dl->die < cl.time && dl->key == 0 )
		{
			memset( dl, 0, sizeof( *dl ));
			dl->key = key;
			return dl;
		}
	}

	// otherwise grab first dlight
	dl = &cl_dlights[0];
	memset( dl, 0, sizeof( *dl ));
	dl->key = key;

	return dl;
}

/*
===============
CL_AllocElight

===============
*/
dlight_t *CL_AllocElight( int key )
{
	dlight_t	*dl;
	int	i;

	// first look for an exact key match
	if( key )
	{
		for( i = 0, dl = cl_elights; i < MAX_ELIGHTS; i++, dl++ )
		{
			if( dl->key == key )
			{
				// reuse this light
				memset( dl, 0, sizeof( *dl ));
				dl->key = key;
				return dl;
			}
		}
	}

	// then look for anything else
	for( i = 0, dl = cl_elights; i < MAX_ELIGHTS; i++, dl++ )
	{
		if( dl->die < cl.time && dl->key == 0 )
		{
			memset( dl, 0, sizeof( *dl ));
			dl->key = key;
			return dl;
		}
	}

	// otherwise grab first dlight
	dl = &cl_elights[0];
	memset( dl, 0, sizeof( *dl ));
	dl->key = key;

	return dl;
}

/*
===============
CL_DecayLights

===============
*/
void CL_DecayLights( void )
{
	dlight_t	*dl;
	float	time;
	int	i;
	
	time = cl.time - cl.oldtime;

	for( i = 0, dl = cl_dlights; i < MAX_DLIGHTS; i++, dl++ )
	{
		if( !dl->radius ) continue;

		dl->radius -= time * dl->decay;
		if( dl->radius < 0 ) dl->radius = 0;

		if( dl->die < cl.time || !dl->radius ) 
			memset( dl, 0, sizeof( *dl ));
	}

	for( i = 0, dl = cl_elights; i < MAX_ELIGHTS; i++, dl++ )
	{
		if( !dl->radius ) continue;

		dl->radius -= time * dl->decay;
		if( dl->radius < 0 ) dl->radius = 0;

		if( dl->die < cl.time || !dl->radius ) 
			memset( dl, 0, sizeof( *dl ));
	}
}

/*
================
CL_UpdateFlashlight

update client flashlight
================
*/
void CL_UpdateFlashlight( cl_entity_t *ent )
{
	vec3_t	forward, view_ofs;
	vec3_t	vecSrc, vecEnd;
	float	falloff;
	pmtrace_t	trace;
	dlight_t	*dl;

	if( ent->index == ( cl.playernum + 1 ))
	{
		// local player case
		AngleVectors( cl.viewangles, forward, NULL, NULL );
		VectorCopy( cl.viewheight, view_ofs );
	}
	else	// non-local player case
	{
		vec3_t	v_angle;

		// NOTE: pitch divided by 3.0 twice. So we need apply 3^2 = 9
		v_angle[PITCH] = ent->curstate.angles[PITCH] * 9.0f;
		v_angle[YAW] = ent->angles[YAW];
		v_angle[ROLL] = 0.0f; // roll not used

		AngleVectors( v_angle, forward, NULL, NULL );
		view_ofs[0] = view_ofs[1] = 0.0f;

		// FIXME: these values are hardcoded ...
		if( ent->curstate.usehull == 1 )
			view_ofs[2] = 12.0f;	// VEC_DUCK_VIEW;
		else view_ofs[2] = 28.0f;		// DEFAULT_VIEWHEIGHT
	}

	VectorAdd( ent->origin, view_ofs, vecSrc );
	VectorMA( vecSrc, FLASHLIGHT_DISTANCE, forward, vecEnd );

	trace = CL_TraceLine( vecSrc, vecEnd, PM_STUDIO_BOX );

	// update flashlight endpos
	dl = CL_AllocDlight( ent->index );

	if( trace.ent > 0 && clgame.pmove->physents[trace.ent].studiomodel )
		VectorCopy( clgame.pmove->physents[trace.ent].origin, dl->origin );
	else VectorCopy( trace.endpos, dl->origin );

	// compute falloff
	falloff = trace.fraction * FLASHLIGHT_DISTANCE;
	if( falloff < 500.0f ) falloff = 1.0f;
	else falloff = 500.0f / falloff;
	falloff *= falloff;

	// apply brigthness to dlight			
	dl->color.r = bound( 0, 255 * falloff, 255 );
	dl->color.g = bound( 0, 255 * falloff, 255 );
	dl->color.b = bound( 0, 255 * falloff, 255 );
	dl->die = cl.time + 0.01f; // die on next frame
	dl->radius = 80;
}

/*
================
CL_AddEntityEffects

apply various effects to entity origin or attachment
================
*/
void CL_AddEntityEffects( cl_entity_t *ent )
{
	// yellow flies effect 'monster stuck in the wall'
	if( FBitSet( ent->curstate.effects, EF_BRIGHTFIELD ))
		CL_EntityParticles( ent );

	if( FBitSet( ent->curstate.effects, EF_DIMLIGHT ))
	{
		if( ent->player )
		{
			CL_UpdateFlashlight( ent );
		}
		else
		{
			dlight_t	*dl = CL_AllocDlight( ent->index );
			dl->color.r = dl->color.g = dl->color.b = 100;
			dl->radius = COM_RandomFloat( 200, 231 );
			VectorCopy( ent->origin, dl->origin );
			dl->die = cl.time + 0.001;
		}
	}	

	if( FBitSet( ent->curstate.effects, EF_BRIGHTLIGHT ))
	{			
		dlight_t	*dl = CL_AllocDlight( ent->index );
		dl->color.r = dl->color.g = dl->color.b = 250;
		if( ent->player ) dl->radius = 400; // don't flickering
		else dl->radius = COM_RandomFloat( 400, 431 );
		VectorCopy( ent->origin, dl->origin );
		dl->die = cl.time + 0.001;
		ent->origin[2] += 16.0f;
	}

	// add light effect
	if( FBitSet( ent->curstate.effects, EF_LIGHT ))
	{
		dlight_t	*dl = CL_AllocDlight( ent->index );
		dl->color.r = dl->color.g = dl->color.b = 100;
		VectorCopy( ent->origin, dl->origin );
		CL_RocketFlare( ent->origin );
		dl->die = cl.time + 0.001;
		dl->radius = 200;
	}
}

/*
================
CL_AddStudioEffects

these effects will be enable by flag in model header
================
*/
void CL_AddStudioEffects( cl_entity_t *ent )
{
	vec3_t	oldorigin;

	if( !ent->model || ent->model->type != mod_studio )
		return;

	VectorCopy( ent->latched.prevorigin, oldorigin );

	// NOTE: this completely over control about angles and don't broke interpolation
	if( FBitSet( ent->model->flags, STUDIO_ROTATE ))
		ent->angles[1] = anglemod( 100.0f * cl.time );

	if( FBitSet( ent->model->flags, STUDIO_GIB ))
		CL_RocketTrail( oldorigin, ent->curstate.origin, 2 );

	if( FBitSet( ent->model->flags, STUDIO_ZOMGIB ))
		CL_RocketTrail( oldorigin, ent->curstate.origin, 4 );

	if( FBitSet( ent->model->flags, STUDIO_TRACER ))
		CL_RocketTrail( oldorigin, ent->curstate.origin, 3 );

	if( FBitSet( ent->model->flags, STUDIO_TRACER2 ))
		CL_RocketTrail( oldorigin, ent->curstate.origin, 5 );

	if( FBitSet( ent->model->flags, STUDIO_ROCKET ))
	{
		dlight_t	*dl = CL_AllocDlight( ent->index );

		dl->color.r = dl->color.g = dl->color.b = 200;
		VectorCopy( ent->origin, dl->origin );

		// HACKHACK: get radius from head entity
		if( ent->curstate.rendermode != kRenderNormal )
			dl->radius = Q_max( 0, ent->curstate.renderamt - 55 );
		else dl->radius = 200;

		dl->die = cl.time + 0.01f;

		CL_RocketTrail( oldorigin, ent->curstate.origin, 0 );
	}

	if( FBitSet( ent->model->flags, STUDIO_GRENADE ))
		CL_RocketTrail( oldorigin, ent->curstate.origin, 1 );

	if( FBitSet( ent->model->flags, STUDIO_TRACER3 ))
		CL_RocketTrail( oldorigin, ent->curstate.origin, 6 );
}

/*
================
CL_TestLights

if cl_testlights is set, create 32 lights models
================
*/
void CL_TestLights( void )
{
	int	i, j;
	float	f, r;
	dlight_t	*dl;

	if( !cl_testlights->value ) return;
	
	for( i = 0; i < bound( 1, cl_testlights->value, MAX_DLIGHTS ); i++ )
	{
		dl = &cl_dlights[i];

		r = 64 * ((i % 4) - 1.5f );
		f = 64 * ( i / 4) + 128;

		for( j = 0; j < 3; j++ )
			dl->origin[j] = RI.vieworg[j] + RI.vforward[j] * f + RI.vright[j] * r;

		dl->color.r = ((((i % 6) + 1) & 1)>>0) * 255;
		dl->color.g = ((((i % 6) + 1) & 2)>>1) * 255;
		dl->color.b = ((((i % 6) + 1) & 4)>>2) * 255;
		dl->die = cl.time + host.frametime;
		dl->radius = 200;
	}
}

/*
==============================================================

DECAL MANAGEMENT

==============================================================
*/
/*
===============
CL_DecalShoot

normal temporary decal
===============
*/
void CL_DecalShoot( int textureIndex, int entityIndex, int modelIndex, float *pos, int flags )
{
	R_DecalShoot( textureIndex, entityIndex, modelIndex, pos, flags, NULL, 1.0f );
}

/*
===============
CL_FireCustomDecal

custom temporary decal
===============
*/
void CL_FireCustomDecal( int textureIndex, int entityIndex, int modelIndex, float *pos, int flags, float scale )
{
	R_DecalShoot( textureIndex, entityIndex, modelIndex, pos, flags, NULL, scale );
}

/*
===============
CL_PlayerDecal

spray custom colored decal (clan logo etc)
===============
*/
void CL_PlayerDecal( int textureIndex, int entityIndex, float *pos )
{
	R_DecalShoot( textureIndex, entityIndex, 0, pos, 0, NULL, 1.0f );
}

/*
===============
CL_DecalIndexFromName

get decal global index from decalname
===============
*/
int CL_DecalIndexFromName( const char *name )
{
	int	i;

	if( !name || !name[0] )
		return 0;

	// look through the loaded sprite name list for SpriteName
	for( i = 0; i < (MAX_DECALS - 1) && host.draw_decals[i+1][0]; i++ )
	{
		if( !Q_stricmp( name, host.draw_decals[i+1] ))
			return i+1;
	}
	return 0; // invalid decal
}

/*
===============
CL_DecalIndex

get texture index from decal global index
===============
*/
int CL_DecalIndex( int id )
{
	id = bound( 0, id, MAX_DECALS - 1 );

	host.decal_loading = true;
	if( !cl.decal_index[id] )
	{
		qboolean	load_external = false;

		if( Mod_AllowMaterials( ))
		{
			char	decalname[64];
			int	gl_texturenum = 0;

			Q_snprintf( decalname, sizeof( decalname ), "materials/decals/%s.tga", host.draw_decals[id] );

			if( FS_FileExists( decalname, false ))
				gl_texturenum = GL_LoadTexture( decalname, NULL, 0, TF_DECAL, NULL );

			if( gl_texturenum )
			{
				byte	*fin;
				mip_t	*mip;

				// find real decal dimensions and store it into texture srcWidth\srcHeight
				if(( fin = FS_LoadFile( va( "decals.wad/%s", host.draw_decals[id] ), NULL, false )) != NULL )
				{
					mip = (mip_t *)fin;
					R_GetTexture( gl_texturenum )->srcWidth = mip->width;
					R_GetTexture( gl_texturenum )->srcHeight = mip->height;
					Mem_Free( fin ); // release low-quality decal
				}

				cl.decal_index[id] = gl_texturenum;
				load_external = true; // sucessfully loaded
			}
		}

		if( !load_external ) cl.decal_index[id] = GL_LoadTexture( host.draw_decals[id], NULL, 0, TF_DECAL, NULL );
	}
	host.decal_loading = false;

	return cl.decal_index[id];
}

/*
===============
CL_DecalRemoveAll

remove all decals with specified texture
===============
*/
void CL_DecalRemoveAll( int textureIndex )
{
	int id = bound( 0, textureIndex, MAX_DECALS - 1 );	
	R_DecalRemoveAll( cl.decal_index[id] );
}

/*
==============================================================

EFRAGS MANAGEMENT

==============================================================
*/
efrag_t	cl_efrags[MAX_EFRAGS];

/*
==============
CL_ClearEfrags
==============
*/
void CL_ClearEfrags( void )
{
	int	i;

	memset( cl_efrags, 0, sizeof( cl_efrags ));

	// allocate the efrags and chain together into a free list
	clgame.free_efrags = cl_efrags;
	for( i = 0; i < MAX_EFRAGS - 1; i++ )
		clgame.free_efrags[i].entnext = &clgame.free_efrags[i+1];
	clgame.free_efrags[i].entnext = NULL;
}
	
/*
==============
CL_ClearEffects
==============
*/
void CL_ClearEffects( void )
{
	CL_ClearEfrags ();
	CL_ClearDlights ();
	CL_ClearTempEnts ();
	CL_ClearViewBeams ();
	CL_ClearParticles ();
	CL_ClearLightStyles ();
}