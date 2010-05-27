//=======================================================================
//			Copyright XashXT Group 2007 ©
//			    cm_local.h - main struct
//=======================================================================
#ifndef CM_LOCAL_H
#define CM_LOCAL_H

#include <stdio.h>
#include <windows.h>
#include "launch_api.h"
#include "engine_api.h"
#include "entity_def.h"
#include "physic_api.h"
#include "qfiles_ref.h"
#include "bmodel_ref.h"
#include "trace_def.h"

extern physic_imp_t		pi;
extern stdlib_api_t		com;

#define Host_Error		com.error

// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON	(0.03125)
#define MAX_BOX_LEAFS	256
#define DVIS_PVS		0
#define DVIS_PHS		1

typedef struct
{
	char		name[64];
	int		contents;
} ctexture_t;

typedef struct
{
	float		vecs[2][4];
	ctexture_t	*texture;
	int		flags;
} ctexinfo_t;

// surface flags
#define SURF_PLANEBACK	BIT( 0 )

typedef struct csurface_s
{
	cplane_t		*plane;		// pointer to shared plane			
	int		flags;		// see SURF_ #defines

	int		firstedge;	// look up in model->surfedges[], negative numbers
	int		numedges;		// are backwards edges

	ctexinfo_t	*texinfo;		
	
	// lighting info
	byte		styles[LM_STYLES];	// index into d_lightstylevalue[] for animated lights 
					// no one surface can be effected by more than 4 
					// animated lights.
	byte		*samples;		// [numstyles*surfsize]
} csurface_t;

typedef struct cleaf_s
{
// common with node
	int		contents;
	struct cnode_s	*parent;
	cplane_t		*plane;		// always == NULL 

// leaf specific
	byte		*visdata;		// decompressed visdata after loading
	byte		*pasdata;		// decompressed pasdata after loading
	byte		ambient_sound_level[NUM_AMBIENTS];

	csurface_t	**firstMarkSurface;
	int		numMarkSurfaces;
} cleaf_t;

typedef struct cnode_s
{
// common with leaf
	int		contents;		// 0, to differentiate from leafs
	struct cnode_s	*parent;
	cplane_t		*plane;		// always != NULL

// node specific
	struct cnode_s	*children[2];	
} cnode_t;

typedef struct
{
	string		name;		// model name
	byte		*mempool;		// private mempool
	int		registration_sequence;

	// shared modelinfo
	modtype_t		type;		// model type
	vec3_t		mins, maxs;	// bounding box at angles '0 0 0'

	// brush model
	int		firstmodelsurface;
	int		nummodelsurfaces;

	int		numsubmodels;
	dmodel_t		*submodels;	// and studio animations too

	int		numplanes;
	cplane_t		*planes;

	int		numleafs;		// number of visible leafs, not counting 0
	cleaf_t		*leafs;

	int		numvertexes;
	vec3_t		*vertexes;

	int		numedges;
	dedge_t		*edges;

	int		numnodes;
	cnode_t		*nodes;

	int		numtexinfo;
	ctexinfo_t	*texinfo;

	int		numsurfaces;
	csurface_t	*surfaces;

	int		numsurfedges;
	int		*surfedges;

	int		numclipnodes;
	clipnode_t	*clipnodes;

	int		nummarksurfaces;
	csurface_t	**marksurfaces;

	chull_t		hulls[MAX_MAP_HULLS];

	int		numtextures;
	ctexture_t	**textures;

	script_t		*entityscript;	// only actual for world
	byte		*lightdata;	// for GetEntityIllum
	byte		*extradata;	// models extradata

	int		numframes;	// sprite framecount
} cmodel_t;

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

	byte		*pvs;		// fully uncompressed visdata alloced in cm.mempool;
	byte		*phs;
	byte		nullrow[MAX_MAP_LEAFS/8];
} clipmap_t;

extern clipmap_t		cm;
extern cmodel_t		*sv_models[MAX_MODELS];	// replacement client-server table
extern cmodel_t		*loadmodel;
extern cmodel_t		*worldmodel;

//
// cm_debug.c
//
void CM_DrawCollision( cmdraw_t callback );

//
// cm_test.c
//
byte *CM_LeafPVS( int leafnum );
byte *CM_LeafPHS( int leafnum );
int CM_PointLeafnum( const vec3_t p );
bool CM_HeadnodeVisible( int nodenum, byte *visbits );
int CM_BoxLeafnums( const vec3_t mins, const vec3_t maxs, short *list, int listsize, int *lastleaf );
model_t CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, bool capsule );
bool CM_BoxVisible( const vec3_t mins, const vec3_t maxs, byte *visbits );
int CM_HullPointContents( chull_t *hull, int num, const vec3_t p );
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
int CM_NumInlineModels( void );
script_t *CM_EntityScript( void );
void CM_ModelBounds( model_t handle, vec3_t mins, vec3_t maxs );
void CM_ModelFrames( model_t handle, int *numFrames );
modtype_t CM_ModelType( model_t handle );
cmodel_t *CM_ClipHandleToModel( model_t handle );
chull_t *CM_HullForBox( const vec3_t mins, const vec3_t maxs );
model_t CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, bool capsule );
void CM_BeginRegistration ( const char *name, bool clientload, uint *checksum );
bool CM_RegisterModel( const char *name, int sv_index );
void *CM_Extradata( model_t handle );
cmodel_t *CM_ModForName( const char *name, bool world );
void CM_EndRegistration( void );

//
// cm_studio.c
//
void CM_SpriteModel( cmodel_t *mod, byte *buffer );
void CM_StudioModel( cmodel_t *mod, byte *buffer );
void CM_StudioInitBoxHull( void );
int CM_StudioBodyVariations( model_t handle );
void CM_StudioGetAttachment( edict_t *e, int iAttachment, float *org, float *ang );
bool CM_StudioTrace( edict_t *e, const vec3_t start, const vec3_t end, trace_t *tr );
void CM_GetBonePosition( edict_t* e, int iBone, float *rgflOrigin, float *rgflAngles );
	
//
// cm_trace.c
//
trace_t CM_ClipMoveToEntity( edict_t *ent, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int flags );
const char *CM_TraceTexture( const vec3_t start, trace_t trace );
chull_t *CM_HullForBsp( edict_t *ent, float *offset );

#endif//CM_LOCAL_H