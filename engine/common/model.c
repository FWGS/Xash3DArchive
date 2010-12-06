//=======================================================================
//			Copyright XashXT Group 2007 ©
//			    model.c - modelloader
//=======================================================================

#include "cm_local.h"
#include "sprite.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "studio.h"
#include "wadfile.h"
#include "world.h"
#include "gl_local.h"

clipmap_t		cm;

byte		*mod_base;
static model_t	*sv_models[MAX_MODELS];	// server replacement modeltable
static model_t	cm_inline[MAX_MAP_MODELS];	// inline bsp models
static model_t	cm_models[MAX_MODELS];
static int	cm_nummodels;

model_t		*loadmodel;
model_t		*worldmodel;

// cvars
convar_t		*sv_novis;		// disable server culling entities by vis
convar_t		*gl_subdivide_size;

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

			CM COMMON UTILS

===============================================================================
*/
script_t *CM_GetEntityScript( void )
{
	string	entfilename;
	script_t	*ents;

	if( !worldmodel )
		return NULL;

	// check for entfile too
	com.strncpy( entfilename, worldmodel->name, sizeof( entfilename ));
	FS_StripExtension( entfilename );
	FS_DefaultExtension( entfilename, ".ent" );

	if(( ents = Com_OpenScript( entfilename, NULL, 0 )))
	{
		MsgDev( D_INFO, "^2Read entity patch:^7 %s\n", entfilename );
		return ents;
	}
	return cm.entityscript;
}

void CM_SetupHulls( float mins[4][3], float maxs[4][3] )
{
	Mem_Copy( mins, cm_hullmins, sizeof( cm_hullmins ));
	Mem_Copy( maxs, cm_hullmaxs, sizeof( cm_hullmaxs ));
}

/*
================
CM_StudioBodyVariations
================
*/
static int CM_StudioBodyVariations( model_t *mod )
{
	studiohdr_t	*pstudiohdr;
	mstudiobodyparts_t	*pbodypart;
	int		i, count;

	pstudiohdr = (studiohdr_t *)Mod_Extradata( mod );
	if( !pstudiohdr ) return 0;

	count = 1;
	pbodypart = (mstudiobodyparts_t *)((byte *)pstudiohdr + pstudiohdr->bodypartindex);

	// each body part has nummodels variations so there are as many total variations as there
	// are in a matrix of each part by each other part
	for( i = 0; i < pstudiohdr->numbodyparts; i++ )
	{
		count = count * pbodypart[i].nummodels;
	}
	return count;
}

/*
==================
Mod_PointInLeaf

==================
*/
mleaf_t *Mod_PointInLeaf( const vec3_t p, mnode_t *node )
{
	mplane_t	*plane;

	do
	{
		plane = node->plane;
		node = node->children[PlaneDiff( p, plane ) < 0];
	} while( node->contents >= 0 );

	return (mleaf_t *)node;
}

