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
#ifndef R_MODEL_H
#define R_MODEL_H

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

#include "sprite.h"
#include "studio.h"

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/
//
// in memory representation
//
typedef enum
{
	mod_bad = -1,
	mod_brush, 
	mod_sprite, 
	mod_alias, 
	mod_studio,
	mod_world,	// added
} modtype_t;

typedef struct
{
	vec3_t		mins, maxs;
	vec3_t		origin;
	float		radius;
	int		firstnode;	// used for lighting bmodels
	int		firstface;
	int		numfaces;
	int		visleafs;
} mmodel_t;

typedef struct
{
	ref_shader_t	*shader;
	mplane_t		*visibleplane;

	int		numplanes;
	mplane_t		*planes;
} mfog_t;

typedef struct decal_s
{
	struct decal_s	*pnext;		// linked list for each surface
	struct msurface_s	*psurf;		// surface id for persistence / unlinking
	ref_shader_t	*shader;		// decal image

	vec3_t		position;		// location of the decal center in world space.
	vec3_t		worldPos;		// untransformed position, keep for serialization
	vec3_t		saxis;		// direction of the s axis in world space
	float		dx, dy;		// Offsets into surface texture
	float		scale;		// pixel scale
	short		flags;		// decal flags  FDECAL_*
	short		entityIndex;	// entity this is attached to
	mesh_t		*mesh;		// cached mesh, created on first decal rendering

	// dynamic decals stuff
	float		fadeDuration;	// Negative value means to fade in
	float		fadeStartTime;
	float		currentFrame;	// for animated decals
	rgba_t		color;
} decal_t;

// miptex features (will be convert to a shader settings)
#define MIPTEX_CONVEYOR	BIT( 0 )		// create conveyour surface
#define MIPTEX_LIQUID	BIT( 1 )		// is a liquid
#define MIPTEX_TRANSPARENT	BIT( 2 )		// transparent texture
#define MIPTEX_RENDERMODE	BIT( 3 )		// this surface requires a rendermode stuff
#define MIPTEX_NOLIGHTMAP	BIT( 4 )		// this surface if fullbright
#define MIPTEX_WARPSURFACE	BIT( 5 )		// this surface is warped

typedef struct mtexinfo_s
{
	float		vecs[2][4];
	short		texturenum;	// number in cached.textures
	short		width;
	short		height;
} mtexinfo_t;

typedef struct msurface_s
{
	int		visframe;		// should be drawn when node is crossed
	int		flags;		// see SURF_ for details
	int		contents;

	int		firstedge;	// look up in model->edges[]. negative
	int		numedges;		// numbers are backwards edges

	short		textureMins[2];
	short		extents[2];

	mtexinfo_t	*texinfo;
	ref_shader_t	*shader;
	mesh_t		*mesh;
	mfog_t		*fog;
	decal_t		*pdecals;		// linked surface decals
	mplane_t		*plane;

	union
	{
		float	origin[3];
		float	mins[3];
	};
	union
	{
		float	maxs[3];
		float	color[3];
	};

	// lighting info
	int		lmWidth;
	int		lmHeight;
	int		lmS;
	int		lmT;
	int		lmNum;		// real lightmap texnum
	byte		*samples;
	int		numstyles;
	byte		styles[LM_STYLES];
	float		cached[LM_STYLES];	// values currently used in lightmap

	int		superLightStyle;
	int		fragmentframe;	// for multi-check avoidance
} msurface_t;

#define CONTENTS_NODE	1		// fake contents to determine nodes

typedef struct mnode_s
{
	// common with leaf
	mplane_t		*plane;
	int		pvsframe;
	int		contents;		// for fast checking solid leafs

	float		mins[3];
	float		maxs[3];		// for bounding box culling

	struct mnode_s	*parent;

	// node specific
	struct mnode_s	*children[2];
	msurface_t	*firstface;	// used for grab lighting info, decals etc
	uint		numfaces;
} mnode_t;

