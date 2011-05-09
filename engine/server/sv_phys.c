/*
sv_phys.c - server physic
Copyright (C) 2007 Uncle Mike

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
#include "server.h"
#include "const.h"

/*

pushmove objects do not obey gravity, and do not interact with each other or trigger fields,
but block normal movement and push normal objects when they move.

onground is set for toss objects when they come to a complete rest.  it is set for steping or walking objects

doors, plats, etc are SOLID_BSP, and MOVETYPE_PUSH
bonus items are SOLID_TRIGGER touch, and MOVETYPE_TOSS
corpses are SOLID_NOT and MOVETYPE_TOSS
crates are SOLID_BBOX and MOVETYPE_TOSS
walking monsters are SOLID_BBOX and MOVETYPE_STEP
flying/floating monsters are SOLID_BBOX and MOVETYPE_FLY

solid_edge items only clip against bsp models.
*/
#define MOVE_EPSILON	0.01f
#define MAX_CLIP_PLANES	5

/*
===============================================================================

Utility functions

===============================================================================
*/
/*
================
SV_CheckAllEnts
================
*/
void SV_CheckAllEnts( void )
{
	edict_t	*e;
	int	i;

	if( !sv_check_errors->integer || sv.state != ss_active )
		return;

	// check edicts errors
	for( i = svgame.globals->maxClients + 1; i < svgame.numEntities; i++ )
	{
		e = EDICT_NUM( i );

		if( e->free && e->pvPrivateData != NULL )
		{
			MsgDev( D_ERROR, "Freed entity %s (%i) has private data.\n", SV_ClassName( e ), i );
			continue;
		}

		if( !SV_IsValidEdict( e ))
			continue;

		if( !e->v.pContainingEntity || e->v.pContainingEntity != e )
		{
			MsgDev( D_ERROR, "Entity %s (%i) has invalid container, fixed.\n", SV_ClassName( e ), i );
			e->v.pContainingEntity = e;
			continue;
		}

		if( !e->pvPrivateData || !Mem_IsAllocatedExt( svgame.mempool, e->pvPrivateData ))
		{
			MsgDev( D_ERROR, "Entity %s (%i) trashed private data.\n", SV_ClassName( e ), i );
			e->pvPrivateData = NULL;
			continue;
		}

		SV_CheckVelocity( e );
	}
}

/*
================
SV_CheckVelocity
================
*/
void SV_CheckVelocity( edict_t *ent )
{
	int	i;

	// bound velocity
	for( i = 0; i < 3; i++ )
	{
		if( IS_NAN( ent->v.velocity[i] ))
		{
			MsgDev( D_INFO, "Got a NaN velocity on %s\n", STRING( ent->v.classname ));
			ent->v.velocity[i] = 0.0f;
		}
		if( IS_NAN( ent->v.origin[i] ))
		{
			MsgDev( D_INFO, "Got a NaN origin on %s\n", STRING( ent->v.classname ));
			ent->v.origin[i] = 0.0f;
		}

		if( ent->v.velocity[i] > sv_maxvelocity->value )
			ent->v.velocity[i] = sv_maxvelocity->value;
		else if( ent->v.velocity[i] < -sv_maxvelocity->value )
			ent->v.velocity[i] = -sv_maxvelocity->value;
	}
}

/*
================
SV_UpdateBaseVelocity
================
*/
void SV_UpdateBaseVelocity( edict_t *ent )
{
	if( ent->v.flags & FL_ONGROUND )
	{
		edict_t	*groundentity = ent->v.groundentity;

		if( SV_IsValidEdict( groundentity ))
		{
			// On conveyor belt that's moving?
			if( groundentity->v.flags & FL_CONVEYOR )
			{
				vec3_t	new_basevel;

				VectorScale( groundentity->v.movedir, groundentity->v.speed, new_basevel );
				if( ent->v.flags & FL_BASEVELOCITY )
					VectorAdd( new_basevel, ent->v.basevelocity, new_basevel );

				ent->v.flags |= FL_BASEVELOCITY;
				VectorCopy( new_basevel, ent->v.basevelocity );
			}
		}
	}
}

/*
=============
SV_RunThink

Runs thinking code if time.  There is some play in the exact time the think
function will be called, because it is called before any movement is done
in a frame.  Not used for pushmove objects, because they must be exact.
Returns false if the entity removed itself.
=============
*/
qboolean SV_RunThink( edict_t *ent )
{
	float	thinktime;

	if(!( ent->v.flags & FL_SPECTATOR ))
	{
		thinktime = ent->v.nextthink;
		if( thinktime <= 0.0f || thinktime > sv.time + host.frametime )
			return true;
		
		if( thinktime < sv.time )
			thinktime = sv.time;	// don't let things stay in the past.
						// it is possible to start that way
						// by a trigger with a local time.
		ent->v.nextthink = 0.0f;
		svgame.globals->time = thinktime;
		svgame.dllFuncs.pfnThink( ent );
	}

//	if( ent->v.flags & FL_SPECTATOR )
//		SV_FreeEdict( ent );

	return !ent->free;
}

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
void SV_Impact( edict_t *e1, trace_t *trace )
{
	edict_t	*e2 = trace->ent;

	svgame.globals->time = sv.time;

	if(( e1->v.flags|e2->v.flags ) & FL_SPECTATOR )
		return;

	if( e1->v.groupinfo && e2->v.groupinfo )
	{
		if(( !svs.groupop && !( e1->v.groupinfo & e2->v.groupinfo )) ||
		( svs.groupop == 1 && ( e1->v.groupinfo & e2->v.groupinfo )))
			return;
	}

	if( e1->v.solid != SOLID_NOT )
	{
		SV_CopyTraceToGlobal( trace );
		svgame.dllFuncs.pfnTouch( e1, e2 );
	}

	if( e2->v.solid != SOLID_NOT )
	{
		SV_CopyTraceToGlobal( trace );
		svgame.dllFuncs.pfnTouch( e2, e1 );
	}
}