/*
==================
Mod_LeafPVS

==================
*/
byte *Mod_LeafPVS( mleaf_t *leaf, model_t *model )
{
	if( !model || !leaf || leaf == model->leafs || !model->visdata )
		return cm.nullrow;
	return Mod_DecompressVis( leaf->compressed_vis );
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
	return Mod_PointInLeaf( p, worldmodel->nodes ) - worldmodel->leafs - 1;
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

	cm.checkcount++;
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
===============================================================================

POINT TESTING IN HULLS

===============================================================================
*/
/*
==================
Mod_HullPointContents

==================
*/
int Mod_HullPointContents( hull_t *hull, int num, const vec3_t p )
{
	while( num >= 0 )
		num = hull->clipnodes[num].children[(hull->planes[hull->clipnodes[num].planenum].type < 3 ? p[hull->planes[hull->clipnodes[num].planenum].type] : DotProduct (hull->planes[hull->clipnodes[num].planenum].normal, p)) < hull->planes[hull->clipnodes[num].planenum].dist];
	return num;
}

/*
==================
Mod_PointContents

==================
*/
int Mod_PointContents( const vec3_t p )
{
	if( !worldmodel ) return 0;
	return Mod_HullPointContents( &worldmodel->hulls[0], 0, p );
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

	MsgDev( D_INFO, "Mod_FreeModel: %s\n", mod->name );

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

			CM INITALIZE\SHUTDOWN

===============================================================================
*/

void Mod_Init( void )
{
	sv_novis = Cvar_Get( "sv_novis", "0", 0, "force to ignore server visibility" );
	gl_subdivide_size = Cvar_Get( "gl_subdivide_size", "128", CVAR_ARCHIVE, "how large water polygons should be" );

	cm.studiopool = Mem_AllocPool( "Studio Cache" );
	Mem_Set( cm.nullrow, 0xFF, MAX_MAP_LEAFS / 8 );
}

void Mod_Shutdown( void )
{
	int	i;

	for( i = 0; i < cm_nummodels; i++ )
		Mod_FreeModel( &cm_models[i] );

	Mem_FreePool( &cm.studiopool );
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
static void Mod_LoadSubmodels( dlump_t *l )
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
	}
}

/*
=================
Mod_LoadTextures
=================
*/
static void Mod_LoadTextures( dlump_t *l )
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

		Msg( "texname %s\n", texname );

		tx->width = mt->width;
		tx->height = mt->height;

		// check for sky texture (quake1 only!)
		if( cm.version == Q1BSP_VERSION && !com.strncmp( mt->name, "sky", 3 ))
		{	
			R_InitSky( mt, tx );
		}
		else if( mt->offsets[0] > 0 )
		{
			// NOTE: imagelib detect miptex version by size
			// 770 additional bytes is indicated custom palette
			int size = (int)sizeof( mip_t ) + ((mt->width * mt->height * 85)>>6);
			if( cm.version == HLBSP_VERSION ) size += sizeof( short ) + 768;

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
				if( cm.version == HLBSP_VERSION ) size += sizeof( short ) + 768;

				tx->fb_texturenum = GL_LoadTexture( texname, (byte *)mt, size, TF_MAKELUMA );
			}
			else
			{
				// okay, loading it from wad
				tx->fb_texturenum = GL_LoadTexture( texname, NULL, 0, TF_MAKELUMA );
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

	switch( cm.version )
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

	if( surf->flags & SURF_DRAWTILED )
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
Mod_LoadSurfaces
=================
*/
static void Mod_LoadSurfaces( const dlump_t *l )
{
	dface_t		*in;
	msurface_t	*out;
	int		i, j, count;
	int		lightofs;

	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( dface_t ))
		Host_Error( "Mod_LoadSurfaces: funny lump size in '%s'\n", loadmodel->name );
	count = l->filelen / sizeof( dface_t );

	loadmodel->numsurfaces = count;
	loadmodel->surfaces = Mem_Alloc( loadmodel->mempool, count * sizeof( msurface_t ));
	out = loadmodel->surfaces;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->firstedge = in->firstedge;
		out->numedges = in->numedges;

		if(( out->firstedge + out->numedges ) > loadmodel->numsurfedges )
		{
			Msg( "Bad surface %i from %i\n", i, count );
			continue;
		} 

		if( in->side ) out->flags |= SURF_PLANEBACK;
		out->plane = loadmodel->planes + in->planenum;
		out->texinfo = loadmodel->texinfo + in->texinfo;

		// some DMC maps have bad textures
		if( out->texinfo->texture )
		{
			texture_t	*tex = out->texinfo->texture;

			if( !com.strncmp( tex->name, "sky", 3 ))
				out->flags |= (SURF_DRAWSKY|SURF_DRAWTILED);

			if( tex->name[0] == '*' || tex->name[0] == '!' || !com.strnicmp( tex->name, "water", 5 ))
				out->flags |= (SURF_DRAWTURB|SURF_DRAWTILED);
		}

		Mod_CalcSurfaceExtents( out );

		if( out->flags & SURF_DRAWTILED ) lightofs = -1;
		else lightofs = in->lightofs;

		for( j = 0; j < MAXLIGHTMAPS; j++ )
			out->styles[j] = in->styles[j];

		if( loadmodel->lightdata && lightofs != -1 )
		{
			if( cm.version == HLBSP_VERSION )
				out->samples = loadmodel->lightdata + (lightofs / 3);
			else out->samples = loadmodel->lightdata + lightofs;
		}

		if( out->flags & SURF_DRAWTILED && lightofs == -1 )
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
static void Mod_LoadNodes( dlump_t *l )
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
static void Mod_LoadLeafs( dlump_t *l )
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
static void Mod_LoadPlanes( dlump_t *l )
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
static void Mod_LoadVisibility( dlump_t *l )
{
	if( !l->filelen )
	{
		MsgDev( D_WARN, "map ^2%s^7 has no visibility\n", loadmodel->name );
		cm.pvs = cm.phs = loadmodel->visdata = NULL;
		return;
	}

	loadmodel->visdata = Mem_Alloc( loadmodel->mempool, l->filelen );
	Mem_Copy( loadmodel->visdata, (void *)(mod_base + l->fileofs), l->filelen );
}

/*
=================
Mod_LoadEntities
=================
*/
static void Mod_LoadEntities( dlump_t *l )
{
	byte	*in;

	in = (void *)(mod_base + l->fileofs);
	loadmodel->entities = Mem_Alloc( loadmodel->mempool, l->filelen );	
	Mem_Copy( loadmodel->entities, mod_base + l->fileofs, l->filelen );
	cm.entityscript = Com_OpenScript( "entities", in, l->filelen );
}

/*
=================
Mod_LoadClipnodes
=================
*/
static void Mod_LoadClipnodes( dlump_t *l )
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
	VectorSubtract( hull->clip_maxs, hull->clip_mins, cm.hull_sizes[1] );

	hull = &loadmodel->hulls[2];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	VectorCopy( GI->client_mins[2], hull->clip_mins ); // copy large hull
	VectorCopy( GI->client_maxs[2], hull->clip_maxs );
	VectorSubtract( hull->clip_maxs, hull->clip_mins, cm.hull_sizes[2] );

	hull = &loadmodel->hulls[3];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	VectorCopy( GI->client_mins[3], hull->clip_mins ); // copy head hull
	VectorCopy( GI->client_maxs[3], hull->clip_maxs );
	VectorSubtract( hull->clip_maxs, hull->clip_mins, cm.hull_sizes[3] );

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

	for( i = 0; i < mod->numtextures; i++ )
	{
		tx = mod->textures[i];
		if( !tx ) continue;	// free slot

		GL_FreeTexture( tx->gl_texturenum );	// main texture
		GL_FreeTexture( tx->fb_texturenum );	// luma texture
	}

	Mem_FreePool( &mod->mempool );
	Mem_Set( mod, 0, sizeof( *mod ));
}

/*
=================
CM_BrushModel
=================
*/
static void CM_BrushModel( model_t *mod, byte *buffer )
{
	dheader_t	*header;
	dmodel_t 	*bm;
	int	i, j;			
	
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
		MsgDev( D_ERROR, "CM_BrushModel: %s has wrong version number (%i should be %i)", loadmodel->name, i, HLBSP_VERSION );
		return;
	}

	// will be merged later
	loadmodel->type = mod_brush;
	cm.version = i;

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
	cm.version = 0;
	
	// set up the submodels (FIXME: this is confusing)
	for( i = 0; i < mod->numsubmodels; i++ )
	{
		model_t	*starmod;

		bm = &mod->submodels[i];
		starmod = &cm_inline[i];

		*starmod = *loadmodel;

		starmod->type = mod_brush;
		starmod->hulls[0].firstclipnode = bm->headnode[0];

		for( j = 1; j < MAX_MAP_HULLS; j++ )
		{
			starmod->hulls[j].firstclipnode = bm->headnode[j];
			starmod->hulls[j].lastclipnode = mod->numclipnodes - 1;
		}
		
		starmod->firstmodelsurface = bm->firstface;
		starmod->nummodelsurfaces = bm->numfaces;
		starmod->numleafs = bm->visleafs;
	
		VectorCopy( bm->maxs, starmod->maxs );
		VectorCopy( bm->mins, starmod->mins );

		// copy worldinfo back to cm_models[0]
		if( i == 0 ) *loadmodel = *starmod;

		starmod->numleafs = bm->visleafs;
		com.sprintf( starmod->name, "*%i", i + 1 );
	}
}

