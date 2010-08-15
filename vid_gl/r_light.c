/*
Copyright (C) 1997-2001 Id Software, Inc.
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

// r_light.c

#include "r_local.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "bspfile.h"

/*
=============================================================================

 DYNAMIC LIGHTS

=============================================================================
*/
/*
=============
R_SurfPotentiallyLit
=============
*/
bool R_SurfPotentiallyLit( msurface_t *surf )
{
	ref_shader_t *shader;

	if( surf->flags & ( SURF_DRAWSKY|SURF_DRAWTURB ))
		return false;

	if( !surf->samples )
		return false;

	shader = surf->shader;
	if( shader->flags & SHADER_SKYPARMS || shader->type == SHADER_FLARE || !shader->num_stages )
		return false;

	return true;
}

/*
=============
R_LightBounds
=============
*/
void R_LightBounds( const vec3_t origin, float intensity, vec3_t mins, vec3_t maxs )
{
	VectorSet( mins, origin[0] - intensity, origin[1] - intensity, origin[2] - intensity );
	VectorSet( maxs, origin[0] + intensity, origin[1] + intensity, origin[2] + intensity );
}

/*
=============
R_AddSurfDlighbits
=============
*/
uint R_AddSurfDlighbits( msurface_t *surf, uint dlightbits )
{
	uint		k, bit;
	ref_dlight_t	*lt;
	float		dist;

	for( k = 0, bit = 1, lt = r_dlights; k < r_numDlights; k++, bit <<= 1, lt++ )
	{
		if( dlightbits & bit )
		{
			if( surf->plane->type < 3 ) dist = lt->origin[surf->plane->type] - surf->plane->dist;
			else dist = DotProduct( lt->origin, surf->plane->normal ) - surf->plane->dist;

			dist = PlaneDiff( lt->origin, surf->plane );
			if( dist <= -lt->intensity || dist >= lt->intensity )
				dlightbits &= ~bit; // how lucky...
		}
	}
	return dlightbits;
}

