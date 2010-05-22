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

static int numFragmentVerts;
static int maxFragmentVerts;
static vec3_t *fragmentVerts;

static int numClippedFragments;
static int maxClippedFragments;
static fragment_t *clippedFragments;

static cplane_t fragmentPlanes[6];
static vec3_t fragmentNormal;
static float fragmentDiameterSquared;

static int r_fragmentframecount;

#define	MAX_FRAGMENT_VERTS  64

/*
=================
R_WindingClipFragment

This function operates on windings (convex polygons without
any points inside) like triangles, quads, etc. The output is
a convex fragment (polygon, trifan) which the result of clipping
the input winding by six fragment planes.
=================
*/
static bool R_WindingClipFragment( vec3_t *wVerts, int numVerts, msurface_t *surf, vec3_t snorm )
{
	int i, j;
	int stage, newc, numv;
	cplane_t *plane;
	bool front;
	float *v, *nextv, d;
	float dists[MAX_FRAGMENT_VERTS+1];
	int sides[MAX_FRAGMENT_VERTS+1];
	vec3_t *verts, *newverts, newv[2][MAX_FRAGMENT_VERTS], t;
	fragment_t *fr;

	numv = numVerts;
	verts = wVerts;

	for( stage = 0, plane = fragmentPlanes; stage < 6; stage++, plane++ )
	{
		for( i = 0, v = verts[0], front = false; i < numv; i++, v += 3 )
		{
			d = PlaneDiff( v, plane );

			if( d > ON_EPSILON )
			{
				front = true;
				sides[i] = SIDE_FRONT;
			}
			else if( d < -ON_EPSILON )
			{
				sides[i] = SIDE_BACK;
			}
			else
			{
				front = true;
				sides[i] = SIDE_ON;
			}
			dists[i] = d;
		}

		if( !front )
			return false;

		// clip it
		sides[i] = sides[0];
		dists[i] = dists[0];

		newc = 0;
		newverts = newv[stage & 1];
		for( i = 0, v = verts[0]; i < numv; i++, v += 3 )
		{
			switch( sides[i] )
			{
			case SIDE_FRONT:
				if( newc == MAX_FRAGMENT_VERTS )
					return false;
				VectorCopy( v, newverts[newc] );
				newc++;
				break;
			case SIDE_BACK:
				break;
			case SIDE_ON:
				if( newc == MAX_FRAGMENT_VERTS )
					return false;
				VectorCopy( v, newverts[newc] );
				newc++;
				break;
			}

			if( sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i] )
				continue;
			if( newc == MAX_FRAGMENT_VERTS )
				return false;

			d = dists[i] / ( dists[i] - dists[i+1] );
			nextv = ( i == numv - 1 ) ? verts[0] : v + 3;
			for( j = 0; j < 3; j++ )
				newverts[newc][j] = v[j] + d * ( nextv[j] - v[j] );
			newc++;
		}

		if( newc <= 2 )
			return false;

		// continue with new verts
		numv = newc;
		verts = newverts;
	}

	// fully clipped
	if( numFragmentVerts + numv > maxFragmentVerts )
		return false;

	fr = &clippedFragments[numClippedFragments++];
	fr->numverts = numv;
	fr->firstvert = numFragmentVerts;
	fr->fognum = surf->fog ? surf->fog - r_worldbrushmodel->fogs + 1 : -1;
	VectorCopy( snorm, fr->normal );
	for( i = 0, v = verts[0], nextv = fragmentVerts[numFragmentVerts]; i < numv; i++, v += 3, nextv += 3 )
		VectorCopy( v, nextv );

	numFragmentVerts += numv;
	if( numFragmentVerts == maxFragmentVerts && numClippedFragments == maxClippedFragments )
		return true;

	// if all of the following is true:
	// a) all clipping planes are perpendicular
	// b) there are 4 in a clipped fragment
	// c) all sides of the fragment are equal (it is a quad)
	// d) all sides are radius*2 +- epsilon (0.001)
	// then it is safe to assume there's only one fragment possible
	// not sure if it's 100% correct, but sounds convincing
	if( numv == 4 )
	{
		for( i = 0, v = verts[0]; i < numv; i++, v += 3 )
		{
			nextv = ( i == 3 ) ? verts[0] : v + 3;
			VectorSubtract( v, nextv, t );

			d = fragmentDiameterSquared - DotProduct( t, t );
			if( d > 0.01 || d < -0.01 )
				return false;
		}
		return true;
	}

	return false;
}

