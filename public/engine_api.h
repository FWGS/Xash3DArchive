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
#define MAX_CLIENTS			32	// max allowed clients (modify with precaution)
#define MAX_DLIGHTS			32	// dynamic lights (rendered per one frame)
#define MAX_LIGHTSTYLES		256	// can't be blindly increased
#define MAX_DECALS			4096	// max rendering decals per a level
#define MAX_DECALNAMES		1024	// server decal indexes (different decalnames, not a render limit)
#define MAX_USER_MESSAGES		200	// another 56 messages reserved for engine routines
#define MAX_EVENTS			1024	// playback events that can be queued (a byte range, don't touch)
#define MAX_MSGLEN			16384	// max length of network message
#define MAX_GENERICS		1024	// generic files that can download from server
#define MAX_CLASSNAMES		512	// maxcount of various edicts classnames
#define MAX_SOUNDS			2048	// max unique loaded sounds (not counting sequences)
#define MAX_MODELS			2048	// total count of brush & studio various models per one map
#define MAX_EDICTS			32768	// absolute limit that never be reached, (do not edit!)

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

/*
==============================================================================

 Generic LAUNCH.DLL INTERFACE
==============================================================================
*/
typedef struct launch_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(launch_api_t)
	size_t	com_size;		// must matched with sizeof(stdlib_api_t)

	void (*Init)( const int argc, const char **argv );	// init host
	void (*Main)( void );				// host frame
	void (*Free)( void );				// close host
	void (*CPrint)( const char *msg );			// host print
	void (*CmdForward)( void );				// cmd forward to server
	void (*CmdComplete)( char *complete_string );		// cmd autocomplete for system console
} launch_exp_t;

#endif//ENGINE_API_H