/*
=================
R_AddDynamicLights
=================
*/
void R_AddDynamicLights( uint dlightbits, int state )
{
	uint		i, j, numTempElems;
	bool		cullAway;
	const ref_dlight_t	*light;
	const ref_shader_t	*shader;
	vec3_t		tvec, dlorigin, normal;
	elem_t		tempElemsArray[MAX_ARRAY_ELEMENTS];
	float		inverseIntensity, *v1, *v2, *v3, dist;
	GLfloat		xyzFallof[4][4] = {{ 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 0 }};
	matrix4x4		m;

	r_backacc.numColors = 0;
	Matrix4x4_BuildLightIdentity( m );

	// we multitexture or texture3D support for dynamic lights
	if( !GL_Support( R_TEXTURE_3D_EXT ) && !GL_Support( R_ARB_MULTITEXTURE ))
		return;

	for( i = 0; i < ( GL_Support( R_TEXTURE_3D_EXT ) ? 1 : 2 ); i++ )
	{
		GL_SelectTexture( i );
		GL_TexEnv( GL_MODULATE );
		GL_SetState( state | ( i ? 0 : GLSTATE_BLEND_MTEX ));
		GL_SetTexCoordArrayMode( 0 );
		GL_EnableTexGen( GL_S, GL_OBJECT_LINEAR );
		GL_EnableTexGen( GL_T, GL_OBJECT_LINEAR );
		GL_EnableTexGen( GL_R, 0 );
		GL_EnableTexGen( GL_Q, 0 );
	}

	if( GL_Support( R_TEXTURE_3D_EXT ))
	{
		GL_EnableTexGen( GL_R, GL_OBJECT_LINEAR );
		pglDisable( GL_TEXTURE_2D );
		pglEnable( GL_TEXTURE_3D );
	}
	else
	{
		pglEnable( GL_TEXTURE_2D );
	}

	for( i = 0, light = r_dlights; i < r_numDlights; i++, light++ )
	{
		if( light->flags & DLIGHT_ONLYENTS )
			continue;

		if(!( dlightbits & ( 1<<i )))
			continue; // not lit by this light

		VectorSubtract( light->origin, RI.currententity->origin, dlorigin );
		if( !Matrix3x3_Compare( RI.currententity->axis, matrix3x3_identity ))
		{
			VectorCopy( dlorigin, tvec );
			Matrix3x3_Transform( RI.currententity->axis, tvec, dlorigin );
		}

		shader = light->shader;
		if( shader && ( shader->flags & SHADER_CULL_BACK ))
			cullAway = true;
		else cullAway = false;

		numTempElems = 0;
		if( cullAway )
		{
			for( j = 0; j < r_backacc.numElems; j += 3 )
			{
				v1 = (float *)(vertsArray + elemsArray[j+0]);
				v2 = (float *)(vertsArray + elemsArray[j+1]);
				v3 = (float *)(vertsArray + elemsArray[j+2]);

				normal[0] = ( v1[1] - v2[1] ) * ( v3[2] - v2[2] ) - ( v1[2] - v2[2] ) * ( v3[1] - v2[1] );
				normal[1] = ( v1[2] - v2[2] ) * ( v3[0] - v2[0] ) - ( v1[0] - v2[0] ) * ( v3[2] - v2[2] );
				normal[2] = ( v1[0] - v2[0] ) * ( v3[1] - v2[1] ) - ( v1[1] - v2[1] ) * ( v3[0] - v2[0] );
				dist = (dlorigin[0] - v1[0]) * normal[0] + (dlorigin[1] - v1[1]) * normal[1] + (dlorigin[2] - v1[2]) * normal[2];

				if( dist <= 0 || dist * rsqrt( DotProduct( normal, normal )) >= light->intensity )
					continue;

				tempElemsArray[numTempElems++] = elemsArray[j+0];
				tempElemsArray[numTempElems++] = elemsArray[j+1];
				tempElemsArray[numTempElems++] = elemsArray[j+2];
			}
			if( !numTempElems ) continue;
		}

		inverseIntensity = 1.0f / light->intensity;

		GL_Bind( 0, tr.dlightTexture );
		GL_LoadTexMatrix( m );
		pglColor4f( light->color[0], light->color[1], light->color[2], 255 );

		xyzFallof[0][0] = inverseIntensity;
		xyzFallof[0][3] = -dlorigin[0] * inverseIntensity;
		pglTexGenfv( GL_S, GL_OBJECT_PLANE, xyzFallof[0] );

		xyzFallof[1][1] = inverseIntensity;
		xyzFallof[1][3] = -dlorigin[1] * inverseIntensity;
		pglTexGenfv( GL_T, GL_OBJECT_PLANE, xyzFallof[1] );

		xyzFallof[2][2] = inverseIntensity;
		xyzFallof[2][3] = -dlorigin[2] * inverseIntensity;

		if( GL_Support( R_TEXTURE_3D_EXT ))
		{
			pglTexGenfv( GL_R, GL_OBJECT_PLANE, xyzFallof[2] );
		}
		else
		{
			GL_Bind( 1, tr.dlightTexture );
			GL_LoadTexMatrix( m );

			pglTexGenfv( GL_S, GL_OBJECT_PLANE, xyzFallof[2] );
			pglTexGenfv( GL_T, GL_OBJECT_PLANE, xyzFallof[3] );
		}

		if( numTempElems )
		{
			if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
				pglDrawRangeElementsEXT( GL_TRIANGLES, 0, r_backacc.numVerts, numTempElems, GL_UNSIGNED_INT, tempElemsArray );
			else pglDrawElements( GL_TRIANGLES, numTempElems, GL_UNSIGNED_INT, tempElemsArray );
		}
		else
		{
			if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
				pglDrawRangeElementsEXT( GL_TRIANGLES, 0, r_backacc.numVerts, r_backacc.numElems, GL_UNSIGNED_INT, elemsArray );
			else pglDrawElements( GL_TRIANGLES, r_backacc.numElems, GL_UNSIGNED_INT, elemsArray );
		}
	}

	if( GL_Support( R_TEXTURE_3D_EXT ))
	{
		GL_EnableTexGen( GL_R, 0 );
		pglDisable( GL_TEXTURE_3D );
		pglEnable( GL_TEXTURE_2D );
	}
	else
	{
		pglDisable( GL_TEXTURE_2D );
		GL_SelectTexture( 0 );
	}
}

//===================================================================

static ref_shader_t *r_coronaShader;

/*
=================
R_InitCoronas
=================
*/
void R_InitCoronas( void )
{
	r_coronaShader = R_LoadShader( "*corona", SHADER_FLARE, true, TF_NOMIPMAP|TF_NOPICMIP|TF_UNCOMPRESSED|TF_CLAMP, SHADER_INVALID );
}

