//=======================================================================
//			Copyright XashXT Group 2009 �
//			sound.h - sndlib main header
//=======================================================================

#ifndef SOUND_H
#define SOUND_H

extern byte *sndpool;

#include "mathlib.h"

// local flags (never sending acorss the net)
#define SND_LOCALSOUND	(1<<9)	// not paused, not looped, for internal use
#define SND_STOP_LOOPING	(1<<10)	// stop all looping sounds on the entity.

// sound engine rate defines
#define SOUND_DMA_SPEED	44100	// hardware playback rate
#define SOUND_11k		11025	// 11khz sample rate
#define SOUND_22k		22050	// 22khz sample rate
#define SOUND_44k		44100	// 44khz sample rate

// fixed point stuff for real-time resampling
#define FIX_BITS		28
#define FIX_SCALE		(1 << FIX_BITS)
#define FIX_MASK		((1 << FIX_BITS)-1)
#define FIX_FLOAT(a)	((int)((a) * FIX_SCALE))
#define FIX(a)		(((int)(a)) << FIX_BITS)
#define FIX_INTPART(a)	(((int)(a)) >> FIX_BITS)
#define FIX_FRACTION(a,b)	(FIX(a)/(b))
#define FIX_FRACPART(a)	((a) & FIX_MASK)

#define CLIP( x )		(( x ) > 32767 ? 32767 : (( x ) < -32767 ? -32767 : ( x )))
#define SWAP( a, b, t )	{(t) = (a); (a) = (b); (b) = (t);}
#define AVG( a, b )		(((a) + (b)) >> 1 )
#define AVG4( a, b, c, d )	(((a) + (b) + (c) + (d)) >> 2 )

#define PAINTBUFFER_SIZE	1024	// 44k: was 512
#define PAINTBUFFER		(g_curpaintbuffer)
#define CPAINTBUFFERS	3

typedef struct
{
	int		left;
	int		right;
} portable_samplepair_t;

// sound mixing buffer

#define CPAINTFILTERMEM		3
#define CPAINTFILTERS		4	// maximum number of consecutive upsample passes per paintbuffer

typedef struct
{
	qboolean			factive;	// if true, mix to this paintbuffer using flags
	portable_samplepair_t	*pbuf;	// front stereo mix buffer, for 2 or 4 channel mixing
	int			ifilter;	// current filter memory buffer to use for upsampling pass
	portable_samplepair_t	fltmem[CPAINTFILTERS][CPAINTFILTERMEM];
} paintbuffer_t;

typedef struct sfx_s
{
	string 		name;
	wavdata_t		*cache;

	int		touchFrame;
	uint		hashValue;
	struct sfx_s	*hashNext;
} sfx_t;

extern portable_samplepair_t	drybuffer[];
extern portable_samplepair_t	paintbuffer[];
extern portable_samplepair_t	roombuffer[];
extern portable_samplepair_t	temppaintbuffer[];
extern portable_samplepair_t	*g_curpaintbuffer;
extern paintbuffer_t	paintbuffers[];

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
	int		samples;		// mono samples in buffer
	int		samplepos;	// in mono samples
	byte		*buffer;
	qboolean		initialized;	// sound engine is active
} dma_t;

#include "vox.h"

typedef struct
{
	double		sample;

	wavdata_t		*pData;
	double 		forcedEndSample;
	qboolean		finished;
} mixer_t;

typedef struct
{
	sfx_t		*sfx;		// sfx number

	int		leftvol;		// 0-255 left volume
	int		rightvol;		// 0-255 right volume

	int		entnum;		// entity soundsource
	int		entchannel;	// sound channel (CHAN_STREAM, CHAN_VOICE, etc.)	
	vec3_t		origin;		// only use if fixed_origin is set
	float		dist_mult;	// distance multiplier (attenuation/clipK)
	int		master_vol;	// 0-255 master volume
	qboolean		isSentence;	// bit who indicated sentence
	int		basePitch;	// base pitch percent (100% is normal pitch playback)
	float		pitch;		// real-time pitch after any modulation or shift by dynamic data
	qboolean		use_loop;		// don't loop default and local sounds
	qboolean		staticsound;	// use origin instead of fetching entnum's origin
	qboolean		localsound;	// it's a local menu sound (not looped, not paused)
	qboolean		bdry;		// if true, bypass all dsp processing for this sound (ie: music)
	mixer_t		pMixer;

	// sentence mixer
	int		wordIndex;
	mixer_t		*currentWord;	// NULL if sentence is finished
	voxword_t		words[CVOXWORDMAX];
} channel_t;

