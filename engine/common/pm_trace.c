/*
pm_trace.c - pmove player trace code
Copyright (C) 2010 Uncle Mike

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
#include "mathlib.h"
#include "mod_local.h"
#include "pm_local.h"
#include "pm_movevars.h"
#include "studio.h"
#include "world.h"

static mplane_t	pm_boxplanes[6];
static dclipnode_t	pm_boxclipnodes[6];
static hull_t	pm_boxhull;

void Pmove_Init( void )
{
	PM_InitBoxHull ();
	PM_InitStudioHull ();
}

/*
===================
PM_InitBoxHull

Set up the planes and clipnodes so that the six floats of a bounding box
can just be stored out and get a proper hull_t structure.
===================
*/
void PM_InitBoxHull( void )
{
	int	i, side;

	pm_boxhull.clipnodes = pm_boxclipnodes;
	pm_boxhull.planes = pm_boxplanes;
	pm_boxhull.firstclipnode = 0;
	pm_boxhull.lastclipnode = 5;

	for( i = 0; i < 6; i++ )
	{
		pm_boxclipnodes[i].planenum = i;
		
		side = i & 1;
		
		pm_boxclipnodes[i].children[side] = CONTENTS_EMPTY;
		if( i != 5 ) pm_boxclipnodes[i].children[side^1] = i + 1;
		else pm_boxclipnodes[i].children[side^1] = CONTENTS_SOLID;
		
		pm_boxplanes[i].type = i>>1;
		pm_boxplanes[i].normal[i>>1] = 1.0f;
		pm_boxplanes[i].signbits = 0;
	}
	
}

/*
===================
PM_HullForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
hull_t *PM_HullForBox( const vec3_t mins, const vec3_t maxs )
{
	pm_boxplanes[0].dist = maxs[0];
	pm_boxplanes[1].dist = mins[0];
	pm_boxplanes[2].dist = maxs[1];
	pm_boxplanes[3].dist = mins[1];
	pm_boxplanes[4].dist = maxs[2];
	pm_boxplanes[5].dist = mins[2];

	return &pm_boxhull;
}

/*
==================
PM_HullPointContents

==================
*/
int PM_HullPointContents( hull_t *hull, int num, const vec3_t p )
{
	mplane_t		*plane;

	while( num >= 0 )
	{
		plane = &hull->planes[hull->clipnodes[num].planenum];
		num = hull->clipnodes[num].children[PlaneDiff( p, plane ) < 0];
	}
	return num;
}

/*
================
PM_HullForEntity

Returns a hull that can be used for testing or clipping an object of mins/maxs size.
Offset is filled in to contain the adjustment that must be added to the
testing object's origin to get a point to use with the returned hull.
================
*/
hull_t *PM_HullForEntity( physent_t *pe, vec3_t mins, vec3_t maxs, vec3_t offset )
{
	hull_t	*hull;
	vec3_t	hullmins, hullmaxs;

	// decide which clipping hull to use, based on the size
	if( pe->model && ( pe->solid == SOLID_BSP || pe->skin == CONTENTS_LADDER ) && pe->model->type == mod_brush )
	{
		vec3_t	size;

		VectorSubtract( maxs, mins, size );

		if( size[0] <= 8.0f )
		{
			hull = &pe->model->hulls[0];
			VectorCopy( hull->clip_mins, offset ); 
		}
		else
		{
			if( size[0] <= 36.0f )
			{
				if( size[2] <= 36.0f )
					hull = &pe->model->hulls[3];
				else hull = &pe->model->hulls[1];
			}
			else hull = &pe->model->hulls[2];

			VectorSubtract( hull->clip_mins, mins, offset );
		}

		// calculate an offset value to center the origin
		VectorAdd( offset, pe->origin, offset );
	}
	else
	{
		// studiomodel or pushable
		// create a temp hull from bounding box sizes
		VectorSubtract( pe->mins, maxs, hullmins );
		VectorSubtract( pe->maxs, mins, hullmaxs );
		hull = PM_HullForBox( hullmins, hullmaxs );
		VectorCopy( pe->origin, offset );
	}
	return hull;
}

/*
==================
PM_HullForBsp

assume physent is valid
==================
*/
hull_t *PM_HullForBsp( physent_t *pe, playermove_t *pmove, float *offset )
{
	hull_t	*hull;

	ASSERT( pe && pe->model != NULL );

	switch( pmove->usehull )
	{
	case 1:
		hull = &pe->model->hulls[3];
		break;
	case 2:
		hull = &pe->model->hulls[0];
		break;
	case 3:
		hull = &pe->model->hulls[2];
		break;
	default:
		hull = &pe->model->hulls[1];
		break;
	}

	ASSERT( hull != NULL );

	// calculate an offset value to center the origin
	VectorSubtract( hull->clip_mins, pmove->player_mins[pmove->usehull], offset );
	VectorAdd( offset, pe->origin, offset );

	return hull;
}

