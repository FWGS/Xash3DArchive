//=======================================================================
//			Copyright XashXT Group 2009 ©
//		       sound.h - client sound i/o functions
//=======================================================================

#ifndef SOUND_H
#define SOUND_H

#include <windows.h>
#include "launch_api.h"
#include "qfiles_ref.h"
#include "engine_api.h"	// trace_t declaration
#include "vsound_api.h"
#include "entity_def.h"

extern stdlib_api_t	com;
extern vsound_imp_t	si;
extern byte	*sndpool;

#define DEFAULT_SOUND_PACKET_VOLUME		255
#define DEFAULT_SOUND_PACKET_ATTENUATION	1.0
#define MAX_CHANNELS			128
#define MAX_DYNAMIC_CHANNELS			8
#define PAINTBUFFER_SIZE			512
#define MAX_RAW_SAMPLES			8192

typedef struct
{
	int		left;
	int		right;
} portable_samplepair_t;

typedef struct sfx_s
{
	string 		name;
	wavdata_t		*cache;

	int		touchFrame;
	uint		hashValue;
	struct sfx_s	*hashNext;
} sfx_t;

// structure used for fading in and out client sound volume.
typedef struct
{
	float		initial_percent;
	float		percent;  	// how far to adjust client's volume down by.
	float		starttime;	// GetHostTime() when we started adjusting volume
	float		fadeouttime;	// # of seconds to get to faded out state
	float		holdtime;		// # of seconds to hold
	float		fadeintime;	// # of seconds to restore
} soundfade_t;

typedef struct
{
	int		channels;
	int		samples;		// mono samples in buffer
	int		submission_chunk;	// don't mix less than this #
	int		samplepos;	// in mono samples
	int		samplebits;
	int		speed;
	byte		*buffer;
} dma_t;

typedef struct
{
	sfx_t		*sfx;		// sfx number
	int		leftvol;		// 0-255 volume
	int		rightvol;		// 0-255 volume
	int		end;		// end time in global paintsamples
	int 		pos;		// sample position in sfx
	int		looping;		// where to loop, -1 = no looping
	int		entnum;		// to allow overriding a specific sound
	int		entchannel;	//
	vec3_t		origin;		// origin of sound effect
	vec_t		dist_mult;	// distance multiplier (attenuation/clipK)
	int		master_vol;	// 0-255 master volume
} channel_t;

typedef struct
{
	vec3_t		origin;		// simorg
	vec3_t		vieworg;		// simorg + view_ofs
	vec3_t		velocity;
	vec3_t		forward;
	vec3_t		right;
	vec3_t		up;

	int		entnum;
	int		waterlevel;
	float		frametime;	// used for sound fade
	bool		ingame;		// listener in-game ?
	bool		paused;
} listener_t;

typedef struct
{
	string		loopName;
	stream_t		*stream;
} bg_track_t;

/*
====================================================================

  SYSTEM SPECIFIC FUNCTIONS

====================================================================
*/
#define Host_Error		com.error
#define Z_Malloc( size )	Mem_Alloc( sndpool, size )

void S_Shutdown( void );
void S_StopSound( int entnum, int channel, const char *soundname );
void S_StopAllSounds( void );
void S_ClearBuffer( void );
void S_ExtraUpdate( void );

void S_PaintChannels( int endtime );
void S_InitPaintChannels( void );

// picks a channel based on priorities, empty slots, number of channels
channel_t *SND_PickChannel( int entnum, int entchannel );

// spatializes a channel
void SND_Spatialize( channel_t *ch );

//
// s_backend.c
//
void S_PrintDeviceName( void );
void S_SetHWND( void *hInst );
int SNDDMA_Init( void );
int SNDDMA_GetDMAPos( void );
void SNDDMA_Shutdown( void );

//
// s_dsp.c
//
void SX_Init( void );
void SX_Free( void );
void SX_RoomFX( int endtime, int fFilter, int fTimefx );

//
// s_load.c
//
void S_SoundList_f( void );
void S_BeginRegistration( void );
sound_t S_RegisterSound( const char *sample );
sfx_t *S_GetSfxByHandle( sound_t handle );
void S_EndRegistration( void );
void S_FreeSounds( void );

//
// s_main.c
//
bool S_Init( void *hInst );
void S_Activate( bool active );
void S_FadeClientVolume( float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds );
int S_StartLocalSound( const char *name, float volume, int pitch, const float *org );
void S_StartSound( const vec3_t pos, int ent, int chan, sound_t sfx, float fvol, float attn, int pitch, int flags );
void S_StaticSound( const vec3_t pos, int ent, int chan, sound_t sfx, float fvol, float attn, int pitch, int flags );
void S_SoundFrame( ref_params_t *fd );
float S_GetMasterVolume( void );

//
// s_mix.c
//
void SND_InitScaletable( void );


//
// s_mouth.c
//
void SND_InitMouth( int entnum, int entchannel );
void SND_CloseMouth( channel_t *ch );

//
// s_stream.c
//
void S_StartStreaming( void );
void S_StopStreaming( void );
void S_StreamRawSamples( int samples, int rate, int width, int channels, const byte *data );
void S_StartBackgroundTrack( const char *intro, const char *loop );
void S_StreamBackgroundTrack( void );
void S_StopBackgroundTrack( void );

// ====================================================================
// User-setable variables
// ====================================================================

// 0 to MAX_DYNAMIC_CHANNELS-1	= normal entity sounds
// MAX_DYNAMIC_CHANNELS to MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS -1 = water, etc
// MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS to total_channels = static sounds

extern listener_t		listener;
extern portable_samplepair_t	paintbuffer[PAINTBUFFER_SIZE];
extern portable_samplepair_t	rawsamples[MAX_RAW_SAMPLES];
extern channel_t		channels[MAX_CHANNELS];
extern int		total_channels;

// Fake dma is a synchronous faking of the DMA progress used for
// isolating performance in the renderer.  The fakedma_updates is
// number of times S_Update() is called per second.

extern bool 	fakedma;
extern int 	fakedma_updates;
extern int	paintedtime;
extern int	soundtime;
extern int	rawend;
extern dma_t	dma;
extern float	sound_nominal_clip_dist;

extern cvar_t	*musicvolume;
extern cvar_t	*volume;

extern bool	snd_initialized;
extern int	snd_blocked;

wavdata_t *S_LoadSound( sfx_t *s );

void SNDDMA_Submit( void );

void S_AmbientOff( void );
void S_AmbientOn( void );

#endif//SOUND_H