/*
=================
R_DrawCoronas
=================
*/
void R_DrawCoronas( void )
{
	uint		i;
	float		dist;
	ref_dlight_t	*light;
	meshbuffer_t	*mb;
	pmtrace_t		tr;

	if( r_dynamiclight->integer != 2 )
		return;

	for( i = 0, light = r_dlights; i < r_numDlights; i++, light++ )
	{
		dist =	RI.vpn[0] * ( light->origin[0] - RI.viewOrigin[0] ) +
			RI.vpn[1] * ( light->origin[1] - RI.viewOrigin[1] ) +
			RI.vpn[2] * ( light->origin[2] - RI.viewOrigin[2] );

		if( dist < 24.0f ) continue;
		dist -= light->intensity;

		R_TraceLine( &tr, light->origin, RI.viewOrigin, FTRACE_IGNORE_GLASS );
		if( tr.fraction != 1.0f )
			continue;

		mb = R_AddMeshToList( MB_CORONA, NULL, r_coronaShader, -((signed int)i + 1 ));
		if( mb ) mb->shaderkey |= ( bound( 1, 0x4000 - (uint)dist, 0x4000 - 1 ) << 12 );
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

	if( !r_worldbrushmodel->lightgrid )
	{
		// pre-defined light vector
		VectorCopy( RI.refdef.skyVec, lightDir );
		return;
	}

	for( i = 0; i < 3; i++ )
	{
		vf1[i] = (origin[i] - r_worldbrushmodel->gridMins[i]) / r_worldbrushmodel->gridSize[i];
		vi[i] = (int)vf1[i];
		vf1[i] = vf1[i] - floor( vf1[i] );
		vf2[i] = 1.0f - vf1[i];
	}

	elem[0] = vi[2] * r_worldbrushmodel->gridBounds[3] + vi[1] * r_worldbrushmodel->gridBounds[0] + vi[0];
	elem[1] = elem[0] + r_worldbrushmodel->gridBounds[0];
	elem[2] = elem[0] + r_worldbrushmodel->gridBounds[3];
	elem[3] = elem[2] + r_worldbrushmodel->gridBounds[0];

	for( i = 0; i < 4; i++ )
	{
		if( elem[i] < 0 || elem[i] >= r_worldbrushmodel->numgridpoints - 1 )
		{
			VectorSet( lightDir, 1.0f, 0.0f, -1.0f );
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
		R_LatLongToNorm( r_worldbrushmodel->lightgrid[elem[i]+0].direction, tdir );
		VectorScale( tdir, scale[i*2+0], tdir );

		for( k = 0; k < LM_STYLES && ( s = r_worldbrushmodel->lightgrid[elem[i]+0].styles[k] ) != 255; k++ )
		{
			lightDir[0] += r_lightStyles[s].rgb[0] * tdir[0];
			lightDir[1] += r_lightStyles[s].rgb[1] * tdir[1];
			lightDir[2] += r_lightStyles[s].rgb[2] * tdir[2];
		}

		R_LatLongToNorm( r_worldbrushmodel->lightgrid[elem[i]+1].direction, tdir );
		VectorScale( tdir, scale[i*2+1], tdir );

		for( k = 0; k < LM_STYLES && ( s = r_worldbrushmodel->lightgrid[elem[i]+1].styles[k] ) != 255; k++ )
		{
			lightDir[0] += r_lightStyles[s].rgb[0] * tdir[0];
			lightDir[1] += r_lightStyles[s].rgb[1] * tdir[1];
			lightDir[2] += r_lightStyles[s].rgb[2] * tdir[2];
		}
	}
}

/*
=======================================================================

	AMBIENT & DIFFUSE LIGHTING

=======================================================================
*/

static vec3_t	r_pointColor;
static vec3_t	r_lightColors[MAX_ARRAY_VERTS];

/*
=================
R_RecursiveLightPoint
=================
*/
static bool R_RecursiveLightPoint( mnode_t *node, const vec3_t start, const vec3_t end )
{
	int		side;
	mplane_t		*plane;
	msurface_t	*surf;
	mtexinfo_t	*tex;
	vec3_t		mid, scale;
	float		front, back, frac;
	int		i, map, size, s, t;
	byte		*lm;

	// didn't hit anything
	if( !node->plane ) return false;

	// calculate mid point
	plane = node->plane;
	if( plane->type < 3 )
	{
		front = start[plane->type] - plane->dist;
		back = end[plane->type] - plane->dist;
	}
	else
	{
		front = DotProduct( start, plane->normal ) - plane->dist;
		back = DotProduct( end, plane->normal ) - plane->dist;
	}

	side = front < 0;
	if(( back < 0 ) == side )
		return R_RecursiveLightPoint( node->children[side], start, end );

	frac = front / ( front - back );

	VectorLerp( start, frac, end, mid );

	// co down front side	
	if( R_RecursiveLightPoint( node->children[side], start, mid ))
		return true; // hit something

	if(( back < 0 ) == side )
		return false;// didn't hit anything

	// check for impact on this node
	surf = node->firstface;

	for( i = 0; i < node->numfaces; i++, surf++ )
	{
		tex = surf->texinfo;

		if( surf->flags & SURF_DRAWTILED )
			continue;	// no lightmaps

		s = DotProduct( mid, tex->vecs[0] ) + tex->vecs[0][3] - surf->textureMins[0];
		t = DotProduct( mid, tex->vecs[1] ) + tex->vecs[1][3] - surf->textureMins[1];

		if(( s < 0 || s > surf->extents[0] ) || ( t < 0 || t > surf->extents[1] ))
			continue;

		s >>= 4;
		t >>= 4;

		if( !surf->samples )
			return true;

		VectorClear( r_pointColor );

		lm = surf->samples + 3 * (t * surf->lmWidth + s);
		size = surf->lmWidth * surf->lmHeight * 3;

		for( map = 0; map < surf->numstyles; map++ )
		{
			VectorScale( r_lightStyles[surf->styles[map]].rgb, r_lighting_modulate->value * (1.0 / 255), scale );

			r_pointColor[0] += lm[0] * scale[0];
			r_pointColor[1] += lm[1] * scale[1];
			r_pointColor[2] += lm[2] * scale[2];

			lm += size; // skip to next lightmap
		}

		return true;
	}

	// go down back side
	return R_RecursiveLightPoint( node->children[!side], mid, end );
}

/*
=================
R_LightForPoint
=================
*/
void R_LightForPoint( const vec3_t point, vec3_t ambientLight )
{
	ref_dlight_t	*dl;
	vec3_t		end, dir;
	float		dist, add;
	int		lnum;

	// set to full bright if no light data
	if( !r_worldbrushmodel || !r_worldbrushmodel->lightdata )
	{
		VectorSet( ambientLight, 1.0f, 1.0f, 1.0f );
		return;
	}

	// Get lighting at this point
	VectorSet( end, point[0], point[1], point[2] - 8192 );
	VectorSet( r_pointColor, 1.0f, 1.0f, 1.0f );

	R_RecursiveLightPoint( r_worldbrushmodel->nodes, point, end );

	VectorCopy( r_pointColor, ambientLight );

	// add dynamic lights
	if( r_dynamiclight->integer )
	{
		for( lnum = 0, dl = r_dlights; lnum < r_numDlights; lnum++, dl++ )
		{
			VectorSubtract( dl->origin, point, dir );
			dist = VectorLength( dir );

			if( !dist || dist > dl->intensity )
				continue;

			add = ( dl->intensity - dist ) * ( 1.0f / 255 );
			VectorMA( ambientLight, add, dl->color, ambientLight );
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
	ref_dlight_t	*dl;
	vec3_t		dir;
	float		dist;
	int		lnum;

	// get light direction from light grid
	R_ReadLightGrid( origin, lightDir );

	// add dynamic lights
	if( radius > 0.0f && r_dynamiclight->integer && r_numDlights )
	{
		for( lnum = 0, dl = r_dlights; lnum < r_numDlights; lnum++, dl++ )
		{
			if( !BoundsAndSphereIntersect( dl->mins, dl->maxs, origin, radius ))
				continue;

			VectorSubtract( dl->origin, origin, dir );
			dist = VectorLength( dir );

			if( !dist || dist > dl->intensity + radius )
				continue;
			VectorAdd( lightDir, dir, lightDir );
		}
	}
}

/*
===============
R_LightForOrigin
===============
*/
void R_LightForOrigin( const vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius )
{
	int		i, j;
	int		k, s;
	float		dot, t[8];
	vec3_t		vf, vf2, tdir;
	int		vi[3], elem[4];
	vec3_t		ambientLocal, diffuseLocal;
	vec_t		*gridSize, *gridMins;
	int		*gridBounds;
	mgridlight_t	*lightgrid;

	if( !r_worldmodel || !r_worldbrushmodel->lightgrid || !r_worldbrushmodel->numgridpoints )
	{
		// get fullbright
		VectorSet( ambientLocal, 255, 255, 255 );
		VectorSet( diffuseLocal, 255, 255, 255 );
		VectorSet( dir, 0.5f, 0.2f, -1.0f );
		goto dynamic;
	}
	else
	{
		VectorSet( ambientLocal, 0, 0, 0 );
		VectorSet( diffuseLocal, 0, 0, 0 );
	}

	lightgrid = r_worldbrushmodel->lightgrid;
	gridSize = r_worldbrushmodel->gridSize;
	gridMins = r_worldbrushmodel->gridMins;
	gridBounds = r_worldbrushmodel->gridBounds;

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
		if( elem[i] < 0 || elem[i] >= ( r_worldbrushmodel->numgridpoints - 1 ))
		{
			VectorSet( dir, 0.5f, 0.2f, -1.0f );
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
			dir[0] += r_lightStyles[s].rgb[0] * tdir[0];
			dir[1] += r_lightStyles[s].rgb[1] * tdir[1];
			dir[2] += r_lightStyles[s].rgb[2] * tdir[2];
		}

		R_LatLongToNorm( lightgrid[elem[i] + 1].direction, tdir );
		VectorScale( tdir, t[i*2+1], tdir );
		for( k = 0; k < LM_STYLES && ( s = lightgrid[elem[i] + 1].styles[k] ) != 255; k++ )
		{
			dir[0] += r_lightStyles[s].rgb[0] * tdir[0];
			dir[1] += r_lightStyles[s].rgb[1] * tdir[1];
			dir[2] += r_lightStyles[s].rgb[2] * tdir[2];
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
					if( ( s = lightgrid[elem[i]].styles[k] ) != 255 )
						ambientLocal[j] += t[i*2] * lightgrid[elem[i]].ambient[k][j] * r_lightStyles[s].rgb[j];
					if( ( s = lightgrid[elem[i] + 1].styles[k] ) != 255 )
						ambientLocal[j] += t[i*2+1] * lightgrid[elem[i] + 1].ambient[k][j] * r_lightStyles[s].rgb[j];
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
						diffuseLocal[j] += t[i*2] * lightgrid[elem[i]].diffuse[k][j] * r_lightStyles[s].rgb[j];
					if( ( s = lightgrid[elem[i] + 1].styles[k] ) != 255 )
						diffuseLocal[j] += t[i*2+1] * lightgrid[elem[i] + 1].diffuse[k][j] * r_lightStyles[s].rgb[j];
				}
			}
		}
	}

dynamic:
	// add dynamic lights
	if( radius && r_dynamiclight->integer && r_numDlights )
	{
		uint		lnum;
		ref_dlight_t	*dl;
		float		dist, dist2, add;
		vec3_t		direction;
		bool		anyDlights = false;

		for( lnum = 0, dl = r_dlights; lnum < r_numDlights; lnum++, dl++ )
		{
			if( !BoundsAndSphereIntersect( dl->mins, dl->maxs, origin, radius ) )
				continue;

			VectorSubtract( dl->origin, origin, direction );
			dist = VectorLength( direction );

			if( !dist || dist > dl->intensity + radius )
				continue;

			if( !anyDlights )
			{
				VectorNormalizeFast( dir );
				anyDlights = true;
			}

			add = 1.0f - (dist / (dl->intensity + radius));
			dist2 = add * 0.5f / dist;
			for( i = 0; i < 3; i++ )
			{
				dot = dl->color[i] * add;
				diffuseLocal[i] += dot;
				ambientLocal[i] += dot * 0.05;
				dir[i] += direction[i] * dist2;
			}
		}
	}

	VectorNormalizeFast( dir );

	if( ambient )
	{
		dot = bound( 0.0f, r_lighting_ambientscale->value, 1.0f ) * ( 1.0f / 255.0f );
		for( i = 0; i < 3; i++ )
		{
			ambient[i] = ambientLocal[i] * dot;
			ambient[i] = bound( 0, ambient[i], 1 );
		}
		ambient[3] = 1.0f;
	}

	if( diffuse )
	{
		dot = bound( 0.0f, r_lighting_directedscale->value, 1.0f ) * ( 1.0f / 255.0f );
		for( i = 0; i < 3; i++ )
		{
			diffuse[i] = diffuseLocal[i] * dot;
			diffuse[i] = bound( 0, diffuse[i], 1 );
		}
		diffuse[3] = 1.0f;
	}
}
/*
=================
R_LightForEntity
=================
*/
void R_LightForEntity( ref_entity_t *e, byte *bArray )
{
	ref_dlight_t	*dl;
	vec3_t		end, dir;
	float		add, dot, dist, intensity, radius;
	vec3_t		ambientLight, directedLight, lightDir;
	float		*cArray;
	int		i, l;

	if(( e->flags & EF_FULLBRIGHT ) || r_fullbright->integer )
		return;

	// never gets diffuse lighting for world brushes
	if( !e->model || ( e->model->type == mod_brush ) || ( e->model->type == mod_world ))
		return;

	// get lighting at this point
	VectorSet( end, e->lightingOrigin[0], e->lightingOrigin[1], e->lightingOrigin[2] - 8192 );
	VectorSet( r_pointColor, 1.0f, 1.0f, 1.0f );

	R_RecursiveLightPoint( r_worldbrushmodel->nodes, e->lightingOrigin, end );

	VectorScale( r_pointColor, r_lighting_ambientscale->value, ambientLight );
	VectorScale( r_pointColor, r_lighting_directedscale->value, directedLight );

	R_ReadLightGrid( e->lightingOrigin, lightDir );

	// always have some light
	if( e->flags & EF_MINLIGHT )
	{
		for( i = 0; i < 3; i++ )
		{
			if( ambientLight[i] > 0.01f )
				break;
		}

		if( i == 3 ) VectorSet( ambientLight, 0.01f, 0.01f, 0.01f );
	}

	// compute lighting at each vertex

	// rotate direction
	Matrix3x3_Transform( e->axis, lightDir, dir );
	VectorNormalizeFast( dir );

	for( i = 0; i < r_backacc.numColors; i++ )
	{
		dot = DotProduct( normalsArray[i], dir );
		if( dot <= 0.0f ) VectorCopy( ambientLight, r_lightColors[i] );
		else VectorMA( ambientLight, dot, directedLight, r_lightColors[i] );
	}

	// add dynamic lights
	if( r_dynamiclight->integer )
	{
		if( e->rtype == RT_MODEL )
			radius = e->model->radius;
		else radius = e->radius;

		for( l = 0, dl = r_dlights; l < r_numDlights; l++, dl++ )
		{
			VectorSubtract( dl->origin, e->lightingOrigin, dir );
			dist = VectorLength( dir );

			if( !dist || dist > dl->intensity + radius )
				continue;

			Matrix3x3_Transform( e->axis, dir, lightDir );
			intensity = dl->intensity * 8;

			// compute lighting at each vertex
			for( i = 0; i < r_backacc.numColors; i++ )
			{
				VectorSubtract( lightDir, vertsArray[i], dir );
				add = DotProduct( normalsArray[i], dir );
				if( add <= 0.0f ) continue;

				dot = DotProduct( dir, dir );
				add *= ( intensity / dot ) * rsqrt( dot );
				VectorMA( r_lightColors[i], add, dl->color, r_lightColors[i] );
			}
		}
	}

	cArray = r_lightColors[0];

	for( i = 0; i < r_backacc.numColors; i++, bArray += 4, cArray += 3 )
	{
		bArray[0] = R_FloatToByte( cArray[0] );
		bArray[1] = R_FloatToByte( cArray[1] );
		bArray[2] = R_FloatToByte( cArray[2] );
	}
}

/*
=======================================================================

 LIGHT SAMPLING

=======================================================================
*/
static vec3_t	r_blockLights[LIGHTMAP_TEXTURE_WIDTH*LIGHTMAP_TEXTURE_HEIGHT];

/*
=================
R_SetCacheState
=================
*/
static void R_SetCacheState( msurface_t *surf )
{
	int	map;

	for( map = 0; map < surf->numstyles; map++ )
		surf->cached[map] = r_lightStyles[surf->styles[map]].white;
}

/*
=================
R_BuildLightmap

combine and scale multiple lightmaps into the
floating format in r_blockLights
=================
*/
static void R_BuildLightmap( msurface_t *surf, byte *dest, int stride )
{
	int	i, map, size, s, t;
	byte	*lm;
	vec3_t	scale;
	float	*bl, max;

	lm = surf->samples;
	size = surf->lmWidth * surf->lmHeight;

	if( !lm )
	{
		// set to full bright if no light data
		for( i = 0, bl = r_blockLights[0]; i < size; i++, bl += 3 )
		{
			bl[0] = 255;
			bl[1] = 255;
			bl[2] = 255;
		}
	}
	else
	{
		// add all the lightmaps
		VectorScale( r_lightStyles[surf->styles[0]].rgb, r_lighting_modulate->value, scale );

		for( i = 0, bl = r_blockLights[0]; i < size; i++, bl += 3, lm += 3 )
		{
			bl[0] = lm[0] * scale[0];
			bl[1] = lm[1] * scale[1];
			bl[2] = lm[2] * scale[2];
		}

		if( surf->numstyles > 1 )
		{
			for( map = 1; map < surf->numstyles; map++ )
			{
				VectorScale( r_lightStyles[surf->styles[map]].rgb, r_lighting_modulate->value, scale );

				for( i = 0, bl = r_blockLights[0]; i < size; i++, bl += 3, lm += 3 )
				{
					bl[0] += lm[0] * scale[0];
					bl[1] += lm[1] * scale[1];
					bl[2] += lm[2] * scale[2];
				}
			}
		}
	}

	// put into texture format
	stride -= (surf->lmWidth << 2);
	bl = r_blockLights[0];

	for( t = 0; t < surf->lmHeight; t++ )
	{
		for( s = 0; s < surf->lmWidth; s++ )
		{
			// catch negative lights
			if( bl[0] < 0.0f ) bl[0] = 0.0f;
			if( bl[1] < 0.0f ) bl[1] = 0.0f;
			if( bl[2] < 0.0f ) bl[2] = 0.0f;

			// Determine the brightest of the three color components
			max = VectorMax( bl );

			// rescale all the color components if the intensity of the
			// greatest channel exceeds 255
			if( max > 255.0f )
			{
				max = 255.0f / max;

				dest[0] = bl[0] * max;
				dest[1] = bl[1] * max;
				dest[2] = bl[2] * max;
				dest[3] = 255;
			}
			else
			{
				dest[0] = bl[0];
				dest[1] = bl[1];
				dest[2] = bl[2];
				dest[3] = 255;
			}

			bl += 3;
			dest += 4;
		}
		dest += stride;
	}
}

/*
=======================================================================

 LIGHTSTYLE HANDLING

=======================================================================
*/
/*
=======================
R_AddSuperLightStyle
=======================
*/
int R_AddSuperLightStyle( const int lightmapNum, const byte *lightmapStyles )
{
	int		i, j;
	ref_style_t	*sls;

	for( i = 0, sls = tr.superLightStyles; i < tr.numSuperLightStyles; i++, sls++ )
	{
		if( sls->lightmapNum != lightmapNum )
			continue;

		for( j = 0; j < LM_STYLES; j++ )
		{
			if( sls->lightmapStyles[j] != lightmapStyles[j] )
				break;
		}
		if( j == LM_STYLES )
			return i;
	}

	if( tr.numSuperLightStyles == MAX_SUPER_STYLES )
		Host_Error( "R_AddSuperLightStyle: MAX_SUPERSTYLES limit exceeded\n" );

	// create new style
	sls->features = 0;
	sls->lightmapNum = lightmapNum;

	for( j = 0; j < LM_STYLES; j++ )
	{
		sls->lightmapStyles[j] = lightmapStyles[j];

		if( j )
		{
			if( lightmapStyles[j] != 255 )
				sls->features |= ( MF_LMCOORDS << j );
		}
	}
	return tr.numSuperLightStyles++;
}

/*
=======================================================================

 LIGHTMAP ALLOCATION

=======================================================================
*/
typedef struct
{
	int	currentNum;
	int	allocated[LIGHTMAP_TEXTURE_WIDTH];
	byte	buffer[LIGHTMAP_TEXTURE_WIDTH*LIGHTMAP_TEXTURE_HEIGHT*4];
} lmState_t;

static lmState_t	r_lmState;

/*
=================
 R_UploadLightmap
=================
*/
static void R_UploadLightmap( void )
{
	string	lmName;
	rgbdata_t	r_image;
	texture_t *image;	

	if( r_lmState.currentNum == MAX_LIGHTMAPS - 1 )
		Host_Error( "R_UploadLightmap: MAX_LIGHTMAPS limit exceeded\n" );

	com.snprintf( lmName, sizeof( lmName ), "*lightmap%i", r_lmState.currentNum );

	Mem_Set( &r_image, 0, sizeof( r_image ));

	r_image.width = LIGHTMAP_TEXTURE_WIDTH;
	r_image.height = LIGHTMAP_TEXTURE_HEIGHT;
	r_image.type = PF_RGBA_GN;
	r_image.size = r_image.width * r_image.height * 4;
	r_image.depth = r_image.numMips = 1;
	r_image.flags = IMAGE_HAS_COLOR;	// FIXME: detecting grayscale lightmaps for quake1
	r_image.buffer = r_lmState.buffer;

	image = R_LoadTexture( lmName, &r_image, 0, TF_LIGHTMAP|TF_NOPICMIP|TF_UNCOMPRESSED|TF_CLAMP|TF_NOMIPMAP );
	tr.lightmapTextures[r_lmState.currentNum++] = image;

	// reset
	Mem_Set( r_lmState.allocated, 0, sizeof( r_lmState.allocated ));
	Mem_Set( r_lmState.buffer, 255, sizeof( r_lmState.buffer ));
}

/*
=================
R_AllocLightmapBlock
=================
*/
static byte *R_AllocLightmapBlock( int width, int height, int *s, int *t )
{
	int	i, j;
	int	best1, best2;

	best1 = LIGHTMAP_TEXTURE_HEIGHT;

	for( i = 0; i < LIGHTMAP_TEXTURE_WIDTH - width; i++ )
	{
		best2 = 0;

		for( j = 0; j < width; j++ )
		{
			if( r_lmState.allocated[i+j] >= best1 )
				break;
			if( r_lmState.allocated[i+j] > best2 )
				best2 = r_lmState.allocated[i+j];
		}

		if( j == width )
		{
			// this is a valid spot
			*s = i;
			*t = best1 = best2;
		}
	}

	if( best1 + height > LIGHTMAP_TEXTURE_HEIGHT )
		return NULL;

	for( i = 0; i < width; i++ )
		r_lmState.allocated[*s + i] = best1 + height;

	return r_lmState.buffer + (( *t * LIGHTMAP_TEXTURE_WIDTH + *s ) * 4 );
}

/*
=================
R_BeginBuildingLightmaps
=================
*/
void R_BeginBuildingLightmaps( void )
{
	int	i;

	// setup the base lightstyles so the lightmaps won't have to be 
	// regenerated the first time they're seen
	for( i = 0; i < MAX_LIGHTSTYLES; i++ )
	{
		r_lightStyles[i].white = 3;
		r_lightStyles[i].rgb[0] = 1;
		r_lightStyles[i].rgb[1] = 1;
		r_lightStyles[i].rgb[2] = 1;
	}

	// release old lightmaps
	for( i = 0; i < r_lmState.currentNum; i++ )
	{
		if( !tr.lightmapTextures[i] ) continue;
		R_FreeImage( tr.lightmapTextures[i] );
	}

	r_lmState.currentNum = -1;
	tr.numSuperLightStyles = 0;

	Mem_Set( tr.lightmapTextures, 0, sizeof( tr.lightmapTextures ));
	Mem_Set( r_lmState.allocated, 0, sizeof( r_lmState.allocated ));
	Mem_Set( r_lmState.buffer, 255, sizeof( r_lmState.buffer ));
}

/*
=================
R_EndBuildingLightmaps
=================
*/
void R_EndBuildingLightmaps( void )
{
	if( r_lmState.currentNum == -1 )
		return;

	R_UploadLightmap();
}

/*
=================
R_BuildSurfaceLightmap
=================
*/
void R_BuildSurfaceLightmap( msurface_t *surf )
{
	byte	*base;

	if( !R_SurfPotentiallyLit( surf ))
	{
		surf->lmNum = -1;
		return; // no lightmaps
	}

	base = R_AllocLightmapBlock( surf->lmWidth, surf->lmHeight, &surf->lmS, &surf->lmT );

	if( !base )
	{
		if( r_lmState.currentNum != -1 ) R_UploadLightmap();

		base = R_AllocLightmapBlock( surf->lmWidth, surf->lmHeight, &surf->lmS, &surf->lmT );
		if( !base ) Host_Error( "AllocBlock: full\n" );
	}

	if( r_lmState.currentNum == -1 )
		r_lmState.currentNum = 0;

	surf->lmNum = r_lmState.currentNum;

	R_SetCacheState( surf );
	R_BuildLightmap( surf, base, LIGHTMAP_TEXTURE_WIDTH * 4 );
}

/*
=================
R_UpdateSurfaceLightmap
=================
*/
void R_UpdateSurfaceLightmap( msurface_t *surf )
{
	int	map;

	ASSERT( surf );

	// is this surface allowed to have a lightmap?
	if( !R_SurfPotentiallyLit( surf ))
		return;

	// dynamic this frame or dynamic previously
	if( r_dynamiclight->integer )
	{
		for( map = 0; map < surf->numstyles; map++ )
		{
			if( r_lightStyles[surf->styles[map]].white != surf->cached[map] )
				goto update_lightmap;
		}
	}

	// no need to update
	return;

update_lightmap:
	R_SetCacheState( surf );
	R_BuildLightmap( surf, r_lmState.buffer, surf->lmWidth * 4 );

	GL_Bind( 0, tr.lightmapTextures[surf->lmNum] );

	pglTexSubImage2D( GL_TEXTURE_2D, 0, surf->lmS, surf->lmT, surf->lmWidth, surf->lmHeight,
		GL_RGBA, GL_UNSIGNED_BYTE, r_lmState.buffer );
}