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

// r_model.c -- model loading and caching

#include "stdio.h"		// sscanf
#include "r_local.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "byteorder.h"
#include "bspfile.h"

#define Mod_CopyString( m, str )	com.stralloc( (m)->mempool, str, __FILE__, __LINE__ )
#define MAX_SIDE_VERTS		256	// per one polygon

typedef struct epair_s
{
	struct epair_s	*next;
	char		*key;
	char		*value;
} epair_t;

typedef struct
{
	vec3_t		origin;
	epair_t		*epairs;
} mapent_t;

typedef enum
{
	emit_point,
	emit_spotlight,
	emit_surface,
	emit_skylight
} emittype_t;

typedef struct
{
	int	numpoints;
	int	maxpoints;
	vec3_t	points[8];	// variable sized
} winding_t;

typedef struct
{
	vec3_t		dir;
	vec3_t		color;
	int		style;
} contribution_t;

typedef struct light_s
{
	struct light_s	*next;
	emittype_t	type;
	int		style;
	vec3_t		origin;
	vec3_t		color;
	vec3_t		normal;		// for surfaces and spotlights
	float		photons;
	float		dist;
	float		stopdot;		// for spotlights
	float		stopdot2;		// for spotlights
	winding_t		*w;
} light_t;

typedef struct
{
	char		name[32];
	ref_shader_t	*shader;

	mip_t		*base;		// general texture

	bool		animated;
	mip_t		*anim_frames[2][10];// (indexed as [alternate][frame])
	int		anim_total[2];	// total frames in sequence and alternate sequence

	int		width;
	int		height;
} cachedimage_t;

// intermediate cached data
static struct
{
	int		version;		// brushmodel version
	string		modelname;	// mapname without path, extension etc
	vec3_t		*vertexes;	// intermediate data comes here
	int		numvertexes;
	dedge_t		*edges;
	int		numedges;
	dsurfedge_t	*surfedges;
	int		numsurfedges;
	cachedimage_t	*textures;
	int		numtextures;
	light_t		*lights;		// stored pointlights
	int		numPointLights;
	script_t		*entscript;
	mapent_t		*entities;	// sizeof( mapent_t ) * GI->max_edicts
	int		numents;
} cached; 

static ref_model_t		*loadmodel;
static byte		*cached_mempool;

void Mod_SpriteLoadModel( ref_model_t *mod, const void *buffer );
void Mod_StudioLoadModel( ref_model_t *mod, const void *buffer );
void Mod_BrushLoadModel( ref_model_t *mod, const void *buffer );

ref_model_t *Mod_LoadModel( ref_model_t *mod, bool crash );

static ref_model_t		*r_inlinemodels = NULL;
static byte		mod_novis[MAX_MAP_LEAFS/8];
static ref_model_t		r_models[MAX_MODELS];
static int		r_nummodels;