/*
=============
SV_AngularMove

may use friction for smooth stopping
=============
*/
void SV_AngularMove( edict_t *ent, float frametime, float friction )
{
	int	i;
	float	adjustment;

	VectorMA( ent->v.angles, frametime, ent->v.avelocity, ent->v.angles );
	if( friction == 0.0f ) return;

	adjustment = frametime * (sv_stopspeed->value / 10) * sv_friction->value * fabs( friction );

	for( i = 0; i < 3; i++ )
	{
		if( ent->v.avelocity[i] > 0.0f )
		{
			ent->v.avelocity[i] -= adjustment;
			if( ent->v.avelocity[i] < 0.0f )
				ent->v.avelocity[i] = 0.0f;
		}
		else
		{
			ent->v.avelocity[i] += adjustment;
			if( ent->v.avelocity[i] > 0.0f )
				ent->v.avelocity[i] = 0.0f;
		}
	}
}

/*
=============
SV_LinearMove

use friction for smooth stopping
=============
*/
void SV_LinearMove( edict_t *ent, float frametime, float friction )
{
	int	i;
	float	adjustment;

	VectorMA( ent->v.origin, frametime, ent->v.velocity, ent->v.origin );
	if( friction == 0.0f ) return;

	adjustment = frametime * (sv_stopspeed->value / 10) * sv_friction->value * fabs( friction );

	for( i = 0; i < 3; i++ )
	{
		if( ent->v.velocity[i] > 0.0f )
		{
			ent->v.velocity[i] -= adjustment;
			if( ent->v.velocity[i] < 0.0f ) ent->v.velocity[i] = 0.0f;
		}
		else
		{
			ent->v.velocity[i] += adjustment;
			if( ent->v.velocity[i] > 0.0f ) ent->v.velocity[i] = 0.0f;
		}
	}
}

