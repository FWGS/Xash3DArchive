/*
sv_world.c - world query functions
Copyright (C) 2008 Uncle Mike

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
#include "pm_local.h"

// more precision but doesn't matched with HL gameplay
// g-cont. this is pretty generic solution for maps with different hull sizes
// but in HL this got 'wrong' sizes for pushables :(
	
//#define HULL_AUTOSELECT

typedef struct moveclip_s
{
	vec3_t		boxmins, boxmaxs;	// enclose the test object along entire move
	float		*mins, *maxs;	// size of the moving object
	vec3_t		mins2, maxs2;	// size when clipping against mosnters
	const float	*start, *end;
	trace_t		trace;
	edict_t		*passedict;
	int		type;		// move type
	int		flags;		// trace flags
	int		hull;		// -1 to let entity select hull
	qboolean		isPoint;		// true is indicates pointtrace
} moveclip_t;

static int		sv_lastofs;	// lightstyles code use this

/*
===============================================================================

HULL BOXES

===============================================================================
*/

static hull_t	box_hull;
static dclipnode_t	box_clipnodes[6];
static mplane_t	box_planes[6];

/*
===================
SV_InitBoxHull

Set up the planes and clipnodes so that the six floats of a bounding box
can just be stored out and get a proper hull_t structure.
===================
*/
void SV_InitBoxHull( void )
{
	int	i, side;

	box_hull.clipnodes = box_clipnodes;
	box_hull.planes = box_planes;
	box_hull.firstclipnode = 0;
	box_hull.lastclipnode = 5;

	for( i = 0; i < 6; i++ )
	{
		box_clipnodes[i].planenum = i;
		
		side = i & 1;
		
		box_clipnodes[i].children[side] = CONTENTS_EMPTY;
		if( i != 5 ) box_clipnodes[i].children[side^1] = i + 1;
		else box_clipnodes[i].children[side^1] = CONTENTS_SOLID;
		
		box_planes[i].type = i>>1;
		box_planes[i].normal[i>>1] = 1;
	}
	
}

/*
===================
SV_HullForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
hull_t *SV_HullForBox( const vec3_t mins, const vec3_t maxs )
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = mins[2];

	return &box_hull;
}

/*
================
SV_HullForEntity

Returns a hull that can be used for testing or clipping an object of mins/maxs
size.
Offset is filled in to contain the adjustment that must be added to the
testing object's origin to get a point to use with the returned hull.
================
*/
hull_t *SV_HullForEntity( edict_t *ent, int hullNumber, vec3_t mins, vec3_t maxs, vec3_t offset )
{
	hull_t	*hull;
	model_t	*model;
	vec3_t	hullmins, hullmaxs;
	vec3_t	size;

	model = Mod_Handle( ent->v.modelindex );

	// decide which clipping hull to use, based on the size
	if( model && ( ent->v.solid == SOLID_BSP || ent->v.skin == CONTENTS_LADDER ))
	{
		// explicit hulls in the BSP model
		if( ent->v.movetype != MOVETYPE_PUSH )
			Host_Error( "SOLID_BSP without MOVETYPE_PUSH\n" );

		if( model->type != mod_brush )
			Host_Error( "MOVETYPE_PUSH with a non bsp model\n" );

		VectorSubtract( maxs, mins, size );

		if( hullNumber == -1 )
		{
#ifdef HULL_AUTOSELECT
			float	curdiff;
			float	lastdiff = 999;
			int	i;

			hullNumber = 0;	// assume we fail

			// select the hull automatically
			for( i = 0; i < 4; i++ )
			{
				curdiff = floor( VectorAvg( size )) - floor( VectorAvg( world.hull_sizes[i] ));
				curdiff = fabs( curdiff );

				if( curdiff < lastdiff )
				{
					hullNumber = i;
					lastdiff = curdiff;
				}
			}

			// TraceHull stuff
			hull = &model->hulls[hullNumber];

			// calculate an offset value to center the origin
			// NOTE: never get offset of drawing hull
			if( !hullNumber ) VectorCopy( hull->clip_mins, offset );
			else  VectorSubtract( hull->clip_mins, mins, offset );
#else
			if( size[0] <= 8.0f )
			{
				hull = &model->hulls[0];
				VectorCopy( hull->clip_mins, offset ); 
			}
			else
			{
				if( size[0] <= 36.0f )
				{
					if( size[2] <= 36.0f )
						hull = &model->hulls[3];
					else hull = &model->hulls[1];
				}
				else hull = &model->hulls[2];

				VectorSubtract( hull->clip_mins, mins, offset );
			}
#endif
		}
		else
		{
			// TraceHull stuff
			hull = &model->hulls[hullNumber];

			// calculate an offset value to center the origin
			VectorSubtract( hull->clip_mins, mins, offset );
		}
		VectorAdd( offset, ent->v.origin, offset );
	}
	else
	{
		// hullNumber is force to use hull from brushmodel (even if solid == SOLID_NOT)
		if( hullNumber != -1 && Mod_GetType( ent->v.modelindex ) == mod_brush )
		{
			model = Mod_Handle( ent->v.modelindex );
			if( !model ) Host_Error( "SV_HullForEntity: using custom hull on bad bsp model\n" );

			// TraceHull stuff
			hull = &model->hulls[hullNumber];

			// calculate an offset value to center the origin
			VectorSubtract( hull->clip_mins, mins, offset );
			VectorAdd( offset, ent->v.origin, offset );
		}
		else
		{
			// create a temp hull from bounding box sizes
			VectorSubtract( ent->v.mins, maxs, hullmins );
			VectorSubtract( ent->v.maxs, mins, hullmaxs );
			hull = SV_HullForBox( hullmins, hullmaxs );
		
			VectorCopy( ent->v.origin, offset );
		}
	}
	return hull;
}

