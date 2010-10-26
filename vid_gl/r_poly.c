/*
Copyright (C) 2002-2007 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// r_poly.c - handles fragments and arbitrary polygons

#include "r_local.h"
#include "mathlib.h"
#include "matrix_lib.h"

static mesh_t poly_mesh;

/*
=================
R_PushPoly
=================
*/
void R_PushPoly( const meshbuffer_t *mb )
{
	poly_t		*p;
	ref_shader_t	*shader;
	int		i, j, features;

	MB_NUM2SHADER( mb->shaderkey, shader );

	features = ( shader->features|MF_TRIFAN );

	for( i = -mb->infokey-1, p = r_polys + i; i < mb->lastPoly; i++, p++ )
	{
		poly_mesh.numVerts = p->numverts;
		poly_mesh.vertexArray = inVertsArray;
		poly_mesh.normalsArray = inNormalsArray;
		poly_mesh.stCoordArray = p->stcoords;
		poly_mesh.colorsArray = p->colors;

		for( j = 0; j < p->numverts; j++ )
		{
			Vector4Set( inVertsArray[r_backacc.numVerts+j], p->verts[j][0], p->verts[j][1], p->verts[j][2], 1 );
			VectorCopy( p->normal, inNormalsArray[r_backacc.numVerts+j] );
		}

		R_PushMesh( &poly_mesh, features );
	}
}

/*
=================
R_AddPolysToList
=================
*/
void R_AddPolysToList( void )
{
	uint		i, nverts = 0;
	int		fognum = -1;
	mfog_t		*fog, *lastFog = NULL;
	meshbuffer_t	*mb = NULL;
	ref_shader_t	*shader;
	vec3_t		lastNormal = { 0, 0, 0 };
	poly_t		*p;

	RI.currententity = r_worldent;

	for( i = 0, p = r_polys; i < r_numPolys; nverts += p->numverts, mb->lastPoly++, i++, p++ )
	{
		shader = p->shader;
		if( p->fognum < 0 ) fognum = -1;
		else if( p->fognum ) fognum = bound( 1, p->fognum, r_worldbrushmodel->numfogs + 1 );
		else fognum = r_worldbrushmodel->numfogs ? 0 : -1;

		if( fognum == -1 ) fog = NULL;
		else if( !fognum ) fog = R_FogForSphere( p->verts[0], 0 );
		else fog = r_worldbrushmodel->fogs + fognum - 1;

		// we ignore SHADER_ENTITY_MERGABLE here because polys are just regular trifans
		if( !mb || mb->shaderkey != (int)shader->sortkey
			|| lastFog != fog || nverts + p->numverts > MAX_ARRAY_VERTS
			|| (( shader->flags & SHADER_MATERIAL ) && !VectorCompare( p->normal, lastNormal )))
		{
			nverts = 0;
			lastFog = fog;
			VectorCopy( p->normal, lastNormal );

			mb = R_AddMeshToList( MB_POLY, fog, shader, -( (signed int)i+1 ) );
			mb->lastPoly = i;
		}
	}
}

//==================================================================================

static int r_fragmentframecount;
static vec3_t trace_start, trace_end;
static vec3_t trace_absmins, trace_absmaxs;
static float trace_fraction;

static vec3_t trace_impact;
static mplane_t trace_plane;
static msurface_t *trace_surface;
static int trace_hitbox;

