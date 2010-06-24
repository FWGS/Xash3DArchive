//=======================================================================
//			Copyright XashXT Group 2009 ©
//			   s_main.c - sound engine
//=======================================================================

#include <dsound.h>
#include "sound.h"
#include "const.h"
#include "bmodel_ref.h"
#include "mathlib.h"

// =======================================================================
// Internal sound data & structures
// =======================================================================
dma_t		dma;
byte		*sndpool;
listener_t	listener;
static soundfade_t	soundfade;
channel_t		channels[MAX_CHANNELS];
int		total_channels;

int		snd_blocked = 0;
static bool	snd_ambient = 1;
dma_t		dma;

extern DWORD		gSndBufSize;
extern LPDIRECTSOUNDBUFFER	pDSBuf, pDSPBuf;
float		sound_nominal_clip_dist = 1000.0;
int		soundtime;	// sample PAIRS
int   		paintedtime; 	// sample PAIRS
sound_t		ambient_sfx[NUM_AMBIENTS];

// console variables
cvar_t		*musicvolume;
cvar_t		*volume;
cvar_t		*ambient_level;
cvar_t		*ambient_fade;
cvar_t		*snd_mixahead;
cvar_t		*snd_khz;
cvar_t		*snd_show;
cvar_t		*snd_allowEAX;
cvar_t		*snd_allowA3D;

// ====================================================================
// User-setable variables
// ====================================================================
void S_AmbientOff( void )
{
	snd_ambient = false;
}

void S_AmbientOn( void )
{
	snd_ambient = true;
}

/*
===============================================================================

console functions

===============================================================================
*/
void S_Play_f( void )
{
	if( Cmd_Argc() == 1 )
	{
		Msg( "Usage: play <soundfile>\n" );
		return;
	}
	S_StartLocalSound( Cmd_Argv( 1 ), 1.0f, PITCH_NORM, NULL );
}

/*
=================
S_Music_f
=================
*/
void S_Music_f( void )
{
	int	c = Cmd_Argc();

	if( c == 2 ) S_StartBackgroundTrack( Cmd_Argv(1), Cmd_Argv(1) );
	else if( c == 3 ) S_StartBackgroundTrack( Cmd_Argv(1), Cmd_Argv(2) );
	else Msg( "Usage: music <musicfile> [loopfile]\n" );
}

/*
=================
S_StopSound_f
=================
*/
void S_StopSound_f( void )
{
	S_StopAllSounds();
}

/*
=================
S_SoundInfo_f
=================
*/
void S_SoundInfo_f( void )
{
	S_PrintDeviceName();
	Msg( "%5d channel(s)\n", dma.channels );
	Msg( "%5d samples\n", dma.samples );
	Msg( "%5d bits/sample\n", dma.samplebits );
	Msg( "%5d bytes/sec\n", dma.speed );
	Msg( "%5d total_channels\n", total_channels );
	
	MsgDev( D_NOTE, "%5d samplepos\n", dma.samplepos );
	MsgDev( D_NOTE, "%5d submission_chunk\n", dma.submission_chunk );
	MsgDev( D_NOTE, "0x%x dma buffer\n", dma.buffer );
}

/*
================
S_Init
================
*/
bool S_Init( void *hInst )
{
	Cmd_ExecuteString( "sndlatch\n" );

	volume = Cvar_Get( "s_volume", "0.7", CVAR_ARCHIVE, "sound volume" );
	musicvolume = Cvar_Get("s_musicvolume", "1.0", CVAR_ARCHIVE, "background music volume" );
	ambient_level = Cvar_Get( "ambient_level", "0.3", CVAR_ARCHIVE, "volume of environment noises (water and wind)" );
	ambient_fade = Cvar_Get( "ambient_fade", "100", CVAR_ARCHIVE, "rate of volume fading when moving from one environment to another" );
	snd_khz = Cvar_Get( "s_khz", "22", CVAR_LATCH_AUDIO|CVAR_ARCHIVE, "output sound frequency" );
	snd_mixahead = Cvar_Get( "s_mixahead", "0.2", CVAR_ARCHIVE, "how much sound to mix ahead of time" );
	snd_show = Cvar_Get( "s_show", "0", 0, "show playing sounds" );
	snd_allowEAX = Cvar_Get("s_allowEAX", "1", CVAR_LATCH_AUDIO|CVAR_ARCHIVE, "allow EAX 2.0 extension" );
	snd_allowA3D = Cvar_Get("s_allowA3D", "1", CVAR_LATCH_AUDIO|CVAR_ARCHIVE, "allow A3D 2.0 extension" );

	Cmd_AddCommand( "play", S_Play_f, "playing a specified sound file" );
	Cmd_AddCommand( "s_stop", S_StopSound_f, "stop all sounds" );
	Cmd_AddCommand( "music", S_Music_f, "starting a background track" );
	Cmd_AddCommand( "soundlist", S_SoundList_f, "display loaded sounds" );
	Cmd_AddCommand( "s_info", S_SoundInfo_f, "print sound system information" );

	S_SetHWND( hInst );

	if( !SNDDMA_Init( ))
	{
		MsgDev( D_INFO, "S_Init: sound system can't be initialized\n" );
		return false;
	}

	sndpool = Mem_AllocPool( "Sound Zone" );
	soundtime = 0;
	paintedtime = 0;

	SND_InitScaletable ();
	S_StopAllSounds ();
	SX_Init ();

	ambient_sfx[AMBIENT_WATER] = S_RegisterSound( "ambience/water1.wav" );
	ambient_sfx[AMBIENT_SKY] = S_RegisterSound( "ambience/wind2.wav" );

	return true;
}

