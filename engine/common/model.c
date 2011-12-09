/*
model.c - modelloader
Copyright (C) 2007 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "mod_local.h"
#include "sprite.h"
#include "mathlib.h"
#include "studio.h"
#include "wadfile.h"
#include "world.h"
#include "gl_local.h"

world_static_t	world;

byte		*mod_base;
byte		*com_studiocache;		// cache for submodels
static model_t	*com_models[MAX_MODELS];	// shared replacement modeltable
static model_t	cm_models[MAX_MODELS];
static int	cm_nummodels = 0;
static byte	visdata[MAX_MAP_LEAFS/8];	// intermediate buffer
int		bmodel_version;		// global stuff to detect bsp version
char		modelname[64];		// short model name (without path and ext)
		
model_t		*loadmodel;
model_t		*worldmodel;

// cvars


// default hullmins
static vec3_t cm_hullmins[4] =
{
{ -16, -16, -36 },
{ -16, -16, -18 },
{   0,   0,   0 },
{ -32, -32, -32 },
};

// defualt hummaxs
static vec3_t cm_hullmaxs[4] =
{
{  16,  16,  36 },
{  16,  16,  18 },
{   0,   0,   0 },
{  32,  32,  32 },
};

/*
===============================================================================

			MOD COMMON UTILS

===============================================================================
*/
/*
================
Mod_SetupHulls
================
*/
void Mod_SetupHulls( float mins[4][3], float maxs[4][3] )
{
	Q_memcpy( mins, cm_hullmins, sizeof( cm_hullmins ));
	Q_memcpy( maxs, cm_hullmaxs, sizeof( cm_hullmaxs ));
}

/*
===================
Mod_CompressVis
===================
*/
byte *Mod_CompressVis( const byte *in, size_t *size )
{
	int	j, rep;
	int	visrow;
	byte	*dest_p;

	if( !worldmodel )
	{
		Host_Error( "Mod_CompressVis: no worldmodel\n" );
		return NULL;
	}
	
	dest_p = visdata;
	visrow = (worldmodel->numleafs + 7)>>3;
	
	for( j = 0; j < visrow; j++ )
	{
		*dest_p++ = in[j];
		if( in[j] ) continue;

		rep = 1;
		for( j++; j < visrow; j++ )
		{
			if( in[j] || rep == 255 )
				break;
			else rep++;
		}
		*dest_p++ = rep;
		j--;
	}

	if( size ) *size = dest_p - visdata;

	return visdata;
}

/*
===================
Mod_DecompressVis
===================
*/
byte *Mod_DecompressVis( const byte *in )
{
	int	c, row;
	byte	*out;

	if( !worldmodel )
	{
		Host_Error( "Mod_DecompressVis: no worldmodel\n" );
		return NULL;
	}

	row = (worldmodel->numleafs + 7)>>3;	
	out = visdata;

	if( !in )
	{	
		// no vis info, so make all visible
		while( row )
		{
			*out++ = 0xff;
			row--;
		}
		return visdata;
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
	} while( out - visdata < row );

	return visdata;
}

/*
==================
Mod_PointInLeaf

==================
*/
mleaf_t *Mod_PointInLeaf( const vec3_t p, mnode_t *node )
{
	ASSERT( node != NULL );

	while( 1 )
	{
		if( node->contents < 0 )
			return (mleaf_t *)node;
		node = node->children[PlaneDiff( p, node->plane ) < 0];
	}

	// never reached
	return NULL;
}

/*
==================
Mod_LeafPVS

==================
*/
byte *Mod_LeafPVS( mleaf_t *leaf, model_t *model )
{
	if( !model || !leaf || leaf == model->leafs || !model->visdata )
		return Mod_DecompressVis( NULL );
	return Mod_DecompressVis( leaf->compressed_vis );
}

/*
==================
Mod_LeafPHS

==================
*/
byte *Mod_LeafPHS( mleaf_t *leaf, model_t *model )
{
	if( !model || !leaf || leaf == model->leafs || !model->visdata )
		return Mod_DecompressVis( NULL );
	return Mod_DecompressVis( leaf->compressed_pas );
}

/*
==================
Mod_PointLeafnum

==================
*/
int Mod_PointLeafnum( const vec3_t p )
{
	// map not loaded
	if ( !worldmodel ) return 0;
	return Mod_PointInLeaf( p, worldmodel->nodes ) - worldmodel->leafs;
}

/*
======================================================================

LEAF LISTING

======================================================================
*/
static void Mod_BoxLeafnums_r( leaflist_t *ll, mnode_t *node )
{
	mplane_t	*plane;
	int	s;

	while( 1 )
	{
		if( node->contents == CONTENTS_SOLID )
			return;

		if( node->contents < 0 )
		{
			mleaf_t	*leaf = (mleaf_t *)node;

			// it's a leaf!
			if( ll->count >= ll->maxcount )
			{
				ll->overflowed = true;
				return;
			}

			ll->list[ll->count++] = leaf - worldmodel->leafs - 1;
			return;
		}
	
		plane = node->plane;
		s = BOX_ON_PLANE_SIDE( ll->mins, ll->maxs, plane );

		if( s == 1 )
		{
			node = node->children[0];
		}
		else if( s == 2 )
		{
			node = node->children[1];
		}
		else
		{
			// go down both
			if( ll->topnode == -1 )
				ll->topnode = node - worldmodel->nodes;
			Mod_BoxLeafnums_r( ll, node->children[0] );
			node = node->children[1];
		}
	}
}

/*
==================
Mod_BoxLeafnums
==================
*/
int Mod_BoxLeafnums( const vec3_t mins, const vec3_t maxs, short *list, int listsize, int *topnode )
{
	leaflist_t	ll;

	if( !worldmodel ) return 0;

	VectorCopy( mins, ll.mins );
	VectorCopy( maxs, ll.maxs );
	ll.count = 0;
	ll.maxcount = listsize;
	ll.list = list;
	ll.topnode = -1;
	ll.overflowed = false;

	Mod_BoxLeafnums_r( &ll, worldmodel->nodes );

	if( topnode ) *topnode = ll.topnode;
	return ll.count;
}

/*
=============
Mod_BoxVisible

Returns true if any leaf in boxspace
is potentially visible
=============
*/
qboolean Mod_BoxVisible( const vec3_t mins, const vec3_t maxs, const byte *visbits )
{
	short	leafList[MAX_BOX_LEAFS];
	int	i, count;

	if( !visbits || !mins || !maxs )
		return true;

	count = Mod_BoxLeafnums( mins, maxs, leafList, MAX_BOX_LEAFS, NULL );

	for( i = 0; i < count; i++ )
	{
		int	leafnum = leafList[i];

		if( leafnum != -1 && visbits[leafnum>>3] & (1<<( leafnum & 7 )))
			return true;
	}
	return false;
}

/*
==================
Mod_AmbientLevels

grab the ambient sound levels for current point
==================
*/
void Mod_AmbientLevels( const vec3_t p, byte *pvolumes )
{
	mleaf_t	*leaf;

	if( !worldmodel || !p || !pvolumes )
		return;	

	leaf = Mod_PointInLeaf( p, worldmodel->nodes );
	*(int *)pvolumes = *(int *)leaf->ambient_sound_level;
}

/*
================
Mod_FreeModel
================
*/
static void Mod_FreeModel( model_t *mod )
{
	if( !mod || !mod->mempool )
		return;

	// select the properly unloader
	switch( mod->type )
	{
	case mod_sprite:
		Mod_UnloadSpriteModel( mod );
		break;
	case mod_studio:
		Mod_UnloadStudioModel( mod );
		break;
	case mod_brush:
		Mod_UnloadBrushModel( mod );
		break;
	}
}

