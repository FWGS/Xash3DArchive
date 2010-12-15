//=======================================================================
//			Copyright XashXT Group 2009 �
//		   cl_tent.c - temp entity effects management
//=======================================================================

#include "common.h"
#include "client.h"
#include "r_efx.h"
#include "event_flags.h"
#include "entity_types.h"
#include "triangleapi.h"
#include "cl_tent.h"
#include "pm_local.h"
#include "studio.h"

/*
==============================================================

TEMPENTS MANAGEMENT

==============================================================
*/
#define MAX_MUZZLEFLASH		4
#define SHARD_VOLUME		12.0f	// on shard ever n^3 units
#define SF_FUNNEL_REVERSE		1

TEMPENTITY	*cl_active_tents;
TEMPENTITY	*cl_free_tents;
TEMPENTITY	*cl_tempents = NULL;		// entities pool
int		cl_muzzleflash[MAX_MUZZLEFLASH];	// muzzle flashes

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

	// update muzzleflash indexes
	cl_muzzleflash[0] = CL_FindModelIndex( "sprites/muzzleflash1.spr" );
	cl_muzzleflash[1] = CL_FindModelIndex( "sprites/muzzleflash2.spr" );
	cl_muzzleflash[2] = CL_FindModelIndex( "sprites/muzzleflash3.spr" );
	cl_muzzleflash[3] = CL_FindModelIndex( "sprites/muzzleflash.spr" );

	if( !cl_tempents ) return;

	for( i = 0; i < GI->max_tents - 1; i++ )
	{
		cl_tempents[i].next = &cl_tempents[i+1];
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
	if( cl_tempents ) Mem_Free( cl_tempents );
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

	Mem_Set( pTemp, 0, sizeof( *pTemp ));

	// Use these to set per-frame and termination conditions / actions
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
CL_TEntPlaySound

play collide sound
==============
*/
void CL_TEntPlaySound( TEMPENTITY *pTemp, float damp )
{
	float	fvol;
	char	soundname[32];
	qboolean	isshellcasing = false;
	int	zvel;

	ASSERT( pTemp != NULL );

	switch( pTemp->hitSound )
	{
	case BOUNCE_GLASS:
		com.snprintf( soundname, sizeof( soundname ), "debris/glass%i.wav", Com_RandomLong( 1, 4 ));
		break;
	case BOUNCE_METAL:
		com.snprintf( soundname, sizeof( soundname ), "debris/metal%i.wav", Com_RandomLong( 1, 6 ));
		break;
	case BOUNCE_FLESH:
		com.snprintf( soundname, sizeof( soundname ), "debris/flesh%i.wav", Com_RandomLong( 1, 7 ));
		break;
	case BOUNCE_WOOD:
		com.snprintf( soundname, sizeof( soundname ), "debris/wood%i.wav", Com_RandomLong( 1, 4 ));
		break;
	case BOUNCE_SHRAP:
		com.snprintf( soundname, sizeof( soundname ), "weapons/ric%i.wav", Com_RandomLong( 1, 5 ));
		break;
	case BOUNCE_SHOTSHELL:
		com.snprintf( soundname, sizeof( soundname ), "weapons/sshell%i.wav", Com_RandomLong( 1, 3 ));
		isshellcasing = true; // shell casings have different playback parameters
		break;
	case BOUNCE_SHELL:
		com.snprintf( soundname, sizeof( soundname ), "player/pl_shell%i.wav", Com_RandomLong( 1, 3 ));
		isshellcasing = true; // shell casings have different playback parameters
		break;
	case BOUNCE_CONCRETE:
		com.snprintf( soundname, sizeof( soundname ), "debris/concrete%i.wav", Com_RandomLong( 1, 3 ));
		break;
	default:	// null sound
		return;
	}

	zvel = abs( pTemp->entity.baseline.origin[2] );
		
	// only play one out of every n
	if( isshellcasing )
	{	
		// play first bounce, then 1 out of 3		
		if( zvel < 200 && Com_RandomLong( 0, 3 ))
			return;
	}
	else
	{
		if( Com_RandomLong( 0, 5 )) 
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
		
		if( !Com_RandomLong( 0, 3 ) && !isshellcasing )
			pitch = Com_RandomLong( 95, 105 );
		else pitch = PITCH_NORM;

		handle = S_RegisterSound( soundname );
		S_StartSound( pTemp->entity.origin, 0, CHAN_AUTO, handle, fvol, ATTN_NORM, pitch, 0 );
	}
}

/*
==============
CL_TEntAddEntity

add entity to renderlist
==============
*/
int CL_TEntAddEntity( cl_entity_t *pEntity )
{
	ASSERT( pEntity != NULL );

	return CL_AddVisibleEntity( pEntity, ET_TEMPENTITY );
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
CL_AddTempEnts

temp-entities will be added on a user-side
setup client callback
==============
*/
void CL_AddTempEnts( void )
{
	double	ft = cl.time - cl.oldtime;
	float	gravity = clgame.movevars.gravity;

	clgame.dllFuncs.pfnTempEntUpdate( ft, cl.time, gravity, &cl_free_tents, &cl_active_tents,
		CL_TEntAddEntity, CL_TEntPlaySound );	// callbacks
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
	else xspeed = yspeed = 0.0f;	// zonly

	Mod_GetFrames( modelIndex, &frameCount );

	for( i = 0; i < count; i++ )
	{
		origin[0] = mins[0] + Com_RandomLong( 0, width - 1 );
		origin[1] = mins[1] + Com_RandomLong( 0, depth - 1 );
		origin[2] = mins[2];
		pTemp = CL_TempEntAlloc( origin, CM_ClipHandleToModel( modelIndex ));

		if ( !pTemp ) return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];

		zspeed = Com_RandomLong( 80, 140 );
		VectorSet( pTemp->entity.baseline.origin, xspeed, yspeed, zspeed );
		pTemp->die = cl.time + ( maxHeight / zspeed ) - 0.1f;
		pTemp->entity.curstate.frame = Com_RandomLong( 0, frameCount - 1 );
		// Set sprite scale
		pTemp->entity.curstate.scale = 1.0f / Com_RandomFloat( 2.0f, 5.0f );
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
		origin[0] = Com_RandomLong( mins[0], maxs[0] );
		origin[1] = Com_RandomLong( mins[1], maxs[1] );
		origin[2] = Com_RandomLong( mins[2], maxs[2] );
		pTemp = CL_TempEntAlloc( origin, CM_ClipHandleToModel( modelIndex ));
		if( !pTemp ) return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];
		angle = Com_RandomLong( -M_PI, M_PI );
		sine = com.sin( angle );
		cosine = com.cos( angle );
		
		zspeed = Com_RandomLong( 80, 140 );
		VectorSet( pTemp->entity.baseline.origin, speed * cosine, speed * sine, zspeed );
		pTemp->die = cl.time + ((height - (origin[2] - mins[2])) / zspeed) - 0.1f;
		pTemp->entity.curstate.frame = Com_RandomLong( 0, frameCount - 1 );
		
		// Set sprite scale
		pTemp->entity.curstate.scale = 1.0f / Com_RandomFloat( 4.0f, 16.0f );
		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 192;	// g-cont. why difference with FizzEffect ???		
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
		dist = Com_RandomFloat( 0, 1.0 );
		VectorLerp( start, dist, end, origin );
		pTemp = CL_TempEntAlloc( origin, CM_ClipHandleToModel( modelIndex ));
		if( !pTemp ) return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];
		angle = Com_RandomLong( -M_PI, M_PI );

		zspeed = Com_RandomLong( 80, 140 );
		VectorSet( pTemp->entity.baseline.origin, speed * com.cos( angle ), speed * com.sin( angle ), zspeed );
		pTemp->die = cl.time + (( flWaterZ - origin[2]) / zspeed ) - 0.1f;
		pTemp->entity.curstate.frame = Com_RandomLong( 0, frameCount - 1 );
		// Set sprite scale
		pTemp->entity.curstate.scale = 1.0 / Com_RandomFloat( 4, 8 );
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

	if ( client <= 0 || client > cl.maxclients )
	{
		MsgDev( D_INFO, "Bad client in AttachTentToPlayer()!\n" );
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

	pTemp = CL_TempEntAllocHigh( position, CM_ClipHandleToModel( modelIndex ));

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
		MsgDev( D_INFO, "Bad client in KillAttachedTents()!\n" );
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
	pTemp->entity.angles[ROLL] = 45 * Com_RandomLong( 0, 7 );
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
	pmodel = CM_ClipHandleToModel( modelIndex );
	if( !pmodel ) return;

	Mod_GetFrames( modelIndex, &nframeCount );

	pTemp = CL_TempEntAlloc( pos, pmodel );
	if ( !pTemp ) return;

	pTemp->frameMax = nframeCount - 1;
	pTemp->entity.curstate.rendermode = kRenderGlow;
	pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
	pTemp->entity.curstate.renderamt = 200;
	pTemp->entity.curstate.framerate = 1.0;
	pTemp->entity.curstate.frame = Com_RandomLong( 0, nframeCount - 1 );
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

	index = bound( 0, type % 10, MAX_MUZZLEFLASH - 1 );
	scale = (type / 10) * 0.1f;
	if( scale == 0.0f ) scale = 0.5f;

	modelIndex = cl_muzzleflash[index];
	if( !modelIndex ) return;

	Mod_GetFrames( modelIndex, &frameCount );

	// must set position for right culling on render
	pTemp = CL_TempEntAllocHigh( pos, CM_ClipHandleToModel( modelIndex ));
	if( !pTemp ) return;
	
	pTemp->entity.curstate.rendermode = kRenderTransAdd;
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.renderfx = 0;
	pTemp->die = cl.time + 0.01; // die at next frame
	pTemp->entity.curstate.frame = Com_RandomLong( 0, frameCount - 1 );
	pTemp->frameMax = frameCount - 1;

	if( index == 0 )
	{
		// Rifle flash
		pTemp->entity.curstate.scale = scale * Com_RandomFloat( 0.5f, 0.6f );
		pTemp->entity.angles[2] = 90 * Com_RandomLong( 0, 3 );
	}
	else
	{
		pTemp->entity.curstate.scale = scale;
		pTemp->entity.angles[2] = Com_RandomLong( 0, 359 );
	}

	CL_TEntAddEntity( &pTemp->entity );
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

	// Large, single blood sprite is a high-priority tent
	if(( pTemp = CL_TempEntAllocHigh( org, CM_ClipHandleToModel( modelIndex ))) != NULL )
	{
		int	i, frameCount;
		vec3_t	offset, dir;
		vec3_t	forward, right, up;

		colorIndex = bound( 0, colorIndex, 256 );

		Mod_GetFrames( modelIndex, &frameCount );
		pTemp->entity.curstate.rendermode = kRenderTransTexture;
		pTemp->entity.curstate.renderfx = kRenderFxClampMinScale;
		pTemp->entity.curstate.scale = Com_RandomFloat(( size / 25.0f ), ( size / 35.0f ));
		pTemp->flags = FTENT_SPRANIMATE;

		pTemp->entity.curstate.rendercolor.r = clgame.palette[colorIndex][0];
		pTemp->entity.curstate.rendercolor.g = clgame.palette[colorIndex][1];
		pTemp->entity.curstate.rendercolor.b = clgame.palette[colorIndex][2];
		pTemp->entity.curstate.framerate = frameCount * 4; // Finish in 0.250 seconds
		// play the whole thing once
		pTemp->die = cl.time + (frameCount / pTemp->entity.curstate.framerate);

		pTemp->entity.angles[2] = Com_RandomLong( 0, 360 );
		pTemp->bounceFactor = 0;

		VectorSet( forward, 0.0f, 0.0f, 1.0f );	// up-vector
		VectorVectors( forward, right, up );

		// create blood drops
		for( i = 0; i < 14; i++ )
		{
			// originate from within a circle 'scale' inches in diameter.
			VectorCopy( org, offset );
			VectorMA( offset, Com_RandomFloat( -0.5f, 0.5f ) * size, right, offset ); 
			VectorMA( offset, Com_RandomFloat( -0.5f, 0.5f ) * size, up, offset ); 

			pTemp = CL_TempEntAllocHigh( org, CM_ClipHandleToModel( modelIndex2 ));
			if( !pTemp ) return;

			pTemp->flags = FTENT_SPRANIMATELOOP|FTENT_COLLIDEWORLD|FTENT_SLOWGRAVITY;

			pTemp->entity.curstate.rendermode = kRenderTransTexture;
			pTemp->entity.curstate.renderfx = kRenderFxClampMinScale; 
			pTemp->entity.curstate.scale = Com_RandomFloat(( size / 25.0f), ( size / 35.0f ));
			pTemp->entity.curstate.rendercolor.r = clgame.palette[colorIndex][0];
			pTemp->entity.curstate.rendercolor.g = clgame.palette[colorIndex][1];
			pTemp->entity.curstate.rendercolor.b = clgame.palette[colorIndex][2];
			pTemp->entity.curstate.framerate = frameCount * 4; // Finish in 0.250 seconds
			pTemp->die = cl.time + Com_RandomFloat( 4.0f, 16.0f );

			pTemp->entity.angles[2] = Com_RandomLong( 0, 360 );
			pTemp->bounceFactor = 0;

			dir[0] = forward[0] + Com_RandomFloat( -0.3f, 0.3f );
			dir[1] = forward[1] + Com_RandomFloat( -0.3f, 0.3f );
			dir[2] = forward[2] + Com_RandomFloat( -0.3f, 0.3f );

			VectorScale( dir, Com_RandomFloat( 4.0f * size, 16.0f * size ), pTemp->entity.baseline.origin );
			pTemp->entity.baseline.origin[2] += Com_RandomFloat( 4.0f, 16.0f ) * size;
		}
	}
}

/*
==============
CL_BreakModel

Create a shards
==============
*/
void CL_BreakModel( const vec3_t pos, const vec3_t size, const vec3_t dir, float random, float life, int count, int modelIndex, char flags )
{
	int		i, frameCount;
	TEMPENTITY	*pTemp;
	char		type;

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

	// limit to 100 pieces
	if( count > 100 ) count = 100;

	for( i = 0; i < count; i++ ) 
	{
		vec3_t	vecSpot;

		// fill up the box with stuff
		vecSpot[0] = pos[0] + Com_RandomFloat( -0.5f, 0.5f ) * size[0];
		vecSpot[1] = pos[1] + Com_RandomFloat( -0.5f, 0.5f ) * size[1];
		vecSpot[2] = pos[2] + Com_RandomFloat( -0.5f, 0.5f ) * size[2];

		pTemp = CL_TempEntAlloc( vecSpot, CM_ClipHandleToModel( modelIndex ));
		if( !pTemp ) return;

		// keep track of break_type, so we know how to play sound on collision
		pTemp->hitSound = type;
		
		if( Mod_GetType( modelIndex ) == mod_sprite )
			pTemp->entity.curstate.frame = Com_RandomLong( 0, frameCount - 1 );
		else if( Mod_GetType( modelIndex ) == mod_studio )
			pTemp->entity.curstate.body = Com_RandomLong( 0, frameCount - 1 );

		pTemp->flags |= FTENT_COLLIDEWORLD | FTENT_FADEOUT | FTENT_SLOWGRAVITY;

		if ( Com_RandomLong( 0, 255 ) < 200 ) 
		{
			pTemp->flags |= FTENT_ROTATE;
			pTemp->entity.baseline.angles[0] = Com_RandomFloat( -256, 255 );
			pTemp->entity.baseline.angles[1] = Com_RandomFloat( -256, 255 );
			pTemp->entity.baseline.angles[2] = Com_RandomFloat( -256, 255 );
		}

		if (( Com_RandomLong( 0, 255 ) < 100 ) && ( flags & BREAK_SMOKE ))
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

		pTemp->entity.baseline.origin[0] = dir[0] + Com_RandomFloat( -random, random );
		pTemp->entity.baseline.origin[1] = dir[1] + Com_RandomFloat( -random, random );
		pTemp->entity.baseline.origin[2] = dir[2] + Com_RandomFloat( 0, random );

		pTemp->die = cl.time + life + Com_RandomFloat( 0.0f, 1.0f ); // Add an extra 0-1 secs of life
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

	pTemp = CL_TempEntAlloc( pos, CM_ClipHandleToModel( modelIndex ));
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
	pTemp->flags = (FTENT_COLLIDEWORLD|FTENT_FADEOUT|FTENT_GRAVITY|FTENT_ROTATE);
	pTemp->entity.baseline.angles[0] = Com_RandomFloat( -255, 255 );
	pTemp->entity.baseline.angles[1] = Com_RandomFloat( -255, 255 );
	pTemp->entity.baseline.angles[2] = Com_RandomFloat( -255, 255 );
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

	pTemp = CL_TempEntAlloc( pos, CM_ClipHandleToModel( spriteIndex ));
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

	pTemp = CL_TempEntAlloc( pos, CM_ClipHandleToModel( modelIndex ));
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

	// NOTE: Xash3D doesn't needs this stuff, because sprites
	// have right rendermodes already at loading point
	// but i'm leave it for backward compatibility
	if( flags & TE_EXPLFLAG_NOADDITIVE )
	{
		// solid sprite
		pTemp->entity.curstate.rendermode = kRenderNormal;
		pTemp->entity.curstate.renderamt = 255; 
	}
	else if( flags & TE_EXPLFLAG_DRAWALPHA )
	{
		// alpha sprite
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
		pTemp->entity.angles[2] = Com_RandomLong( 0, 360 );
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

	iColor = Com_RandomLong( 20, 35 );
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

		pTemp = CL_TempEntAlloc( pos, CM_ClipHandleToModel( modelIndex ));
		if( !pTemp ) return;

		pTemp->entity.curstate.rendermode = renderMode;
		pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
		pTemp->entity.curstate.scale = 0.5f;
		pTemp->flags |= FTENT_FADEOUT|FTENT_SLOWGRAVITY;
		pTemp->fadeSpeed = 2.0f;

		// make the spittle fly the direction indicated, but mix in some noise.
		velocity[0] = dir[0] + Com_RandomFloat( -noise, noise );
		velocity[1] = dir[1] + Com_RandomFloat( -noise, noise );
		velocity[2] = dir[2] + Com_RandomFloat( 0, znoise );
		scale = Com_RandomFloat(( speed * 0.8f ), ( speed * 1.2f ));
		VectorScale( velocity, scale, pTemp->entity.baseline.origin );
		pTemp->die = cl.time + 0.35f;
		pTemp->entity.curstate.frame = Com_RandomLong( 0, frameCount - 1 );
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

	flAmplitude /= 256.0;

	for( i = 0; i < nCount; i++ )
	{
		vec3_t	vecPos, vecVel;

		// Be careful of divide by 0 when using 'count' here...
		if( i == 0 ) VectorCopy( vecStart, vecPos );
		else VectorMA( vecStart, ( i / ( nCount - 1.0f )), vecDelta, vecPos );

		pTemp = CL_TempEntAlloc( vecPos, CM_ClipHandleToModel( modelIndex ));
		if( !pTemp ) return;

		pTemp->flags |= FTENT_COLLIDEWORLD | FTENT_SPRCYCLE | FTENT_FADEOUT | FTENT_SLOWGRAVITY;

		VectorScale( vecDir, flSpeed, vecVel );
		vecVel[0] += Com_RandomFloat( -127.0f, 128.0f ) * flAmplitude;
		vecVel[1] += Com_RandomFloat( -127.0f, 128.0f ) * flAmplitude;
		vecVel[2] += Com_RandomFloat( -127.0f, 128.0f ) * flAmplitude;
		VectorCopy( vecVel, pTemp->entity.baseline.origin );
		VectorCopy( vecPos, pTemp->entity.origin );

		pTemp->entity.curstate.scale = flSize;
		pTemp->entity.curstate.rendermode = kRenderGlow;
		pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
		pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = nRenderamt;

		pTemp->entity.curstate.frame = Com_RandomLong( 0, flFrameCount - 1 );
		pTemp->frameMax = flFrameCount - 1;
		pTemp->die = cl.time + flLife + Com_RandomFloat( 0.0f, 4.0f );
	}
}

/*
===============
CL_Large_Funnel

Create a funnel effect
===============
*/
void CL_Large_Funnel( const vec3_t pos, int flags )
{
	CL_FunnelSprite( pos, CL_FindModelIndex( "sprites/flare6.spr" ), flags );
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
			if( pTemp )
			{
				pPart = CL_AllocParticle( NULL );
				pTemp = NULL;
			}
			else
			{
				pTemp = CL_TempEntAlloc( pos, CM_ClipHandleToModel( spriteIndex ));
				pPart = NULL;
			}

			if( pTemp || pPart )
			{
				if( flags & SF_FUNNEL_REVERSE )
				{
					VectorCopy( pos, m_vecPos );

					dest[0] = pos[0] + i;
					dest[1] = pos[1] + j;
					dest[2] = pos[2] + Com_RandomFloat( 100, 800 );

					// send particle heading to dest at a random speed
					VectorSubtract( dest, m_vecPos, dir );

					vel = dest[2] / 8;// velocity based on how far particle has to travel away from org
				}
				else
				{
					m_vecPos[0] = pos[0] + i;
					m_vecPos[1] = pos[1] + j;
					m_vecPos[2] = pos[2] + Com_RandomFloat( 100, 800 );

					// send particle heading to org at a random speed
					VectorSubtract( pos, m_vecPos, dir );

					vel = m_vecPos[2] / 8;// velocity based on how far particle starts from org
				}

				flDist = VectorNormalizeLength( dir );	// save the distance
				if( vel < 64 ) vel = 64;
				
				vel += Com_RandomFloat( 64, 128  );
				life = ( flDist / vel );

				if( pTemp )
				{
					VectorCopy( m_vecPos, pTemp->entity.origin );
					VectorScale( dir, vel, pTemp->entity.baseline.origin );
					pTemp->entity.curstate.rendermode = kRenderTransAdd;
					pTemp->flags |= FTENT_FADEOUT;
					pTemp->fadeSpeed = 2.0f;
					pTemp->die = cl.time + Com_RandomFloat( life * 0.5, life );
					pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 255;
				}
				
				if( pPart )
				{
					VectorCopy( m_vecPos, pPart->org );
					pPart->color = colorIndex;
					pPart->type = pt_static;

					VectorScale( dir, vel, pPart->vel );
					// die right when you get there
					pPart->die += Com_RandomFloat( life * 0.5f, life );
				}
			}
		}
	}
}

/*
===============
CL_SparkEffect

Create a custom sparkle effect
===============
*/
void CL_SparkEffect( const vec3_t pos, int count, int velocityMin, int velocityMax )
{
	vec3_t	m_vecDir;
	model_t	*pmodel;
	int	i;

	pmodel = CM_ClipHandleToModel( CL_FindModelIndex( "sprites/richo1.spr" ));
	CL_RicochetSprite( pos, pmodel, 0.0f, Com_RandomFloat( 0.4f, 0.6f ));

	for( i = 0; i < Com_RandomLong( 1, count ); i++ )
	{
		m_vecDir[0] = Com_RandomFloat( velocityMin, velocityMax );
		m_vecDir[1] = Com_RandomFloat( velocityMin, velocityMax );
		m_vecDir[2] = Com_RandomFloat( velocityMin, velocityMax );
		VectorNormalize( m_vecDir );

		CL_SparkleTracer( pos, m_vecDir );
	}
}

/*
===============
CL_StreakSplash

Create a streak tracers
===============
*/
void CL_StreakSplash( const vec3_t pos, const vec3_t dir, int color, int count, float speed, int velMin, int velMax )
{
	int	i;

	for( i = 0; i < count; i++ )
	{
		vec3_t	vel;

		vel[0] = (dir[0] * speed) + Com_RandomFloat( velMin, velMax );
		vel[1] = (dir[1] * speed) + Com_RandomFloat( velMin, velMax );
		vel[2] = (dir[2] * speed) + Com_RandomFloat( velMin, velMax );

		CL_StreakTracer( pos, vel, color );
	}
}

/*
===============
CL_StreakSplash

Create a sparks like streaks
===============
*/
void CL_SparkStreaks( const vec3_t pos, int count, int velocityMin, int velocityMax )
{
	int	i;
	vec3_t	dir;
	float	speed = Com_RandomFloat( SPARK_ELECTRIC_MINSPEED, SPARK_ELECTRIC_MAXSPEED );

	dir[0] = Com_RandomFloat( -1.0f, 1.0f );
	dir[1] = Com_RandomFloat( -1.0f, 1.0f );
	dir[2] = Com_RandomFloat( -1.0f, 1.0f );

	for( i = 0; i < count; i++ )
	{
		vec3_t	vel;

		vel[0] = (dir[0] * speed) + Com_RandomFloat( velocityMin, velocityMax );
		vel[1] = (dir[1] * speed) + Com_RandomFloat( velocityMin, velocityMax );
		vel[2] = (dir[2] * speed) + Com_RandomFloat( velocityMin, velocityMax );

		CL_SparkleTracer( pos, vel );
	}
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
	int	iPitch = Com_RandomLong( 90, 105 );
	float	fvol = Com_RandomFloat( 0.7f, 0.9f );
	sound_t	handle;

	com.snprintf( soundpath, sizeof( soundpath ), "weapons/ric%i.wav", Com_RandomLong( 1, 5 ));
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

	pTemp = CL_TempEntAlloc( origin, CM_ClipHandleToModel( modelIndex ));
	if( !pTemp ) return;

	pfnVecToAngles( velocity, pTemp->entity.angles );
	VectorCopy( velocity, pTemp->entity.baseline.origin );

	pTemp->entity.curstate.body = 0;
	pTemp->flags = FTENT_COLLIDEALL;
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
		pTemp = CL_TempEntAlloc( pos, CM_ClipHandleToModel( modelIndex ));
		if( !pTemp ) return;

		dir[0] = forward[0] + Com_RandomFloat( -0.3f, 0.3f );
		dir[1] = forward[1] + Com_RandomFloat( -0.3f, 0.3f );
		dir[2] = forward[2] + Com_RandomFloat( -0.3f, 0.3f );

		VectorScale( dir, Com_RandomFloat( 30.0f, 40.0f ), pTemp->entity.baseline.origin );
		pTemp->entity.curstate.body = 0;
		pTemp->flags = (FTENT_COLLIDEWORLD|FTENT_SMOKETRAIL|FTENT_FLICKER|FTENT_GRAVITY|FTENT_ROTATE);
		pTemp->entity.baseline.angles[0] = Com_RandomFloat( -255, 255 );
		pTemp->entity.baseline.angles[1] = Com_RandomFloat( -255, 255 );
		pTemp->entity.baseline.angles[2] = Com_RandomFloat( -255, 255 );
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
			dl->die = cl.time + 0.01f;
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

	hSound = S_RegisterSound( "weapons/explode3.wav" );
	S_StartSound( pos, 0, CHAN_AUTO, hSound, VOL_NORM, ATTN_NORM, PITCH_NORM, 0 );
}

/*
==============
CL_MultiGunshot

Client version of shotgun shot
==============
*/
void CL_MultiGunshot( const vec3_t org, const vec3_t dir, const vec3_t noise, int count, int decalCount, int *decalIndices )
{
	// FIXME: implement
}

/*
==============
CL_FireField

Makes a field of fire
==============
*/
void CL_FireField( float *org, int radius, int modelIndex, int count, int flags, float life )
{
	// FIXME: implement
}

/*
==============
CL_Sprite_WallPuff

Create a wallpuff
==============
*/
void CL_Sprite_WallPuff( TEMPENTITY *pTemp, float scale )
{
	// FIXME: implement
}

/*
==============
CL_PlayerSprites

Unknown effect
==============
*/
void CL_PlayerSprites( int client, int modelIndex, int count, int size )
{
	// in GoldSrc any trying to create this effect will invoke immediately crash
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
	int		iSize = BF_ReadByte( msg );
	int		type, color, count, flags;
	int		decalIndex, modelIndex, entityIndex;
	float		scale, life, frameRate, vel, random;
	float		brightness, r, g, b;
	vec3_t		pos, pos2, ang;
	TEMPENTITY	*pTemp;
	cl_entity_t	*pEnt;
	dlight_t		*dl;

	// parse user message into buffer
	BF_ReadBytes( msg, pbuf, iSize );

	// init a safe tempbuffer
	BF_Init( &buf, "TempEntity", pbuf, iSize );

	type = BF_ReadByte( &buf );

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
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		CL_RicochetSound( pos );
		CL_RunParticleEffect( pos, vec3_origin, 0, 20 );
		break;
	case TE_EXPLOSION:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		modelIndex = BF_ReadShort( &buf );
		scale = (float)(BF_ReadByte( &buf ) * 0.1f);
		frameRate = BF_ReadByte( &buf );
		flags = BF_ReadByte( &buf );
		CL_Explosion( pos, modelIndex, scale, frameRate, flags );
		break;
	case TE_TAREXPLOSION:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		CL_BlobExplosion( pos );
		break;
	case TE_SMOKE:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		modelIndex = BF_ReadShort( &buf );
		scale = (float)(BF_ReadByte( &buf ) * 0.1f);
		frameRate = BF_ReadByte( &buf );
		pTemp = CL_DefaultSprite( pos, modelIndex, frameRate );
		CL_Sprite_Smoke( pTemp, scale );
		break;
	case TE_TRACER:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		pos2[0] = BF_ReadCoord( &buf );
		pos2[1] = BF_ReadCoord( &buf );
		pos2[2] = BF_ReadCoord( &buf );
		CL_TracerEffect( pos, pos2 );
		break;
	case TE_SPARKS:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		CL_SparkShower( pos );
		break;
	case TE_LAVASPLASH:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		CL_LavaSplash( pos );
		break;
	case TE_TELEPORT:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		CL_TeleportSplash( pos );
		break;
	case TE_EXPLOSION2:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		color = BF_ReadByte( &buf );
		count = BF_ReadByte( &buf );
		CL_ParticleExplosion2( pos, color, count );
		break;
	case TE_BSPDECAL:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		decalIndex = BF_ReadShort( &buf );
		entityIndex = BF_ReadShort( &buf );
		if( entityIndex ) modelIndex = BF_ReadShort( &buf );
		CL_DecalShoot( CL_DecalIndex( decalIndex ), entityIndex, modelIndex, pos, FDECAL_PERMANENT );
		break;
	case TE_IMPLOSION:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		scale = BF_ReadByte( &buf );
		count = BF_ReadByte( &buf );
		life = (float)(BF_ReadByte( &buf ) * 0.1f);
		CL_Implosion( pos, scale, count, life );
		break;
	case TE_SPRITETRAIL:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		pos2[0] = BF_ReadCoord( &buf );
		pos2[1] = BF_ReadCoord( &buf );
		pos2[2] = BF_ReadCoord( &buf );
		modelIndex = BF_ReadShort( &buf );
		count = BF_ReadByte( &buf );
		life = (float)(BF_ReadByte( &buf ) * 0.1f);
		scale = (float)(BF_ReadByte( &buf ) * 0.1f);
		vel = (float)BF_ReadByte( &buf );
		random = (float)BF_ReadByte( &buf );
		CL_Sprite_Trail( type, pos, pos2, modelIndex, count, life, scale, random, 255, vel );
		break;
	case TE_SPRITE:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		modelIndex = BF_ReadShort( &buf );
		scale = (float)(BF_ReadByte( &buf ) * 0.1f);
		brightness = (float)BF_ReadByte( &buf );

		if(( pTemp = CL_DefaultSprite( pos, modelIndex, 0 )) != NULL )
		{
			pTemp->entity.baseline.scale = scale;
			pTemp->entity.baseline.renderamt = brightness;
			pTemp->entity.curstate.renderamt = brightness;
		}
		break;
	case TE_GLOWSPRITE:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		modelIndex = BF_ReadShort( &buf );
		scale = (float)(BF_ReadByte( &buf ) * 0.1f);

		if(( pTemp = CL_DefaultSprite( pos, modelIndex, 0 )) != NULL )
		{
			pTemp->entity.curstate.rendermode = kRenderGlow;
			pTemp->entity.baseline.scale = scale;
		}
		break;
	case TE_STREAK_SPLASH:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		pos2[0] = BF_ReadCoord( &buf );
		pos2[1] = BF_ReadCoord( &buf );
		pos2[2] = BF_ReadCoord( &buf );
		color = BF_ReadByte( &buf );
		count = BF_ReadShort( &buf );
		vel = (float)BF_ReadShort( &buf );
		random = (float)BF_ReadShort( &buf );
		CL_StreakSplash( pos, pos2, color, count, vel, -random, random );
		break;
	case TE_DLIGHT:
		dl = CL_AllocDlight( 0 );
		dl->origin[0] = BF_ReadCoord( &buf );
		dl->origin[1] = BF_ReadCoord( &buf );
		dl->origin[2] = BF_ReadCoord( &buf );
		dl->radius = (float)(BF_ReadByte( &buf ) * 0.1f);
		dl->color.r = BF_ReadByte( &buf );
		dl->color.g = BF_ReadByte( &buf );
		dl->color.b = BF_ReadByte( &buf );
		brightness = (float)BF_ReadByte( &buf );
		dl->color.r *= brightness;
		dl->color.g *= brightness;
		dl->color.b *= brightness;
		dl->die = cl.time + (float)(BF_ReadByte( &buf ) * 0.1f);
		dl->decay = (float)(BF_ReadByte( &buf ) * 0.1f);
		break;
	case TE_ELIGHT:
		dl = CL_AllocElight( 0 );
		entityIndex = BF_ReadShort( &buf );
		dl->origin[0] = BF_ReadCoord( &buf );
		dl->origin[1] = BF_ReadCoord( &buf );
		dl->origin[2] = BF_ReadCoord( &buf );
		dl->radius = BF_ReadCoord( &buf );
		dl->color.r = BF_ReadByte( &buf );
		dl->color.g = BF_ReadByte( &buf );
		dl->color.b = BF_ReadByte( &buf );
		dl->die = cl.time + (float)(BF_ReadByte( &buf ) * 0.1f);
		dl->decay = BF_ReadCoord( &buf );
		break;
	case TE_TEXTMESSAGE:
		CL_ParseTextMessage( &buf );
		break;
	case TE_LINE:
	case TE_BOX:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		pos2[0] = BF_ReadCoord( &buf );
		pos2[1] = BF_ReadCoord( &buf );
		pos2[2] = BF_ReadCoord( &buf );
		life = (float)(BF_ReadShort( &buf ) * 0.1f);
		r = BF_ReadByte( &buf );
		g = BF_ReadByte( &buf );
		b = BF_ReadByte( &buf );
		if( type == TE_LINE ) CL_ParticleLine( pos, pos2, r, g, b, life );
		else CL_ParticleBox( pos, pos2, r, g, b, life );
		break;
	case TE_LARGEFUNNEL:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		modelIndex = BF_ReadShort( &buf );
		flags = BF_ReadShort( &buf );
		CL_FunnelSprite( pos, modelIndex, flags );
		break;
	case TE_BLOODSTREAM:
	case TE_BLOOD:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		pos2[0] = BF_ReadCoord( &buf );
		pos2[1] = BF_ReadCoord( &buf );
		pos2[2] = BF_ReadCoord( &buf );
		color = BF_ReadByte( &buf );
		vel = (float)BF_ReadByte( &buf );
		if( type == TE_BLOOD ) CL_Blood( pos, pos2, color, vel );
		else CL_BloodStream( pos, pos2, color, vel );
		break;
	case TE_SHOWLINE:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		pos2[0] = BF_ReadCoord( &buf );
		pos2[1] = BF_ReadCoord( &buf );
		pos2[2] = BF_ReadCoord( &buf );
		CL_ShowLine( pos, pos2 );
		break;
	case TE_DECAL:
	case TE_DECALHIGH:
	case TE_WORLDDECAL:
	case TE_WORLDDECALHIGH:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		decalIndex = BF_ReadByte( &buf );
		if( type == TE_DECAL || type == TE_DECALHIGH )
			entityIndex = BF_ReadShort( &buf );
		else entityIndex = 0;
		if( type == TE_DECALHIGH || type == TE_WORLDDECALHIGH )
			decalIndex += 256;
		pEnt = CL_GetEntityByIndex( entityIndex );
		modelIndex = (pEnt) ? pEnt->curstate.modelindex : 0;
		CL_DecalShoot( CL_DecalIndex( decalIndex ), entityIndex, modelIndex, pos, 0 );
		break;
	case TE_FIZZ:
		entityIndex = BF_ReadShort( &buf );
		modelIndex = BF_ReadShort( &buf );
		scale = BF_ReadByte( &buf );	// same as density
		pEnt = CL_GetEntityByIndex( entityIndex );
		CL_FizzEffect( pEnt, modelIndex, scale );
		break;
	case TE_MODEL:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		pos2[0] = BF_ReadCoord( &buf );
		pos2[1] = BF_ReadCoord( &buf );
		pos2[2] = BF_ReadCoord( &buf );
		VectorSet( ang, 0.0f, BF_ReadAngle( &buf ), 0.0f ); // yaw angle
		modelIndex = BF_ReadShort( &buf );
		flags = BF_ReadByte( &buf );	// sound flags
		life = (float)(BF_ReadByte( &buf ) * 0.1f);
		CL_TempModel( pos, pos2, ang, life, modelIndex, flags );
		break;
	case TE_EXPLODEMODEL:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		vel = BF_ReadCoord( &buf );
		modelIndex = BF_ReadShort( &buf );
		count = BF_ReadShort( &buf );
		life = (float)(BF_ReadByte( &buf ) * 0.1f);
		CL_TempSphereModel( pos, vel, life, count, modelIndex );
		break;
	case TE_BREAKMODEL:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		pos2[0] = BF_ReadCoord( &buf );
		pos2[1] = BF_ReadCoord( &buf );
		pos2[2] = BF_ReadCoord( &buf );
		ang[0] = BF_ReadCoord( &buf );
		ang[1] = BF_ReadCoord( &buf );
		ang[2] = BF_ReadCoord( &buf );
		random = (float)BF_ReadByte( &buf );
		modelIndex = BF_ReadShort( &buf );
		count = BF_ReadByte( &buf );
		life = (float)(BF_ReadByte( &buf ) * 0.1f);
		flags = BF_ReadByte( &buf );
		CL_BreakModel( pos, pos2, ang, random, life, count, modelIndex, (char)flags );
		break;
	case TE_GUNSHOTDECAL:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		entityIndex = BF_ReadShort( &buf );
		decalIndex = BF_ReadByte( &buf );
		pEnt = CL_GetEntityByIndex( entityIndex );
		modelIndex = (pEnt) ? pEnt->curstate.modelindex : 0;
		CL_DecalShoot( CL_DecalIndex( decalIndex ), entityIndex, modelIndex, pos, 0 );
		CL_RicochetSound( pos );
		break;
	case TE_SPRAY:
	case TE_SPRITE_SPRAY:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		pos2[0] = BF_ReadCoord( &buf );
		pos2[1] = BF_ReadCoord( &buf );
		pos2[2] = BF_ReadCoord( &buf );
		modelIndex = BF_ReadShort( &buf );
		count = BF_ReadByte( &buf );
		vel = (float)BF_ReadByte( &buf );
		random = (float)BF_ReadByte( &buf );
		if( type == TE_SPRAY )
		{
			flags = BF_ReadByte( &buf );	// rendermode
			CL_Spray( pos, pos2, modelIndex, count, vel, random, flags );
		}
		else CL_Sprite_Spray( pos, pos2, modelIndex, count, vel, random );
		break;
	case TE_ARMOR_RICOCHET:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		scale = (float)(BF_ReadByte( &buf ) * 0.1f);
		modelIndex = CL_FindModelIndex( "sprites/richo1.spr" );
		CL_RicochetSprite( pos, CM_ClipHandleToModel( modelIndex ), 0.0f, scale );
		CL_RicochetSound( pos );
		break;
	case TE_PLAYERDECAL:
		color = BF_ReadByte( &buf );	// playernum
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		entityIndex = BF_ReadShort( &buf );
		decalIndex = BF_ReadByte( &buf );
		CL_PlayerDecal( CL_DecalIndex( decalIndex ), entityIndex, pos, NULL );
		break;
	case TE_BUBBLES:
	case TE_BUBBLETRAIL:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		pos2[0] = BF_ReadCoord( &buf );
		pos2[1] = BF_ReadCoord( &buf );
		pos2[2] = BF_ReadCoord( &buf );
		scale = BF_ReadCoord( &buf );	// water height
		modelIndex = BF_ReadShort( &buf );
		count = BF_ReadByte( &buf );
		vel = BF_ReadCoord( &buf );
		if( type == TE_BUBBLES ) CL_Bubbles( pos, pos2, scale, modelIndex, count, vel );
		else CL_BubbleTrail( pos, pos2, scale, modelIndex, count, vel );
		break;
	case TE_BLOODSPRITE:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		modelIndex = BF_ReadShort( &buf );	// sprite #1
		decalIndex = BF_ReadShort( &buf );	// sprite #2
		color = BF_ReadByte( &buf );
		scale = (float)BF_ReadByte( &buf );
		CL_BloodSprite( pos, color, modelIndex, decalIndex, scale );
		break;
	case TE_PROJECTILE:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		pos2[0] = BF_ReadCoord( &buf );
		pos2[1] = BF_ReadCoord( &buf );
		pos2[2] = BF_ReadCoord( &buf );
		modelIndex = BF_ReadShort( &buf );
		life = (float)(BF_ReadByte( &buf ) * 0.1f);
		color = BF_ReadByte( &buf );	// playernum
		CL_Projectile( pos, pos2, modelIndex, life, color, NULL );
		break;
	case TE_PLAYERSPRITES:
		color = BF_ReadByte( &buf );	// playernum
		modelIndex = BF_ReadShort( &buf );
		count = BF_ReadByte( &buf );
		random = (float)BF_ReadByte( &buf );
		CL_PlayerSprites( color, modelIndex, count, random );
		break;
	case TE_PARTICLEBURST:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		scale = (float)BF_ReadShort( &buf );
		color = BF_ReadByte( &buf );
		life = (float)(BF_ReadByte( &buf ) * 0.1f);
		CL_ParticleBurst( pos, scale, color, life );
		break;
	case TE_FIREFIELD:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		scale = (float)BF_ReadShort( &buf );
		modelIndex = BF_ReadShort( &buf );
		count = BF_ReadByte( &buf );
		flags = BF_ReadByte( &buf );
		life = (float)(BF_ReadByte( &buf ) * 0.1f);
		CL_FireField( pos, scale, modelIndex, count, flags, life );
		break;
	case TE_PLAYERATTACHMENT:
		color = BF_ReadByte( &buf );	// playernum
		scale = BF_ReadCoord( &buf );	// height
		modelIndex = BF_ReadShort( &buf );
		life = (float)(BF_ReadShort( &buf ) * 0.1f);
		CL_AttachTentToPlayer( color, modelIndex, scale, life );
		break;
	case TE_KILLPLAYERATTACHMENTS:
		color = BF_ReadByte( &buf );	// playernum
		CL_KillAttachedTents( color );
		break;
	case TE_MULTIGUNSHOT:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		pos2[0] = BF_ReadCoord( &buf );
		pos2[1] = BF_ReadCoord( &buf );
		pos2[2] = BF_ReadCoord( &buf );
		ang[0] = BF_ReadCoord( &buf ) * 0.01f;
		ang[1] = BF_ReadCoord( &buf ) * 0.01f;
		ang[2] = 0.0f;
		count = BF_ReadByte( &buf );
		decalIndex = BF_ReadByte( &buf );
		CL_MultiGunshot( pos, pos2, ang, count, 1, &decalIndex );
		break;
	case TE_USERTRACER:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		pos2[0] = BF_ReadCoord( &buf );
		pos2[1] = BF_ReadCoord( &buf );
		pos2[2] = BF_ReadCoord( &buf );
		life = (float)(BF_ReadByte( &buf ) * 0.1f);
		color = BF_ReadByte( &buf );
		scale = (float)(BF_ReadByte( &buf ) * 0.1f);
		CL_UserTracerParticle( pos, pos2, life, color, scale, 0, NULL );
		break;
	default:
		MsgDev( D_ERROR, "ParseTempEntity: illegible te_message %i\n", type );
		break;
	}

	// throw warning
	if( BF_CheckOverflow( &buf )) MsgDev( D_WARN, "ParseTempEntity: overflow te_message\n" );
}


