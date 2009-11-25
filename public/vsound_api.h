//=======================================================================
//			Copyright XashXT Group 2008 �
//		  vsound_api.h - xash sound engine (OpenAL based)
//=======================================================================
#ifndef VSOUND_API_H
#define VSOUND_API_H

#include "ref_params.h"

typedef int		sound_t;

// snd internal flags (lower bits are used for snd channels)
#define CHAN_NO_PHS_ADD	(1<<3)	// send to all clients, not just ones in PHS (ATTN 0 will also do this)
#define CHAN_RELIABLE	(1<<4)	// send by reliable message, not datagram

/*
==============================================================================

VSOUND.DLL INTERFACE
==============================================================================
*/
typedef struct vsound_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(vprogs_api_t)
	size_t	com_size;		// must matched with sizeof(stdlib_api_t)

	bool (*Init)( void *hInst );	// init sound
	void (*Shutdown)( void );

	// sound manager
	void (*BeginRegistration)( void );
	sound_t (*RegisterSound)( const char *name );
	void (*EndRegistration)( void );

	void (*StartSound)( const vec3_t pos, int ent, int chan, sound_t sfx, float vol, float attn, float pitch, int flags );
	void (*StreamRawSamples)( int samples, int rate, int width, int channels, const byte *data );
	bool (*AddLoopingSound)( int entnum, sound_t handle, float volume, float attn );
	bool (*StartLocalSound)( const char *name, float volume, float pitch, const float *origin );
	void (*FadeClientVolume)( float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds );
	void (*StartBackgroundTrack)( const char *introTrack, const char *loopTrack );
	void (*StopBackgroundTrack)( void );

	void (*StartStreaming)( void );
	void (*StopStreaming)( void );

	void (*RenderFrame)( ref_params_t *fd );
	void (*StopAllSounds)( void );
	void (*FreeSounds)( void );

	void (*Activate)( bool active );

} vsound_exp_t;

typedef struct vsound_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(vsound_imp_t)

	void (*GetSoundSpatialization)( int entnum, vec3_t origin, vec3_t velocity );
	int  (*PointContents)( const vec3_t point );
	edict_t *(*GetClientEdict)( int index );
	void (*AddLoopingSounds)( void );
	int  (*GetServerTime)( void );
} vsound_imp_t;

#endif//VSOUND_API_H