/*
gl_rsurf.c - surface-related refresh code
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
#include "gl_local.h"
#include "mod_local.h"
#include "mathlib.h"

typedef struct
{
	int		allocated[BLOCK_WIDTH];
	int		current_lightmap_texture;
	msurface_t	*dynamic_surfaces;
	msurface_t	*lightmap_surfaces[MAX_LIGHTMAPS];
	byte		lightmap_buffer[BLOCK_WIDTH*BLOCK_HEIGHT*4];
} gllightmapstate_t;

static vec3_t		modelorg;       // relative to viewpoint
static vec3_t		modelmins;
static vec3_t		modelmaxs;
static byte		visbytes[MAX_MAP_LEAFS/8];
static uint		r_blocklights[BLOCK_WIDTH*BLOCK_HEIGHT*3];
static glpoly_t		*fullbright_polys[MAX_TEXTURES];
static qboolean		draw_fullbrights = false;
static gllightmapstate_t	gl_lms;
static msurface_t		*skychain = NULL;

static void LM_UploadBlock( int lightmapnum );

byte *Mod_GetCurrentVis( void )
{
	return Mod_LeafPVS( r_viewleaf, cl.worldmodel );
}

static void BoundPoly( int numverts, float *verts, vec3_t mins, vec3_t maxs )
{
	int	i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;

	for( i = 0, v = verts; i < numverts; i++ )
	{
		for( j = 0; j < 3; j++, v++ )
		{
			if( *v < mins[j] ) mins[j] = *v;
			if( *v > maxs[j] ) maxs[j] = *v;
		}
	}
}

static void SubdividePolygon_r( msurface_t *warpface, int numverts, float *verts )
{
	int	i, j, k, f, b;
	vec3_t	mins, maxs;
	float	m, frac, s, t, *v;
	vec3_t	front[SUBDIVIDE_SIZE], back[SUBDIVIDE_SIZE], total;
	float	dist[SUBDIVIDE_SIZE], total_s, total_t;
	glpoly_t	*poly;

	if( numverts > ( SUBDIVIDE_SIZE - 4 ))
		Host_Error( "Mod_SubdividePolygon: too many vertexes on face ( %i )\n", numverts );

	BoundPoly( numverts, verts, mins, maxs );

	for( i = 0; i < 3; i++ )
	{
		m = ( mins[i] + maxs[i] ) * 0.5f;
		m = SUBDIVIDE_SIZE * floor( m / SUBDIVIDE_SIZE + 0.5f );
		if( maxs[i] - m < 8 ) continue;
		if( m - mins[i] < 8 ) continue;

		// cut it
		v = verts + i;
		for( j = 0; j < numverts; j++, v += 3 )
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy( verts, v );

		f = b = 0;
		v = verts;
		for( j = 0; j < numverts; j++, v += 3 )
		{
			if( dist[j] >= 0 )
			{
				VectorCopy( v, front[f] );
				f++;
			}

			if( dist[j] <= 0 )
			{
				VectorCopy (v, back[b]);
				b++;
			}

			if( dist[j] == 0 || dist[j+1] == 0 )
				continue;

			if(( dist[j] > 0 ) != ( dist[j+1] > 0 ))
			{
				// clip point
				frac = dist[j] / ( dist[j] - dist[j+1] );
				for( k = 0; k < 3; k++ )
					front[f][k] = back[b][k] = v[k] + frac * (v[3+k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon_r( warpface, f, front[0] );
		SubdividePolygon_r( warpface, b, back[0] );
		return;
	}

	// add a point in the center to help keep warp valid
	poly = Mem_Alloc( loadmodel->mempool, sizeof( glpoly_t ) + ((numverts-4)+2) * VERTEXSIZE * sizeof( float ));
	poly->next = warpface->polys;
	poly->flags = warpface->flags;
	warpface->polys = poly;
	poly->numverts = numverts + 2;
	VectorClear( total );
	total_s = 0;
	total_t = 0;

	for( i = 0; i < numverts; i++, verts += 3 )
	{
		VectorCopy( verts, poly->verts[i+1] );
		s = DotProduct( verts, warpface->texinfo->vecs[0] );
		t = DotProduct( verts, warpface->texinfo->vecs[1] );

		total_s += s;
		total_t += t;
		VectorAdd( total, verts, total );

		poly->verts[i+1][3] = s;
		poly->verts[i+1][4] = t;
	}

	VectorScale( total, ( 1.0f / numverts ), poly->verts[0] );
	poly->verts[0][3] = total_s / numverts;
	poly->verts[0][4] = total_t / numverts;

	// copy first vertex to last
	Q_memcpy( poly->verts[i+1], poly->verts[1], sizeof( poly->verts[0] ));
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void GL_SubdivideSurface( msurface_t *fa )
{
	vec3_t	verts[SUBDIVIDE_SIZE];
	int	numverts;
	int	i, lindex;
	float	*vec;

	// convert edges back to a normal polygon
	numverts = 0;
	for( i = 0; i < fa->numedges; i++ )
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if( lindex > 0 ) vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	// do subdivide
	SubdividePolygon_r( fa, numverts, verts[0] );
}

/*
================
GL_BuildPolygonFromSurface
================
*/
void GL_BuildPolygonFromSurface( msurface_t *fa )
{
	int		i, lindex, lnumverts;
	medge_t		*pedges, *r_pedge;
	int		vertpage;
	float		*vec;
	float		s, t;
	glpoly_t		*poly;

	// already created
	if( fa->polys ) return;

	if( !fa->texinfo || !fa->texinfo->texture )
		return; // bad polygon ?

	// reconstruct the polygon
	pedges = loadmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	// draw texture
	poly = Mem_Alloc( loadmodel->mempool, sizeof( glpoly_t ) + ( lnumverts - 4 ) * VERTEXSIZE * sizeof( float ));
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for( i = 0; i < lnumverts; i++ )
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if( lindex > 0 )
		{
			r_pedge = &pedges[lindex];
			vec = loadmodel->vertexes[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = loadmodel->vertexes[r_pedge->v[1]].position;
		}

		s = DotProduct( vec, fa->texinfo->vecs[0] ) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct( vec, fa->texinfo->vecs[1] ) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy( vec, poly->verts[i] );
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		// lightmap texture coordinates
		s = DotProduct( vec, fa->texinfo->vecs[0] ) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s * LM_SAMPLE_SIZE;
		s += 8;
		s /= BLOCK_WIDTH * LM_SAMPLE_SIZE; //fa->texinfo->texture->width;

		t = DotProduct( vec, fa->texinfo->vecs[1] ) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t * LM_SAMPLE_SIZE;
		t += 8;
		t /= BLOCK_HEIGHT * LM_SAMPLE_SIZE; //fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}
}

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation( texture_t *base )
{
	int	reletive;
	int	count, speed;

	if( RI.currententity->curstate.frame )
	{
		if( base->alternate_anims )
			base = base->alternate_anims;
	}
	
	if( !base->anim_total )
		return base;

	// GoldSrc and Quake1 has different animating speed
	if( world.version == Q1BSP_VERSION )
		speed = 10;
	else speed = 20;

	reletive = (int)(cl.time * speed) % base->anim_total;

	count = 0;	
	while( base->anim_min > reletive || base->anim_max <= reletive )
	{
		base = base->anim_next;
		if( !base ) Host_Error( "R_TextureAnimation: broken loop\n" );
		if( ++count > 100 ) Host_Error( "R_TextureAnimation: infinite loop\n" );
	}
	return base;
}

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights( msurface_t *surf )
{
	float		dist, rad, minlight;
	int		lnum, s, t, sd, td, smax, tmax;
	float		sl, tl, sacc, tacc;
	vec3_t		impact, origin_l;
	mtexinfo_t	*tex;
	dlight_t		*dl;
	uint		*bl;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	tex = surf->texinfo;

	for( lnum = 0; lnum < MAX_DLIGHTS; lnum++ )
	{
		if(!( surf->dlightbits & BIT( lnum )))
			continue;	// not lit by this light

		dl = &cl_dlights[lnum];

		if( !tr.modelviewIdentity )
		{
			matrix4x4	imatrix;
	
			// transform light origin to local bmodel space
			Matrix4x4_Invert_Simple( imatrix, RI.objectMatrix );
			Matrix4x4_VectorTransform( imatrix, dl->origin, origin_l );
		}
		else VectorCopy( dl->origin, origin_l );

		rad = dl->radius;
		dist = PlaneDiff( origin_l, surf->plane );
		rad -= fabs( dist );

		// rad is now the highest intensity on the plane
		minlight = dl->minlight;
		if( rad < minlight )
			continue;

		minlight = rad - minlight;

		if( surf->plane->type < 3 )
		{
			VectorCopy( origin_l, impact );
			impact[surf->plane->type] -= dist;
		}
		else VectorMA( origin_l, -dist, surf->plane->normal, impact );

		sl = DotProduct( impact, tex->vecs[0] ) + tex->vecs[0][3] - surf->texturemins[0];
		tl = DotProduct( impact, tex->vecs[1] ) + tex->vecs[1][3] - surf->texturemins[1];

		bl = r_blocklights;
		for( t = 0, tacc = 0; t < tmax; t++, tacc += LM_SAMPLE_SIZE )
		{
			td = tl - tacc;
			if( td < 0 ) td = -td;

			for( s = 0, sacc = 0; s < smax; s++, sacc += LM_SAMPLE_SIZE, bl += 3 )
			{
				sd = sl - sacc;
				if( sd < 0 ) sd = -sd;

				if( sd > td ) dist = sd + (td >> 1);
				else dist = td + (sd >> 1);

				if( dist < minlight )
				{
					bl[0] += ( rad - dist ) * dl->color.r;
					bl[1] += ( rad - dist ) * dl->color.g;
					bl[2] += ( rad - dist ) * dl->color.b;
				}
			}
		}
	}
}

/*
================
R_SetCacheState
================
*/
void R_SetCacheState( msurface_t *surf )
{
	int	maps;

	for( maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++ )
	{
		surf->cached_light[maps] = RI.lightstylevalue[surf->styles[maps]];
	}
}

/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/
static void LM_InitBlock( void )
{
	Q_memset( gl_lms.allocated, 0, sizeof( gl_lms.allocated ));
}

static int LM_AllocBlock( int w, int h, int *x, int *y )
{
	int	i, j;
	int	best, best2;

	best = BLOCK_HEIGHT;

	for( i = 0; i < BLOCK_WIDTH - w; i++ )
	{
		best2 = 0;

		for( j = 0; j < w; j++ )
		{
			if( gl_lms.allocated[i+j] >= best )
				break;
			if( gl_lms.allocated[i+j] > best2 )
				best2 = gl_lms.allocated[i+j];
		}

		if( j == w )
		{	
			// this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if( best + h > BLOCK_HEIGHT )
		return false;

	for( i = 0; i < w; i++ )
		gl_lms.allocated[*x + i] = best + h;

	return true;
}

static void LM_UploadBlock( qboolean dynamic )
{
	int	i;

	if( dynamic )
	{
		int	height = 0;

		for( i = 0; i < BLOCK_WIDTH; i++ )
		{
			if( gl_lms.allocated[i] > height )
				height = gl_lms.allocated[i];
		}

		GL_MBind( tr.dlightTexture );

		pglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, BLOCK_WIDTH, height, GL_RGBA, GL_UNSIGNED_BYTE,
		gl_lms.lightmap_buffer );
	}
	else
	{
		rgbdata_t	r_lightmap;
		char	lmName[16];

		i = gl_lms.current_lightmap_texture;

		// upload static lightmaps only during loading
		Q_memset( &r_lightmap, 0, sizeof( r_lightmap ));
		Q_snprintf( lmName, sizeof( lmName ), "*lightmap%i", i );

		r_lightmap.width = BLOCK_WIDTH;
		r_lightmap.height = BLOCK_HEIGHT;
		r_lightmap.type = PF_RGBA_32;
		r_lightmap.size = r_lightmap.width * r_lightmap.height * 4;
		r_lightmap.flags = ( world.version == Q1BSP_VERSION ) ? 0 : IMAGE_HAS_COLOR;
		r_lightmap.buffer = gl_lms.lightmap_buffer;
		tr.lightmapTextures[i] = GL_LoadTextureInternal( lmName, &r_lightmap, TF_FONT|TF_LIGHTMAP, false );
		GL_SetTextureType( tr.lightmapTextures[i], TEX_LIGHTMAP );

		if( ++gl_lms.current_lightmap_texture == MAX_LIGHTMAPS )
			Host_Error( "AllocBlock: full\n" );
	}
}

/*
=================
R_BuildLightmap

Combine and scale multiple lightmaps into the floating
format in r_blocklights
=================
*/
static void R_BuildLightMap( msurface_t *surf, byte *dest, int stride )
{
	int	smax, tmax;
	uint	*bl, scale;
	int	i, map, size, s, t;
	color24	*lm;

	smax = ( surf->extents[0] >> 4 ) + 1;
	tmax = ( surf->extents[1] >> 4 ) + 1;
	size = smax * tmax;

	lm = surf->samples;

	Q_memset( r_blocklights, 0, sizeof( uint ) * size * 3 );

	// add all the lightmaps
	for( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255 && lm; map++ )
	{
		scale = RI.lightstylevalue[surf->styles[map]];

		for( i = 0, bl = r_blocklights; i < size; i++, bl += 3, lm++ )
		{
			bl[0] += lm->r * scale;
			bl[1] += lm->g * scale;
			bl[2] += lm->b * scale;
		}
	}

	// add all the dynamic lights
	if( surf->dlightframe == tr.framecount )
		R_AddDynamicLights( surf );

	// Put into texture format
	stride -= (smax << 2);
	bl = r_blocklights;

	for( t = 0; t < tmax; t++, dest += stride )
	{
		for( s = 0; s < smax; s++ )
		{
			dest[0] = min((bl[0] >> 7), 255 );
			dest[1] = min((bl[1] >> 7), 255 );
			dest[2] = min((bl[2] >> 7), 255 );
			dest[3] = 255;

			bl += 3;
			dest += 4;
		}
	}
}

/*
================
DrawGLPoly
================
*/
void DrawGLPoly( glpoly_t *p )
{
	float		*v;
	float		sOffset, sy;
	float		tOffset, cy;
	cl_entity_t	*e = RI.currententity;
	int		i;

	if( p->flags & SURF_CONVEYOR )
	{
		gltexture_t	*texture;
		float		flConveyorSpeed;
		float		flRate, flAngle;

		flConveyorSpeed = (e->curstate.rendercolor.g<<8|e->curstate.rendercolor.b) / 16.0f;
		if( e->curstate.rendercolor.r ) flConveyorSpeed = -flConveyorSpeed;
		texture = R_GetTexture( glState.currentTextures[glState.activeTMU] );

		flRate = abs( flConveyorSpeed ) / (float)texture->srcWidth;
		flAngle = ( flConveyorSpeed >= 0 ) ? 180 : 0;

		SinCos( flAngle * ( M_PI / 180.0f ), &sy, &cy );
		sOffset = RI.refdef.time * cy * flRate;
		tOffset = RI.refdef.time * sy * flRate;
	
		// make sure that we are positive
		if( sOffset < 0.0f ) sOffset += 1.0f + -(int)sOffset;
		if( tOffset < 0.0f ) tOffset += 1.0f + -(int)tOffset;

		// make sure that we are in a [0,1] range
		sOffset = sOffset - (int)sOffset;
		tOffset = tOffset - (int)tOffset;
	}
	else
	{
		sOffset = tOffset = 0.0f;
	}

	pglBegin( GL_POLYGON );

	v = p->verts[0];
	for( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
	{
		pglTexCoord2f( v[3] + sOffset, v[4] + tOffset );
		pglVertex3fv( v );
	}

	pglEnd();
}

/*
================
DrawGLPolyChain

Render lightmaps
================
*/
void DrawGLPolyChain( glpoly_t *p, float soffset, float toffset )
{
	qboolean	dynamic = true;

	if( soffset == 0.0f && toffset == 0.0f )
		dynamic = false;

	for( ; p != NULL; p = p->chain )
	{
		float	*v;
		int	i;

		pglBegin( GL_POLYGON );

		v = p->verts[0];
		for( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
		{
			if( !dynamic ) pglTexCoord2f( v[5], v[6] );
			else pglTexCoord2f( v[5] - soffset, v[6] - toffset );
			pglVertex3fv( v );
		}
		pglEnd ();
	}
}

/*
================
R_BlendLightmaps
================
*/
void R_BlendLightmaps( void )
{
	msurface_t	*surf, *newsurf = NULL;
	mextrasurf_t	*info;
	int		i;

	if( r_fullbright->integer || !cl.worldmodel->lightdata )
		return;

	if( RI.currententity )
	{
		// check for rendermode
		switch( RI.currententity->curstate.rendermode )
		{
		case kRenderTransTexture:
		case kRenderTransColor:
		case kRenderTransAdd:
		case kRenderGlow:
			return; // no lightmaps
		}
	}

	if( !r_lightmap->integer )
	{
		pglEnable( GL_BLEND );
		pglDepthMask( GL_FALSE );
		pglDepthFunc( GL_EQUAL );
		pglDisable( GL_ALPHA_TEST );
		pglBlendFunc( GL_ZERO, GL_SRC_COLOR );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	}

	// render static lightmaps first
	for( i = 0; i < MAX_LIGHTMAPS; i++ )
	{
		if( gl_lms.lightmap_surfaces[i] )
		{
			GL_MBind( tr.lightmapTextures[i] );

			for( surf = gl_lms.lightmap_surfaces[i]; surf != NULL; surf = surf->lightmapchain )
			{
				if( surf->polys ) DrawGLPolyChain( surf->polys, 0.0f, 0.0f );
			}
		}
	}

	// render dynamic lightmaps
	if( r_dynamic->integer )
	{
		LM_InitBlock();

		GL_MBind( tr.dlightTexture );

		newsurf = gl_lms.dynamic_surfaces;

		for( surf = gl_lms.dynamic_surfaces; surf != NULL; surf = surf->lightmapchain )
		{
			int	smax, tmax;
			byte	*base;

			smax = ( surf->extents[0] >> 4 ) + 1;
			tmax = ( surf->extents[1] >> 4 ) + 1;
			info = SURF_INFO( surf, RI.currentmodel );

			if( LM_AllocBlock( smax, tmax, &info->dlight_s, &info->dlight_t ))
			{
				base = gl_lms.lightmap_buffer;
				base += ( info->dlight_t * BLOCK_WIDTH + info->dlight_s ) * 4;

				R_BuildLightMap( surf, base, BLOCK_WIDTH * 4 );
			}
			else
			{
				msurface_t	*drawsurf;

				// upload what we have so far
				LM_UploadBlock( true );

				// draw all surfaces that use this lightmap
				for( drawsurf = newsurf; drawsurf != surf; drawsurf = drawsurf->lightmapchain )
				{
					if( drawsurf->polys )
					{
						info = SURF_INFO( drawsurf, RI.currentmodel );

						DrawGLPolyChain( drawsurf->polys,
						( drawsurf->light_s - info->dlight_s ) * ( 1.0f / 128.0f ), 
						( drawsurf->light_t - info->dlight_t ) * ( 1.0f / 128.0f ));
					}
				}

				newsurf = drawsurf;

				// clear the block
				LM_InitBlock();

				info = SURF_INFO( surf, RI.currentmodel );

				// try uploading the block now
				if( !LM_AllocBlock( smax, tmax, &info->dlight_s, &info->dlight_t ) )
					Host_Error( "AllocBlock: full\n" );

				base = gl_lms.lightmap_buffer;
				base += ( info->dlight_t * BLOCK_WIDTH + info->dlight_s ) * 4;

				R_BuildLightMap( surf, base, BLOCK_WIDTH * 4 );
			}
		}

		// draw remainder of dynamic lightmaps that haven't been uploaded yet
		if( newsurf ) LM_UploadBlock( true );

		for( surf = newsurf; surf != NULL; surf = surf->lightmapchain )
		{
			if( surf->polys )
			{
				info = SURF_INFO( surf, RI.currentmodel );

				DrawGLPolyChain( surf->polys,
				( surf->light_s - info->dlight_s ) * ( 1.0f / 128.0f ),
				( surf->light_t - info->dlight_t ) * ( 1.0f / 128.0f ));
			}
		}
	}

	if( !r_lightmap->integer )
	{
		pglDisable( GL_BLEND );
		pglDepthMask( GL_TRUE );
		pglDepthFunc( GL_LEQUAL );
		pglDisable( GL_ALPHA_TEST );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	}
}

/*
================
R_RenderFullbrights
================
*/
void R_RenderFullbrights( void )
{
	glpoly_t	*p;
	int	i;

	if( !draw_fullbrights )
		return;

	pglEnable( GL_BLEND );
	pglDepthMask( GL_FALSE );
	pglDisable( GL_ALPHA_TEST );
	pglBlendFunc( GL_ONE, GL_ONE );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	for( i = 1; i < MAX_TEXTURES; i++ )
	{
		if( !fullbright_polys[i] )
			continue;
		GL_Bind( GL_TEXTURE0, i );

		for( p = fullbright_polys[i]; p; p = p->next )
		{
			if( p->flags & SURF_DRAWTURB )
				EmitWaterPolys( p, ( p->flags & SURF_NOCULL ));
			else DrawGLPoly( p );
		}

		fullbright_polys[i] = NULL;		
	}

	pglDisable( GL_BLEND );
	pglDepthMask( GL_TRUE );
	pglDisable( GL_ALPHA_TEST );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );

	draw_fullbrights = false;
}

/*
================
R_RenderBrushPoly
================
*/
void R_RenderBrushPoly( msurface_t *fa )
{
	texture_t	*t;
	int	maps;
	qboolean	is_dynamic = false;
	
	if( RI.currententity == clgame.entities )
		r_stats.c_world_polys++;
	else r_stats.c_brush_polys++; 

	if( fa->flags & SURF_DRAWSKY )
	{	
		if( world.version == Q1BSP_VERSION )
		{
			// warp texture, no lightmaps
			EmitSkyLayers( fa );
		}
		return;
	}
		
	t = R_TextureAnimation( fa->texinfo->texture );
	GL_MBind( t->gl_texturenum );

	if( fa->flags & SURF_DRAWTURB )
	{	
		// warp texture, no lightmaps
		EmitWaterPolys( fa->polys, ( fa->flags & SURF_NOCULL ));
		return;
	}

	if( t->fb_texturenum )
	{
		// HACKHACK: store fullbrights in poly->next (only for non-water surfaces)
		fa->polys->next = fullbright_polys[t->fb_texturenum];
		fullbright_polys[t->fb_texturenum] = fa->polys;
		draw_fullbrights = true;
	}

	DrawGLPoly( fa->polys );
	DrawSurfaceDecals( fa );

	// check for lightmap modification
	for( maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++ )
	{
		if( RI.lightstylevalue[fa->styles[maps]] != fa->cached_light[maps] )
			goto dynamic;
	}

	// dynamic this frame or dynamic previously
	if(( fa->dlightframe == tr.framecount ))
	{
dynamic:
		// NOTE: at this point we have only valid textures
		if( r_dynamic->integer ) is_dynamic = true;
	}

	if( is_dynamic )
	{
		if(( fa->styles[maps] >= 32 || fa->styles[maps] == 0 ) && ( fa->dlightframe != tr.framecount ))
		{
			byte	temp[34*34*4];
			int	smax, tmax;

			smax = ( fa->extents[0] >> 4 ) + 1;
			tmax = ( fa->extents[1] >> 4 ) + 1;

			R_BuildLightMap( fa, temp, smax * 4 );
			R_SetCacheState( fa );
                              
			GL_MBind( tr.lightmapTextures[fa->lightmaptexturenum] );

			pglTexSubImage2D( GL_TEXTURE_2D, 0, fa->light_s, fa->light_t, smax, tmax,
			GL_RGBA, GL_UNSIGNED_BYTE, temp );

			fa->lightmapchain = gl_lms.lightmap_surfaces[fa->lightmaptexturenum];
			gl_lms.lightmap_surfaces[fa->lightmaptexturenum] = fa;
		}
		else
		{
			fa->lightmapchain = gl_lms.dynamic_surfaces;
			gl_lms.dynamic_surfaces = fa;
		}
	}
	else
	{
		fa->lightmapchain = gl_lms.lightmap_surfaces[fa->lightmaptexturenum];
		gl_lms.lightmap_surfaces[fa->lightmaptexturenum] = fa;
	}
}

/*
================
R_DrawTextureChains
================
*/
void R_DrawTextureChains( void )
{
	int		i;
	msurface_t	*s;
	texture_t		*t;

	// make sure what color is reset
	pglColor4ub( 255, 255, 255, 255 );
	R_LoadIdentity();	// set identity matrix

	// restore worldmodel
	RI.currententity = clgame.entities;
	RI.currentmodel = RI.currententity->model;

	// world has mirrors!
	if( RP_NORMALPASS() && tr.mirror_entities[0].chain != NULL )
	{
		tr.mirror_entities[0].ent = clgame.entities;
		tr.num_mirror_entities++;
	}

	// clip skybox surfaces
	for( s = skychain; s != NULL; s = s->texturechain )
		R_AddSkyBoxSurface( s );

	for( i = 0; i < cl.worldmodel->numtextures; i++ )
	{
		t = cl.worldmodel->textures[i];
		if( !t ) continue;

		s = t->texturechain;
		if( !s ) continue;

		if( i == tr.skytexturenum )
		{
			if( world.version == Q1BSP_VERSION )
				R_DrawSkyChain( s );
		}
		else
		{
			for( ; s != NULL; s = s->texturechain )
				R_RenderBrushPoly( s );
		}
		t->texturechain = NULL;
	}
}

/*
================
R_DrawWaterSurfaces
================
*/
void R_DrawWaterSurfaces( void )
{
	int		i;
	msurface_t	*s;
	texture_t		*t;

	if( !RI.drawWorld || RI.refdef.onlyClientDraw )
		return;

	// non-transparent water is already drawed
	if( r_wateralpha->value >= 1.0f )
		return;

	// go back to the world matrix
	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI.worldviewMatrix );

	pglEnable( GL_BLEND );
	pglDepthMask( GL_FALSE );
	pglDisable( GL_ALPHA_TEST );
	pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	pglColor4f( 1.0f, 1.0f, 1.0f, r_wateralpha->value );

	for( i = 0; i < cl.worldmodel->numtextures; i++ )
	{
		t = cl.worldmodel->textures[i];
		if( !t ) continue;

		s = t->texturechain;
		if( !s ) continue;

		if(!( s->flags & SURF_DRAWTURB ))
			continue;

		// set modulate mode explicitly
		GL_Bind( GL_TEXTURE0, t->gl_texturenum );

		for( ; s; s = s->texturechain )
			EmitWaterPolys( s->polys, ( s->flags & SURF_NOCULL ));
			
		t->texturechain = NULL;
	}

	pglDisable( GL_BLEND );
	pglDepthMask( GL_TRUE );
	pglDisable( GL_ALPHA_TEST );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	pglColor4ub( 255, 255, 255, 255 );
}

/*
=================
R_SurfaceCompare

compare translucent surfaces
=================
*/
static int R_SurfaceCompare( const msurface_t **a, const msurface_t **b )
{
	msurface_t	*surf1, *surf2;
	mextrasurf_t	*info1, *info2;
	vec3_t		vecLength, org1, org2;
	float		len1, len2;

	surf1 = (msurface_t *)*a;
	surf2 = (msurface_t *)*b;

	info1 = SURF_INFO( surf1, RI.currentmodel );
	info2 = SURF_INFO( surf2, RI.currentmodel );

	VectorAdd( RI.currententity->origin, info1->origin, org1 );
	VectorAdd( RI.currententity->origin, info2->origin, org2 );

	VectorSubtract( RI.pvsorigin, org1, vecLength );
	len1 = VectorLength( vecLength );
	VectorSubtract( RI.pvsorigin, org2, vecLength );
	len2 = VectorLength( vecLength );

	if( len1 > len2 )
		return -1;
	if( len1 < len2 )
		return 1;

	return 0;
}

static _inline qboolean R_CullSurface( msurface_t *surf, uint clipflags )
{
	mextrasurf_t	*info;

	if( !surf || !surf->texinfo || !surf->texinfo->texture )
		return true;

	if( surf->flags & SURF_WATERCSG && !( RI.currententity->curstate.effects & EF_NOWATERCSG ))
		return true;

	if( surf->flags & SURF_NOCULL )
		return false;

	if( r_nocull->integer )
		return false;

	// world surfaces can be culled by vis frame too
	if( RI.currententity == clgame.entities && surf->visframe != tr.framecount )
		return true;

	if( r_faceplanecull->integer && glState.faceCull != 0 )
	{
		if(!(surf->flags & SURF_DRAWTURB) || !RI.currentWaveHeight )
		{
			if( !VectorIsNull( surf->plane->normal ))
			{
				float	dist;

				dist = PlaneDiff( modelorg, surf->plane );

				if( glState.faceCull == GL_FRONT || ( RI.params & RP_MIRRORVIEW ))
				{
					if( surf->flags & SURF_PLANEBACK )
					{
						if( dist >= -BACKFACE_EPSILON )
							return true; // wrong side
					}
					else
					{
						if( dist <= BACKFACE_EPSILON )
							return true; // wrong side
					}
				}
				else if( glState.faceCull == GL_BACK )
				{
					if( surf->flags & SURF_PLANEBACK )
					{
						if( dist <= BACKFACE_EPSILON )
							return true; // wrong side
					}
					else
					{
						if( dist >= -BACKFACE_EPSILON )
							return true; // wrong side
					}
				}
			}
		}
	}

	info = SURF_INFO( surf, RI.currentmodel );

	return ( clipflags && R_CullBox( info->mins, info->maxs, clipflags ));
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel( cl_entity_t *e )
{
	int		i, k, num_sorted;
	qboolean		need_sort = false;
	vec3_t		mins, maxs;
	msurface_t	*psurf;
	model_t		*clmodel;
	qboolean		rotated;
	dlight_t		*l;

	clmodel = e->model;
	RI.currentWaveHeight = RI.currententity->curstate.scale * 32.0f;

	if( !VectorIsNull( e->angles ))
	{
		for( i = 0; i < 3; i++ )
		{
			mins[i] = e->origin[i] - clmodel->radius;
			maxs[i] = e->origin[i] + clmodel->radius;
		}
		rotated = true;
	}
	else
	{
		VectorAdd( e->origin, clmodel->mins, mins );
		VectorAdd( e->origin, clmodel->maxs, maxs );
		rotated = false;
	}

	if( R_CullBox( mins, maxs, RI.clipFlags ))
		return;

	Q_memset( gl_lms.lightmap_surfaces, 0, sizeof( gl_lms.lightmap_surfaces ));
	gl_lms.dynamic_surfaces = NULL;

	if( rotated ) R_RotateForEntity( e );
	else R_TranslateForEntity( e );

	VectorSubtract( RI.cullorigin, e->origin, modelorg );
	e->visframe = tr.framecount; // visible

	if( rotated )
	{
		vec3_t	temp;
		VectorCopy( modelorg, temp );
		Matrix4x4_VectorITransform( RI.objectMatrix, temp, modelorg );
	}

	// calculate dynamic lighting for bmodel if it's not an
	// instanced model
	if( clmodel->firstmodelsurface != 0 )
	{
		vec3_t	origin_l, oldorigin;
		matrix4x4	imatrix;

		for( k = 0, l = cl_dlights; k < MAX_DLIGHTS; k++, l++ )
		{
			if( l->die < cl.time || !l->radius )
				continue;

			VectorCopy( l->origin, oldorigin ); // save oldorigin
			Matrix4x4_Invert_Simple( imatrix, RI.objectMatrix );
			Matrix4x4_VectorTransform( imatrix, l->origin, origin_l );
			VectorCopy( origin_l, l->origin ); // move light in bmodel space
			R_MarkLights( l, 1<<k, clmodel->nodes + clmodel->hulls[0].firstclipnode );
			VectorCopy( oldorigin, l->origin ); // restore lightorigin
		}
	}

	// setup the rendermode
	GL_SetRenderMode( e->curstate.rendermode );

	// setup the color and alpha
	switch( e->curstate.rendermode )
	{
	case kRenderTransAdd:
	case kRenderTransTexture:
		need_sort = true;
	case kRenderGlow:
		pglColor4ub( 255, 255, 255, e->curstate.renderamt );
		break;
	case kRenderTransColor:
		pglDisable( GL_TEXTURE_2D );
		pglColor4ub( e->curstate.rendercolor.r, e->curstate.rendercolor.g,
			e->curstate.rendercolor.b, e->curstate.renderamt );
		break;
	case kRenderTransAlpha:
		// NOTE: brushes can't change renderamt for 'Solid' mode
		pglAlphaFunc( GL_GEQUAL, 0.5f );
	default:	
		pglColor4ub( 255, 255, 255, 255 );
		break;
	}

	num_sorted = 0;

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for( i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
	{
		if( R_CullSurface( psurf, 0 ))
			continue;

		if( RP_NORMALPASS() && psurf->flags & SURF_MIRROR )
		{
			psurf->texturechain = tr.mirror_entities[tr.num_mirror_entities].chain;
			tr.mirror_entities[tr.num_mirror_entities].chain = psurf;
		}
		else if( need_sort )
		{
			world.draw_surfaces[num_sorted] = psurf;
			num_sorted++;
			ASSERT( world.max_surfaces >= num_sorted );
		}
		else
		{
			// render unsorted (solid)
			R_RenderBrushPoly( psurf );
		}
	}

	// store new mirror entity
	if( RP_NORMALPASS() && tr.mirror_entities[tr.num_mirror_entities].chain != NULL )
	{
		tr.mirror_entities[tr.num_mirror_entities].ent = RI.currententity;
		tr.num_mirror_entities++;
	}

	if( need_sort )
		qsort( world.draw_surfaces, num_sorted, sizeof( msurface_t* ), R_SurfaceCompare );

	// draw sorted translucent surfaces
	for( i = 0; i < num_sorted; i++ )
		R_RenderBrushPoly( world.draw_surfaces[i] );

	if( e->curstate.rendermode == kRenderTransColor )
		pglEnable( GL_TEXTURE_2D );

	R_BlendLightmaps();
	R_RenderFullbrights();
	R_LoadIdentity();	// restore worldmatrix
}

/*
=================
R_DrawStaticModel

Merge static model brushes with world surfaces
=================
*/
void R_DrawStaticModel( cl_entity_t *e )
{
	int		i, k;
	model_t		*clmodel;
	msurface_t	*psurf;
	dlight_t		*l;
	
	clmodel = e->model;
	if( R_CullBox( clmodel->mins, clmodel->maxs, RI.clipFlags ))
		return;

	// calculate dynamic lighting for bmodel if it's not an
	// instanced model
	if( clmodel->firstmodelsurface != 0 )
	{
		for( k = 0, l = cl_dlights; k < MAX_DLIGHTS; k++, l++ )
		{
			if( l->die < cl.time || !l->radius )
				continue;
			R_MarkLights( l, 1<<k, clmodel->nodes + clmodel->hulls[0].firstclipnode );
		}
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for( i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
	{
		if( R_CullSurface( psurf, 0 ))
			continue;

		if( RP_NORMALPASS() && psurf->flags & SURF_MIRROR )
		{
			psurf->texturechain = tr.mirror_entities[0].chain;
			tr.mirror_entities[0].chain = psurf;
		}
		else
		{
			psurf->texturechain = psurf->texinfo->texture->texturechain;
			psurf->texinfo->texture->texturechain = psurf;
		}
	}
}

/*
=================
R_DrawStaticBrushes

Insert static brushes into world texture chains
=================
*/
void R_DrawStaticBrushes( void )
{
	int	i;

	// draw static entities
	for( i = 0; i < tr.num_static_entities; i++ )
	{
		if( RI.refdef.onlyClientDraw )
			break;

		RI.currententity = tr.static_entities[i];
		RI.currentmodel = RI.currententity->model;
	
		ASSERT( RI.currententity != NULL );
		ASSERT( RI.currententity->model != NULL );

		switch( RI.currententity->model->type )
		{
		case mod_brush:
			R_DrawStaticModel( RI.currententity );
			break;
		default:
			Host_Error( "R_DrawStatics: non bsp model in static list!\n" );
			break;
		}
	}
}

/*
=============================================================

	MIRROR RENDERING

=============================================================
*/
void R_PlaneForMirror( msurface_t *surf, mplane_t *out )
{
	cl_entity_t	*ent;

	ASSERT( out != NULL );

	ent = RI.currententity;

	// setup mirror plane
	*out = *surf->plane;

	if( surf->flags & SURF_PLANEBACK )
	{
		VectorNegate( out->normal, out->normal );
	}

	if( !VectorIsNull( ent->angles ))
		R_RotateForEntity( ent );
	else R_TranslateForEntity( ent );

	// tranform mirror plane by entity matrix
	if( !tr.modelviewIdentity )
	{
		mplane_t	tmp;

		tmp = *out;

		Matrix4x4_TransformPositivePlane( RI.objectMatrix, tmp.normal, tmp.dist, out->normal, &out->dist );
	}
}

void R_DrawMirrors( void )
{
	ref_instance_t	oldRI;
	mplane_t		plane;
	msurface_t	*surf, *mirrorchain;
	vec3_t		forward, right, up;
	vec3_t		origin, angles;
	int		i;
	float		d;

	if( !tr.num_mirror_entities ) return; // mo mirrors for this frame

	oldRI = RI; // make refinst backup

	for( i = 0; i < tr.num_mirror_entities; i++ )
	{
		mirrorchain = tr.mirror_entities[i].chain;

		for( surf = mirrorchain; surf != NULL; surf = surf->texturechain )
		{
			RI.currententity = tr.mirror_entities[i].ent;
			RI.currentmodel = RI.currententity->model;

			ASSERT( RI.currententity != NULL );
			ASSERT( RI.currentmodel != NULL );

			R_PlaneForMirror( surf, &plane );

			d = -2.0f * ( DotProduct( RI.vieworg, plane.normal ) - plane.dist );
			VectorMA( RI.vieworg, d, plane.normal, origin );

			d = -2.0f * DotProduct( RI.vforward, plane.normal );
			VectorMA( RI.vforward, d, plane.normal, forward );
			VectorNormalize( forward );

			d = -2.0f * DotProduct( RI.vright, plane.normal );
			VectorMA( RI.vright, d, plane.normal, right );
			VectorNormalize( right );

			d = -2.0f * DotProduct( RI.vup, plane.normal );
			VectorMA( RI.vup, d, plane.normal, up );
			VectorNormalize( up );

			VectorsAngles( forward, right, up, angles );
			angles[ROLL] = -angles[ROLL];

			RI.params = RP_MIRRORVIEW|RP_FLIPFRONTFACE|RP_CLIPPLANE;
			if( r_viewleaf ) RI.params |= RP_OLDVIEWLEAF;

			RI.clipPlane = plane;
			RI.clipFlags |= ( 1<<5 );

			RI.frustum[5] = plane;
			RI.frustum[5].signbits = SignbitsForPlane( RI.frustum[5].normal );
			RI.frustum[5].type = PLANE_NONAXIAL;

			RI.refdef.viewangles[0] = anglemod( angles[0] );
			RI.refdef.viewangles[1] = anglemod( angles[1] );
			RI.refdef.viewangles[2] = anglemod( angles[2] );
			VectorCopy( origin, RI.refdef.vieworg );
			VectorCopy( origin, RI.pvsorigin );
			VectorCopy( origin, RI.cullorigin );

			R_RenderScene( &RI.refdef );

			if( !( RI.params & RP_OLDVIEWLEAF ))
				r_oldviewleaf = r_viewleaf = NULL; // force markleafs next frame

			RI = oldRI; // restore ref instance

			// FIXME: draw mirror surface here
		}

		tr.mirror_entities[i].chain = NULL; // done
		tr.mirror_entities[i].ent = NULL;
	}

	tr.num_mirror_entities = 0;
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/
/*
================
R_RecursiveWorldNode
================
*/
void R_RecursiveWorldNode( mnode_t *node, uint clipflags )
{
	const mplane_t	*clipplane;
	int		i, clipped;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	int		c, side;
	float		dot;

	if( node->contents == CONTENTS_SOLID )
		return; // hit a solid leaf

	if( node->visframe != tr.visframecount )
		return;

	if( clipflags )
	{
		for( i = 0, clipplane = RI.frustum; i < 6; i++, clipplane++ )
		{
			if(!( clipflags & ( 1<<i )))
				continue;

			clipped = BoxOnPlaneSide( node->minmaxs, node->minmaxs + 3, clipplane );
			if( clipped == 2 ) return;
			if( clipped == 1 ) clipflags &= ~(1<<i);
		}
	}

	// if a leaf node, draw stuff
	if( node->contents < 0 )
	{
		pleaf = (mleaf_t *)node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if( c )
		{
			do
			{
				(*mark)->visframe = tr.framecount;
				mark++;
			} while( --c );
		}

		// deal with model fragments in this leaf
		if( pleaf->efrags )
			R_StoreEfrags( &pleaf->efrags );

		r_stats.c_world_leafs++;
		return;
	}

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	dot = PlaneDiff( modelorg, node->plane );
	side = (dot >= 0) ? 0 : 1;

	// recurse down the children, front side first
	R_RecursiveWorldNode( node->children[side], clipflags );

	// draw stuff
	for( c = node->numsurfaces, surf = cl.worldmodel->surfaces + node->firstsurface; c; c--, surf++ )
	{
		if( R_CullSurface( surf, clipflags ))
			continue;

		if( surf->flags & SURF_DRAWSKY && world.version == HLBSP_VERSION )
		{
			// make sky chain to right clip the skybox
			surf->texturechain = skychain;
			skychain = surf;
		}
		else if( RP_NORMALPASS() && surf->flags & SURF_MIRROR )
		{
			surf->texturechain = tr.mirror_entities[0].chain;
			tr.mirror_entities[0].chain = surf;
		}
		else
		{ 
			surf->texturechain = surf->texinfo->texture->texturechain;
			surf->texinfo->texture->texturechain = surf;
		}
	}

	// recurse down the back side
	R_RecursiveWorldNode( node->children[!side], clipflags );
}

/*
=============
R_DrawTriangleOutlines
=============
*/
void R_DrawTriangleOutlines( void )
{
	int	i, j;
	glpoly_t	*p;

	if( !gl_wireframe->integer )
		return;

	pglDisable( GL_TEXTURE_2D );
	pglDisable( GL_DEPTH_TEST );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

	for( i = 0; i < MAX_LIGHTMAPS; i++ )
	{
		msurface_t	*surf;
		float		*v;

		for( surf = gl_lms.lightmap_surfaces[i]; surf != NULL; surf = surf->lightmapchain )
		{
			p = surf->polys;
			for( ; p != NULL; p = p->chain )
			{
				pglBegin( GL_POLYGON );
				v = p->verts[0];
				for( j = 0; j < p->numverts; j++, v += VERTEXSIZE )
					pglVertex3fv( v );
				pglEnd ();
			}
		}
	}

	pglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	pglEnable( GL_DEPTH_TEST );
	pglEnable( GL_TEXTURE_2D );
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld( void )
{
	if( !RI.drawWorld || RI.refdef.onlyClientDraw )
		return;

	RI.currententity = clgame.entities;
	RI.currentmodel = RI.currententity->model;

	VectorCopy( RI.cullorigin, modelorg );
	Q_memset( gl_lms.lightmap_surfaces, 0, sizeof( gl_lms.lightmap_surfaces ));
	Q_memset( fullbright_polys, 0, sizeof( fullbright_polys ));
	RI.currentWaveHeight = RI.waveHeight;
	GL_SetRenderMode( kRenderNormal );
	gl_lms.dynamic_surfaces = NULL;

	R_ClearSkyBox ();

	// draw the world fog
	R_DrawFog ();

	R_RecursiveWorldNode( cl.worldmodel->nodes, RI.clipFlags );

	R_DrawStaticBrushes();
	R_DrawTextureChains();

	R_BlendLightmaps();
	R_RenderFullbrights();

	if( skychain )
		R_DrawSkyBox();
	skychain = NULL;

	R_DrawTriangleOutlines ();
}

/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current leaf
===============
*/
void R_MarkLeaves( void )
{
	byte	*vis;
	mnode_t	*node;
	int	i;

	if( !RI.drawWorld ) return;
	if( r_viewleaf == r_oldviewleaf && r_viewleaf2 == r_oldviewleaf2 && !r_novis->integer && r_viewleaf != NULL )
		return;

	// development aid to let you run around
	// and see exactly where the pvs ends
	if( r_lockpvs->integer )
		return;

	tr.visframecount++;
	r_oldviewleaf = r_viewleaf;
	r_oldviewleaf2 = r_viewleaf2;
			
	if( r_novis->integer || r_viewleaf == NULL || !cl.worldmodel->visdata )
	{
		// force to get full visibility
		vis = Mod_LeafPVS( NULL, NULL );
	}
	else
	{
		// may have to combine two clusters
		// because of solid water boundaries
		vis = Mod_LeafPVS( r_viewleaf, cl.worldmodel );

		if( r_viewleaf != r_viewleaf2 )
		{
			int	longs = ( cl.worldmodel->numleafs + 31 ) >> 5;

			Q_memcpy( visbytes, vis, longs << 2 );
			vis = Mod_LeafPVS( r_viewleaf2, cl.worldmodel );

			for( i = 0; i < longs; i++ )
				((int *)visbytes)[i] |= ((int *)vis)[i];

			vis = visbytes;
		}
	}

	for( i = 0; i < cl.worldmodel->numleafs; i++ )
	{
		if( vis[i>>3] & ( 1<<( i & 7 )))
		{
			node = (mnode_t *)&cl.worldmodel->leafs[i+1];
			do
			{
				if( node->visframe == tr.visframecount )
					break;
				node->visframe = tr.visframecount;
				node = node->parent;
			} while( node );
		}
	}
}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap( msurface_t *surf )
{
	int	smax, tmax;
	byte	*base;

	if( !cl.worldmodel->lightdata ) return;
	if( surf->flags & SURF_DRAWTILED )
		return;

	smax = ( surf->extents[0] >> 4 ) + 1;
	tmax = ( surf->extents[1] >> 4 ) + 1;

	if( !LM_AllocBlock( smax, tmax, &surf->light_s, &surf->light_t ))
	{
		LM_UploadBlock( false );
		LM_InitBlock();

		if( !LM_AllocBlock( smax, tmax, &surf->light_s, &surf->light_t ))
			Host_Error( "AllocBlock: full\n" );
	}

	surf->lightmaptexturenum = gl_lms.current_lightmap_texture;

	base = gl_lms.lightmap_buffer;
	base += ( surf->light_t * BLOCK_WIDTH + surf->light_s ) * 4;

	R_SetCacheState( surf );
	R_BuildLightMap( surf, base, BLOCK_WIDTH * 4 );
}

/*
==================
GL_BuildLightmaps

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void GL_BuildLightmaps( void )
{
	int	i, j;
	model_t	*m;

	// release old lightmaps
	for( i = 0; i < MAX_LIGHTMAPS; i++ )
	{
		if( !tr.lightmapTextures[i] ) break;
		GL_FreeTexture( tr.lightmapTextures[i] );
	}

	Q_memset( tr.lightmapTextures, 0, sizeof( tr.lightmapTextures ));
	Q_memset( tr.mirror_entities, 0, sizeof( tr.mirror_entities ));
	Q_memset( visbytes, 0x00, sizeof( visbytes ));
	
	skychain = NULL;

	tr.framecount = 1;	// no dlight cache
	gl_lms.current_lightmap_texture = 0;

	// setup all the lightstyles
	R_AnimateLight();

	LM_InitBlock();	

	for( i = 1; i < MAX_MODELS; i++ )
	{
		if(( m = Mod_Handle( i )) == NULL )
			continue;

		if( m->name[0] == '*' || m->type != mod_brush )
			continue;

		loadmodel = m;

		for( j = 0; j < m->numsurfaces; j++ )
		{
			// clearing all decal chains
			m->surfaces[j].pdecals = NULL;

			GL_CreateSurfaceLightmap( m->surfaces + j );

			if( m->surfaces[i].flags & SURF_DRAWTURB )
				continue;

			if( m->surfaces[i].flags & SURF_DRAWSKY && world.version == Q1BSP_VERSION )
				continue;

			GL_BuildPolygonFromSurface( m->surfaces + j );
		}
	}

	loadmodel = NULL;

	LM_UploadBlock( false );
}