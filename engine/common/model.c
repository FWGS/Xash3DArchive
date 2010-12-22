//=======================================================================
//			Copyright XashXT Group 2007 ©
//			    model.c - modelloader
//=======================================================================

#include "cm_local.h"
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
	Mem_Copy( mins, cm_hullmins, sizeof( cm_hullmins ));
	Mem_Copy( maxs, cm_hullmaxs, sizeof( cm_hullmaxs ));
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

	// FIXME: Could save a loop here by traversing the tree in this routine like the code above
	count = Mod_BoxLeafnums( mins, maxs, leafList, MAX_BOX_LEAFS, NULL );

	for( i = 0; i < count; i++ )
	{
		int	leafnum = leafList[i];

		if( visbits[leafnum>>3] & (1<<( leafnum & 7 )))
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

	Mem_Set( cm_models, 0, sizeof( cm_models ));
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
	char		texname[32];
	mip_t		*mt;
	int 		i, j; 

	// release old sky layers first
	GL_FreeTexture( tr.solidskyTexture );
	GL_FreeTexture( tr.alphaskyTexture );
	tr.solidskyTexture = tr.alphaskyTexture = 0;

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
		if( in->dataofs[i] == -1 ) continue; // missed

		mt = (mip_t *)((byte *)in + in->dataofs[i] );

		if( !mt->name[0] )
		{
			MsgDev( D_WARN, "unnamed texture in %s\n", loadmodel->name );
			com.snprintf( mt->name, sizeof( mt->name ), "miptex_%i", i );
		}

		tx = Mem_Alloc( loadmodel->mempool, sizeof( *tx ));
		loadmodel->textures[i] = tx;

		// convert to lowercase
		com.strnlwr( mt->name, mt->name, sizeof( mt->name ));
		com.strncpy( tx->name, mt->name, sizeof( tx->name ));
		com.snprintf( texname, sizeof( texname ), "%s%s.mip", ( mt->offsets[0] > 0 ) ? "#" : "", mt->name );

		tx->width = mt->width;
		tx->height = mt->height;

		// check for sky texture (quake1 only!)
		if( world.version == Q1BSP_VERSION && !com.strncmp( mt->name, "sky", 3 ))
		{	
			R_InitSky( mt, tx );
		}
		else if( mt->offsets[0] > 0 )
		{
			// NOTE: imagelib detect miptex version by size
			// 770 additional bytes is indicated custom palette
			int size = (int)sizeof( mip_t ) + ((mt->width * mt->height * 85)>>6);
			if( world.version == HLBSP_VERSION ) size += sizeof( short ) + 768;

			tx->gl_texturenum = GL_LoadTexture( texname, (byte *)mt, size, 0 );
		}
		else
		{
			// okay, loading it from wad
			tx->gl_texturenum = GL_LoadTexture( texname, NULL, 0, 0 );
		}

		// check for luma texture
		if( R_GetTexture( tx->gl_texturenum )->flags & TF_HAS_LUMA )
		{
			com.snprintf( texname, sizeof( texname ), "%s%s_luma.mip", mt->offsets[0] > 0 ? "#" : "", mt->name );
			if( mt->offsets[0] > 0 )
			{
				// NOTE: imagelib detect miptex version by size
				// 770 additional bytes is indicated custom palette
				int size = (int)sizeof( mip_t ) + ((mt->width * mt->height * 85)>>6);
				if( world.version == HLBSP_VERSION ) size += sizeof( short ) + 768;

				tx->fb_texturenum = GL_LoadTexture( texname, (byte *)mt, size, TF_MAKELUMA|TF_NOMIPMAP );
			}
			else
			{
				// okay, loading it from wad
				tx->fb_texturenum = GL_LoadTexture( texname, NULL, 0, TF_MAKELUMA|TF_NOMIPMAP );
			}
		}

		// apply texture type (just for debug)
		GL_SetTextureType( tx->gl_texturenum, TEX_BRUSH );
		GL_SetTextureType( tx->fb_texturenum, TEX_BRUSH );
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
		Mem_Set( anims, 0, sizeof( anims ));
		Mem_Set( altanims, 0, sizeof( altanims ));

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

			if( com.strcmp( tx2->name + 2, tx->name + 2 ))
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

	switch( world.version )
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
		Mem_Copy( loadmodel->lightdata, in, l->filelen );
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

	if( surf->flags & ( SURF_DRAWSKY|SURF_DRAWTURB ))
	{
		surf->extents[0] = surf->extents[1] = 16384;
		surf->texturemins[0] = surf->texturemins[1] = -8192;
		return;
	}

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

		// some DMC maps have bad textures
		if( out->texinfo->texture )
		{
			texture_t	*tex = out->texinfo->texture;

			if( !com.strncmp( tex->name, "sky", 3 ))
				out->flags |= (SURF_DRAWTILED|SURF_DRAWSKY);

			if( tex->name[0] == '*' || tex->name[0] == '!' || !com.strnicmp( tex->name, "water", 5 ))
				out->flags |= (SURF_DRAWTURB|SURF_DRAWTILED);

			if( com.stristr( tex->name, "scroll" ))
				out->flags |= SURF_CONVEYOR;

			if( out->texinfo->flags & TEX_SPECIAL )
				out->flags |= SURF_DRAWTILED;
		}

		Mod_CalcSurfaceBounds( out, info );
		Mod_CalcSurfaceExtents( out );

		if( loadmodel->lightdata && in->lightofs != -1 )
		{
			if( world.version == HLBSP_VERSION )
				out->samples = loadmodel->lightdata + (in->lightofs / 3);
			else out->samples = loadmodel->lightdata + in->lightofs;
		}

		for( j = 0; j < MAXLIGHTMAPS; j++ )
			out->styles[j] = in->styles[j];

		if( out->flags & SURF_DRAWSKY && world.version == Q1BSP_VERSION )
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

	for( i = 0; i < count; i++, in++, out++ )
	{
		VectorCopy( in->point, out->position );
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

	Mem_Copy( loadmodel->surfedges, in, count * sizeof( dsurfedge_t ));
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
	if( !l->filelen )
	{
		MsgDev( D_WARN, "map ^2%s^7 has no visibility\n", loadmodel->name );
		loadmodel->visdata = NULL;
		world.visdatasize = 0;
		return;
	}

	loadmodel->visdata = Mem_Alloc( loadmodel->mempool, l->filelen );
	Mem_Copy( loadmodel->visdata, (void *)(mod_base + l->fileofs), l->filelen );
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
	Mem_Copy( loadmodel->entities, mod_base + l->fileofs, l->filelen );
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
	if( !worldmodel || !worldmodel->visdata )
		return;

	MsgDev( D_NOTE, "Building PAS...\n" );
	timestart = Sys_DoubleTime();

	// NOTE: first leaf is skipped becuase is a outside leaf. Now all leafs have shift up by 1.
	// and last leaf (which equal worldmodel->numleafs) has no visdata! Add extra one leaf
	// to avoid this situation. FIXME: this is need to be detail tested 
	num = worldmodel->numleafs + 1;
	rowwords = (num + 31)>>5;
	rowbytes = rowwords * 4;

	// typycally PHS enough more room because RLE fails on multiple 1 not 0
	phsdatasize = world.visdatasize * 4;

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
		Mem_Copy( scan, Mod_LeafPVS( worldmodel->leafs + i, worldmodel ), rowbytes );
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
		Mem_Copy( dest, scan, rowbytes );

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
			Host_Error( "CalcPHS: vismap expansion overflow %s > %s\n", memprint( total_size ), memprint( phsdatasize ));

		Mem_Copy( vismap_p, comp, rowsize );
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
	Msg( "VIS size %s, PAS size %s\n", memprint( world.visdatasize ), memprint( total_size ));

	// apply leaf pointers
	for( i = 0; i < worldmodel->numleafs; i++ )
		worldmodel->leafs[i].compressed_pas = compressed_pas + visofs[i];

	// release uncompressed data
	Mem_Free( uncompressed_vis );
	Mem_Free( visofs );	// release vis offsets

	// NOTE: we don't need to store off pointer to compressed pas-data
	// because this is will be automatiaclly frees by mempool internal pointer
	// and we not use this pointer any time after this point
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
			if( !tx ) continue;	// free slot

			GL_FreeTexture( tx->gl_texturenum );	// main texture
			GL_FreeTexture( tx->fb_texturenum );	// luma texture
		}

		Mem_FreePool( &mod->mempool );
	}

	Mem_Set( mod, 0, sizeof( *mod ));
}

