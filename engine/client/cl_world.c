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
	
	anode->dist = 0.5f * ( maxs[anode->axis] + mins[anode->axis] );
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
	Mem_Set( cl_areanodes, 0, sizeof( cl_areanodes ));
	cl_numareanodes = 0;

	CL_CreateAreaNode( 0, cl.worldmodel->mins, cl.worldmodel->maxs );
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
void CL_LinkEdict( cl_entity_t *ent )
{
	areanode_t	*node;

	if( !ent ) return;
	if( ent->area.prev ) CL_UnlinkEdict( ent ); // unlink from old position
	if( ent->index <= 0 ) return;

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
		InsertLinkBefore( &ent->area, &node->trigger_edicts );
	else if( ent->curstate.solid == SOLID_NOT && ent->curstate.skin != CONTENTS_NONE )
		InsertLinkBefore (&ent->area, &node->water_edicts );
	else InsertLinkBefore (&ent->area, &node->solid_edicts );
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
		if( !check ) continue;

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