/*
===============================================================================

			MODEL INITALIZE\SHUTDOWN

===============================================================================
*/

void Mod_Init( void )
{
	com_studiocache = Mem_AllocPool( "Studio Cache" );
}

void Mod_ClearAll( void )
{
	int	i;

	for( i = 0; i < cm_nummodels; i++ )
		Mod_FreeModel( &cm_models[i] );

	Q_memset( cm_models, 0, sizeof( cm_models ));
	cm_nummodels = 0;
}

void Mod_Shutdown( void )
{
	Mod_ClearAll();
	Mem_FreePool( &com_studiocache );
}

/*
===============================================================================

			MAP LOADING

===============================================================================
*/
/*
=================
Mod_LoadSubmodels
=================
*/
static void Mod_LoadSubmodels( const dlump_t *l )
{
	dmodel_t	*in;
	dmodel_t	*out;
	int	i, j, count;
	
	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "Mod_LoadBModel: funny lump size\n" );
	count = l->filelen / sizeof( *in );

	if( count < 1 ) Host_Error( "Map %s without models\n", loadmodel->name );
	if( count > MAX_MAP_MODELS ) Host_Error( "Map %s has too many models\n", loadmodel->name );

	// allocate extradata
	out = Mem_Alloc( loadmodel->mempool, count * sizeof( *out ));
	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;
	if( world.loading ) world.max_surfaces = 0;

	for( i = 0; i < count; i++, in++, out++ )
	{
		for( j = 0; j < 3; j++ )
		{
			// spread the mins / maxs by a pixel
			out->mins[j] = in->mins[j] - 1;
			out->maxs[j] = in->maxs[j] + 1;
			out->origin[j] = in->origin[j];
		}

		for( j = 0; j < MAX_MAP_HULLS; j++ )
			out->headnode[j] = in->headnode[j];

		out->visleafs = in->visleafs;
		out->firstface = in->firstface;
		out->numfaces = in->numfaces;

		if( i == 0 || !world.loading )
			continue; // skip the world

		world.max_surfaces = max( world.max_surfaces, out->numfaces ); 
	}

	if( world.loading )
		world.draw_surfaces = Mem_Alloc( loadmodel->mempool, world.max_surfaces * sizeof( msurface_t* ));
}