/*
==================
SV_HullForBsp

forcing to select BSP hull
==================
*/
hull_t *SV_HullForBsp( edict_t *ent, const vec3_t mins, const vec3_t maxs, float *offset )
{
	hull_t		*hull;
	model_t		*model;
	vec3_t		size;
	float		curdiff = 0, lastdiff = 999;
	int		i = 0, hullNumber = 0;
			
	// decide which clipping hull to use, based on the size
	model = Mod_Handle( ent->v.modelindex );

	if( !model || model->type != mod_brush )
		Host_Error( "Entity %i SOLID_BSP with a non bsp model %i\n", NUM_FOR_EDICT( ent ), model->type );

	VectorSubtract( maxs, mins, size );

#ifdef HULL_AUTOSELECT	// more precision but doesn't matched with HL gameplay

	// select the hull automatically
	for( i = 0; i < 4; i++ )
	{
		curdiff = floor( VectorAvg( size )) - floor( VectorAvg( world.hull_sizes[i] ));
		curdiff = fabs( curdiff );

		if( curdiff < lastdiff )
		{
			hullNumber = i;
			lastdiff = curdiff;
		}
	}

	// TraceHull stuff
	hull = &model->hulls[hullNumber];

	// calculate an offset value to center the origin
	// NOTE: never get offset of drawing hull
	if( !hullNumber ) VectorCopy( hull->clip_mins, offset );
	else VectorSubtract( hull->clip_mins, mins, offset );
#else
	if( size[0] <= 8.0f )
	{
		hull = &model->hulls[0];
		VectorCopy( hull->clip_mins, offset ); 
	}
	else
	{
		if( size[0] <= 36.0f )
		{
			if( size[2] <= 36.0f )
				hull = &model->hulls[3];
			else hull = &model->hulls[1];
		}
		else hull = &model->hulls[2];

		VectorSubtract( hull->clip_mins, mins, offset );
	}
#endif
	VectorAdd( offset, ent->v.origin, offset );

	return hull;
}

/*
===============================================================================

ENTITY AREA CHECKING

===============================================================================
*/

areanode_t	sv_areanodes[AREA_NODES];
int		sv_numareanodes;

/*
===============
SV_CreateAreaNode

builds a uniformly subdivided tree for the given world size
===============
*/
areanode_t *SV_CreateAreaNode( int depth, vec3_t mins, vec3_t maxs )
{
	areanode_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1;
	vec3_t		mins2, maxs2;

	anode = &sv_areanodes[sv_numareanodes++];

	ClearLink( &anode->trigger_edicts );
	ClearLink( &anode->solid_edicts );
	ClearLink( &anode->water_edicts );
	
	if( depth == AREA_DEPTH )
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}
	
	VectorSubtract( maxs, mins, size );
	if( size[0] > size[1] )
		anode->axis = 0;
	else anode->axis = 1;
	
	anode->dist = 0.5f * ( maxs[anode->axis] + mins[anode->axis] );
	VectorCopy( mins, mins1 );	
	VectorCopy( mins, mins2 );	
	VectorCopy( maxs, maxs1 );	
	VectorCopy( maxs, maxs2 );	
	
	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;
	anode->children[0] = SV_CreateAreaNode( depth+1, mins2, maxs2 );
	anode->children[1] = SV_CreateAreaNode( depth+1, mins1, maxs1 );

	return anode;
}

/*
===============
SV_ClearWorld

===============
*/
void SV_ClearWorld( void )
{
	int	i;

	SV_InitBoxHull();		// for box testing
	SV_InitStudioHull();	// for hitbox testing

	// clear lightstyles
	for( i = 0; i < MAX_LIGHTSTYLES; i++ )
		sv.lightstyles[i].value = 256.0f;
	sv_lastofs = -1;

	Q_memset( sv_areanodes, 0, sizeof( sv_areanodes ));
	sv_numareanodes = 0;

	SV_CreateAreaNode( 0, sv.worldmodel->mins, sv.worldmodel->maxs );
}

/*
===============
SV_UnlinkEdict
===============
*/
void SV_UnlinkEdict( edict_t *ent )
{
	// not linked in anywhere
	if( !ent->area.prev ) return;

	RemoveLink( &ent->area );
	ent->area.prev = NULL;
	ent->area.next = NULL;
}

