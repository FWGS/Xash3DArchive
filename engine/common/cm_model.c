//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_model.c - collision model
//=======================================================================

#include "cm_local.h"
#include "sprite.h"
#include "byteorder.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "studio.h"
#include "wadfile.h"
#include "const.h"

clipmap_t		cm;

byte		*mod_base;
static model_t	*sv_models[MAX_MODELS];	// server replacement modeltable
static model_t	cm_inline[MAX_MAP_MODELS];	// inline bsp models
static model_t	cm_models[MAX_MODELS];
static int	cm_nummodels;

model_t		*loadmodel;
model_t		*worldmodel;

// cvars
cvar_t		*cm_novis;

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

/*
================
CM_StudioBodyVariations
================
*/
static int CM_StudioBodyVariations( int handle )
{
	studiohdr_t	*pstudiohdr;
	mstudiobodyparts_t	*pbodypart;
	int		i, count;

	pstudiohdr = (studiohdr_t *)Mod_Extradata( handle );
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
================
CM_FreeModel
================
*/
static void CM_FreeModel( model_t *mod )
{
	if( !mod || !mod->mempool )
		return;

	Mem_FreePool( &mod->mempool );
	Mem_Set( mod, 0, sizeof( *mod ));
}

/*
===============================================================================

			CM INITALIZE\SHUTDOWN

===============================================================================
*/

bool CM_InitPhysics( void )
{
	cm_novis = Cvar_Get( "cm_novis", "0", 0, "force to ignore server visibility" );

	Mem_Set( cm.nullrow, 0xFF, MAX_MAP_LEAFS / 8 );
	return true;
}

void CM_FreePhysics( void )
{
	int	i;

	for( i = 0; i < cm_nummodels; i++ )
		CM_FreeModel( &cm_models[i] );
}

/*
===============================================================================

			MAP LOADING

===============================================================================
*/
/*
=================
BSP_LoadSubmodels
=================
*/
static void BSP_LoadSubmodels( dlump_t *l )
{
	dmodel_t	*in;
	dmodel_t	*out;
	int	i, j, count;
	
	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadModels: funny lump size\n" );
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
			out->mins[j] = LittleFloat( in->mins[j] ) - 1;
			out->maxs[j] = LittleFloat( in->maxs[j] ) + 1;
			out->origin[j] = LittleFloat( in->origin[j] );
		}

		for( j = 0; j < MAX_MAP_HULLS; j++ )
			out->headnode[j] = LittleLong( in->headnode[j] );

		out->visleafs = LittleLong( in->visleafs );
		out->firstface = LittleLong( in->firstface );
		out->numfaces = LittleLong( in->numfaces );
	}
}

/*
=================
BSP_LoadTextures
=================
*/
static void BSP_LoadTextures( dlump_t *l )
{
	dmiptexlump_t	*in;
	mtexture_t	*out;
	mip_t		*mt;
	int 		i;

	if( !l->filelen )
	{
		// no textures
		loadmodel->textures = NULL;
		return;
	}

	in = (void *)(mod_base + l->fileofs);
	in->nummiptex = LittleLong( in->nummiptex );

	loadmodel->numtextures = in->nummiptex;
	loadmodel->textures = (mtexture_t **)Mem_Alloc( loadmodel->mempool, loadmodel->numtextures * sizeof( mtexture_t* ));

	for( i = 0; i < loadmodel->numtextures; i++ )
	{
		in->dataofs[i] = LittleLong( in->dataofs[i] );
		if( in->dataofs[i] == -1 ) continue; // bad offset ?

		mt = (mip_t *)((byte *)in + in->dataofs[i] );
		out = Mem_Alloc( loadmodel->mempool, sizeof( *out ));
		loadmodel->textures[i] = out;

		com.strnlwr( mt->name, out->name, sizeof( out->name ));
//		out->contents = CM_ContentsFromShader( out->name );	// FIXME: implement
	}
}

/*
=================
BSP_LoadTexInfo
=================
*/
static void BSP_LoadTexInfo( const dlump_t *l )
{
	dtexinfo_t	*in;
	mtexinfo_t	*out;
	int		miptex;
	int		i, j, count;
	uint		surfaceParm = 0;

	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in ))
		Host_Error( "BSP_LoadTexInfo: funny lump size in %s\n", loadmodel->name );

	count = l->filelen / sizeof( *in );
          out = Mem_Alloc( loadmodel->mempool, count * sizeof( *out ));
	
	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		for( j = 0; j < 8; j++ )
			out->vecs[0][j] = LittleFloat( in->vecs[0][j] );

		miptex = LittleLong( in->miptex );
		if( miptex < 0 || miptex > loadmodel->numtextures )
			Host_Error( "BSP_LoadTexInfo: bad miptex number in '%s'\n", loadmodel->name );
		out->texture = loadmodel->textures[miptex];
	}
}