/*
=================
Mod_LoadTextures
=================
*/
static void Mod_LoadTextures( const dlump_t *l )
{
	dmiptexlump_t	*in;
	texture_t		*tx, *tx2;
	texture_t		*anims[10];
	texture_t		*altanims[10];
	int		num, max, altmax;
	char		texname[64];
	mip_t		*mt;
	int 		i, j; 

	if( world.loading )
	{
		// release old sky layers first
		GL_FreeTexture( tr.solidskyTexture );
		GL_FreeTexture( tr.alphaskyTexture );
		tr.solidskyTexture = tr.alphaskyTexture = 0;
		world.has_mirrors = false;
		world.sky_sphere = false;
	}

	if( !l->filelen )
	{
		// no textures
		loadmodel->textures = NULL;
		return;
	}

	in = (void *)(mod_base + l->fileofs);

	loadmodel->numtextures = in->nummiptex;
	loadmodel->textures = (texture_t **)Mem_Alloc( loadmodel->mempool, loadmodel->numtextures * sizeof( texture_t* ));

	for( i = 0; i < loadmodel->numtextures; i++ )
	{
		qboolean	load_external = false;
		qboolean	load_external_luma = false;

		if( in->dataofs[i] == -1 )
		{
			// create default texture (some mods requires this)
			tx = Mem_Alloc( loadmodel->mempool, sizeof( *tx ));
			loadmodel->textures[i] = tx;
		
			Q_strncpy( tx->name, "*default", sizeof( tx->name ));
			tx->gl_texturenum = tr.defaultTexture;
			tx->width = tx->height = 16;
			continue; // missed
		}

		mt = (mip_t *)((byte *)in + in->dataofs[i] );

		if( !mt->name[0] )
		{
			MsgDev( D_WARN, "unnamed texture in %s\n", loadmodel->name );
			Q_snprintf( mt->name, sizeof( mt->name ), "miptex_%i", i );
		}

		tx = Mem_Alloc( loadmodel->mempool, sizeof( *tx ));
		loadmodel->textures[i] = tx;

		// convert to lowercase
		Q_strnlwr( mt->name, mt->name, sizeof( mt->name ));
		Q_strncpy( tx->name, mt->name, sizeof( tx->name ));

		tx->width = mt->width;
		tx->height = mt->height;

		// check for multi-layered sky texture
		if( world.loading && !Q_strncmp( mt->name, "sky", 3 ) && mt->width == 256 && mt->height == 128 )
		{	
			if( host_allow_materials->integer )
			{
				// build standard path: "materials/mapname/texname_solid.tga"
				Q_snprintf( texname, sizeof( texname ), "materials/%s/%s_solid.tga", modelname, mt->name );

				if( !FS_FileExists( texname, false ))
				{
					// build common path: "materials/mapname/texname_solid.tga"
					Q_snprintf( texname, sizeof( texname ), "materials/common/%s_solid.tga", mt->name );

					if( FS_FileExists( texname, false ))
						load_external = true;
				}
				else load_external = true;

				if( load_external )
				{
					tr.solidskyTexture = GL_LoadTexture( texname, NULL, 0, TF_UNCOMPRESSED|TF_NOMIPMAP );
					GL_SetTextureType( tr.solidskyTexture, TEX_BRUSH );
					load_external = false;
				}

				if( tr.solidskyTexture )
				{
					// build standard path: "materials/mapname/texname_alpha.tga"
					Q_snprintf( texname, sizeof( texname ), "materials/%s/%s_alpha.tga", modelname, mt->name );

					if( !FS_FileExists( texname, false ))
					{
						// build common path: "materials/mapname/texname_alpha.tga"
						Q_snprintf( texname, sizeof( texname ), "materials/common/%s_alpha.tga", mt->name );

						if( FS_FileExists( texname, false ))
							load_external = true;
					}
					else load_external = true;

					if( load_external )
					{
						tr.alphaskyTexture = GL_LoadTexture( texname, NULL, 0, TF_UNCOMPRESSED|TF_NOMIPMAP );
						GL_SetTextureType( tr.alphaskyTexture, TEX_BRUSH );
						load_external = false;
					}
				}

				if( !tr.solidskyTexture || !tr.alphaskyTexture )
				{
					// couldn't find one of layer
					GL_FreeTexture( tr.solidskyTexture );
					GL_FreeTexture( tr.alphaskyTexture );
					tr.solidskyTexture = tr.alphaskyTexture = 0;
				}
			}

			if( !tr.solidskyTexture && !tr.alphaskyTexture )
				R_InitSky( mt, tx ); // fallback to standard sky

			if( tr.solidskyTexture && tr.alphaskyTexture )
				world.sky_sphere = true;
		}
		else 
		{
			if( host_allow_materials->integer )
			{
				if( mt->name[0] == '*' ) mt->name[0] = '!'; // replace unexpected symbol

				// build standard path: "materials/mapname/texname.tga"
				Q_snprintf( texname, sizeof( texname ), "materials/%s/%s.tga", modelname, mt->name );

				if( !FS_FileExists( texname, false ))
				{
					// build common path: "materials/mapname/texname.tga"
					Q_snprintf( texname, sizeof( texname ), "materials/common/%s.tga", mt->name );

					if( FS_FileExists( texname, false ))
						load_external = true;
				}
				else load_external = true;
			}
load_wad_textures:
			if( !load_external )
				Q_snprintf( texname, sizeof( texname ), "%s%s.mip", ( mt->offsets[0] > 0 ) ? "#" : "", mt->name );
			else MsgDev( D_NOTE, "loading HQ: %s\n", texname );

			if( mt->offsets[0] > 0 && !load_external )
			{
				// NOTE: imagelib detect miptex version by size
				// 770 additional bytes is indicated custom palette
				int size = (int)sizeof( mip_t ) + ((mt->width * mt->height * 85)>>6);
				if( bmodel_version == HLBSP_VERSION ) size += sizeof( short ) + 768;

				tx->gl_texturenum = GL_LoadTexture( texname, (byte *)mt, size, 0 );
			}
			else
			{
				// okay, loading it from wad
				tx->gl_texturenum = GL_LoadTexture( texname, NULL, 0, 0 );

				if( !tx->gl_texturenum && load_external )
				{
					// in case we failed to loading 32-bit texture
					MsgDev( D_ERROR, "Couldn't load %s\n", texname );
					load_external = false;
					goto load_wad_textures;
				}
			}
		}

		// set the emo-texture for missed
		if( !tx->gl_texturenum ) tx->gl_texturenum = tr.defaultTexture;

		if( load_external )
		{
			// build standard luma path: "materials/mapname/texname_luma.tga"
			Q_snprintf( texname, sizeof( texname ), "materials/%s/%s_luma.tga", modelname, mt->name );

			if( !FS_FileExists( texname, false ))
			{
				// build common path: "materials/mapname/texname_luma.tga"
				Q_snprintf( texname, sizeof( texname ), "materials/common/%s_luma.tga", mt->name );

				if( FS_FileExists( texname, false ))
					load_external_luma = true;
			}
			else load_external_luma = true;
		}

		// check for luma texture
		if( R_GetTexture( tx->gl_texturenum )->flags & TF_HAS_LUMA || load_external_luma )
		{
			if( !load_external_luma )
				Q_snprintf( texname, sizeof( texname ), "#%s_luma.mip", mt->name );
			else MsgDev( D_NOTE, "loading luma HQ: %s\n", texname );

			if( mt->offsets[0] > 0 && !load_external_luma )
			{
				// NOTE: imagelib detect miptex version by size
				// 770 additional bytes is indicated custom palette
				int size = (int)sizeof( mip_t ) + ((mt->width * mt->height * 85)>>6);
				if( bmodel_version == HLBSP_VERSION ) size += sizeof( short ) + 768;

				tx->fb_texturenum = GL_LoadTexture( texname, (byte *)mt, size, TF_NOMIPMAP|TF_MAKELUMA );
			}
			else
			{
				size_t srcSize = 0;
				byte *src = NULL;

				// NOTE: we can't loading it from wad as normal because _luma texture doesn't exist
				// and not be loaded. But original texture is already loaded and can't be modified
				// So load original texture manually and convert it to luma
				if( !load_external_luma ) src = FS_LoadFile( va( "%s.mip", tx->name ), &srcSize, false );

				// okay, loading it from wad or hi-res version
				tx->fb_texturenum = GL_LoadTexture( texname, src, srcSize, TF_NOMIPMAP|TF_MAKELUMA );
				if( src ) Mem_Free( src );

				if( !tx->fb_texturenum && load_external_luma )
				{
					// in case we failed to loading 32-bit luma texture
					MsgDev( D_ERROR, "Couldn't load %s\n", texname );
				} 
			}
		}

		if( tx->gl_texturenum != tr.defaultTexture )
		{
			// apply texture type (just for debug)
			GL_SetTextureType( tx->gl_texturenum, TEX_BRUSH );
			GL_SetTextureType( tx->fb_texturenum, TEX_BRUSH );
		}
	}

	// sequence the animations
	for( i = 0; i < loadmodel->numtextures; i++ )
	{
		tx = loadmodel->textures[i];

		if( !tx || tx->name[0] != '+' )
			continue;

		if( tx->anim_next )
			continue;	// allready sequenced

		// find the number of frames in the animation
		Q_memset( anims, 0, sizeof( anims ));
		Q_memset( altanims, 0, sizeof( altanims ));

		max = tx->name[1];
		altmax = 0;

		if( max >= '0' && max <= '9' )
		{
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		}
		else if( max >= 'a' && max <= 'j' )
		{
			altmax = max - 'a';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else Host_Error( "Mod_LoadTextures: bad animating texture %s\n", tx->name );

		for( j = i + 1; j < loadmodel->numtextures; j++ )
		{
			tx2 = loadmodel->textures[j];
			if( !tx2 || tx2->name[0] != '+' )
				continue;

			if( Q_strcmp( tx2->name + 2, tx->name + 2 ))
				continue;

			num = tx2->name[1];
			if( num >= '0' && num <= '9' )
			{
				num -= '0';
				anims[num] = tx2;
				if (num+1 > max)
					max = num + 1;
			}
			else if( num >= 'a' && num <= 'j' )
			{
				num = num - 'a';
				altanims[num] = tx2;
				if( num + 1 > altmax )
					altmax = num + 1;
			}
			else Host_Error( "Mod_LoadTextures: bad animating texture %s\n", tx->name );
		}

		// link them all together
		for( j = 0; j < max; j++ )
		{
			tx2 = anims[j];
			if( !tx2 ) Host_Error( "Mod_LoadTextures: missing frame %i of %s\n", j, tx->name );
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = anims[(j + 1) % max];
			if( altmax ) tx2->alternate_anims = altanims[0];
		}

		for( j = 0; j < altmax; j++ )
		{
			tx2 = altanims[j];
			if( !tx2 ) Host_Error( "Mod_LoadTextures: missing frame %i of %s\n", j, tx->name );
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = altanims[(j + 1) % altmax];
			if( max ) tx2->alternate_anims = anims[0];
		}
	}

	// sequence the detail textures
	for( i = 0; i < loadmodel->numtextures; i++ )
	{
		tx = loadmodel->textures[i];

		if( !tx || tx->name[0] != '-' )
			continue;

		if( tx->anim_next )
			continue;	// allready sequenced

		// find the number of frames in the sequence
		Q_memset( anims, 0, sizeof( anims ));

		max = tx->name[1];

		if( max >= '0' && max <= '9' )
		{
			max -= '0';
			anims[max] = tx;
			max++;
		}
		else Host_Error( "Mod_LoadTextures: bad detail texture %s\n", tx->name );

		for( j = i + 1; j < loadmodel->numtextures; j++ )
		{
			tx2 = loadmodel->textures[j];
			if( !tx2 || tx2->name[0] != '-' )
				continue;

			if( Q_strcmp( tx2->name + 2, tx->name + 2 ))
				continue;

			num = tx2->name[1];
			if( num >= '0' && num <= '9' )
			{
				num -= '0';
				anims[num] = tx2;
				if( num+1 > max )
					max = num + 1;
			}
			else Host_Error( "Mod_LoadTextures: bad detail texture %s\n", tx->name );
		}

		// link them all together
		for( j = 0; j < max; j++ )
		{
			tx2 = anims[j];
			if( !tx2 ) Host_Error( "Mod_LoadTextures: missing frame %i of %s\n", j, tx->name );
			tx2->anim_total = -( max * ANIM_CYCLE ); // to differentiate from animations
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = anims[(j + 1) % max];
		}	
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
	float		len1, len2;
	
	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadTexInfo: funny lump size in %s\n", loadmodel->name );

	count = l->filelen / sizeof( *in );
          out = Mem_Alloc( loadmodel->mempool, count * sizeof( *out ));
	
	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		for( j = 0; j < 8; j++ )
			out->vecs[0][j] = in->vecs[0][j];

		len1 = VectorLength( out->vecs[0] );
		len2 = VectorLength( out->vecs[1] );
		len1 = ( len1 + len2 ) / 2;

		// g-cont: can use this info for GL_TEXTURE_LOAD_BIAS_EXT ?
		if( len1 < 0.32f ) out->mipadjust = 4;
		else if( len1 < 0.49f ) out->mipadjust = 3;
		else if( len1 < 0.99f ) out->mipadjust = 2;
		else out->mipadjust = 1;

		miptex = in->miptex;
		if( miptex < 0 || miptex > loadmodel->numtextures )
			Host_Error( "Mod_LoadTexInfo: bad miptex number in '%s'\n", loadmodel->name );

		out->texture = loadmodel->textures[miptex];
		out->flags = in->flags;
	}
}

/*
=================
Mod_LoadLighting
=================
*/
static void Mod_LoadLighting( const dlump_t *l )
{
	byte	d, *in;
	color24	*out;
	int	i;

	if( !l->filelen ) return;
	in = (void *)(mod_base + l->fileofs);

	switch( bmodel_version )
	{
	case Q1BSP_VERSION:
		// expand the white lighting data
		loadmodel->lightdata = (color24 *)Mem_Alloc( loadmodel->mempool, l->filelen * sizeof( color24 ));
		out = loadmodel->lightdata;

		for( i = 0; i < l->filelen; i++, out++ )
		{
			d = *in++;
			out->r = d;
			out->g = d;
			out->b = d;
		}
		break;
	case HLBSP_VERSION:
		// load colored lighting
		loadmodel->lightdata = Mem_Alloc( loadmodel->mempool, l->filelen );
		Q_memcpy( loadmodel->lightdata, in, l->filelen );
		break;
	}
}

/*
=================
Mod_CalcSurfaceExtents

Fills in surf->texturemins[] and surf->extents[]
=================
*/
static void Mod_CalcSurfaceExtents( msurface_t *surf )
{
	float		mins[2], maxs[2], val;
	int		bmins[2], bmaxs[2];
	int		i, j, e;
	mtexinfo_t	*tex;
	mvertex_t		*v;

	tex = surf->texinfo;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -999999;

	for( i = 0; i < surf->numedges; i++ )
	{
		e = loadmodel->surfedges[surf->firstedge + i];
		if( e >= 0 ) v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		for( j = 0; j < 2; j++ )
		{
			val = DotProduct( v->position, surf->texinfo->vecs[j] ) + surf->texinfo->vecs[j][3];
			if( val < mins[j] ) mins[j] = val;
			if( val > maxs[j] ) maxs[j] = val;
		}
	}

	for( i = 0; i < 2; i++ )
	{
		bmins[i] = floor( mins[i] / LM_SAMPLE_SIZE );
		bmaxs[i] = ceil( maxs[i] / LM_SAMPLE_SIZE );

		surf->texturemins[i] = bmins[i] * LM_SAMPLE_SIZE;
		surf->extents[i] = (bmaxs[i] - bmins[i]) * LM_SAMPLE_SIZE;

		if(!( tex->flags & TEX_SPECIAL ) && surf->extents[i] > 4096 )
			MsgDev( D_ERROR, "Bad surface extents %i\n", surf->extents[i] );
	}
}

/*
=================
Mod_CalcSurfaceBounds

fills in surf->mins and surf->maxs
=================
*/
static void Mod_CalcSurfaceBounds( msurface_t *surf, mextrasurf_t *info )
{
	int	i, e;
	mvertex_t	*v;

	ClearBounds( info->mins, info->maxs );

	for( i = 0; i < surf->numedges; i++ )
	{
		e = loadmodel->surfedges[surf->firstedge + i];
		if( e >= 0 ) v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];
		AddPointToBounds( v->position, info->mins, info->maxs );
	}
	VectorAverage( info->mins, info->maxs, info->origin );
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
	mextrasurf_t	*info;
	int		i, j;
	int		count;

	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( dface_t ))
		Host_Error( "Mod_LoadSurfaces: funny lump size in '%s'\n", loadmodel->name );
	count = l->filelen / sizeof( dface_t );

	loadmodel->numsurfaces = count;
	loadmodel->surfaces = Mem_Alloc( loadmodel->mempool, count * sizeof( msurface_t ));
	loadmodel->cache.data =  Mem_Alloc( loadmodel->mempool, count * sizeof( mextrasurf_t ));
	out = loadmodel->surfaces;
	info = loadmodel->cache.data;

	for( i = 0; i < count; i++, in++, out++, info++ )
	{
		texture_t	*tex;

		if(( in->firstedge + in->numedges ) > loadmodel->numsurfedges )
		{
			MsgDev( D_ERROR, "Bad surface %i from %i\n", i, count );
			continue;
		} 

		out->firstedge = in->firstedge;
		out->numedges = in->numedges;
		out->flags = 0;

		if( in->side ) out->flags |= SURF_PLANEBACK;
		out->plane = loadmodel->planes + in->planenum;
		out->texinfo = loadmodel->texinfo + in->texinfo;

		tex = out->texinfo->texture;

		if( !Q_strncmp( tex->name, "sky", 3 ))
			out->flags |= (SURF_DRAWTILED|SURF_DRAWSKY);

		if(( tex->name[0] == '*' && Q_stricmp( tex->name, "*default" )) || tex->name[0] == '!' )
			out->flags |= (SURF_DRAWTURB|SURF_DRAWTILED);

		if( !Q_strncmp( tex->name, "water", 5 ) || !Q_strnicmp( tex->name, "laser", 5 ))
			out->flags |= (SURF_DRAWTURB|SURF_DRAWTILED|SURF_NOCULL);

		if( !Q_strncmp( tex->name, "scroll", 6 ))
			out->flags |= SURF_CONVEYOR;

		// g-cont this texture from decals.wad he-he
		if( !Q_strncmp( tex->name, "reflect", 7 ))
		{
			out->flags |= SURF_REFLECT;
			world.has_mirrors = true;
		}

		if( tex->name[0] == '{' )
			out->flags |= SURF_TRANSPARENT;

		if( out->texinfo->flags & TEX_SPECIAL )
			out->flags |= SURF_DRAWTILED;

		Mod_CalcSurfaceBounds( out, info );
		Mod_CalcSurfaceExtents( out );

		if( loadmodel->lightdata && in->lightofs != -1 )
		{
			if( bmodel_version == HLBSP_VERSION )
				out->samples = loadmodel->lightdata + (in->lightofs / 3);
			else out->samples = loadmodel->lightdata + in->lightofs;
		}

		for( j = 0; j < MAXLIGHTMAPS; j++ )
			out->styles[j] = in->styles[j];

		if( out->flags & SURF_DRAWSKY && world.loading && world.sky_sphere )
			GL_SubdivideSurface( out ); // cut up polygon for warps

		if( out->flags & SURF_DRAWTURB )
			GL_SubdivideSurface( out ); // cut up polygon for warps
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
	mvertex_t	*out;
	int	i, count;

	in = (void *)( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadVertexes: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof( *in );

	loadmodel->numvertexes = count;
	out = loadmodel->vertexes = Mem_Alloc( loadmodel->mempool, count * sizeof( mvertex_t ));

	if( world.loading ) ClearBounds( world.mins, world.maxs );

	for( i = 0; i < count; i++, in++, out++ )
	{
		VectorCopy( in->point, out->position );
		if( world.loading ) AddPointToBounds( in->point, world.mins, world.maxs );
	}

	if( !world.loading ) return;

	VectorSubtract( world.maxs, world.mins, world.size );

	for( i = 0; i < 3; i++ )
	{
		// spread the mins / maxs by a pixel
		world.mins[i] -= 1;
		world.maxs[i] += 1;
	}
}

/*
=================
Mod_LoadEdges
=================
*/
static void Mod_LoadEdges( const dlump_t *l )
{
	dedge_t	*in;
	medge_t	*out;
	int	i, count;

	in = (void *)( mod_base + l->fileofs );	
	if( l->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadEdges: funny lump size in %s\n", loadmodel->name );

	count = l->filelen / sizeof( *in );
	loadmodel->edges = out = Mem_Alloc( loadmodel->mempool, count * sizeof( medge_t ));
	loadmodel->numedges = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->v[0] = (word)in->v[0];
		out->v[1] = (word)in->v[1];
	}
}

/*
=================
Mod_LoadSurfEdges
=================
*/
static void Mod_LoadSurfEdges( const dlump_t *l )
{
	dsurfedge_t	*in;
	int		count;

	in = (void *)( mod_base + l->fileofs );	
	if( l->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadSurfEdges: funny lump size in %s\n", loadmodel->name );

	count = l->filelen / sizeof( dsurfedge_t );
	loadmodel->surfedges = Mem_Alloc( loadmodel->mempool, count * sizeof( dsurfedge_t ));
	loadmodel->numsurfedges = count;

	Q_memcpy( loadmodel->surfedges, in, count * sizeof( dsurfedge_t ));
}

/*
=================
Mod_LoadMarkSurfaces
=================
*/
static void Mod_LoadMarkSurfaces( const dlump_t *l )
{
	dmarkface_t	*in;
	int		i, j, count;
	msurface_t	**out;
	
	in = (void *)( mod_base + l->fileofs );	
	if( l->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadMarkFaces: funny lump size in %s\n", loadmodel->name );

	count = l->filelen / sizeof( *in );
	loadmodel->marksurfaces = out = Mem_Alloc( loadmodel->mempool, count * sizeof( *out ));
	loadmodel->nummarksurfaces = count;

	for( i = 0; i < count; i++ )
	{
		j = in[i];
		if( j < 0 ||  j >= loadmodel->numsurfaces )
			Host_Error( "Mod_LoadMarkFaces: bad surface number in '%s'\n", loadmodel->name );
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_SetParent
=================
*/
static void Mod_SetParent( mnode_t *node, mnode_t *parent )
{
	node->parent = parent;

	if( node->contents < 0 ) return; // it's node
	Mod_SetParent( node->children[0], node );
	Mod_SetParent( node->children[1], node );
}

/*
=================
Mod_LoadNodes
=================
*/
static void Mod_LoadNodes( const dlump_t *l )
{
	dnode_t	*in;
	mnode_t	*out;
	int	i, j, p;
	
	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "Mod_LoadNodes: funny lump size\n" );
	loadmodel->numnodes = l->filelen / sizeof( *in );

	if( loadmodel->numnodes < 1 ) Host_Error( "Map %s has no nodes\n", loadmodel->name );
	out = loadmodel->nodes = (mnode_t *)Mem_Alloc( loadmodel->mempool, loadmodel->numnodes * sizeof( *out ));

	for( i = 0; i < loadmodel->numnodes; i++, out++, in++ )
	{
		for( j = 0; j < 3; j++ )
		{
			out->minmaxs[j] = in->mins[j];
			out->minmaxs[3+j] = in->maxs[j];
		}

		p = in->planenum;
		out->plane = loadmodel->planes + p;
		out->firstsurface = in->firstface;
		out->numsurfaces = in->numfaces;

		for( j = 0; j < 2; j++ )
		{
			p = in->children[j];
			if( p >= 0 ) out->children[j] = loadmodel->nodes + p;
			else out->children[j] = (mnode_t *)(loadmodel->leafs + ( -1 - p ));
		}
	}

	// sets nodes and leafs
	Mod_SetParent( loadmodel->nodes, NULL );
}

/*
=================
Mod_LoadLeafs
=================
*/
static void Mod_LoadLeafs( const dlump_t *l )
{
	dleaf_t 	*in;
	mleaf_t	*out;
	int	i, j, p, count;
		
	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "Mod_LoadLeafs: funny lump size\n" );

	count = l->filelen / sizeof( *in );
	if( count < 1 ) Host_Error( "Map %s with no leafs\n", loadmodel->name );
	out = (mleaf_t *)Mem_Alloc( loadmodel->mempool, count * sizeof( *out ));

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		for( j = 0; j < 3; j++ )
		{
			out->minmaxs[j] = in->mins[j];
			out->minmaxs[3+j] = in->maxs[j];
		}

		out->contents = in->contents;
	
		p = in->visofs;

		if( p == -1 ) out->compressed_vis = NULL;
		else out->compressed_vis = loadmodel->visdata + p;

		for( j = 0; j < 4; j++ )
			out->ambient_sound_level[j] = in->ambient_level[j];

		out->firstmarksurface = loadmodel->marksurfaces + in->firstmarksurface;
		out->nummarksurfaces = in->nummarksurfaces;

		// gl underwater warp
		if( out->contents != CONTENTS_EMPTY )
		{
			for( j = 0; j < out->nummarksurfaces; j++ )
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
	}

	if( loadmodel->leafs[0].contents != CONTENTS_SOLID )
		Host_Error( "Mod_LoadLeafs: Map %s has leaf 0 is not CONTENTS_SOLID\n", loadmodel->name );
}

/*
=================
Mod_LoadPlanes
=================
*/
static void Mod_LoadPlanes( const dlump_t *l )
{
	dplane_t	*in;
	mplane_t	*out;
	int	i, j, count;
	
	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "Mod_LoadPlanes: funny lump size\n" );
	count = l->filelen / sizeof( *in );

	if( count < 1 ) Host_Error( "Map %s with no planes\n", loadmodel->name );
	out = (mplane_t *)Mem_Alloc( loadmodel->mempool, count * sizeof( *out ));

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		for( j = 0; j < 3; j++ )
		{
			out->normal[j] = in->normal[j];
			if( out->normal[j] < 0.0f )
				out->signbits |= 1<<j;
		}

		out->dist = in->dist;
		out->type = in->type;
	}
}

/*
=================
Mod_LoadVisibility
=================
*/
static void Mod_LoadVisibility( const dlump_t *l )
{
	// bmodels has no visibility
	if( !world.loading ) return;

	if( !l->filelen )
	{
		MsgDev( D_WARN, "map ^2%s^7 has no visibility\n", loadmodel->name );
		loadmodel->visdata = NULL;
		world.visdatasize = 0;
		return;
	}

	loadmodel->visdata = Mem_Alloc( loadmodel->mempool, l->filelen );
	Q_memcpy( loadmodel->visdata, (void *)(mod_base + l->fileofs), l->filelen );
	world.visdatasize = l->filelen; // save it for PHS allocation
}

/*
=================
Mod_LoadEntities
=================
*/
static void Mod_LoadEntities( const dlump_t *l )
{
	byte	*in;

	in = (void *)(mod_base + l->fileofs);
	loadmodel->entities = Mem_Alloc( loadmodel->mempool, l->filelen );	
	Q_memcpy( loadmodel->entities, mod_base + l->fileofs, l->filelen );
}

/*
=================
Mod_LoadClipnodes
=================
*/
static void Mod_LoadClipnodes( const dlump_t *l )
{
	dclipnode_t	*in, *out;
	int		i, count;
	hull_t		*hull;

	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "Mod_LoadClipnodes: funny lump size\n" );
	count = l->filelen / sizeof( *in );
	out = Mem_Alloc( loadmodel->mempool, count * sizeof( *out ));	

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

	// hulls[0] is a point hull, always zeroed

	hull = &loadmodel->hulls[1];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	VectorCopy( GI->client_mins[1], hull->clip_mins ); // copy human hull
	VectorCopy( GI->client_maxs[1], hull->clip_maxs );
	VectorSubtract( hull->clip_maxs, hull->clip_mins, world.hull_sizes[1] );

	hull = &loadmodel->hulls[2];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	VectorCopy( GI->client_mins[2], hull->clip_mins ); // copy large hull
	VectorCopy( GI->client_maxs[2], hull->clip_maxs );
	VectorSubtract( hull->clip_maxs, hull->clip_mins, world.hull_sizes[2] );

	hull = &loadmodel->hulls[3];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	VectorCopy( GI->client_mins[3], hull->clip_mins ); // copy head hull
	VectorCopy( GI->client_maxs[3], hull->clip_maxs );
	VectorSubtract( hull->clip_maxs, hull->clip_mins, world.hull_sizes[3] );

	for( i = 0; i < count; i++, out++, in++ )
	{
		out->planenum = in->planenum;
		out->children[0] = in->children[0];
		out->children[1] = in->children[1];
	}
}

