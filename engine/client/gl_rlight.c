//=======================================================================
//			Copyright XashXT Group 2010 ©
//		      gl_rlight.c - dynamic and static lights
//=======================================================================

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
	lightstyle_t	*ls;

	if( !RI.drawWorld || !cl.worldmodel )
		return;

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
			RI.lightstylecolor[i] = 1.0f;
			continue;
		}

		if( !ls->length )
		{
			RI.lightstylevalue[i] = 256 * r_lighting_modulate->value;
			RI.lightstylecolor[i] = 1.0f * r_lighting_ambient->value;
			continue;
		}
		else if( ls->length == 1 )
		{
			// single length style so don't bother interpolating
			RI.lightstylevalue[i] = ls->map[0] * 22 * r_lighting_modulate->value;
			RI.lightstylecolor[i] = (ls->map[0] / 12.0f ) * r_lighting_ambient->value;
			continue;
		}
		else if( !ls->interp || !cl_lightstyle_lerping->integer )
		{
			RI.lightstylevalue[i] = ls->map[flight%ls->length] * 22 * r_lighting_modulate->value;
			RI.lightstylecolor[i] = (ls->map[flight%ls->length] / 12.0f) * r_lighting_ambient->value;
			continue;
		}

		// interpolate animating light
		// frame just gone
		k = ls->map[flight % ls->length];
		l = (float)( k * 22 ) * backlerp;
		c = (float)( k / 12 ) * backlerp;

		// upcoming frame
		k = ls->map[clight % ls->length];
		l += (float)( k * 22 ) * lerpfrac;
		c += (float)( k / 12 ) * lerpfrac;

		RI.lightstylevalue[i] = (int)l * r_lighting_modulate->value;
		RI.lightstylecolor[i] = c * r_lighting_ambient->value;
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
		R_MarkLights( l, 1<<i, cl.worldmodel->nodes );
	}
}

//===================================================================
/*
=================
R_ReadLightGrid
=================
*/
static void R_ReadLightGrid( const vec3_t origin, vec3_t lightDir )
{
	vec3_t	vf1, vf2, tdir;
	int	vi[3], elem[4];
	float	scale[8];
	int	i, k, s;

	if( !world.lightgrid )
	{
		if( RI.refdef.movevars )
		{
			// pre-defined light vector
			lightDir[0] = RI.refdef.movevars->skyvec_x;
			lightDir[1] = RI.refdef.movevars->skyvec_y;
			lightDir[2] = RI.refdef.movevars->skyvec_z;
		}
		return;
	}

	for( i = 0; i < 3; i++ )
	{
		vf1[i] = (origin[i] - world.gridMins[i]) / world.gridSize[i];
		vi[i] = (int)vf1[i];
		vf1[i] = vf1[i] - floor( vf1[i] );
		vf2[i] = 1.0f - vf1[i];
	}

	elem[0] = vi[2] * world.gridBounds[3] + vi[1] * world.gridBounds[0] + vi[0];
	elem[1] = elem[0] + world.gridBounds[0];
	elem[2] = elem[0] + world.gridBounds[3];
	elem[3] = elem[2] + world.gridBounds[0];

	for( i = 0; i < 4; i++ )
	{
		if( elem[i] < 0 || elem[i] >= world.numgridpoints - 1 )
		{
			// pre-defined light vector
			lightDir[0] = RI.refdef.movevars->skyvec_x;
			lightDir[1] = RI.refdef.movevars->skyvec_y;
			lightDir[2] = RI.refdef.movevars->skyvec_z;
			return;
		}
	}

	scale[0] = vf2[0] * vf2[1] * vf2[2];
	scale[1] = vf1[0] * vf2[1] * vf2[2];
	scale[2] = vf2[0] * vf1[1] * vf2[2];
	scale[3] = vf1[0] * vf1[1] * vf2[2];
	scale[4] = vf2[0] * vf2[1] * vf1[2];
	scale[5] = vf1[0] * vf2[1] * vf1[2];
	scale[6] = vf2[0] * vf1[1] * vf1[2];
	scale[7] = vf1[0] * vf1[1] * vf1[2];

	VectorClear( lightDir );

	for( i = 0; i < 4; i++ )
	{
		R_LatLongToNorm( world.lightgrid[elem[i]+0].direction, tdir );
		VectorScale( tdir, scale[i*2+0], tdir );

		for( k = 0; k < LM_STYLES && ( s = world.lightgrid[elem[i]+0].styles[k] ) != 255; k++ )
		{
			lightDir[0] += RI.lightstylecolor[s] * tdir[0];
			lightDir[1] += RI.lightstylecolor[s] * tdir[1];
			lightDir[2] += RI.lightstylecolor[s] * tdir[2];
		}

		R_LatLongToNorm( world.lightgrid[elem[i]+1].direction, tdir );
		VectorScale( tdir, scale[i*2+1], tdir );

		for( k = 0; k < LM_STYLES && ( s = world.lightgrid[elem[i]+1].styles[k] ) != 255; k++ )
		{
			lightDir[0] += RI.lightstylecolor[s] * tdir[0];
			lightDir[1] += RI.lightstylecolor[s] * tdir[1];
			lightDir[2] += RI.lightstylecolor[s] * tdir[2];
		}
	}
}

