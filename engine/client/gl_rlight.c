/*
gl_rlight.c - dynamic and static lights
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
#include "client.h"
#include "mathlib.h"
#include "gl_local.h"
#include "pm_local.h"
#include "studio.h"

/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/
/*
==================
R_AnimateLight

==================
*/
void R_AnimateLight( void )
{
	int		i, k, flight, clight;
	float		l, c, lerpfrac, backlerp;
	float		scale;
	lightstyle_t	*ls;

	if( !RI.drawWorld || !cl.worldmodel )
		return;

	scale = r_lighting_modulate->value;

	// light animations
	// 'm' is normal light, 'a' is no light, 'z' is double bright
	flight = (int)floor( cl.time * 10 );
	clight = (int)ceil( cl.time * 10 );
	lerpfrac = ( cl.time * 10 ) - flight;
	backlerp = 1.0f - lerpfrac;

	for( i = 0, ls = cl.lightstyles; i < MAX_LIGHTSTYLES; i++, ls++ )
	{
		if( r_fullbright->integer || !cl.worldmodel->lightdata )
		{
			RI.lightstylevalue[i] = 256 * 256;
			RI.lightcache[i] = 3.0f;
			continue;
		}

		if( !ls->length )
		{
			RI.lightstylevalue[i] = 256 * scale;
			RI.lightcache[i] = 3.0f * scale;
			continue;
		}
		else if( ls->length == 1 )
		{
			// single length style so don't bother interpolating
			RI.lightstylevalue[i] = ls->map[0] * 22 * scale;
			RI.lightcache[i] = ( ls->map[0] / 12.0f ) * 3.0f * scale;
			continue;
		}
		else if( !ls->interp || !cl_lightstyle_lerping->integer )
		{
			RI.lightstylevalue[i] = ls->map[flight%ls->length] * 22 * scale;
			RI.lightcache[i] = ( ls->map[flight%ls->length] / 12.0f ) * 3.0f * scale;
			continue;
		}

		// interpolate animating light
		// frame just gone
		k = ls->map[flight % ls->length];
		l = (float)( k * 22.0f ) * backlerp;
		c = (float)( k / 12.0f ) * backlerp;

		// upcoming frame
		k = ls->map[clight % ls->length];
		l += (float)( k * 22.0f ) * lerpfrac;
		c += (float)( k / 12.0f ) * lerpfrac;

		RI.lightstylevalue[i] = (int)l * scale;
		RI.lightcache[i] = c * 3.0f * scale;
	}
}

/*
=============
R_MarkLights
=============
*/
void R_MarkLights( dlight_t *light, int bit, mnode_t *node )
{
	float		dist;
	msurface_t	*surf;
	int		i;
	
	if( node->contents < 0 )
		return;

	dist = PlaneDiff( light->origin, node->plane );

	if( dist > light->radius )
	{
		R_MarkLights( light, bit, node->children[0] );
		return;
	}
	if( dist < -light->radius )
	{
		R_MarkLights( light, bit, node->children[1] );
		return;
	}
		
	// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;

	for( i = 0; i < node->numsurfaces; i++, surf++ )
	{
		mextrasurf_t	*info = SURF_INFO( surf, cl.worldmodel );

		if( !BoundsAndSphereIntersect( info->mins, info->maxs, light->origin, light->radius ))
			continue;	// no intersection

		if( surf->dlightframe != tr.dlightframecount )
		{
			surf->dlightbits = 0;
			surf->dlightframe = tr.dlightframecount;
		}
		surf->dlightbits |= bit;
	}

	R_MarkLights( light, bit, node->children[0] );
	R_MarkLights( light, bit, node->children[1] );
}

/*
=============
R_PushDlights
=============
*/
void R_PushDlights( void )
{
	dlight_t	*l;
	int	i;

	tr.dlightframecount = tr.framecount + 1;	// because the count hasn't
						// advanced yet for this frame
	l = cl_dlights;

	for( i = 0; i < MAX_DLIGHTS; i++, l++ )
	{
		if( l->die < cl.time || !l->radius )
			continue;

		if( R_CullSphere( l->origin, l->radius, 15 ))
			continue;

		R_MarkLights( l, 1<<i, cl.worldmodel->nodes );
	}
}

/*
=======================================================================

	AMBIENT LIGHTING

=======================================================================
*/
static uint	r_pointColor[3];
static vec3_t	r_lightColors[MAXSTUDIOVERTS];