/*
=================
Mod_FindModelOrigin

routine to detect bmodels with origin-brush
=================
*/
static void Mod_FindModelOrigin( const char *entities, const char *modelname, vec3_t origin )
{
	char	*pfile;
	string	keyname;
	char	token[2048];
	qboolean	model_found;
	qboolean	origin_found;

	if( !entities || !modelname || !*modelname || !origin )
		return;

	pfile = (char *)entities;

	while(( pfile = COM_ParseFile( pfile, token )) != NULL )
	{
		if( token[0] != '{' )
			Host_Error( "Mod_FindModelOrigin: found %s when expecting {\n", token );

		model_found = origin_found = false;
		VectorClear( origin );

		while( 1 )
		{
			// parse key
			if(( pfile = COM_ParseFile( pfile, token )) == NULL )
				Host_Error( "Mod_FindModelOrigin: EOF without closing brace\n" );
			if( token[0] == '}' ) break; // end of desc

			Q_strncpy( keyname, token, sizeof( keyname ));

			// parse value	
			if(( pfile = COM_ParseFile( pfile, token )) == NULL ) 
				Host_Error( "Mod_FindModelOrigin: EOF without closing brace\n" );

			if( token[0] == '}' )
				Host_Error( "Mod_FindModelOrigin: closing brace without data\n" );

			if( !Q_stricmp( keyname, "model" ) && !Q_stricmp( modelname, token ))
				model_found = true;

			if( !Q_stricmp( keyname, "origin" ))
			{
				Q_atov( origin, token, 3 );
				origin_found = true;
			}
		}

		if( model_found ) break;
	}	
}

