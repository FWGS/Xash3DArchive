//=======================================================================
//			Copyright XashXT Group 2009 ©
//		        cl_world.c - world query functions
//=======================================================================

#include "common.h"
#include "client.h"
#include "const.h"
#include "pm_defs.h"

areanode_t	cl_areanodes[AREA_NODES];
int		cl_numareanodes;

/*
===============
CL_CreateAreaNode

builds a uniformly subdivided tree for the given world size
===============
*/
areanode_t *CL_CreateAreaNode( int depth, vec3_t mins, vec3_t maxs )
{
	areanode_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1;
	vec3_t		mins2, maxs2;

	anode = &cl_areanodes[cl_numareanodes++];

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
	anode->children[0] = CL_CreateAreaNode( depth+1, mins2, maxs2 );
	anode->children[1] = CL_CreateAreaNode( depth+1, mins1, maxs1 );

	return anode;
}

/*
===============
CL_ClearWorld

===============
*/
void CL_ClearWorld( void )
{
	vec3_t	mins, maxs;
	int	worldIndex = 1;

	cl_numareanodes = 0;
	Mod_GetBounds( worldIndex, mins, maxs );
	Mem_Set( cl_areanodes, 0, sizeof( cl_areanodes ));
	CL_CreateAreaNode( 0, mins, maxs );
}

/*
=================
CL_ClassifyEdict

sorting edict by type
=================
*/
void CL_ClassifyEdict( cl_entity_t *cl_ent )
{
	if( !cl_ent || cl_ent->curstate.ed_type != ED_SPAWNED )
		return;

	cl_ent->curstate.ed_type = ED_TEMPENTITY; // it's client entity

	// display type
	// Msg( "%s: <%s>\n", STRING( ent->v.classname ), ed_name[cl_ent->curstate.ed_type] );
}

/*
===============
CL_UnlinkEdict
===============
*/
void CL_UnlinkEdict( cl_entity_t *ent )
{
	// not linked in anywhere
	if( !ent->area.prev )
		return;

	RemoveLink( &ent->area );
	ent->area.prev = NULL;
	ent->area.next = NULL;
}

void CL_SetAbsBbox( cl_entity_t *ent )
{
	if (( ent->curstate.solid == SOLID_BSP ) && !VectorIsNull( ent->angles ))
	{	
		// expand for rotation
		float	max = 0, v;
		int	i;

		for ( i = 0; i < 3; i++ )
		{
			v = fabs( ent->curstate.mins[i] );
			if ( v > max ) max = v;
			v = fabs( ent->curstate.maxs[i] );
			if ( v > max ) max = v;
		}

		for ( i = 0; i < 3; i++ )
		{
			ent->absmin[i] = ent->origin[i] - max;
			ent->absmax[i] = ent->origin[i] + max;
		}
	}
	else
	{
		VectorAdd( ent->origin, ent->curstate.mins, ent->absmin );
		VectorAdd( ent->origin, ent->curstate.maxs, ent->absmax );
	}

	ent->absmin[0] -= 1;
	ent->absmin[1] -= 1;
	ent->absmin[2] -= 1;
	ent->absmax[0] += 1;
	ent->absmax[1] += 1;
	ent->absmax[2] += 1;
}