/*
=================
BSP_LoadLighting
=================
*/
static void BSP_LoadLighting( const dlump_t *l )
{
	byte	d, *in, *out;
	int	i;

	if( !l->filelen ) return;
	in = (void *)(mod_base + l->fileofs);

	switch( cm.version )
	{
	case Q1BSP_VERSION:
		// expand the white lighting data
		loadmodel->lightdata = Mem_Alloc( loadmodel->mempool, l->filelen * 3 );
		out = loadmodel->lightdata;

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
		loadmodel->lightdata = Mem_Alloc( loadmodel->mempool, l->filelen );
		Mem_Copy( loadmodel->lightdata, in, l->filelen );
		break;
	}
}

/*
=================
BSP_CalcSurfaceExtents

Fills in surf->textureMins and surf->extents
=================
*/
static void BSP_CalcSurfaceExtents( msurface_t *surf )
{
	float	mins[2], maxs[2], val;
	int	bmins[2], bmaxs[2];
	int	i, j, e;
	float	*v;

	if( surf->flags & SURF_DRAWTURB )
	{
		surf->extents[0] = surf->extents[1] = 16384;
		surf->texturemins[0] = surf->texturemins[1] = -8192;
		return;
	}

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -999999;

	for( i = 0; i < surf->numedges; i++ )
	{
		e = loadmodel->surfedges[surf->firstedge + i];
		if( e >= 0 ) v = (float *)&loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else v = (float *)&loadmodel->vertexes[loadmodel->edges[-e].v[1]];

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

		surf->texturemins[i] = bmins[i] * LM_SAMPLE_SIZE;
		surf->extents[i] = (bmaxs[i] - bmins[i]) * LM_SAMPLE_SIZE;
	}
}

/*
=================
BSP_LoadSurfaces
=================
*/
static void BSP_LoadSurfaces( const dlump_t *l )
{
	dface_t		*in;
	msurface_t	*out;
	int		i, count;
	int		lightofs;

	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( dface_t ))
		Host_Error( "BSP_LoadFaces: funny lump size in '%s'\n", loadmodel->name );
	count = l->filelen / sizeof( dface_t );

	loadmodel->numsurfaces = count;
	loadmodel->surfaces = Mem_Alloc( loadmodel->mempool, count * sizeof( msurface_t ));
	out = loadmodel->surfaces;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->firstedge = LittleLong( in->firstedge );
		out->numedges = LittleLong( in->numedges );

		if( LittleShort( in->side )) out->flags |= SURF_PLANEBACK;
		out->plane = loadmodel->planes + LittleLong( in->planenum );
		out->texinfo = loadmodel->texinfo + LittleLong( in->texinfo );

		if( !com.strncmp( out->texinfo->texture->name, "sky", 3 ))
			out->flags |= (SURF_DRAWSKY|SURF_DRAWTILED);

		if( out->texinfo->texture->name[0] == '*' || out->texinfo->texture->name[0] == '!' )
			out->flags |= (SURF_DRAWTURB|SURF_DRAWTILED);

		BSP_CalcSurfaceExtents( out );

		if( out->flags & SURF_DRAWTILED ) lightofs = -1;
		else lightofs = LittleLong( in->lightofs );

		if( loadmodel->lightdata && lightofs != -1 )
		{
			if( cm.version == HLBSP_VERSION )
				out->samples = loadmodel->lightdata + lightofs;
			else out->samples = loadmodel->lightdata + (lightofs * 3);
		}

		while( out->numstyles < LM_STYLES && in->styles[out->numstyles] != 255 )
		{
			out->styles[out->numstyles] = in->styles[out->numstyles];
			out->numstyles++;
		}
	}
}

/*
=================
BSP_LoadVertexes
=================
*/
static void BSP_LoadVertexes( const dlump_t *l )
{
	dvertex_t	*in;
	float	*out;
	int	i, j, count;

	in = (void *)( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ))
		Host_Error( "BSP_LoadVertexes: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof( *in );

	loadmodel->numvertexes = count;
	out = (float *)loadmodel->vertexes = Mem_Alloc( loadmodel->mempool, count * sizeof( vec3_t ));

	for( i = 0; i < count; i++, in++, out += 3 )
	{
		for( j = 0; j < 3; j++ )
			out[j] = LittleFloat( in->point[j] );
	}
}

