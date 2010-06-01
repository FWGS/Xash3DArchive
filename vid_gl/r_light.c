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

/*
=============================================================================

 DYNAMIC LIGHTS

=============================================================================
*/

/*
=============
R_RecursiveLightNode
=============
*/
void R_RecursiveLightNode( dlight_t *light, int bit, mnode_t *node )
{
	float		dist;
	msurface_t	*surf;
	cplane_t		*splitplane;
	int		i, sidebit;

	if( !node->plane ) return;

	splitplane = node->plane;
	if( splitplane->type < 3 ) dist = light->origin[splitplane->type] - splitplane->dist;
	else dist = DotProduct( light->origin, splitplane->normal ) - splitplane->dist;
	
	if( dist > light->intensity )
	{
		R_RecursiveLightNode( light, bit, node->children[0] );
		return;
	}

	if( dist < -light->intensity )
	{
		R_RecursiveLightNode( light, bit, node->children[1] );
		return;
	}
		
	// mark the polygons
	surf = node->firstface;

	for( i = 0; i < node->numfaces; i++, surf++ )
	{
		if( surf->plane->type < 3 ) dist = light->origin[surf->plane->type] - surf->plane->dist;
		else dist = DotProduct( light->origin, surf->plane->normal ) - surf->plane->dist;

		if( dist >= 0 ) sidebit = 0;
		else sidebit = SURF_PLANEBACK;

		if(( surf->flags & SURF_PLANEBACK ) != sidebit )
			continue;

		if( surf->dlightFrame != r_framecount )
		{
			surf->dlightBits = bit; // was 0
			surf->dlightFrame = r_framecount;
		}
		else surf->dlightBits |= bit;
	}

	R_RecursiveLightNode( light, bit, node->children[0] );
	R_RecursiveLightNode( light, bit, node->children[1] );
}

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
=================
R_MarkLights
=================
*/
void R_MarkLights( uint clipflags )
{
	dlight_t	*dl;
	int	l;

	if( !r_dynamiclight->integer || !r_numDlights )
		return;

	for( l = 0, dl = r_dlights; l < r_numDlights; l++, dl++ )
	{
		if( R_CullSphere( dl->origin, dl->intensity, clipflags ))
			continue;

		R_RecursiveLightNode( dl, BIT( l ), r_worldbrushmodel->nodes );
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
	dlight_t		*light;
	meshbuffer_t	*mb;
	trace_t		tr;

	if( r_dynamiclight->integer != 2 )
		return;

	for( i = 0, light = r_dlights; i < r_numDlights; i++, light++ )
	{
		dist =	RI.vpn[0] * ( light->origin[0] - RI.viewOrigin[0] ) +
			RI.vpn[1] * ( light->origin[1] - RI.viewOrigin[1] ) +
			RI.vpn[2] * ( light->origin[2] - RI.viewOrigin[2] );

		if( dist < 24.0f ) continue;
		dist -= light->intensity;

		R_TraceLine( &tr, light->origin, RI.viewOrigin );
		if( tr.flFraction != 1.0f )
			continue;

		mb = R_AddMeshToList( MB_CORONA, NULL, r_coronaShader, -((signed int)i + 1 ));
		if( mb ) mb->shaderkey |= ( bound( 1, 0x4000 - (uint)dist, 0x4000 - 1 ) << 12 );
	}
}

//===================================================================

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
	mgridlight_t	**lightarray;

	if( !r_worldmodel || !r_worldbrushmodel->lightgrid || !r_worldbrushmodel->numlightgridelems )
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

	lightarray = r_worldbrushmodel->lightarray;
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
		if( elem[i] < 0 || elem[i] >= ( r_worldbrushmodel->numlightarrayelems - 1 ))
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
		R_LatLongToNorm( lightarray[elem[i]]->direction, tdir );
		VectorScale( tdir, t[i *2], tdir );
		for( k = 0; k < LM_STYLES && ( s = lightarray[elem[i]]->styles[k] ) != 255; k++ )
		{
			dir[0] += r_lightStyles[s].rgb[0] * tdir[0];
			dir[1] += r_lightStyles[s].rgb[1] * tdir[1];
			dir[2] += r_lightStyles[s].rgb[2] * tdir[2];
		}

		R_LatLongToNorm( lightarray[elem[i] + 1]->direction, tdir );
		VectorScale( tdir, t[i *2+1], tdir );
		for( k = 0; k < LM_STYLES && ( s = lightarray[elem[i] + 1]->styles[k] ) != 255; k++ )
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
					if( ( s = lightarray[elem[i]]->styles[k] ) != 255 )
						ambientLocal[j] += t[i*2] * lightarray[elem[i]]->ambient[k][j] * r_lightStyles[s].rgb[j];
					if( ( s = lightarray[elem[i] + 1]->styles[k] ) != 255 )
						ambientLocal[j] += t[i*2+1] * lightarray[elem[i] + 1]->ambient[k][j] * r_lightStyles[s].rgb[j];
				}
			}
		}
		if( diffuse || radius )
		{
			for( i = 0; i < 4; i++ )
			{
				for( k = 0; k < LM_STYLES; k++ )
				{
					if( ( s = lightarray[elem[i]]->styles[k] ) != 255 )
						diffuseLocal[j] += t[i*2] * lightarray[elem[i]]->diffuse[k][j] * r_lightStyles[s].rgb[j];
					if( ( s = lightarray[elem[i] + 1]->styles[k] ) != 255 )
						diffuseLocal[j] += t[i*2+1] * lightarray[elem[i] + 1]->diffuse[k][j] * r_lightStyles[s].rgb[j];
				}
			}
		}
	}