static void CM_StudioModel( model_t *mod, byte *buffer )
{
	studiohdr_t	*phdr;
	mstudioseqdesc_t	*pseqdesc;

	phdr = (studiohdr_t *)buffer;
	if( phdr->version != STUDIO_VERSION )
	{
		MsgDev( D_ERROR, "CM_StudioModel: %s has wrong version number (%i should be %i)\n", loadmodel->name, phdr->version, STUDIO_VERSION );
		return;
	}

	loadmodel->type = mod_studio;
	pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
	loadmodel->numframes = pseqdesc[0].numframes;
	loadmodel->needload = cm.load_sequence;

	loadmodel->mempool = Mem_AllocPool( va("^2%s^7", loadmodel->name ));
	loadmodel->cache.data = Mem_Alloc( loadmodel->mempool, phdr->length );
	Mem_Copy( loadmodel->cache.data, buffer, phdr->length );

	// setup bounding box
	VectorCopy( phdr->bbmin, loadmodel->mins );
	VectorCopy( phdr->bbmax, loadmodel->maxs );
}

static void CM_SpriteModel( model_t *mod, byte *buffer )
{
	dsprite_t		*phdr;

	phdr = (dsprite_t *)buffer;

	if( phdr->version != SPRITE_VERSION )
	{
		MsgDev( D_ERROR, "CM_SpriteModel: %s has wrong version number (%i should be %i)\n", loadmodel->name, phdr->version, SPRITE_VERSION );
		return;
	}
          
	loadmodel->type = mod_sprite;
	loadmodel->numframes = phdr->numframes;
	loadmodel->needload = cm.load_sequence;

	// setup bounding box
	loadmodel->mins[0] = loadmodel->mins[1] = -phdr->bounds[0] / 2;
	loadmodel->maxs[0] = loadmodel->maxs[1] = phdr->bounds[0] / 2;
	loadmodel->mins[2] = -phdr->bounds[1] / 2;
	loadmodel->maxs[2] = phdr->bounds[1] / 2;
}

