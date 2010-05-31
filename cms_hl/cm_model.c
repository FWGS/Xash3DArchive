//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_model.c - collision model
//=======================================================================

#include "cm_local.h"
#include "byteorder.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "const.h"

clipmap_t		cm;

byte		*mod_base;
cmodel_t		*sv_models[MAX_MODELS];	// server replacement modeltable
static cmodel_t	cm_inline[MAX_MAP_MODELS];	// inline bsp models
static cmodel_t	cm_models[MAX_MODELS];
static int	cm_nummodels;

cplane_t		box_planes[6];
clipnode_t	box_clipnodes[6];
chull_t		box_hull[1];
cmodel_t		*loadmodel;
cmodel_t		*worldmodel;

/*
===============================================================================

			CM COMMON UTILS

===============================================================================
*/
/*
===================
CM_BmodelInitBoxHull

Set up the planes and nodes so that the six floats of a bounding box
can just be stored out and get a proper clipping hull structure.
===================
*/
void CM_BmodelInitBoxHull( void )
{
	cplane_t		*p;
	clipnode_t	*c;
	int		i, side;

	box_hull->clipnodes = box_clipnodes;
	box_hull->planes = box_planes;
	box_hull->firstclipnode = 0;
	box_hull->lastclipnode = 5;

	for( i = 0; i < 6; i++ )
	{
		side = i & 1;

		// setup clipnodes
		c = &box_clipnodes[i];
		c->planenum = i;
		
		c->children[side] = CONTENTS_EMPTY;
		if( i != 5 ) c->children[side^1] = i + 1;
		else c->children[side^1] = CONTENTS_SOLID;

		// setup planes
		p = &box_planes[i];
		VectorClear( p->normal );

		p->type = i>>1;
		p->normal[i>>1] = 1.0f;
		p->signbits = 0;
	}
}

/*
===================
CM_HullForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
chull_t *CM_HullForBox( const vec3_t mins, const vec3_t maxs )
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = mins[2];

	return box_hull;
}

/*
================
CM_FreeModel
================
*/
void CM_FreeModel( cmodel_t *mod )
{
	if( !mod || !mod->mempool )
		return;

	if( mod->entityscript )
	{
		Com_CloseScript( mod->entityscript );
		mod->entityscript = NULL;
	}

	Mem_FreePool( &mod->mempool );
	Mem_Set( mod, 0, sizeof( *mod ));
}

int CM_NumInlineModels( void )
{
	if( worldmodel )
		return worldmodel->numsubmodels;
	return 0;
}

script_t *CM_EntityScript( void )
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
	return worldmodel->entityscript;
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
	ctexture_t	*out;
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
	loadmodel->textures = (ctexture_t **)Mem_Alloc( loadmodel->mempool, loadmodel->numtextures * sizeof( ctexture_t* ));

	for( i = 0; i < loadmodel->numtextures; i++ )
	{
		in->dataofs[i] = LittleLong( in->dataofs[i] );
		if( in->dataofs[i] == -1 ) continue; // bad offset ?

		mt = (mip_t *)((byte *)in + in->dataofs[i] );
		out = Mem_Alloc( loadmodel->mempool, sizeof( *out ));
		loadmodel->textures[i] = out;

		Mem_Copy( out->name, mt->name, sizeof( out->name ));
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
	ctexinfo_t	*out;
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
BSP_LoadSurfaces
=================
*/
static void BSP_LoadSurfaces( const dlump_t *l )
{
	dface_t		*in;
	csurface_t	*out;
	int		i, count;

	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( dface_t ))
		Host_Error( "BSP_LoadFaces: funny lump size in '%s'\n", loadmodel->name );
	count = l->filelen / sizeof( dface_t );

	loadmodel->numsurfaces = count;
	loadmodel->surfaces = Mem_Alloc( loadmodel->mempool, count * sizeof( csurface_t ));
	out = loadmodel->surfaces;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->firstedge = LittleLong( in->firstedge );
		out->numedges = LittleLong( in->numedges );

		if( LittleShort( in->side )) out->flags |= SURF_PLANEBACK;
		out->plane = loadmodel->planes + LittleLong( in->planenum );
		out->texinfo = loadmodel->texinfo + LittleLong( in->texinfo );
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
	loadmodel->marksurfaces = Mem_Alloc( loadmodel->mempool, count * sizeof( csurface_t* ));

	for( i = 0; i < count; i++ )
	{
		j = LittleLong( in[i] );
		if( j < 0 ||  j >= count )
			Host_Error( "BSP_LoadMarkFaces: bad surface number in '%s'\n", loadmodel->name );
		loadmodel->marksurfaces[i] = loadmodel->surfaces + j;
	}
}