/*
=================
Mod_MakeHull0

Duplicate the drawing hull structure as a clipping hull
=================
*/
static void Mod_MakeHull0( void )
{
	mnode_t		*in, *child;
	dclipnode_t	*out;
	hull_t		*hull;
	int		i, j, count;
	
	hull = &loadmodel->hulls[0];	
	
	in = loadmodel->nodes;
	count = loadmodel->numnodes;
	out = Mem_Alloc( loadmodel->mempool, count * sizeof( *out ));	

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;

	for( i = 0; i < count; i++, out++, in++ )
	{
		out->planenum = in->plane - loadmodel->planes;

		for( j = 0; j < 2; j++ )
		{
			child = in->children[j];

			if( child->contents < 0 )
				out->children[j] = child->contents;
			else out->children[j] = child - loadmodel->nodes;
		}
	}
}

/*
=================
Mod_CalcPHS
=================
*/
void Mod_CalcPHS( void )
{
	int	hcount, vcount;
	int	i, j, k, l, index, num;
	int	rowbytes, rowwords;
	int	bitbyte, rowsize;
	int	*visofs, total_size = 0;
	byte	*vismap, *vismap_p;
	byte	*uncompressed_vis;
	byte	*uncompressed_pas;
	byte	*compressed_pas;
	byte	*scan, *comp;
	uint	*dest, *src;
	double	timestart;
	size_t	phsdatasize;

	// no worldmodel or no visdata
	if( !world.loading || !worldmodel || !worldmodel->visdata )
		return;

	MsgDev( D_NOTE, "Building PAS...\n" );
	timestart = Sys_DoubleTime();

	// NOTE: first leaf is skipped becuase is a outside leaf. Now all leafs have shift up by 1.
	// and last leaf (which equal worldmodel->numleafs) has no visdata! Add extra one leaf
	// to avoid this situation.
	num = worldmodel->numleafs + 1;
	rowwords = (num + 31)>>5;
	rowbytes = rowwords * 4;

	// typycally PHS reqiured more room because RLE fails on multiple 1 not 0
	phsdatasize = world.visdatasize * 4; // empirically determined

	// allocate pvs and phs data single array
	visofs = Mem_Alloc( worldmodel->mempool, num * sizeof( int ));
	uncompressed_vis = Mem_Alloc( worldmodel->mempool, rowbytes * num * 2 );
	uncompressed_pas = uncompressed_vis + rowbytes * num;
	compressed_pas = Mem_Alloc( worldmodel->mempool, phsdatasize );
	vismap = vismap_p = compressed_pas; // compressed PHS buffer
	scan = uncompressed_vis;
	vcount = 0;

	// uncompress pvs first
	for( i = 0; i < num; i++, scan += rowbytes )
	{
		Q_memcpy( scan, Mod_LeafPVS( worldmodel->leafs + i, worldmodel ), rowbytes );
		if( i == 0 ) continue;

		for( j = 0; j < num; j++ )
		{
			if( scan[j>>3] & (1<<( j & 7 )))
				vcount++;
		}
	}

	scan = uncompressed_vis;
	hcount = 0;

	dest = (uint *)uncompressed_pas;

	for( i = 0; i < num; i++, dest += rowwords, scan += rowbytes )
	{
		Q_memcpy( dest, scan, rowbytes );

		for( j = 0; j < rowbytes; j++ )
		{
			bitbyte = scan[j];
			if( !bitbyte ) continue;

			for( k = 0; k < 8; k++ )
			{
				if(!( bitbyte & ( 1<<k )))
					continue;
				// or this pvs row into the phs
				// +1 because pvs is 1 based
				index = ((j<<3) + k + 1);
				if( index >= num ) continue;

				src = (uint *)uncompressed_vis + index * rowwords;
				for( l = 0; l < rowwords; l++ )
					dest[l] |= src[l];
			}
		}

		// compress PHS data back
		comp = Mod_CompressVis( (byte *)dest, &rowsize );
		visofs[i] = vismap_p - vismap; // leaf 0 is a common solid 
		total_size += rowsize;

		if( total_size > phsdatasize )
		{
			Host_Error( "CalcPHS: vismap expansion overflow %s > %s\n",
				Q_memprint( total_size ), Q_memprint( phsdatasize ));
		}

		Q_memcpy( vismap_p, comp, rowsize );
		vismap_p += rowsize; // move pointer

		if( i == 0 ) continue;

		for( j = 0; j < num; j++ )
		{
			if(((byte *)dest)[j>>3] & (1<<( j & 7 )))
				hcount++;
		}
	}

	// adjust compressed pas data to fit the size
	compressed_pas = Mem_Realloc( worldmodel->mempool, compressed_pas, total_size );

	// apply leaf pointers
	for( i = 0; i < worldmodel->numleafs; i++ )
		worldmodel->leafs[i].compressed_pas = compressed_pas + visofs[i];

	// release uncompressed data
	Mem_Free( uncompressed_vis );
	Mem_Free( visofs );	// release vis offsets

	// NOTE: we don't need to store off pointer to compressed pas-data
	// because this is will be automatiaclly frees by mempool internal pointer
	// and we never use this pointer after this point
	MsgDev( D_NOTE, "Average leaves visible / audible / total: %i / %i / %i\n", vcount / num, hcount / num, num );
	MsgDev( D_NOTE, "PAS building time: %g secs\n", Sys_DoubleTime() - timestart );
}