/*
==================
PM_RecursiveHullCheck
==================
*/
qboolean PM_RecursiveHullCheck( hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, pmtrace_t *trace )
{
	dclipnode_t	*node;
	mplane_t		*plane;
	float		t1, t2;
	float		frac, midf;
	int		side;
	vec3_t		mid;
loc0:
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
		Sys_Error( "PM_RecursiveHullCheck: bad node number\n" );
		
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
	{
		num = node->children[0];
		goto loc0;
	}

	if( t1 < 0 && t2 < 0 )
	{
		num = node->children[1];
		goto loc0;
	}

	// put the crosspoint DIST_EPSILON pixels on the near side
	side = (t1 < 0);

	if( side ) frac = ( t1 + DIST_EPSILON ) / ( t1 - t2 );
	else frac = ( t1 - DIST_EPSILON ) / ( t1 - t2 );

	if( frac < 0 ) frac = 0;
	if( frac > 1 ) frac = 1;
		
	midf = p1f + ( p2f - p1f ) * frac;
	VectorLerp( p1, frac, p2, mid );

	// move up to the node
	if( !PM_RecursiveHullCheck( hull, node->children[side], p1f, midf, p1, mid, trace ))
		return false;

	// this recursion can not be optimized because mid would need to be duplicated on a stack
	if( PM_HullPointContents( hull, node->children[side^1], mid ) != CONTENTS_SOLID )
	{
		// go past the node
		return PM_RecursiveHullCheck( hull, node->children[side^1], midf, p2f, mid, p2, trace );
	}	

	// never got out of the solid area
	if( trace->allsolid )
		return false;
		
	// the other side of the node is solid, this is the impact point
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

		if( frac < 0 )
		{
			trace->fraction = midf;
			VectorCopy( mid, trace->endpos );
			MsgDev( D_WARN, "trace backed up past 0.0\n" );
			return false;
		}

		midf = p1f + ( p2f - p1f ) * frac;
		VectorLerp( p1, frac, p2, mid );
	}

	trace->fraction = midf;
	VectorCopy( mid, trace->endpos );

	return false;
}

/*
==================
PM_TraceModel

Handles selection or creation of a clipping hull, and offseting
(and eventually rotation) of the end points
==================
*/
static qboolean PM_BmodelTrace( physent_t *pe, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, pmtrace_t *ptr ) 
{
	vec3_t	offset, temp;
	vec3_t	start_l, end_l;
	matrix4x4	matrix;
	hull_t	*hull;

	// assume we didn't hit anything
	Q_memset( ptr, 0, sizeof( pmtrace_t ));
	VectorCopy( end, ptr->endpos );
	ptr->allsolid = true;
	ptr->fraction = 1.0f;
	ptr->hitgroup = -1;
	ptr->ent = -1;

	// get the clipping hull
	hull = PM_HullForEntity( pe, mins, maxs, offset );

	ASSERT( hull != NULL );

	VectorSubtract( start, offset, start_l );
	VectorSubtract( end, offset, end_l );

	// rotate start and end into the models frame of reference
	if( pe->solid == SOLID_BSP && !VectorIsNull( pe->angles ))
	{
		matrix4x4	imatrix;

		Matrix4x4_CreateFromEntity( matrix, pe->angles, offset, 1.0f );
		Matrix4x4_Invert_Simple( imatrix, matrix );

		Matrix4x4_VectorTransform( imatrix, start, start_l );
		Matrix4x4_VectorTransform( imatrix, end, end_l );
	}

	// do trace
	PM_RecursiveHullCheck( hull, hull->firstclipnode, 0, 1, start_l, end_l, ptr );

	// rotate endpos back to world frame of reference
	if( pe->solid == SOLID_BSP && !VectorIsNull( pe->angles ))
	{
		if( ptr->fraction != 1.0f )
		{
			// compute endpos
			VectorCopy( ptr->plane.normal, temp );
			VectorLerp( start, ptr->fraction, end, ptr->endpos );
			Matrix4x4_TransformPositivePlane( matrix, temp, ptr->plane.dist, ptr->plane.normal, &ptr->plane.dist );
		}
	}
	else
	{
		// special case for non-rotated bmodels
		if( ptr->fraction != 1.0f )
		{
			// fix trace up by the offset when we hit bmodel
			VectorAdd( ptr->endpos, offset, ptr->endpos );
		}
		ptr->plane.dist = DotProduct( ptr->endpos, ptr->plane.normal );
	}

	// did we clip the move?
	if( ptr->fraction < 1.0f || ptr->startsolid )
		return true;
	return false;
}