model_t *CM_ModForName( const char *name, qboolean world )
{
	byte	*buf;
	model_t	*mod;
	int	i, size;

	if( !name || !name[0] )
		return NULL;

	// fast check for worldmodel
	if( !com.strcmp( name, cm_models[0].name ))
		return &cm_models[0];

	// check for submodel
	if( name[0] == '*' ) 
	{
		i = com.atoi( name + 1 );
		if( i < 1 || !worldmodel || i >= worldmodel->numsubmodels )
		{
			MsgDev( D_ERROR, "CM_InlineModel: bad submodel number %d\n", i );
			return NULL;
		}
		return &cm_inline[i];
	}

	// search the currently loaded models
	for( i = 0, mod = cm_models; i < cm_nummodels; i++, mod++ )
          {
		if( !mod->name[0] ) continue;
		if( !com.strcmp( name, mod->name ))
		{
			// prolonge registration
			mod->needload = cm.load_sequence;
			return mod;
		}
	}

	// find a free model slot spot
	for( i = 0, mod = cm_models; i < cm_nummodels; i++, mod++ )
		if( !mod->name[0] ) break; // free spot

	if( i == cm_nummodels )
	{
		if( cm_nummodels == MAX_MODELS )
			Host_Error( "Mod_ForName: MAX_MODELS limit exceeded\n" );
		cm_nummodels++;
	}
	
	buf = FS_LoadFile( name, &size );
	if( !buf )
	{
		if( world ) Host_Error( "Mod_ForName: %s couldn't load\n", name );
		else MsgDev( D_ERROR, "Mod_ForName: %s couldn't load\n", name );
		return NULL;
	}

	// if it's world - calc the map checksum
	if( world ) CRC32_MapFile( &cm.checksum, name );

	MsgDev( D_NOTE, "CM_LoadModel: %s\n", name );
	com.strncpy( mod->name, name, sizeof( mod->name ));
	mod->needload = cm.load_sequence;	// register mod
	mod->type = mod_bad;
	loadmodel = mod;

	// call the apropriate loader
	switch( *(uint *)buf )
	{
	case IDSTUDIOHEADER:
		CM_StudioModel( mod, buf );
		break;
	case IDSPRITEHEADER:
		CM_SpriteModel( mod, buf );
		break;
	default:
		CM_BrushModel( mod, buf );
		break;
	}

	Mem_Free( buf ); 

	if( mod->type == mod_bad )
	{
		Mod_FreeModel( mod );

		// check for loading problems
		if( world ) Host_Error( "Mod_ForName: %s unknown format\n", name );
		else MsgDev( D_ERROR, "Mod_ForName: %s unknown format\n", name );
		return NULL;
	}

	return mod;
}