/*
=============
SV_CheckWater
=============
*/
qboolean SV_CheckWater( edict_t *ent )
{
	int	cont, truecont;
	vec3_t	point;

	point[0] = ent->v.origin[0] + (ent->v.mins[0] + ent->v.maxs[0]) * 0.5f;
	point[1] = ent->v.origin[1] + (ent->v.mins[1] + ent->v.maxs[1]) * 0.5f;
	point[2] = ent->v.origin[2] + ent->v.mins[2] + 1;	

	ent->v.waterlevel = 0;
	ent->v.watertype = CONTENTS_EMPTY;
	svs.groupmask = ent->v.groupinfo;
	cont = SV_PointContents( point );

	if( cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
	{
		svs.groupmask = ent->v.groupinfo;
		truecont = SV_TruePointContents( point );

		ent->v.watertype = cont;
		ent->v.waterlevel = 1;

		point[2] = ent->v.origin[2] + (ent->v.mins[2] + ent->v.maxs[2]) * 0.5f;			

		svs.groupmask = ent->v.groupinfo;
		cont = SV_PointContents( point );
		if( cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
		{
			ent->v.waterlevel = 2;
			point[2] = ent->v.origin[2] + ent->v.view_ofs[2];
			svs.groupmask = ent->v.groupinfo;
			cont = SV_PointContents( point );
			if( cont <= CONTENTS_WATER )
				ent->v.waterlevel = 3;
		}

		if( truecont <= CONTENTS_CURRENT_0 && truecont >= CONTENTS_CURRENT_DOWN )
		{
			static vec3_t current_table[] =
			{
			{ 1,  0, 0 },
			{ 0,  1, 0 },
			{-1,  0, 0 },
			{ 0, -1, 0 },
			{ 0,  0, 1 },
			{ 0,  0, -1}
			};

			float speed = 50.0f * ent->v.waterlevel;
			float *dir = current_table[CONTENTS_CURRENT_0 - truecont];

			VectorMA( ent->v.basevelocity, speed, dir, ent->v.basevelocity );
		}
	}

	return ent->v.waterlevel > 1;
}

/*
==================
SV_ClipVelocity

Slide off of the impacting object
==================
*/
int SV_ClipVelocity( vec3_t in, vec3_t normal, vec3_t out, float overbounce )
{
	float	backoff;
	float	change;
	int	i, blocked;

	blocked = 0;
	if( normal[2] > 0.0f ) blocked |= 1;	// floor
	if( !normal[2] ) blocked |= 2;	// step
	
	backoff = DotProduct( in, normal ) * overbounce;

	for( i = 0; i < 3; i++ )
	{
		change = normal[i] * backoff;
		out[i] = in[i] - change;

		if( out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON )
			out[i] = 0.0f;
	}

	return blocked;
}

/*
===============================================================================

	FLYING MOVEMENT CODE

===============================================================================
*/
/*
============
SV_FlyMove

The basic solid body movement clip that slides along multiple planes
*steptrace - if not NULL, the trace results of any vertical wall hit will be stored
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
============
*/
int SV_FlyMove( edict_t *ent, float time, trace_t *steptrace )
{
	int		i, j, numplanes, bumpcount, blocked;
	vec3_t		dir, end, planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity, original_velocity, new_velocity;
	float		d, time_left, allFraction;
	trace_t		trace;

	blocked = 0;
	VectorCopy( ent->v.velocity, original_velocity );
	VectorCopy( ent->v.velocity, primal_velocity );
	numplanes = 0;

	allFraction = 0;
	time_left = time;

	for( bumpcount = 0; bumpcount < MAX_CLIP_PLANES - 1; bumpcount++ )
	{
		if( VectorIsNull( ent->v.velocity ))
			break;

		VectorMA( ent->v.origin, time_left, ent->v.velocity, end );
		trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent );

		allFraction += trace.fraction;

		if( trace.allsolid )
		{	
			// entity is trapped in another solid
			VectorClear( ent->v.velocity );
			return 4;
		}

		if( trace.fraction > 0.0f )
		{	
			// actually covered some distance
			VectorCopy( trace.endpos, ent->v.origin );
			VectorCopy( ent->v.velocity, original_velocity );
			numplanes = 0;
		}

		if( trace.fraction == 1.0f )
			 break; // moved the entire distance

		if( !trace.ent )
			MsgDev( D_ERROR, "SV_FlyMove: trace.ent == NULL\n" );

		if( trace.plane.normal[2] > 0.7f )
		{
			blocked |= 1; // floor

         			if( trace.ent->v.solid == SOLID_BSP || trace.ent->v.solid == SOLID_SLIDEBOX ||
			trace.ent->v.movetype == MOVETYPE_PUSHSTEP || (trace.ent->v.flags & FL_CLIENT))
			{
				ent->v.flags |= FL_ONGROUND;
				ent->v.groundentity = trace.ent;
			}
		}

		if( trace.plane.normal[2] == 0.0f )
		{
			blocked |= 2; // step
			if( steptrace ) *steptrace = trace; // save for player extrafriction
		}

		// run the impact function
		SV_Impact( ent, &trace );

		// break if removed by the impact function
		if( ent->free ) break;

		time_left -= time_left * trace.fraction;

		// clipped to another plane
		if( numplanes >= MAX_CLIP_PLANES )
		{
			// this shouldn't really happen
			VectorClear( ent->v.velocity );
			break;
		}

		VectorCopy( trace.plane.normal, planes[numplanes] );
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		for( i = 0; i < numplanes; i++ )
		{
			SV_ClipVelocity( original_velocity, planes[i], new_velocity, 1.0f );
			for( j = 0; j < numplanes; j++ )
			{
				if( j != i )
				{
					if( DotProduct( new_velocity, planes[j] ) < 0.0f )
						break; // not ok
				}
			}
			if( j == numplanes )
				break;
		}

		if( i != numplanes )
		{
			// go along this plane
			VectorCopy( new_velocity, ent->v.velocity );
		}
		else
		{
			// go along the crease
			if( numplanes != 2 )
			{
				VectorClear( ent->v.velocity );
				break;
			}

			CrossProduct( planes[0], planes[1], dir );
			d = DotProduct( dir, ent->v.velocity );
			VectorScale( dir, d, ent->v.velocity );
		}

		// if current velocity is against the original velocity,
		// stop dead to avoid tiny occilations in sloping corners
		if( DotProduct( ent->v.velocity, primal_velocity ) <= 0.0f )
		{
			VectorClear( ent->v.velocity );
			break;
		}
	}

	if( allFraction == 0.0f )
		VectorClear( ent->v.velocity );

	return blocked;
}

/*
============
SV_AddGravity

============
*/
void SV_AddGravity( edict_t *ent )
{
	float	ent_gravity;

#if 0
	if( ent->v.flags & FL_ONGROUND )
		return; // already onground
#endif

	if( ent->v.gravity )
		ent_gravity = ent->v.gravity;
	else ent_gravity = 1.0f;

	// add gravity incorrectly
	ent->v.velocity[2] -= (ent_gravity * sv_gravity->value * host.frametime );
	ent->v.velocity[2] += ent->v.basevelocity[2] * host.frametime;
	ent->v.basevelocity[2] = 0.0f;

	// bound velocity
	SV_CheckVelocity( ent );
}

/*
============
SV_AddHalfGravity

============
*/
void SV_AddHalfGravity( edict_t *ent, float timestep )
{
	float	ent_gravity;

	if( ent->v.gravity )
		ent_gravity = ent->v.gravity;
	else ent_gravity = 1.0f;

	// Add 1/2 of the total gravitational effects over this timestep
	ent->v.velocity[2] -= ( 0.5f * ent_gravity * sv_gravity->value * timestep );
	ent->v.velocity[2] += ent->v.basevelocity[2] * host.frametime;
	ent->v.basevelocity[2] = 0.0f;
	
	// bound velocity
	SV_CheckVelocity( ent );
}

/*
===============================================================================

PUSHMOVE

===============================================================================
*/
/*
============
SV_AllowPushRotate

Allows to chnage entity yaw?
============
*/
qboolean SV_AllowPushRotate( edict_t *ent )
{
	model_t	*mod;

	mod = Mod_Handle( ent->v.modelindex );
	if( !mod || mod->type != mod_brush )
		return true;

	if( !sv_allow_rotate_pushables->integer )
		return false;

	return (mod->flags & MODEL_HAS_ORIGIN) ? true : false;
}

/*
============
SV_PushEntity

Does not change the entities velocity at all
============
*/
trace_t SV_PushEntity( edict_t *ent, const vec3_t lpush, const vec3_t apush, int *blocked )
{
	trace_t		trace;
	sv_client_t	*cl;
	qboolean		push = false;
	int		type;
	vec3_t		end;

	VectorAdd( ent->v.origin, lpush, end );

	if( ent->v.movetype == MOVETYPE_FLYMISSILE )
		type = MOVE_MISSILE;
	else if( ent->v.solid == SOLID_TRIGGER || ent->v.solid == SOLID_NOT )
		type = MOVE_NOMONSTERS; // only clip against bmodels
	else type = MOVE_NORMAL;

	trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, end, type, ent );

	if( sv_fix_pushents->integer )
		push = ( trace.fraction != 0.0f && !trace.allsolid ) ? true : false;
	else push = ( trace.fraction != 0.0f ) ? true : false;

	if( push )
	{
		VectorCopy( trace.endpos, ent->v.origin );

		if( apush[YAW] && ent->v.flags & FL_CLIENT && ( cl = SV_ClientFromEdict( ent, true )) != NULL )
		{
			ent->v.avelocity[1] += apush[1];
			ent->v.fixangle = 2;
		}

		// don't rotate pushables!
		if( SV_AllowPushRotate( ent ))
			ent->v.angles[YAW] += trace.fraction * apush[YAW];
	}

	SV_LinkEdict( ent, true );

	if( blocked ) *blocked = !VectorCompare( ent->v.origin, end ); // can't move full distance

	// so we can run impact function afterwards.
	if( SV_IsValidEdict( trace.ent )) SV_Impact( ent, &trace );

	return trace;
}

