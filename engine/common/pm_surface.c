//=======================================================================
//			Copyright XashXT Group 2010 ©
//			pm_surface.c - surface tracing
//=======================================================================

#include "common.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "pm_local.h"

static float PM_DistanceToIntersect( const vec3_t org, const vec3_t dir, const vec3_t origin, const vec3_t normal )
{
	float	d = -(DotProduct( normal, origin ));
	float	numerator = DotProduct( normal, org ) + d;
	float	denominator = DotProduct( normal, dir );
 
	if( fabs( denominator ) < EQUAL_EPSILON )
		return -1.0f;  // normal is orthogonal to vector, no intersection

	return -( numerator / denominator );
}

/*
==================
PM_TraceTexture

find the face where the traceline hit
==================
*/
const char *PM_TraceTexture( physent_t *pe, vec3_t v1, vec3_t v2 )
{
	vec3_t		intersect, temp, vecStartPos;
	msurface_t	**mark, *surf, *hitface = NULL;
	float		d1, d2, min_diff = 9999.9f;
	vec3_t		forward, vecPos1, vecPos2;
	model_t		*bmodel;
	mleaf_t		*endleaf;
	mplane_t		*plane;
	pmtrace_t		trace;
	int		i;

	if( !PM_TraceModel( pe, v1, vec3_origin, vec3_origin, v2, &trace, PM_STUDIO_IGNORE ))
		return NULL; // not intersect

	bmodel = pe->model;
	if( !bmodel || bmodel->type != mod_brush && bmodel->type != mod_world )
		return NULL;

	// making trace adjustments 
	VectorSubtract( v1, pe->origin, vecStartPos );

	// rotate start and end into the models frame of reference
	if( pe->solid == SOLID_BSP && !VectorIsNull( pe->angles ))
	{
		matrix4x4	matrix, imatrix;

		Matrix4x4_CreateFromEntity( matrix, pe->origin[0], pe->origin[1], pe->origin[2], pe->angles[PITCH], pe->angles[YAW], pe->angles[ROLL], 1.0f );
		Matrix4x4_Invert_Simple( imatrix, matrix );

		VectorCopy( vecStartPos, temp );
		Matrix4x4_VectorTransform( imatrix, temp, vecStartPos );
		VectorCopy( trace.endpos, temp );
		Matrix4x4_VectorTransform( imatrix, temp, trace.endpos );
	}

	VectorSubtract( trace.endpos, vecStartPos, forward );
	VectorNormalize( forward );

	// nudge endpos back to can be trace face between two points
	VectorMA( trace.endpos,  5.0f, trace.plane.normal, vecPos1 );
	VectorMA( trace.endpos, -5.0f, trace.plane.normal, vecPos2 );

	endleaf = CM_PointInLeaf( trace.endpos, bmodel->nodes + bmodel->hulls[0].firstclipnode );
	mark = endleaf->firstmarksurface;

	// find a plane with endpos on one side and hitpos on the other side...
	for( i = 0; i < endleaf->nummarksurfaces; i++ )
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
			dist = PM_DistanceToIntersect( vecStartPos, forward, plane_origin, plane->normal );

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