/*
=======================================================================

	AMBIENT & DIFFUSE LIGHTING

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
	if( node->contents < 0 )
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
	vec3_t		end, dir;
	float		dist, add;
	model_t		*pmodel;
	mnode_t		*pnodes;
	int		lnum;

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
	VectorCopy( point, end );
	if( invLight ) end[2] = point[2] + 8192;
	else end[2] = point[2] - 8192;

	// always have valid model
	pmodel = cl.worldmodel;
	pnodes = pmodel->nodes;
	m_pGround = NULL;

	if( r_lighting_extended->integer )
	{
		trace = PM_PlayerTrace( clgame.pmove, (float *)point, end, PM_STUDIO_IGNORE, 0, -1, NULL );
		m_pGround = CL_GetEntityByIndex( pfnIndexFromTrace( &trace ));
	}

	if( m_pGround && m_pGround->model && VectorIsNull( m_pGround->origin ) && VectorIsNull( m_pGround->angles ))
	{
		pmodel = m_pGround->model;
		pnodes = &pmodel->nodes[pmodel->hulls[0].firstclipnode];
	}

	r_pointColor[0] = RI.refdef.movevars->skycolor_r;
	r_pointColor[1] = RI.refdef.movevars->skycolor_g;
	r_pointColor[2] = RI.refdef.movevars->skycolor_b;

	if( R_RecursiveLightPoint( pmodel, pnodes, point, end ))
	{
		ambientLight->r = min((r_pointColor[0] >> 7), 255 );
		ambientLight->g = min((r_pointColor[1] >> 7), 255 );
		ambientLight->b = min((r_pointColor[2] >> 7), 255 );
	}
	else
	{
		ambientLight->r = RI.refdef.movevars->skycolor_r;
		ambientLight->g = RI.refdef.movevars->skycolor_g;
		ambientLight->b = RI.refdef.movevars->skycolor_b;
	}

	// add dynamic lights
	if( radius && r_dynamic->integer )
	{
		for( lnum = 0, dl = cl_dlights; lnum < MAX_DLIGHTS; lnum++, dl++ )
		{
			if( dl->die < cl.time || !dl->radius )
				continue;

			VectorSubtract( dl->origin, point, dir );
			dist = VectorLength( dir );

			if( !dist || dist > dl->radius + radius )
				continue;

			add = (dl->radius - dist);
			ambientLight->r = ambientLight->r + (dl->color.r * add);
			ambientLight->g = ambientLight->g + (dl->color.g * add);
			ambientLight->b = ambientLight->b + (dl->color.b * add);
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

	// get light direction from light grid
	R_ReadLightGrid( origin, lightDir );

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
	}

	// normalize final direction
//	VectorNormalize( lightDir );
}

/*
===============
R_LightForOrigin

extended version of LightForPoint
===============
*/
void R_LightForOrigin( const vec3_t origin, vec3_t dir, color24 *ambient, color24 *diffuse, float radius )
{
	int		i, j, k, s;
	float		dot, t[8];
	vec3_t		vf, vf2, tdir;
	int		vi[3], elem[4];
	vec3_t		ambientLocal, diffuseLocal;
	float		*gridSize, *gridMins;
	int		*gridBounds;
	mgridlight_t	*lightgrid;

	if( !cl.worldmodel || !world.lightgrid || !world.numgridpoints )
	{
		// get fullbright
		VectorSet( ambientLocal, 255, 255, 255 );
		VectorSet( diffuseLocal, 255, 255, 255 );
		dir[0] = RI.refdef.movevars->skyvec_x;
		dir[1] = RI.refdef.movevars->skyvec_y;
		dir[2] = RI.refdef.movevars->skyvec_z;
		goto dynamic;
	}
	else
	{
		VectorClear( ambientLocal );
		VectorClear( diffuseLocal );
	}

	lightgrid = world.lightgrid;
	gridSize = world.gridSize;
	gridMins = world.gridMins;
	gridBounds = world.gridBounds;

	for( i = 0; i < 3; i++ )
	{
		vf[i] = ( origin[i] - gridMins[i] ) / gridSize[i];
		vi[i] = (int)vf[i];
		vf[i] = vf[i] - floor( vf[i] );
		vf2[i] = 1.0f - vf[i];
	}

	elem[0] = vi[2] * gridBounds[3] + vi[1] * gridBounds[0] + vi[0];
	elem[1] = elem[0] + gridBounds[0];
	elem[2] = elem[0] + gridBounds[3];
	elem[3] = elem[2] + gridBounds[0];

	for( i = 0; i < 4; i++ )
	{
		if( elem[i] < 0 || elem[i] >= ( world.numgridpoints - 1 ))
		{
			dir[0] = RI.refdef.movevars->skyvec_x;
			dir[1] = RI.refdef.movevars->skyvec_y;
			dir[2] = RI.refdef.movevars->skyvec_z;
			goto dynamic;
		}
	}

	t[0] = vf2[0] * vf2[1] * vf2[2];
	t[1] = vf[0] * vf2[1] * vf2[2];
	t[2] = vf2[0] * vf[1] * vf2[2];
	t[3] = vf[0] * vf[1] * vf2[2];
	t[4] = vf2[0] * vf2[1] * vf[2];
	t[5] = vf[0] * vf2[1] * vf[2];
	t[6] = vf2[0] * vf[1] * vf[2];
	t[7] = vf[0] * vf[1] * vf[2];

	VectorClear( dir );

	for( i = 0; i < 4; i++ )
	{
		R_LatLongToNorm( lightgrid[elem[i]].direction, tdir );
		VectorScale( tdir, t[i*2+0], tdir );
		for( k = 0; k < LM_STYLES && ( s = lightgrid[elem[i]].styles[k] ) != 255; k++ )
		{
			dir[0] += RI.lightstylecolor[s] * tdir[0];
			dir[1] += RI.lightstylecolor[s] * tdir[1];
			dir[2] += RI.lightstylecolor[s] * tdir[2];
		}

		R_LatLongToNorm( lightgrid[elem[i] + 1].direction, tdir );
		VectorScale( tdir, t[i*2+1], tdir );
		for( k = 0; k < LM_STYLES && ( s = lightgrid[elem[i] + 1].styles[k] ) != 255; k++ )
		{
			dir[0] += RI.lightstylecolor[s] * tdir[0];
			dir[1] += RI.lightstylecolor[s] * tdir[1];
			dir[2] += RI.lightstylecolor[s] * tdir[2];
		}
	}

	for( j = 0; j < 3; j++ )
	{
		if( ambient )
		{
			for( i = 0; i < 4; i++ )
			{
				for( k = 0; k < LM_STYLES; k++ )
				{
					if(( s = lightgrid[elem[i]].styles[k] ) != 255 )
						ambientLocal[j] += t[i*2+0] * lightgrid[elem[i]+0].ambient[k][j] * RI.lightstylecolor[s];
					if(( s = lightgrid[elem[i] + 1].styles[k] ) != 255 )
						ambientLocal[j] += t[i*2+1] * lightgrid[elem[i]+1].ambient[k][j] * RI.lightstylecolor[s];
				}
			}
		}

		if( diffuse || radius )
		{
			for( i = 0; i < 4; i++ )
			{
				for( k = 0; k < LM_STYLES; k++ )
				{
					if( ( s = lightgrid[elem[i]].styles[k] ) != 255 )
						diffuseLocal[j] += t[i*2+0] * lightgrid[elem[i]+0].diffuse[k][j] * RI.lightstylecolor[s];
					if( ( s = lightgrid[elem[i] + 1].styles[k] ) != 255 )
						diffuseLocal[j] += t[i*2+1] * lightgrid[elem[i]+1].diffuse[k][j] * RI.lightstylecolor[s];
				}
			}
		}
	}

dynamic:
	// add dynamic lights
	if( radius && r_dynamic->integer )
	{
		uint	lnum;
		dlight_t	*dl;
		float	dist, dist2, add;
		vec3_t	direction;
		qboolean	anyDlights = false;

		for( lnum = 0, dl = cl_dlights; lnum < MAX_DLIGHTS; lnum++, dl++ )
		{
			if( dl->die < cl.time || !dl->radius )
				continue;

			VectorSubtract( dl->origin, origin, direction );
			dist = VectorLength( direction );

			if( !dist || dist > dl->radius + radius )
				continue;

			if( !anyDlights )
			{
				VectorNormalizeFast( dir );
				anyDlights = true;
			}

			add = 1.0f - (dist / ( dl->radius + radius ));
			dist2 = add * 0.5f / dist;

			dot = dl->color.r * add;
			diffuseLocal[0] += dot;
			ambientLocal[0] += dot * 0.05f;
			dir[0] += direction[0] * dist2;
			dot = dl->color.g * add;
			diffuseLocal[1] += dot;
			ambientLocal[1] += dot * 0.05f;
			dir[1] += direction[1] * dist2;
			dot = dl->color.b * add;
			diffuseLocal[2] += dot;
			ambientLocal[2] += dot * 0.05f;
			dir[2] += direction[2] * dist2;
		}
	}

	VectorNormalizeFast( dir );

	if( ambient )
	{
		dot = bound( 0.0f, r_lighting_ambient->value, 1.0f ) * 255.0f;
		ambient->r = (byte)bound( 0, ambientLocal[0] * dot, 255 );
		ambient->g = (byte)bound( 0, ambientLocal[1] * dot, 255 );
		ambient->b = (byte)bound( 0, ambientLocal[2] * dot, 255 );
	}

	if( diffuse )
	{
		dot = bound( 0.0f, r_lighting_direct->value, 1.0f ) * 255.0f;
		diffuse->r = (byte)bound( 0, diffuseLocal[0] * dot, 255 );
		diffuse->g = (byte)bound( 0, diffuseLocal[1] * dot, 255 );
		diffuse->b = (byte)bound( 0, diffuseLocal[2] * dot, 255 );
	}
}
/*
=================
R_LightForEntity
=================
*/
void R_LightForEntity( cl_entity_t *e, byte *bArray )
{
	dlight_t	*dl;
	vec3_t	end, dir;
	float	scale = 1.0f;
	float 	add, dot, dist, intensity, radius;
	vec3_t	ambientLight, directedLight, lightDir;
	matrix4x4	matrix, imatrix;
	vec3_t	pointColor;
	float	*cArray;
	int	i, l;

	if(( e->curstate.effects & EF_FULLBRIGHT ) || r_fullbright->integer )
		return;

	// never gets diffuse lighting for world brushes or sprites
	if( !e->model || ( e->model->type == mod_brush ) || ( e->model->type == mod_sprite ))
		return;

	// get lighting at this point
	if( e->curstate.effects & EF_INVLIGHT )
		VectorSet( end, e->origin[0], e->origin[1], e->origin[2] + 8192 );
	else VectorSet( end, e->origin[0], e->origin[1], e->origin[2] - 8192 );
	VectorSet( r_pointColor, 1.0f, 1.0f, 1.0f );

	R_RecursiveLightPoint( cl.worldmodel, cl.worldmodel->nodes, e->origin, end );

	pointColor[0] = (float)min((r_pointColor[0] >> 7), 255 ) * (1.0f / 255.0f);
	pointColor[1] = (float)min((r_pointColor[1] >> 7), 255 ) * (1.0f / 255.0f);
	pointColor[2] = (float)min((r_pointColor[2] >> 7), 255 ) * (1.0f / 255.0f);

	VectorScale( pointColor, r_lighting_ambient->value, ambientLight );
	VectorScale( pointColor, r_lighting_direct->value, directedLight );

	R_ReadLightGrid( e->origin, lightDir );

	// always have some light
	if( e->curstate.effects & EF_MINLIGHT )
	{
		for( i = 0; i < 3; i++ )
		{
			if( ambientLight[i] > 0.01f )
				break;
		}

		if( i == 3 )
		{
			VectorSet( ambientLight, 0.01f, 0.01f, 0.01f );
		}
	}

	if( e->model->type != mod_brush && e->curstate.scale > 0.0f )
		scale = e->curstate.scale;
	
	// compute lighting at each vertex
	Matrix4x4_CreateFromEntity( matrix, e->angles, vec3_origin, scale );
	Matrix4x4_Invert_Simple( imatrix, matrix );
			
	// rotate direction
	Matrix4x4_VectorRotate( imatrix, lightDir, dir );
	VectorNormalizeFast( dir );

	for( i = 0; i < tr.numColors; i++ )
	{
		dot = DotProduct( tr.normalArray[i], dir );
		if( dot <= 0.0f ) VectorCopy( ambientLight, r_lightColors[i] );
		else VectorMA( ambientLight, dot, directedLight, r_lightColors[i] );
	}

	// add dynamic lights
	if( r_dynamic->integer )
	{
		radius = e->model->radius;

		for( l = 0, dl = cl_dlights; l < MAX_DLIGHTS; l++, dl++ )
		{
			if( dl->die < cl.time || !dl->radius )
				continue;

			VectorSubtract( dl->origin, e->origin, dir );
			dist = VectorLength( dir );

			if( !dist || dist > dl->radius + radius )
				continue;

			Matrix4x4_VectorRotate( imatrix, dir, lightDir );
			intensity = dl->radius * 8;

			// compute lighting at each vertex
			for( i = 0; i < tr.numColors; i++ )
			{
				VectorSubtract( lightDir, tr.vertexArray[i], dir );
				add = DotProduct( tr.normalArray[i], dir );
				if( add <= 0.0f ) continue;

				dot = DotProduct( dir, dir );
				add *= ( intensity / dot ) * rsqrt( dot );
				r_lightColors[i][0] = r_lightColors[i][0] + (dl->color.r * add);
				r_lightColors[i][1] = r_lightColors[i][1] + (dl->color.g * add);
				r_lightColors[i][2] = r_lightColors[i][2] + (dl->color.b * add);
			}
		}
	}

	cArray = r_lightColors[0];

	for( i = 0; i < tr.numColors; i++, bArray += 4, cArray += 3 )
	{
		bArray[0] = R_FloatToByte( cArray[0] );
		bArray[1] = R_FloatToByte( cArray[1] );
		bArray[2] = R_FloatToByte( cArray[2] );
	}
}