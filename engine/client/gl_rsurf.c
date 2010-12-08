//=======================================================================
//			Copyright XashXT Group 2010 ©
//		    gl_rsurf.c - surface-related refresh code
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"
#include "cm_local.h"

#define BLOCK_WIDTH		256
#define BLOCK_HEIGHT	256

typedef struct
{
	glpoly_t		*lightmap_polys[MAX_LIGHTMAPS];
	qboolean		lightmap_modified[MAX_LIGHTMAPS];
	wrect_t		lightmap_rectchange[MAX_LIGHTMAPS];
	int		allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];
	byte		lightmaps[MAX_LIGHTMAPS*BLOCK_WIDTH*BLOCK_HEIGHT*4];
} lightmap_state_t;

static vec3_t		modelorg;       // relative to viewpoint
static vec3_t		modelmins;
static vec3_t		modelmaxs;
static byte		visbytes[MAX_MAP_LEAFS/8];
static vec3_t		r_blockLights[BLOCK_WIDTH*BLOCK_HEIGHT];
static lightmap_state_t	r_lmState;
static msurface_t		*skychain = NULL;
static msurface_t		*waterchain = NULL;

static void R_BuildLightMap( msurface_t *surf, byte *dest, int stride );
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

	// already created
	if( fa->polys ) return;

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
	if( ws.version == Q1BSP_VERSION )
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
================
DrawGLWaterPoly