/*
===================
Mod_DecompressVis
===================
*/
static byte *Mod_DecompressVis( const byte *in, mbrushmodel_t *model )
{
	static byte	decompressed[MAX_MAP_LEAFS/8];
	int		c, row;
	byte		*out;

	if( !model )
	{
		Host_Error( "Mod_DecompressVis: no worldmodel\n" );
		return NULL;
	}

	row = (model->numleafs + 7)>>3;	
	out = decompressed;

	if( !in )
	{	
		// no vis info, so make all visible
		while( row )
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;		
	}

	do
	{
		if( *in )
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		in += 2;

		while( c )
		{
			*out++ = 0;
			c--;
		}
	} while( out - decompressed < row );

	return decompressed;
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf( const vec3_t p, ref_model_t *model )
{
	mnode_t		*node;
	cplane_t		*plane;
	mbrushmodel_t	*bmodel;

	if( !model || !( bmodel = ( mbrushmodel_t *)model->extradata ) || !bmodel->nodes )
	{
		Host_Error( "Mod_PointInLeaf: bad model\n" );
		return NULL;
	}

	node = bmodel->nodes;
	do
	{
		plane = node->plane;
		node = node->children[PlaneDiff( p, plane ) < 0];
	} while( node->plane != NULL );

	return (mleaf_t *)node;
}

/*
==============
Mod_LeafPVS
==============
*/
byte *Mod_LeafPVS( mleaf_t *leaf, ref_model_t *model )
{
	mbrushmodel_t	*bmodel = (mbrushmodel_t *)model->extradata;

	if( !model || !bmodel || !leaf || leaf == bmodel->leafs || !bmodel->visdata )
		return mod_novis;
	return Mod_DecompressVis( leaf->compressed_vis, bmodel );
}


//===============================================================================

/*
================
Mod_Modellist_f
================
*/
void Mod_Modellist_f( void )
{
	int		i, nummodels;
	ref_model_t	*mod;

	Msg( "\n" );
	Msg( "-----------------------------------\n" );

	for( i = nummodels = 0, mod = r_models; i < r_nummodels; i++, mod++ )
	{
		if( !mod->name ) continue; // free slot
		Msg( "%s%s\n", mod->name, (mod->type == mod_bad) ? " (DEFAULTED)" : "" );
		nummodels++;
	}

	Msg( "-----------------------------------\n" );
	Msg( "%i total models\n", nummodels );
	Msg( "\n" );
}

/*
================
Mod_FreeModel
================
*/
void Mod_FreeModel( ref_model_t *mod )
{
	if( !mod || !mod->mempool ) return;

	if( mod == r_worldmodel ) R_FreeSky();
	Mem_FreePool( &mod->mempool );
	Mem_Set( mod, 0, sizeof( *mod ));
}

/*
================
Mod_FreeAll
================
*/
void Mod_FreeAll( void )
{
	int	i;

	if( r_inlinemodels )
	{
		Mem_Free( r_inlinemodels );
		r_inlinemodels = NULL;
	}

	for( i = 0; i < r_nummodels; i++ )
		Mod_FreeModel( &r_models[i] );
}

/*
===============
R_InitModels
===============
*/
void R_InitModels( void )
{
	Mem_Set( mod_novis, 0xff, sizeof( mod_novis ));
	cached_mempool = Mem_AllocPool( "Mod cache" );
	r_nummodels = 0;

	R_StudioInit();
	R_SpriteInit();
}

/*
================
R_ShutdownModels
================
*/
void R_ShutdownModels( void )
{
	if( !cached_mempool ) return;

	R_StudioShutdown();
	Mod_FreeAll();

	r_worldmodel = NULL;
	r_worldbrushmodel = NULL;

	r_nummodels = 0;
	Mem_Set( r_models, 0, sizeof( r_models ));
	Mem_FreePool( &cached_mempool );
}

/*
==================
Mod_FindSlot
==================
*/
static ref_model_t *Mod_FindSlot( const char *name )
{
	ref_model_t	*mod;
	int		i;

	// find a free model slot spot
	for( i = 0, mod = r_models; i < r_nummodels; i++, mod++ )
		if( !mod->name ) break; // free spot

	if( i == r_nummodels )
	{
		if( r_nummodels == MAX_MODELS )
			Host_Error( "Mod_ForName: MAX_MODELS limit exceeded\n" );
		r_nummodels++;
	}
	return mod;
}

/*
==================
Mod_Handle
==================
*/
uint Mod_Handle( ref_model_t *mod )
{
	return mod - r_models;
}

/*
==================
Mod_ForHandle
==================
*/
ref_model_t *Mod_ForHandle( uint handle )
{
	return r_models + handle;
}

/*
=================
Mod_UpdateShaders

update shader and associated textures
=================
*/
static void Mod_UpdateShaders( ref_model_t *mod )
{
	ref_shader_t	*shader;
	int		i;

	if( !mod || !mod->name )
		return;

	if( mod->type == mod_world || mod->type == mod_brush )
	{
		mbrushmodel_t	*bmodel;
	
		// clearing all linked decals here
		bmodel = (mbrushmodel_t *)mod->extradata;
		for( i = 0; i < bmodel->numsurfaces; i++ )
			bmodel->surfaces[i].pdecals = NULL;
	}
  
	for( i = 0; i < mod->numshaders; i++ )
	{
		shader = mod->shaders[i];
		if( !shader || !shader->name ) continue;
		Shader_TouchImages( shader, false );
	}

	if( mod == r_worldmodel && tr.currentSkyShader )
		Shader_TouchImages( tr.currentSkyShader, false );
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
ref_model_t *Mod_ForName( const char *name, bool crash )
{
	ref_model_t	*mod;
	uint		*buf;
	int	 	i;

	if( !name[0] ) Host_Error( "Mod_ForName: NULL name\n" );

	// inline models are grabbed only from worldmodel
	if( name[0] == '*' )
	{
		i = com.atoi( name + 1 );
		if( i < 1 || !r_worldmodel || i >= r_worldbrushmodel->numsubmodels )
		{
			MsgDev( D_ERROR, "bad inline model number %i\n", i );
			return NULL;
		}
		return &r_inlinemodels[i];
	}

	// search the currently loaded models
	for( i = 0, mod = r_models; i < r_nummodels; i++, mod++ )
	{
		if( !mod->name ) continue;
		if( !com.strcmp( mod->name, name ))
		{
			// prolonge registration
			mod->touchFrame = tr.registration_sequence;
			return mod;
		}
	}

	mod = Mod_FindSlot( name );

	Com_Assert( mod == NULL );

	// load the file
	buf = (uint *)FS_LoadFile( name, NULL );
	if( !buf )
	{
		if( crash ) Host_Error( "Mod_ForName: %s not found\n", name );
		return NULL; // return the NULL model
	}

	loadmodel = mod;
	Mem_EmptyPool( cached_mempool );
	Mem_Set( &cached, 0, sizeof( cached ));

	mod->type = mod_bad;
	mod->mempool = Mem_AllocPool( va( "cl: ^1%s^7", name ));
	mod->name = Mod_CopyString( mod, name );
	FS_FileBase( mod->name, cached.modelname );

	// call the apropriate loader
	switch( LittleLong( *(uint *)buf ))
	{
	case IDSTUDIOHEADER:
		Mod_StudioLoadModel( mod, buf );
		break;
	case IDSPRITEHEADER:
		Mod_SpriteLoadModel( mod, buf );
		break;
	default:
		Mod_BrushLoadModel( mod, buf );
		break;
	}

	Mem_Free( buf );

	if( mod->type == mod_bad )
	{
		// check for loading problems
		if( crash ) Host_Error( "Mod_ForName: %s unknown format\n", name );
		else MsgDev( D_ERROR, "Mod_ForName: %s unknown format\n", name );
		Mod_FreeModel( mod );
		return NULL;
	}
	return mod;
}

/*
===============================================================================

BRUSHMODEL LOADING

===============================================================================
*/

static byte		*mod_base;
static mbrushmodel_t	*loadbmodel;

/*
=================
Mod_CheckDeluxemaps
=================
*/
static void Mod_CheckDeluxemaps( const dlump_t *l, byte *lmData )
{
	if( !r_lighting_deluxemapping->integer )
		return;

	// deluxemapping temporare disabled
	// FIXME: re-enable it again
//	if( GL_Support( R_SHADER_GLSL100_EXT ))
	mapConfig.deluxeMappingEnabled = false;
}

/*
=================
Mod_SetNodeParent
=================
*/
static void Mod_SetNodeParent( mnode_t *node, mnode_t *parent )
{
	node->parent = parent;
	if( node->contents < 0 ) return; // it's a leaf

	Mod_SetNodeParent( node->children[0], node );
	Mod_SetNodeParent( node->children[1], node );
}

/*
=================
Mod_CalcSurfaceBounds

fills in surf->mins and surf->maxs
=================
*/
static void Mod_CalcSurfaceBounds( msurface_t *surf )
{
	int	i, e;
	float	*v;

	ClearBounds( surf->mins, surf->maxs );

	for( i = 0; i < surf->numedges; i++ )
	{
		e = cached.surfedges[surf->firstedge + i];
		if( e >= 0 ) v = (float *)&cached.vertexes[cached.edges[e].v[0]];
		else v = (float *)&cached.vertexes[cached.edges[-e].v[1]];
		AddPointToBounds( v, surf->mins, surf->maxs );
	}
}

/*
=================
Mod_CalcSurfaceExtents

Fills in surf->textureMins and surf->extents
=================
*/
static void Mod_CalcSurfaceExtents( msurface_t *surf )
{
	float	mins[2], maxs[2], val;
	int	bmins[2], bmaxs[2];
	int	i, j, e;
	float	*v;

	if( surf->flags & SURF_DRAWTURB )
	{
		surf->extents[0] = surf->extents[1] = 16384;
		surf->textureMins[0] = surf->textureMins[1] = -8192;
		return;
	}

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -999999;

	for( i = 0; i < surf->numedges; i++ )
	{
		e = cached.surfedges[surf->firstedge + i];
		if( e >= 0 ) v = (float *)&cached.vertexes[cached.edges[e].v[0]];
		else v = (float *)&cached.vertexes[cached.edges[-e].v[1]];

		for( j = 0; j < 2; j++ )
		{
			val = DotProduct( v, surf->texinfo->vecs[j] ) + surf->texinfo->vecs[j][3];
			if( val < mins[j] ) mins[j] = val;
			if( val > maxs[j] ) maxs[j] = val;
		}
	}

	for( i = 0; i < 2; i++ )
	{
		bmins[i] = floor( mins[i] / LM_SAMPLE_SIZE );
		bmaxs[i] = ceil( maxs[i] / LM_SAMPLE_SIZE );

		surf->textureMins[i] = bmins[i] * LM_SAMPLE_SIZE;
		surf->extents[i] = (bmaxs[i] - bmins[i]) * LM_SAMPLE_SIZE;
	}
}

/*
=================
Mod_BuildPolygon
=================
*/
static void Mod_BuildPolygon( msurface_t *surf, int numVerts, const float *verts )
{
	float		s, t;
	uint		index, bufSize;
	mtexinfo_t	*texinfo = surf->texinfo;
	bool		createSTverts = false;
	int		i, numElems;
	byte		*buffer;
	vec3_t		normal;
	mesh_t		*mesh;

	// allocate mesh
	numElems = (numVerts - 2) * 3;

	if( mapConfig.deluxeMappingEnabled || ( surf->shader->flags & SHADER_PORTAL_CAPTURE2 ))
		createSTverts = true;

	// mesh + ( align vertex, align normal, (st + lmst) + elem * numElems) * numVerts;
	bufSize = sizeof( mesh_t ) + numVerts * ( sizeof( vec4_t ) + sizeof( vec4_t ) + sizeof( vec4_t )) + numElems * sizeof( elem_t );
	if( createSTverts ) bufSize += numVerts * sizeof( vec4_t );

	buffer = Mod_Malloc( loadmodel, bufSize );

	mesh = (mesh_t *)buffer;
	buffer += sizeof( mesh_t );
	mesh->numVerts = numVerts;
	mesh->numElems = numElems;

	// setup pointers
	mesh->vertexArray = (vec4_t *)buffer;
	buffer += numVerts * sizeof( vec4_t );
	mesh->normalsArray = (vec4_t *)buffer;
	buffer += numVerts * sizeof( vec4_t );
	mesh->stCoordArray = (vec2_t *)buffer;
	buffer += numVerts * sizeof( vec2_t );
	mesh->lmCoordArray = (vec2_t *)buffer;
	buffer += numVerts * sizeof( vec2_t );
	mesh->elems = (elem_t *)buffer;
	buffer += numElems * sizeof( elem_t );

	mesh->next = surf->mesh;
	surf->mesh = mesh;

	// create indices
	for( i = 0, index = 2; i < mesh->numElems; i += 3, index++ )
	{
		mesh->elems[i+0] = 0;
		mesh->elems[i+1] = index - 1;
		mesh->elems[i+2] = index;
	}

	// setup normal
	if( surf->flags & SURF_PLANEBACK )
		VectorNegate( surf->plane->normal, normal );
	else VectorCopy( surf->plane->normal, normal );

	VectorNormalize( normal );

	// create vertices
	mesh->numVerts = numVerts;
	
	for( i = 0; i < numVerts; i++, verts += 3 )
	{
		// vertex
		VectorCopy( verts, mesh->vertexArray[i] );
		VectorCopy( normal, mesh->normalsArray[i] );

		mesh->vertexArray[i][3] = 1.0f;
		mesh->normalsArray[i][3] = 1.0f;

		// texture coordinates
		s = DotProduct( verts, texinfo->vecs[0] ) + texinfo->vecs[0][3];
		if( texinfo->width != -1 ) s /= texinfo->width;
		else s /= surf->shader->stages[0].textures[0]->width;

		t = DotProduct( verts, texinfo->vecs[1] ) + texinfo->vecs[1][3];
		if( texinfo->height != -1 ) t /= texinfo->height;
		else t /= surf->shader->stages[0].textures[0]->height;

		mesh->stCoordArray[i][0] = s;
		mesh->stCoordArray[i][1] = t;

		// lightmap texture coordinates
		s = DotProduct( verts, texinfo->vecs[0] ) + texinfo->vecs[0][3] - surf->textureMins[0];
		s += surf->lmS * LM_SAMPLE_SIZE;
		s += LM_SAMPLE_SIZE >> 1;
		s /= LIGHTMAP_TEXTURE_WIDTH * LM_SAMPLE_SIZE;

		t = DotProduct( verts, texinfo->vecs[1] ) + texinfo->vecs[1][3] - surf->textureMins[1];
		t += surf->lmT * LM_SAMPLE_SIZE;
		t += LM_SAMPLE_SIZE >> 1;
		t /= LIGHTMAP_TEXTURE_HEIGHT * LM_SAMPLE_SIZE;

		mesh->lmCoordArray[i][0] = s;
		mesh->lmCoordArray[i][1] = t;
	}

	if( createSTverts )
	{
		mesh->sVectorsArray = (vec4_t *)buffer;
		buffer += numVerts * sizeof( vec4_t );
		R_BuildTangentVectors( mesh->numVerts, mesh->vertexArray, mesh->normalsArray, mesh->stCoordArray, mesh->numElems / 3, mesh->elems, mesh->sVectorsArray );
	}
}

/*
=================
Mod_SubdividePolygon
=================
*/
static void Mod_SubdividePolygon( msurface_t *surf, int numVerts, float *verts )
{
	int		i, j, f, b, subdivideSize;
	vec3_t		vTotal, nTotal, mins, maxs;
	mtexinfo_t	*texinfo = surf->texinfo;
	vec3_t		front[MAX_SIDE_VERTS], back[MAX_SIDE_VERTS];
	float		*v, m, oneDivVerts, dist, dists[MAX_SIDE_VERTS];
	bool		createSTverts = false;
	vec2_t		totalST, totalLM;
	uint		bufSize;
	byte		*buffer;
	float		s, t;
	mesh_t		*mesh;

	subdivideSize = surf->shader->tessSize;

	ClearBounds( mins, maxs );
	for( i = 0, v = verts; i < numVerts; i++, v += 3 )
		AddPointToBounds( v, mins, maxs );

	for( i = 0; i < 3; i++ )
	{
		m = subdivideSize * floor((( mins[i] + maxs[i] ) * 0.5f ) / subdivideSize + 0.5f );

		if( maxs[i] - m < 8 ) continue;
		if( m - mins[i] < 8 ) continue;

		// cut it
		v = verts + i;
		for( j = 0; j < numVerts; j++, v += 3 )
			dists[j] = *v - m;

		// wrap cases
		dists[j] = dists[0];
		v -= i;
		VectorCopy( verts, v );

		for( f = j = b = 0, v = verts; j < numVerts; j++, v += 3 )
		{
			if( dists[j] >= 0 )
			{
				VectorCopy( v, front[f] );
				f++;
			}
			if( dists[j] <= 0 )
			{
				VectorCopy( v, back[b] );
				b++;
			}
			
			if( dists[j] == 0 || dists[j+1] == 0 )
				continue;
			
			if((dists[j] > 0) != (dists[j+1] > 0))
			{
				// clip point
				dist = dists[j] / (dists[j] - dists[j+1]);
				front[f][0] = back[b][0] = v[0] + (v[3] - v[0]) * dist;
				front[f][1] = back[b][1] = v[1] + (v[4] - v[1]) * dist;
				front[f][2] = back[b][2] = v[2] + (v[5] - v[2]) * dist;
				f++;
				b++;
			}
		}

		Mod_SubdividePolygon( surf, f, front[0] );
		Mod_SubdividePolygon( surf, b, back[0] );
		return;
	}

	if( mapConfig.deluxeMappingEnabled || ( surf->shader->flags & SHADER_PORTAL_CAPTURE2 ))
		createSTverts = true;

	// allocate mesh
	bufSize = sizeof( mesh_t ) + ((numVerts + 2) * sizeof( rgba_t )) + ((numVerts + 2) * sizeof( vec4_t ) * 2) + ((numVerts + 2) * sizeof( vec2_t ) * 2);
	if( createSTverts ) bufSize += (numVerts + 2) * sizeof( vec4_t );

	buffer = Mod_Malloc( loadmodel, bufSize );

	mesh = (mesh_t *)buffer;
	buffer += sizeof( mesh_t );

	// create vertices
	mesh->numVerts = numVerts + 2;
	mesh->numElems = numVerts * 3;

	// setup pointers
	mesh->colorsArray = (rgba_t *)buffer;
	buffer += mesh->numVerts * sizeof( rgba_t );
	mesh->vertexArray = (vec4_t *)buffer;
	buffer += mesh->numVerts * sizeof( vec4_t );
	mesh->normalsArray = (vec4_t *)buffer;
	buffer += mesh->numVerts * sizeof( vec4_t );
	mesh->stCoordArray = (vec2_t *)buffer;
	buffer += mesh->numVerts * sizeof( vec2_t );
	mesh->lmCoordArray = (vec2_t *)buffer;
	buffer += mesh->numVerts * sizeof( vec2_t );

	VectorClear( vTotal );
	VectorClear( nTotal );
	totalST[0] = totalST[1] = 0;
	totalLM[0] = totalLM[1] = 0;

	for( i = 0; i < numVerts; i++, verts += 3 )
	{
		// colors
		Vector4Set( mesh->colorsArray[i+1], 255, 255, 255, 255 );
		
		// vertex
		VectorCopy( verts, mesh->vertexArray[i+1] );

		// setup normal
		if( surf->flags & SURF_PLANEBACK )
			VectorNegate( surf->plane->normal, mesh->normalsArray[i+1] );
		else VectorCopy( surf->plane->normal, mesh->normalsArray[i+1] );

		mesh->vertexArray[i+1][3] = 1.0f;
		mesh->normalsArray[i+1][3] = 1.0f;

		VectorAdd( vTotal, verts, vTotal );
 		VectorAdd( nTotal, surf->plane->normal, nTotal );

		// texture coordinates
		s = DotProduct( verts, texinfo->vecs[0] ) + texinfo->vecs[0][3];
		if( texinfo->width != -1 ) s /= texinfo->width;
		else s /= surf->shader->stages[0].textures[0]->width;

		t = DotProduct( verts, texinfo->vecs[1] ) + texinfo->vecs[1][3];
		if( texinfo->height != -1 ) t /= texinfo->height;
		else t /= surf->shader->stages[0].textures[0]->height;

		mesh->stCoordArray[i+1][0] = s;
		mesh->stCoordArray[i+1][1] = t;

		totalST[0] += s;
		totalST[1] += t;

		// lightmap texture coordinates
		s = DotProduct( verts, texinfo->vecs[0] ) + texinfo->vecs[0][3] - surf->textureMins[0];
		s += surf->lmS * LM_SAMPLE_SIZE;
		s += LM_SAMPLE_SIZE >> 1;
		s /= LIGHTMAP_TEXTURE_WIDTH * LM_SAMPLE_SIZE;

		t = DotProduct( verts, texinfo->vecs[1] ) + texinfo->vecs[1][3] - surf->textureMins[1];
		t += surf->lmT * LM_SAMPLE_SIZE;
		t += LM_SAMPLE_SIZE >> 1;
		t /= LIGHTMAP_TEXTURE_HEIGHT * LM_SAMPLE_SIZE;

		mesh->lmCoordArray[i+1][0] = s;
		mesh->lmCoordArray[i+1][1] = t;

		totalLM[0] += s;
		totalLM[1] += t;
	}

	// vertex
	oneDivVerts = ( 1.0f / (float)numVerts );

	Vector4Set( mesh->colorsArray[0], 255, 255, 255, 255 );
	VectorScale( vTotal, oneDivVerts, mesh->vertexArray[0] );
	VectorScale( nTotal, oneDivVerts, mesh->normalsArray[0] );
	VectorNormalize( mesh->normalsArray[0] );

	// texture coordinates
	mesh->stCoordArray[0][0] = totalST[0] * oneDivVerts;
	mesh->stCoordArray[0][1] = totalST[1] * oneDivVerts;

	// lightmap texture coordinates
	mesh->lmCoordArray[0][0] = totalLM[0] * oneDivVerts;
	mesh->lmCoordArray[0][1] = totalLM[1] * oneDivVerts;

	// copy first vertex to last
	Vector4Set( mesh->colorsArray[i+1], 255, 255, 255, 255 );
	VectorCopy( mesh->vertexArray[1], mesh->vertexArray[i+1] );
	VectorCopy( mesh->normalsArray[1], mesh->normalsArray[i+1] );
	Vector2Copy( mesh->stCoordArray[1], mesh->stCoordArray[i+1] );
	Vector2Copy( mesh->lmCoordArray[1], mesh->lmCoordArray[i+1] );

	if( createSTverts )
	{
		mesh->sVectorsArray = (vec4_t *)buffer;
		buffer += (numVerts + 2) * sizeof( vec4_t );
		R_BuildTangentVectors( mesh->numVerts, mesh->vertexArray, mesh->normalsArray, mesh->stCoordArray, mesh->numElems / 3, mesh->elems, mesh->sVectorsArray );
	}

	mesh->next = surf->mesh;
	surf->mesh = mesh;
}

/*
================
Mod_ConvertSurface
================
*/
static void Mod_ConvertSurface( msurface_t *surf )
{
	byte		*buffer;
	mesh_t		*poly, *next;
	uint		totalIndexes;
	uint		totalVerts;
	byte		*outColors;
	float		*outCoords;
	elem_t		*outIndexes;
	float		*outLMCoords;
	mesh_t		*outMesh;
	float		*outNormals;
	float		*outVerts;
	int		i;

	// find the total vertex count and index count
	totalIndexes = 0;
	totalVerts = 0;

	for( poly = surf->mesh; poly; poly = poly->next )
	{
		totalIndexes += ( poly->numVerts - 2 ) * 3;
		totalVerts += poly->numVerts;
	}

	// allocate space
	if( surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
	{

		buffer = Mod_Malloc( loadmodel, sizeof( mesh_t )
			+ (totalVerts * sizeof( vec4_t ) * 2 )
			+ (totalIndexes * sizeof( elem_t ))
			+ (totalVerts * sizeof( vec2_t ))
			+ (totalVerts * sizeof( rgba_t )));
	}
	else
	{
		buffer = Mod_Malloc( loadmodel, sizeof( mesh_t )
			+ (totalVerts * sizeof( vec4_t ) * 2 )
			+ (totalIndexes * sizeof( elem_t ))
			+ (totalVerts * sizeof( vec2_t ) * 2 )
			+ (totalVerts * sizeof( rgba_t )));
	}

	outMesh = (mesh_t *)buffer;
	outMesh->numElems = totalIndexes;
	outMesh->numVerts = totalVerts;

	buffer += sizeof( mesh_t );
	outVerts = (float *)buffer;

	buffer += sizeof( vec4_t ) * totalVerts;
	outNormals = (float *)buffer;

	buffer += sizeof( vec4_t ) * totalVerts;
	outIndexes = (elem_t *)buffer;

	buffer += sizeof( elem_t ) * totalIndexes;
	outCoords = (float *)buffer;

	if( surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
	{
		outLMCoords = NULL;
	}
	else
	{
		buffer += sizeof( vec2_t ) * totalVerts;
		outLMCoords = (float *)buffer;
	}

	buffer += sizeof( vec2_t ) * totalVerts;
	outColors = (byte *)buffer;

	outMesh->colorsArray = (rgba_t *)outColors;
	outMesh->stCoordArray = (vec2_t *)outCoords;
	outMesh->elems = (elem_t *)outIndexes;
	outMesh->lmCoordArray = (vec2_t *)outLMCoords;
	outMesh->normalsArray = (vec4_t *)outNormals;
	outMesh->vertexArray = (vec4_t *)outVerts;
	outMesh->sVectorsArray = NULL;
	outMesh->tVectorsArray = NULL;

	// check mesh validity
	if( R_InvalidMesh( outMesh ))
	{
		MsgDev( D_ERROR, "Mod_ConvertSurface: surface mesh is invalid!\n" );
		Mem_Free( buffer );
		return;
	}

	// store vertex data
	totalIndexes = 0;
	totalVerts = 0;

	for( poly = surf->mesh; poly; poly = poly->next )
	{
		// indexes
		outIndexes = outMesh->elems + totalIndexes;
		totalIndexes += (poly->numVerts - 2) * 3;

		for( i = 2; i < poly->numVerts; i++ )
		{
			outIndexes[0] = totalVerts;
			outIndexes[1] = totalVerts + i - 1;
			outIndexes[2] = totalVerts + i;
			outIndexes += 3;
		}

		for( i = 0; i < poly->numVerts; i++ )
		{
			// vertices
			outVerts[0] = poly->vertexArray[i][0];
			outVerts[1] = poly->vertexArray[i][1];
			outVerts[2] = poly->vertexArray[i][2];
			outVerts[3] = 1.0f;

			// Normals
			outNormals[0] = poly->normalsArray[i][0];
			outNormals[1] = poly->normalsArray[i][1];
			outNormals[2] = poly->normalsArray[i][2];
			outNormals[3] = 1.0f;

			// colors
			outColors[0] = 255;
			outColors[1] = 255;
			outColors[2] = 255;
			outColors[3] = 255;

			// coords
			outCoords[0] = poly->stCoordArray[i][0];
			outCoords[1] = poly->stCoordArray[i][1];

			outVerts += 4;
			outNormals += 4;
			outColors += 4;
			outCoords += 2;
		}

		totalVerts += poly->numVerts;
	}

	// lightmap coords
	if(!( surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB )))
	{
		for( poly = surf->mesh; poly; poly = poly->next )
		{
			for( i = 0; i < poly->numVerts; i++ )
			{
				outLMCoords[0] = poly->lmCoordArray[i][0];
				outLMCoords[1] = poly->lmCoordArray[i][1];

				outLMCoords += 2;
			}
		}
	}

	// release the old q2_polys crap
	for( poly = surf->mesh; poly; poly = next )
	{
		next = poly->next;
		Mem_Free( poly );
	}

	surf->mesh = outMesh;
}

/*
=================
Mod_BuildSurfacePolygons
=================
*/
static void Mod_BuildSurfacePolygons( msurface_t *surf )
{
	float	*v;
	int	i, e;
	vec3_t	verts[MAX_SIDE_VERTS];
	vec3_t	ebbox = { 0, 0, 0 };

	// convert edges back to a normal polygon
	for( i = 0; i < surf->numedges; i++ )
	{
		if( i == 256 ) break; // too big polygon ?

		e = cached.surfedges[surf->firstedge + i];
		if( e >= 0 ) v = cached.vertexes[cached.edges[e].v[0]];
		else v = cached.vertexes[cached.edges[-e].v[1]];
		VectorCopy( v, verts[i] );
	}

	R_DeformvBBoxForShader( surf->shader, ebbox );

	if( surf->shader->tessSize != 0.0f )
	{
		Mod_SubdividePolygon( surf, surf->numedges, verts[0] );
		Mod_ConvertSurface( surf );
	}
	else Mod_BuildPolygon( surf, surf->numedges, verts[0] );

	if( !surf->mesh ) return;

	ClearBounds( surf->mins, surf->maxs );
	for( i = 0, v = surf->mesh->vertexArray[0]; i < surf->mesh->numVerts; i++, v += 4 )
		AddPointToBounds( v, surf->mins, surf->maxs );
	VectorSubtract( surf->mins, ebbox, surf->mins );
	VectorAdd( surf->maxs, ebbox, surf->maxs );
}

/*
=================
Mod_LoadTexture
=================
*/
texture_t *Mod_LoadTexture( mip_t *mt )
{
	texture_t	*tx;

	Com_Assert( mt == NULL );

	if( mt->offsets[0] > 0 )
	{
		// NOTE: imagelib detect miptex version by size
		// 770 additional bytes is indicated custom palette
		int size = (int)sizeof( mip_t ) + ((mt->width * mt->height * 85)>>6);
		if( cached.version == HLBSP_VERSION ) size += sizeof( short ) + 768;

		// loading internal texture if present
		tx = R_FindTexture( va( "\"#%s.mip\"", mt->name ), (byte *)mt, size, 0 );
	}
	else
	{
		// okay, loading it from wad
		tx = R_FindTexture( va( "\"%s.mip\"", mt->name ), NULL, 0, 0 );
	}

	// apply emo-texture if missing :)
	if( !tx ) tx = tr.defaultTexture;

	if( tx->srcFlags & IMAGE_HAS_LUMA )
		Msg( "Texture %s has luma\n", tx->name );

	R_ShaderAddStageTexture( tx );

	return tx;
}

/*
=================
Mod_LoadCachedImage
=================
*/
static ref_shader_t *Mod_LoadCachedImage( cachedimage_t *image )
{
	mip_t	*mt = image->base;
	int	i, shader_type = SHADER_TEXTURE;
	texture_t	*tx;

	// see if already loaded
	if( image->shader )
		return image->shader;

	Com_Assert( mt == NULL );

	com.strncpy( image->name, mt->name, sizeof( image->name ));

	if( R_ShaderCheckCache( image->name ))
	{
		R_SetInternalTexture( mt );	// support map $default
		goto load_shader;	// external shader found
	}
	else R_SetInternalTexture( NULL );

	// build the unique shadername because we don't want keep this for other maps
#if 0
	if( mt->offsets[0] > 0 && com.strncmp( mt->name, "sky", 3 ))
		com.snprintf( image->name, sizeof( image->name ), "%s/%s", cached.modelname, mt->name );
#endif

	// determine shader parms by texturename
	if( !com.strncmp( mt->name, "scroll", 6 ))
		R_ShaderSetMiptexFlags( MIPTEX_CONVEYOR );

	if( mt->name[0] == '*' || mt->name[0] == '!' )
		R_ShaderSetMiptexFlags( MIPTEX_WARPSURFACE|MIPTEX_NOLIGHTMAP );

	if( image->animated )
	{
		float	fps = ( cached.version == HLBSP_VERSION ) ? 10.0f : 5.0f;

		R_SetAnimFrequency( fps );	// set primary animation
		for( i = 0; i < image->anim_total[0]; i++ )
			tx = Mod_LoadTexture( image->anim_frames[0][i] );

		R_SetAnimFrequency( fps );	// set alternate animation
		for( i = 0; i < image->anim_total[1]; i++ )
			tx = Mod_LoadTexture( image->anim_frames[1][i] );
	}
	else tx = Mod_LoadTexture( mt );	// load the base image

	// force to get it from texture
	if( tx == tr.defaultTexture )
		image->width = image->height = -1;

load_shader:
	if( !com.strncmp( mt->name, "sky", 3 ) && cached.version == Q1BSP_VERSION )
		shader_type = SHADER_SKY;

	image->shader = R_LoadShader( image->name, shader_type, false, 0, SHADER_INVALID );

	return image->shader;
}

/*
=================
Mod_LoadTextures
=================
*/
static void Mod_LoadTextures( const dlump_t *l )
{
	dmiptexlump_t	*in;
	cachedimage_t	*out, *tx1, *tx2, *anims[10], *altanims[10];
	cvar_t		*scr_loading = Cvar_Get( "scr_loading", "0", 0, "loading bar progress" );
	int		i, j, k, num, max, altmax, count;
	bool		incomplete;
	mip_t		*mt;

	if( !l->filelen )
	{
		loadmodel->numshaders = 0;
		return;
	}
	
	in = (void *)(mod_base + l->fileofs);
	count = LittleLong( in->nummiptex );

	cached.textures = Mem_Alloc( cached_mempool, count * sizeof( *out ));

	loadmodel->shaders = Mod_Malloc( loadmodel, count * sizeof( ref_shader_t* ));
	loadmodel->numshaders = count;

	out = cached.textures;
	cached.numtextures = count;

	for( i = 0; i < count; i++, out++ )
	{
		in->dataofs[i] = LittleLong( in->dataofs[i] );

		if( in->dataofs[i] == -1 )
		{
			out->shader = tr.defaultShader;
			out->width = out->height = -1;
			continue; // texture is completely missing
		}

		mt = (mip_t *)((byte *)in + in->dataofs[i] );

		if( !mt->name[0] )
		{
			MsgDev( D_WARN, "unnamed texture in %s\n", loadmodel->name );
			com.snprintf( mt->name, sizeof( mt->name ), "*MIPTEX%i", i );
		}

		// convert to lowercase
		com.strnlwr( mt->name, mt->name, sizeof( mt->name ));
		com.strncpy( out->name, mt->name, sizeof( out->name ));

		// original dimensions for adjust lightmap on a face
		out->width = LittleLong( mt->width );
		out->height = LittleLong( mt->height );
		out->base = mt;

		// sky must be loading first
		if( !com.strncmp( mt->name, "sky", 3 ))
			Mod_LoadCachedImage( out );
	}

	// sequence the animations
	for( i = 0; i < count; i++ )
	{
		tx1 = cached.textures + i;

		if( tx1->name[0] != '+' || tx1->name[1] == 0 || tx1->name[2] == 0 )
			continue;

		if( tx1->anim_total[0] || tx1->anim_total[1] )
			continue;	// already sequenced

		// find the number of frames in the animation
		Mem_Set( anims, 0, sizeof( anims ));
		Mem_Set( altanims, 0, sizeof( altanims ));

		for( j = i; j < count; j++ )
		{
			tx2 = cached.textures + j;
			if( tx2->name[0] != '+' || com.strcmp( tx2->name + 2, tx1->name + 2 ))
				continue;

			num = tx2->name[1];
			if( num >= '0' && num <= '9' )
				anims[num - '0'] = tx2;
			else if( num >= 'a' && num <= 'j' )
				altanims[num - 'a'] = tx2;
			else MsgDev( D_WARN, "bad animating texture %s\n", tx1->name );
		}

		max = altmax = 0;
		for( j = 0; j < 10; j++ )
		{
			if( anims[j] ) max = j + 1;
			if( altanims[j] ) altmax = j + 1;
		}

		MsgDev( D_LOAD, "linking animation %s ( %i:%i frames )\n", tx1->name, max, altmax );

		incomplete = false;
		for( j = 0; j < max; j++ )
		{
			if( !anims[j] )
			{
				MsgDev( D_WARN, "missing frame %i of %s\n", j, tx1->name );
				incomplete = true;
			}
		}

		for( j = 0; j < altmax; j++ )
		{
			if( !altanims[j] )
			{
				MsgDev( D_WARN, "missing altframe %i of %s\n", j, tx1->name );
				incomplete = true;
			}
		}

		// bad animchain
		if( incomplete ) continue;

		if( altmax < 1 )
		{
			// if there is no alternate animation, duplicate the primary
			// animation into the alternate
			altmax = max;
			for( k = 0; k < 10; k++ )
				altanims[k] = anims[k];
		}

		// link together the primary animation
		for( j = 0; j < max; j++ )
		{
			tx2 = anims[j];
			tx2->animated = true;
			tx2->anim_total[0] = max;
			tx2->anim_total[1] = altmax;

			for( k = 0; k < 10; k++ )
			{
				tx2->anim_frames[0][k] = (anims[k]) ? anims[k]->base : NULL;
				tx2->anim_frames[1][k] = (altanims[k]) ? altanims[k]->base : NULL;
			}
		}

		// if there really is an alternate anim...
		if( anims[0] != altanims[0] )
		{
			// link together the alternate animation
			for( j = 0; j < altmax; j++ )
			{
				tx2 = altanims[j];
				tx2->animated = true;

				// the primary/alternate are reversed here
				tx2->anim_total[0] = altmax;
				tx2->anim_total[1] = max;

				for( k = 0; k < 10; k++ )
				{
					tx2->anim_frames[0][k] = (altanims[k]) ? altanims[k]->base : NULL;
					tx2->anim_frames[1][k] = (anims[k]) ? anims[k]->base : NULL;
				}
			}
		}
	}

	// load single frames and sequence groups
	for( i = 0; i < count; i++ )
	{
		out = cached.textures + i;

//		out->contents = Mod_ContentsFromShader( out->name );	// FIXME: implement
		loadmodel->shaders[i] = Mod_LoadCachedImage( out );

		Cvar_SetValue( "scr_loading", scr_loading->value + 50.0f / count );
		if( ri.UpdateScreen ) ri.UpdateScreen();
	}
}

/*
=================
Mod_LoadLighting
=================
*/
static void Mod_LoadLighting( const dlump_t *l )
{
	byte	d, *in, *out;
	int	i;

	if( !l->filelen ) return;
	in = (mod_base + l->fileofs);

	Mod_CheckDeluxemaps( l, in );

	switch( cached.version )
	{
	case Q1BSP_VERSION:
		// expand the white lighting data
		loadbmodel->lightdata = Mod_Malloc( loadmodel, l->filelen * 3 );
		out = loadbmodel->lightdata;

		for( i = 0; i < l->filelen; i++ )
		{
			d = *in++;
			*out++ = d;
			*out++ = d;
			*out++ = d;
		}
		break;
	case HLBSP_VERSION:
		// load colored lighting
		loadbmodel->lightdata = Mod_Malloc( loadmodel, l->filelen );
		Mem_Copy( loadbmodel->lightdata, in, l->filelen );
		break;
	}
}

/*
=================
Mod_LoadVertexes
=================
*/
static void Mod_LoadVertexes( const dlump_t *l )
{
	dvertex_t	*in;
	float	*out;
	int	i, j, count;

	in = (void *)( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Host_Error( "Mod_LoadVertexes: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof( *in );

	cached.numvertexes = count;
	out = (float *)cached.vertexes = Mem_Alloc( cached_mempool, count * sizeof( vec3_t ));

	for( i = 0; i < count; i++, in++, out += 3 )
	{
		for( j = 0; j < 3; j++ )
			out[j] = LittleFloat( in->point[j] );
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
static void Mod_LoadSubmodels( const dlump_t *l )
{
	int		i, j, count;
	dmodel_t		*in;
	mmodel_t		*out;
	mbrushmodel_t	*bmodel;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Host_Error( "Mod_LoadSubmodels: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ));

	r_inlinemodels = Mod_Malloc( loadmodel, count * ( sizeof( *r_inlinemodels ) + sizeof( *bmodel )));
	loadmodel->extradata = bmodel = (mbrushmodel_t *)((byte*)r_inlinemodels + count * sizeof( *r_inlinemodels ));

	loadbmodel = bmodel;
	loadbmodel->submodels = out;
	loadbmodel->numsubmodels = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		r_inlinemodels[i].extradata = bmodel + i;

		for( j = 0; j < 3; j++ )
		{
			out->mins[j] = LittleFloat( in->mins[j] );
			out->maxs[j] = LittleFloat( in->maxs[j] );
			out->origin[j] = LittleFloat( in->origin[j] );
		}

		out->radius = RadiusFromBounds( out->mins, out->maxs );
		out->firstnode = LittleLong( in->headnode[0] );	// drawing hull #0
		out->firstface = LittleLong( in->firstface );
		out->numfaces = LittleLong( in->numfaces );
		out->visleafs = LittleLong( in->visleafs );
	}
}

/*
=================
Mod_LoadTexInfo
=================
*/
static void Mod_LoadTexInfo( const dlump_t *l )
{
	dtexinfo_t	*in;
	mtexinfo_t	*out;
	int		miptex;
	int		i, j, count;
	uint		surfaceParm = 0;

	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadTexInfo: funny lump size in %s\n", loadmodel->name );

	count = l->filelen / sizeof( *in );
          out = Mod_Malloc( loadmodel, count * sizeof( *out ));
	
	loadbmodel->texinfo = out;
	loadbmodel->numtexinfo = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		for( j = 0; j < 8; j++ )
			out->vecs[0][j] = LittleFloat( in->vecs[0][j] );

		miptex = LittleLong( in->miptex );
		if( miptex < 0 || miptex > loadmodel->numshaders )
			Host_Error( "Mod_LoadTexInfo: bad shader number in '%s'\n", loadmodel->name );
		out->texturenum = miptex;

		// also copy additional info from cachedinfo
		out->width = cached.textures[miptex].width;
		out->height = cached.textures[miptex].height;
	}
}

/*
=================
Mod_LoadSurfaces
=================
*/
static void Mod_LoadSurfaces( const dlump_t *l )
{
	dface_t		*in;
	msurface_t	*out;
	cachedimage_t	*texture;
	size_t		lightofs;
	int		i, texnum, count;

	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( dface_t ))
		Host_Error( "R_LoadFaces: funny lump size in '%s'\n", loadmodel->name );
	count = l->filelen / sizeof( dface_t );

	loadbmodel->numsurfaces = count;
	loadbmodel->surfaces = Mod_Malloc( loadmodel, count * sizeof( msurface_t ));
	out = loadbmodel->surfaces;

	R_BeginBuildingLightmaps();

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->firstedge = LittleLong( in->firstedge );
		out->numedges = LittleLong( in->numedges );

		if( LittleShort( in->side )) out->flags |= SURF_PLANEBACK;
		out->plane = loadbmodel->planes + LittleLong( in->planenum );
		out->texinfo = loadbmodel->texinfo + LittleLong( in->texinfo );
		texnum = out->texinfo->texturenum;

		if( texnum < 0 || texnum > cached.numtextures )
			Host_Error( "Mod_LoadFaces: bad texture number in '%s'\n", loadmodel->name );

		texture = &cached.textures[texnum];
		out->shader = texture->shader;
		out->fog = NULL;	// FIXME: build conception of realtime fogs

		if( !com.strncmp( texture->name, "sky", 3 ))
			out->flags |= (SURF_DRAWSKY|SURF_DRAWTILED);

		if( texture->name[0] == '*' || texture->name[0] == '!' )
			out->flags |= (SURF_DRAWTURB|SURF_DRAWTILED);

		Mod_CalcSurfaceBounds( out );
		Mod_CalcSurfaceExtents( out );

		// lighting info
		out->lmWidth = (out->extents[0] >> 4) + 1;
		out->lmHeight = (out->extents[1] >> 4) + 1;

		if( out->flags & SURF_DRAWTILED ) lightofs = -1;
		else lightofs = LittleLong( in->lightofs );

		if( loadbmodel->lightdata && lightofs != -1 )
		{
			if( cached.version == HLBSP_VERSION )
				out->samples = loadbmodel->lightdata + lightofs;
			else out->samples = loadbmodel->lightdata + (lightofs * 3);
		}

		while( out->numstyles < LM_STYLES && in->styles[out->numstyles] != 255 )
		{
			out->styles[out->numstyles] = in->styles[out->numstyles];
			out->numstyles++;
		}

		if( !tr.currentSkyShader && ( out->flags & SURF_DRAWSKY || out->shader->flags & SHADER_SKYPARMS ))
		{
			// because sky shader may missing skyParms, but always has surfaceparm 'sky'
			tr.currentSkyShader = out->shader;
		}

		// create lightmap
		R_BuildSurfaceLightmap( out );

		// create polygons
		Mod_BuildSurfacePolygons( out );

		// create unique lightstyle for right sorting surfaces
		out->superLightStyle = R_AddSuperLightStyle( out->lmNum, out->styles );
	}
	R_EndBuildingLightmaps();
}

/*
=================
Mod_LoadMarkFaces
=================
*/
static void Mod_LoadMarkFaces( const dlump_t *l )
{
	dmarkface_t	*in;
	int		i, j, count;

	in = (void *)( mod_base + l->fileofs );	
	if( l->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadMarkFaces: funny lump size in %s\n", loadmodel->name );

	count = l->filelen / sizeof( *in );
	loadbmodel->marksurfaces = Mod_Malloc( loadmodel, count * sizeof( msurface_t* ));

	for( i = 0; i < count; i++ )
	{
		j = LittleLong( in[i] );
		if( j < 0 ||  j >= count )
			Host_Error( "Mod_LoadMarkFaces: bad surface number in '%s'\n", loadmodel->name );
		loadbmodel->marksurfaces[i] = loadbmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadNodes
=================
*/
static void Mod_LoadNodes( const dlump_t *l )
{
	int	i, j, count, p;
	dnode_t	*in;
	mnode_t	*out;

	in = (void *)( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Host_Error( "Mod_LoadNodes: funny lump size in %s\n", loadmodel->name );

	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ));

	loadbmodel->nodes = out;
	loadbmodel->numnodes = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		bool	badBounds = false;

		out->plane = loadbmodel->planes + LittleLong( in->planenum );
		out->firstface = loadbmodel->surfaces + LittleLong( in->firstface );
		out->numfaces = LittleLong( in->numfaces );
		out->contents = CONTENTS_NODE;

		for( j = 0; j < 3; j++ )
		{
			out->mins[j] = (float)LittleShort( in->mins[j] );
			out->maxs[j] = (float)LittleShort( in->maxs[j] );
			if( out->mins[j] > out->maxs[j] ) badBounds = true;
		}

		if( !badBounds && VectorCompare( out->mins, out->maxs ))
			badBounds = true;

		if( badBounds )
		{
			MsgDev( D_WARN, "bad node %i bounds:\n", i );
			MsgDev( D_WARN, "mins: %i %i %i\n", Q_rint( out->mins[0] ), Q_rint( out->mins[1] ), Q_rint( out->mins[2] ));
			MsgDev( D_WARN, "maxs: %i %i %i\n", Q_rint( out->maxs[0] ), Q_rint( out->maxs[1] ), Q_rint( out->maxs[2] ));
		}

		for( j = 0; j < 2; j++ )
		{
			p = LittleShort( in->children[j] );
			if( p >= 0 ) out->children[j] = loadbmodel->nodes + p;
			else out->children[j] = (mnode_t *)(loadbmodel->leafs + ( -1 - p ));
		}
	}

	Mod_SetNodeParent( loadbmodel->nodes, NULL );
}

/*
=================
Mod_LoadLeafs
=================
*/
static void Mod_LoadLeafs( const dlump_t *l )
{
	dleaf_t	*in;
	mleaf_t	*out;
	int	i, j, p, count;

	in = (void *)( mod_base + l->fileofs );	
	if( l->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadLeafs: funny lump size in %s\n", loadmodel->name );

	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ));

	loadbmodel->numleafs = count;
	loadbmodel->leafs = out;

	for( i = 0; i < count; i++, in++, out++ )
	{
		bool	badBounds = false;

		for( j = 0; j < 3; j++ )
		{
			out->mins[j] = (float)LittleShort( in->mins[j] );
			out->maxs[j] = (float)LittleShort( in->maxs[j] );
			if( out->mins[j] > out->maxs[j] ) badBounds = true;
		}

		if( !badBounds && VectorCompare( out->mins, out->maxs ))
			badBounds = true;

		if(( i > 0 ) && badBounds )
		{
			MsgDev( D_NOTE, "bad leaf %i bounds:\n", i );
			MsgDev( D_NOTE, "mins: %i %i %i\n", Q_rint( out->mins[0] ), Q_rint( out->mins[1] ), Q_rint( out->mins[2] ));
			MsgDev( D_NOTE, "maxs: %i %i %i\n", Q_rint( out->maxs[0] ), Q_rint( out->maxs[1] ), Q_rint( out->maxs[2] ));
		}

		out->plane = NULL;	// to differentiate from nodes
		out->contents = LittleLong( in->contents );
		
		p = LittleLong( in->visofs );
		out->compressed_vis = (p == -1) ? NULL : loadbmodel->visdata + p;

		out->firstMarkSurface = loadbmodel->marksurfaces + LittleShort( in->firstmarksurface );
		out->numMarkSurfaces = LittleShort( in->nummarksurfaces );
	}
}

/*
=================
Mod_LoadEdges
=================
*/
static void Mod_LoadEdges( const dlump_t *l )
{
	dedge_t	*in, *out;
	int	i, count;

	in = (void *)( mod_base + l->fileofs );	
	if( l->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadEdges: funny lump size in %s\n", loadmodel->name );

	count = l->filelen / sizeof( dedge_t );
	cached.edges = out = Mem_Alloc( cached_mempool, count * sizeof( dedge_t ));
	cached.numedges = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->v[0] = (word)LittleShort( in->v[0] );
		out->v[1] = (word)LittleShort( in->v[1] );
	}
}

/*
=================
Mod_LoadSurfEdges
=================
*/
static void Mod_LoadSurfEdges( const dlump_t *l )
{
	dsurfedge_t	*in, *out;
	int		i, count;

	in = (void *)( mod_base + l->fileofs );	
	if( l->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadSurfEdges: funny lump size in %s\n", loadmodel->name );

	count = l->filelen / sizeof( dsurfedge_t );
	cached.surfedges = out = Mem_Alloc( cached_mempool, count * sizeof( dsurfedge_t ));
	cached.numsurfedges = count;

	for( i = 0; i < count; i++ )
		out[i] = LittleLong( in[i] );
}

/*
=================
Mod_LoadPlanes
=================
*/
static void Mod_LoadPlanes( const dlump_t *l )
{
	cplane_t	*out;
	dplane_t	*in;
	int	i, count;

	in = (void *)( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadPlanes: funny lump size in %s\n", loadmodel->name );

	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ));

	loadbmodel->planes = out;
	loadbmodel->numplanes = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->normal[0] = LittleFloat( in->normal[0] );
		out->normal[1] = LittleFloat( in->normal[1] );
		out->normal[2] = LittleFloat( in->normal[2] );
		out->signbits = SignbitsForPlane( out->normal );
		out->dist = LittleFloat( in->dist );
		out->type = LittleLong( in->type );
	}
}

/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility( dlump_t *l )
{
	if( !l->filelen )
	{
		loadbmodel->visdata = NULL;
		return;
	}

	loadbmodel->visdata = Mod_Malloc( loadmodel, l->filelen );
	Mem_Copy( loadbmodel->visdata, (void *)(mod_base + l->fileofs), l->filelen );
}

void StripTrailing( char *e )
{
	char	*s;

	s = e + com.strlen( e ) - 1;
	while( s >= e && *s <= 32 )
	{
		*s = 0;
		s--;
	}
}

/*
================
ValueForKey

gets the value for an entity key
================
*/
const char *ValueForKey( const mapent_t *ent, const char *key )
{
	epair_t	*ep;

	if( !ent ) return "";
	
	for( ep = ent->epairs; ep != NULL; ep = ep->next )
	{
		if( !com.strcmp( ep->key, key ))
			return ep->value;
	}
	return "";
}

/*
================
IntForKey

gets the integer point value for an entity key
================
*/
int IntForKey( const mapent_t *ent, const char *key )
{
	return com.atoi( ValueForKey( ent, key ));
}

/*
================
FloatForKey

gets the floating point value for an entity key
================
*/
float FloatForKey( const mapent_t *ent, const char *key )
{
	return com.atof( ValueForKey( ent, key ));
}

/*
================
GetVectorForKey

gets a 3-element vector value for an entity key
================
*/
void GetVectorForKey( const mapent_t *ent, const char *key, vec3_t vec )
{
	const char	*k;
	double		v1, v2, v3;

	k = ValueForKey( ent, key );
	
	// scanf into doubles, then assign, so it is vec_t size independent
	v1 = v2 = v3 = 0.0;
	sscanf( k, "%lf %lf %lf", &v1, &v2, &v3 );
	VectorSet( vec, v1, v2, v3 );
}

/*
================
FindTargetEntity

finds an entity target
================
*/
mapent_t *FindTargetEntity( const char *target )
{
	int		i;
	const char	*n;
	
	for( i = 0; i < cached.numents; i++ )
	{
		n = ValueForKey( &cached.entities[i], "targetname" );
		if( !com.strcmp( n, target )) return &cached.entities[i];
	}
	return NULL;
}

/*
=================
Mod_ParseEpair

parses a single quoted "key" "value" pair into an epair struct
=================
*/
static epair_t *Mod_ParseEpair( script_t *script, token_t *token )
{
	epair_t	*e;

	e = Mem_Alloc( cached_mempool, sizeof( epair_t ));
	
	if( com.strlen( token->string ) >= MAX_KEY - 1 )
		Host_Error( "Mod_ParseEpair: key %s too long\n", token->string );
	e->key = com.stralloc( cached_mempool, token->string, __FILE__, __LINE__ );

	Com_ReadToken( script, SC_PARSE_GENERIC, token );
	if( com.strlen( token->string ) >= MAX_VALUE - 1 )
		Host_Error( "Mod_ParseEpair: value %s too long\n", token->string );
	e->value = com.stralloc( cached_mempool, token->string, __FILE__, __LINE__ );

	// strip trailing spaces if needs
	StripTrailing( e->key );
	StripTrailing( e->value );

	return e;
}

/*
================
ParseEntity

parses an entity's epairs
================
*/
static bool Mod_ParseEntity( void )
{
	epair_t	*pair;
	token_t	token;
	mapent_t	*mapEnt;

	if( !Com_ReadToken( cached.entscript, SC_ALLOW_NEWLINES, &token ))
		return false;

	if( com.stricmp( token.string, "{" )) Host_Error( "Mod_ParseEntity: '{' not found\n" );
	if( cached.numents == GI->max_edicts ) return false; // probably stupid user set wrong limit

	mapEnt = &cached.entities[cached.numents];
	cached.numents++;

	while( 1 )
	{
		if( !Com_ReadToken( cached.entscript, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token ))
			Host_Error( "Mod_ParseEntity: EOF without closing brace\n" );
		if( !com.stricmp( token.string, "}" )) break;
		pair = Mod_ParseEpair( cached.entscript, &token );
		pair->next = mapEnt->epairs;
		mapEnt->epairs = pair;
	}
	return true;
}

/*
=================
Mod_LoadEntities
=================
*/
static void Mod_LoadEntities( const dlump_t *l, vec3_t ambient )
{
	float		ambientScale = 0.0f;
	const char	*name, *target;
	mapent_t		*e, *e2;
	float		angle;
	vec3_t		dest;
	light_t		*dl;
	int		i;

	cached.entscript = Com_OpenScript( "entities", (char *)mod_base + l->fileofs, l->filelen );
	if( !cached.entscript ) return;

	cached.entities = Mem_Alloc( cached_mempool, sizeof( mapent_t ) * GI->max_edicts );
	cached.numents = 0;

	// read all the ents
	while( Mod_ParseEntity( ));
	Com_CloseScript( cached.entscript );
	cached.entscript = NULL;

	VectorClear( ambient );

	for( i = 0; i < cached.numents; i++ )
	{
		const char	*pLight;
		double		r, g, b, scaler;
		int		argCnt;

		e = &cached.entities[i];

		name = ValueForKey( e, "classname" );
		if( !com.strncmp( name, "worldspawn", 10 ))
		{
			GetVectorForKey( e, "_color", ambient );
			ambientScale = FloatForKey( e, "_ambient" );

			if( VectorIsNull( ambient )) VectorSet( ambient, 1.0f, 1.0f, 1.0f );
			if( ambientScale > 0.0f ) VectorScale( ambient, ambientScale, ambient );
		}

		name = ValueForKey( e, "classname" );
		if( com.strncmp( name, "light", 5 ))
			continue;

		cached.numPointLights++;
		dl = Mem_Alloc( cached_mempool, sizeof( light_t ));

		GetVectorForKey( e, "origin", dl->origin );

		dl->next = cached.lights;
		cached.lights = dl;

		dl->style = IntForKey( e, "style" );

		pLight = ValueForKey( e, "_light" );
		if( !*pLight )
		{
			pLight = ValueForKey( e, "light" );
			dl->photons = com.atof( pLight );
			argCnt = 1;
		}
		else
		{
			// scanf into doubles, then assign, so it is vec_t size independent
			r = g = b = scaler = 0;
			argCnt = sscanf( pLight, "%lf %lf %lf %lf", &r, &g, &b, &scaler );
			dl->color[0] = (float)r;
		}

		if( argCnt == 1 )
		{
			// The R,G,B values are all equal.
			dl->color[0] = dl->color[1] = dl->color[2] = 1.0f;
		}
		else if( argCnt == 3 || argCnt == 4 )
		{
			// save the other two G,B values.
			dl->color[0] = (float)r;
			dl->color[1] = (float)g;
			dl->color[2] = (float)b;

			// did we also get an "intensity" scaler value too?
			if( argCnt == 4 )
			{
				// scale the normalized 0-255 R,G,B values by the intensity scaler
//				dl->color[0] = dl->color[0] / 255 * (float)scaler;
//				dl->color[1] = dl->color[1] / 255 * (float)scaler;
//				dl->color[2] = dl->color[2] / 255 * (float)scaler;
				dl->photons = scaler;
			}
		}
		else
		{
			MsgDev( D_WARN, "entity at (%f,%f,%f) has bad '_light' value : '%s'\n", 
					dl->origin[0], dl->origin[1], dl->origin[2], pLight );
			continue;
		}

		if( !dl->photons ) dl->photons = 300; 
		ColorNormalize( dl->color, dl->color );

		target = ValueForKey (e, "target");

		if( !com.strcmp( name, "light_spot" ) || !com.strcmp( name, "light_environment" ) || target[0] )
		{
			if( !VectorAvg( dl->color ))
				VectorSet( dl->color, 500, 500, 500 );

			dl->type = emit_spotlight;
			dl->stopdot = FloatForKey( e, "_cone" );
			if( !dl->stopdot ) dl->stopdot = 10;
			dl->stopdot2 = FloatForKey( e, "_cone2" );
			if( !dl->stopdot2 ) dl->stopdot2 = dl->stopdot;
			if( dl->stopdot2 < dl->stopdot ) dl->stopdot2 = dl->stopdot;
			dl->stopdot2 = (float)com.cos( dl->stopdot2 / 180 * M_PI );
			dl->stopdot = (float)com.cos( dl->stopdot / 180 * M_PI );

			if( target[0] )
			{
				// point towards target
				e2 = FindTargetEntity( target );
				if( !e2 )
				{
					MsgDev( D_WARN, "light at (%i %i %i) has missing target\n",
					(int)dl->origin[0], (int)dl->origin[1], (int)dl->origin[2] );
				}
				else
				{
					GetVectorForKey( e2, "origin", dest );
					VectorSubtract( dest, dl->origin, dl->normal );
					VectorNormalize( dl->normal );
				}
			}
			else
			{	// point down angle
				vec3_t	vAngles;

				GetVectorForKey( e, "angles", vAngles );

				angle = IntForKey( e, "angle" );

				if( angle == ANGLE_UP )
				{
					VectorSet( dl->normal, 0.0f, 0.0f, 1.0f );
				}
				else if( angle == ANGLE_DOWN )
				{
					VectorSet( dl->normal, 0.0f, 0.0f, -1.0f );
				}
				else
				{
					// if we don't have a specific "angle" use the "angles" YAW
					if( !angle ) angle = vAngles[1];

					dl->normal[2] = 0;
					dl->normal[0] = (float)com.cos( angle / 180 * M_PI );
					dl->normal[1] = (float)com.sin( angle / 180 * M_PI );
				}

				angle = FloatForKey( e, "pitch" );

				// if we don't have a specific "pitch" use the "angles" PITCH
				if( !angle ) angle = vAngles[0];

				dl->normal[2]  = (float)com.sin( angle / 180 * M_PI );
				dl->normal[0] *= (float)com.cos( angle / 180 * M_PI );
				dl->normal[1] *= (float)com.cos( angle / 180 * M_PI );
			}

			if( IntForKey( e, "_sky" ) || !com.strcmp( name, "light_environment" )) 
			{
				dl->type = emit_skylight;
				dl->stopdot2 = FloatForKey( e, "_sky" ); // hack stopdot2 to a sky key number
			}
		}
		else
		{
			if( !VectorAvg( dl->color ))
				VectorSet( dl->color, 1.0f, 1.0f, 1.0f );
			dl->type = emit_point;
		}

		dl->photons *= 7500;
/*
		if( dl->type != emit_skylight )
		{
			float	l1 = max( dl->intensity[0], max( dl->intensity[1], dl->intensity[2] ));
			l1 = l1 * l1 / 10;

			dl->intensity[0] *= l1;
			dl->intensity[1] *= l1;
			dl->intensity[2] *= l1;
		}
*/
	}
}

/*
=============================================================================

 LIGHTGRID CALCULATING

=============================================================================
*/
static bool R_PointInSolid( const vec3_t p )
{
	return (Mod_PointInLeaf( p, loadmodel )->contents == CONTENTS_SOLID);
}

/*
================
R_PointToPolygonFormFactor
================
*/
float R_PointToPolygonFormFactor( const vec3_t point, const vec3_t normal, const winding_t *w )
{
	float	total;
	vec3_t	dirs[64];
	vec3_t	triVector, triNormal;
	float	dot, angle, facing;
	int	i, j;
	
	for( i = 0; i < w->numpoints; i++ )
	{
		VectorSubtract( w->points[i], point, dirs[i] );
		VectorNormalize( dirs[i] );
	}

	// duplicate first vertex to avoid mod operation
	VectorCopy( dirs[0], dirs[i] );

	total = 0;
	for( i = 0 ; i < w->numpoints; i++ )
	{
		j = i + 1;
		dot = DotProduct( dirs[i], dirs[j] );

		// roundoff can cause slight creep, which gives an IND from acos
		dot = bound( -1.0f, dot, 1.0f );
		
		angle = com.acos( dot );
		CrossProduct( dirs[i], dirs[j], triVector );
		if( VectorNormalizeLength2( triVector, triNormal ) < 0.0001f )
			continue;

		facing = DotProduct( normal, triNormal );
		total += facing * angle;

		if( total > 6.3f || total < -6.3f )
			return 0;
	}

	total /= M_PI2;	// now in the range of 0 to 1 over the entire incoming hemisphere

	return total;
}

/*
========================
R_LightContributionToPoint
========================
*/
bool R_LightContributionToPoint( const light_t *light, const vec3_t origin, vec3_t color )
{
	trace_t	trace;
	float	add;

	add = 0;

	VectorClear( color );

	// testing exact PTPFF
	if( light->type == emit_surface )
	{
		float	d, factor;
		vec3_t	normal;

		// see if the point is behind the light
		d = DotProduct( origin, light->normal ) - light->dist;
		if( d < 1 ) return false; // point is behind light

		// test occlusion
		// clip the line, tracing from the surface towards the light
		R_TraceLine( &trace, origin, light->origin );
		if( trace.flFraction != 1.0f ) return false;

		// calculate the contribution
		VectorSubtract( light->origin, origin, normal );
		if( VectorNormalizeLength( normal ) == 0 )
			return false;

		factor = R_PointToPolygonFormFactor( origin, normal, light->w );
		if( factor <= 0 ) return false;

		// FIXME: we needs to rescale color with factor ?
		VectorScale( light->color, factor, color );
		return true;
	}

	// calculate the amount of light at this sample
	if( light->type == emit_point || light->type == emit_spotlight )
	{
		vec3_t	dir;
		float	dist;

		VectorSubtract( light->origin, origin, dir );
		dist = VectorLength( dir );

		// clamp the distance to prevent super hot spots
		if( dist < 16 ) dist = 16;

		add = light->photons * (1.0 / 8000) - dist;

		add = light->photons / ( dist * dist );
//		add = light->color[0] + light->color[1] + light->color[2];
	}
	else
	{
		return false;
	}

	if( add <= 1.0f )
	{
		return false;
	}

	// clip the line, tracing from the surface towards the light
	if( R_TraceLine( &trace, origin, light->origin ))
		return false;

	// other light rays must not hit anything
	if( trace.flFraction != 1.0f )
		return false;

	// add the result
	VectorScale( light->color, add, color );

	return true;
}

static void R_TraceGrid( int num )
{
	int		x, y, z;
	vec3_t		origin;
	light_t		*light;
	vec3_t		summedDir;
	vec3_t		ambientColor[LM_STYLES];
	vec3_t		directedColor[LM_STYLES];
	contribution_t	contributions[1024];
	int		i, j, mod, numCon, numStyles;
	mgridlight_t	*gp;

	mod = num;
	z = mod / ( loadbmodel->gridBounds[0] * loadbmodel->gridBounds[1] );
	mod -= z * ( loadbmodel->gridBounds[0] * loadbmodel->gridBounds[1] );

	y = mod / loadbmodel->gridBounds[0];
	mod -= y * loadbmodel->gridBounds[0];

	x = mod;

	origin[0] = loadbmodel->gridMins[0] + x * loadbmodel->gridSize[0];
	origin[1] = loadbmodel->gridMins[1] + y * loadbmodel->gridSize[1];
	origin[2] = loadbmodel->gridMins[2] + z * loadbmodel->gridSize[2];

	if( R_PointInSolid( origin ))
	{
		vec3_t	baseOrigin;
		int	step;

		VectorCopy( origin, baseOrigin );

		// try to nudge the origin around to find a valid point
		for( step = 9; step <= 18; step += 9 )
		{
			for( i = 0; i < 8; i++ )
			{
				VectorCopy( baseOrigin, origin );

				if( i & 1 ) origin[0] += step;
				else origin[0] -= step;

				if( i & 2 ) origin[1] += step;
				else origin[1] -= step;

				if( i & 4 ) origin[2] += step;
				else origin[2] -= step;

				if( !R_PointInSolid( origin ))
					break;
			}

			if( i != 8 ) break;
		}

		// can't find a valid point at all
		if( step > 18 ) return;
	}

	VectorClear( summedDir );

	// trace to all the lights

	// find the major light direction, and divide the
	// total light between that along the direction and
	// the remaining in the ambient 
	numCon = 0;
	for( light = cached.lights; light; light = light->next )
	{
		vec3_t	add;
		vec3_t	dir;
		float	addSize;

		if( !R_LightContributionToPoint( light, origin, add ))
			continue;

		VectorSubtract( light->origin, origin, dir );
		VectorNormalize( dir );

		VectorCopy( add, contributions[numCon].color );
		VectorCopy( dir, contributions[numCon].dir );
		contributions[numCon].style = light->style;
		numCon++;

		addSize = VectorLength( add );
		VectorMA( summedDir, addSize, dir, summedDir );

		if( numCon == 1023 )
			break;
	}

	// now that we have identified the primary light direction,
	// go back and seperate all the light into directed and ambient
	VectorNormalize( summedDir );

	for( i = 0; i < LM_STYLES; i++ )
	{
		VectorClear( ambientColor[i] );
		VectorClear( directedColor[i] );
	}

	gp = loadbmodel->lightgrid + num;

	numStyles = 1;
	for( i = 0; i < numCon; i++ )
	{
		float	d;

		d = DotProduct( contributions[i].dir, summedDir );
		if( d < 0.0f ) d = 0.0f;

		// find appropriate style
		for( j = 0; j < numStyles; j++ )
		{
			if( gp->styles[j] == contributions[i].style )
				break;
		}

		// style not found?
		if( j >= numStyles )
		{
			// add a new style
			if( numStyles < LM_STYLES )
			{
				gp->styles[numStyles] = contributions[i].style;
				numStyles++;
			}
			else j = 0;
		}

		VectorMA( directedColor[j], d, contributions[i].color, directedColor[j] );

		// the ambient light will be at 1/4 the value of directed light
		d = 0.25f * ( 1.0f - d );
		VectorMA( ambientColor[j], d, contributions[i].color, ambientColor[j] );
	}

	// store off sample
	for( i = 0; i < LM_STYLES; i++ )
	{
		VectorMA( ambientColor[i], 0.125f, directedColor[i], ambientColor[i] );

		// save the resulting value out
		ColorToBytes( ambientColor[i], gp->ambient[i] );
		ColorToBytes( directedColor[i], gp->diffuse[i] );
	}

	// now do some fudging to keep the ambient from being too low
	VectorNormalize( summedDir );
	NormToLatLong( summedDir, gp->direction );
}

void R_BuildLightGrid( mbrushmodel_t *world )
{
	double	timestart = Sys_DoubleTime();
	int	i;

	MsgDev( D_INFO, "Building LightGrid...\n" );

	// assume lightgrid size as player hull (eg. 64x64x128)
	VectorSet( world->gridSize, 64, 64, 128 );

	for( i = 0; i < 3; i++ )
	{
		vec3_t	maxs;

		world->gridMins[i] = world->gridSize[i] * ceil(( world->submodels[0].mins[i] + 1 ) / world->gridSize[i] );
		maxs[i] = world->gridSize[i] * floor(( world->submodels[0].maxs[i] - 1 ) / world->gridSize[i] );
		world->gridBounds[i] = ( maxs[i] - world->gridMins[i] ) / world->gridSize[i] + 1;
	}

	world->numgridpoints = world->gridBounds[2] * world->gridBounds[1] * world->gridBounds[0];	
	world->gridBounds[3] = world->gridBounds[1] * world->gridBounds[0];
	world->lightgrid = Mod_Malloc( loadmodel, world->numgridpoints * sizeof( mgridlight_t ));
	r_worldent->model = loadmodel; // setup trace world

	for( i = 0; i < world->numgridpoints; i++ )
		R_TraceGrid( i );
 
	Msg( "numGridPoints %i, mem %s\n", world->numgridpoints, memprint( world->numgridpoints * sizeof( mgridlight_t )));
	MsgDev( D_INFO, "LightGrid building time: %g secs\n", Sys_DoubleTime() - timestart );
}

/*
=================
Mod_Finish
=================
*/
static void Mod_Finish( const dlump_t *faces, const dlump_t *light, vec3_t ambient )
{
	// set up lightgrid
//	R_BuildLightGrid( loadbmodel );

	// ambient lighting
	VectorCopy( RI.refdef.skyColor, mapConfig.environmentColor );
	mapConfig.environmentColor[3] = 255;

	Mem_EmptyPool( cached_mempool );
}

/*
=================
Mod_BrushLoadModel
=================
*/
void Mod_BrushLoadModel( ref_model_t *mod, const void *buffer )
{
	dheader_t	*header;
	mmodel_t	*bm;
	vec3_t	ambient;
	int	i, j;

	header = (dheader_t *)buffer;
	cached.version = LittleLong( header->version );

	switch( cached.version )
	{
	case Q1BSP_VERSION:
	case HLBSP_VERSION:
		break;
	default:
		MsgDev( D_ERROR, "Mod_BrushModel: %s has wrong version number (%i should be %i)", loadmodel->name, cached.version, HLBSP_VERSION );
		return;
	}

	mod->type = mod_brush;
	mod_base = (byte *)header;

	// swap all the lumps
	for( i = 0; i < sizeof( dheader_t )/4; i++ )
		((int *)header )[i] = LittleLong( ((int *)header )[i] );

	// load into heap
	Mod_LoadSubmodels( &header->lumps[LUMP_MODELS] );

	if( header->lumps[LUMP_PLANES].filelen % sizeof( dplane_t ))
	{
		// blue-shift swapped lumps
		Mod_LoadEntities( &header->lumps[LUMP_PLANES], ambient );
		Mod_LoadPlanes( &header->lumps[LUMP_ENTITIES] );
	}
	else
	{
		// normal half-life lumps
		Mod_LoadEntities( &header->lumps[LUMP_ENTITIES], ambient );
		Mod_LoadPlanes( &header->lumps[LUMP_PLANES] );
	}

	Mod_LoadVertexes( &header->lumps[LUMP_VERTEXES] );
	Mod_LoadEdges( &header->lumps[LUMP_EDGES] );
	Mod_LoadSurfEdges( &header->lumps[LUMP_SURFEDGES] );
	Mod_LoadTextures( &header->lumps[LUMP_TEXTURES] );
	Mod_LoadLighting( &header->lumps[LUMP_LIGHTING] );
	Mod_LoadVisibility( &header->lumps[LUMP_VISIBILITY] );
	Mod_LoadTexInfo( &header->lumps[LUMP_TEXINFO] );
	Mod_LoadSurfaces( &header->lumps[LUMP_FACES] );
	Mod_LoadMarkFaces( &header->lumps[LUMP_MARKSURFACES] );
	Mod_LoadLeafs( &header->lumps[LUMP_LEAFS] );
	Mod_LoadNodes( &header->lumps[LUMP_NODES] );

	Mod_Finish( &header->lumps[LUMP_FACES], &header->lumps[LUMP_LIGHTING], ambient );
	mod->touchFrame = tr.registration_sequence; // register model

	// set up the submodels
	for( i = 0; i < loadbmodel->numsubmodels; i++ )
	{
		ref_model_t	*starmod;
		mbrushmodel_t	*bmodel;

		bm = &loadbmodel->submodels[i];
		starmod = &r_inlinemodels[i];
		bmodel = (mbrushmodel_t *)starmod->extradata;

		Mem_Copy( starmod, mod, sizeof( ref_model_t ));
		Mem_Copy( bmodel, mod->extradata, sizeof( mbrushmodel_t ));

		bmodel->firstmodelsurface = bmodel->surfaces + bm->firstface;
		bmodel->firstmodelnode = bmodel->nodes + bm->firstnode;
		bmodel->nummodelsurfaces = bm->numfaces;
		bmodel->numleafs = bm->visleafs + 1; // include solid leaf
		starmod->extradata = bmodel;

		VectorCopy( bm->maxs, starmod->maxs );
		VectorCopy( bm->mins, starmod->mins );
		starmod->radius = bm->radius;

		for( j = 0; i != 0 && j < bmodel->nummodelsurfaces; j++ )
		{
			msurface_t *surf = bmodel->firstmodelsurface + j;

			// kill water backplanes for submodels (half-life rules)
			if( surf->flags & SURF_DRAWTURB && surf->mins[2] == bm->mins[2] )
				surf->mesh = NULL;
		}

		if( i == 0 ) *mod = *starmod;
		else bmodel->numsubmodels = 0;
	}
}
//=============================================================================

/*
=================
R_RegisterWorldModel

Specifies the model that will be used as the world
=================
*/
void R_BeginRegistration( const char *mapname )
{
	string	fullname;

	tr.registration_sequence++;
	mapConfig.deluxeMappingEnabled = false;
	VectorClear( mapConfig.ambient );

	com.strncpy( fullname, mapname, MAX_STRING );

	// now replacement table is invalidate
	Mem_Set( cl_models, 0, sizeof( cl_models ));

	// explicitly free the old map if different
	if( com.strcmp( r_models[0].name, fullname ))
	{
		Mod_FreeModel( &r_models[0] );
	}
	else
	{
		// update progress bar
		Cvar_SetValue( "scr_loading", 50.0f );
		if( ri.UpdateScreen ) ri.UpdateScreen();
	}

	R_NewMap ();

	r_farclip_min = Z_NEAR;		// sky shaders will most likely modify this value
	r_environment_color->modified = true;

	r_worldmodel = Mod_ForName( fullname, true );
	r_worldbrushmodel = (mbrushmodel_t *)r_worldmodel->extradata;
	r_worldmodel->type = mod_world;
	
	r_worldent->scale = 1.0f;
	r_worldent->model = r_worldmodel;
	r_worldent->rtype = RT_MODEL;
	r_worldent->ent_type = ED_NORMAL;
	r_worldent->renderamt = 255;		// i'm hope we don't want to see semisolid world :) 
	Matrix3x3_LoadIdentity( r_worldent->axis );
	Mod_UpdateShaders( r_worldmodel );

	r_framecount = r_framecount2 = 1;
	r_oldviewleaf = r_viewleaf = NULL;  // force markleafs
}

void R_EndRegistration( const char *skyname )
{
	int		i;
	ref_model_t	*mod;

	// half-life or quake2 skybox-style
	R_SetupSky( skyname );

	for( i = 0, mod = r_models; i < r_nummodels; i++, mod++ )
	{
		if( !mod->name ) continue;
		if( mod->touchFrame != tr.registration_sequence )
			Mod_FreeModel( mod );
	}

	// purge all unused shaders
	R_ShaderFreeUnused();
}

/*
=================
R_RegisterModel
=================
*/
ref_model_t *R_RegisterModel( const char *name )
{
	ref_model_t	*mod;
	
	mod = Mod_ForName( name, false );
	Mod_UpdateShaders( mod );

	return mod;
}

/*
=================
R_ModelBounds
=================
*/
void R_ModelBounds( const ref_model_t *model, vec3_t mins, vec3_t maxs )
{
	if( model )
	{
		VectorCopy( model->mins, mins );
		VectorCopy( model->maxs, maxs );
	}
	else if( r_worldmodel )
	{
		VectorCopy( r_worldmodel->mins, mins );
		VectorCopy( r_worldmodel->maxs, maxs );
	}
}