/*
=================
CM_SetParent
=================
*/
static void CM_SetParent( cnode_t *node, cnode_t *parent )
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
	cnode_t	*out;
	int	i, j, p;
	
	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadNodes: funny lump size\n" );
	loadmodel->numnodes = l->filelen / sizeof( *in );

	if( loadmodel->numnodes < 1 ) Host_Error( "Map %s has no nodes\n", loadmodel->name );
	out = loadmodel->nodes = (cnode_t *)Mem_Alloc( loadmodel->mempool, loadmodel->numnodes * sizeof( *out ));

	for( i = 0; i < loadmodel->numnodes; i++, out++, in++ )
	{
		p = LittleLong( in->planenum );
		out->plane = loadmodel->planes + p;
		out->contents = CONTENTS_NODE;

		for( j = 0; j < 2; j++ )
		{
			p = LittleShort( in->children[j] );
			if( p >= 0 ) out->children[j] = loadmodel->nodes + p;
			else out->children[j] = (cnode_t *)(loadmodel->leafs + ( -1 - p ));
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
	cleaf_t	*out;
	int	i, j, p, count;
		
	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadLeafs: funny lump size\n" );

	count = l->filelen / sizeof( *in );
	if( count < 1 ) Host_Error( "Map %s with no leafs\n", loadmodel->name );
	out = (cleaf_t *)Mem_Alloc( loadmodel->mempool, count * sizeof( *out ));

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

		out->firstMarkSurface = loadmodel->marksurfaces + LittleShort( in->firstmarksurface );
		out->numMarkSurfaces = LittleShort( in->nummarksurfaces );
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
	cplane_t	*out;
	int	i, j, count;
	
	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadPlanes: funny lump size\n" );
	count = l->filelen / sizeof( *in );

	if( count < 1 ) Host_Error( "Map %s with no planes\n", loadmodel->name );
	out = (cplane_t *)Mem_Alloc( loadmodel->mempool, count * sizeof( *out ));

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
void BSP_LoadVisibility( dlump_t *l )
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
void BSP_LoadEntityString( dlump_t *l )
{
	byte	*in;

	in = (void *)(mod_base + l->fileofs);
	loadmodel->entityscript = Com_OpenScript( LUMP_ENTITIES, in, l->filelen );
}

/*
=================
BSP_LoadClipnodes
=================
*/
static void BSP_LoadClipnodes( dlump_t *l )
{
	dclipnode_t	*in;
	clipnode_t	*out;
	int		i, count;
	chull_t		*hull;

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

	hull = &loadmodel->hulls[2];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	VectorCopy( GI->client_mins[2], hull->clip_mins ); // copy large hull
	VectorCopy( GI->client_maxs[2], hull->clip_maxs );

	hull = &loadmodel->hulls[3];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	VectorCopy( GI->client_mins[3], hull->clip_mins ); // copy head hull
	VectorCopy( GI->client_maxs[3], hull->clip_maxs );

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
void CM_MakeHull0( void )
{
	cnode_t		*in, *child;
	clipnode_t	*out;
	chull_t		*hull;
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

			if( child->contents < 0 ) out->children[j] = child->contents;
			else out->children[j] = child - loadmodel->nodes;
		}
	}
}


/*
=================
CM_BrushModel
=================
*/
static void CM_BrushModel( cmodel_t *mod, byte *buffer )
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
	BSP_LoadTextures( &header->lumps[LUMP_TEXTURES] );
	BSP_LoadTexInfo( &header->lumps[LUMP_TEXINFO] );
	BSP_LoadEdges( &header->lumps[LUMP_EDGES] );
	BSP_LoadSurfEdges( &header->lumps[LUMP_SURFEDGES] );
	BSP_LoadSurfaces( &header->lumps[LUMP_FACES] );
	BSP_LoadVisibility( &header->lumps[LUMP_VISIBILITY] );
	BSP_LoadMarkFaces( &header->lumps[LUMP_MARKSURFACES] );
	BSP_LoadLeafs( &header->lumps[LUMP_LEAFS] );
	BSP_LoadNodes( &header->lumps[LUMP_NODES] );
	BSP_LoadClipnodes( &header->lumps[LUMP_CLIPNODES] );
	BSP_LoadSubmodels( &header->lumps[LUMP_MODELS] );

	CM_MakeHull0 ();
	
	loadmodel->numframes = 2;	// regular and alternate animation
	
	// set up the submodels (FIXME: this is confusing)
	for( i = 0; i < mod->numsubmodels; i++ )
	{
		cmodel_t	*starmod;

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

void CM_FreeWorld( void )
{
	if( worldmodel )
		CM_FreeModel( &cm_models[0] );
	worldmodel = NULL;
}

void CM_FreeModels( void )
{
	int	i;

	for( i = 0; i < cm_nummodels; i++ )
		CM_FreeModel( &cm_models[i] );
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
		CM_FreeModel( &cm_models[0] );
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
			// reset entity script
			Com_ResetScript( worldmodel->entityscript );
		}
		sv_models[1] = cm_models; // make link to world

		// still have the right version
		return;
	}

	CM_FreeModel( &cm_models[0] );// release old map
	cm.registration_sequence++;	// all models are invalid

	// load the newmap
	worldmodel = CM_ModForName( name, true );
	if( !worldmodel ) Host_Error( "Couldn't load %s\n", name );
	worldmodel->type = mod_world;
	sv_models[1] = cm_models; // make link to world
		
	if( checksum ) *checksum = cm.checksum;

	CM_BmodelInitBoxHull ();
	CM_StudioInitBoxHull (); // hitbox tracing

	CM_CalcPHS ();
}