/*
=================
BSP_LoadEdges
=================
*/
static void BSP_LoadEdges( const dlump_t *l )
{
	dedge_t	*in, *out;
	int	i, count;

	in = (void *)( mod_base + l->fileofs );	
	if( l->filelen % sizeof( *in ))
		Host_Error( "BSP_LoadEdges: funny lump size in %s\n", loadmodel->name );

	count = l->filelen / sizeof( dedge_t );
	loadmodel->edges = out = Mem_Alloc( loadmodel->mempool, count * sizeof( dedge_t ));
	loadmodel->numedges = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->v[0] = (word)LittleShort( in->v[0] );
		out->v[1] = (word)LittleShort( in->v[1] );
	}
}

/*
=================
BSP_LoadSurfEdges
=================
*/
static void BSP_LoadSurfEdges( const dlump_t *l )
{
	dsurfedge_t	*in, *out;
	int		i, count;

	in = (void *)( mod_base + l->fileofs );	
	if( l->filelen % sizeof( *in ))
		Host_Error( "BSP_LoadSurfEdges: funny lump size in %s\n", loadmodel->name );

	count = l->filelen / sizeof( dsurfedge_t );
	loadmodel->surfedges = out = Mem_Alloc( loadmodel->mempool, count * sizeof( dsurfedge_t ));
	loadmodel->numsurfedges = count;

	for( i = 0; i < count; i++ )
		out[i] = LittleLong( in[i] );
}

/*
=================
BSP_LoadMarkFaces
=================
*/
static void BSP_LoadMarkFaces( const dlump_t *l )
{
	dmarkface_t	*in;
	int		i, j, count;

	in = (void *)( mod_base + l->fileofs );	
	if( l->filelen % sizeof( *in ))
		Host_Error( "BSP_LoadMarkFaces: funny lump size in %s\n", loadmodel->name );

	count = l->filelen / sizeof( *in );
	loadmodel->marksurfaces = Mem_Alloc( loadmodel->mempool, count * sizeof( msurface_t* ));

	for( i = 0; i < count; i++ )
	{
		j = LittleLong( in[i] );
		if( j < 0 ||  j >= loadmodel->numsurfaces )
			Host_Error( "BSP_LoadMarkFaces: bad surface number in '%s'\n", loadmodel->name );
		loadmodel->marksurfaces[i] = loadmodel->surfaces + j;
	}
}

/*
=================
CM_SetParent
=================
*/
static void CM_SetParent( mnode_t *node, mnode_t *parent )
{
	node->parent = parent;

	if( node->contents < 0 ) return; // it's node
	CM_SetParent( node->children[0], node );
	CM_SetParent( node->children[1], node );
}

/*
=================
BSP_LoadNodes
=================
*/
static void BSP_LoadNodes( dlump_t *l )
{
	dnode_t	*in;
	mnode_t	*out;
	int	i, j, p;
	
	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadNodes: funny lump size\n" );
	loadmodel->numnodes = l->filelen / sizeof( *in );

	if( loadmodel->numnodes < 1 ) Host_Error( "Map %s has no nodes\n", loadmodel->name );
	out = loadmodel->nodes = (mnode_t *)Mem_Alloc( loadmodel->mempool, loadmodel->numnodes * sizeof( *out ));

	for( i = 0; i < loadmodel->numnodes; i++, out++, in++ )
	{
		p = LittleLong( in->planenum );
		out->plane = loadmodel->planes + p;
		out->contents = CONTENTS_NODE;
		out->firstface = loadmodel->surfaces + LittleLong( in->firstface );
		out->numfaces = LittleLong( in->numfaces );

		for( j = 0; j < 2; j++ )
		{
			p = LittleShort( in->children[j] );
			if( p >= 0 ) out->children[j] = loadmodel->nodes + p;
			else out->children[j] = (mnode_t *)(loadmodel->leafs + ( -1 - p ));
		}
	}

	// sets nodes and leafs
	CM_SetParent( loadmodel->nodes, NULL );
}