/*
=================
Mod_LoadBrushModel
=================
*/
static void Mod_LoadBrushModel( model_t *mod, const void *buffer )
{
	int	i, j;
	dheader_t	*header;
	dmodel_t 	*bm;
	
	header = (dheader_t *)buffer;
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
	loadmodel->type = mod_brush;
	if( world.loading ) world.version = i;

	// swap all the lumps
	mod_base = (byte *)header;
	loadmodel->mempool = Mem_AllocPool( va( "sv: ^2%s^7", loadmodel->name ));

	// load into heap
	if( header->lumps[LUMP_PLANES].filelen % sizeof( dplane_t ))
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
	
	// set up the submodels (FIXME: this is confusing)
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
		mod->flags = 0; // clear flags

		for( j = 0; i != 0 && j < mod->nummodelsurfaces; j++ )
		{
			msurface_t	*surf = mod->surfaces + mod->firstmodelsurface + j;
			mextrasurf_t	*info = SURF_INFO( surf, mod );
			vec3_t		normal, vup = { 0, 0, 1 };

			if( surf->flags & SURF_CONVEYOR )
				mod->flags |= MODEL_CONVEYOR;

			// kill water backplanes for submodels (half-life rules)
			if( surf->flags & SURF_DRAWTURB )
			{
				if( surf->flags & SURF_PLANEBACK )
					VectorNegate( surf->plane->normal, normal );
				else VectorCopy( surf->plane->normal, normal );

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
			com.snprintf( name, sizeof( name ), "*%i", i + 1 );
			loadmodel = Mod_FindName( name, true );
			*loadmodel = *mod;
			com.strncpy( loadmodel->name, name, sizeof( loadmodel->name ));
			loadmodel->mempool = NULL;
			mod = loadmodel;
		}
	}
}