/*
===============
CL_LinkEntity
===============
*/
void CL_LinkEdict( cl_entity_t *ent, bool touch_triggers )
{
	areanode_t	*node;

	if( !ent ) return;
	if( ent->area.prev ) CL_UnlinkEdict( ent ); // unlink from old position
	if( ent->index <= 0 ) return;

	// trying to classify unclassified edicts
	if( cls.state == ca_active && ent->curstate.ed_type == ED_SPAWNED )
		CL_ClassifyEdict( ent );

	// set the abs box
	CL_SetAbsBbox( ent );

	// ignore not solid bodies
	if( ent->curstate.solid == SOLID_NOT && ent->curstate.skin == CONTENTS_NONE )
		return;

	// find the first node that the ent's box crosses
	node = cl_areanodes;

	while( 1 )
	{
		if( node->axis == -1 ) break;
		if( ent->absmin[node->axis] > node->dist )
			node = node->children[0];
		else if( ent->absmax[node->axis] < node->dist )
			node = node->children[1];
		else break; // crosses the node
	}
	
	// link it in	
	if( ent->curstate.solid == SOLID_TRIGGER )
		InsertLinkBefore( &ent->area, &node->trigger_edicts, ent->index );
	else if( ent->curstate.solid == SOLID_NOT && ent->curstate.skin != CONTENTS_NONE )
		InsertLinkBefore (&ent->area, &node->water_edicts, ent->index );
	else InsertLinkBefore (&ent->area, &node->solid_edicts, ent->index );

	if( touch_triggers ) Host_Error( "CL_LinkEdict: touch_tiggers\n" );
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
CL_AreaEdicts_r
====================
*/
void CL_AreaEdicts_r( areanode_t *node, area_t *ap )
{
	link_t		*l, *next, *start;
	cl_entity_t	*check;
	int		count = 0;

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

		if( check->curstate.solid == SOLID_NOT && check->curstate.skin == CONTENTS_NONE )
			continue; // deactivated
		if( !BoundsIntersect( check->absmin, check->absmax, ap->mins, ap->maxs ))
			continue;	// not touching

		if( ap->count == ap->maxcount )
		{
			MsgDev( D_WARN, "CL_AreaEdicts: maxcount hit\n" );
			return;
		}

		ap->list[ap->count] = check;
		ap->count++;
	}
	
	if( node->axis == -1 ) return; // terminal node

	// recurse down both sides
	if( ap->maxs[node->axis] > node->dist ) CL_AreaEdicts_r( node->children[0], ap );
	if( ap->mins[node->axis] < node->dist ) CL_AreaEdicts_r( node->children[1], ap );
}

/*
================
CL_AreaEdicts
================
*/
int CL_AreaEdicts( const vec3_t mins, const vec3_t maxs, cl_entity_t **list, int maxcount, int areatype )
{
	area_t	ap;

	ap.mins = mins;
	ap.maxs = maxs;
	ap.list = list;
	ap.count = 0;
	ap.maxcount = maxcount;
	ap.type = areatype;

	CL_AreaEdicts_r( cl_areanodes, &ap );

	return ap.count;
}