/*
============
SV_CanPushed

filter entities for push
============
*/
qboolean SV_CanPushed( edict_t *ent )
{
	// filter movetypes to collide with
	switch( ent->v.movetype )
	{
	case MOVETYPE_NONE:
	case MOVETYPE_PUSH:
	case MOVETYPE_FOLLOW:
	case MOVETYPE_NOCLIP:
	case MOVETYPE_COMPOUND:
		return false;
	}
	return true;
}

/*
============
SV_CanBlock

allow entity to block pusher?
============
*/
static qboolean SV_CanBlock( edict_t *ent )
{
	if( ent->v.solid == SOLID_NOT || ent->v.solid == SOLID_TRIGGER )
		return false;

	// deadbody
	if( ent->v.deadflag >= DEAD_DEAD )
		return false;

	// point entities never block push
	if( VectorCompare( ent->v.mins, ent->v.maxs ))
		return false;

	return true;
}

/*
============
SV_BuildPushList

build the list of all entities which contacted with pusher
NOTE: don't use this. This optimization is required personal linked-list in edict_t (edict->area2)
and can't working correctly with node->solid_edicts only.
============
*/
void SV_BuildPushList( areanode_t *node, edict_t *pusher, const vec3_t mins, const vec3_t maxs )
{
	link_t	*l, *next;
	edict_t	*check;
	int	oldsolid;
	qboolean	block;

	oldsolid = pusher->v.solid;

	// touch linked edicts
	for( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;
		check = EDICT_FROM_AREA( l );

		pusher->v.solid = SOLID_NOT;
		block = SV_TestEntityPosition( check, pusher );
		pusher->v.solid = oldsolid;
		if( block ) continue;

		// if the entity is standing on the pusher, it will definately be moved
		if( !(( check->v.flags & FL_ONGROUND ) && check->v.groundentity == pusher ))
		{
			if( check->v.absmin[0] >= maxs[0]
			 || check->v.absmin[1] >= maxs[1]
			 || check->v.absmin[2] >= maxs[2]
			 || check->v.absmax[0] <= mins[0]
			 || check->v.absmax[1] <= mins[1]
			 || check->v.absmax[2] <= mins[2] )
				continue;

			// see if the ent's bbox is inside the pusher's final position
			if( !SV_TestEntityPosition( check, NULL ))
				continue;
		}
#ifdef BUILD_PUSH_LIST
		if( svgame.numpushents >= MAX_PUSHED_ENTS )
		{
			MsgDev( D_ERROR, "SV_BuildPushList: overflow!\n" );
			return;
		}

		// all tests passed. add entity to pushlist
		svgame.pushlist[svgame.numpushents] = check;
		svgame.numpushents++;
#endif
	}

	// recurse down both sides
	if( node->axis == -1 ) return;

	if( maxs[node->axis] > node->dist )
		SV_BuildPushList( node->children[0], pusher, mins, maxs );
	if( mins[node->axis] < node->dist )
		SV_BuildPushList( node->children[1], pusher, mins, maxs );
}

