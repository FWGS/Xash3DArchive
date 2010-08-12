//=======================================================================
//			Copyright XashXT Group 2007 ©
//			    cm_local.h - main struct
//=======================================================================
#ifndef CM_LOCAL_H
#define CM_LOCAL_H

#include "common.h"
#include "bspfile.h"
#include "trace_def.h"
#include "com_model.h"

#define FMOVE_IGNORE_GLASS	0x100
#define FMOVE_SIMPLEBOX	0x200

// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON	(0.03125)
#define MAX_BOX_LEAFS	256
#define DVIS_PVS		0
#define DVIS_PHS		1

extern cvar_t		*cm_novis;

typedef struct
{
	int		length;
	float		map[MAX_STRING];
	float		value;		// current lightvalue
} clightstyle_t;

typedef struct leaflist_s
{
	int		count;
	int		maxcount;
	bool		overflowed;
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

	script_t		*entityscript;	// only actual for world

	// run local lightstyles to get SV_LightPoint grab the actual information
	clightstyle_t	lightstyle[MAX_LIGHTSTYLES];
	int		lastofs;
} clipmap_t;

extern clipmap_t		cm;
extern model_t		*sv_models[MAX_MODELS];	// replacement client-server table
extern model_t		*loadmodel;
extern model_t		*worldmodel;

//
// cm_debug.c
//
void CM_DrawCollision( cmdraw_t callback );

//
// cm_light.c
//
void CM_RunLightStyles( float time );
void CM_SetLightStyle( int style, const char* val );
int CM_LightEntity( edict_t *pEdict );
void CM_ClearLightStyles( void );

//
// cm_main.c
//
bool CM_InitPhysics( void );
void CM_Frame( float time );
void CM_FreePhysics( void );

//
// cm_test.c
//
byte *CM_LeafPVS( int leafnum );
byte *CM_LeafPHS( int leafnum );
int CM_PointLeafnum( const vec3_t p );
mleaf_t *CM_PointInLeaf( const vec3_t p, mnode_t *node );
bool CM_HeadnodeVisible( int nodenum, byte *visbits );
int CM_BoxLeafnums( const vec3_t mins, const vec3_t maxs, short *list, int listsize, int *lastleaf );
int CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, bool capsule );
bool CM_BoxVisible( const vec3_t mins, const vec3_t maxs, byte *visbits );
int CM_HullPointContents( hull_t *hull, int num, const vec3_t p );
int CM_PointContents( const vec3_t p );
void CM_AmbientLevels( const vec3_t p, byte *pvolumes );
	
//
// cm_portals.c
//
void CM_CalcPHS( void );
byte *CM_FatPVS( const vec3_t org, bool portal );
byte *CM_FatPHS( const vec3_t org, bool portal );

//
// cm_model.c
//
void CM_FreeModels( void );
int CM_NumBmodels( void );
script_t *CM_GetEntityScript( void );
void Mod_GetBounds( int handle, vec3_t mins, vec3_t maxs );
void Mod_GetFrames( int handle, int *numFrames );
modtype_t CM_GetModelType( int handle );
model_t *CM_ClipHandleToModel( int handle );
hull_t *CM_HullForBox( const vec3_t mins, const vec3_t maxs );
int CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, bool capsule );
void CM_BeginRegistration ( const char *name, bool clientload, uint *checksum );
bool CM_RegisterModel( const char *name, int sv_index );
void *Mod_Extradata( int handle );
model_t *CM_ModForName( const char *name, bool world );
void CM_EndRegistration( void );

//
// cm_studio.c
//
void CM_SpriteModel( model_t *mod, byte *buffer );
void CM_StudioModel( model_t *mod, byte *buffer );
void CM_StudioInitBoxHull( void );
int CM_StudioBodyVariations( int handle );
void CM_StudioGetAttachment( edict_t *e, int iAttachment, float *org, float *ang );
bool CM_StudioTrace( edict_t *e, const vec3_t start, const vec3_t end, trace_t *tr );
void CM_GetBonePosition( edict_t* e, int iBone, float *rgflOrigin, float *rgflAngles );
	
//
// cm_trace.c
//
trace_t CM_ClipMove( edict_t *ent, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int flags );
const char *CM_TraceTexture( edict_t *pTextureEntity, const vec3_t v1, const vec3_t v2 );
hull_t *CM_HullForBsp( edict_t *ent, const vec3_t mins, const vec3_t maxs, float *offset );

#endif//CM_LOCAL_H