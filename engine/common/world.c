//=======================================================================
//			Copyright XashXT Group 2009 �
//		      com_world.c - common worldtrace routines
//=======================================================================

#include "common.h"
#include "world.h"
#include "pm_defs.h"
#include "cm_local.h"
#include "mathlib.h"
#include "studio.h"

const char *et_name[] =
{
	"normal",
	"player",
	"tempentity",
	"beam",
	"fragmented",
	"viewentity",
	"portal",
	"skyportal",
};

/*
===============================================================================

	ENTITY LINKING

===============================================================================
*/
/*
===============
ClearLink

ClearLink is used for new headnodes
===============
*/
void ClearLink( link_t *l )
{
	l->prev = l->next = l;
}

/*
===============
RemoveLink

remove link from chain
===============
*/
void RemoveLink( link_t *l )
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

/*
===============
InsertLinkBefore

kept trigger and solid entities seperate
===============
*/
void InsertLinkBefore( link_t *l, link_t *before )
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

/*
==================
World_MoveBounds
==================
*/
void World_MoveBounds( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, vec3_t boxmins, vec3_t boxmaxs )
{
	int	i;
	
	for( i = 0; i < 3; i++ )
	{
		if( end[i] > start[i] )
		{
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		}
		else
		{
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
}

trace_t World_CombineTraces( trace_t *cliptrace, trace_t *trace, edict_t *touch )
{
	if( trace->allsolid || trace->fraction < cliptrace->fraction )
	{
		trace->ent = touch;
		
		if( cliptrace->startsolid )
		{
			*cliptrace = *trace;
			cliptrace->startsolid = true;
		}
		else *cliptrace = *trace;
	}
	else if( trace->startsolid )
		cliptrace->startsolid = true;

	return *cliptrace;
}

qboolean World_UseSimpleBox( qboolean simpleBox, int solid, qboolean isPointTrace, model_t *mod )
{
	if( !mod || mod->type != mod_studio || simpleBox )
		return true; // force to simplebox

	if( solid == SOLID_SLIDEBOX && isPointTrace )
		return false;

	if( solid == SOLID_BBOX && ( mod->flags & STUDIO_TRACE_HITBOX || isPointTrace ))
		return false;

	return true;
}

/*
==================
RankForContents

Used for determine contents priority
==================
*/
int RankForContents( int contents )
{
	switch( contents )
	{
	case CONTENTS_EMPTY:	return 0;
	case CONTENTS_WATER:	return 1;
	case CONTENTS_TRANSLUCENT:	return 2;
	case CONTENTS_CURRENT_0:	return 3;
	case CONTENTS_CURRENT_90:	return 4;
	case CONTENTS_CURRENT_180:	return 5;
	case CONTENTS_CURRENT_270:	return 6;
	case CONTENTS_CURRENT_UP:	return 7;
	case CONTENTS_CURRENT_DOWN:	return 8;
	case CONTENTS_SLIME:	return 9;
	case CONTENTS_LAVA:		return 10;
	case CONTENTS_SKY:		return 11;
	case CONTENTS_SOLID:	return 12;
	}
	return -1;
}

/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
int BoxOnPlaneSide( const vec3_t emins, const vec3_t emaxs, const mplane_t *p )
{
	float	dist1, dist2;
	int	sides = 0;

	// general case
	switch( p->signbits )
	{
	case 0:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		break;
	case 1:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		break;
	case 2:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		break;
	case 3:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		break;
	case 4:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		break;
	case 5:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		break;
	case 6:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		break;
	case 7:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		break;
	default:
		// shut up compiler
		dist1 = dist2 = 0;
		break;
	}

	if( dist1 >= p->dist )
		sides = 1;
	if( dist2 < p->dist )
		sides |= 2;

	return sides;
}