/*
==============================================================

LIGHT STYLE MANAGEMENT

==============================================================
*/
#define STYLE_LERPING_THRESHOLD	3.0f // because we wan't interpolate fast sequences (e.g. on\off)
		
/*
================
CL_ClearLightStyles
================
*/
void CL_ClearLightStyles( void )
{
	Mem_Set( cl.lightstyles, 0, sizeof( cl.lightstyles ));
}

void CL_SetLightstyle( int style, const char *s )
{
	int		i, k;
	lightstyle_t	*ls;
	float		val1, val2;

	ASSERT( s );
	ASSERT( style >= 0 && style < MAX_LIGHTSTYLES );

	ls = &cl.lightstyles[style];

	com.strncpy( ls->pattern, s, sizeof( ls->pattern ));

	ls->length = com.strlen( s );

	for( i = 0; i < ls->length; i++ )
		ls->map[i] = (float)(s[i] - 'a');

	ls->interp = true;

	// check for allow interpolate
	// NOTE: fast flickering styles looks ugly when interpolation is running
	for( k = 0; k < ls->length; k++ )
	{
		val1 = ls->map[(k+0) % ls->length];
		val2 = ls->map[(k+1) % ls->length];

		if( fabs( val1 - val2 ) > STYLE_LERPING_THRESHOLD )
		{
			ls->interp = false;
			break;
		}
	}
}