/*
================
S_Shutdown
================
*/
void S_Shutdown( void )
{
	Cmd_RemoveCommand( "play" );
	Cmd_RemoveCommand( "s_stop" );
	Cmd_RemoveCommand( "music" );
	Cmd_RemoveCommand( "soundlist" );
	Cmd_RemoveCommand( "s_info" );

	S_StopAllSounds ();
	S_FreeSounds ();
	SX_Free ();

	SNDDMA_Shutdown ();
	Mem_FreePool( &sndpool );
}

/*
=============================================================================

		SOUNDS PROCESSING

=============================================================================
*/
/*
=================
S_GetMasterVolume
=================
*/
float S_GetMasterVolume( void )
{
	float	scale = 1.0f;

	if( listener.ingame && soundfade.percent != 0 )
	{
		scale = bound( 0.0f, soundfade.percent / 100.0f, 1.0f );
		scale = 1.0f - scale;
	}
	return volume->value * scale;
}

/*
=================
S_FadeClientVolume
=================
*/
void S_FadeClientVolume( float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds )
{
	soundfade.starttime	= si.GetServerTime();
	soundfade.initial_percent = fadePercent;       
	soundfade.fadeouttime = fadeOutSeconds;    
	soundfade.holdtime = holdTime;   
	soundfade.fadeintime = fadeInSeconds;
}

/*
=================
S_UpdateSoundFade
=================
*/
void S_UpdateSoundFade( void )
{
	float	f, totaltime, elapsed;

	// determine current fade value.
	// assume no fading remains
	soundfade.percent = 0;  

	totaltime = soundfade.fadeouttime + soundfade.fadeintime + soundfade.holdtime;

	elapsed = si.GetServerTime() - soundfade.starttime;

	// clock wrapped or reset (BUG) or we've gone far enough
	if( elapsed < 0.0f || elapsed >= totaltime || totaltime <= 0.0f )
		return;

	// We are in the fade time, so determine amount of fade.
	if( soundfade.fadeouttime > 0.0f && ( elapsed < soundfade.fadeouttime ))
	{
		// ramp up
		f = elapsed / soundfade.fadeouttime;
	}
	else if( elapsed <= ( soundfade.fadeouttime + soundfade.holdtime ))	// Inside the hold time
	{
		// stay
		f = 1.0f;
	}
	else
	{
		// ramp down
		f = ( elapsed - ( soundfade.fadeouttime + soundfade.holdtime ) ) / soundfade.fadeintime;
		f = 1.0f - f; // backward interpolated...
	}

	// spline it.
	f = SimpleSpline( f );
	f = bound( 0.0f, f, 1.0f );

	soundfade.percent = soundfade.initial_percent * f;
}

/*
=================
S_IsClient
=================
*/
bool S_IsClient( int entnum )
{
	return ( entnum == listener.entnum );
}