/*
=================
R_PlanarSurfClipFragment

NOTE: one might want to combine this function with
R_WindingClipFragment for special cases like trifans (q1 and
q2 polys) or tristrips for ultra-fast clipping, providing there's
enough stack space (depending on MAX_FRAGMENT_VERTS value).
=================
*/
static bool R_PlanarSurfClipFragment( msurface_t *surf, const vec3_t normal )
{
	int	i;
	mesh_t	*mesh;
	elem_t	*elem;
	vec4_t	*verts;
	vec3_t	poly[4];
	vec3_t	dir1, dir2, snorm;
	bool	planar;

	planar = surf->plane && !VectorCompare( surf->plane->normal, vec3_origin );
	if( planar )
	{
		VectorCopy( surf->plane->normal, snorm );
		if( DotProduct( normal, snorm ) < 0.5 )
			return false; // greater than 60 degrees
	}

	mesh = surf->mesh;
	elem = mesh->elems;
	verts = mesh->vertexArray;

	// clip each triangle individually
	for( i = 0; i < mesh->numElems; i += 3, elem += 3 )
	{
		VectorCopy( verts[elem[0]], poly[0] );
		VectorCopy( verts[elem[1]], poly[1] );
		VectorCopy( verts[elem[2]], poly[2] );

		if( !planar )
		{
			// calculate two mostly perpendicular edge directions
			VectorSubtract( poly[0], poly[1], dir1 );
			VectorSubtract( poly[2], poly[1], dir2 );

			// we have two edge directions, we can calculate a third vector from
			// them, which is the direction of the triangle normal
			CrossProduct( dir1, dir2, snorm );
			VectorNormalize( snorm );

			// we multiply 0.5 by length of snorm to avoid normalizing
			if( DotProduct( normal, snorm ) < 0.5 )
				continue; // greater than 60 degrees
		}

		if( R_WindingClipFragment( poly, 3, surf, snorm ) )
			return true;
	}

	return false;
}