typedef struct
{
	vec3_t		origin;		// simorg + view_ofs
	vec3_t		velocity;
	vec3_t		forward;
	vec3_t		right;
	vec3_t		up;

	int		entnum;
	int		waterlevel;
	float		frametime;	// used for sound fade
	qboolean		active;
	qboolean		inmenu;		// listener in-menu ?
	qboolean		paused;
	qboolean		streaming;	// playing AVI-file
	qboolean		lerping;		// lerp stream ?
	qboolean		stream_paused;	// pause only background track
} listener_t;

typedef struct
{
	string		loopName;
	stream_t		*stream;
	int		source;		// may be game, menu, etc
} bg_track_t;

/*
====================================================================

  SYSTEM SPECIFIC FUNCTIONS

====================================================================
*/
#define Host_Error		com.error

// initializes cycling through a DMA buffer and returns information on it
qboolean SNDDMA_Init( void *hInst );
int SNDDMA_GetSoundtime( void );
void SNDDMA_Shutdown( void );
void SNDDMA_BeginPainting( void );
void SNDDMA_Submit( void );

//====================================================================

#define MAX_DYNAMIC_CHANNELS	28
#define MAX_CHANNELS	128
#define MAX_RAW_SAMPLES	8192

extern channel_t	channels[MAX_CHANNELS];
extern int	total_channels;
extern int	paintedtime;
extern int	s_rawend;
extern int	soundtime;
extern dma_t	dma;
extern listener_t	s_listener;
extern int	idsp_room;

extern convar_t	*s_check_errors;
extern convar_t	*s_volume;
extern convar_t	*s_musicvolume;
extern convar_t	*s_show;
extern convar_t	*s_mixahead;
extern convar_t	*s_primary;
extern convar_t	*s_lerping;
extern convar_t	*dsp_off;

extern portable_samplepair_t		s_rawsamples[MAX_RAW_SAMPLES];

void S_InitScaletable( void );
wavdata_t *S_LoadSound( sfx_t *sfx );
float S_GetMasterVolume( void );
void S_PrintDeviceName( void );

//
// s_main.c
//
void S_FreeChannel( channel_t *ch );

//
// s_mix.c
//
int S_MixDataToDevice( channel_t *pChannel, int sampleCount, int outputRate, int outputOffset );
void MIX_ClearAllPaintBuffers( int SampleCount, qboolean clearFilters );
void MIX_InitAllPaintbuffers( void );
void MIX_FreeAllPaintbuffers( void );
void MIX_PaintChannels( int endtime );

// s_load.c
qboolean S_TestSoundChar( const char *pch, char c );
char *S_SkipSoundChar( const char *pch );
sfx_t *S_FindName( const char *name, int *pfInCache );
void S_FreeSound( sfx_t *sfx );

// s_dsp.c
qboolean AllocDsps( void );
void FreeDsps( void );
void CheckNewDspPresets( void );
void DSP_Process( int idsp, portable_samplepair_t *pbfront, int sampleCount );
float DSP_GetGain( int idsp );
void DSP_ClearState( void );

qboolean S_Init( void *hInst );
void S_Shutdown( void );
void S_Activate( qboolean active, void *hInst );
void S_SoundList_f( void );
void S_SoundInfo_f( void );

channel_t *SND_PickDynamicChannel( int entnum, int channel, sfx_t *sfx );
channel_t *SND_PickStaticChannel( int entnum, sfx_t *sfx );
int S_GetCurrentStaticSounds( soundlist_t *pout, int size, int entchannel );
sfx_t *S_GetSfxByHandle( sound_t handle );
void S_StopSound( int entnum, int channel, const char *soundname );
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
void S_StreamSoundTrack( void );
void S_StreamBackgroundTrack( void );

//
// s_utils.c
//
int S_ZeroCrossingAfter( wavdata_t *pWaveData, int sample );
int S_ZeroCrossingBefore( wavdata_t *pWaveData, int sample );
int S_GetOutputData( wavdata_t *pSource, void **pData, int samplePosition, int sampleCount, qboolean use_loop );
void S_SetSampleStart( channel_t *pChan, wavdata_t *pSource, int newPosition );
void S_SetSampleEnd( channel_t *pChan, wavdata_t *pSource, int newEndPosition );
float S_SimpleSpline( float value );

//
// s_vox.c
//
void VOX_Init( void );
void VOX_Shutdown( void );
void VOX_SetChanVol( channel_t *ch );
void VOX_LoadSound( channel_t *pchan, const char *psz );
float VOX_ModifyPitch( channel_t *ch, float pitch );
int VOX_MixDataToDevice( channel_t *pChannel, int sampleCount, int outputRate, int outputOffset );

#endif//SOUND_H