/*
=================
Mod_UnloadBrushModel

Release all uploaded textures
=================
*/
void Mod_UnloadBrushModel( model_t *mod )
{
	texture_t	*tx;
	int	i;

	ASSERT( mod != NULL );

	if( mod->type != mod_brush )
		return; // not a bmodel

	if( mod->name[0] != '*' )
	{
		for( i = 0; i < mod->numtextures; i++ )
		{
			tx = mod->textures[i];
			if( !tx || tx->gl_texturenum == tr.defaultTexture )
				continue;	// free slot

			GL_FreeTexture( tx->gl_texturenum );	// main texture
			GL_FreeTexture( tx->fb_texturenum );	// luma texture
		}

		Mem_FreePool( &mod->mempool );
	}

	Q_memset( mod, 0, sizeof( *mod ));
}

/*
=================
Mod_LoadBrushModel
=================
*/
static void Mod_LoadBrushModel( model_t *mod, const void *buffer, qboolean *loaded )
{
	int	i, j;
	char	*ents;
	dheader_t	*header;
	dmodel_t 	*bm;

	if( loaded ) *loaded = false;	
	header = (dheader_t *)buffer;
	loadmodel->type = mod_brush;
	i = header->version;

	switch( i )
	{
	case 28:	// get support for quake1 beta
		i = Q1BSP_VERSION;
		break;
	case Q1BSP_VERSION:
	case HLBSP_VERSION:
		break;
	default:
		MsgDev( D_ERROR, "%s has wrong version number (%i should be %i)", loadmodel->name, i, HLBSP_VERSION );
		return;
	}

	// will be merged later
	if( world.loading ) world.version = i;
	bmodel_version = i;	// share it

	// swap all the lumps
	mod_base = (byte *)header;
	loadmodel->mempool = Mem_AllocPool( va( "sv: ^2%s^7", loadmodel->name ));

	// load into heap
	if( header->lumps[LUMP_ENTITIES].fileofs <= 1024 && (header->lumps[LUMP_ENTITIES].filelen % sizeof( dplane_t )) == 0 )
	{
		// blue-shift swapped lumps
		Mod_LoadEntities( &header->lumps[LUMP_PLANES] );
		Mod_LoadPlanes( &header->lumps[LUMP_ENTITIES] );
	}
	else
	{
		// normal half-life lumps
		Mod_LoadEntities( &header->lumps[LUMP_ENTITIES] );
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
	Mod_LoadMarkSurfaces( &header->lumps[LUMP_MARKSURFACES] );
	Mod_LoadLeafs( &header->lumps[LUMP_LEAFS] );
	Mod_LoadNodes( &header->lumps[LUMP_NODES] );
	Mod_LoadClipnodes( &header->lumps[LUMP_CLIPNODES] );
	Mod_LoadSubmodels( &header->lumps[LUMP_MODELS] );

	Mod_MakeHull0 ();
	
	loadmodel->numframes = 2;	// regular and alternate animation
	ents = loadmodel->entities;
	
	// set up the submodels
	for( i = 0; i < mod->numsubmodels; i++ )
	{
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for( j = 1; j < MAX_MAP_HULLS; j++ )
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes - 1;
		}
		
		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		VectorCopy( bm->mins, mod->mins );		
		VectorCopy( bm->maxs, mod->maxs );

		mod->radius = RadiusFromBounds( mod->mins, mod->maxs );
		mod->numleafs = bm->visleafs;
		mod->flags = 0;

		if( i != 0 )
		{
			Mod_FindModelOrigin( ents, va( "*%i", i ), bm->origin );

			// flag 2 is indicated model with origin brush!
			if( !VectorIsNull( bm->origin )) mod->flags |= MODEL_HAS_ORIGIN;
		}

		for( j = 0; i != 0 && j < mod->nummodelsurfaces; j++ )
		{
			msurface_t	*surf = mod->surfaces + mod->firstmodelsurface + j;
			mextrasurf_t	*info = SURF_INFO( surf, mod );

			if( surf->flags & SURF_CONVEYOR )
				mod->flags |= MODEL_CONVEYOR;

			// kill water backplanes for submodels (half-life rules)
			if( surf->flags & SURF_DRAWTURB )
			{
				if( surf->plane->type == PLANE_Z )
				{
					// kill bottom plane too
					if( info->mins[2] == bm->mins[2] + 1 )
						surf->flags |= SURF_WATERCSG;
				}
				else
				{
					// kill side planes
					surf->flags |= SURF_WATERCSG;
				}
			}
		}

		if( i < mod->numsubmodels - 1 )
		{
			char	name[8];

			// duplicate the basic information
			Q_snprintf( name, sizeof( name ), "*%i", i + 1 );
			loadmodel = Mod_FindName( name, true );
			*loadmodel = *mod;
			Q_strncpy( loadmodel->name, name, sizeof( loadmodel->name ));
			loadmodel->mempool = NULL;
			mod = loadmodel;
		}
	}

	if( loaded ) *loaded = true;	// all done
}