qboolean PM_TraceModel( playermove_t *pmove, physent_t *pe, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, pmtrace_t *ptr, int flags )
{
	qboolean	hitEnt = false;
	qboolean	bSimpleBox = false;

	// assume we didn't hit anything
	Q_memset( ptr, 0, sizeof( pmtrace_t ));
	VectorCopy( end, ptr->endpos );
	ptr->fraction = 1.0f;
	ptr->hitgroup = -1;
	ptr->ent = -1;

	// completely ignore studiomodels (same as MOVE_NOMONSTERS)
	if( pe->studiomodel && ( flags & PM_STUDIO_IGNORE ))
		return hitEnt;

	if( pe->movetype == MOVETYPE_PUSH || pe->solid == SOLID_BSP )
	{
		if( flags & PM_GLASS_IGNORE )
		{
			// we ignore brushes with rendermode != kRenderNormal
			switch( pe->rendermode )
			{
			case kRenderTransAdd:
			case kRenderTransAlpha:
			case kRenderTransTexture:
				// passed through glass
				return hitEnt;
			default: break;
			}
		}

		if( !pe->model )
			return hitEnt;
		hitEnt = PM_BmodelTrace( pe, start, mins, maxs, end, ptr );
	}
	else
	{
		bSimpleBox = ( flags & PM_STUDIO_BOX ) ? true : false;
		bSimpleBox = World_UseSimpleBox( bSimpleBox, pe->solid, VectorCompare( mins, maxs ), pe->studiomodel );

		if( bSimpleBox ) hitEnt = PM_BmodelTrace( pe, start, mins, maxs, end, ptr );
		else hitEnt = PM_StudioTrace( pmove, pe, start, mins, maxs, end, ptr );
	}
	return hitEnt;
}

/*
================
PM_PlayerTrace
================
*/
pmtrace_t PM_PlayerTrace( playermove_t *pmove, vec3_t start, vec3_t end, int flags, int usehull, int ignore_pe, pfnIgnore pmFilter )
{
	physent_t	*pe;
	pmtrace_t	trace, total;
	float	*mins = pmove->player_mins[usehull];
	float	*maxs = pmove->player_maxs[usehull];
	int	i;

	// assume we didn't hit anything
	Q_memset( &total, 0, sizeof( pmtrace_t ));
	VectorCopy( end, total.endpos );
	total.fraction = 1.0f;
	total.hitgroup = -1;
	total.ent = -1;

	for( i = 0; i < pmove->numphysent; i++ )
	{
		// run simple trace filter
		if( i == ignore_pe ) continue;

		pe = &pmove->physents[i];

		// run custom user filter
		if( pmFilter && pmFilter( pe ))
			continue;

		if(( i > 0 ) && !VectorIsNull( mins ) && VectorIsNull( pe->mins ))
			continue;	// points never interact

		// might intersect, so do an exact clip
		if( total.allsolid ) return total;

		if( PM_TraceModel( pmove, pe, start, mins, maxs, end, &trace, flags ))
		{
			// set entity
			trace.ent = i;
		}

		if( trace.allsolid || trace.fraction < total.fraction )
		{
			trace.ent = i;

			if( total.startsolid )
			{
				total = trace;
				total.startsolid = true;
			}
			else total = trace;
		}
		else if( trace.startsolid )
		{
			total.startsolid = true;
			total.ent = i;
		}

		if( total.startsolid )
			total.fraction = 0.0f;

		if( i == 0 && ( flags & PM_WORLD_ONLY ))
			break; // done

	}
	return total;
}

int PM_TestPlayerPosition( playermove_t *pmove, vec3_t pos, pfnIgnore pmFilter )
{
	physent_t	*pe;
	hull_t	*hull;
	float	*mins = pmove->player_mins[pmove->usehull];
	float	*maxs = pmove->player_maxs[pmove->usehull];
	vec3_t	offset, pos_l;
	int	i;

	for( i = 0; i < pmove->numphysent; i++ )
	{

		pe = &pmove->physents[i];

		if( pmFilter != NULL && pmFilter( pe ))
			continue;

		if( pe->model && pe->solid == SOLID_NOT && pe->skin != 0 )
			continue;

		// FIXME: check studiomodels with flag 512 by each hitbox ?
		// we ignore it for now
		if( pe->studiomodel && pe->studiomodel->flags & STUDIO_TRACE_HITBOX )
			continue;

		hull = PM_HullForEntity( pe, mins, maxs, offset );

		// offset the test point appropriately for this hull.
		VectorSubtract( pos, offset, pos_l );

		// CM_TransformedPointContents :-)
		if( pe->solid == SOLID_BSP && !VectorIsNull( pe->angles ))
		{
			matrix4x4	matrix, imatrix;

			Matrix4x4_CreateFromEntity( matrix, pe->angles, offset, 1.0f );
			Matrix4x4_Invert_Simple( imatrix, matrix );
			Matrix4x4_VectorTransform( imatrix, pos, pos_l );
		}

		if( PM_HullPointContents( hull, hull->firstclipnode, pos_l ) == CONTENTS_SOLID )
			return i;
	}

	return -1; // didn't hit anything
}