void CM_EndRegistration( void )
{
	cmodel_t	*mod;
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
cmodel_t *CM_ClipHandleToModel( model_t handle )
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
CM_Extradata
===================
*/
void *CM_Extradata( model_t handle )
{
	cmodel_t	*mod = CM_ClipHandleToModel( handle );

	if( mod && mod->type == mod_studio )
		return mod->extradata;
	return NULL;
}

/*
===================
CM_ModelFrames
===================
*/
void CM_ModelFrames( model_t handle, int *numFrames )
{
	cmodel_t	*mod = CM_ClipHandleToModel( handle );

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
CM_ModelType
===================
*/
modtype_t CM_ModelType( model_t handle )
{
	cmodel_t	*mod = CM_ClipHandleToModel( handle );

	if( !mod ) return mod_bad;
	return mod->type;
}

/*
===================
CM_ModelBounds
===================
*/
void CM_ModelBounds( model_t handle, vec3_t mins, vec3_t maxs )
{
	cmodel_t	*cmod = CM_ClipHandleToModel( handle );

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

cmodel_t *CM_ModForName( const char *name, bool world )
{
	byte	*buf;
	cmodel_t	*mod;
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
		// check for loading problems
		if( world ) Host_Error( "CMod_ForName: %s unknown format\n", name );
		else MsgDev( D_ERROR, "CMod_ForName: %s unknown format\n", name );
		CM_FreeModel( mod );
		return NULL;
	}

	return mod;
}

bool CM_RegisterModel( const char *name, int index )
{
	cmodel_t	*mod;

	if( index < 0 || index > MAX_MODELS )
		return false;

	// this array used for acess to servermodels
	mod = CM_ModForName( name, false );
	sv_models[index] = mod;

	return (mod != NULL);
}