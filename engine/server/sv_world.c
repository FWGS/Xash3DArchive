//=======================================================================
//			Copyright XashXT Group 2008 �
//		        sv_world.c - world query functions
//=======================================================================

#include "common.h"
#include "server.h"
#include "const.h"
#include "pm_local.h"

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
	
	anode->dist = 0.5f * (maxs[anode->axis] + mins[anode->axis]);
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
	vec3_t	mins, maxs;
	int	worldIndex = 1;

	sv_numareanodes = 0;
	Mod_GetBounds( worldIndex, mins, maxs );
	Mem_Set( sv_areanodes, 0, sizeof( sv_areanodes ));
	SV_CreateAreaNode( 0, mins, maxs );
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
		if( touch == ent ) continue;
		if( touch->free || touch->v.solid != SOLID_TRIGGER ) // disabled ?
			continue;

		if( !BoundsIntersect( ent->v.absmin, ent->v.absmax, touch->v.absmin, touch->v.absmax ))
			continue;

		if( CM_GetModelType( touch->v.modelindex ) == mod_brush )
		{
			hull = CM_HullForBsp( touch, ent->v.mins, ent->v.maxs, test );

			// offset the test point appropriately for this hull.
			VectorSubtract( ent->v.origin, test, test );

			// test hull for intersection with this model
			if( CM_HullPointContents( hull, hull->firstclipnode, test ) == CONTENTS_EMPTY )
				continue;
		}

		svgame.dllFuncs.pfnTouch( touch, ent );
		if( ent->free ) break; // killtarget issues
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
===============
SV_CheckForOutside

Remove entity out of level
===============
*/
void SV_CheckForOutside( edict_t *ent )
{
	// not solid edicts can be fly through walls
	if( ent->v.solid == SOLID_NOT ) return;

	// other ents probably may travels across the void
	if( ent->v.movetype != MOVETYPE_NONE ) return;

	// clients can flying outside
	if( ent->v.flags & FL_CLIENT ) return;
	
	// sprites and brushes can be stucks in the walls normally
	if( CM_GetModelType( ent->v.modelindex ) != mod_studio )
		return;

	if( SV_PointContents( ent->v.origin ) == CONTENTS_SOLID )
	{
		const float *org = ent->v.origin;

		MsgDev( D_ERROR, "%s outside of the world at %g %g %g\n", SV_ClassName( ent ), org[0], org[1], org[2] );
		ent->v.flags |= FL_KILLME;
	}
}

/*
===============
SV_LinkEntity
===============
*/
void SV_LinkEdict( edict_t *ent, bool touch_triggers )
{
	areanode_t	*node;
	short		leafs[MAX_TOTAL_ENT_LEAFS];
	int		i, j, num_leafs, topNode;

	if( ent->area.prev ) SV_UnlinkEdict( ent ); // unlink from old position
	if( ent == EDICT_NUM( 0 )) return; // don't add the world
	if( ent->free ) return;

	// set the abs box
	svgame.dllFuncs.pfnSetAbsBox( ent );

	// link to PVS leafs
	ent->num_leafs = 0;

	// get all leafs, including solids
	num_leafs = CM_BoxLeafnums( ent->v.absmin, ent->v.absmax, leafs, MAX_TOTAL_ENT_LEAFS, &topNode );

	// if none of the leafs were inside the map, the
	// entity is outside the world and can be considered unlinked
	if( !num_leafs )
	{
		SV_CheckForOutside( ent );
		return;
	}

	if( num_leafs >= MAX_ENT_LEAFS )
	{	
		// assume we missed some leafs, and mark by headnode
		ent->num_leafs = -1;
		ent->headnode = topNode;
	}
	else
	{
		ent->num_leafs = 0;

		for( i = 0; i < num_leafs; i++ )
		{
			if( leafs[i] == -1 )
				continue;		// not a visible leaf

			for( j = 0; j < i; j++ )
			{
				if( leafs[j] == leafs[i] )
					break;
			}

			if( j == i )
			{
				if( ent->num_leafs == MAX_ENT_LEAFS )
				{
					// assume we missed some leafs, and mark by headNode
					ent->num_leafs = -1;
					ent->headnode = topNode;
					break;
				}
				ent->leafnums[ent->num_leafs++] = leafs[i];
			}
		}
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
	if( !SV_CopyEdictToPhysEnt( &pe, ent, true ))
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
=============
SV_PointContents

=============
*/
int SV_TruePointContents( const vec3_t p )
{
	int	i, num, contents;
	edict_t	*hit, *touch[MAX_EDICTS];

	// sanity check
	if( !p ) return CONTENTS_NONE;

	// get base contents from world
	contents = CM_PointContents( p );

	if( contents != CONTENTS_EMPTY )
		return contents; // have some world contents

	// check contents from all the solid entities
	num = SV_AreaEdicts( p, p, touch, MAX_EDICTS, AREA_SOLID );

	for( i = 0; i < num; i++ )
	{
		hit = touch[i];

		if( hit->v.solid != SOLID_BSP )
			continue; // monsters, players

		// solid entity found
		return CONTENTS_SOLID;
	}

	// check contents from all the custom entities
	num = SV_AreaEdicts( p, p, touch, MAX_EDICTS, AREA_CUSTOM );

	for( i = 0; i < num; i++ )
	{
		hit = touch[i];

		if( hit->v.solid != SOLID_NOT || hit->v.skin == CONTENTS_NONE )
			continue; // invalid water ?

		// custom contents found
		return hit->v.skin;
	}
	return contents;
}

int SV_PointContents( const vec3_t p )
{
	int cont = SV_TruePointContents( p );

	if( cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN )
		cont = CONTENTS_WATER;
	return cont;
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