typedef struct mleaf_s
{
	// common with node
	mplane_t		*plane;
	int		pvsframe;
	int		contents;

	float		mins[3];
	float		maxs[3];		// for bounding box culling

	struct mnode_s	*parent;

	// leaf specific
	byte		*compressed_vis;
	int		visframe;

	msurface_t	**firstMarkSurface;
	int		numMarkSurfaces;
} mleaf_t;

typedef struct
{
	byte		ambient[LM_STYLES][3];
	byte		diffuse[LM_STYLES][3];
	byte		styles[LM_STYLES];
	byte		direction[2];
} mgridlight_t;

typedef struct
{
	int		texNum;
	float		texMatrix[2][2];
} mlightmapRect_t;

typedef struct
{
	int		numsubmodels;
	mmodel_t		*submodels;

	int		nummodelsurfaces;
	msurface_t	*firstmodelsurface;

	mnode_t		*firstmodelnode;	// used for lighting bmodels

	int		numplanes;
	mplane_t		*planes;

	int		numleafs;		// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int		numnodes;
	mnode_t		*nodes;

	int		numsurfaces;
	msurface_t	*surfaces;
	msurface_t	**marksurfaces;

	int		numgridpoints;
	mgridlight_t	*lightgrid;

	int		numtexinfo;
	mtexinfo_t	*texinfo;

	byte		*visdata;		// compressed visdata
	byte		*lightdata;

	int		numfogs;
	mfog_t		*fogs;
	mfog_t		*globalfog;

	vec3_t		gridSize;
	vec3_t		gridMins;
	int		gridBounds[4];
} mbrushmodel_t;

/*
==============================================================================

STUDIO MODELS

==============================================================================
*/
typedef struct
{
	studiohdr_t	*phdr;
          studiohdr_t	*thdr;

	void		*submodels;
	int		numsubmodels;
	vec3_t		*m_pSVectors;	// UNDONE: calc SVectors on loading, simple transform on rendering
} mstudiodata_t;

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/
//
// in memory representation
//
typedef struct mspriteframe_s
{
	int		width;
	int		height;
	float		up, down, left, right;
	float		radius;
	shader_t		shader;
} mspriteframe_t;

typedef struct
{
	int		numframes;
	float		*intervals;
	mspriteframe_t	*frames[1];
} mspritegroup_t;

typedef struct
{
	frametype_t	type;
	mspriteframe_t	*frameptr;
} mspriteframedesc_t;

typedef struct
{
	int		type;
	int		rendermode;
	int		numframes;
	mspriteframedesc_t	frames[1];
} msprite_t;

//===================================================================

//
// Whole model
//
typedef struct ref_model_s
{
	char		*name;
	modtype_t		type;

	int		touchFrame;	// registration sequence

	// volume occupied by the model graphics
	vec3_t		mins, maxs;
	float		radius;
	int		flags;		// effect flags

	// memory representation pointer
	byte		*mempool;
	void		*extradata;

	// shader pointers for refresh registration_sequence
	int		numshaders;
	ref_shader_t	**shaders;
} ref_model_t;

//============================================================================

void		R_InitModels( void );
void		R_ShutdownModels( void );

void		Mod_ClearAll( void );
ref_model_t	*Mod_ForName( const char *name, bool crash );
mleaf_t		*Mod_PointInLeaf( const vec3_t p, ref_model_t *model );
byte		*Mod_LeafPVS( mleaf_t *leaf, ref_model_t *model );
uint		Mod_Handle( ref_model_t *mod );
ref_model_t	*Mod_ForHandle( unsigned int elem );
ref_model_t	*R_RegisterModel( const char *name );
void		R_BeginRegistration( const char *model );
void		R_EndRegistration( const char *skyname );
texture_t		*Mod_LoadTexture( struct mip_s *mt );

#define		Mod_Malloc( mod, size ) Mem_Alloc(( mod )->mempool, size )
#define		Mod_Realloc( mod, data, size ) Mem_Realloc(( mod )->mempool, data, size )
#define		Mod_Free( data ) Mem_Free( data )
void		Mod_Modellist_f( void );

#endif // R_MODEL_H