/*
=================
SND_PickChannel
=================
*/
channel_t *SND_PickChannel( int entnum, int entchannel )
{
	int	ch_idx;
	int	first_to_die;
	int	life_left;

	// check for replacement sound, or find the best one to replace
	first_to_die = -1;
	life_left = 0x7fffffff;

	for( ch_idx = NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++ )
	{
		if( entchannel != 0		// channel 0 never overrides
		&& channels[ch_idx].entnum == entnum
		&& ( channels[ch_idx].entchannel == entchannel || entchannel == -1 ))
		{	
			// allways override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if( S_IsClient( channels[ch_idx].entnum ) && !S_IsClient( entnum ) && channels[ch_idx].sfx )
			continue;

		if( channels[ch_idx].end - paintedtime < life_left )
		{
			life_left = channels[ch_idx].end - paintedtime;
			first_to_die = ch_idx;
		}
	}

	if( first_to_die == -1 )
		return NULL;

	if( channels[first_to_die].sfx )
		channels[first_to_die].sfx = NULL;

	return &channels[first_to_die];    
}       

/*
=================
SND_Spatialize
=================
*/
void SND_Spatialize( channel_t *ch )
{
	float	dist, dot;
	float	lscale, rscale, scale;
	vec3_t	source_vec;
	sfx_t	*snd;

	// anything coming from the view entity will allways be full volume
	if( S_IsClient( ch->entnum ))
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

	// calculate stereo seperation and distance attenuation
	snd = ch->sfx;
	VectorSubtract( ch->origin, listener.origin, source_vec );
	
	dist = VectorNormalizeLength( source_vec ) * ch->dist_mult;
	dot = DotProduct( listener.right, source_vec );

	if( dma.channels == 1 )
	{
		rscale = 1.0f;
		lscale = 1.0f;
	}
	else
	{
		rscale = 1.0f + dot;
		lscale = 1.0f - dot;
	}

	// add in distance effect
	scale = ( 1.0f - dist ) * rscale;
	ch->rightvol = (int)( ch->master_vol * scale );
	if( ch->rightvol < 0 ) ch->rightvol = 0;

	scale = ( 1.0f - dist ) * lscale;
	ch->leftvol = (int)( ch->master_vol * scale );
	if( ch->leftvol < 0 ) ch->leftvol = 0;
}           


// =======================================================================
// Start a sound effect
// =======================================================================
void S_StartSound( const vec3_t pos, int ent, int chan, sound_t handle, float fvol, float attn, int pitch, int flags )
{
	channel_t	*target_chan, *check;
	int	vol, ch_idx, skip;
	sfx_t	*sfx = NULL;

	vol = fvol * 255;

	// pick a channel to play on
	target_chan = SND_PickChannel( ent, chan );
	if( !target_chan ) return;
		
	// spatialize
	Mem_Set( target_chan, 0, sizeof( *target_chan ));
	if( pos ) VectorCopy( pos, target_chan->origin );
	target_chan->dist_mult = attn / sound_nominal_clip_dist;
	target_chan->master_vol = vol;
	target_chan->entnum = ent;
	target_chan->entchannel = chan;

	SND_Spatialize( target_chan );

	if( !target_chan->leftvol && !target_chan->rightvol )
		return; // not audible at all

	// new channel
	sfx = S_GetSfxByHandle( handle );
	if( !sfx )
	{
		target_chan->sfx = NULL;
		return; // couldn't load the sound's data
	}

	target_chan->sfx = sfx;
	target_chan->pos = 0.0;
	target_chan->end = paintedtime + sfx->cache->samples;	

	// if an identical sound has also been started this frame, offset the pos
	// a bit to keep it from just making the first one louder
	check = &channels[NUM_AMBIENTS];

	for( ch_idx = NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++, check++ )
	{
		if( check == target_chan )
			continue;

		if( check->sfx == sfx && !check->pos )
		{
			skip = rand () % (int)(0.1f * dma.speed );
			if( skip >= target_chan->end )
				skip = target_chan->end - 1;
			target_chan->pos += skip;
			target_chan->end -= skip;
			break;
		}
		
	}
}

/*
==================
S_StopSound

stop all sounds for entity on a channel.
==================
*/
void S_StopSound( int entnum, int channel, const char *soundname )
{
	int	i;

	for( i = 0; i < MAX_DYNAMIC_CHANNELS; i++ )
	{
		channel_t	*ch = &channels[i];

		if( ch->entnum == entnum && ch->entchannel == channel )
		{
			if( soundname && com.strcmp( ch->sfx->name, soundname ))
				continue;
			ch->end = 0;
			ch->sfx = NULL;
			return;
		}
	}
}

/*
==================
S_StopAllSounds
==================
*/
void S_StopAllSounds( void )
{
	int	i;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS; // no statics

	for( i = 0; i < MAX_CHANNELS; i++ )
		if( channels[i].sfx )
			channels[i].sfx = NULL;

	// clear any remaining soundfade
	Mem_Set( &soundfade, 0, sizeof( soundfade ));
	volume->modified = true; // rebuild scaletable

	// clear all the channels
	Mem_Set( channels, 0, sizeof( channel_t ) * MAX_CHANNELS );
	S_ClearBuffer ();
}

void S_ClearBuffer( void )
{
	int	clear;
		
	if( !dma.buffer && !pDSBuf )
		return;

	if( dma.samplebits == 8 )
		clear = 0x80;
	else clear = 0;

	if( pDSBuf )
	{
		DWORD	dwSize;
		DWORD	*pData;
		HRESULT	hresult;
		int	reps = 0;

		while(( hresult = pDSBuf->lpVtbl->Lock( pDSBuf, 0, gSndBufSize, &pData, &dwSize, NULL, NULL, 0 )) != DS_OK )
		{
			if( hresult != DSERR_BUFFERLOST )
			{
				MsgDev( D_ERROR, "S_ClearBuffer: DS->Lock failed\n" );
				S_Shutdown();
				return;
			}

			if( ++reps > 10000 )
			{
				MsgDev( D_ERROR, "S_ClearBuffer: couldn't restore buffer\n" );
				S_Shutdown();
				return;
			}
		}

		Mem_Set( pData, clear, dma.samples * dma.samplebits / 8 );
		pDSBuf->lpVtbl->Unlock( pDSBuf, pData, dwSize, NULL, 0 );
	
	}
	else
	{
		Mem_Set( dma.buffer, clear, dma.samples * dma.samplebits / 8 );
	}
}

/*
=================
S_StaticSound
=================
*/
void S_StaticSound( const vec3_t pos, int ent, int chan, sound_t handle, float fvol, float attn, int pitch, int flags )
{
	channel_t	*target_chan;
	sfx_t	*sfx = NULL;

	if( total_channels == MAX_CHANNELS )
	{
		MsgDev( D_ERROR, "S_StaticSound: no free channels\n" );
		return;
	}

	target_chan = &channels[total_channels];
	total_channels++;

	// new channel
	sfx = S_GetSfxByHandle( handle );
	if( !sfx )
	{
		target_chan->sfx = NULL;
		return; // couldn't load the sound's data
	}

	if( sfx->cache->loopStart == -1 )
	{
		MsgDev( D_INFO, "sound %s not looped. Ignored.\n", sfx->name );
		return;
	}
	
	target_chan->sfx = sfx;
	if( pos ) VectorCopy( pos, target_chan->origin );
	target_chan->master_vol = (int)fvol;
	target_chan->dist_mult = ( attn / 64 ) / sound_nominal_clip_dist;
	target_chan->end = paintedtime + sfx->cache->samples;	
	
	SND_Spatialize( target_chan );
}

/*
==================
S_StartLocalSound
==================
*/
bool S_StartLocalSound(  const char *name, float volume, int pitch, const float *org )
{
	sound_t	sfxHandle;
	
	sfxHandle = S_RegisterSound( name );
	S_StartSound( org, listener.entnum, CHAN_AUTO, sfxHandle, volume, ATTN_NONE, pitch, SND_STOP_LOOPING );

	return true;
}

//=============================================================================

/*
===================
S_UpdateAmbientSounds
===================
*/
void S_UpdateAmbientSounds( void )
{
	float	vol;
	int	ambient_channel;
	byte	volumes[NUM_AMBIENTS];
	channel_t	*chan;

	if( !snd_ambient ) return;

	// calc ambient sound levels
	if( !listener.ingame || listener.paused )
		return;

	if( !ambient_level->value )
	{
		for( ambient_channel = 0; ambient_channel < NUM_AMBIENTS; ambient_channel++ )
			channels[ambient_channel].sfx = NULL;
		return;
	}

	si.AmbientLevels( listener.origin, volumes );

	for( ambient_channel = 0; ambient_channel < NUM_AMBIENTS; ambient_channel++ )
	{
		chan = &channels[ambient_channel];	
		chan->sfx = S_GetSfxByHandle( ambient_sfx[ambient_channel] );
	
		vol = ambient_level->value * volumes[ambient_channel];
		if( vol < 8 ) vol = 0;

		// don't adjust volume too fast
		if( chan->master_vol < vol )
		{
			chan->master_vol += listener.frametime * ambient_fade->value;
			if( chan->master_vol > vol )
				chan->master_vol = vol;
		}
		else if( chan->master_vol > vol )
		{
			chan->master_vol -= listener.frametime * ambient_fade->value;
			if( chan->master_vol < vol )
				chan->master_vol = vol;
		}
		chan->leftvol = chan->rightvol = chan->master_vol;
	}
}

void GetSoundtime( void )
{
	static int	buffers;
	static int	oldsamplepos;
	int		fullsamples;
	int		samplepos;	

	fullsamples = dma.samples / dma.channels;
	samplepos = SNDDMA_GetDMAPos();

	if( samplepos < oldsamplepos )
	{
		buffers++; // buffer wrapped
		
		if( paintedtime > 0x40000000 )
		{	
			// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds();
		}
	}

	oldsamplepos = samplepos;
	soundtime = buffers * fullsamples + samplepos / dma.channels;
}

void S_Update( void )
{
	uint	endtime;
	int	samps;
	
	if( snd_blocked > 0 )
		return;

	// updates DMA time
	GetSoundtime();

	// check to make sure that we haven't overshot
	if( paintedtime < soundtime )
		paintedtime = soundtime;

	// mix ahead of current position
	endtime = soundtime + snd_mixahead->value * dma.speed;
	samps = dma.samples >> ( dma.channels - 1 );
	if( endtime - soundtime > samps )
		endtime = soundtime + samps;

	// if the buffer was lost or stopped, restore it and/or restart it
	if( pDSBuf )
	{
		DWORD	dwStatus;
		
		if( pDSBuf->lpVtbl->GetStatus( pDSBuf, &dwStatus) != DD_OK )
			MsgDev( D_ERROR, "S_Update: DS->GetStatus failed\n" );
			
		if( dwStatus & DSBSTATUS_BUFFERLOST )
			pDSBuf->lpVtbl->Restore( pDSBuf );
			
		if(!( dwStatus & DSBSTATUS_PLAYING ))
			pDSBuf->lpVtbl->Play( pDSBuf, 0, 0, DSBPLAY_LOOPING );
	}

	S_PaintChannels( endtime);
	SNDDMA_Submit();
}

/*
============
S_Update

Called once each time through the main loop
============
*/
void S_SoundFrame( ref_params_t *fd )
{
	int	i, j, total;
	channel_t	*ch, *combine;

	if( !fd || ( snd_blocked > 0 ))
		return;

	// if the loading plaque is up, clear everything
	// out to make sure we aren't looping a dirty
	// dma buffer while loading
	if( fd->paused && !listener.paused )
	{
		S_ClearBuffer();
	}

	// update any client side sound fade
	S_UpdateSoundFade();

	if( listener.ingame ^ si.IsInGame())
	{
		// state changed, rebuild scaletable
		volume->modified = true;
	}

	listener.entnum = fd->viewentity;	// can be camera entity too
	listener.frametime = fd->frametime;
	listener.waterlevel = fd->waterlevel;
	listener.ingame = si.IsInGame();
	listener.paused = fd->paused;

	if( listener.paused ) return;

	VectorCopy( fd->simorg, listener.origin );
	VectorCopy( fd->vieworg, listener.vieworg );
	VectorCopy( fd->simvel, listener.velocity );
	VectorCopy( fd->forward, listener.forward );
	VectorCopy( fd->right, listener.right );
	VectorCopy( fd->up, listener.up );

	// rebuild scale tables if volume is modified
	if( volume->modified || soundfade.percent != 0 )
		SND_InitScaletable();
	
	// update general area ambient sound sources
	S_UpdateAmbientSounds ();

	combine = NULL;

	// update spatialization for static and dynamic sounds	
	ch = channels + NUM_AMBIENTS;
	for( i = NUM_AMBIENTS; i < total_channels; i++, ch++ )
	{
		if( !ch->sfx ) continue;

		SND_Spatialize( ch ); // respatialize channel
		if( !ch->leftvol && !ch->rightvol )
			continue;

		// try to combine static sounds with a previous channel of the same
		// sound effect so we don't mix five torches every frame
		if( i >= MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS )
		{
			// see if it can just use the last one
			if( combine && combine->sfx == ch->sfx )
			{
				combine->leftvol += ch->leftvol;
				combine->rightvol += ch->rightvol;
				ch->leftvol = ch->rightvol = 0;
				continue;
			}
			// search for one
			combine = channels + MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;
			for( j = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS; j < i; j++, combine++ )
			{
				if( combine->sfx == ch->sfx )
					break;
			}					

			if( j == total_channels )
			{
				combine = NULL;
			}
			else
			{
				if( combine != ch )
				{
					combine->leftvol += ch->leftvol;
					combine->rightvol += ch->rightvol;
					ch->leftvol = ch->rightvol = 0;
				}
				continue;
			}
		}
	}

	// debugging output
	if( snd_show->integer )
	{
		total = 0;
		ch = channels;
		for( i = 0; i < total_channels; i++, ch++ )
		{
			if( ch->sfx && (ch->leftvol || ch->rightvol ))
			{
				//Msg( "%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name );
				total++;
			}
		}
		Msg( "----(%i)----\n", total );
	}

	// mix some sound
	S_Update();
}