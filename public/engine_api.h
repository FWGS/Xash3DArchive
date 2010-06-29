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
#define MAX_GENERICS		1024	// generic files that can download from server
#define MAX_CLASSNAMES		512	// maxcount of various edicts classnames
#define MAX_SOUNDS			2048	// max unique loaded sounds (not counting sequences)
#define MAX_MODELS			2048	// total count of brush & studio various models per one map
#define MAX_PARTICLES		4096	// total particle count per one frame
#define MAX_EDICTS			32768	// absolute limit that never be reached, (do not edit!)

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