/*
====================
CL_ClipToLinks

Mins and maxs enclose the entire area swept by the move
====================
*/
static void CL_ClipToLinks( areanode_t *node, moveclip_t *clip )
{
	link_t		*l, *next;
	cl_entity_t	*touch;
	trace_t		trace;

	// touch linked edicts
	for( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;

		touch = EDICT_FROM_AREA( l );

		if( touch->curstate.solid == SOLID_NOT )
			continue;
		if( touch == (cl_entity_t *)clip->passedict )
			continue;
		if( touch->curstate.solid == SOLID_TRIGGER )
		{
			// throw warn and ignore
			MsgDev( D_WARN, "entity %s (%i) with SOLID_TRIGGER in clipping list\n", CL_ClassName( touch ), touch->index );
			touch->curstate.solid = SOLID_NOT;
			continue;
		}
		if( clip->type == MOVE_NOMONSTERS && touch->curstate.solid != SOLID_BSP )
			continue;

		// don't clip points against points (they can't collide)
		if( VectorCompare( touch->curstate.mins, touch->curstate.maxs ) && (clip->type != MOVE_MISSILE || !(touch->curstate.flags & FL_MONSTER)))
			continue;

		if( clip->type == MOVE_WORLDONLY )
		{
			// accept only real bsp models with FL_WORLDBRUSH set
			if( CM_GetModelType( touch->curstate.modelindex ) == mod_brush && touch->curstate.flags & FL_WORLDBRUSH );
			else continue;
		}

		if( !BoundsIntersect( clip->boxmins, clip->boxmaxs, touch->absmin, touch->absmax ))
			continue;

		// monsterclip filter
		if( CM_GetModelType( touch->curstate.modelindex ) == mod_brush && ( touch->curstate.flags & FL_MONSTERCLIP ))
		{
			if( clip->passedict && ((cl_entity_t *)&clip->passedict)->curstate.flags & FL_MONSTERCLIP );
			else continue;
		}

		if( clip->flags & FMOVE_IGNORE_GLASS && CM_GetModelType( touch->curstate.modelindex ) == mod_brush )
		{
			vec3_t	point;

			// we can ignore brushes with rendermode != kRenderNormal
			switch( touch->curstate.rendermode )
			{
			case kRenderTransTexture:
			case kRenderTransAlpha:
			case kRenderTransAdd:
				if( touch->curstate.renderamt < 200 )
					continue;
				// check for translucent contents
				if( VectorIsNull( touch->origin ))
					VectorAverage( touch->absmin, touch->absmax, point );
				else VectorCopy( touch->origin, point );
				if( CL_PointContents( point ) == CONTENTS_TRANSLUCENT )
					continue; // grate detected
			default:	break;
			}
		}

		// might intersect, so do an exact clip
		if( clip->trace.fAllSolid ) return;

		if( clip->passedict )
		{
		 	if( CL_GetEntityByIndex( touch->curstate.owner ) == (cl_entity_t *)clip->passedict )
				continue;	// don't clip against own missiles
			if(CL_GetEntityByIndex( ((cl_entity_t *)&clip->passedict)->curstate.owner ) == touch )
				continue;	// don't clip against owner
		}
#if 0
		// temporary disabled: wait for moving physic.dll into engine
		if( touch->curstate.flags & FL_MONSTER )
			trace = CM_ClipMove( touch, clip->start, clip->mins2, clip->maxs2, clip->end, clip->flags );
		else trace = CM_ClipMove( touch, clip->start, clip->mins, clip->maxs, clip->end, clip->flags );

		clip->trace = World_CombineTraces( &clip->trace, &trace, touch );
#endif
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;

	if( clip->boxmaxs[node->axis] > node->dist )
		CL_ClipToLinks( node->children[0], clip );
	if( clip->boxmins[node->axis] < node->dist )
		CL_ClipToLinks( node->children[1], clip );
}

/*
==================
CL_Move
==================
*/
trace_t CL_Move( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int type, cl_entity_t *e )
{
	trace_t		trace;
#if 0
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
	clip.trace = CM_ClipMove( EDICT_NUM( 0 ), start, mins, maxs, end, 0 );

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
	CL_ClipToLinks( cl_areanodes, &clip );

	return clip.trace;
#endif
	// FIXME: write client trace

	// fill in a default trace
	Mem_Set( &trace, 0, sizeof( trace_t ));
	VectorCopy( end, trace.vecEndPos );
	trace.flFraction = 1.0f;
	trace.fAllSolid = true;
	trace.iHitgroup = -1;

	return trace;
}

/*
=============
CL_PointContents

=============
*/
int CL_TruePointContents( const vec3_t p )
{
	int	i, num, contents;
	cl_entity_t	*hit, *touch[MAX_EDICTS];

	// sanity check
	if( !p ) return CONTENTS_NONE;

	// get base contents from world
	contents = CM_PointContents( p );

	if( contents != CONTENTS_EMPTY )
		return contents; // have some world contents

	// check contents from all the solid entities
	num = CL_AreaEdicts( p, p, touch, MAX_EDICTS, AREA_SOLID );

	for( i = 0; i < num; i++ )
	{
		hit = touch[i];

		if( hit->curstate.solid != SOLID_BSP )
			continue; // monsters, players

		// solid entity found
		return CONTENTS_SOLID;
	}

	// check contents from all the custom entities
	num = CL_AreaEdicts( p, p, touch, MAX_EDICTS, AREA_CUSTOM );

	for( i = 0; i < num; i++ )
	{
		hit = touch[i];

		if( hit->curstate.solid != SOLID_NOT || hit->curstate.skin == CONTENTS_NONE )
			continue; // invalid water ?

		// custom contents found
		return hit->curstate.skin;
	}
	return contents;
}

int CL_PointContents( const vec3_t p )
{
	int cont = CL_TruePointContents( p );

	if( cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN )
		cont = CONTENTS_WATER;
	return cont;
}

/*
============
CL_TraceLine

soundlib light version
============
*/
trace_t CL_TraceLine( const vec3_t start, const vec3_t end )
{
	return CL_Move( start, vec3_origin, vec3_origin, end, MOVE_NOMONSTERS, NULL );
}

/*
============
CL_TestPlayerPosition

============
*/
cl_entity_t *CL_TestPlayerPosition( const vec3_t origin, cl_entity_t *pass, TraceResult *tr )
{
	float	*mins, *maxs;
	trace_t	result;

	clgame.pmove->usehull = bound( 0, clgame.pmove->usehull, 3 );
	mins = clgame.pmove->player_mins[clgame.pmove->usehull];
	maxs = clgame.pmove->player_maxs[clgame.pmove->usehull];

	result = CL_Move( origin, mins, maxs, origin, MOVE_NORMAL, pass );
	if( tr ) Mem_Copy( tr, &result, sizeof( *tr ));

	return result.pEnt;
}