/*
=================
BSP_LoadLeafs
=================
*/
static void BSP_LoadLeafs( dlump_t *l )
{
	dleaf_t 	*in;
	mleaf_t	*out;
	int	i, j, p, count;
		
	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadLeafs: funny lump size\n" );

	count = l->filelen / sizeof( *in );
	if( count < 1 ) Host_Error( "Map %s with no leafs\n", loadmodel->name );
	out = (mleaf_t *)Mem_Alloc( loadmodel->mempool, count * sizeof( *out ));

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		p = LittleLong( in->contents );
		out->contents = p;
		out->plane = NULL;	// differentiate to nodes
	
		p = LittleLong( in->visofs );

		if( p == -1 ) out->visdata = NULL;
		else out->visdata = cm.pvs + p;

		// will be initialized later
		out->pasdata = NULL;

		for( j = 0; j < 4; j++ )
			out->ambient_sound_level[j] = in->ambient_level[j];

		out->firstmarksurface = loadmodel->marksurfaces + LittleShort( in->firstmarksurface );
		out->nummarksurfaces = LittleShort( in->nummarksurfaces );
	}

	if( loadmodel->leafs[0].contents != CONTENTS_SOLID )
		Host_Error( "BSP_LoadLeafs: Map %s has leaf 0 is not CONTENTS_SOLID\n", loadmodel->name );
}

/*
=================
BSP_LoadPlanes
=================
*/
static void BSP_LoadPlanes( dlump_t *l )
{
	dplane_t	*in;
	mplane_t	*out;
	int	i, j, count;
	
	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadPlanes: funny lump size\n" );
	count = l->filelen / sizeof( *in );

	if( count < 1 ) Host_Error( "Map %s with no planes\n", loadmodel->name );
	out = (mplane_t *)Mem_Alloc( loadmodel->mempool, count * sizeof( *out ));

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		for( j = 0; j < 3; j++ )
		{
			out->normal[j] = LittleFloat( in->normal[j] );
			if( out->normal[j] < 0.0f ) out->signbits |= 1<<j;
		}

		out->dist = LittleFloat( in->dist );
		out->type = LittleLong( in->type );
	}
}

/*
=================
BSP_LoadVisibility
=================
*/
static void BSP_LoadVisibility( dlump_t *l )
{
	if( !l->filelen )
	{
		MsgDev( D_WARN, "map ^2%s^7 has no visibility\n", loadmodel->name );
		cm.pvs = cm.phs = NULL;
		return;
	}

	cm.pvs = Mem_Alloc( loadmodel->mempool, l->filelen );
	Mem_Copy( cm.pvs, (void *)(mod_base + l->fileofs), l->filelen );
}

/*
=================
BSP_LoadEntityString
=================
*/
static void BSP_LoadEntityString( dlump_t *l )
{
	byte	*in;

	in = (void *)(mod_base + l->fileofs);
	cm.entityscript = Com_OpenScript( LUMP_ENTITIES, in, l->filelen );
}

/*
=================
BSP_LoadClipnodes
=================
*/
static void BSP_LoadClipnodes( dlump_t *l )
{
	dclipnode_t	*in, *out;
	int		i, count;
	hull_t		*hull;

	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadClipnodes: funny lump size\n" );
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
		out->planenum = LittleLong( in->planenum );
		out->children[0] = LittleShort( in->children[0] );
		out->children[1] = LittleShort( in->children[1] );
	}
}