/*
====================
SV_TouchLinks
====================
*/
void SV_TouchLinks( edict_t *ent, areanode_t *node )
{
	link_t	*l, *next;
	edict_t	*touch;
	hull_t	*hull;
	vec3_t	test;

	// touch linked edicts
	for( l = node->trigger_edicts.next; l != &node->trigger_edicts; l = next )
	{
		next = l->next;
		touch = EDICT_FROM_AREA( l );
#if 0
		if( touch->v.solid >= SOLID_BBOX )
		{
			// did you forget call SV_LinkEdict ?
			SV_LinkEdict( touch, false );
			continue;			
		}
#endif
		if( touch->v.groupinfo && ent->v.groupinfo )
		{
			if(( !svs.groupop && !(touch->v.groupinfo & ent->v.groupinfo)) ||
			(svs.groupop == 1 && (touch->v.groupinfo & ent->v.groupinfo)))
				continue;
		}

		if( touch == ent || touch->v.solid != SOLID_TRIGGER ) // disabled ?
			continue;

		if( !BoundsIntersect( ent->v.absmin, ent->v.absmax, touch->v.absmin, touch->v.absmax ))
			continue;

		// check brush triggers accuracy
		if( Mod_GetType( touch->v.modelindex ) == mod_brush )
		{
			// force to select bsp-hull
			hull = SV_HullForBsp( touch, ent->v.mins, ent->v.maxs, test );

			// offset the test point appropriately for this hull.
			VectorSubtract( ent->v.origin, test, test );

			// test hull for intersection with this model
			if( PM_HullPointContents( hull, hull->firstclipnode, test ) == CONTENTS_EMPTY )
				continue;
		}

      		svgame.globals->time = sv.time;
		svgame.dllFuncs.pfnTouch( touch, ent );
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;
	
	if( ent->v.absmax[node->axis] > node->dist )
		SV_TouchLinks( ent, node->children[0] );
	if( ent->v.absmin[node->axis] < node->dist )
		SV_TouchLinks( ent, node->children[1] );
}

/*
===============
SV_FindTouchedLeafs

===============
*/
void SV_FindTouchedLeafs( edict_t *ent, mnode_t *node )
{
	mplane_t	*splitplane;
	int	sides, leafnum;
	mleaf_t	*leaf;

	if( node->contents == CONTENTS_SOLID )
		return;
	
	// add an efrag if the node is a leaf
	if(  node->contents < 0 )
	{
		// get headnode
		if( ent->headnode == -1 )
			ent->headnode = node - sv.worldmodel->nodes;

		if( ent->num_leafs >= MAX_ENT_LEAFS )
		{
			// continue counting leafs,
			// so we know how many it's overrun
			ent->num_leafs++;
			return;
		}

		leaf = (mleaf_t *)node;
		leafnum = leaf - sv.worldmodel->leafs - 1;

		ent->leafnums[ent->num_leafs] = leafnum;
		ent->num_leafs++;			
		return;
	}
	
	// NODE_MIXED
	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE( ent->v.absmin, ent->v.absmax, splitplane );

	if( sides == 3 )
	{
		// get headnode
		if( ent->headnode == -1 )
			ent->headnode = node - sv.worldmodel->nodes;
	}
	
	// recurse down the contacted sides
	if( sides & 1 ) SV_FindTouchedLeafs( ent, node->children[0] );
	if( sides & 2 ) SV_FindTouchedLeafs( ent, node->children[1] );
}

/*
=============
SV_HeadnodeVisible
=============
*/
qboolean SV_HeadnodeVisible( mnode_t *node, byte *visbits )
{
	mleaf_t	*leaf;
	int	leafnum;

	if( node->contents < 0 )
	{
		if( node->contents != CONTENTS_SOLID )
		{
			leaf = (mleaf_t *)node;
			leafnum = leaf - sv.worldmodel->leafs - 1;

			if( visbits[leafnum >> 3] & (1<<( leafnum & 7 )))
				return true;
		}
		return false;
	}
	
	if( SV_HeadnodeVisible( node->children[0], visbits ))
		return true;
	return SV_HeadnodeVisible( node->children[1], visbits );
}

/*
===============
SV_LinkEdict
===============
*/
void SV_LinkEdict( edict_t *ent, qboolean touch_triggers )
{
	areanode_t	*node;

	if( ent->area.prev ) SV_UnlinkEdict( ent );	// unlink from old position
	if( ent == svgame.edicts ) return;		// don't add the world
	if( !SV_IsValidEdict( ent )) return;		// never add freed ents

	// set the abs box
	svgame.dllFuncs.pfnSetAbsBox( ent );

	if( ent->v.movetype == MOVETYPE_FOLLOW && SV_IsValidEdict( ent->v.aiment ))
	{
		ent->headnode = ent->v.aiment->headnode;
		ent->num_leafs = ent->v.aiment->num_leafs;
		Q_memcpy( ent->leafnums, ent->v.aiment->leafnums, sizeof( ent->leafnums ));
	}
	else
	{
		// link to PVS leafs
		ent->num_leafs = 0;

		if( ent->v.modelindex )
		{
			ent->headnode = -1;
			SV_FindTouchedLeafs( ent, sv.worldmodel->nodes );

			if( ent->num_leafs > MAX_ENT_LEAFS )
				ent->num_leafs = 0;		// so we use headnode instead
			else ent->headnode = -1;		// use normal leafs check
		}
	}

	// ignore not solid bodies (but pass allowed for push)
	if( ent->v.solid == SOLID_NOT && ent->v.skin == CONTENTS_NONE )
		return;

	// find the first node that the ent's box crosses
	node = sv_areanodes;

	while( 1 )
	{
		if( node->axis == -1 ) break;
		if( ent->v.absmin[node->axis] > node->dist )
			node = node->children[0];
		else if( ent->v.absmax[node->axis] < node->dist )
			node = node->children[1];
		else break; // crosses the node
	}
	
	// link it in	
	if( ent->v.solid == SOLID_TRIGGER )
		InsertLinkBefore( &ent->area, &node->trigger_edicts );
	else if( ent->v.solid == SOLID_NOT && ent->v.skin != CONTENTS_NONE )
		InsertLinkBefore( &ent->area, &node->water_edicts );
	else InsertLinkBefore( &ent->area, &node->solid_edicts );

	if( touch_triggers ) SV_TouchLinks( ent, sv_areanodes );
}

/*
===============================================================================

POINT TESTING IN HULLS

===============================================================================
*/
void SV_WaterLinks( const vec3_t origin, int *pCont, areanode_t *node )
{
	link_t	*l, *next;
	edict_t	*touch;
	hull_t	*hull;
	vec3_t	test;

	// get water edicts
	for( l = node->water_edicts.next; l != &node->water_edicts; l = next )
	{
		next = l->next;
		touch = EDICT_FROM_AREA( l );

		if( touch->v.solid != SOLID_NOT ) // disabled ?
			continue;

		if( touch->v.groupinfo )
		{
			if(( !svs.groupop && !(touch->v.groupinfo & svs.groupmask)) ||
			(svs.groupop == 1 && (touch->v.groupinfo & svs.groupmask)))
				continue;
		}

		// only brushes can have special contents
		if( Mod_GetType( touch->v.modelindex ) != mod_brush )
			continue;

		if( !BoundsIntersect( origin, origin, touch->v.absmin, touch->v.absmax ))
			continue;

		// check water brushes accuracy
		hull = SV_HullForBsp( touch, vec3_origin, vec3_origin, test );

		// offset the test point appropriately for this hull.
		VectorSubtract( origin, test, test );

		// test hull for intersection with this model
		if( PM_HullPointContents( hull, hull->firstclipnode, test ) == CONTENTS_EMPTY )
			continue;

		// compare contents ranking
		if( RankForContents( touch->v.skin ) > RankForContents( *pCont ))
			*pCont = touch->v.skin; // new content has more priority
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;
	
	if( origin[node->axis] > node->dist )
		SV_WaterLinks( origin, pCont, node->children[0] );
	if( origin[node->axis] < node->dist )
		SV_WaterLinks( origin, pCont, node->children[1] );
}

/*
=============
SV_TruePointContents

=============
*/
int SV_TruePointContents( const vec3_t p )
{
	int	cont;

	// sanity check
	if( !p ) return CONTENTS_NONE;

	// get base contents from world
	cont = PM_HullPointContents( &sv.worldmodel->hulls[0], 0, p );

	// check all water entities
	SV_WaterLinks( p, &cont, sv_areanodes );

	return cont;
}

/*
=============
SV_PointContents

=============
*/
int SV_PointContents( const vec3_t p )
{
	int cont = SV_TruePointContents( p );

	if( cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN )
		cont = CONTENTS_WATER;
	return cont;
}

//===========================================================================

/*
============
SV_TestEntityPosition

returns true if the entity is in solid currently
============
*/
qboolean SV_TestEntityPosition( edict_t *ent, edict_t *blocker )
{
	trace_t	trace;
#if 1
	if( ent->v.flags & (FL_CLIENT|FL_FAKECLIENT))
	{
		// to avoid falling through tracktrain update client mins\maxs here
		if( ent->v.flags & FL_DUCKING ) 
			SV_SetMinMaxSize( ent, svgame.pmove->player_mins[1], svgame.pmove->player_maxs[1] );
		else SV_SetMinMaxSize( ent, svgame.pmove->player_mins[0], svgame.pmove->player_maxs[0] );
	}
#endif
	trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_NORMAL|FMOVE_SIMPLEBOX, ent );

	if( SV_IsValidEdict( blocker ) && SV_IsValidEdict( trace.ent ))
	{
		if( trace.ent->v.movetype == MOVETYPE_PUSH || trace.ent == blocker )
			return trace.startsolid;
		return false;
	}
	return trace.startsolid;
}

/*
============
SV_TestPlayerPosition

same as SV_TestEntityPosition but check only players
============
*/
qboolean SV_TestPlayerPosition( edict_t *ent )
{
	if( ent->v.flags & (FL_CLIENT|FL_FAKECLIENT))
		return SV_TestEntityPosition( ent, NULL );
	return false;
}

/*
===============================================================================

LINE TESTING IN HULLS

===============================================================================
*/
/*
==================
SV_RecursiveHullCheck

==================
*/
qboolean SV_RecursiveHullCheck( hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t *trace )
{
	dclipnode_t	*node;
	mplane_t		*plane;
	float		t1, t2;
	float		frac, midf;
	int		side;
	vec3_t		mid;

	// check for empty
	if( num < 0 )
	{
		if( num != CONTENTS_SOLID )
		{
			trace->allsolid = false;
			if( num == CONTENTS_EMPTY )
				trace->inopen = true;
			else trace->inwater = true;
		}
		else trace->startsolid = true;
		return true; // empty
	}

	if( num < hull->firstclipnode || num > hull->lastclipnode )
		Sys_Error( "SV_RecursiveHullCheck: bad node number\n" );

	// find the point distances
	node = hull->clipnodes + num;
	plane = hull->planes + node->planenum;

	if( plane->type < 3 )
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	}
	else
	{
		t1 = DotProduct( plane->normal, p1 ) - plane->dist;
		t2 = DotProduct( plane->normal, p2 ) - plane->dist;
	}
	
	if( t1 >= 0 && t2 >= 0 )
		return SV_RecursiveHullCheck( hull, node->children[0], p1f, p2f, p1, p2, trace );
	if( t1 < 0 && t2 < 0 )
		return SV_RecursiveHullCheck( hull, node->children[1], p1f, p2f, p1, p2, trace );

	// put the crosspoint DIST_EPSILON pixels on the near side
	if( t1 < 0 ) frac = ( t1 + DIST_EPSILON ) / ( t1 - t2 );
	else frac = ( t1 - DIST_EPSILON ) / ( t1 - t2 );

	if( frac < 0.0f ) frac = 0.0f;
	if( frac > 1.0f ) frac = 1.0f;
		
	midf = p1f + ( p2f - p1f ) * frac;
	VectorLerp( p1, frac, p2, mid );

	side = (t1 < 0);

	// move up to the node
	if( !SV_RecursiveHullCheck( hull, node->children[side], p1f, midf, p1, mid, trace ))
		return false;

	if( PM_HullPointContents( hull, node->children[side^1], mid ) != CONTENTS_SOLID )
	{
		// go past the node
		return SV_RecursiveHullCheck (hull, node->children[side^1], midf, p2f, mid, p2, trace);
	}	

	if( trace->allsolid )
		return false; // never got out of the solid area
		
	//==================
	// the other side of the node is solid, this is the impact point
	//==================
	if( !side )
	{
		VectorCopy( plane->normal, trace->plane.normal );
		trace->plane.dist = plane->dist;
	}
	else
	{
		VectorNegate( plane->normal, trace->plane.normal );
		trace->plane.dist = -plane->dist;
	}

	while( PM_HullPointContents( hull, hull->firstclipnode, mid ) == CONTENTS_SOLID )
	{
		// shouldn't really happen, but does occasionally
		frac -= 0.1f;

		if( frac < 0.0f )
		{
			trace->fraction = midf;
			VectorCopy( mid, trace->endpos );
			MsgDev( D_WARN, "trace backed up past 0.0\n" );
			return false;
		}

		midf = p1f + (p2f - p1f) * frac;
		VectorLerp( p1, frac, p2, mid );
	}

	trace->fraction = midf;
	VectorCopy( mid, trace->endpos );

	return false;
}

