//=======================================================================
//			Copyright XashXT Group 2008 ©
//		  vsound_api.h - xash sound engine (OpenAL based)
//=======================================================================
#ifndef VSOUND_API_H
#define VSOUND_API_H

typedef int	sound_t;

typedef struct
{
	string	name;
	int	entnum;
	int	entchannel;
	vec3_t	origin;
	float	volume;
	float	attenuation;
	bool	looping;
	int	pitch;
} soundlist_t;

// sound flags
#define SND_VOLUME			(1<<0)	// a scaled byte
#define SND_ATTENUATION		(1<<1)	// a byte
#define SND_PITCH			(1<<2)	// a byte
#define SND_FIXED_ORIGIN		(1<<3)	// a vector
#define SND_SENTENCE		(1<<4)	// set if sound num is actually a sentence num
#define SND_STOP			(1<<5)	// stop the sound
#define SND_CHANGE_VOL		(1<<6)	// change sound vol
#define SND_CHANGE_PITCH		(1<<7)	// change sound pitch
#define SND_SPAWNING		(1<<8)	// we're spawning, used in some cases for ambients

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
	int  (*GetCurrentStaticSounds)( soundlist_t *pout, int size, int entchannel );
	void (*StartBackgroundTrack)( const char *introTrack, const char *loopTrack );
	void (*StopBackgroundTrack)( void );

	void (*StartStreaming)( void );
	void (*StopStreaming)( void );

	void (*RenderFrame)( struct ref_params_s *fd );
	void (*StopSound)( int entnum, int channel, const char *soundname );
	void (*StopAllSounds)( void );
	void (*ExtraUpdate)( void );

	void (*Activate)( bool active, void *hInst );

} vsound_exp_t;

typedef struct vsound_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(vsound_imp_t)

	bool (*GetEntitySpatialization)( int entnum, vec3_t origin, vec3_t velocity );
	void (*AmbientLevels)( const vec3_t p, byte *pvolumes );
	struct cl_entity_s *(*GetClientEdict)( int index );
	long (*GetAudioChunk)( char *rawdata, long length );		// movie soundtrack update
	wavdata_t *(*GetMovieInfo)( void );				// params for soundtrack
	float (*GetServerTime)( void );
	bool (*IsInMenu)( void );	// returns true when client is in-menu
	bool (*IsActive)( void );	// returns true when client is completely in-game
} vsound_imp_t;

#endif//VSOUND_API_H