/*
=================
R_RecursiveLightPoint
=================
*/
static qboolean R_RecursiveLightPoint( model_t *model, mnode_t *node, const vec3_t start, const vec3_t end )
{
	float		front, back, frac;
	int		i, map, side, size, s, t;
	msurface_t	*surf;
	mtexinfo_t	*tex;
	color24		*lm;
	vec3_t		mid;

	// didn't hit anything
	if( !node || node->contents < 0 )
		return false;

	// calculate mid point
	front = PlaneDiff( start, node->plane );
	back = PlaneDiff( end, node->plane );

	side = front < 0;
	if(( back < 0 ) == side )
		return R_RecursiveLightPoint( model, node->children[side], start, end );

	frac = front / ( front - back );

	VectorLerp( start, frac, end, mid );

	// co down front side	
	if( R_RecursiveLightPoint( model, node->children[side], start, mid ))
		return true; // hit something

	if(( back < 0 ) == side )
		return false;// didn't hit anything

	// check for impact on this node
	surf = model->surfaces + node->firstsurface;

	for( i = 0; i < node->numsurfaces; i++, surf++ )
	{
		tex = surf->texinfo;

		if( surf->flags & ( SURF_DRAWSKY|SURF_DRAWTURB ))
			continue;	// no lightmaps

		s = DotProduct( mid, tex->vecs[0] ) + tex->vecs[0][3] - surf->texturemins[0];
		t = DotProduct( mid, tex->vecs[1] ) + tex->vecs[1][3] - surf->texturemins[1];

		if(( s < 0 || s > surf->extents[0] ) || ( t < 0 || t > surf->extents[1] ))
			continue;

		s >>= 4;
		t >>= 4;

		if( !surf->samples )
			return true;

		VectorClear( r_pointColor );

		lm = surf->samples + (t * ((surf->extents[0] >> 4) + 1) + s);
		size = ((surf->extents[0] >> 4) + 1) * ((surf->extents[1] >> 4) + 1);

		for( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++ )
		{
			uint	scale = RI.lightstylevalue[surf->styles[map]];

			r_pointColor[0] += lm->r * scale;
			r_pointColor[1] += lm->g * scale;
			r_pointColor[2] += lm->b * scale;

			lm += size; // skip to next lightmap
		}
		return true;
	}

	// go down back side
	return R_RecursiveLightPoint( model, node->children[!side], mid, end );
}

