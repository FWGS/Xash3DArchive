//=======================================================================
//			Copyright XashXT Group 2009 ©
//			cm_trace.c - geometry tracing
//=======================================================================

#include "cm_local.h"
#include "mathlib.h"
#include "matrix_lib.h"

float CM_DistanceToIntersect( const vec3_t org, const vec3_t dir, const vec3_t plane_origin, const vec3_t plane_normal )
{
	float	d = -(DotProduct( plane_normal, plane_origin ));
	float	numerator = DotProduct( plane_normal, org ) + d;
	float	denominator = DotProduct( plane_normal, dir );
 
	if( fabs( denominator ) < EQUAL_EPSILON )
		return -1.0f;  // normal is orthogonal to vector, no intersection

	return -( numerator / denominator );
}

/*
===============================================================================

BOX TRACING

===============================================================================
*/
/*
================
SV_HullForEntity

Returns a hull that can be used for testing or clipping an object of mins/maxs size.
Offset is filled in to contain the adjustment that must be added to the
testing object's origin to get a point to use with the returned hull.
================
*/
chull_t *CM_HullForEntity( edict_t *ent, vec3_t mins, vec3_t maxs, vec3_t offset )
{
	chull_t		*hull;
	cmodel_t		*model;
	vec3_t		size, hullmins, hullmaxs;

	// decide which clipping hull to use, based on the size
	if( ent->v.solid == SOLID_BSP )
	{
		// explicit hulls in the BSP model
		if( ent->v.movetype != MOVETYPE_PUSH )
			Host_Error( "Entity %i SOLID_BSP without MOVETYPE_PUSH\n", ent->serialnumber );

		model = CM_ClipHandleToModel( ent->v.modelindex );

		if( !model || model->type != mod_brush && model->type != mod_world )
			Host_Error( "Entity %i SOLID_BSP with a non bsp model\n", ent->serialnumber );

		VectorSubtract( maxs, mins, size );

		if( size[0] < 3 )
		{
			// point hull
			hull = &model->hulls[0];
		}
		else if( size[0] <= 36 )
		{
			if( size[2] < 36 )
			{
				// head hull (ducked)
				hull = &model->hulls[3];
			}
			else
			{
				// human hull
				hull = &model->hulls[1];
			}
		}
		else
		{
			// large hull
			hull = &model->hulls[2];
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
		hull = CM_HullForBox( hullmins, hullmaxs );
		VectorCopy( ent->v.origin, offset );
	}
	return hull;
}

/*
==================
CM_HullForBsp

assume edict is valid
==================
*/
chull_t *CM_HullForBsp( edict_t *ent, const vec3_t mins, const vec3_t maxs, float *offset )
{
	chull_t		*hull;
	cmodel_t		*model;
	vec3_t		size;

	// decide which clipping hull to use, based on the size
	model = CM_ClipHandleToModel( ent->v.modelindex );

	if( !model || ( model->type != mod_brush && model->type != mod_world ))
		Host_Error( "Entity %i SOLID_BSP with a non bsp model %i\n", ent->serialnumber, model->type );

	VectorSubtract( maxs, mins, size );

	if( size[0] < 3 )
	{
		// point hull
		hull = &model->hulls[0];
	}
	else if( size[0] < 36 )
	{
		if( size[2] < 36 )
		{
			// head hull (ducked)
			hull = &model->hulls[3];
		}
		else
		{
			// human hull
			hull = &model->hulls[1];
		}
	}
	else
	{
		// large hull
		hull = &model->hulls[2];
	}

	// calculate an offset value to center the origin
	VectorSubtract( hull->clip_mins, mins, offset );
	VectorAdd( offset, ent->v.origin, offset );

	return hull;
}

/*
==================
CM_RecursiveHullCheck
==================
*/
bool CM_RecursiveHullCheck( chull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t *trace )
{
	clipnode_t	*node;
	cplane_t		*plane;
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
			trace->fAllSolid = false;
			if( num == CONTENTS_EMPTY )
				trace->fInOpen = true;
			else trace->fInWater = true;
		}
		else trace->fStartSolid = true;
		return true; // empty
	}

	if( num < hull->firstclipnode || num > hull->lastclipnode )
		Host_Error( "CM_RecursiveHullCheck: bad node number\n" );
		
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

	if( side ) frac = bound( 0, ( t1 + DIST_EPSILON ) / ( t1 - t2 ), 1 );
	else frac = bound( 0, ( t1 - DIST_EPSILON ) / ( t1 - t2 ), 1 );
		
	midf = p1f + ( p2f - p1f ) * frac;
	VectorLerp( p1, frac, p2, mid );

	// move up to the node
	if( !CM_RecursiveHullCheck( hull, node->children[side], p1f, midf, p1, mid, trace ))
		return false;

	// this recursion can not be optimized because mid would need to be duplicated on a stack
	if( CM_HullPointContents( hull, node->children[side^1], mid ) != CONTENTS_SOLID )
	{
		// go past the node
		return CM_RecursiveHullCheck( hull, node->children[side^1], midf, p2f, mid, p2, trace );
	}	

	// never got out of the solid area
	if( trace->fAllSolid )
		return false;
		
	// the other side of the node is solid, this is the impact point
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

	while( CM_HullPointContents( hull, hull->firstclipnode, mid ) == CONTENTS_SOLID )
	{
		// shouldn't really happen, but does occasionally
		frac -= 0.1f;

		if( frac < 0 )
		{
			trace->flFraction = midf;
			VectorCopy( mid, trace->vecEndPos );
			MsgDev( D_INFO, "backup past 0\n" );
			return false;
		}

		midf = p1f + ( p2f - p1f ) * frac;
		VectorLerp( p1, frac, p2, mid );
	}

	trace->flFraction = midf;
	VectorCopy( mid, trace->vecEndPos );

	return false;
}

