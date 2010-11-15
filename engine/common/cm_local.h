//=======================================================================
//			Copyright XashXT Group 2007 ©
//			    cm_local.h - main struct
//=======================================================================
#ifndef CM_LOCAL_H
#define CM_LOCAL_H

#include "common.h"
#include "bspfile.h"
#include "edict.h"
#include "eiface.h"
#include "com_model.h"

// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON	(1.0f / 32.0f)
#define FRAC_EPSILON	(1.0f / 1024.0f)
#define MAX_BOX_LEAFS	256
#define DVIS_PVS		0
#define DVIS_PHS		1

extern convar_t		*cm_novis;

typedef struct leaflist_s
{
	int		count;
	int		maxcount;
	qboolean		overflowed;
	short		*list;
	vec3_t		mins, maxs;
	int		topnode;		// for overflows where each leaf can't be stored individually
} leaflist_t;

typedef struct clipmap_s
{
	uint		checksum;		// map checksum
	int		registration_sequence;
	int		checkcount;
	int		version;		// current map version

	byte		*pvs;		// fully uncompressed visdata alloced in cm.mempool;
	byte		*phs;
	byte		nullrow[MAX_MAP_LEAFS/8];

	vec3_t		hull_sizes[4];	// hull sizes

	byte		*studiopool;	// cache for submodels

	script_t		*entityscript;	// only actual for world
} clipmap_t;

extern clipmap_t		cm;
extern model_t		*loadmodel;
extern model_t		*worldmodel;

//
// cm_test.c
//
byte *CM_LeafPVS( int leafnum );
byte *CM_LeafPHS( int leafnum );
int CM_PointLeafnum( const vec3_t p );
mleaf_t *CM_PointInLeaf( const vec3_t p, mnode_t *node );
int CM_BoxLeafnums( const vec3_t mins, const vec3_t maxs, short *list, int listsize, int *lastleaf );
qboolean CM_BoxVisible( const vec3_t mins, const vec3_t maxs, const byte *visbits );
int CM_HullPointContents( hull_t *hull, int num, const vec3_t p );
int CM_PointContents( const vec3_t p );
void CM_AmbientLevels( const vec3_t p, byte *pvolumes );
	
//
// cm_portals.c
//
void CM_CalcPHS( void );
byte *CM_FatPVS( const vec3_t org, qboolean portal );
byte *CM_FatPHS( const vec3_t org, qboolean portal );

//
// cm_model.c
//
qboolean CM_InitPhysics( void );
void CM_FreePhysics( void );
script_t *CM_GetEntityScript( void );
void CM_SetupHulls( float mins[4][3], float maxs[4][3] );
void Mod_GetBounds( int handle, vec3_t mins, vec3_t maxs );
void Mod_GetFrames( int handle, int *numFrames );
modtype_t CM_GetModelType( int handle );
model_t *CM_ClipHandleToModel( int handle );
int CM_ClipModelToHandle( const model_t *pmodel );
void CM_BeginRegistration ( const char *name, qboolean clientload, uint *checksum );
model_t *CM_ModForName( const char *name, qboolean world );
qboolean CM_RegisterModel( const char *name, int index );
void CM_EndRegistration( void );
void *Mod_Calloc( int number, size_t size );
void *Mod_CacheCheck( struct cache_user_s *c );
void Mod_LoadCacheFile( const char *path, struct cache_user_s *cu );
void *Mod_Extradata( model_t *mod );

#endif//CM_LOCAL_H