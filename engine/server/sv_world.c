//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        sv_world.c - world query functions
//=======================================================================

#include "common.h"
#include "server.h"
#include "const.h"
#include "pm_local.h"

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
	int		trace_hull;	// -1 to let entity select hull
} moveclip_t;

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

	// decide which clipping hull to use, based on the size
	if( ent->v.solid == SOLID_BSP )
	{
		// explicit hulls in the BSP model
		if( ent->v.movetype != MOVETYPE_PUSH )
			Host_Error( "SOLID_BSP without MOVETYPE_PUSH\n" );

		model = CM_ClipHandleToModel( ent->v.modelindex );

		if( !model || model->type != mod_brush && model->type != mod_world )
			Host_Error( "MOVETYPE_PUSH with a non bsp model\n" );

		VectorSubtract( maxs, mins, size );

		if( hullNumber == -1 )
		{
			// FIXME: these constanst doesn't works with user-defined hulls
			// revisit this
			if( size[0] < 3 )
				hull = &model->hulls[0];		// point hull
			else if( size[0] <= 36 )
			{
				if( size[2] <= 36 )
					hull = &model->hulls[3];	// head hull (ducked)
				else hull = &model->hulls[1];		// human hull
			}
			else hull = &model->hulls[2];			// large hull
		}
		else
		{
			// TraceHull stuff
			hull = &model->hulls[hullNumber];
		}

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
	SV_InitBoxHull();	// for box testing
	SV_InitStudioHull();// for hitbox testing

	Mem_Set( sv_areanodes, 0, sizeof( sv_areanodes ));
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

		if( touch == ent || touch->v.solid != SOLID_TRIGGER ) // disabled ?
			continue;

		if( !BoundsIntersect( ent->v.absmin, ent->v.absmax, touch->v.absmin, touch->v.absmax ))
			continue;

		// check triggers accuracy
		if( CM_GetModelType( touch->v.modelindex ) == mod_brush )
		{
			hull = SV_HullForEntity( touch, -1, ent->v.mins, ent->v.maxs, test );

			// offset the test point appropriately for this hull.
			VectorSubtract( ent->v.origin, test, test );

			// test hull for intersection with this model
			if( SV_HullPointContents( hull, hull->firstclipnode, test ) == CONTENTS_EMPTY )
				continue;
		}

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
SV_CheckForOutside

Remove entity out of the level
===============
*/
void SV_CheckForOutside( edict_t *ent )
{
	const float	*org;

	// not solid edicts can be fly through walls
	if( ent->v.solid == SOLID_NOT ) return;

	// other ents probably may travels across the void
	if( ent->v.movetype != MOVETYPE_NONE ) return;

	// clients can flying outside
	if( ent->v.flags & FL_CLIENT ) return;
	
	// sprites and brushes can be stucks in the walls normally
	if( CM_GetModelType( ent->v.modelindex ) != mod_studio )
		return;

	org = ent->v.origin;

	if( SV_PointContents( org ) != CONTENTS_SOLID )
		return;

	MsgDev( D_ERROR, "%s outside of the world at %g %g %g\n", SV_ClassName( ent ), org[0], org[1], org[2] );

	ent->v.flags |= FL_KILLME;
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
			ent->headnode = node - worldmodel->nodes;

		if( ent->num_leafs == MAX_ENT_LEAFS )
			return;

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
			ent->headnode = node - worldmodel->nodes;
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
bool SV_HeadnodeVisible( mnode_t *node, byte *visbits )
{
	mleaf_t	*leaf;
	int	leafnum;

	if( node->contents < 0 )
	{
		if( node->contents != CONTENTS_SOLID )
		{
			leaf = (mleaf_t *)node;
			leafnum = (leaf - worldmodel->leafs - 1);

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
void SV_LinkEdict( edict_t *ent, bool touch_triggers )
{
	areanode_t	*node;

	if( ent->area.prev ) SV_UnlinkEdict( ent );	// unlink from old position
	if( ent == svgame.edicts ) return;		// don't add the world
	if( !SV_IsValidEdict( ent )) return;		// never add freed ents

	// set the abs box
	svgame.dllFuncs.pfnSetAbsBox( ent );

	// link to PVS leafs
	ent->num_leafs = 0;

	if( ent->v.modelindex )
	{
		ent->headnode = -1;
		SV_FindTouchedLeafs( ent, sv.worldmodel->nodes );

		// if none of the leafs were inside the map, the
		// entity is outside the world and can be considered unlinked
		if( !ent->num_leafs )
		{
			SV_CheckForOutside( ent );
			ent->headnode = -1;
			return;
		}

		if( ent->num_leafs == MAX_ENT_LEAFS )
			ent->num_leafs = 0;		// so we use headnode instead
		else ent->headnode = -1;		// use normal leafs check
	}

	// ignore not solid bodies
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
		InsertLinkBefore( &ent->area, &node->trigger_edicts, NUM_FOR_EDICT( ent ));
	else if( ent->v.solid == SOLID_NOT && ent->v.skin != CONTENTS_NONE )
		InsertLinkBefore( &ent->area, &node->water_edicts, NUM_FOR_EDICT( ent ));
	else InsertLinkBefore( &ent->area, &node->solid_edicts, NUM_FOR_EDICT( ent ));

	if( touch_triggers ) SV_TouchLinks( ent, sv_areanodes );
}

/*
===============================================================================

POINT TESTING IN HULLS

===============================================================================
*/
/*
==================
SV_HullPointContents

==================
*/
int SV_HullPointContents( hull_t *hull, int num, const vec3_t p )
{
	float		d;
	dclipnode_t	*node;
	mplane_t		*plane;

	while( num >= 0 )
	{
		if( num < hull->firstclipnode || num > hull->lastclipnode )
			Host_Error( "SV_HullPointContents: bad node number %i\n", num );
	
		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;
		
		if( plane->type < 3 )
			d = p[plane->type] - plane->dist;
		else d = DotProduct( plane->normal, p ) - plane->dist;

		if( d < 0 ) num = node->children[1];
		else num = node->children[0];
	}
	return num;
}

/*
====================
SV_WaterLinks
====================
*/
void SV_WaterLinks( const vec3_t mins, const vec3_t maxs, int *pCont, areanode_t *node )
{
	link_t	*l, *next;
	edict_t	*touch;

	// get water edicts
	for( l = node->water_edicts.next; l != &node->water_edicts; l = next )
	{
		next = l->next;
		touch = EDICT_FROM_AREA( l );

		if( touch->v.solid != SOLID_NOT ) // disabled ?
			continue;

		// only brushes can have special contents
		if( CM_GetModelType( touch->v.modelindex ) != mod_brush )
			continue;

		if( !BoundsIntersect( mins, maxs, touch->v.absmin, touch->v.absmax ))
			continue;

		// compare contents ranking
		if( RankForContents( touch->v.skin ) > RankForContents( *pCont ))
			*pCont = touch->v.skin; // new content has more priority
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;
	
	if( maxs[node->axis] > node->dist )
		SV_WaterLinks( mins, maxs, pCont, node->children[0] );
	if( mins[node->axis] < node->dist )
		SV_WaterLinks( mins, maxs, pCont, node->children[1] );
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
	cont = SV_HullPointContents( &sv.worldmodel->hulls[0], 0, p );

	// check all water entities
	SV_WaterLinks( p, p, &cont, sv_areanodes );

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
bool SV_TestEntityPosition( edict_t *ent )
{
	trace_t	trace;

	trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_NORMAL, ent );

	return trace.fStartSolid;
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
bool SV_RecursiveHullCheck( hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t *trace )
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
			trace->fAllSolid = false;
			if( num == CONTENTS_EMPTY )
				trace->fInOpen = true;
			else trace->fInWater = true;
		}
		else trace->fStartSolid = true;
		return true; // empty
	}

	if( num < hull->firstclipnode || num > hull->lastclipnode )
		Host_Error( "SV_RecursiveHullCheck: bad node number %i\n", num );

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

	if( SV_HullPointContents( hull, node->children[side^1], mid ) != CONTENTS_SOLID )
	{
		// go past the node
		return SV_RecursiveHullCheck (hull, node->children[side^1], midf, p2f, mid, p2, trace);
	}	

	if( trace->fAllSolid )
		return false; // never got out of the solid area
		
	//==================
	// the other side of the node is solid, this is the impact point
	//==================
	if( !side )
	{
		VectorCopy( plane->normal, trace->vecPlaneNormal );
		trace->flPlaneDist = plane->dist;
	}
	else
	{
		VectorNegate( plane->normal, trace->vecPlaneNormal );
		trace->flPlaneDist = -plane->dist;
	}

	while( SV_HullPointContents( hull, hull->firstclipnode, mid ) == CONTENTS_SOLID )
	{
		// shouldn't really happen, but does occasionally
		frac -= 0.1f;

		if( frac < 0.0f )
		{
			trace->flFraction = midf;
			VectorCopy( mid, trace->vecEndPos );
			MsgDev( D_WARN, "trace backed up 0.0\n" );
			return false;
		}

		midf = p1f + (p2f - p1f) * frac;
		VectorLerp( p1, frac, p2, mid );
	}

	trace->flFraction = midf;
	VectorCopy( mid, trace->vecEndPos );

	return false;
}

/*
==================
SV_ClipMove

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
trace_t SV_ClipMove( edict_t *ent, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int flags )
{
	trace_t	trace;
	pmtrace_t	pmtrace;
	int	pm_flags = 0;
	physent_t	pe;

	// fill in a default trace
	Mem_Set( &trace, 0, sizeof( trace_t ));
	VectorCopy( end, trace.vecEndPos );
	trace.flFraction = 1.0f;
	trace.iHitgroup = -1;

	// setup physentity
	if( !SV_CopyEdictToPhysEnt( &pe, ent, false ))
		return trace;

	// setup pm_flags
	if( flags & FMOVE_SIMPLEBOX )
		pm_flags |= PM_STUDIO_BOX;
	if( flags & FMOVE_IGNORE_GLASS )
		pm_flags |= PM_GLASS_IGNORE; 

	if( !PM_TraceModel( &pe, start, mins, maxs, end, &pmtrace, pm_flags ))
		return trace; // didn't hit anything

	// copy the trace results
	trace.fAllSolid = pmtrace.allsolid;
	trace.fStartSolid = pmtrace.startsolid;
	trace.fInOpen = pmtrace.inopen;
	trace.fInWater = pmtrace.inwater;
	trace.flFraction = pmtrace.fraction;
	VectorCopy( pmtrace.endpos, trace.vecEndPos );
	trace.flPlaneDist = pmtrace.plane.dist;
	VectorCopy( pmtrace.plane.normal, trace.vecPlaneNormal );
	trace.iHitgroup = pmtrace.hitgroup;
	trace.pHit = ent;

	return trace;
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
	trace_t	trace;

	// touch linked edicts
	for( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;

		touch = EDICT_FROM_AREA( l );

		if( touch->v.solid == SOLID_NOT )
			continue;
		if( touch == clip->passedict )
			continue;
		if( touch->v.solid == SOLID_TRIGGER )
			Host_Error( "trigger in clipping list\n" );

		if( clip->type == MOVE_NOMONSTERS && touch->v.solid != SOLID_BSP )
			continue;

		// don't clip points against points (they can't collide)
		if( VectorCompare( touch->v.mins, touch->v.maxs ) && (clip->type != MOVE_MISSILE || !( touch->v.flags & FL_MONSTER )))
			continue;

		// can't tracehull at dead monsters, only traceline
		if( touch->v.deadflag == DEAD_DEAD && !VectorCompare( touch->v.mins, touch->v.maxs ))
			continue;

		if( clip->type == MOVE_WORLDONLY )
		{
			// accept only real bsp models with FL_WORLDBRUSH set
			if( CM_GetModelType( touch->v.modelindex ) == mod_brush && touch->v.flags & FL_WORLDBRUSH );
			else continue;
		}

		if( !BoundsIntersect( clip->boxmins, clip->boxmaxs, touch->v.absmin, touch->v.absmax ))
			continue;

		if( clip->passedict && !VectorIsNull( clip->passedict->v.size ) && VectorIsNull( touch->v.size ))
			continue;	// points never interact

		// monsterclip filter
		if( CM_GetModelType( touch->v.modelindex ) == mod_brush && ( touch->v.flags & FL_MONSTERCLIP ))
		{
			if( clip->passedict && clip->passedict->v.flags & FL_MONSTERCLIP );
			else continue;
		}

		// custom user filter
		if( svgame.dllFuncs2.pfnShouldCollide )
		{
			if( !svgame.dllFuncs2.pfnShouldCollide( touch, clip->passedict ))
				continue;
		}

		if( clip->flags & FMOVE_IGNORE_GLASS && CM_GetModelType( touch->v.modelindex ) == mod_brush )
		{
			vec3_t	point;

			// we ignore brushes with rendermode != kRenderNormal
			switch( touch->v.rendermode )
			{
			case kRenderTransTexture:
			case kRenderTransAlpha:
			case kRenderTransAdd:
				if( touch->v.renderamt < 200 )
					continue;
				// check for translucent contents
				if( VectorIsNull( touch->v.origin ))
					VectorAverage( touch->v.absmin, touch->v.absmax, point );
				else VectorCopy( touch->v.origin, point );
				if( SV_PointContents( point ) == CONTENTS_TRANSLUCENT )
					continue; // grate detected
			default:	break;
			}
		}

		// might intersect, so do an exact clip
		if( clip->trace.fAllSolid ) return;

		if( clip->passedict )
		{
		 	if( touch->v.owner == clip->passedict )
				continue;	// don't clip against own missiles
			if( clip->passedict->v.owner == touch )
				continue;	// don't clip against owner
		}

		if( touch->v.flags & FL_MONSTER )
			trace = SV_ClipMove( touch, clip->start, clip->mins2, clip->maxs2, clip->end, clip->flags );
		else trace = SV_ClipMove( touch, clip->start, clip->mins, clip->maxs, clip->end, clip->flags );

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

	Mem_Set( &clip, 0, sizeof( moveclip_t ));

	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.type = (type & 0xFF);
	clip.flags = (type & 0xFF00);
	clip.passedict = e;

	// clip to world
	clip.trace = SV_ClipMove( EDICT_NUM( 0 ), start, mins, maxs, end, 0 );

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

	// create the bounding box of the entire move
	World_MoveBounds( start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs );

	// clip to entities
	SV_ClipToLinks( sv_areanodes, &clip );

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
		VectorCopy( trace.vecEndPos, tossent->v.origin );
		if( trace.flFraction < 1.0f ) break;
	}

	VectorCopy( original_origin, tossent->v.origin );
	VectorCopy( original_velocity, tossent->v.velocity );
	VectorCopy( original_angles, tossent->v.angles );
	VectorCopy( original_avelocity, tossent->v.avelocity );

	return trace;
}

/*
============================================================================

AREA QUERY

Fills in a list of all entities who's absmin / absmax intersects the given
bounds.  This does NOT mean that they actually touch in the case of bmodels.
============================================================================
*/
/*
====================
SV_AreaEdicts_r
====================
*/
void SV_AreaEdicts_r( areanode_t *node, area_t *ap )
{
	link_t	*l, *next, *start;
	edict_t	*check;
	int	count = 0;

	// touch linked edicts
	if( ap->type == AREA_SOLID )
		start = &node->solid_edicts;
	else if( ap->type == AREA_TRIGGERS )
		start = &node->trigger_edicts;
	else start = &node->water_edicts;

	for( l = start->next; l != start; l = next )
	{
		next = l->next;
		check = EDICT_FROM_AREA( l );

		if( check->v.solid == SOLID_NOT && check->v.skin == CONTENTS_NONE )
			continue; // deactivated

		if( !BoundsIntersect( check->v.absmin, check->v.absmax, ap->mins, ap->maxs ))
			continue;	// not touching

		if( ap->count == ap->maxcount )
		{
			MsgDev( D_WARN, "SV_AreaEdicts: maxcount hit\n" );
			return;
		}

		ap->list[ap->count] = check;
		ap->count++;
	}
	
	if( node->axis == -1 ) return; // terminal node

	// recurse down both sides
	if( ap->maxs[node->axis] > node->dist ) SV_AreaEdicts_r( node->children[0], ap );
	if( ap->mins[node->axis] < node->dist ) SV_AreaEdicts_r( node->children[1], ap );
}

/*
================
SV_AreaEdicts
================
*/
int SV_AreaEdicts( const vec3_t mins, const vec3_t maxs, edict_t **list, int maxcount, int areatype )
{
	area_t	ap;

	ap.mins = mins;
	ap.maxs = maxs;
	ap.list = list;
	ap.count = 0;
	ap.maxcount = maxcount;
	ap.type = areatype;

	SV_AreaEdicts_r( sv_areanodes, &ap );

	return ap.count;
}

/*
============
SV_TestPlayerPosition

============
*/
edict_t *SV_TestPlayerPosition( const vec3_t origin, edict_t *pass, TraceResult *tr )
{
	float	*mins, *maxs;
	trace_t	result;

	svgame.pmove->usehull = bound( 0, svgame.pmove->usehull, 3 );
	mins = svgame.pmove->player_mins[svgame.pmove->usehull];
	maxs = svgame.pmove->player_maxs[svgame.pmove->usehull];

	if( pass ) SV_SetMinMaxSize( pass, mins, maxs );
	
	result = SV_Move( origin, mins, maxs, origin, MOVE_NORMAL, pass );
	if( tr ) *tr = result;

	if( result.pHit )
		return result.pHit;
	return NULL;
}