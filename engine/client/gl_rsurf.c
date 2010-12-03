//=======================================================================
//			Copyright XashXT Group 2010 ©
//		    gl_rsurf.c - surface-related refresh code
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"
#include "cm_local.h"

#define LIGHTMAP_BYTES	4
#define BLOCK_WIDTH		128
#define BLOCK_HEIGHT	128
#define MAX_LIGHTMAP_SIZE	4096
#define SUBDIVIDE_SIZE	64

typedef struct
{
	int		currentNum;
	uint		blocklights[MAX_LIGHTMAP_SIZE*3];
	glpoly_t		*lightmap_polys[MAX_LIGHTMAPS];
	qboolean		lightmap_modified[MAX_LIGHTMAPS];
	wrect_t		lightmap_rectchange[MAX_LIGHTMAPS];
	int		allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];
	byte		lightmaps[MAX_LIGHTMAPS*BLOCK_WIDTH*BLOCK_HEIGHT*3];
} gllightmapstate_t;

static gllightmapstate_t	r_lmState;

byte *Mod_GetCurrentVis( void )
{
//	return Mod_LeafPVS( r_viewleaf, r_worldmodel );
	return NULL;
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
	Mem_Copy( poly->verts[i+1], poly->verts[1], sizeof( poly->verts[0] ));
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
	vec3_t		total;

	// reconstruct the polygon
	pedges = loadmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	VectorClear( total );

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

		VectorAdd( total, vec, total );
		VectorCopy( vec, poly->verts[i] );
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		// lightmap texture coordinates
		s = DotProduct( vec, fa->texinfo->vecs[0] ) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s * 16;
		s += 8;
		s /= BLOCK_WIDTH * 16; //fa->texinfo->texture->width;

		t = DotProduct( vec, fa->texinfo->vecs[1] ) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t * 16;
		t += 8;
		t /= BLOCK_HEIGHT * 16; //fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}
	poly->numverts = lnumverts;
}

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights( msurface_t *surf )
{
	int		lnum, sd, td, s, t;
	float		dist, rad, minlight;
	vec3_t		impact, local;
	int		smax, tmax;
	mtexinfo_t	*tex;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	for( lnum = 0; lnum < MAX_DLIGHTS; lnum++ )
	{
		if(!( surf->dlightbits & ( 1<<lnum )))
			continue;	// not lit by this light

		rad = cl_dlights[lnum].radius;
		dist = DotProduct( cl_dlights[lnum].origin, surf->plane->normal ) - surf->plane->dist;
		rad -= fabs( dist );
		minlight = cl_dlights[lnum].minlight;
		if( rad < minlight ) continue;

		minlight = rad - minlight;

		VectorMA( cl_dlights[lnum].origin, -dist, surf->plane->normal, impact );
		local[0] = DotProduct( impact, tex->vecs[0] ) + tex->vecs[0][3];
		local[1] = DotProduct( impact, tex->vecs[1] ) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];
		
		for( t = 0; t < tmax; t++ )
		{
			td = local[1] - t * 16;
			if( td < 0 ) td = -td;

			for( s = 0; s < smax; s++ )
			{
				sd = local[0] - s * 16;
				if( sd < 0 ) sd = -sd;
				if( sd > td ) dist = sd + (td >> 1);
				else dist = td + (sd >> 1);
				if( dist < minlight )
					r_lmState.blocklights[t * smax + s] += (rad - dist) * 256;
			}
		}
	}
}

/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/
static void LM_InitBlock( void )
{
	Mem_Set( r_lmState.allocated, 0, sizeof( r_lmState.allocated ));
}