/*
=================
R_TraceAgainstTriangle

Ray-triangle intersection as per
http://geometryalgorithms.com/Archive/algorithm_0105/algorithm_0105.htm
(original paper by Dan Sunday)
=================
*/
static void R_TraceAgainstTriangle( const vec_t *a, const vec_t *b, const vec_t *c )
{
	const vec_t *p1 = trace_start, *p2 = trace_end, *p0 = a;
	vec3_t u, v, w, n, p;
	float d1, d2, d, frac;
	float uu, uv, vv, wu, wv, s, t;

	// calculate two mostly perpendicular edge directions
	VectorSubtract( b, p0, u );
	VectorSubtract( c, p0, v );

	// we have two edge directions, we can calculate the normal
	CrossProduct( v, u, n );
	if( VectorCompare( n, vec3_origin ) )
		return;		// degenerate triangle

	VectorSubtract( p2, p1, p );
	VectorSubtract( p1, p0, w );

	d1 = -DotProduct( n, w );
	d2 = DotProduct( n, p );
	if( fabs( d2 ) < 0.0001 )
		return;

	// get intersect point of ray with triangle plane
	frac = (d1) / d2;
	if( frac <= 0 ) return;
	if( frac >= trace_fraction )
		return;		// we have hit something earlier

	// calculate the impact point
	VectorLerp( p1, frac, p2, p );

	// does p lie inside triangle?
	uu = DotProduct( u, u );
	uv = DotProduct( u, v );
	vv = DotProduct( v, v );

	VectorSubtract( p, p0, w );
	wu = DotProduct( w, u );
	wv = DotProduct( w, v );
	d = uv * uv - uu * vv;

	// get and test parametric coords

	s = (uv * wv - vv * wu) / d;
	if( s < 0.0 || s > 1.0 )
		return;		// p is outside

	t = (uv * wu - uu * wv) / d;
	if( t < 0.0 || (s + t) > 1.0 )
		return;		// p is outside

	trace_fraction = frac;
	VectorCopy( p, trace_impact );
	VectorCopy( n, trace_plane.normal );
}

/*
=================
R_TraceAgainstSurface
=================
*/
static qboolean R_TraceAgainstSurface( msurface_t *surf )
{
	int	i;
	mesh_t	*mesh;
	elem_t	*elem;
	vec4_t	*verts;
	float	old_frac = trace_fraction;

	if( !surf ) return false;
	mesh = surf->mesh;
	if( !mesh ) return false;
	elem = mesh->elems;
	if( !elem ) return false;
	verts = mesh->vertexArray;		
	if( !verts ) return false;

	// clip each triangle individually
	for( i = 0; i < mesh->numElems; i += 3, elem += 3 )
	{
		R_TraceAgainstTriangle( verts[elem[0]], verts[elem[1]], verts[elem[2]] );
		if( old_frac > trace_fraction )
		{
			// flip normal is we are on the backside (does it really happen?)...
			if( DotProduct( trace_plane.normal, surf->plane->normal ) < 0 )
				VectorNegate( trace_plane.normal, trace_plane.normal );
			return true;
		}
	}
	return false;
}

/*
=================
R_TraceAgainstLeaf
=================
*/
static int R_TraceAgainstLeaf( mleaf_t *leaf )
{
	msurface_t	*surf, **mark;
	int		i;

	if( leaf->contents == CONTENTS_SOLID )
		return 1;	// solid leaf

	mark = leaf->firstMarkSurface;

	for( i = 0, mark = leaf->firstMarkSurface; i < leaf->numMarkSurfaces; i++, mark++ )
	{
		surf = *mark;

		if( surf->fragmentframe == r_fragmentframecount )
			continue;	// do not test the same surface more than once
		surf->fragmentframe = r_fragmentframecount;

		if( surf->mesh )
		{
			if( R_TraceAgainstSurface( surf ) )
				trace_surface = surf;	// impact surface
		}
	}
	return 0;
}

/*
=================
R_TraceAgainstBmodel
=================
*/
static int R_TraceAgainstBmodel( mbrushmodel_t *bmodel )
{
	int i;
	msurface_t *surf;

	for( i = 0; i < bmodel->nummodelsurfaces; i++ )
	{
		surf = bmodel->firstmodelsurface + i;

		if( R_TraceAgainstSurface( surf ) )
			trace_surface = surf;	// impact point
	}

	return 0;
}

/*
=================
R_RecursiveHullCheck
=================
*/
static int R_RecursiveHullCheck( mnode_t *node, const vec3_t start, const vec3_t end )
{
	int side, r;
	float t1, t2;
	float frac;
	vec3_t mid;
	const vec_t *p1 = start, *p2 = end;
	mplane_t *plane;

loc0:
	plane = node->plane;
	if( !plane )
		return R_TraceAgainstLeaf( ( mleaf_t * )node );

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

	if( t1 >= -ON_EPSILON && t2 >= -ON_EPSILON )
	{
		node = node->children[0];
		goto loc0;
	}
	
	if( t1 < ON_EPSILON && t2 < ON_EPSILON )
	{
		node = node->children[1];
		goto loc0;
	}

	side = t1 < 0;
	frac = t1 / (t1 - t2);
	VectorLerp( p1, frac, p2, mid );

	r = R_RecursiveHullCheck( node->children[side], p1, mid );
	if( r )
		return r;

	return R_RecursiveHullCheck( node->children[!side], mid, p2 );
}