/*
==================
Mod_FindName

==================
*/
model_t *Mod_FindName( const char *filename, qboolean create )
{
	model_t	*mod;
	char	name[64];
	int	i;
	
	if( !filename || !filename[0] )
		return NULL;

	if( *filename == '!' ) filename++;
	Q_strncpy( name, filename, sizeof( name ));
	COM_FixSlashes( name );
		
	// search the currently loaded models
	for( i = 0, mod = cm_models; i < cm_nummodels; i++, mod++ )
	{
		if( !mod->name[0] ) continue;
		if( !Q_stricmp( mod->name, name ))
		{
			// prolonge registration
			mod->needload = world.load_sequence;
			return mod;
		}
	}

	if( !create ) return NULL;			

	// find a free model slot spot
	for( i = 0, mod = cm_models; i < cm_nummodels; i++, mod++ )
		if( !mod->name[0] ) break; // this is a valid spot

	if( i == cm_nummodels )
	{
		if( cm_nummodels == MAX_MODELS )
			Host_Error( "Mod_ForName: MAX_MODELS limit exceeded\n" );
		cm_nummodels++;
	}

	// copy name, so model loader can find model file
	Q_strncpy( mod->name, name, sizeof( mod->name ));

	return mod;
}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t *Mod_LoadModel( model_t *mod, qboolean crash )
{
	byte	*buf;
	char	tempname[64];
	qboolean	loaded;

	if( !mod )
	{
		if( crash ) Host_Error( "Mod_ForName: NULL model\n" );
		else MsgDev( D_ERROR, "Mod_ForName: NULL model\n" );
		return NULL;		
	}

	// check if already loaded (or inline bmodel)
	if( mod->mempool || mod->name[0] == '*' )
		return mod;

	// store modelname to show error
	Q_strncpy( tempname, mod->name, sizeof( tempname ));
	COM_FixSlashes( tempname );

	buf = FS_LoadFile( tempname, NULL, false );

	if( !buf )
	{
		Q_memset( mod, 0, sizeof( model_t ));

		if( crash ) Host_Error( "Mod_ForName: %s couldn't load\n", tempname );
		else MsgDev( D_ERROR, "Mod_ForName: %s couldn't load\n", tempname );

		return NULL;
	}

	FS_FileBase( mod->name, modelname );

	MsgDev( D_NOTE, "Mod_LoadModel: %s\n", mod->name );
	mod->needload = world.load_sequence; // register mod
	mod->type = mod_bad;
	loadmodel = mod;

	// call the apropriate loader
	switch( *(uint *)buf )
	{
	case IDSTUDIOHEADER:
		Mod_LoadStudioModel( mod, buf, &loaded );
		break;
	case IDSPRITEHEADER:
		Mod_LoadSpriteModel( mod, buf, &loaded );
		break;
	case Q1BSP_VERSION:
	case HLBSP_VERSION:
		Mod_LoadBrushModel( mod, buf, &loaded );
		break;
	default:
		Mem_Free( buf );
		if( crash ) Host_Error( "Mod_ForName: %s unknown format\n", tempname );
		else MsgDev( D_ERROR, "Mod_ForName: %s unknown format\n", tempname );
		return NULL;
	}

	Mem_Free( buf ); 

	if( !loaded )
	{
		Mod_FreeModel( mod );

		if( crash ) Host_Error( "Mod_ForName: %s couldn't load\n", tempname );
		else MsgDev( D_ERROR, "Mod_ForName: %s couldn't load\n", tempname );
		return NULL;
	}
	return mod;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName( const char *name, qboolean crash )
{
	model_t	*mod;
	
	mod = Mod_FindName( name, true );
	return Mod_LoadModel( mod, crash );
}

/*
==================
Mod_LoadWorld

Loads in the map and all submodels
==================
*/
void Mod_LoadWorld( const char *name, uint *checksum, qboolean force )
{
	int	i;

	// now replacement table is invalidate
	Q_memset( com_models, 0, sizeof( com_models ));

	if( !Q_stricmp( cm_models[0].name, name ) && !force )
	{
		// singleplayer mode: server already loading map
		com_models[1] = cm_models; // make link to world
		if( checksum ) *checksum = world.checksum;

		// still have the right version
		return;
	}

	// clear all studio submodels on restart
	// HACKHACK: throw all external BSP-models to refresh their lightmaps properly
	for( i = 1; i < cm_nummodels; i++ )
	{
		if( cm_models[i].type == mod_studio )
			cm_models[i].submodels = NULL;
		else if( cm_models[i].type == mod_brush )
			Mod_FreeModel( cm_models + i );
	}

	// purge all submodels
	Mem_EmptyPool( com_studiocache );
	Mod_FreeModel( &cm_models[0] );
	world.load_sequence++;	// now all models are invalid

	// load the newmap
	world.loading = true;
	worldmodel = Mod_ForName( name, true );
	com_models[1] = cm_models; // make link to world
	CRC32_MapFile( &world.checksum, worldmodel->name );
	world.loading = false;

	if( checksum ) *checksum = world.checksum;
		
	// calc Potentially Hearable Set and compress it
	Mod_CalcPHS();
}

/*
==================
Mod_FreeUnused

Purge all unused models
==================
*/
void Mod_FreeUnused( void )
{
	model_t	*mod;
	int	i;

	for( i = 0, mod = cm_models; i < cm_nummodels; i++, mod++ )
	{
		if( !mod->name[0] ) continue;
		if( mod->needload != world.load_sequence )
			Mod_FreeModel( mod );
	}
}

/*
===================
Mod_GetType
===================
*/
modtype_t Mod_GetType( int handle )
{
	model_t	*mod = Mod_Handle( handle );

	if( !mod ) return mod_bad;
	return mod->type;
}

/*
===================
Mod_GetFrames
===================
*/
void Mod_GetFrames( int handle, int *numFrames )
{
	model_t	*mod = Mod_Handle( handle );

	if( !numFrames ) return;
	if( !mod )
	{
		*numFrames = 1;
		return;
	}

	*numFrames = mod->numframes;
	if( *numFrames < 1 ) *numFrames = 1;
}

/*
===================
Mod_GetBounds
===================
*/
void Mod_GetBounds( int handle, vec3_t mins, vec3_t maxs )
{
	model_t	*cmod;

	if( handle <= 0 ) return;
	cmod = Mod_Handle( handle );

	if( cmod )
	{
		if( mins ) VectorCopy( cmod->mins, mins );
		if( maxs ) VectorCopy( cmod->maxs, maxs );
	}
	else
	{
		MsgDev( D_ERROR, "Mod_GetBounds: NULL model %i\n", handle );
		if( mins ) VectorClear( mins );
		if( maxs ) VectorClear( maxs );
	}
}

/*
===============
Mod_Calloc

===============
*/
void *Mod_Calloc( int number, size_t size )
{
	cache_user_t *cu;

	if( number <= 0 || size <= 0 ) return NULL;
	cu = (cache_user_t *)Mem_Alloc( com_studiocache, sizeof( cache_user_t ) + number * size );
	cu->data = (void *)cu; // make sure what cu->data is not NULL

	return cu;
}

/*
===============
Mod_CacheCheck

===============
*/
void *Mod_CacheCheck( cache_user_t *c )
{
	return Cache_Check( com_studiocache, c );
}

/*
===============
Mod_LoadCacheFile

===============
*/
void Mod_LoadCacheFile( const char *filename, cache_user_t *cu )
{
	byte	*buf;
	string	name;
	size_t	i, j, size;

	ASSERT( cu != NULL );

	if( !filename || !filename[0] ) return;

	// eliminate '!' symbol (i'm doesn't know what this doing)
	for( i = j = 0; i < Q_strlen( filename ); i++ )
	{
		if( filename[i] == '!' ) continue;
		else if( filename[i] == '\\' ) name[j] = '/';
		else name[j] = Q_tolower( filename[i] );
		j++;
	}
	name[j] = '\0';

	buf = FS_LoadFile( name, &size, false );
	if( !buf || !size ) Host_Error( "LoadCacheFile: ^1can't load %s^7\n", filename );
	cu->data = Mem_Alloc( com_studiocache, size );
	Q_memcpy( cu->data, buf, size );
	Mem_Free( buf );
}

/*
===================
Mod_RegisterModel

register model with shared index
===================
*/
qboolean Mod_RegisterModel( const char *name, int index )
{
	model_t	*mod;

	if( index < 0 || index > MAX_MODELS )
		return false;

	// this array used for acess to servermodels
	mod = Mod_ForName( name, false );
	com_models[index] = mod;

	return ( mod != NULL );
}

/*
===============
Mod_Extradata

===============
*/
void *Mod_Extradata( model_t *mod )
{
	if( mod && mod->type == mod_studio )
		return mod->cache.data;
	return NULL;
}

/*
==================
Mod_Handle

==================
*/
model_t *Mod_Handle( int handle )
{
	if( handle < 0 || handle > MAX_MODELS )
	{
		Host_Error( "Mod_Handle: bad handle #%i\n", handle );
		return NULL;
	}
	return com_models[handle];
}