static int LM_AllocBlock( int w, int h, int *x, int *y )
{
	int	i, j, best, best2, texnum;

	for( texnum = 0; texnum < MAX_LIGHTMAPS; texnum++ )
	{
		best = BLOCK_HEIGHT;

		for( i = 0; i < BLOCK_WIDTH - w; i++ )
		{
			best2 = 0;

			for( j = 0; j < w; j++ )
			{
				if( r_lmState.allocated[texnum][i+j] >= best )
					break;
				if( r_lmState.allocated[texnum][i+j] > best2 )
					best2 = r_lmState.allocated[texnum][i+j];
			}
			if( j == w )
			{	
				// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if( best + h > BLOCK_HEIGHT )
			continue;

		for( i = 0; i < w; i++ )
			r_lmState.allocated[texnum][*x+i] = best + h;

		return texnum;
	}

	Host_Error( "AllocBlock: full\n" );

	return 0;
}

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
static void R_BuildLightMap( msurface_t *surf, byte *dest, int stride )
{
	int	smax, tmax, t, i, j;
	int	size, maps, blocksize;
	byte	*lightmap;
	uint	scale, *bl;

	surf->cached_dlight = ( surf->dlightframe == tr.framecount );

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;
	blocksize = size * 3;
	lightmap = (byte *)surf->samples;

	// set to full bright if no light data
	if( r_fullbright->integer || !cl.worldmodel->lightdata )
	{
		for( i = 0; i < blocksize; i++ )
			r_lmState.blocklights[i] = 255 * 256;
		goto store_lightmap;
	}

	// clear to no light
	Mem_Set( r_lmState.blocklights, 0, blocksize * sizeof( int ));

	// add all the lightmaps
	if( lightmap )
	{
		for( maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++ )
		{
			scale = r_lightstyles[surf->styles[maps]].rgb[0];	// FIXME!!!
			surf->cached_light[maps] = scale;	// 8.8 fraction
			bl = r_lmState.blocklights;

			for( i = 0; i < blocksize; i++ )
				*bl++ += lightmap[i] * scale;
			lightmap += blocksize; // skip to next lightmap
		}
	}

	// add all the dynamic lights
	if( surf->cached_dlight )
		R_AddDynamicLights( surf );

// bound, invert, and shift
store_lightmap:
	bl = r_lmState.blocklights;
	stride -= smax * 3;

	for( i = 0; i < tmax; i++, dest += stride )
	{
		for( j = smax; j != 0; j-- )
		{
			t = bl[0];
			t = t >> 7;
			t = min( t, 255 );
			dest[0] = 255 - t;

			t = bl[1];
			t = t >> 7;
			t = min( t, 255 );
			dest[1] = 255 - t;

			t = bl[2];
			t = t >> 7;
			t = min( t, 255 );
			dest[2] = 255 - t;

			bl += 3;
			dest += 3;
		}
	}
}

static void LM_UploadBlock( int lightmapnum )
{
	wrect_t	*rect;

	r_lmState.lightmap_modified[lightmapnum] = false;
	rect = &r_lmState.lightmap_rectchange[lightmapnum];

	pglTexSubImage2D( GL_TEXTURE_2D, 0, 0, rect->right, BLOCK_WIDTH, rect->bottom, GL_RGB, GL_UNSIGNED_BYTE,
	&r_lmState.lightmaps[(lightmapnum * BLOCK_HEIGHT + rect->right) * BLOCK_WIDTH * 3] );

	// reset rectangle
	rect->left = BLOCK_WIDTH;
	rect->right = BLOCK_HEIGHT;
	rect->top = 0;
	rect->bottom = 0;
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

	if( surf->flags & ( SURF_DRAWSKY|SURF_DRAWTURB ))
		return;

	smax = ( surf->extents[0] >> 4 ) + 1;
	tmax = ( surf->extents[1] >> 4 ) + 1;

	if( smax > BLOCK_WIDTH )
		Host_Error( "GL_CreateSurfaceLightmap: lightmap width %d > %d\n", smax, BLOCK_WIDTH );
	if( tmax > BLOCK_HEIGHT )
		Host_Error( "GL_CreateSurfaceLightmap: lightmap height %d > %d\n", tmax, BLOCK_HEIGHT );
	if( smax * tmax > MAX_LIGHTMAP_SIZE )
		Host_Error( "GL_CreateSurfaceLightmap: lightmap size %d > %d\n", smax * tmax, MAX_LIGHTMAP_SIZE );

	surf->lightmaptexturenum = LM_AllocBlock( smax, tmax, &surf->light_s, &surf->light_t );
	base = &r_lmState.lightmaps[surf->lightmaptexturenum * BLOCK_WIDTH * BLOCK_HEIGHT * 3];
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * 3;
	r_numdlights = 0;

	R_BuildLightMap( surf, base, BLOCK_WIDTH * 3 );
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
	rgbdata_t	r_lightmap;
	char	lmName[16];
	model_t	*m;
	
	// setup the base lightstyles so the lightmaps won't have to be regenerated
	// the first time they're seen
	for( i = 0; i < MAX_LIGHTSTYLES; i++ )
	{
		r_lightstyles[i].rgb[0] = 1.0f;
		r_lightstyles[i].rgb[1] = 1.0f;
		r_lightstyles[i].rgb[2] = 1.0f;
	}

	// release old lightmaps
	for( i = 0; i < r_lmState.currentNum; i++ )
	{
		if( !tr.lightmapTextures[i] ) continue;
		GL_FreeTexture( tr.lightmapTextures[i] );
	}

	tr.framecount = 1;		// no dlightcache
	r_lmState.currentNum = 0;
	Mem_Set( tr.lightmapTextures, 0, sizeof( tr.lightmapTextures ));

	LM_InitBlock();	

	for( i = 1; i < MAX_MODELS; i++ )
	{
		if(( m = CM_ClipHandleToModel( i )) == NULL )
			break;

		if( m->name[0] == '*' || m->type != mod_brush )
			continue;

		for( j = 0; j < m->numsurfaces; j++ )
		{
			GL_CreateSurfaceLightmap( m->surfaces + j );
		}
	}

	// upload all lightmaps that were filled
	for( i = 0; i < MAX_LIGHTMAPS; i++ )
	{
		if( !r_lmState.allocated[i][0] ) break; // no more used
		r_lmState.lightmap_modified[i] = false;
		r_lmState.lightmap_rectchange[i].left = BLOCK_WIDTH;
		r_lmState.lightmap_rectchange[i].right = BLOCK_HEIGHT;
		r_lmState.lightmap_rectchange[i].top = 0;
		r_lmState.lightmap_rectchange[i].bottom = 0;

		Mem_Set( &r_lightmap, 0, sizeof( r_lightmap ));

		com.snprintf( lmName, sizeof( lmName ), "*lightmap%i", i );
		r_lightmap.width = BLOCK_WIDTH;
		r_lightmap.height = BLOCK_HEIGHT;
		r_lightmap.type = PF_RGB_24;
		r_lightmap.size = r_lightmap.width * r_lightmap.height * 3;
		r_lightmap.flags = IMAGE_HAS_COLOR;	// FIXME: detecting grayscale lightmaps for quake1
		r_lightmap.buffer = (byte *)&r_lmState.lightmaps[r_lightmap.size*i];
		tr.lightmapTextures[i] = GL_LoadTextureInternal( lmName, &r_lightmap, TF_LIGHTMAP, false );
		r_lmState.currentNum++;
	}
}