dynamic:
	// add dynamic lights
	if( radius && r_dynamiclight->integer && r_numDlights )
	{
		unsigned int lnum;
		dlight_t *dl;
		float dist, dist2, add;
		vec3_t direction;
		bool anyDlights = false;

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
		dot = bound( 0.0f, r_lighting_ambientscale->value, 1.0f ) * ( 1 << mapConfig.pow2MapOvrbr ) * ( 1.0 / 255.0 );
		for( i = 0; i < 3; i++ )
		{
			ambient[i] = ambientLocal[i] * dot;
			ambient[i] = bound( 0, ambient[i], 1 );
		}
		ambient[3] = 1.0f;
	}

	if( diffuse )
	{
		dot = bound( 0.0f, r_lighting_directedscale->value, 1.0f ) * ( 1 << mapConfig.pow2MapOvrbr ) * ( 1.0 / 255.0 );
		for( i = 0; i < 3; i++ )
		{
			diffuse[i] = diffuseLocal[i] * dot;
			diffuse[i] = bound( 0, diffuse[i], 1 );
		}
		diffuse[3] = 1.0f;
	}
}

/*
===============
R_LightForEntity
===============
*/
void R_LightForEntity( ref_entity_t *e, byte *bArray )
{
	dlight_t	*dl;
	uint	i, lnum;
	uint	dlightbits;
	float	dot, dist;
	vec3_t	lightDirs[MAX_DLIGHTS], direction, temp;
	vec4_t	ambient, diffuse;

	if(( e->flags & EF_FULLBRIGHT ) || r_fullbright->value )
		return;

	// probably weird shader, see mpteam4 for example
	if( !e->model || ( e->model->type == mod_brush ) || (e->model->type == mod_world ))
		return;

	R_LightForOrigin( e->lightingOrigin, temp, ambient, diffuse, 0 );

	if( e->flags & EF_MINLIGHT )
	{
		for( i = 0; i < 3; i++ )
			if( ambient[i] > 0.01 )
				break;
		if( i == 3 )
			VectorSet( ambient, 0.01f, 0.01f, 0.01f );
	}

	// rotate direction
	Matrix3x3_Transform( e->axis, temp, direction );

	// see if we are affected by dynamic lights
	dlightbits = 0;
	if( r_dynamiclight->integer == 1 && r_numDlights )
	{
		for( lnum = 0, dl = r_dlights; lnum < r_numDlights; lnum++, dl++ )
		{
			// translate
			VectorSubtract( dl->origin, e->origin, lightDirs[lnum] );
			dist = VectorLength( lightDirs[lnum] );

			if( !dist || dist > dl->intensity + e->model->radius * e->scale )
				continue;
			dlightbits |= ( 1<<lnum );
		}
	}

	if( !dlightbits )
	{
		vec3_t color;

		for( i = 0; i < r_backacc.numColors; i++, bArray += 4 )
		{
			dot = DotProduct( normalsArray[i], direction );
			if( dot <= 0 )
				VectorCopy( ambient, color );
			else
				VectorMA( ambient, dot, diffuse, color );

			bArray[0] = R_FloatToByte( color[0] );
			bArray[1] = R_FloatToByte( color[1] );
			bArray[2] = R_FloatToByte( color[2] );
		}
	}
	else
	{
		float add, intensity8, dot, *cArray, *dir;
		vec3_t dlorigin, tempColorsArray[MAX_ARRAY_VERTS];

		cArray = tempColorsArray[0];
		for( i = 0; i < r_backacc.numColors; i++, cArray += 3 )
		{
			dot = DotProduct( normalsArray[i], direction );
			if( dot <= 0 )
				VectorCopy( ambient, cArray );
			else
				VectorMA( ambient, dot, diffuse, cArray );
		}

		for( lnum = 0, dl = r_dlights; lnum < r_numDlights; lnum++, dl++ )
		{
			if( !( dlightbits & ( 1<<lnum ) ) )
				continue;

			// translate
			dir = lightDirs[lnum];

			// rotate
			Matrix3x3_Transform( e->axis, dir, dlorigin );
			intensity8 = dl->intensity * 8 * e->scale;

			cArray = tempColorsArray[0];
			for( i = 0; i < r_backacc.numColors; i++, cArray += 3 )
			{
				VectorSubtract( dlorigin, vertsArray[i], dir );
				add = DotProduct( normalsArray[i], dir );

				if( add > 0 )
				{
					dot = DotProduct( dir, dir );
					add *= ( intensity8 / dot ) * rsqrt( dot );
					VectorMA( cArray, add, dl->color, cArray );
				}
			}
		}

		cArray = tempColorsArray[0];
		for( i = 0; i < r_backacc.numColors; i++, bArray += 4, cArray += 3 )
		{
			bArray[0] = R_FloatToByte( cArray[0] );
			bArray[1] = R_FloatToByte( cArray[1] );
			bArray[2] = R_FloatToByte( cArray[2] );
		}
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
R_AddDynamicLights
=================
*/
static void R_AddDynamicLights( msurface_t *surf )
{
	int		l, s, t, sd, td;
	float		sl, tl, sacc, tacc;
	float		dist, rad, scale;
	vec3_t		origin, tmp, impact;
	mtexinfo_t	*tex = surf->texinfo;
	ref_entity_t	*e = RI.currententity;
	cplane_t		*plane;
	dlight_t		*dl;
	float		*bl;

	// invalid entity ?
	if( !e ) return;

	for (l = 0, dl = r_dlights; l < r_numDlights; l++, dl++ )
	{
		if( !( surf->dlightBits & ( 1<<l )))
			continue;	// not lit by this light

		if( !Matrix3x3_Compare( e->axis, matrix3x3_identity ))
		{
			VectorSubtract( dl->origin, e->origin, tmp );
			Matrix3x3_Transform( e->axis, tmp, origin );
		}
		else VectorSubtract( dl->origin, e->origin, origin );

		plane = surf->plane;

		if( plane->type < 3 )
			dist = origin[plane->type] - plane->dist;
		else dist = DotProduct( origin, plane->normal ) - plane->dist;

		// rad is now the highest intensity on the plane
		rad = dl->intensity - fabs( dist );
		if( rad < 0.0f ) continue;

		if( plane->type < 3 )
		{
			VectorCopy( origin, impact );
			impact[plane->type] -= dist;
		}
		else VectorMA( origin, -dist, plane->normal, impact );

		sl = DotProduct( impact, tex->vecs[0] ) + tex->vecs[0][3] - surf->textureMins[0];
		tl = DotProduct( impact, tex->vecs[1] ) + tex->vecs[1][3] - surf->textureMins[1];

		bl = (float *)r_blockLights;

		for( t = 0, tacc = 0; t < surf->lmHeight; t++, tacc += LM_SAMPLE_SIZE )
		{
			td = tl - tacc;
			if( td < 0 ) td = -td;

			for( s = 0, sacc = 0; s < surf->lmWidth; s++, sacc += LM_SAMPLE_SIZE )
			{
				sd = sl - sacc;
				if( sd < 0 ) sd = -sd;

				if( sd > td ) dist = sd + (td >> 1);
				else dist = td + (sd >> 1);

				if( dist < rad )
				{
					scale = rad - dist;

					bl[0] += dl->color[0] * scale;
					bl[1] += dl->color[1] * scale;
					bl[2] += dl->color[2] * scale;
				}
				bl += 3;
			}
		}
	}
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
		for( i = 0, bl = (float *)r_blockLights; i < size; i++, bl += 3 )
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

		for( i = 0, bl = (float *)r_blockLights; i < size; i++, bl += 3, lm += 3 )
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

				for( i = 0, bl = (float *)r_blockLights; i < size; i++, bl += 3, lm += 3 )
				{
					bl[0] += lm[0] * scale[0];
					bl[1] += lm[1] * scale[1];
					bl[2] += lm[2] * scale[2];
				}
			}
		}

		// add all the dynamic lights
		if( surf->dlightFrame == r_framecount )
			R_AddDynamicLights( surf );
	}

	// put into texture format
	stride -= (surf->lmWidth << 2);
	bl = (float *)r_blockLights;

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
=======================
R_SuperLightStylesCmp