static void CM_FreeWorld( void )
{
	if( worldmodel )
		Mod_FreeModel( &cm_models[0] );

	if( cm.entityscript )
	{
		Com_CloseScript( cm.entityscript );
		cm.entityscript = NULL;
	}
	worldmodel = NULL;
}

/*
==================
CM_BeginRegistration

Loads in the map and all submodels
==================
*/
void CM_BeginRegistration( const char *name, qboolean clientload, uint *checksum )
{
	// now replacement table is invalidate
	Mem_Set( sv_models, 0, sizeof( sv_models ));

	// purge all submodels
	Mem_EmptyPool( cm.studiopool );

	if( !com.strlen( name ))
	{
		CM_FreeWorld ();
		sv_models[1] = NULL; // no worldmodel
		if( checksum ) *checksum = 0;
		return;
	}

	if( !com.strcmp( cm_models[0].name, name ))
	{
		// singleplayer mode: server already loading map
		if( checksum ) *checksum = cm.checksum;
		if( !clientload )
		{
			// reset the entity script
			Com_ResetScript( cm.entityscript );
		}
		sv_models[1] = cm_models; // make link to world

		// still have the right version
		return;
	}

	CM_FreeWorld ();
	cm.load_sequence++;	// now all models are invalid

	// load the newmap
	worldmodel = CM_ModForName( name, true );
	if( !worldmodel ) Host_Error( "Couldn't load %s\n", name );
	sv_models[1] = cm_models; // make link to world
		
	if( checksum ) *checksum = cm.checksum;

	CM_CalcPHS ();
}

void CM_EndRegistration( void )
{
	model_t	*mod;
	int	i;

	for( i = 0, mod = cm_models; i < cm_nummodels; i++, mod++ )
	{
		if( !mod->name[0] ) continue;
		if( mod->needload != cm.load_sequence )
			Mod_FreeModel( mod );
	}
}

/*
==================
CM_ClipHandleToModel
==================
*/
model_t *CM_ClipHandleToModel( int handle )
{
	if( handle < 0 || handle > MAX_MODELS )
	{
		Host_Error( "CM_ClipHandleToModel: bad handle #%i\n", handle );
		return NULL;
	}
	return sv_models[handle];
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

	if( mod->type == mod_sprite )
		*numFrames = mod->numframes;
	else if( mod->type == mod_studio )
		*numFrames = CM_StudioBodyVariations( mod );		
	if( *numFrames < 1 ) *numFrames = 1;
}

/*
===================
CM_GetModelType
===================
*/
modtype_t CM_GetModelType( int handle )
{
	model_t	*mod = CM_ClipHandleToModel( handle );

	if( !mod ) return mod_bad;
	return mod->type;
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

qboolean CM_RegisterModel( const char *name, int index )
{
	model_t	*mod;

	if( index < 0 || index > MAX_MODELS )
		return false;

	// this array used for acess to servermodels
	mod = CM_ModForName( name, false );
	sv_models[index] = mod;

	return ( mod != NULL );
}

/*
===============
Mod_Calloc

===============
*/
void *Mod_Calloc( int number, size_t size )
{
	if( number <= 0 || size <= 0 ) return NULL;
	return Mem_Alloc( cm.studiopool, number * size );
}

/*
===============
Mod_CacheCheck

===============
*/
void *Mod_CacheCheck( cache_user_t *c )
{
	return Cache_Check( cm.studiopool, c );
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
	cu->data = Mem_Alloc( cm.studiopool, size );
	Mem_Copy( cu->data, buf, size );
	Mem_Free( buf );
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