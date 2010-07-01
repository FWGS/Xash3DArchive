//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       physic_api.h - xash physic library api
//=======================================================================
#ifndef PHYSIC_API_H
#define PHYSIC_API_H

#include "trace_def.h"

#define FMOVE_IGNORE_GLASS	0x100
#define FMOVE_SIMPLEBOX	0x200

/*
==============================================================================

PHYSIC.DLL INTERFACE
==============================================================================
*/
typedef struct physic_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(physic_exp_t)
	size_t	com_size;		// must matched with sizeof(stdlib_api_t)

	// initialize
	bool (*Init)( void );				// init all physic systems
	void (*Shutdown)( void );				// shutdown all render systems

	void (*DrawCollision)( cmdraw_t callback );		// debug draw world
	void (*Frame)( float time );				// physics frame

	// models loading
	void (*BeginRegistration)( const char *name, bool clientload, uint *checksum );
	bool (*RegisterModel)( const char *name, int sv_index ); // also build replacement index table
	void (*EndRegistration)( void );

	// testing in leaf
	int (*BoxLeafnums)( vec3_t mins, vec3_t maxs, short *list, int listsize, int *topNode );
	bool (*BoxVisible)( const vec3_t mins, const vec3_t maxs, byte *visbits );
	bool (*HeadnodeVisible)( int nodenum, byte *visbits );
	void (*AmbientLevels)( const vec3_t p, byte *pvolumes );
	int (*PointLeafnum)( const vec3_t p );
	byte *(*LeafPVS)( int leafnum );
	byte *(*LeafPHS)( int leafnum );
	byte *(*FatPVS)( const vec3_t org, bool portal );
	byte *(*FatPHS)( const vec3_t org, bool portal );

	// map info
	int (*NumBmodels)( void );
	script_t *(*GetEntityScript)( void );

	// models info
	modtype_t (*Mod_GetType)( model_t handle );
	void *(*Mod_Extradata)( model_t handle );
	void (*Mod_GetFrames)( model_t handle, int *numFrames );
	void (*Mod_GetBounds)( model_t handle, vec3_t mins, vec3_t maxs );
	void (*Mod_GetAttachment)( edict_t *ent, int iAttachment, float *rgflOrigin, float *rgflAngles );
	void (*Mod_GetBonePos)( edict_t *ent, int iBone, float *rgflOrigin, float *rgflAngles );

	// lighting info
	void (*AddLightstyle)( int style, const char* val );
	int (*LightPoint)( edict_t *pEdict );	// for GETENTITYILLUM

	// tracing
	int (*PointContents)( const vec3_t p );
	int (*HullPointContents)( chull_t *hull, int num, const vec3_t p );
	trace_t (*Trace)( edict_t *ent, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int flags );
	const char *(*TraceTexture)( const vec3_t start, trace_t trace );
	chull_t *(*HullForBsp)( edict_t *ent, const vec3_t mins, const vec3_t maxs, float *offset );
} physic_exp_t;

typedef struct physic_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(physic_imp_t)

} physic_imp_t;

#endif//PHYSIC_API_H