Compare function for qsort
=======================
*/
static int R_SuperLightStylesCmp( ref_style_t *sls1, ref_style_t *sls2 )
{
	int	i;

	if( sls2->lightmapNum > sls1->lightmapNum )
		return 1;
	else if( sls1->lightmapNum > sls2->lightmapNum )
			return -1;

	for( i = 0; i < LM_STYLES; i++ )
	{	
		// compare lightmap styles
		if( sls2->lightmapStyles[i] > sls1->lightmapStyles[i] )
			return 1;
		else if( sls1->lightmapStyles[i] > sls2->lightmapStyles[i] )
			return -1;
	}
	return 0; // equal
}

/*
=======================
R_SortSuperLightStyles
=======================
*/
void R_SortSuperLightStyles( void )
{
	qsort( tr.superLightStyles, tr.numSuperLightStyles, sizeof( ref_style_t ), ( int ( * )( const void *, const void * ))R_SuperLightStylesCmp );
}

/*
=======================================================================

 LIGHTMAP ALLOCATION

=======================================================================
*/
typedef struct
{
	int	glFormat;
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

	image = R_LoadTexture( lmName, &r_image, 4, TF_LIGHTMAP|TF_NOPICMIP|TF_UNCOMPRESSED|TF_CLAMP|TF_NOMIPMAP );
	tr.lightmapTextures[r_lmState.currentNum++] = image;
	r_lmState.glFormat = image->format;

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
		if( tr.lightmapTextures[i] && tr.lightmapTextures[i] != tr.dlightTexture )
			R_FreeImage( tr.lightmapTextures[i] );
	}

	r_lmState.currentNum = -1;

	Mem_Set( tr.lightmapTextures, 0, sizeof( tr.lightmapTextures ));
	Mem_Set( r_lmState.allocated, 0, sizeof( r_lmState.allocated ));
	Mem_Set( r_lmState.buffer, 255, sizeof( r_lmState.buffer ));
	tr.lightmapTextures[DLIGHT_TEXTURE] = tr.dlightTexture;
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

	if( !( surf->shader->flags & SHADER_HASLIGHTMAP ))
		return; // no lightmaps

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
	surf->lightmapTexnum = -1;
	surf->lightmapFrame = r_framecount - 1;

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

	Com_Assert( surf == NULL );

	// Don't attempt a surface more than once a frame
	// FIXME: This is just a nasty work-around at best
	if( surf->lightmapFrame == r_framecount )
		return;
	surf->lightmapFrame = r_framecount;

	// is this surface allowed to have a lightmap?
	if( surf->flags & ( SURF_DRAWSKY|SURF_DRAWTURB ))
	{
		surf->lightmapTexnum = -1;
		return;
	}

	// dynamic this frame or dynamic previously
	if( r_dynamiclight->integer )
	{
		for( map = 0; map < surf->numstyles; map++ )
		{
			if( r_lightStyles[surf->styles[map]].white != surf->cached[map] )
				goto update_lightmap;
		}

		if( surf->dlightFrame == r_framecount )
			goto update_lightmap;
	}

	// no need to update
	surf->lightmapTexnum = surf->lmNum;
	return;

update_lightmap:
	// update texture
	R_BuildLightmap( surf, r_lmState.buffer, surf->lmWidth * 4 );

	if(( surf->styles[map] >= 32 || surf->styles[map] == 0 ) && surf->dlightFrame != r_framecount )
	{
		R_SetCacheState( surf );

		GL_Bind( 0, tr.lightmapTextures[surf->lmNum] );
		surf->lightmapTexnum = surf->lmNum;
	}
	else
	{
		GL_Bind( 0, tr.dlightTexture );
		surf->lightmapTexnum = DLIGHT_TEXTURE;
	}

	pglTexSubImage2D( GL_TEXTURE_2D, 0, surf->lmS, surf->lmT, surf->lmWidth, surf->lmHeight,
		r_lmState.glFormat, GL_UNSIGNED_BYTE, r_lmState.buffer );
}