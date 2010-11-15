//=======================================================================
//			Copyright XashXT Group 2010 ©
//		       com_model.h - cient model structures
//=======================================================================
#ifndef COM_MODEL_H
#define COM_MODEL_H

#include "bspfile.h"	// we need some declarations from it

/*
==============================================================================

	ENGINE MODEL FORMAT
==============================================================================
*/
#define STUDIO_RENDER	1
#define STUDIO_EVENTS	2

// surface flags
#define SURF_PLANEBACK	BIT( 0 )

#define CONTENTS_NODE	1		// fake contents to determine nodes
#define ZISCALE		((float)0x8000)

// model types
typedef enum
{
	mod_bad = -1,
	mod_brush, 
	mod_sprite, 
	mod_alias, 
	mod_studio
} modtype_t;

typedef struct alight_s
{
	int		ambientlight;	// clip at 128
	int		shadelight;	// clip at 192 - ambientlight
	vec3_t		color;
	float		*plightvec;
} alight_t;

typedef struct mplane_s
{
	vec3_t		normal;
	float		dist;
	byte		type;		// for fast side tests
	byte		signbits;		// signx + (signy<<1) + (signz<<1)
	byte		pad[2];
} mplane_t;

typedef struct hull_s
{
	dclipnode_t	*clipnodes;
	mplane_t		*planes;
	int		firstclipnode;
	int		lastclipnode;
	vec3_t		clip_mins;
	vec3_t		clip_maxs;
} hull_t;

typedef struct
{
	char		name[16];
	int		contents;

	// put here info about fog color and density ?
} mtexture_t;

typedef struct
{
	float		vecs[2][4];
	mtexture_t	*texture;
	int		flags;		// texture flags
} mtexinfo_t;

typedef struct msurface_s
{
	mplane_t		*plane;		// pointer to shared plane			
	int		flags;		// see SURF_ #defines

	int		firstedge;	// look up in model->surfedges[], negative numbers
	int		numedges;		// are backwards edges

	mtexinfo_t	*texinfo;		

	short		texturemins[2];
	short		extents[2];
	
	// lighting info
	byte		*samples;		// [numstyles*surfsize]
	int		numstyles;
	byte		styles[4];	// index into d_lightstylevalue[] for animated lights 
					// no one surface can be effected by more than 4 
					// animated lights.
} msurface_t;

typedef struct mleaf_s
{
// common with node
	int		contents;
	struct mnode_s	*parent;
	mplane_t		*plane;		// always == NULL 

// leaf specific
	byte		*visdata;		// decompressed visdata after loading
	byte		*pasdata;		// decompressed pasdata after loading
	byte		ambient_sound_level[NUM_AMBIENTS];
	struct efrag_s	*efrags;

	msurface_t	**firstmarksurface;
	int		nummarksurfaces;
} mleaf_t;

typedef struct mnode_s
{
// common with leaf
	int		contents;		// CONTENTS_NODE, to differentiate from leafs
	struct mnode_s	*parent;
	mplane_t		*plane;		// always != NULL

// node specific
	struct mnode_s	*children[2];	

	msurface_t	*firstface;	// used for grab lighting info, decals etc
	unsigned int	numfaces;
} mnode_t;

typedef struct model_s
{
	char		name[64];		// model name
	int		registration_sequence;

	// shared modelinfo
	modtype_t		type;		// model type
	int		numframes;	// sprite's framecount
	byte		*mempool;		// private mempool (was synctype)
	int		flags;		// hl compatibility
	vec3_t		mins, maxs;	// bounding box at angles '0 0 0'

	// brush model
	int		firstmodelsurface;
	int		nummodelsurfaces;

	int		numsubmodels;
	dmodel_t		*submodels;	// or studio animations

	int		numplanes;
	mplane_t		*planes;

	int		numleafs;		// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int		numvertexes;
	vec3_t		*vertexes;

	int		numedges;
	dedge_t		*edges;

	int		numnodes;
	mnode_t		*nodes;

	int		numtexinfo;
	mtexinfo_t	*texinfo;

	int		numsurfaces;
	msurface_t	*surfaces;

	int		numsurfedges;
	int		*surfedges;

	int		numclipnodes;
	dclipnode_t	*clipnodes;

	int		nummarksurfaces;
	msurface_t	**marksurfaces;

	hull_t		hulls[MAX_MAP_HULLS];

	int		numtextures;
	mtexture_t	**textures;

	byte		*lightdata;	// for GetEntityIllum
	byte		*extradata;	// models extradata

	char		*entities;
} model_t;

#endif//COM_MODEL_H