/*
=================
R_TraceLine
=================
*/
msurface_t *R_TransformedTraceLine( pmtrace_t *tr, const vec3_t start, const vec3_t end, ref_entity_t *test, int flags )
{
	ref_model_t	*model;

	r_fragmentframecount++;	// for multi-check avoidance

	// fill in a default trace
	Mem_Set( tr, 0, sizeof( pmtrace_t ));

	trace_hitbox = -1;
	trace_surface = NULL;
	trace_fraction = 1.0f;
	VectorCopy( end, trace_impact );
	Mem_Set( &trace_plane, 0, sizeof( trace_plane ));

	// skip glass ents
	if(( flags & FTRACE_IGNORE_GLASS ) && test->rendermode != kRenderNormal && test->renderamt < 200 )
	{
		tr->fraction = trace_fraction;
		VectorCopy( trace_impact, tr->endpos );

		return trace_surface;
	}

	ClearBounds( trace_absmins, trace_absmaxs );
	AddPointToBounds( start, trace_absmins, trace_absmaxs );
	AddPointToBounds( end, trace_absmins, trace_absmaxs );

	model = test->model;
	if( model )
	{
		if( model->type == mod_world || model->type == mod_brush || model->type == mod_studio )
		{
			mbrushmodel_t	*bmodel = (mbrushmodel_t *)model->extradata;
			qboolean		rotated = !Matrix3x3_Compare( test->axis, matrix3x3_identity );
			vec3_t		temp, start_l, end_l, axis[3];
			vec3_t		model_mins, model_maxs;
			float		v, max = 0.0f;
			int		i;

			// transform
			VectorSubtract( start, test->origin, start_l );
			VectorSubtract( end, test->origin, end_l );

			if( rotated )
			{
				VectorCopy( start_l, temp );
				Matrix3x3_Transform( test->axis, temp, start_l );
				VectorCopy( end_l, temp );
				Matrix3x3_Transform( test->axis, temp, end_l );

				// expand mins/maxs for rotation
				for( i = 0; i < 3; i++ )
				{
					v = fabs( model->mins[i] );
					if( v > max ) max = v;
					v = fabs( model->maxs[i] );
					if( v > max ) max = v;
				}

				for( i = 0; i < 3; i++ )
				{
					model_mins[i] = test->origin[i] - max;
					model_maxs[i] = test->origin[i] + max;
				}

			}
			else
			{
				VectorAdd( test->origin, model->mins, model_mins );
				VectorAdd( test->origin, model->maxs, model_maxs ); 
			}

			VectorCopy( start_l, trace_start );
			VectorCopy( end_l, trace_end );

			// world uses a recursive approach using BSP tree, submodels
			// just walk the list of surfaces linearly
			if( model->type == mod_world )
				R_RecursiveHullCheck( bmodel->nodes, start_l, end_l );
			else if( BoundsIntersect( model_mins, model_maxs, trace_absmins, trace_absmaxs ))
			{
				if( model->type == mod_brush )
					R_TraceAgainstBmodel( bmodel );
				else if( model->type == mod_studio )
				{
					pmtrace_t	tr;

					if( R_StudioTrace( test, trace_start, trace_end, &tr ))
					{
						VectorCopy( tr.endpos, trace_impact );
						VectorCopy( tr.plane.normal, trace_plane.normal );
						trace_fraction = tr.fraction;
						trace_hitbox = tr.hitgroup;
					}
				}
			}

			// transform back
			if( rotated && trace_fraction != 1 )
			{
				Matrix3x3_Transpose( axis, test->axis );
				VectorCopy( tr->plane.normal, temp );
				Matrix3x3_Transform( axis, temp, trace_plane.normal );
			}
		}
	}

	// calculate the impact plane, if any
	if( trace_fraction < 1.0f )
	{
		VectorNormalize( trace_plane.normal );
		trace_plane.dist = DotProduct( trace_plane.normal, trace_impact );
		CategorizePlane( &trace_plane );

		tr->plane.dist = trace_plane.dist;
		VectorCopy( trace_plane.normal, tr->plane.normal );
		tr->ent = test - r_entities;
	}

	tr->hitgroup = trace_hitbox;	
	tr->fraction = trace_fraction;
	VectorCopy( trace_impact, tr->endpos );

	return trace_surface;
}