/*
=================
CM_MakeHull0

Duplicate the drawing hull structure as a clipping hull
=================
*/
static void CM_MakeHull0( void )
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
CM_BrushModel
=================
*/
static void CM_BrushModel( model_t *mod, byte *buffer )
{
	dheader_t	*header;
	dmodel_t 	*bm;
	int	i, j;			
	
	header = (dheader_t *)buffer;
	i = LittleLong( header->version );

	switch( i )
	{
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

	for( i = 0; i < sizeof( dheader_t ) / 4; i++ )
		((int *)header)[i] = LittleLong((( int *)header)[i] );

	loadmodel->mempool = Mem_AllocPool( va( "sv: ^2%s^7", loadmodel->name ));

	// load into heap
	if( header->lumps[LUMP_PLANES].filelen % sizeof( dplane_t ))
	{
		// blue-shift swapped lumps
		BSP_LoadEntityString( &header->lumps[LUMP_PLANES] );
		BSP_LoadPlanes( &header->lumps[LUMP_ENTITIES] );
	}
	else
	{
		// normal half-life lumps
		BSP_LoadEntityString( &header->lumps[LUMP_ENTITIES] );
		BSP_LoadPlanes( &header->lumps[LUMP_PLANES] );
	}

	BSP_LoadVertexes( &header->lumps[LUMP_VERTEXES] );
	BSP_LoadEdges( &header->lumps[LUMP_EDGES] );
	BSP_LoadSurfEdges( &header->lumps[LUMP_SURFEDGES] );
	BSP_LoadTextures( &header->lumps[LUMP_TEXTURES] );
	BSP_LoadLighting( &header->lumps[LUMP_LIGHTING] );
	BSP_LoadVisibility( &header->lumps[LUMP_VISIBILITY] );
	BSP_LoadTexInfo( &header->lumps[LUMP_TEXINFO] );
	BSP_LoadSurfaces( &header->lumps[LUMP_FACES] );
	BSP_LoadMarkFaces( &header->lumps[LUMP_MARKSURFACES] );
	BSP_LoadLeafs( &header->lumps[LUMP_LEAFS] );
	BSP_LoadNodes( &header->lumps[LUMP_NODES] );
	BSP_LoadClipnodes( &header->lumps[LUMP_CLIPNODES] );
	BSP_LoadSubmodels( &header->lumps[LUMP_MODELS] );

	CM_MakeHull0 ();
	
	loadmodel->numframes = 2;	// regular and alternate animation
	cm.version = 0;
	
	// set up the submodels (FIXME: this is confusing)
	for( i = 0; i < mod->numsubmodels; i++ )
	{
		model_t	*starmod;

		bm = &mod->submodels[i];
		starmod = &cm_inline[i];

		*starmod = *loadmodel;

		starmod->type = (i == 0) ? mod_world : mod_brush;
		starmod->hulls[0].firstclipnode = bm->headnode[0];

		for( j = 1; j < MAX_MAP_HULLS; j++ )
		{
			starmod->hulls[j].firstclipnode = bm->headnode[j];
			starmod->hulls[j].lastclipnode = mod->numclipnodes - 1;
		}
		
		starmod->firstmodelsurface = bm->firstface;
		starmod->nummodelsurfaces = bm->numfaces;
		starmod->numleafs = bm->visleafs + 1; // include solid leaf
		
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
	loadmodel->registration_sequence = cm.registration_sequence;

	loadmodel->mempool = Mem_AllocPool( va("^2%s^7", loadmodel->name ));
	loadmodel->extradata = Mem_Alloc( loadmodel->mempool, LittleLong( phdr->length ));
	Mem_Copy( loadmodel->extradata, buffer, LittleLong( phdr->length ));

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
	loadmodel->registration_sequence = cm.registration_sequence;

	// setup bounding box
	loadmodel->mins[0] = loadmodel->mins[1] = -phdr->bounds[0] / 2;
	loadmodel->maxs[0] = loadmodel->maxs[1] = phdr->bounds[0] / 2;
	loadmodel->mins[2] = -phdr->bounds[1] / 2;
	loadmodel->maxs[2] = phdr->bounds[1] / 2;
}

static model_t *CM_ModForName( const char *name, bool world )
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
			mod->registration_sequence = cm.registration_sequence;
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
		MsgDev( D_ERROR, "CM_LoadModel: %s couldn't load\n", name );
		return NULL;
	}

	// if it's world - calc the map checksum
	if( world ) cm.checksum = LittleLong( Com_BlockChecksum( buf, size ));

	MsgDev( D_NOTE, "CM_LoadModel: %s\n", name );
	com.strncpy( mod->name, name, sizeof( mod->name ));
	mod->registration_sequence = cm.registration_sequence;	// register mod
	mod->type = mod_bad;
	loadmodel = mod;

	// call the apropriate loader
	switch( LittleLong( *(uint *)buf ))
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
		CM_FreeModel( mod );

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
		CM_FreeModel( &cm_models[0] );

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
void CM_BeginRegistration( const char *name, bool clientload, uint *checksum )
{
	// now replacement table is invalidate
	Mem_Set( sv_models, 0, sizeof( sv_models ));

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
	cm.registration_sequence++;	// all models are invalid

	// load the newmap
	worldmodel = CM_ModForName( name, true );
	if( !worldmodel ) Host_Error( "Couldn't load %s\n", name );
	worldmodel->type = mod_world;
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
		if( mod->registration_sequence != cm.registration_sequence )
			CM_FreeModel( mod );
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
Mod_Extradata
===================
*/
void *Mod_Extradata( int handle )
{
	model_t	*mod = CM_ClipHandleToModel( handle );

	if( mod && mod->type == mod_studio )
		return mod->extradata;
	return NULL;
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
		*numFrames = CM_StudioBodyVariations( handle );		
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

bool CM_RegisterModel( const char *name, int index )
{
	model_t	*mod;

	if( index < 0 || index > MAX_MODELS )
		return false;

	// this array used for acess to servermodels
	mod = CM_ModForName( name, false );
	sv_models[index] = mod;

	return (mod != NULL);
}