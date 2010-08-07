//=======================================================================
//			Copyright XashXT Group 2008 ©
//		  vsound_api.h - xash sound engine (OpenAL based)
//=======================================================================
#ifndef VSOUND_API_H
#define VSOUND_API_H

#include "ref_params.h"
#include "trace_def.h"

typedef int	sound_t;

/*
==============================================================================

VSOUND.DLL INTERFACE
==============================================================================
*/
typedef struct vsound_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(vsound_api_t)
	size_t	com_size;		// must matched with sizeof(stdlib_api_t)

	bool (*Init)( void *hInst );	// init sound
	void (*Shutdown)( void );

	// sound manager
	void (*BeginRegistration)( void );
	sound_t (*RegisterSound)( const char *name );
	void (*EndRegistration)( void );

	void (*StartLocalSound)( const char *name ); // menus
	void (*StartSound)( const vec3_t pos, int ent, int chan, sound_t sfx, float vol, float attn, int pitch, int flags );
	void (*StaticSound)( const vec3_t pos, int ent, int chan, sound_t sfx, float vol, float attn, int pitch, int flags );
	void (*StreamRawSamples)( int samples, int rate, int width, int channels, const byte *data );
	void (*FadeClientVolume)( float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds );
	void (*StartBackgroundTrack)( const char *introTrack, const char *loopTrack );
	void (*StopBackgroundTrack)( void );

	void (*StartStreaming)( void );
	void (*StopStreaming)( void );

	void (*BeginFrame)( void );
	void (*RenderFrame)( ref_params_t *fd );
	void (*StopSound)( int entnum, int channel, const char *soundname );
	void (*StopAllSounds)( void );

	void (*Activate)( bool active, void *hInst );

} vsound_exp_t;

typedef struct vsound_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(vsound_imp_t)

	trace_t (*TraceLine)( const vec3_t start, const vec3_t end );
	void (*GetEntitySpatialization)( int entnum, vec3_t origin, vec3_t velocity );
	void (*AmbientLevels)( const vec3_t p, byte *pvolumes );
	struct cl_entity_s *(*GetClientEdict)( int index );
	float (*GetServerTime)( void );
	bool (*IsInMenu)( void );	// returns true when client is in-menu
	bool (*IsActive)( void );	// returns true when client is completely in-game
} vsound_imp_t;

#endif//VSOUND_API_H