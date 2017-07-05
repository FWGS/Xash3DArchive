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
CL_RunLightStyles

==================
*/
void CL_RunLightStyles( void )
{
	int		i, k, flight, clight;
	float		l, lerpfrac, backlerp;
	float		frametime = (cl.time - cl.oldtime);
	float		scale;
	lightstyle_t	*ls;

	if( !cl.worldmodel ) return;

	scale = r_lighting_modulate->value;

	// light animations
	// 'm' is normal light, 'a' is no light, 'z' is double bright
	for( i = 0, ls = cl.lightstyles; i < MAX_LIGHTSTYLES; i++, ls++ )
	{
		if( r_fullbright->value || !cl.worldmodel->lightdata )
		{
			tr.lightstylevalue[i] = 256 * 256;
			continue;
		}

		if( !cl.paused && frametime <= 0.1f )
			ls->time += frametime; // evaluate local time

		flight = (int)Q_floor( ls->time * 10 );
		clight = (int)Q_ceil( ls->time * 10 );
		lerpfrac = ( ls->time * 10 ) - flight;
		backlerp = 1.0f - lerpfrac;

		if( !ls->length )
		{
			tr.lightstylevalue[i] = 256 * scale;
			continue;
		}
		else if( ls->length == 1 )
		{
			// single length style so don't bother interpolating
			tr.lightstylevalue[i] = ls->map[0] * 22 * scale;
			continue;
		}
		else if( !ls->interp || !cl_lightstyle_lerping->value )
		{
			tr.lightstylevalue[i] = ls->map[flight%ls->length] * 22 * scale;
			continue;
		}

		// interpolate animating light
		// frame just gone
		k = ls->map[flight % ls->length];
		l = (float)( k * 22.0f ) * backlerp;

		// upcoming frame
		k = ls->map[clight % ls->length];
		l += (float)( k * 22.0f ) * lerpfrac;

		tr.lightstylevalue[i] = (int)l * scale;
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
	surf = RI.currentmodel->surfaces + node->firstsurface;

	for( i = 0; i < node->numsurfaces; i++, surf++ )
	{
		if( !BoundsAndSphereIntersect( surf->info->mins, surf->info->maxs, light->origin, light->radius ))
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

	tr.dlightframecount = tr.framecount;
	l = cl_dlights;

	RI.currententity = clgame.entities;
	RI.currentmodel = RI.currententity->model;

	for( i = 0; i < MAX_DLIGHTS; i++, l++ )
	{
		if( l->die < cl.time || !l->radius )
			continue;

		if( GL_FrustumCullSphere( &RI.frustum, l->origin, l->radius, 15 ))
			continue;

		R_MarkLights( l, 1<<i, RI.currentmodel->nodes );
	}
}

/*
=============
R_CountDlights
=============
*/
int R_CountDlights( void )
{
	dlight_t	*l;
	int	i, numDlights = 0;

	for( i = 0, l = cl_dlights; i < MAX_DLIGHTS; i++, l++ )
	{
		if( l->die < cl.time || !l->radius )
			continue;

		numDlights++;
	}

	return numDlights;
}

/*
=============
R_CountSurfaceDlights
=============
*/
int R_CountSurfaceDlights( msurface_t *surf )
{
	int	i, numDlights = 0;

	for( i = 0; i < MAX_DLIGHTS; i++ )
	{
		if(!( surf->dlightbits & BIT( i )))
			continue;	// not lit by this light

		numDlights++;
	}

	return numDlights;
}

/*
=======================================================================

	AMBIENT LIGHTING

=======================================================================
*/
static float	g_trace_fraction;
static vec3_t	g_trace_lightspot;

/*
=================
R_RecursiveLightPoint
=================
*/
static qboolean R_RecursiveLightPoint( model_t *model, mnode_t *node, float p1f, float p2f, colorVec *cv, const vec3_t start, const vec3_t end )
{
	float		front, back, frac, midf;
	int		i, map, side, size, s, t;
	int		sample_size;
	msurface_t	*surf;
	mtexinfo_t	*tex;
	color24		*lm;
	vec3_t		mid;

	// didn't hit anything
	if( !node || node->contents < 0 )
	{
		cv->r = cv->g = cv->b = cv->a = 0;
		return false;
	}

	// calculate mid point
	front = PlaneDiff( start, node->plane );
	back = PlaneDiff( end, node->plane );

	side = front < 0;
	if(( back < 0 ) == side )
		return R_RecursiveLightPoint( model, node->children[side], p1f, p2f, cv, start, end );

	frac = front / ( front - back );

	VectorLerp( start, frac, end, mid );
	midf = p1f + ( p2f - p1f ) * frac;

	// co down front side	
	if( R_RecursiveLightPoint( model, node->children[side], p1f, midf, cv, start, mid ))
		return true; // hit something

	if(( back < 0 ) == side )
	{
		cv->r = cv->g = cv->b = cv->a = 0;
		return false; // didn't hit anything
	}

	// check for impact on this node
	surf = model->surfaces + node->firstsurface;
	VectorCopy( mid, g_trace_lightspot );

	for( i = 0; i < node->numsurfaces; i++, surf++ )
	{
		tex = surf->texinfo;

		if( FBitSet( surf->flags, SURF_DRAWTILED ))
			continue;	// no lightmaps

		s = DotProduct( mid, tex->vecs[0] ) + tex->vecs[0][3] - surf->texturemins[0];
		t = DotProduct( mid, tex->vecs[1] ) + tex->vecs[1][3] - surf->texturemins[1];

		if(( s < 0 || s > surf->extents[0] ) || ( t < 0 || t > surf->extents[1] ))
			continue;

		cv->r = cv->g = cv->b = cv->a = 0;

		if( !surf->samples )
			return true;

		sample_size = Mod_SampleSizeForFace( surf );
		s /= sample_size;
		t /= sample_size;

		lm = surf->samples + (t * ((surf->extents[0]  / sample_size) + 1) + s);
		size = ((surf->extents[0] / sample_size) + 1) * ((surf->extents[1] / sample_size) + 1);
		g_trace_fraction = midf;

		for( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++ )
		{
			uint	scale = tr.lightstylevalue[surf->styles[map]];

			if( tr.ignore_lightgamma )
			{
				cv->r += lm->r * scale;
				cv->g += lm->g * scale;
				cv->b += lm->b * scale;
			}
			else
			{
				cv->r += LightToTexGamma( lm->r ) * scale;
				cv->g += LightToTexGamma( lm->g ) * scale;
				cv->b += LightToTexGamma( lm->b ) * scale;
			}
			lm += size; // skip to next lightmap
		}

		return true;
	}

	// go down back side
	return R_RecursiveLightPoint( model, node->children[!side], midf, p2f, cv, mid, end );
}

int R_LightTraceFilter( physent_t *pe )
{
	if( !pe || pe->solid != SOLID_BSP || pe->info == 0 )
		return 1;

	return 0;
}

/*
=================
R_LightForPoint
=================
*/
void R_LightForPoint( const vec3_t point, color24 *ambientLight, qboolean invLight, qboolean useAmbient, float radius )
{
	dlight_t		*dl;
	pmtrace_t		trace;
	colorVec		light;
	cl_entity_t	*m_pGround;
	vec3_t		start, end, dir;
	qboolean		secondpass = false;
	float		dist, add;
	model_t		*pmodel;
	mnode_t		*pnodes;

	// set to full bright if no light data
	if( !cl.worldmodel || !cl.worldmodel->lightdata )
	{
		ambientLight->r = LightToTexGamma( clgame.movevars.skycolor_r );
		ambientLight->g = LightToTexGamma( clgame.movevars.skycolor_g );
		ambientLight->b = LightToTexGamma( clgame.movevars.skycolor_b );
		return;
	}

get_light:
	// Get lighting at this point
	VectorCopy( point, start );
	VectorCopy( point, end );
	if( invLight )
	{
		start[2] = point[2] - 64.0f;
		end[2] = point[2] + world.size[2];
	}
	else
	{
		start[2] = point[2] + 64.0f;
		end[2] = point[2] - world.size[2];
	}

	// always have valid model
	pmodel = cl.worldmodel;
	pnodes = pmodel->nodes;
	m_pGround = NULL;

	if( r_lighting_extended->value && !secondpass )
	{
		CL_SetTraceHull( 2 );
		CL_PlayerTraceExt( start, end, PM_STUDIO_IGNORE, R_LightTraceFilter, &trace );
		m_pGround = CL_GetEntityByIndex( pfnIndexFromTrace( &trace ));
		if( trace.startsolid || trace.allsolid ) m_pGround = NULL; // trace in solid
	}

	if( m_pGround && m_pGround->model && m_pGround->model->type == mod_brush )
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
			Matrix4x4_CreateFromEntity( matrix, m_pGround->angles, offset, 1.0f );
			Matrix4x4_VectorITransform( matrix, start, start_l );
			Matrix4x4_VectorITransform( matrix, end, end_l );
		}

		// copy transformed pos back
		VectorCopy( start_l, start );
		VectorCopy( end_l, end );
	}

	if( R_RecursiveLightPoint( pmodel, pnodes, 0.0f, 1.0f, &light, start, end ))
	{
		ambientLight->r = Q_min(( light.r >> 7 ), 255 );
		ambientLight->g = Q_min(( light.g >> 7 ), 255 );
		ambientLight->b = Q_min(( light.b >> 7 ), 255 );
	}
	else
	{
		float	ambient;

		// R_RecursiveLightPoint didn't hit anything, so use default value
		ambient = bound( 0.1f, r_lighting_ambient->value, 1.0f );
		if( !useAmbient ) ambient = 0.0f; // clear ambient
		ambientLight->r = 255 * ambient;
		ambientLight->g = 255 * ambient;
		ambientLight->b = 255 * ambient;
	}

	if( ambientLight->r == 0 && ambientLight->g == 0 && ambientLight->b == 0 && !secondpass )
	{
		// in some cases r_lighting_extended 1 does a wrong results
		// make another pass and try to get lighting info from world
		secondpass = true;
		goto get_light;
	}

	// add dynamic lights
	if( radius && r_dynamic->value )
	{
		int	lnum, total; 
		float	f;

		light.r = light.g = light.b = light.a = 0;

		for( total = lnum = 0, dl = cl_dlights; lnum < MAX_DLIGHTS; lnum++, dl++ )
		{
			if( dl->die < cl.time || !dl->radius )
				continue;

			VectorSubtract( dl->origin, point, dir );
			dist = VectorLength( dir );

			if( !dist || dist > dl->radius + radius )
				continue;

			add = 1.0f - (dist / ( dl->radius + radius ));
			light.r += LightToTexGamma( dl->color.r ) * add;
			light.g += LightToTexGamma( dl->color.g ) * add;
			light.b += LightToTexGamma( dl->color.b ) * add;
			total++;
		}

		if( total != 0 )
		{
			light.r += ambientLight->r;
			light.g += ambientLight->g;
			light.b += ambientLight->b;

			f = max( max( light.r, light.g ), light.b );
			if( f > 1.0f )
			{
				light.r *= (255.0f / f);
				light.g *= (255.0f / f);
				light.b *= (255.0f / f);
			}

			ambientLight->r = light.r;
			ambientLight->g = light.g;
			ambientLight->b = light.b;
		}
	}
}

