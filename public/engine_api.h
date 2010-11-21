//=======================================================================
//			Copyright XashXT Group 2008 ©
//		         engine_api.h - xash engine api
//=======================================================================
#ifndef ENGINE_API_H
#define ENGINE_API_H

#include "const.h"

//
// engine constant limits, touching networking protocol modify with precaution
//
#define MAX_MODEL_BITS		11
#define MAX_MODELS			(1<<MAX_MODEL_BITS)	// 11 bits == 2048 models

#define MAX_SOUND_BITS		11
#define MAX_SOUNDS			(1<<MAX_SOUND_BITS)	// 11 bits == 2048 sounds

#define MAX_DLIGHTS			32	// dynamic lights (rendered per one frame)
#define MAX_DECALS			512	// touching TE_DECAL messages, etc
#define MAX_LIGHTSTYLES		256	// a byte limit, don't modify
#define MAX_EDICTS			4096	// absolute limit, should be enough. (can be up to 32768)
#define MAX_RENDER_DECALS		4096	// max rendering decals per a level

// decal flags
#define FDECAL_PERMANENT		0x01	// This decal should not be removed in favor of any new decals
#define FDECAL_CUSTOM		0x02	// This is a custom clan logo and should not be saved/restored
#define FDECAL_DYNAMIC		0x04	// Indicates the decal is dynamic
#define FDECAL_DONTSAVE		0x08	// Decal was loaded from adjacent level, don't save it for this level
#define FDECAL_CLIPTEST		0x10	// Decal needs to be clip-tested
#define FDECAL_NOCLIP		0x20	// Decal is not clipped by containing polygon
#define FDECAL_USESAXIS		0x40	// Uses the s axis field to determine orientation (footprints)
#define FDECAL_ANIMATED		0x80	// this is decal has multiple frames

// world size
#define MAX_COORD_INTEGER		(16384)	// world half-size, modify with precaution
#define MIN_COORD_INTEGER		(-MAX_COORD_INTEGER)
#define MAX_COORD_FRACTION		( 1.0 - ( 1.0 / 16.0 ))
#define MIN_COORD_FRACTION		(-1.0 + ( 1.0 / 16.0 ))

// network precision
#define COORD_INTEGER_BITS		14
#define COORD_FRACTIONAL_BITS		5
#define COORD_DENOMINATOR		( 1 << ( COORD_FRACTIONAL_BITS ))
#define COORD_RESOLUTION		(1.0 / ( COORD_DENOMINATOR ))

#define NORMAL_FRACTIONAL_BITS	11
#define NORMAL_DENOMINATOR		(( 1 << ( NORMAL_FRACTIONAL_BITS )) - 1 )
#define NORMAL_RESOLUTION		( 1.0 / ( NORMAL_DENOMINATOR ))

// verify that coordsize.h and worldsize.h are consistently defined
#if( MAX_COORD_INTEGER != ( 1 << COORD_INTEGER_BITS ))
#error MAX_COORD_INTEGER does not match COORD_INTEGER_BITS
#endif

#endif//ENGINE_API_H