Warp the vertex coordinates
================
*/
void DrawGLWaterPoly( glpoly_t *p, qboolean lightmap )
{
	float	*v;
	vec3_t	nv;
	int	i;

	GL_CleanUpTextureUnits( 1 );
	pglBegin( GL_TRIANGLE_FAN );

	v = p->verts[0];
	for( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
	{
		if( lightmap )
			pglTexCoord2f( v[5], v[6] );
		else pglTexCoord2f( v[3], v[4] );

		nv[0] = v[0] + 8 * com.sin( v[1] * 0.05f + cl.time ) * com.sin( v[2] * 0.05f + cl.time );
		nv[1] = v[1] + 8 * com.sin( v[0] * 0.05f + cl.time ) * com.sin( v[2] * 0.05f + cl.time );
		nv[2] = v[2];

		pglVertex3fv( nv );
	}

	pglEnd();
}

/*
================
DrawGLPoly
================
*/
void DrawGLPoly( glpoly_t *p )
{
	float	*v;
	int	i;

	pglBegin( GL_POLYGON );

	v = p->verts[0];
	for( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
	{
		pglTexCoord2f( v[3], v[4] );
		pglVertex3fv( v );
	}

	pglEnd();
}

/*
================
R_BlendLightmaps
================
*/
void R_BlendLightmaps( void )
{
	int	i, j;
	glpoly_t	*p;
	float	*v;

	if( r_fullbright->integer )
		return;
	if( !gl_texsort->integer )
		return;

	if( !r_lightmap->integer )
	{
		GL_TexEnv( GL_MODULATE );
		GL_SetState( GLSTATE_SRCBLEND_ZERO|GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR );
	}

	for( i = 0; i < MAX_LIGHTMAPS; i++ )
	{
		p = r_lmState.lightmap_polys[i];
		if( !p ) continue;

		GL_Bind( GL_TEXTURE0, tr.lightmapTextures[i] );
		if( r_lmState.lightmap_modified[i] )
			LM_UploadBlock( i );

		for( ; p; p = p->chain )
		{
			if( p->flags & SURF_UNDERWATER )
			{
				DrawGLWaterPoly( p, true );
			}
			else
			{
				pglBegin( GL_POLYGON );

				v = p->verts[0];
				for( j = 0; j < p->numverts; j++, v += VERTEXSIZE )
				{
					pglTexCoord2f( v[5], v[6] );
					pglVertex3fv( v );
				}

				pglEnd();
			}
		}
	}

	GL_TexEnv( GL_REPLACE );
	GL_SetState( GLSTATE_DEPTHWRITE );
}

/*
================
R_RenderDynamicLightmaps
================
*/
void R_RenderDynamicLightmaps( msurface_t *fa )
{
	byte	*base;
	int	maps;
	wrect_t	*rect;
	int	smax, tmax;
	float	scale;

	if( fa->flags & ( SURF_DRAWSKY|SURF_DRAWTURB ))
		return;
		
	fa->polys->chain = r_lmState.lightmap_polys[fa->lightmaptexturenum];
	r_lmState.lightmap_polys[fa->lightmaptexturenum] = fa->polys;

	// check for lightmap modification
	for( maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++ )
	{
		scale = cl.lightstyles[fa->styles[maps]].value * r_lighting_modulate->value;
		if( scale != fa->cached_light[maps] )
			goto dynamic; // lightstyle or r_lighting_modulate changed
	}

	if( fa->dlightframe == tr.framecount || fa->cached_dlight )
	{
dynamic:
		if( r_dynamic->integer )
		{
			r_lmState.lightmap_modified[fa->lightmaptexturenum] = true;
			rect = &r_lmState.lightmap_rectchange[fa->lightmaptexturenum];

			if( fa->light_t < rect->right )
			{
				if( rect->bottom )
					rect->bottom += rect->right - fa->light_t;
				rect->right = fa->light_t;
			}

			if( fa->light_s < rect->left )
			{
				if( rect->top )
					rect->top += rect->left - fa->light_s;
				rect->left = fa->light_s;
			}

			smax = ( fa->extents[0] >> 4 ) + 1;
			tmax = ( fa->extents[1] >> 4 ) + 1;
			if(( rect->top + rect->left ) < ( fa->light_s + smax ))
				rect->top = ( fa->light_s - rect->left ) + smax;
			if(( rect->bottom + rect->right ) < ( fa->light_t + tmax ))
				rect->bottom = ( fa->light_t - rect->right ) + tmax;
			base = r_lmState.lightmaps + fa->lightmaptexturenum * BLOCK_WIDTH * BLOCK_HEIGHT * 4;
			base += fa->light_t * BLOCK_WIDTH * 4 + fa->light_s * 4;
			R_BuildLightMap( fa, base, BLOCK_WIDTH * 4 );
		}
	}
}

/*
================
R_RenderBrushPoly
================
*/
void R_RenderBrushPoly( msurface_t *fa )
{
	texture_t	*t;

	r_stats.c_brush_polys++;

	if( fa->flags & SURF_DRAWSKY )
	{	
		// warp texture, no lightmaps
		EmitSkyLayers( fa );
		return;
	}
		
	t = R_TextureAnimation( fa->texinfo->texture );
	GL_Bind( GL_TEXTURE0, t->gl_texturenum );

	if( fa->flags & SURF_DRAWTURB )
	{	
		// warp texture, no lightmaps
		EmitWaterPolys( fa );
		return;
	}

	if( fa->flags & SURF_UNDERWATER )
		DrawGLWaterPoly( fa->polys, false );
	else DrawGLPoly( fa->polys );

	// add the poly to the proper lightmap chain
	R_RenderDynamicLightmaps( fa );
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
	float		brightness;
	mtexinfo_t	*tex;
	dlight_t		*dl;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	tex = surf->texinfo;

	for( lnum = 0; lnum < MAX_DLIGHTS; lnum++ )
	{
		if(!( surf->dlightbits & ( 1<<lnum )))
			continue;	// not lit by this light
		dl = &cl_dlights[lnum];

		rad = dl->radius;
		dist = DotProduct( dl->origin, surf->plane->normal ) - surf->plane->dist;
		rad -= fabs( dist );
		minlight = dl->minlight;
		if( rad < minlight ) continue;

		minlight = rad - minlight;

		VectorMA( dl->origin, -dist, surf->plane->normal, impact );
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
				brightness = (rad - dist);

				if( dl->dark )
					brightness = -brightness;

				if( dist < minlight )
				{
					r_blockLights[t * smax + s][0] += dl->color.r * brightness;
					r_blockLights[t * smax + s][1] += dl->color.g * brightness;
					r_blockLights[t * smax + s][2] += dl->color.b * brightness;
				}
			}
		}
	}
}