/*
==================
SV_TraceHull

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
trace_t SV_TraceHull( edict_t *ent, int hullNum, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end )
{
	trace_t	trace;
	matrix4x4	matrix;
	vec3_t	offset, temp;
	vec3_t	start_l, end_l;
	hull_t	*hull;

	// fill in a default trace
	Q_memset( &trace, 0, sizeof( trace_t ));
	VectorCopy( end, trace.endpos );
	trace.fraction = 1.0f;
	trace.allsolid = true;
	trace.hitgroup = -1;

	// get the clipping hull
	hull = SV_HullForEntity( ent, hullNum, mins, maxs, offset );

	VectorSubtract( start, offset, start_l );
	VectorSubtract( end, offset, end_l );

	// rotate start and end into the models frame of reference
	if( ent->v.solid == SOLID_BSP && !VectorIsNull( ent->v.angles ))
	{
		matrix4x4		imatrix;
	
		Matrix4x4_CreateFromEntity( matrix, ent->v.angles, offset, 1.0f );
		Matrix4x4_Invert_Simple( imatrix, matrix );

		Matrix4x4_VectorTransform( imatrix, start, start_l );
		Matrix4x4_VectorTransform( imatrix, end, end_l );
	}

	// trace a line through the apropriate clipping hull
	SV_RecursiveHullCheck( hull, hull->firstclipnode, 0.0f, 1.0f, start_l, end_l, &trace );

	// rotate endpos back to world frame of reference
	if( ent->v.solid == SOLID_BSP && !VectorIsNull( ent->v.angles ))
	{
		if( trace.fraction != 1.0f )
		{
			// compute endpos
			VectorLerp( start, trace.fraction, end, trace.endpos );

			VectorCopy( trace.plane.normal, temp );
			Matrix4x4_TransformPositivePlane( matrix, temp, trace.plane.dist,
				trace.plane.normal, &trace.plane.dist );
		}
	}
	else
	{
		// special case for non-rotated bmodels
		// fix trace up by the offset
		if( trace.fraction != 1.0f )
			VectorAdd( trace.endpos, offset, trace.endpos );

		trace.plane.dist = DotProduct( trace.endpos, trace.plane.normal );
	}

	// did we clip the move?
	if( trace.fraction < 1.0f || trace.startsolid )
		trace.ent = ent;

	return trace;
}

/*
==================
SV_RecursiveSurfCheck

==================
*/
msurface_t *SV_RecursiveSurfCheck( model_t *model, mnode_t *node, vec3_t p1, vec3_t p2 )
{
	float		t1, t2, frac;
	int		side, ds, dt;
	mplane_t		*plane;
	msurface_t	*surf;
	vec3_t		mid;
	int		i;

	if( node->contents < 0 )
		return NULL;

	plane = node->plane;

	if( plane->type < 3 )
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	}
	else
	{
		t1 = DotProduct( plane->normal, p1 ) - plane->dist;
		t2 = DotProduct( plane->normal, p2 ) - plane->dist;
	}

	if( t1 >= 0 && t2 >= 0 )
		return SV_RecursiveSurfCheck( model, node->children[0], p1, p2 );
	if( t1 < 0 && t2 < 0 )
		return SV_RecursiveSurfCheck( model, node->children[1], p1, p2 );

	frac = t1 / ( t1 - t2 );

	if( frac < 0.0f ) frac = 0.0f;
	if( frac > 1.0f ) frac = 1.0f;

	VectorLerp( p1, frac, p2, mid );

	side = (t1 < 0);

	// now this is weird.
	surf = SV_RecursiveSurfCheck( model, node->children[side], p1, mid );

	if( surf != NULL || ( t1 >= 0 && t2 >= 0 ) || ( t1 < 0 && t2 < 0 ))
	{
		return surf;
	}

	surf = model->surfaces + node->firstsurface;

	for( i = 0; i < node->numsurfaces; i++, surf++ )
	{
		ds = (int)((float)DotProduct( mid, surf->texinfo->vecs[0] ) + surf->texinfo->vecs[0][3] );
		dt = (int)((float)DotProduct( mid, surf->texinfo->vecs[1] ) + surf->texinfo->vecs[1][3] );

		if( ds >= surf->texturemins[0] && dt >= surf->texturemins[1] )
		{
			int	s = ds - surf->texturemins[0];
			int	t = dt - surf->texturemins[1];

			if( s <= surf->extents[0] && t <= surf->extents[1] )
				return surf;
		}
	}

	return SV_RecursiveSurfCheck( model, node->children[side^1], mid, p2 );
}

