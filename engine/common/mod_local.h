/*
mod_local.h - model loader
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

#ifndef MOD_LOCAL_H
#define MOD_LOCAL_H

#include "common.h"
#include "bspfile.h"
#include "edict.h"
#include "eiface.h"
#include "com_model.h"

// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON	(1.0f / 32.0f)
#define FRAC_EPSILON	(1.0f / 1024.0f)
#define BACKFACE_EPSILON	0.01f
#define MAX_BOX_LEAFS	256
#define DVIS_PVS		0
#define DVIS_PHS		1
#define ANIM_CYCLE		2

// remapping info
#define SUIT_HUE_START	192
#define SUIT_HUE_END	223
#define PLATE_HUE_START	160
#define PLATE_HUE_END	191

#define SURF_INFO( surf, mod )	((mextrasurf_t *)mod->cache.data + (surf - mod->surfaces)) 
#define INFO_SURF( surf, mod )	(mod->surfaces + (surf - (mextrasurf_t *)mod->cache.data)) 

// model flags (stored in model_t->flags)
#define MODEL_CONVEYOR	BIT( 0 )
#define MODEL_HAS_ORIGIN	BIT( 1 )
#define MODEL_LIQUID	BIT( 2 )	// model has only point hull

typedef struct leaflist_s
{
	int		count;
	int		maxcount;
	qboolean		overflowed;
	short		*list;
	vec3_t		mins, maxs;
	int		topnode;		// for overflows where each leaf can't be stored individually
} leaflist_t;

typedef struct
{
	int		version;		// map version
	uint		checksum;		// current map checksum
	int		load_sequence;	// increace each map change
	vec3_t		hull_sizes[4];	// actual hull sizes
	msurface_t	**draw_surfaces;	// used for sorting translucent surfaces
	int		max_surfaces;	// max surfaces per submodel (for all models)
	size_t		visdatasize;	// actual size of the visdata
	qboolean		loading;		// true if worldmodel is loading
	qboolean		sky_sphere;	// true when quake sky-sphere is used
	qboolean		has_mirrors;	// one or more brush models contain reflective textures

	vec3_t		mins;		// real accuracy world bounds
	vec3_t		maxs;
	vec3_t		size;
} world_static_t;

extern world_static_t	world;
extern byte		*com_studiocache;
extern model_t		*loadmodel;
extern convar_t		*mod_studiocache;

//
// model.c
//
void Mod_Init( void );
void Mod_ClearAll( void );
void Mod_Shutdown( void );
void Mod_SetupHulls( float mins[4][3], float maxs[4][3] );
void Mod_GetBounds( int handle, vec3_t mins, vec3_t maxs );
void Mod_GetFrames( int handle, int *numFrames );
void Mod_LoadWorld( const char *name, uint *checksum, qboolean force );
void Mod_FreeUnused( void );
void *Mod_Calloc( int number, size_t size );
void *Mod_CacheCheck( struct cache_user_s *c );
void Mod_LoadCacheFile( const char *path, struct cache_user_s *cu );
void *Mod_Extradata( model_t *mod );
model_t *Mod_FindName( const char *name, qboolean create );
model_t *Mod_LoadModel( model_t *mod, qboolean world );
model_t *Mod_ForName( const char *name, qboolean world );
qboolean Mod_RegisterModel( const char *name, int index );
int Mod_PointLeafnum( const vec3_t p );
byte *Mod_LeafPVS( mleaf_t *leaf, model_t *model );
byte *Mod_LeafPHS( mleaf_t *leaf, model_t *model );
mleaf_t *Mod_PointInLeaf( const vec3_t p, mnode_t *node );
int Mod_BoxLeafnums( const vec3_t mins, const vec3_t maxs, short *list, int listsize, int *lastleaf );
qboolean Mod_BoxVisible( const vec3_t mins, const vec3_t maxs, const byte *visbits );
void Mod_AmbientLevels( const vec3_t p, byte *pvolumes );
byte *Mod_CompressVis( const byte *in, size_t *size );
byte *Mod_DecompressVis( const byte *in );
modtype_t Mod_GetType( int handle );
model_t *Mod_Handle( int handle );

//
// mod_studio.c
//
void Mod_InitStudioAPI( void );
void Mod_InitStudioHull( void );
qboolean Mod_GetStudioBounds( const char *name, vec3_t mins, vec3_t maxs );
void Mod_StudioGetAttachment( const edict_t *e, int iAttachment, float *org, float *ang );
void Mod_GetBonePosition( const edict_t *e, int iBone, float *org, float *ang );
hull_t *SV_HullForStudioModel( edict_t *ent, int hullNumber, int flags, vec3_t mins, vec3_t maxs, vec3_t offset, int *numhitboxes );
hull_t *Mod_HullForStudio( model_t *m, float frame, int seq, vec3_t ang, vec3_t org, vec3_t size, byte *pcnt, byte *pbl, int *hitboxes, edict_t *ed );
int Mod_HitgroupForStudioHull( int index );

#endif//MOD_LOCAL_H