/*
==================
CM_ClipMoveToEntity

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
trace_t CM_ClipMoveToEntity( edict_t *ent, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int flags )
{
	vec3_t	offset;
	vec3_t	start_l, end_l;
	trace_t	trace;
	chull_t	*hull;

	// fill in a default trace
	Mem_Set( &trace, 0, sizeof( trace_t ));
	VectorCopy( end, trace.vecEndPos );
	trace.flFraction = 1.0f;
	trace.fAllSolid = true;
	trace.iHitgroup = -1;

	// get the clipping hull
	hull = CM_HullForEntity( ent, mins, maxs, offset );

	VectorSubtract( start, offset, start_l );
	VectorSubtract( end, offset, end_l );

	// rotate start and end into the models frame of reference
	if( ent->v.solid == SOLID_BSP && !VectorIsNull( ent->v.angles ))
	{
		vec3_t	forward, right, up;
		vec3_t	temp;

		VectorCopy( ent->v.angles, temp );
		AngleVectors( temp, forward, right, up );

		VectorCopy( start_l, temp );
		start_l[0] = DotProduct( temp, forward );
		start_l[1] = -DotProduct( temp, right );
		start_l[2] = DotProduct( temp, up );

		VectorCopy( end_l, temp );
		end_l[0] = DotProduct( temp, forward );
		end_l[1] = -DotProduct( temp, right );
		end_l[2] = DotProduct( temp, up );
	}

	if(!( flags & FMOVE_SIMPLEBOX ) && CM_ModelType( ent->v.modelindex ) == mod_studio )
	{
		if( CM_StudioTrace( ent, start, end, &trace )); // continue tracing bbox if hitbox missing
		else CM_RecursiveHullCheck( hull, hull->firstclipnode, 0, 1, start_l, end_l, &trace );
	}
          else
          {
		// trace a line through the apropriate clipping hull
		CM_RecursiveHullCheck( hull, hull->firstclipnode, 0, 1, start_l, end_l, &trace );
	}

	// rotate endpos back to world frame of reference
	if( ent->v.solid == SOLID_BSP && !VectorIsNull( ent->v.angles ))
	{
		vec3_t	forward, right, up;
		vec3_t	temp;

		if( trace.flFraction != 1.0f )
		{
			VectorNegate( ent->v.angles, temp );
			AngleVectors( temp, forward, right, up );

			VectorCopy( trace.vecEndPos, temp );
			trace.vecEndPos[0] = DotProduct( temp, forward );
			trace.vecEndPos[1] = -DotProduct( temp, right );
			trace.vecEndPos[2] = DotProduct( temp, up );

			VectorCopy( trace.vecPlaneNormal, temp );
			trace.vecPlaneNormal[0] = DotProduct( temp, forward );
			trace.vecPlaneNormal[1] = -DotProduct( temp, right );
			trace.vecPlaneNormal[2] = DotProduct( temp, up );
		}
	}

	// fix trace up by the offset when we hit bmodel
	if( trace.flFraction != 1.0f && trace.iHitgroup == -1 )
		VectorAdd( trace.vecEndPos, offset, trace.vecEndPos );

	// did we clip the move?
	if( trace.flFraction < 1.0f || trace.fStartSolid )
		trace.pHit = ent;

	if(!( flags & FMOVE_SIMPLEBOX ) && CM_ModelType( ent->v.modelindex ) == mod_studio )
	{
		if( VectorIsNull( mins ) && VectorIsNull( maxs ) && trace.iHitgroup == -1 )
		{
			// NOTE: studio traceline doesn't check studio bbox hitboxes only
			trace.flFraction = 1.0f;
			trace.pHit = NULL;	// clear entity when any hitbox not intersected
		}
	}
	return trace;
}

/*
==================
CM_TraceTexture

find the face where the traceline hit
==================
*/
const char *CM_TraceTexture( const vec3_t start, trace_t trace )
{
	vec3_t		intersect, forward, temp, vecStartPos;
	csurface_t	**mark, *surf, *hitface = NULL;
	float		d1, d2, min_diff = 9999.9f;
	vec3_t		vecPos1, vecPos2;
	cmodel_t		*bmodel;
	cleaf_t		*endleaf;
	cplane_t		*plane;
	int		i;

	if( !trace.pHit ) return NULL; // trace entity must be valid

	bmodel = CM_ClipHandleToModel( trace.pHit->v.modelindex );
	if( !bmodel || bmodel->type != mod_brush && bmodel->type != mod_world )
		return NULL;

	// making trace adjustments 
	VectorSubtract( start, trace.pHit->v.origin, vecStartPos );
	VectorSubtract( trace.vecEndPos, trace.pHit->v.origin, trace.vecEndPos );

	VectorSubtract( trace.vecEndPos, vecStartPos, forward );
	VectorNormalize( forward );

	// nudge endpos back to can be trace face between two points
	VectorMA( trace.vecEndPos,  5.0f, trace.vecPlaneNormal, vecPos1 );
	VectorMA( trace.vecEndPos, -5.0f, trace.vecPlaneNormal, vecPos2 );

	endleaf = CM_PointInLeaf( trace.vecEndPos, bmodel->nodes + bmodel->hulls[0].firstclipnode );
	mark = endleaf->firstMarkSurface;

	// find a plane with endpos on one side and hitpos on the other side...
	for( i = 0; i < endleaf->numMarkSurfaces; i++ )
	{
		surf = *mark++;
		plane = surf->plane;

		d1 = DotProduct( vecPos1, plane->normal ) - plane->dist;
		d2 = DotProduct( vecPos2, plane->normal ) - plane->dist;

		if(( d1 > 0.0f && d2 <= 0.0f ) || ( d1 <= 0.0f && d2 > 0.0f ))
		{
			// found a plane, find the intersection point in the plane...
			vec3_t	plane_origin, angle1, angle2;
			float	dist, anglef, angle_sum = 0.0f;
			int	i, e;

			VectorScale( plane->normal, plane->dist, plane_origin );
			dist = CM_DistanceToIntersect( vecStartPos, forward, plane_origin, plane->normal );

			if( dist < 0.0f ) return NULL; // can't find intersection

			VectorScale( forward, dist, temp );
			VectorAdd( vecStartPos, temp, intersect );

			// loop through all of the vertexes of all the edges of this face and
			// find the angle between vertex-n, v_intersect and vertex-n+1, then add
			// all these angles together.  if the sum of these angles is 360 degrees
			// (or 2 PI radians), then the intersect point lies within that polygon.
			// loop though all of the edges, getting the vertexes...
			for( i = 0; i < surf->numedges; i++ )
			{
				vec3_t	vertex1, vertex2;

				// get the coordinates of the vertex of this edge...
				e = worldmodel->surfedges[surf->firstedge + i];

				if( e < 0 )
				{
					VectorCopy(worldmodel->vertexes[worldmodel->edges[-e].v[1]], vertex1);
					VectorCopy(worldmodel->vertexes[worldmodel->edges[-e].v[0]], vertex2);
				}
				else
				{
					VectorCopy(worldmodel->vertexes[worldmodel->edges[e].v[0]], vertex1);
					VectorCopy(worldmodel->vertexes[worldmodel->edges[e].v[1]], vertex2);
				}

				// now create vectors from the vertexes to the plane intersect point...
				VectorSubtract( vertex1, intersect, angle1 );
				VectorSubtract( vertex2, intersect, angle2 );
	
				VectorNormalize( angle1 );
				VectorNormalize( angle2 );

				// find the angle between these vectors...
				anglef = DotProduct( angle1, angle2 );

				anglef = acos( anglef );
				angle_sum += anglef;
			}

			// is the sum of the angles 360 degrees (2 PI)?...
			if(( angle_sum >= ( M_PI2 - EQUAL_EPSILON )) && ( angle_sum <= ( M_PI2 + EQUAL_EPSILON )))
			{
				// find the difference between the sum and 2 PI...
				float	diff = fabs( angle_sum - M_PI2 );

				if( diff < min_diff )  // is this the BEST so far?...
				{
					min_diff = diff;
					hitface = surf;
				}
			}
		}
	}

	if( hitface && hitface->texinfo && hitface->texinfo->texture )
		return hitface->texinfo->texture->name;
	return NULL;
}