/*
==================
SV_TraceTexture

find the face where the traceline hit
assume pTextureEntity is valid
==================
*/
const char *SV_TraceTexture( edict_t *ent, const vec3_t start, const vec3_t end )
{
	msurface_t	*surf;
	matrix4x4		matrix;
	model_t		*bmodel;
	hull_t		*hull;
	vec3_t		start_l, end_l;
	vec3_t		offset;

	bmodel = Mod_Handle( ent->v.modelindex );
	if( !bmodel || bmodel->type != mod_brush )
		return NULL;

	hull = SV_HullForBsp( ent, vec3_origin, vec3_origin, offset );

	VectorSubtract( start, offset, start_l );
	VectorSubtract( end, offset, end_l );

	// rotate start and end into the models frame of reference
	if( !VectorIsNull( ent->v.angles ))
	{
		matrix4x4	imatrix;

		Matrix4x4_CreateFromEntity( matrix, ent->v.angles, offset, 1.0f );
		Matrix4x4_Invert_Simple( imatrix, matrix );

		Matrix4x4_VectorTransform( imatrix, start, start_l );
		Matrix4x4_VectorTransform( imatrix, end, end_l );
	}

	surf = SV_RecursiveSurfCheck( bmodel, &bmodel->nodes[hull->firstclipnode], start_l, end_l );

	if( !surf || !surf->texinfo || !surf->texinfo->texture )
		return NULL;

	return surf->texinfo->texture->name;
}