/*
=================
R_RecursiveFragmentNode
=================
*/
static void R_RecursiveFragmentNode( mnode_t *node, const vec3_t origin, const vec3_t normal, float radius )
{
	float		dist;
	cplane_t		*plane;
	msurface_t	*surf;
	int		i, inside;

	if( !node->plane ) return;	// hit a leaf

	if( numFragmentVerts == maxFragmentVerts || numClippedFragments == maxClippedFragments )
		return; // already reached the limit somewhere else

	// find which side of the node we are on
	plane = node->plane;
	if( plane->type < 3 )
		dist = origin[plane->type] - plane->dist;
	else dist = DotProduct( origin, plane->normal ) - plane->dist;

	// go down the appropriate sides
	if( dist > radius )
	{
		R_RecursiveFragmentNode( node->children[0], origin, normal, radius );
		return;
	}

	if( dist < -radius )
	{
		R_RecursiveFragmentNode( node->children[1], origin, normal, radius );
		return;
	}

	// clip to each surface
	surf = node->firstface;

	for( i = 0; i < node->numfaces; i++, surf++ )
	{
		if( numFragmentVerts == maxFragmentVerts || numClippedFragments == maxClippedFragments )
			break;	// already reached the limit

		if( surf->fragmentframe == r_fragmentframecount )
			continue;	// already checked this surface in another node
		surf->fragmentframe = r_fragmentframecount;

		if( surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
			continue;	// don't bother clipping

		if( surf->shader->flags & SHADER_NOFRAGMENTS )
			continue;	// don't bother clipping

		if( !BoundsAndSphereIntersect( surf->mins, surf->maxs, origin, radius ))
			continue;	// no intersection

		if(!( surf->flags & SURF_PLANEBACK ))
		{
			if( DotProduct( normal, surf->plane->normal ) < 0.5f )
				continue;	// greater than 60 degrees
		}
		else
		{
			if( DotProduct( normal, surf->plane->normal ) > -0.5f )
				continue;	// greater than 60 degrees
		}

		// clip to the surface
		inside = R_PlanarSurfClipFragment( surf, normal );
		if( inside ) return;
	}

	// recurse down the children
	R_RecursiveFragmentNode( node->children[0], origin, normal, radius );
	R_RecursiveFragmentNode( node->children[1], origin, normal, radius );
}

/*
=================
R_GetClippedFragments
=================
*/
int R_GetClippedFragments( const vec3_t origin, float radius, vec3_t axis[3], int maxfverts, vec3_t *fverts, int maxfragments, fragment_t *fragments )
{
	int	i;
	float	d;

	if( maxfverts <= 0 || fverts == NULL || maxfragments <= 0 || fragments == NULL )
		return 0;	// invalid arguments

	r_fragmentframecount++;

	// initialize fragments
	numFragmentVerts = 0;
	maxFragmentVerts = maxfverts;
	fragmentVerts = fverts;

	numClippedFragments = 0;
	maxClippedFragments = maxfragments;
	clippedFragments = fragments;
	fragmentDiameterSquared = radius * radius * 4;

	// calculate clipping planes
	for( i = 0; i < 3; i++ )
	{
		d = DotProduct( origin, axis[i] );

		VectorCopy( axis[i], fragmentPlanes[i*2].normal );
		fragmentPlanes[i*2].dist = d - radius;
		fragmentPlanes[i*2].type = PlaneTypeForNormal( fragmentPlanes[i*2].normal );

		VectorNegate( axis[i], fragmentPlanes[i*2+1].normal );
		fragmentPlanes[i*2+1].dist = -d - radius;
		fragmentPlanes[i*2+1].type = PlaneTypeForNormal( fragmentPlanes[i*2+1].normal );
	}

	// clip against world geometry
	R_RecursiveFragmentNode( r_worldbrushmodel->nodes, origin, axis[0], radius );

	return numClippedFragments;
}

//==================================================================================

static vec3_t trace_start, trace_end;
static vec3_t trace_absmins, trace_absmaxs;
static float trace_fraction;

static vec3_t trace_impact;
static cplane_t trace_plane;
static msurface_t *trace_surface;

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
static bool R_TraceAgainstSurface( msurface_t *surf )
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
	cplane_t *plane;

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
msurface_t *R_TransformedTraceLine( trace_t *tr, const vec3_t start, const vec3_t end, ref_entity_t *test )
{
	ref_model_t	*model;

	r_fragmentframecount++;	// for multi-check avoidance

	// fill in a default trace
	Mem_Set( tr, 0, sizeof( trace_t ));

	trace_surface = NULL;
	trace_fraction = 1.0f;
	VectorCopy( end, trace_impact );
	Mem_Set( &trace_plane, 0, sizeof( trace_plane ));

	ClearBounds( trace_absmins, trace_absmaxs );
	AddPointToBounds( start, trace_absmins, trace_absmaxs );
	AddPointToBounds( end, trace_absmins, trace_absmaxs );

	model = test->model;
	if( model )
	{
		if( model->type == mod_world || model->type == mod_brush )
		{
			mbrushmodel_t *bmodel = ( mbrushmodel_t * )model->extradata;
			vec3_t temp, start_l, end_l, axis[3];
			bool rotated = !Matrix3x3_Compare( test->axis, matrix3x3_identity );

			// transform
			VectorSubtract( start, test->origin, start_l );
			VectorSubtract( end, test->origin, end_l );
			if( rotated )
			{
				VectorCopy( start_l, temp );
				Matrix3x3_Transform( test->axis, temp, start_l );
				VectorCopy( end_l, temp );
				Matrix3x3_Transform( test->axis, temp, end_l );
			}

			VectorCopy( start_l, trace_start );
			VectorCopy( end_l, trace_end );

			// world uses a recursive approach using BSP tree, submodels
			// just walk the list of surfaces linearly
			if( test->model->type == mod_world )
				R_RecursiveHullCheck( bmodel->nodes, start_l, end_l );
			else if( BoundsIntersect( model->mins, model->maxs, trace_absmins, trace_absmaxs ) )
				R_TraceAgainstBmodel( bmodel );

			// transform back
			if( rotated && trace_fraction != 1 )
			{
				Matrix3x3_Transpose( axis, test->axis );
				VectorCopy( tr->vecPlaneNormal, temp );
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

		tr->flPlaneDist = trace_plane.dist;
		VectorCopy( trace_plane.normal, tr->vecPlaneNormal );
		tr->pHit = (edict_t *)test;
	}
	
	tr->flFraction = trace_fraction;
	VectorCopy( trace_impact, tr->vecEndPos );

	return trace_surface;
}
