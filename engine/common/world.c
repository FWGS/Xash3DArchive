//=======================================================================
//			Copyright XashXT Group 2009 ©
//		      com_world.c - common worldtrace routines
//=======================================================================

#include "common.h"
#include "world.h"
#include "pm_defs.h"
#include "mathlib.h"

const char *ed_name[] =
{
	"unknown",
	"world",
	"static",
	"ambient",
	"normal",
	"brush",
	"player",
	"monster",
	"tempent",
	"beam",
	"mover",
	"viewmodel",
	"physbody",
	"trigger",
	"portal",
	"skyportal",
	"error",
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
	l->entnum = 0;
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
void InsertLinkBefore( link_t *l, link_t *before, int entnum )
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
	l->entnum = entnum;
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
	if( trace->fAllSolid || trace->fStartSolid || trace->flFraction < cliptrace->flFraction )
	{
		trace->pHit = touch;
		
		if( cliptrace->fStartSolid )
		{
			*cliptrace = *trace;
			cliptrace->fStartSolid = true;
		}
		else *cliptrace = *trace;
	}
	else if( trace->fStartSolid )
		cliptrace->fStartSolid = true;

	return *cliptrace;
}