/*
=================
R_LightVec

check bspmodels to get light from
=================
*/
colorVec R_LightVec( const vec3_t start, const vec3_t end, vec3_t lspot )
{
	float	last_fraction;
	int	i, maxEnts = 1;
	colorVec	light, cv;

	if( cl.worldmodel && cl.worldmodel->lightdata )
	{
		light.r = light.g = light.b = light.a = 0;
		last_fraction = 1.0f;

		// get light from bmodels too
		if( r_lighting_extended->value )
			maxEnts = clgame.pmove->numphysent;

		// check al the bsp-models
		for( i = 0; i < maxEnts; i++ )
		{
			physent_t	*pe = &clgame.pmove->physents[i];
			vec3_t	offset, start_l, end_l;
			mnode_t	*pnodes;
			matrix4x4	matrix;

			if( !pe->model || pe->model->type != mod_brush )
				continue; // skip non-bsp models

			pnodes = &pe->model->nodes[pe->model->hulls[0].firstclipnode];
			VectorSubtract( pe->model->hulls[0].clip_mins, vec3_origin, offset );
			VectorAdd( offset, pe->origin, offset );
			VectorSubtract( start, offset, start_l );
			VectorSubtract( end, offset, end_l );

			// rotate start and end into the models frame of reference
			if( !VectorIsNull( pe->angles ))
			{
				Matrix4x4_CreateFromEntity( matrix, pe->angles, offset, 1.0f );
				Matrix4x4_VectorITransform( matrix, start, start_l );
				Matrix4x4_VectorITransform( matrix, end, end_l );
			}

			VectorClear( g_trace_lightspot );
			g_trace_fraction = 1.0f;

			if( !R_RecursiveLightPoint( pe->model, pnodes, 0.0f, 1.0f, &cv, start_l, end_l ))
				continue;	// didn't hit anything

			if( g_trace_fraction < last_fraction )
			{
				if( lspot ) VectorCopy( g_trace_lightspot, lspot );
				light.r = Q_min(( cv.r >> 7 ), 255 );
				light.g = Q_min(( cv.g >> 7 ), 255 );
				light.b = Q_min(( cv.b >> 7 ), 255 );
				last_fraction = g_trace_fraction;
			}
		}
	}
	else
	{
		light.r = light.g = light.b = 255;
		light.a = 0;
	}

	return light;
}

/*
=================
R_LightPoint

light from floor
=================
*/
colorVec R_LightPoint( const vec3_t p0 )
{
	vec3_t	p1;

	VectorSet( p1, p0[0], p0[1], p0[2] - 2048.0f );

	return R_LightVec( p0, p1, NULL );
}