/*
====================
SV_ClipToLinks

Mins and maxs enclose the entire area swept by the move
====================
*/
static void SV_ClipToLinks( areanode_t *node, moveclip_t *clip )
{
	link_t	*l, *next;
	edict_t	*touch;
	model_t	*model;
	trace_t	trace;
	qboolean	bSimpleBox;
	float	*mins, *maxs;
	int	modType;

	// touch linked edicts
	for( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;

		touch = EDICT_FROM_AREA( l );

		if( touch->v.groupinfo != 0 && clip->passedict != NULL && clip->passedict->v.groupinfo != 0 )
		{
			if(( svs.groupop == 0 && ( touch->v.groupinfo & clip->passedict->v.groupinfo ) == 0) ||
			( svs.groupop == 1 && (touch->v.groupinfo & clip->passedict->v.groupinfo ) != 0 ))
				continue;
		}

		if( touch == clip->passedict || touch->v.solid == SOLID_NOT )
			continue;

		if( touch->v.solid == SOLID_TRIGGER )
			Host_Error( "trigger in clipping list\n" );

		// custom user filter
		if( svgame.dllFuncs2.pfnShouldCollide )
		{
			if( !svgame.dllFuncs2.pfnShouldCollide( touch, clip->passedict ))
				return;
		}

		modType = Mod_GetType( touch->v.modelindex );

		// monsterclip filter
		if( modType == mod_brush && ( touch->v.flags & FL_MONSTERCLIP ))
		{
			if( clip->passedict && clip->passedict->v.flags & FL_MONSTERCLIP );
			else continue;
		}

		// completely ignore all edicts but brushes
		if( clip->type == MOVE_NOMONSTERS && modType != mod_brush )
			continue;

		if( clip->type == MOVE_WORLDONLY )
		{
			// accept only real bsp models with FL_WORLDBRUSH set
			if( modType == mod_brush && touch->v.flags & FL_WORLDBRUSH );
			else continue;
		}

		if( !BoundsIntersect( clip->boxmins, clip->boxmaxs, touch->v.absmin, touch->v.absmax ))
			continue;

		if( clip->passedict && clip->passedict->v.solid == SOLID_TRIGGER )
		{
			// never collide items and player (because call "give" always stuck item in player
			// and total trace returns fail (old half-life bug)
			// items touch should be done in SV_TouchLinks not here
			if( touch->v.flags & ( FL_CLIENT|FL_FAKECLIENT ))
				continue;
		}

		if( clip->passedict && !VectorIsNull( clip->passedict->v.size ) && VectorIsNull( touch->v.size ))
			continue;	// points never interact

		if( clip->flags & FMOVE_IGNORE_GLASS && modType == mod_brush )
		{
			// we ignore brushes with rendermode != kRenderNormal and without FL_WORLDBRUSH set
			if( touch->v.rendermode != kRenderNormal && !(touch->v.flags & FL_WORLDBRUSH))
				continue;
		}

		// might intersect, so do an exact clip
		if( clip->trace.allsolid ) return;

		if( clip->passedict )
		{
		 	if( touch->v.owner == clip->passedict )
				continue;	// don't clip against own missiles
			if( clip->passedict->v.owner == touch )
				continue;	// don't clip against owner
		}

		model = Mod_Handle( touch->v.modelindex );

		bSimpleBox = (clip->flags & FMOVE_SIMPLEBOX) ? true : false;
		bSimpleBox = World_UseSimpleBox( bSimpleBox, touch->v.solid, clip->isPoint, model );

		if( touch->v.flags & FL_MONSTER )
		{
			mins = clip->mins2;
			maxs = clip->maxs2;
		}
		else
		{
			mins = clip->mins;
			maxs = clip->maxs;
		}

		// can't trace dead bodies as hull, only traceline (for damage)
		if( touch->v.deadflag == DEAD_DEAD && !VectorCompare( mins, maxs ))
			continue;

		if( bSimpleBox )
			trace = SV_TraceHull( touch, clip->hull, clip->start, mins, maxs, clip->end );
		else trace = SV_TraceHitbox( touch, clip->start, mins, maxs, clip->end );

		clip->trace = World_CombineTraces( &clip->trace, &trace, touch );
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;

	if( clip->boxmaxs[node->axis] > node->dist )
		SV_ClipToLinks( node->children[0], clip );
	if( clip->boxmins[node->axis] < node->dist )
		SV_ClipToLinks( node->children[1], clip );
}

/*
==================
SV_Move
==================
*/
trace_t SV_Move( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int type, edict_t *e )
{
	moveclip_t	clip;
	int		i;

	Q_memset( &clip, 0, sizeof( moveclip_t ));

	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.type = (type & 0xFF);
	clip.flags = (type & 0xFF00);
	clip.passedict = e;
	clip.hull = -1;

	// clip to world
	clip.trace = SV_TraceHull( EDICT_NUM( 0 ), clip.hull, start, mins, maxs, end );

	if( type == MOVE_MISSILE )
	{
		for( i = 0; i < 3; i++ )
		{
			clip.mins2[i] = -15;
			clip.maxs2[i] = 15;
		}
	}
	else
	{
		VectorCopy( mins, clip.mins2 );
		VectorCopy( maxs, clip.maxs2 );
	}

	clip.isPoint = VectorCompare( clip.mins2, clip.maxs2 );

	// create the bounding box of the entire move
	World_MoveBounds( start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs );

	// clip to entities
	SV_ClipToLinks( sv_areanodes, &clip );
	SV_CopyTraceToGlobal( &clip.trace );

	return clip.trace;
}

/*
==================
SV_Move
==================
*/
trace_t SV_MoveHull( const vec3_t start, int hullNumber, const vec3_t end, int type, edict_t *e )
{
	moveclip_t	clip;
	int		i;

	Q_memset( &clip, 0, sizeof( moveclip_t ));

	clip.start = start;
	clip.end = end;
	clip.type = (type & 0xFF);
	clip.flags = (type & 0xFF00);
	clip.passedict = e;
	clip.hull = bound( 0, hullNumber, 3 );
	clip.mins = sv.worldmodel->hulls[clip.hull].clip_mins;
	clip.maxs = sv.worldmodel->hulls[clip.hull].clip_maxs;
	clip.flags |= FMOVE_SIMPLEBOX; // completely ignore hitboxes, trace hulls only

	// clip to world
	clip.trace = SV_TraceHull( EDICT_NUM( 0 ), clip.hull, start, clip.mins, clip.maxs, end );

	if( type == MOVE_MISSILE )
	{
		for( i = 0; i < 3; i++ )
		{
			clip.mins2[i] = -15;
			clip.maxs2[i] = 15;
		}
	}
	else
	{
		VectorCopy( clip.mins, clip.mins2 );
		VectorCopy( clip.maxs, clip.maxs2 );
	}

	clip.isPoint = VectorCompare( clip.mins2, clip.maxs2 );

	// create the bounding box of the entire move
	World_MoveBounds( start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs );

	// clip to entities
	SV_ClipToLinks( sv_areanodes, &clip );
	SV_CopyTraceToGlobal( &clip.trace );

	return clip.trace;
}

/*
==================
SV_MoveToss
==================
*/
trace_t SV_MoveToss( edict_t *tossent, edict_t *ignore )
{
	float 	gravity;
	vec3_t	move, end;
	vec3_t	original_origin;
	vec3_t	original_velocity;
	vec3_t	original_angles;
	vec3_t	original_avelocity;
	trace_t	trace;
	int	i;

	VectorCopy( tossent->v.origin, original_origin );
	VectorCopy( tossent->v.velocity, original_velocity );
	VectorCopy( tossent->v.angles, original_angles );
	VectorCopy( tossent->v.avelocity, original_avelocity );
	gravity = tossent->v.gravity * svgame.movevars.gravity * 0.05f;

	for( i = 0; i < 200; i++ )
	{
		SV_CheckVelocity( tossent );
		tossent->v.velocity[2] -= gravity;
		VectorMA( tossent->v.angles, 0.05f, tossent->v.avelocity, tossent->v.angles );
		VectorScale( tossent->v.velocity, 0.05f, move );
		VectorAdd( tossent->v.origin, move, end );
		trace = SV_Move( tossent->v.origin, tossent->v.mins, tossent->v.maxs, end, MOVE_NORMAL, tossent );
		VectorCopy( trace.endpos, tossent->v.origin );
		if( trace.fraction < 1.0f ) break;
	}

	VectorCopy( original_origin, tossent->v.origin );
	VectorCopy( original_velocity, tossent->v.velocity );
	VectorCopy( original_angles, tossent->v.angles );
	VectorCopy( original_avelocity, tossent->v.avelocity );

	return trace;
}

/*
===============================================================================

	LIGHTING INFO

===============================================================================
*/

static vec3_t	sv_pointColor;

/*
=================
SV_RecursiveLightPoint
=================
*/
static qboolean SV_RecursiveLightPoint( model_t *model, mnode_t *node, const vec3_t start, const vec3_t end )
{
	int		side;
	mplane_t		*plane;
	msurface_t	*surf;
	mtexinfo_t	*tex;
	float		front, back, scale, frac;
	int		i, map, size, s, t;
	color24		*lm;
	vec3_t		mid;

	// didn't hit anything
	if( node->contents < 0 )
		return false;

	// calculate mid point
	plane = node->plane;
	if( plane->type < 3 )
	{
		front = start[plane->type] - plane->dist;
		back = end[plane->type] - plane->dist;
	}
	else
	{
		front = DotProduct( start, plane->normal ) - plane->dist;
		back = DotProduct( end, plane->normal ) - plane->dist;
	}

	side = front < 0;
	if(( back < 0 ) == side )
		return SV_RecursiveLightPoint( model, node->children[side], start, end );

	frac = front / ( front - back );

	VectorLerp( start, frac, end, mid );

	// co down front side	
	if( SV_RecursiveLightPoint( model, node->children[side], start, mid ))
		return true; // hit something

	if(( back < 0 ) == side )
		return false;// didn't hit anything

	// check for impact on this node
	surf = model->surfaces + node->firstsurface;

	for( i = 0; i < node->numsurfaces; i++, surf++ )
	{
		tex = surf->texinfo;

		if( surf->flags & SURF_DRAWTILED )
			continue;	// no lightmaps

		s = DotProduct( mid, tex->vecs[0] ) + tex->vecs[0][3] - surf->texturemins[0];
		t = DotProduct( mid, tex->vecs[1] ) + tex->vecs[1][3] - surf->texturemins[1];

		if(( s < 0 || s > surf->extents[0] ) || ( t < 0 || t > surf->extents[1] ))
			continue;

		s >>= 4;
		t >>= 4;

		if( !surf->samples )
			return true;

		VectorClear( sv_pointColor );

		lm = surf->samples + (t * ((surf->extents[0] >> 4) + 1) + s);
		size = ((surf->extents[0] >> 4) + 1) * ((surf->extents[1] >> 4) + 1);

		for( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++ )
		{
			scale = sv.lightstyles[surf->styles[map]].value;

			sv_pointColor[0] += lm->r * scale;
			sv_pointColor[1] += lm->g * scale;
			sv_pointColor[2] += lm->b * scale;

			lm += size; // skip to next lightmap
		}
		return true;
	}

	// go down back side
	return SV_RecursiveLightPoint( model, node->children[!side], mid, end );
}

void SV_RunLightStyles( void )
{
	int		i, ofs;
	lightstyle_t	*ls;

	// run lightstyles animation
	ofs = (sv.time * 10);

	if( ofs == sv_lastofs ) return;
	sv_lastofs = ofs;

	for( i = 0, ls = sv.lightstyles; i < MAX_LIGHTSTYLES; i++, ls++ )
	{
		if( ls->length == 0 ) ls->value = 256.0f * sv_lighting_modulate->value; // disable this light
		else if( ls->length == 1 ) ls->value = ls->map[0] * 22.0f * sv_lighting_modulate->value;
		else ls->value = ls->map[ofs%ls->length] * 22.0f * sv_lighting_modulate->value;
	}
}

/*
==================
SV_AddLightStyle

needs to get correct working SV_LightPoint
==================
*/
void SV_SetLightStyle( int style, const char* s )
{
	int	j, k;

	ASSERT( s );
	ASSERT( style >= 0 && style < MAX_LIGHTSTYLES );

	Q_strncpy( sv.lightstyles[style].pattern, s, sizeof( sv.lightstyles[0].pattern ));

	j = Q_strlen( s );
	sv.lightstyles[style].length = j;

	for( k = 0; k < j; k++ )
		sv.lightstyles[style].map[k] = (float)(s[k] - 'a');

	if( sv.state != ss_active ) return;

	// tell the clients about changed lightstyle
	BF_WriteByte( &sv.reliable_datagram, svc_lightstyle );
	BF_WriteByte( &sv.reliable_datagram, style );
	BF_WriteString( &sv.reliable_datagram, sv.lightstyles[style].pattern );
}

/*
==================
SV_LightForEntity

grab the ambient lighting color for current point
==================
*/
int SV_LightForEntity( edict_t *pEdict )
{
	vec3_t	start, end;

	if( !pEdict ) return 0;
	if( pEdict->v.effects & EF_FULLBRIGHT || !sv.worldmodel->lightdata )
	{
		return 255;
	}

	if( pEdict->v.flags & FL_CLIENT )
	{
		// player has more precision light level
		// that come from client-side
		return pEdict->v.light_level;
	}

	VectorCopy( pEdict->v.origin, start );
	VectorCopy( pEdict->v.origin, end );

	if( pEdict->v.effects & EF_INVLIGHT )
		end[2] = start[2] + 8192;
	else end[2] = start[2] - 8192;
	VectorSet( sv_pointColor, 1.0f, 1.0f, 1.0f );

	SV_RecursiveLightPoint( sv.worldmodel, sv.worldmodel->nodes, start, end );

	return VectorAvg( sv_pointColor );
}