/*
============
SV_PushMove

============
*/
static edict_t *SV_PushMove( edict_t *pusher, float movetime )
{
	int		i, e, block;
	int		num_moved, oldsolid;
	vec3_t		mins, maxs, lmove;
	sv_pushed_t	*p, *pushed_p;
	edict_t		*check;	

	if( sv.state == ss_loading || VectorIsNull( pusher->v.velocity ))
	{
		pusher->v.ltime += movetime;
		return NULL;
	}

	for( i = 0; i < 3; i++ )
	{
		lmove[i] = pusher->v.velocity[i] * movetime;
		mins[i] = pusher->v.absmin[i] + lmove[i];
		maxs[i] = pusher->v.absmax[i] + lmove[i];
	}

	pushed_p = svgame.pushed;

	// save the pusher's original position
	pushed_p->ent = pusher;
	VectorCopy( pusher->v.origin, pushed_p->origin );
	VectorCopy( pusher->v.angles, pushed_p->angles );
	pushed_p++;
	
	// move the pusher to it's final position
	SV_LinearMove( pusher, movetime, pusher->v.friction );
	SV_LinkEdict( pusher, false );
	pusher->v.ltime += movetime;
	oldsolid = pusher->v.solid;

	// non-solid pushers can't push anything
	if( pusher->v.solid == SOLID_NOT )
		return NULL;

	// see if any solid entities are inside the final position
	num_moved = 0;

#ifdef BUILD_PUSH_LIST
	svgame.numpushents = 0;
	SV_BuildPushList( sv_areanodes, pusher, mins, maxs );

	for( e = 0; e < svgame.numpushents; e++ )
	{
		check = svgame.pushlist[e];
#else
	for( e = 1; e < svgame.numEntities; e++ )
	{
		check = EDICT_NUM( e );
		if( !SV_IsValidEdict( check )) continue;

		// filter movetypes to collide with
		if( !SV_CanPushed( check ))
			continue;

		pusher->v.solid = SOLID_NOT;
		block = SV_TestEntityPosition( check, pusher );
		pusher->v.solid = oldsolid;
		if( block ) continue;

		// if the entity is standing on the pusher, it will definately be moved
		if( !(( check->v.flags & FL_ONGROUND ) && check->v.groundentity == pusher ))
		{
			if( check->v.absmin[0] >= maxs[0]
			 || check->v.absmin[1] >= maxs[1]
			 || check->v.absmin[2] >= maxs[2]
			 || check->v.absmax[0] <= mins[0]
			 || check->v.absmax[1] <= mins[1]
			 || check->v.absmax[2] <= mins[2] )
				continue;

			// see if the ent's bbox is inside the pusher's final position
			if( !SV_TestEntityPosition( check, NULL ))
				continue;
		}
#endif
		// remove the onground flag for non-players
		if( check->v.movetype != MOVETYPE_WALK )
			check->v.flags &= ~FL_ONGROUND;

		// save original position of contacted entity
		pushed_p->ent = check;
		VectorCopy( check->v.origin, pushed_p->origin );
		VectorCopy( check->v.angles, pushed_p->angles );
		pushed_p++;

		// try moving the contacted entity 
		pusher->v.solid = SOLID_NOT;
		SV_PushEntity( check, lmove, vec3_origin, &block );
		pusher->v.solid = oldsolid;

		// if it is still inside the pusher, block
		if( SV_TestEntityPosition( check, NULL ) && block )
		{	
			if( !SV_CanBlock( check ))
				continue;

			pusher->v.ltime -= movetime;

			// move back any entities we already moved
			// go backwards, so if the same entity was pushed
			// twice, it goes back to the original position
			for( p = pushed_p - 1; p >= svgame.pushed; p-- )
			{
				VectorCopy( p->origin, p->ent->v.origin );
				VectorCopy( p->angles, p->ent->v.angles );
				SV_LinkEdict( p->ent, (p->ent == check) ? true : false );
			}
			return check;
		}	
	}
	return NULL;
}

/*
============
SV_PushRotate

============
*/
static edict_t *SV_PushRotate( edict_t *pusher, float movetime )
{
	int		i, e, block, oldsolid;
	matrix4x4		start_l, end_l, temp_l;
	vec3_t		lmove, amove;
	sv_pushed_t	*p, *pushed_p;
	vec3_t		org, org2, temp;
	edict_t		*check;

	if( sv.state == ss_loading || VectorIsNull( pusher->v.avelocity ))
	{
		pusher->v.ltime += movetime;
		return NULL;
	}

	for( i = 0; i < 3; i++ )
		amove[i] = pusher->v.avelocity[i] * movetime;

	// create pusher initial position
	Matrix4x4_CreateFromEntity( temp_l, pusher->v.angles, pusher->v.origin, 1.0f );
	Matrix4x4_Invert_Simple( start_l, temp_l );

	pushed_p = svgame.pushed;

	// save the pusher's original position
	pushed_p->ent = pusher;
	VectorCopy( pusher->v.origin, pushed_p->origin );
	VectorCopy( pusher->v.angles, pushed_p->angles );
	pushed_p++;
	
	// move the pusher to it's final position
	SV_AngularMove( pusher, movetime, pusher->v.friction );
	SV_LinkEdict( pusher, false );
	pusher->v.ltime += movetime;
	oldsolid = pusher->v.solid;

	// non-solid pushers can't push anything
	if( pusher->v.solid == SOLID_NOT )
		return NULL;

	// create pusher final position
	Matrix4x4_CreateFromEntity( end_l, pusher->v.angles, pusher->v.origin, 1.0f );

#ifdef BUILD_PUSH_LIST
	svgame.numpushents = 0;
	SV_BuildPushList( sv_areanodes, pusher, pusher->v.absmin, pusher->v.absmax );

	for( e = 0; e < svgame.numpushents; e++ )
	{
		check = svgame.pushlist[e];
#else
	// see if any solid entities are inside the final position
	for( e = 1; e < svgame.numEntities; e++ )
	{
		check = EDICT_NUM( e );
		if( !SV_IsValidEdict( check )) continue;

		// filter movetypes to collide with
		if( !SV_CanPushed( check ))
			continue;

		pusher->v.solid = SOLID_NOT;
		block = SV_TestEntityPosition( check, pusher );
		pusher->v.solid = oldsolid;
		if( block ) continue;

		// if the entity is standing on the pusher, it will definately be moved
		if( !(( check->v.flags & FL_ONGROUND ) && check->v.groundentity == pusher ))
		{
			if( check->v.absmin[0] >= pusher->v.absmax[0]
			|| check->v.absmin[1] >= pusher->v.absmax[1]
			|| check->v.absmin[2] >= pusher->v.absmax[2]
			|| check->v.absmax[0] <= pusher->v.absmin[0]
			|| check->v.absmax[1] <= pusher->v.absmin[1]
			|| check->v.absmax[2] <= pusher->v.absmin[2] )
				continue;

			// see if the ent's bbox is inside the pusher's final position
			if( !SV_TestEntityPosition( check, NULL ))
				continue;
		}
#endif		
		// save original position of contacted entity
		pushed_p->ent = check;
		VectorCopy( check->v.origin, pushed_p->origin );
		VectorCopy( check->v.angles, pushed_p->angles );
		pushed_p++;

		// calculate destination position
		if( check->v.movetype == MOVETYPE_PUSHSTEP )
			VectorAverage( check->v.absmin, check->v.absmax, org );
		else VectorCopy( check->v.origin, org );

		Matrix4x4_VectorTransform( start_l, org, temp );
		Matrix4x4_VectorTransform( end_l, temp, org2 );
		VectorSubtract( org2, org, lmove );

		// try moving the contacted entity 
		pusher->v.solid = SOLID_NOT;
		SV_PushEntity( check, lmove, amove, &block );
		pusher->v.solid = oldsolid;

		// if it is still inside the pusher, block
		if( SV_TestEntityPosition( check, NULL ) && block )
		{	
			if( !SV_CanBlock( check ))
				continue;

			pusher->v.ltime -= movetime;

			// move back any entities we already moved
			// go backwards, so if the same entity was pushed
			// twice, it goes back to the original position
			for( p = pushed_p - 1; p >= svgame.pushed; p-- )
			{
				VectorCopy( p->origin, p->ent->v.origin );
				VectorCopy( p->angles, p->ent->v.angles );
				SV_LinkEdict( p->ent, (p->ent == check) ? true : false );
				p->ent->v.fixangle = 0; // FIXME: save old fixangle state into pushed array ?
			}
			return check;
		}
	}
	return NULL;
}

/*
================
SV_Physics_Pusher

================
*/
void SV_Physics_Pusher( edict_t *ent )
{
	float	oldtime, oldtime2;
	float	thinktime, movetime;
	edict_t	*pBlocker;

	pBlocker = NULL;
	oldtime = ent->v.ltime;
	thinktime = ent->v.nextthink;

	if( thinktime < ent->v.ltime + host.frametime )
	{
		movetime = thinktime - ent->v.ltime;
		if( movetime < 0.0f ) movetime = 0.0f;
	}
	else movetime = host.frametime;

	if( movetime )
	{
		if( VectorLength2( ent->v.avelocity ) > STOP_EPSILON )
		{
			if( VectorLength2( ent->v.velocity ) > STOP_EPSILON )
			{
				pBlocker = SV_PushRotate( ent, movetime );				

				if( !pBlocker )
				{
					oldtime2 = ent->v.ltime;

					// reset the local time to what it was before we rotated
					ent->v.ltime = oldtime;
					pBlocker = SV_PushMove( ent, movetime );
					if( ent->v.ltime < oldtime2 )
						ent->v.ltime = oldtime2;
				}
			}
			else
			{
				pBlocker = SV_PushRotate( ent, movetime );
			}
		}
		else 
		{
			pBlocker = SV_PushMove( ent, movetime );
		}
	}

	// if the pusher has a "blocked" function, call it
	// otherwise, just stay in place until the obstacle is gone
	if( pBlocker ) svgame.dllFuncs.pfnBlocked( ent, pBlocker );

	if( thinktime > oldtime && (( ent->v.flags & FL_ALWAYSTHINK ) || thinktime <= ent->v.ltime ))
	{
		ent->v.nextthink = 0.0f;
		svgame.globals->time = sv.time;
		svgame.dllFuncs.pfnThink( ent );
		if( ent->free ) return;
	}
}

//============================================================================
/*
=============
SV_Physics_Follow

just copy angles and origin of parent
=============
*/
void SV_Physics_Follow( edict_t *ent )
{
	edict_t	*parent;

	// regular thinking
	if( !SV_RunThink( ent )) return;

	parent = ent->v.aiment;
	if( !SV_IsValidEdict( parent )) return;

	VectorAdd( parent->v.origin, parent->v.view_ofs, ent->v.origin );
	VectorCopy( parent->v.angles, ent->v.angles );

	// noclip ents never touch triggers
	SV_LinkEdict( ent, false );
}

/*
=============
SV_Physics_Compound

a glue two entities together
=============
*/
void SV_Physics_Compound( edict_t *ent )
{
	edict_t	*parent;
	matrix4x4	start_l, end_l, temp_l;
	vec3_t	temp, org, org2, amove, lmove;
	float	movetime;
	
	// regular thinking
	if( !SV_RunThink( ent )) return;

	parent = ent->v.aiment;
	if( !SV_IsValidEdict( parent )) return;
	ent->v.solid = SOLID_NOT;

	switch( parent->v.movetype )
	{
	case MOVETYPE_PUSH:
	case MOVETYPE_PUSHSTEP:
		break;
	default: return;
	}

	// not initialized ?
	if( ent->v.ltime == 0.0f )
	{
		VectorCopy( parent->v.origin, ent->v.oldorigin );
		VectorCopy( parent->v.angles, ent->v.avelocity );
		ent->v.ltime = parent->v.ltime ? parent->v.ltime : host.frametime;
		return;
	}

	movetime = parent->v.ltime - ent->v.ltime;
	VectorScale( parent->v.velocity, movetime, lmove );
	VectorScale( parent->v.avelocity, movetime, amove );

	// create parent old position
	Matrix4x4_CreateFromEntity( temp_l, ent->v.avelocity, ent->v.oldorigin, 1.0f );
	Matrix4x4_Invert_Simple( start_l, temp_l );

	// create parent actual position
	Matrix4x4_CreateFromEntity( end_l, parent->v.angles, parent->v.origin, 1.0f );

	VectorCopy( ent->v.origin, org );
	Matrix4x4_VectorTransform( start_l, org, temp );
	Matrix4x4_VectorTransform( end_l, temp, org2 );
	VectorSubtract( org2, org, lmove );

	amove[0] = 0.0f; // don't pitch rotate

	VectorAdd( ent->v.angles, amove, ent->v.angles );
	VectorAdd( ent->v.origin, lmove, ent->v.origin );

	// noclip ents never touch triggers
	SV_LinkEdict( ent, false );

	// shuffle states
	VectorCopy( parent->v.origin, ent->v.oldorigin );
	VectorCopy( parent->v.angles, ent->v.avelocity );
	ent->v.ltime = parent->v.ltime ? parent->v.ltime : host.frametime;
}

/*
=============
SV_PhysicsNoclip

A moving object that doesn't obey physics
=============
*/
void SV_Physics_Noclip( edict_t *ent )
{
	// regular thinking
	if( !SV_RunThink( ent )) return;

	SV_CheckWater( ent );	

	VectorMA( ent->v.origin, host.frametime, ent->v.velocity,  ent->v.origin );
	VectorMA( ent->v.angles, host.frametime, ent->v.avelocity, ent->v.angles );

	// noclip ents never touch triggers
	SV_LinkEdict( ent, false );
}

/*
==============================================================================

TOSS / BOUNCE

==============================================================================
*/
/*
=============
SV_CheckWaterTransition

=============
*/
void SV_CheckWaterTransition( edict_t *ent )
{
	int	cont;

	svs.groupmask = ent->v.groupinfo;
	cont = SV_PointContents( ent->v.origin );

	if( !ent->v.watertype )
	{
		// just spawned here
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
		return;
	}

	// check if the entity crossed into or out of water
	if( ent->v.watertype <= CONTENTS_WATER ) 
	{
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
	}
	else
	{
		ent->v.watertype = CONTENTS_EMPTY;
		ent->v.waterlevel = 0;
	}
}

/*
=============
SV_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
void SV_Physics_Toss( edict_t *ent )
{
	trace_t	trace;
	vec3_t	move;
	float	backoff;

	// regular thinking
	if( !SV_RunThink( ent )) return;

	SV_CheckWaterTransition( ent );
	SV_CheckWater( ent );

	if( ent->v.velocity[2] > DIST_EPSILON || svgame.globals->changelevel )
	{
		ent->v.flags &= ~FL_ONGROUND;
		ent->v.groundentity = NULL;
          }

	// if on ground and not moving, return.
	if( ent->v.flags & FL_ONGROUND && SV_IsValidEdict( ent->v.groundentity ))
	{
		if( VectorIsNull( ent->v.basevelocity ) && VectorIsNull( ent->v.velocity ))
			return;
	}

	SV_CheckVelocity( ent );

	// add gravity
	switch( ent->v.movetype )
	{
	case MOVETYPE_FLY:
	case MOVETYPE_FLYMISSILE:
	case MOVETYPE_BOUNCEMISSILE:
		break;
	default:
		SV_AddGravity( ent );
		break;
	}

	// move angles (with friction)
	switch( ent->v.movetype )
	{
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
		SV_AngularMove( ent, host.frametime, ent->v.friction );
		break;         
	default:
		SV_AngularMove( ent, host.frametime, 0.0f );
		break;
	}

	// move origin
	// Base velocity is not properly accounted for since this entity will move again
	// after the bounce without taking it into account
	VectorAdd( ent->v.velocity, ent->v.basevelocity, ent->v.velocity );

	SV_CheckVelocity( ent );
	VectorScale( ent->v.velocity, host.frametime, move );
	VectorSubtract( ent->v.velocity, ent->v.basevelocity, ent->v.velocity );

	trace = SV_PushEntity( ent, move, vec3_origin, NULL );
	if( ent->free ) return;

	SV_CheckVelocity( ent );
#if 0
	if( trace.allsolid )
	{
		// entity is trapped in another solid
		ent->v.groundentity = trace.ent;
		ent->v.flags |= FL_ONGROUND;
		VectorClear( ent->v.velocity );
		return;
	}
#endif
	if( trace.fraction == 1.0f )
	{
		SV_CheckWater( ent );
		return;
	}

	if( ent->v.movetype == MOVETYPE_BOUNCE )
		backoff = 2.0f - ent->v.friction;
	else if( ent->v.movetype == MOVETYPE_BOUNCEMISSILE )
		backoff = 2.0f;
	else backoff = 1.0f;

	SV_ClipVelocity( ent->v.velocity, trace.plane.normal, ent->v.velocity, backoff );

	// stop if on ground
	if( trace.plane.normal[2] > 0.7f )
	{		
		float	vel;

		VectorAdd( ent->v.velocity, ent->v.basevelocity, ent->v.velocity );
		if( ent->v.velocity[2] < sv_gravity->value * host.frametime )
		{
			// we're rolling on the ground, add static friction.
			ent->v.groundentity = trace.ent;
			ent->v.flags |= FL_ONGROUND;
			ent->v.velocity[2] = 0.0f;
		}

		vel = DotProduct( ent->v.velocity, ent->v.velocity );

		if( vel < 900.0f || ( ent->v.movetype != MOVETYPE_BOUNCE && ent->v.movetype != MOVETYPE_BOUNCEMISSILE ))
		{
			ent->v.flags |= FL_ONGROUND;
			ent->v.groundentity = trace.ent;
			VectorClear( ent->v.velocity ); // avelocity will be clearing in game.dll
		}
		else
		{
			VectorScale( ent->v.velocity, (1.0f - trace.fraction) * host.frametime * 0.9f, move );
			trace = SV_PushEntity( ent, move, vec3_origin, NULL );
			if( ent->free ) return;
		}
		VectorSubtract( ent->v.velocity, ent->v.basevelocity, ent->v.velocity );
	}
	
	// check for in water
	SV_CheckWater( ent );
}

/*
===============================================================================

STEPPING MOVEMENT

===============================================================================
*/
/*
=============
SV_Physics_Step

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
=============
*/
void SV_Physics_Step( edict_t *ent )
{
	qboolean	wasonground;
	qboolean	wasinwater;
	qboolean	inwater;
	qboolean	isfalling = false;
	trace_t	trace;

	SV_CheckVelocity( ent );

	wasonground = (ent->v.flags & FL_ONGROUND) ? true : false;
	wasinwater = (ent->v.flags & FL_INWATER) ? true : false;

	// add gravity except:
	// flying monsters
	// swimming monsters who are in the water
	// pushables
	inwater = SV_CheckWater( ent );

	if( ent->v.movetype == MOVETYPE_PUSHSTEP )
	{
		if( wasinwater && !inwater )
			ent->v.velocity[2] = 0.0f;

		if( inwater && ( ent->v.flags & FL_FLOAT ))
		{
			ent->v.flags |= FL_INWATER;

			// floating pushables
			if( ent->v.waterlevel >= 2 )
			{
				VectorScale( ent->v.velocity, 1.0f - (host.frametime * 0.5f), ent->v.velocity );
				ent->v.velocity[2] += (ent->v.skin * host.frametime );
			}
			else if( ent->v.waterlevel == 0 )
			{
			}
			else
			{
				ent->v.velocity[2] -= (ent->v.skin * host.frametime);
			}
		}
		else if( !wasonground )
		{
			ent->v.flags &= ~FL_INWATER;
			SV_AddGravity( ent );
		}
	}
	else 
	{
		// monster gravity
		if( !wasonground )
		{
			if(!( ent->v.flags & FL_FLY ))
			{
				if(!( ent->v.flags & FL_SWIM && ent->v.waterlevel > 0 ))
				{
					if( !inwater )
					{
						SV_AddHalfGravity( ent, host.frametime );
						isfalling = true;
					}
				}
			}
		}
	}

	if( !VectorIsNull( ent->v.velocity ) || !VectorIsNull( ent->v.basevelocity ))
	{
		vec3_t	mins, maxs, point;
		float	friction = sv_friction->value;
		int	x, y;

		friction *= ent->v.friction;
		ent->v.flags &= ~FL_ONGROUND;
			
		// apply friction
		// let dead monsters who aren't completely onground slide
		if( wasonground )
		{
			float	speed, newspeed;
			float	*vel, control;

			vel = ent->v.velocity;
			speed = sqrt( vel[0] * vel[0] + vel[1] * vel[1] );	

			// add ground speed
			if( ent->v.groundentity )
			{
				if( ent->v.groundentity->v.flags & FL_CONVEYOR )
					speed += ent->v.groundentity->v.speed;

			}

			if( speed )
			{
				control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
				newspeed = speed - host.frametime * control * friction;

				if( newspeed < 0.0f )
					newspeed = 0.0f;
				newspeed /= speed;

				ent->v.velocity[0] *= newspeed;
				ent->v.velocity[1] *= newspeed;
			}
		}

		VectorAdd( ent->v.velocity, ent->v.basevelocity, ent->v.velocity );
		SV_FlyMove( ent, host.frametime, NULL );
		VectorSubtract( ent->v.velocity, ent->v.basevelocity, ent->v.velocity );

		SV_CheckVelocity( ent );

		// determine if it's on solid ground at all
		VectorAdd( ent->v.origin, ent->v.mins, mins );
		VectorAdd( ent->v.origin, ent->v.maxs, maxs );

		point[2] = mins[2] - 1;
		for( x = 0; x <= 1; x++ )
		{
			if( ent->v.flags & FL_ONGROUND )
				break;

			for( y = 0; y <= 1; y++ )
			{
				point[0] = x ? maxs[0] : mins[0];
				point[1] = y ? maxs[1] : mins[1];

				trace = SV_Move( point, vec3_origin, vec3_origin, point, MOVE_NORMAL, ent );			

				if( trace.startsolid )
				{
					ent->v.flags |= FL_ONGROUND;
					ent->v.groundentity = trace.ent;
					ent->v.friction = 1.0f;
					break;
				}
			}

		}
		SV_LinkEdict( ent, true );
	}
	else
	{
		if( svgame.force_retouch != 0.0f )
		{
			trace = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_NORMAL, ent );
			if(( trace.fraction < 1.0f || trace.startsolid ) && SV_IsValidEdict( trace.ent ))
				SV_Impact( ent, &trace );
		}
	}

	if(!( ent->v.flags & FL_ONGROUND ) && isfalling )
	{
		SV_AddHalfGravity( ent, host.frametime );
	}

	if( !SV_RunThink( ent )) return;

	SV_CheckWaterTransition( ent );
}

/*
=============
SV_PhysicsNone

Non moving objects can only think
=============
*/
void SV_Physics_None( edict_t *ent )
{
	SV_RunThink( ent );
}

//============================================================================
static void SV_Physics_Entity( edict_t *ent )
{
	SV_UpdateBaseVelocity( ent );

	if(!( ent->v.flags & FL_BASEVELOCITY ) && !VectorIsNull( ent->v.basevelocity ))
	{
		// Apply momentum (add in half of the previous frame of velocity first)
		VectorMA( ent->v.velocity, 1.0f + (host.frametime * 0.5f), ent->v.basevelocity, ent->v.velocity );
		VectorClear( ent->v.basevelocity );
	}
	ent->v.flags &= ~FL_BASEVELOCITY;

	if( svgame.force_retouch != 0.0f )
	{
		// force retouch even for stationary
		SV_LinkEdict( ent, true );
	}

	// user dll can override movement type (Xash3D extension)
	if( svgame.dllFuncs2.pfnPhysicsEntity )
	{
		if( svgame.dllFuncs2.pfnPhysicsEntity( ent ))
		{
			if( ent->v.flags & FL_KILLME )
				SV_FreeEdict( ent );
			return; // overrided
		}
	}

	switch( ent->v.movetype )
	{
	case MOVETYPE_NONE:
		SV_Physics_None( ent );
		break;
	case MOVETYPE_NOCLIP:
		SV_Physics_Noclip( ent );
		break;
	case MOVETYPE_FOLLOW:
		SV_Physics_Follow( ent );
		break;
	case MOVETYPE_COMPOUND:
		SV_Physics_Compound( ent );
		break;
	case MOVETYPE_STEP:
	case MOVETYPE_PUSHSTEP:
		SV_Physics_Step( ent );
		break;
	case MOVETYPE_FLY:
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
	case MOVETYPE_FLYMISSILE:
	case MOVETYPE_BOUNCEMISSILE:
		SV_Physics_Toss( ent );
		break;
	case MOVETYPE_PUSH:
		SV_Physics_Pusher( ent );
		break;
	case MOVETYPE_WALK:
		Host_Error( "SV_Physics: bad movetype %i\n", ent->v.movetype );
		break;
	}

	if( ent->v.flags & FL_KILLME )
		SV_FreeEdict( ent );
}

/*
================
SV_Physics

================
*/
void SV_Physics( void )
{
	edict_t	*ent;
	int    	i;
	
	SV_CheckAllEnts ();

	svgame.globals->time = sv.time;
	svgame.force_retouch = svgame.globals->force_retouch;
	if( svgame.force_retouch != 0.0f )
		svgame.globals->force_retouch = max( 0.0f, svgame.globals->force_retouch - 1.0f );

	// let the progs know that a new frame has started
	svgame.dllFuncs.pfnStartFrame();

	// treat each object in turn
	for( i = 0; i < svgame.numEntities; i++ )
	{
		ent = EDICT_NUM( i );
		if( !SV_IsValidEdict( ent )) continue;

		if( i > 0 && i <= svgame.globals->maxClients )
                   		continue;

		SV_Physics_Entity( ent );
	}

	// animate lightstyles (used for GetEntityIllum)
	SV_RunLightStyles ();

	// decrement svgame.numEntities if the highest number entities died
	for( ; EDICT_NUM( svgame.numEntities - 1 )->free; svgame.numEntities-- );
}