/*
==================
Mod_FindName

==================
*/
model_t *Mod_FindName( const char *name, qboolean create )
{
	model_t	*mod;
	int	i;
	
	if( !name || !name[0] )
		return NULL;
		
	// search the currently loaded models
	for( i = 0, mod = cm_models; i < cm_nummodels; i++, mod++ )
	{
		if( !mod->name[0] ) continue;
		if( !com.strcmp( mod->name, name ))
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
	com.strncpy( mod->name, name, sizeof( mod->name ));

	return mod;
}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t *Mod_LoadModel( model_t *mod, qboolean isWorld )
{
	byte	*buf;

	if( !mod )
	{
		if( isWorld ) Host_Error( "Mod_ForName: NULL model\n" );
		else MsgDev( D_ERROR, "Mod_ForName: NULL model\n" );
		return NULL;		
	}

	// check if already loaded (or inline bmodel)
	if( mod->mempool || mod->name[0] == '*' )
		return mod;

	buf = FS_LoadFile( mod->name, NULL );
	if( !buf )
	{
		if( isWorld ) Host_Error( "Mod_ForName: %s couldn't load\n", mod->name );
		else MsgDev( D_ERROR, "Mod_ForName: %s couldn't load\n", mod->name );
		Mem_Set( mod, 0, sizeof( model_t ));
		return NULL;
	}

	// if it's world - calc the map checksum
	if( isWorld ) CRC32_MapFile( &world.checksum, mod->name );
	world.loading = isWorld;

	MsgDev( D_NOTE, "Mod_LoadModel: %s\n", mod->name );
	mod->needload = world.load_sequence; // register mod
	mod->type = mod_bad;
	loadmodel = mod;

	// call the apropriate loader
	switch( *(uint *)buf )
	{
	case IDSTUDIOHEADER:
		Mod_LoadStudioModel( mod, buf );
		break;
	case IDSPRITEHEADER:
		Mod_LoadSpriteModel( mod, buf );
		break;
	case Q1BSP_VERSION:
	case HLBSP_VERSION:
		Mod_LoadBrushModel( mod, buf );
		break;
	}

	Mem_Free( buf ); 

	if( mod->type == mod_bad )
	{
		Mod_FreeModel( mod );

		// check for loading problems
		if( isWorld ) Host_Error( "Mod_ForName: %s unknown format\n", mod->name );
		else MsgDev( D_ERROR, "Mod_ForName: %s unknown format\n", mod->name );
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
model_t *Mod_ForName( const char *name, qboolean world )
{
	model_t	*mod;
	
	mod = Mod_FindName( name, true );
	return Mod_LoadModel( mod, world );
}

/*
==================
Mod_LoadWorld

Loads in the map and all submodels
==================
*/
void Mod_LoadWorld( const char *name, uint *checksum )
{
	int	i;

	// now replacement table is invalidate
	Mem_Set( com_models, 0, sizeof( com_models ));

	if( !com.strcmp( cm_models[0].name, name ))
	{
		// singleplayer mode: server already loading map
		com_models[1] = cm_models; // make link to world
		if( checksum ) *checksum = world.checksum;

		// still have the right version
		return;
	}

	// clear all studio submodels on restart
	for( i = 0; i < cm_nummodels; i++ )
	{
		if( cm_models[i].type != mod_studio )
			continue;
		cm_models[i].submodels = NULL;
	}

	// purge all submodels
	Mem_EmptyPool( com_studiocache );
	Mod_FreeModel( &cm_models[0] );
	world.load_sequence++;	// now all models are invalid

	// load the newmap
	worldmodel = Mod_ForName( name, true );
	com_models[1] = cm_models; // make link to world
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
	model_t	*mod = CM_ClipHandleToModel( handle );

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
	model_t	*mod = CM_ClipHandleToModel( handle );

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
	model_t	*cmod = CM_ClipHandleToModel( handle );

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
	if( number <= 0 || size <= 0 ) return NULL;
	return Mem_Alloc( com_studiocache, number * size );
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
void Mod_LoadCacheFile( const char *path, cache_user_t *cu )
{
	byte	*buf;
	string	filepath;
	size_t	i, size;

	ASSERT( cu != NULL );

	if( !path || !path[0] ) return;

	// replace all '\' with '/'
	for( i = ( path[0] == '/' ||path[0] == '\\' ), size = 0; path[i] && ( size < sizeof( filepath )-1 ); i++ )
	{
		if( path[i] == '\\' ) filepath[size++] = '/';
		else filepath[size++] = path[i];
	}

	if( !size ) return;
	filepath[size] = 0;

	buf = FS_LoadFile( filepath, &size );
	cu->data = Mem_Alloc( com_studiocache, size );
	Mem_Copy( cu->data, buf, size );
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
CM_ClipHandleToModel

FIXME: rename to Mod_Handle
==================
*/
model_t *CM_ClipHandleToModel( int handle )
{
	if( handle < 0 || handle > MAX_MODELS )
	{
		Host_Error( "CM_ClipHandleToModel: bad handle #%i\n", handle );
		return NULL;
	}
	return com_models[handle];
}