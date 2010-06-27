//=======================================================================
//			Copyright XashXT Group 2009 ©
//			sound.h - sndlib main header
//=======================================================================

#ifndef SOUND_H
#define SOUND_H

#include <windows.h>
#include "launch_api.h"
#include "qfiles_ref.h"
#include "engine_api.h"	// trace_t declaration
#include "vsound_api.h"
#include "entity_def.h"

extern stdlib_api_t com;
extern vsound_imp_t	si;
extern byte *sndpool;

#include "mathlib.h"

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

#include "vox.h"

typedef struct
{
	double		sample;

	wavdata_t		*pData;
	double 		forcedEndSample;
	bool		m_finished;
	int		delaySamples;
} mixer_t;

typedef struct
{
	sfx_t		*sfx;		// sfx number
	int		end;		// end time in global paintsamples
	int 		pos;		// sample position in sfx

	int		leftvol;		// 0-255 left volume
	int		rightvol;		// 0-255 right volume

	int		entnum;		// entity soundsource
	int		entchannel;	// sound channel (CHAN_STREAM, CHAN_VOICE, etc.)	
	vec3_t		origin;		// only use if fixed_origin is set
	float		dist_mult;	// distance multiplier (attenuation/clipK)
	int		master_vol;	// 0-255 master volume
	bool		isSentence;	// bit who indicated sentence
	int		basePitch;	// base pitch percent (100% is normal pitch playback)
	float		pitch;		// real-time pitch after any modulation or shift by dynamic data
	bool		use_loop;		// don't loop default and local sounds
	bool		staticsound;	// use origin instead of fetching entnum's origin

	// sentence mixer
	int		wordIndex;
	mixer_t		currentWord;
	voxword_t		words[CVOXWORDMAX];
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
	int		rate;
	int		width;
	int		channels;
	int		loopstart;
	int		samples;
	int		dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;

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

// initializes cycling through a DMA buffer and returns information on it
bool SNDDMA_Init( void *hInst );
int SNDDMA_GetSoundtime( void );
void SNDDMA_Shutdown( void );
void SNDDMA_BeginPainting( void );
void SNDDMA_Submit( void );

//====================================================================

#define MAX_DYNAMIC_CHANNELS	24
#define MAX_CHANNELS	128
#define MAX_RAW_SAMPLES	8192

extern portable_samplepair_t paintbuffer[];
extern channel_t	channels[MAX_CHANNELS];
extern int	total_channels;
extern int	paintedtime;
extern int	s_rawend;
extern int	soundtime;
extern dma_t	dma;
extern listener_t	s_listener;

extern cvar_t	*s_check_errors;
extern cvar_t	*s_volume;
extern cvar_t	*s_musicvolume;
extern cvar_t	*s_khz;
extern cvar_t	*s_show;
extern cvar_t	*s_mixahead;
extern cvar_t	*s_primary;

extern portable_samplepair_t		s_rawsamples[MAX_RAW_SAMPLES];

void S_InitScaletable( void );
wavdata_t *S_LoadSound( sfx_t *sfx );
void S_PaintChannels( int endtime );
float S_GetMasterVolume( void );
void S_PrintDeviceName( void );

//
// s_main.c
//
void S_FreeChannel( channel_t *ch );

// s_load.c
bool S_TestSoundChar( const char *pch, char c );
char *S_SkipSoundChar( const char *pch );
sfx_t *S_FindName( const char *name, int *pfInCache );

// s_dsp.c
void SX_Init( void );
void SX_Free( void );
void SX_RoomFX( int endtime, int fFilter, int fTimefx );

bool S_Init( void *hInst );
void S_Shutdown( void );
void S_Activate( bool active );
void S_SoundList_f( void );
void S_SoundInfo_f( void );

// if origin is NULL, the sound will be dynamically sourced from the entity
void S_StartSound( const vec3_t pos, int ent, int chan, sound_t sfx, float vol, float attn, int pitch, int flags );
void S_StaticSound( const vec3_t pos, int ent, int chan, sound_t handle, float fvol, float attn, int pitch, int flags );
channel_t *SND_PickDynamicChannel( int entnum, int channel, sfx_t *sfx );
channel_t *SND_PickStaticChannel( int entnum, sfx_t *sfx );
void S_FadeClientVolume( float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds );
int S_StartLocalSound( const char *name, float volume, int pitch, const float *org );
sfx_t *S_GetSfxByHandle( sound_t handle );
void S_StopSound( int entnum, int channel, const char *soundname );
void S_RenderFrame( ref_params_t *fd );
void S_StopAllSounds( void );
void S_FreeSounds( void );

//
// s_mouth.c
//
void SND_InitMouth( int entnum, int entchannel );
void SND_MoveMouth8( channel_t *ch, wavdata_t *pSource, int count );
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

//
// s_utils.c
//
int S_ZeroCrossingAfter( wavdata_t *pWaveData, int sample );
int S_ZeroCrossingBefore( wavdata_t *pWaveData, int sample );
int S_GetOutputData( wavdata_t *pSource, void **pData, int samplePosition, int sampleCount );

//
// s_vox.c
//
void VOX_Init( void );
void VOX_Shutdown( void );
void VOX_SetChanVol( channel_t *ch );
void VOX_LoadSound( channel_t *pchan, const char *psz );
void VOX_LoadNextWord( channel_t *pchan );


void S_BeginRegistration( void );
sound_t S_RegisterSound( const char *sample );
void S_EndRegistration( void );

#endif//SOUND_H