/*
=================
R_LightForPoint
=================
*/
void R_LightForPoint( const vec3_t point, color24 *ambientLight, qboolean invLight, float radius )
{
	dlight_t		*dl;
	pmtrace_t		trace;
	cl_entity_t	*m_pGround;
	vec3_t		start, end, dir;
	float		dist, add;
	model_t		*pmodel;
	mnode_t		*pnodes;

	if( !RI.refdef.movevars )
	{
		ambientLight->r = 255;
		ambientLight->g = 255;
		ambientLight->b = 255;
		return;
	}

	// set to full bright if no light data
	if( !cl.worldmodel || !cl.worldmodel->lightdata )
	{
		ambientLight->r = RI.refdef.movevars->skycolor_r;
		ambientLight->g = RI.refdef.movevars->skycolor_g;
		ambientLight->b = RI.refdef.movevars->skycolor_b;
		return;
	}

	// Get lighting at this point
	VectorCopy( point, start );
	VectorCopy( point, end );
	if( invLight )
	{
		start[2] = point[2] - 64;
		end[2] = point[2] + 8192;
	}
	else
	{
		start[2] = point[2] + 64;
		end[2] = point[2] - 8192;
	}

	// always have valid model
	pmodel = cl.worldmodel;
	pnodes = pmodel->nodes;
	m_pGround = NULL;

	if( r_lighting_extended->integer )
	{
		trace = PM_PlayerTrace( clgame.pmove, start, end, PM_STUDIO_IGNORE, 0, -1, NULL );
		m_pGround = CL_GetEntityByIndex( pfnIndexFromTrace( &trace ));
	}

	if( m_pGround && m_pGround->model )
	{
		matrix4x4	matrix;
		hull_t	*hull;
		vec3_t	start_l, end_l;
		vec3_t	offset;

		pmodel = m_pGround->model;
		pnodes = &pmodel->nodes[pmodel->hulls[0].firstclipnode];

		hull = &pmodel->hulls[0];
		VectorSubtract( hull->clip_mins, vec3_origin, offset );
		VectorAdd( offset, m_pGround->origin, offset );

		VectorSubtract( start, offset, start_l );
		VectorSubtract( end, offset, end_l );

		// rotate start and end into the models frame of reference
		if( !VectorIsNull( m_pGround->angles ))
		{
			matrix4x4	imatrix;

			Matrix4x4_CreateFromEntity( matrix, m_pGround->angles, offset, 1.0f );
			Matrix4x4_Invert_Simple( imatrix, matrix );

			Matrix4x4_VectorTransform( imatrix, start, start_l );
			Matrix4x4_VectorTransform( imatrix, end, end_l );
		}

		// copy transformed pos back
		VectorCopy( start_l, start );
		VectorCopy( end_l, end );
	}

	VectorClear( r_pointColor );

	if( R_RecursiveLightPoint( pmodel, pnodes, start, end ))
	{
		ambientLight->r = min((r_pointColor[0] >> 7), 255 );
		ambientLight->g = min((r_pointColor[1] >> 7), 255 );
		ambientLight->b = min((r_pointColor[2] >> 7), 255 );
	}
	else
	{
		float	ambient;

		// R_RecursiveLightPoint didn't hit anything, so use default value
		ambient = bound( 0.1f, r_lighting_ambient->value, 1.0f );
		ambientLight->r = 255 * ambient;
		ambientLight->g = 255 * ambient;
		ambientLight->b = 255 * ambient;
	}

	// add dynamic lights
	if( radius && r_dynamic->integer )
	{
		int	lnum, total; 
		float	f;

		VectorClear( r_pointColor );

		for( total = lnum = 0, dl = cl_dlights; lnum < MAX_DLIGHTS; lnum++, dl++ )
		{
			if( dl->die < cl.time || !dl->radius )
				continue;

			VectorSubtract( dl->origin, point, dir );
			dist = VectorLength( dir );

			if( !dist || dist > dl->radius + radius )
				continue;

			add = 1.0f - (dist / ( dl->radius + radius ));
			r_pointColor[0] += dl->color.r * add;
			r_pointColor[1] += dl->color.g * add;
			r_pointColor[2] += dl->color.b * add;
			total++;
		}

		if( total != 0 )
		{
			r_pointColor[0] += ambientLight->r;
			r_pointColor[1] += ambientLight->g;
			r_pointColor[2] += ambientLight->b;

			f = max( max( r_pointColor[0], r_pointColor[1] ), r_pointColor[2] );
			if( f > 1.0f ) VectorScale( r_pointColor, ( 255.0f / f ), r_pointColor );

			ambientLight->r = r_pointColor[0];
			ambientLight->g = r_pointColor[1];
			ambientLight->b = r_pointColor[2];
		}
	}
}

/*
=================
R_LightDir
=================
*/
void R_LightDir( const vec3_t origin, vec3_t lightDir, float radius )
{
	dlight_t	*dl;
	vec3_t	dir;
	float	dist;
	int	lnum;

	if( RI.refdef.movevars )
	{
		// pre-defined light vector
		lightDir[0] = RI.refdef.movevars->skyvec_x;
		lightDir[1] = RI.refdef.movevars->skyvec_y;
		lightDir[2] = RI.refdef.movevars->skyvec_z;
	}
	else
	{
		VectorSet( lightDir, 0.0f, 0.0f, -1.0f );
	}

	// add dynamic lights
	if( radius > 0.0f && r_dynamic->integer )
	{
		for( lnum = 0, dl = cl_dlights; lnum < MAX_DLIGHTS; lnum++, dl++ )
		{
			if( dl->die < cl.time || !dl->radius )
				continue;

			VectorSubtract( dl->origin, origin, dir );
			dist = VectorLength( dir );

			if( !dist || dist > dl->radius + radius )
				continue;
			VectorAdd( lightDir, dir, lightDir );
		}

		for( lnum = 0, dl = cl_elights; lnum < MAX_ELIGHTS; lnum++, dl++ )
		{
			if( dl->die < cl.time || !dl->radius )
				continue;

			VectorSubtract( dl->origin, origin, dir );
			dist = VectorLength( dir );

			if( !dist || dist > dl->radius + radius )
				continue;
			VectorAdd( lightDir, dir, lightDir );
		}
	}

	// normalize final direction
	VectorNormalize( lightDir );
}