/*
==============================================================

DLIGHT MANAGEMENT

==============================================================
*/
dlight_t	cl_dlights[MAX_DLIGHTS];

/*
================
CL_ClearDlights
================
*/
void CL_ClearDlights( void )
{
	Mem_Set( cl_dlights, 0, sizeof( cl_dlights ));
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
				Mem_Set( dl, 0, sizeof( *dl ));
				dl->key = key;
				return dl;
			}
		}
	}

	// then look for anything else
	for( i = 0, dl = cl_dlights; i < MAX_DLIGHTS; i++, dl++ )
	{
		if( dl->die < cl.time )
		{
			Mem_Set( dl, 0, sizeof( *dl ));
			dl->key = key;
			return dl;
		}
	}

	// otherwise grab first dlight
	dl = &cl_dlights[0];
	Mem_Set( dl, 0, sizeof( *dl ));
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

	dl = CL_AllocDlight( key );
	dl->elight = true;

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
		if( dl->die < cl.time || !dl->radius )
			continue;
		
		dl->radius -= time * dl->decay;
		if( dl->radius < 0 ) dl->radius = 0;
	}
}

/*
================
CL_UpadteFlashlight

update client flashlight
================
*/
void CL_UpadteFlashlight( cl_entity_t *pEnt )
{
	vec3_t	vecSrc, vecEnd;
	vec3_t	forward, view_ofs;
	float	distLight;
	pmtrace_t	trace;
	dlight_t	*dl;

	if(( pEnt->index - 1 ) == cl.playernum )
	{
		// get the predicted angles
		AngleVectors( cl.refdef.cl_viewangles, forward, NULL, NULL );
	}
	else
	{
		vec3_t	v_angle;

		// restore viewangles from angles
		v_angle[PITCH] = -pEnt->angles[PITCH] * 3;
		v_angle[YAW] = pEnt->angles[YAW];
		v_angle[ROLL] = 0; 	// no roll

		AngleVectors( v_angle, forward, NULL, NULL );
	}

	VectorClear( view_ofs );

	// FIXME: grab viewheight from other place
	view_ofs[2] = 28.0f;

	if( pEnt->player )
	{
		if(( pEnt->index - 1 ) == cl.playernum )
		{
			VectorCopy( cl.predicted_viewofs, view_ofs );
		}
		else if( pEnt->curstate.usehull == 1 )
		{
			// FIXME: grab ducked viewheight from other place
			view_ofs[2] = 12.0f;
		}
	}

	VectorAdd( pEnt->origin, view_ofs, vecSrc );
	VectorMA( vecSrc, 512.0f, forward, vecEnd );

	trace = PM_PlayerTrace( clgame.pmove, vecSrc, vecEnd, PM_TRACELINE_PHYSENTSONLY, 2, -1, NULL );
	distLight = (1.0f - trace.fraction);

	// update flashlight endpos
	dl = CL_AllocDlight( pEnt->index );
	VectorCopy( trace.endpos, dl->origin );
	dl->die = cl.time + 0.001f;	// die on next frame
	dl->color.r = 255 * distLight;
	dl->color.g = 255 * distLight;
	dl->color.b = 255 * distLight;
	dl->radius = 64;
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

	if( !cl_testlights->integer ) return;
	
	for( i = 0; i < bound( 1, cl_testlights->integer, MAX_DLIGHTS ); i++ )
	{
		dl = &cl_dlights[i];

		r = 64 * ((i % 4) - 1.5f );
		f = 64 * ( i / 4) + 128;

		for( j = 0; j < 3; j++ )
			dl->origin[j] = cl.refdef.vieworg[j] + cl.refdef.forward[j] * f + cl.refdef.right[j] * r;

		dl->color.r = ((((i % 6) + 1) & 1)>>0) * 255;
		dl->color.g = ((((i % 6) + 1) & 2)>>1) * 255;
		dl->color.b = ((((i % 6) + 1) & 4)>>2) * 255;
		dl->die = cl.time + host.realtime;
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
void CL_DecalShoot( HSPRITE hDecal, int entityIndex, int modelIndex, float *pos, int flags )
{
	rgba_t	color;

	Vector4Set( color, 255, 255, 255, 255 ); // don't use custom colors
	R_DecalShoot( hDecal, entityIndex, modelIndex, pos, NULL, flags, color );
}

/*
===============
CL_PlayerDecal

spray custom colored decal (clan logo etc)
===============
*/
void CL_PlayerDecal( HSPRITE hDecal, int entityIndex, float *pos, byte *color )
{
	cl_entity_t	*pEnt;
	int		modelIndex = 0;
	rgb_t		_clr = { 255, 255, 255 };

	pEnt = CL_GetEntityByIndex( entityIndex );
	if( pEnt ) modelIndex = pEnt->curstate.modelindex;
	if( !color ) color = _clr;

	R_DecalShoot( hDecal, entityIndex, modelIndex, pos, NULL, FDECAL_CUSTOM, color );
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
	for( i = 0; i < MAX_DECALS && host.draw_decals[i+1][0]; i++ )
	{
		if( !com.stricmp( name, host.draw_decals[i+1] ))
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

	if( !cl.decal_index[id] )
		cl.decal_index[id] = GL_LoadTexture( host.draw_decals[id], NULL, 0, TF_DECAL );

	return cl.decal_index[id];
}

/*
===============
CL_DecalRemoveAll

remove all decals with specified shader
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

	Mem_Set( cl_efrags, 0, sizeof( cl_efrags ));

	// allocate the efrags and chain together into a free list
	cl.free_efrags = cl_efrags;
	for( i = 0; i < MAX_EFRAGS - 1; i++ )
		cl.free_efrags[i].entnext = &cl.free_efrags[i+1];
	cl.free_efrags[i].entnext = NULL;
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