/*
================
R_DrawSequentialPoly

Systems that have fast state and texture changes can
just do everything as it passes with no need to sort
================
*/
void R_DrawSequentialPoly( msurface_t *s )
{
	glpoly_t	*p;
	float	*v;
	texture_t	*t;
	vec3_t	nv;
	int	i;

	// normal lightmaped poly
	if(!( s->flags & ( SURF_DRAWSKY|SURF_DRAWTURB|SURF_UNDERWATER )))
	{
		R_RenderDynamicLightmaps( s );
		r_stats.c_brush_polys++;

		if( GL_Support( GL_ARB_MULTITEXTURE ))
		{
			p = s->polys;

			t = R_TextureAnimation( s->texinfo->texture );

			GL_Bind( GL_TEXTURE0, t->gl_texturenum );
			GL_TexEnv( GL_REPLACE );
			GL_SetState( GLSTATE_DEPTHWRITE );

			GL_Bind( GL_TEXTURE1, tr.lightmapTextures[s->lightmaptexturenum] );
			i = s->lightmaptexturenum;

			if( r_lmState.lightmap_modified[i] )
				LM_UploadBlock( i );

			GL_TexEnv( GL_MODULATE );

			pglBegin( GL_POLYGON );
			v = p->verts[0];

			for( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
			{
				GL_MultiTexCoord2f( GL_TEXTURE0, v[3], v[4] );
				GL_MultiTexCoord2f( GL_TEXTURE1, v[5], v[6] );
				pglVertex3fv( v );
			}

			pglEnd();
			return;
		}
		else
		{
			p = s->polys;

			t = R_TextureAnimation( s->texinfo->texture );
			GL_Bind( GL_TEXTURE0, t->gl_texturenum );
			GL_TexEnv( GL_REPLACE );
			GL_SetState( GLSTATE_DEPTHWRITE );
		
			pglBegin( GL_POLYGON );
			v = p->verts[0];

			for( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
			{
				pglTexCoord2f( v[3], v[4] );
				pglVertex3fv( v );
			}

			pglEnd();

			GL_Bind( GL_TEXTURE0, tr.lightmapTextures[s->lightmaptexturenum] );
			GL_TexEnv( GL_MODULATE );
			GL_SetState( GLSTATE_SRCBLEND_ZERO|GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR );

			pglBegin( GL_POLYGON );
			v = p->verts[0];
			for( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
			{
				pglTexCoord2f( v[5], v[6] );
				pglVertex3fv( v );
			}

			pglEnd();
			GL_TexEnv( GL_REPLACE );
			GL_SetState( GLSTATE_DEPTHWRITE );
		}

		return;
	}

	// subdivided water surface warp
	if( s->flags & SURF_DRAWTURB )
	{
		GL_CleanUpTextureUnits( 1 );
		GL_Bind( GL_TEXTURE0, s->texinfo->texture->gl_texturenum );
		EmitWaterPolys( s );
		return;
	}

	// subdivided sky warp
	if( s->flags & SURF_DRAWSKY )
	{
		GL_CleanUpTextureUnits( 1 );
		EmitSkyLayers( s );
		return;
	}

	// underwater warped with lightmap
	R_RenderDynamicLightmaps( s );
	r_stats.c_brush_polys++;

	if( GL_Support( GL_ARB_MULTITEXTURE ))
	{
		p = s->polys;

		t = R_TextureAnimation( s->texinfo->texture );
		GL_Bind( GL_TEXTURE0, t->gl_texturenum );
		GL_TexEnv( GL_REPLACE );
			
		GL_Bind( GL_TEXTURE1, tr.lightmapTextures[s->lightmaptexturenum] );
		i = s->lightmaptexturenum;

		if( r_lmState.lightmap_modified[i] )
			LM_UploadBlock( i );
		GL_TexEnv( GL_BLEND );

		pglBegin( GL_TRIANGLE_FAN );
		v = p->verts[0];

		for( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
		{
			GL_MultiTexCoord2f( GL_TEXTURE0, v[3], v[4] );
			GL_MultiTexCoord2f( GL_TEXTURE1, v[5], v[6] );

			nv[0] = v[0] + 8 * com.sin( v[1] * 0.05f + cl.time ) * com.sin( v[2] * 0.05f + cl.time );
			nv[1] = v[1] + 8 * com.sin( v[0] * 0.05f + cl.time ) * com.sin( v[2] * 0.05f + cl.time );
			nv[2] = v[2];
			pglVertex3fv( nv );
		}

		pglEnd ();

	}
	else
	{
		p = s->polys;

		t = R_TextureAnimation( s->texinfo->texture );
		GL_Bind( GL_TEXTURE0, t->gl_texturenum );
		DrawGLWaterPoly( p, false );

		GL_Bind( GL_TEXTURE0, tr.lightmapTextures[s->lightmaptexturenum] );
		pglEnable( GL_BLEND );
		DrawGLWaterPoly( p, true );
		pglDisable( GL_BLEND );
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

	if( !gl_texsort->integer )
	{
		GL_CleanUpTextureUnits( 1 );
				
		if( skychain )
		{
			R_DrawSkyChain( skychain );
			skychain = NULL;
		}
		return;
	} 

	for( i = 0; i < cl.worldmodel->numtextures; i++ )
	{
		t = cl.worldmodel->textures[i];
		if( !t ) continue;

		s = t->texturechain;
		if( !s ) continue;

		if( i == tr.skytexturenum )
		{
			R_DrawSkyChain( s );
		}
		else
		{
			if(( s->flags & SURF_DRAWTURB ) && r_wateralpha->value != 1.0f )
				continue;	// draw translucent water later
			for( ;s != NULL; s = s->texturechain )
				R_RenderBrushPoly( s );
		}
		t->texturechain = NULL;
	}
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
void R_RecursiveWorldNode( mnode_t *node )
{
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	int		c, side, sidebit;
	float		dot;

	if( node->contents == CONTENTS_SOLID )
		return; // hit a solid leaf

	if( node->visframe != tr.visframecount )
		return;

	if( R_CullBox( node->minmaxs, node->minmaxs + 3 ))
		return;
	
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
		return;
	}

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	dot = PlaneDiff( modelorg, node->plane );

	if( dot >= 0 )
	{
		side = 0;
		sidebit = 0;
	}
	else
	{
		side = 1;
		sidebit = SURF_PLANEBACK;
	}

	// recurse down the children, front side first
	R_RecursiveWorldNode( node->children[side] );

	// draw stuff
	for( c = node->numsurfaces, surf = cl.worldmodel->surfaces + node->firstsurface; c; c--, surf++ )
	{
		if( surf->visframe != tr.framecount )
			continue;

		if(( surf->flags & SURF_PLANEBACK ) != sidebit )
			continue;	// wrong side

		// if sorting by texture, just store it out
		if( gl_texsort->integer )
		{
			surf->texturechain = surf->texinfo->texture->texturechain;
			surf->texinfo->texture->texturechain = surf;
		}
		else if( surf->flags & SURF_DRAWSKY )
		{
			surf->texturechain = skychain;
			skychain = surf;
		}
		else if( surf->flags & SURF_DRAWTURB )
		{
			surf->texturechain = waterchain;
			waterchain = surf;
		}
		else R_DrawSequentialPoly( surf );
	}

	// recurse down the back side
	R_RecursiveWorldNode( node->children[!side] );
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld( void )
{
	RI.currententity = clgame.entities;
	RI.currentmodel = RI.currententity->model;

	VectorCopy( RI.refdef.vieworg, modelorg );
	Mem_Set( r_lmState.lightmap_polys, 0, sizeof( r_lmState.lightmap_polys ));

	ClearBounds( RI.visMins, RI.visMaxs );

	R_ClearSkyBox ();
	R_RecursiveWorldNode( cl.worldmodel->nodes );

	R_DrawTextureChains();

	R_BlendLightmaps();

	R_DrawSkyBox ();
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

			Mem_Copy( visbytes, vis, longs << 2 );
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
	int	smax, tmax, t, s, i;
	int	size, map, blocksize;
	float	*bl, max, scale;
	color24	*lm;

	surf->cached_dlight = ( surf->dlightframe == tr.framecount );

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;
	blocksize = size * 3;
	lm = surf->samples;

	// set to full bright if no light data
	if( !lm || r_fullbright->integer || !cl.worldmodel->lightdata )
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
		scale = cl.lightstyles[surf->styles[0]].value * r_lighting_modulate->value;
		surf->cached_light[surf->styles[0]] = scale;
			
		for( i = 0, bl = r_blockLights[0]; i < size; i++, bl += 3, lm++ )
		{
			bl[0] = lm->r * scale;
			bl[1] = lm->g * scale;
			bl[2] = lm->b * scale;
		}

		for( map = 1; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++ )
		{
			scale = cl.lightstyles[surf->styles[map]].value * r_lighting_modulate->value;
			surf->cached_light[surf->styles[map]] = scale;
		
			for( i = 0, bl = r_blockLights[0]; i < size; i++, bl += 3, lm++ )
			{
				bl[0] += lm->r * scale;
				bl[1] += lm->g * scale;
				bl[2] += lm->b * scale;
			}
		}
	}

	// add all the dynamic lights
	if( surf->cached_dlight )
		R_AddDynamicLights( surf );

	// put into texture format
	stride -= (smax << 2);
	bl = r_blockLights[0];

	for( t = 0; t < tmax; t++, dest += stride )
	{
		for( s = 0; s < smax; s++ )
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

				dest[0] = 255 - ( bl[0] * max );
				dest[1] = 255 - ( bl[1] * max );
				dest[2] = 255 - ( bl[2] * max );
				dest[3] = 255;
			}
			else
			{
				dest[0] = 255 - bl[0];
				dest[1] = 255 - bl[1];
				dest[2] = 255 - bl[2];
				dest[3] = 255;
			}

			bl += 3;
			dest += 4;
		}
	}
}

static void LM_UploadBlock( int lightmapnum )
{
	wrect_t	*rect;

	r_lmState.lightmap_modified[lightmapnum] = false;
	rect = &r_lmState.lightmap_rectchange[lightmapnum];

	pglTexSubImage2D( GL_TEXTURE_2D, 0, 0, rect->right, BLOCK_WIDTH, rect->bottom, GL_RGBA, GL_UNSIGNED_BYTE,
	&r_lmState.lightmaps[(lightmapnum * BLOCK_HEIGHT + rect->right) * BLOCK_WIDTH * 4] );

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
		Host_Error( "GL_CreateSurfaceLightmap: lightmap width %d > %d, %s\n", smax, BLOCK_WIDTH, surf->texinfo->texture->name );
	if( tmax > BLOCK_HEIGHT )
		Host_Error( "GL_CreateSurfaceLightmap: lightmap height %d > %d\n", tmax, BLOCK_HEIGHT );
	if( smax * tmax > BLOCK_WIDTH * BLOCK_HEIGHT )
		Host_Error( "GL_CreateSurfaceLightmap: lightmap size too big\n" );

	surf->lightmaptexturenum = LM_AllocBlock( smax, tmax, &surf->light_s, &surf->light_t );
	base = r_lmState.lightmaps + surf->lightmaptexturenum * BLOCK_WIDTH * BLOCK_HEIGHT * 4;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * 4;
	r_numdlights = 0;

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
	rgbdata_t	r_lightmap;
	char	lmName[16];
	model_t	*m;

	// release old lightmaps
	for( i = 0; i < MAX_LIGHTMAPS; i++ )
	{
		if( !tr.lightmapTextures[i] ) break;
		GL_FreeTexture( tr.lightmapTextures[i] );
	}

	Mem_Set( tr.lightmapTextures, 0, sizeof( tr.lightmapTextures ));
	Mem_Set( visbytes, 0x00, sizeof( visbytes ));
	Mem_Set( &r_lmState, 0, sizeof( r_lmState ));

	skychain = waterchain = NULL;

	LM_InitBlock();	

	for( i = 1; i < MAX_MODELS; i++ )
	{
		if(( m = CM_ClipHandleToModel( i )) == NULL )
			continue;

		if( m->name[0] == '*' || m->type != mod_brush )
			continue;

		loadmodel = m;

		for( j = 0; j < m->numsurfaces; j++ )
		{
			GL_CreateSurfaceLightmap( m->surfaces + j );

			if ( m->surfaces[i].flags & SURF_DRAWTILED )
				continue;

			GL_BuildPolygonFromSurface( m->surfaces + j );
		}
	}

	loadmodel = NULL;

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
		r_lightmap.type = PF_RGBA_32;
		r_lightmap.size = r_lightmap.width * r_lightmap.height * 4;
		r_lightmap.flags = IMAGE_HAS_COLOR; // FIXME: detecting grayscale lightmaps for quake1
		r_lightmap.buffer = (byte *)&r_lmState.lightmaps[r_lightmap.size*i];
		tr.lightmapTextures[i] = GL_LoadTextureInternal( lmName, &r_lightmap, TF_LIGHTMAP, false );
		GL_SetTextureType( tr.lightmapTextures[i], TEX_LIGHTMAP );
	}
}