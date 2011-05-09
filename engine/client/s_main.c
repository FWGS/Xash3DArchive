/*
s_main.c - sound engine
Copyright (C) 2009 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"
#include "sound.h"
#include "client.h"
#include "con_nprint.h"
#include "ref_params.h"

#define MAX_DUPLICATED_CHANNELS	4	// threshold for identical static channels (probably error)
#define SND_CLIP_DISTANCE		1024.0f

dma_t		dma;
byte		*sndpool;
static soundfade_t	soundfade;
channel_t   	channels[MAX_CHANNELS];
listener_t	s_listener;
int		total_channels;
int		soundtime;	// sample PAIRS
int   		paintedtime; 	// sample PAIRS

convar_t		*s_check_errors;
convar_t		*s_volume;
convar_t		*s_musicvolume;
convar_t		*s_show;
convar_t		*s_mixahead;
convar_t		*s_primary;
convar_t		*s_lerping;
convar_t		*dsp_off;		// set to 1 to disable all dsp processing

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

	if( !s_listener.inmenu && soundfade.percent != 0 )
	{
		scale = bound( 0.0f, soundfade.percent / 100.0f, 1.0f );
		scale = 1.0f - scale;
	}
	return s_volume->value * scale;
}

/*
=================
S_FadeClientVolume
=================
*/
void S_FadeClientVolume( float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds )
{
	soundfade.starttime	= cl.mtime[0];
	soundfade.initial_percent = fadePercent;       
	soundfade.fadeouttime = fadeOutSeconds;    
	soundfade.holdtime = holdTime;   
	soundfade.fadeintime = fadeInSeconds;
}

/*
=================
S_IsClient
=================
*/
qboolean S_IsClient( int entnum )
{
	return ( entnum == s_listener.entnum );
}


// free channel so that it may be allocated by the
// next request to play a sound.  If sound is a 
// word in a sentence, release the sentence.
// Works for static, dynamic, sentence and stream sounds
/*
=================
S_FreeChannel
=================
*/
void S_FreeChannel( channel_t *ch )
{
	ch->sfx = NULL;
	ch->use_loop = false;
	ch->isSentence = false;

	// clear mixer
	Q_memset( &ch->pMixer, 0, sizeof( ch->pMixer ));

	SND_CloseMouth( ch );
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

	elapsed = cl.mtime[0] - soundfade.starttime;

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
	f = S_SimpleSpline( f );
	f = bound( 0.0f, f, 1.0f );

	soundfade.percent = soundfade.initial_percent * f;
}

/*
=================
SND_PickDynamicChannel

Select a channel from the dynamic channel allocation area.  For the given entity, 
override any other sound playing on the same channel (see code comments below for
exceptions).
=================
*/
channel_t *SND_PickDynamicChannel( int entnum, int channel, sfx_t *sfx )
{
	int	ch_idx;
	int	first_to_die;
	int	life_left;
	int	timeleft;

	// check for replacement sound, or find the best one to replace
	first_to_die = -1;
	life_left = 0x7fffffff;

	for( ch_idx = 0; ch_idx < MAX_DYNAMIC_CHANNELS; ch_idx++ )
	{
		channel_t	*ch = &channels[ch_idx];
		
		// Never override a streaming sound that is currently playing or
		// voice over IP data that is playing or any sound on CHAN_VOICE( acting )
		if( ch->sfx && ( ch->entchannel == CHAN_STREAM ))
		{
			continue;
		}

		if( channel != CHAN_AUTO && ch->entnum == entnum && ( ch->entchannel == channel || channel == -1 ))
		{
			// always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if( ch->sfx && S_IsClient( ch->entnum ) && !S_IsClient( entnum ))
			continue;

		// try to pick the sound with the least amount of data left to play
		timeleft = 0;
		if( ch->sfx )
		{
			timeleft = 1;	//ch->end - paintedtime
		}

		if( timeleft < life_left )
		{
			life_left = timeleft;
			first_to_die = ch_idx;
		}
	}

	if( first_to_die == -1 )
		return NULL;

	if( channels[first_to_die].sfx )
	{
		// don't restart looping sounds for the same entity
		wavdata_t	*sc = channels[first_to_die].sfx->cache;

		if( sc && sc->loopStart != -1 )
		{
			channel_t	*ch = &channels[first_to_die];

			if( ch->entnum == entnum && ch->entchannel == channel && ch->sfx == sfx )
			{
				// same looping sound, same ent, same channel, don't restart the sound
				return NULL;
			}
		}

		// be sure and release previous channel if sentence.
		S_FreeChannel( &( channels[first_to_die] ));
	}

	return &channels[first_to_die];
}       

/*
=====================
SND_PickStaticChannel

Pick an empty channel from the static sound area, or allocate a new
channel.  Only fails if we're at max_channels (128!!!) or if 
we're trying to allocate a channel for a stream sound that is 
already playing.
=====================
*/
channel_t *SND_PickStaticChannel( int entnum, sfx_t *sfx, const vec3_t pos )
{
	channel_t	*ch = NULL;
	int	i, dupe = 0;

	// check for dupliacte sounds
	for( i = 0; i < total_channels; i++ )
	{
		if( channels[i].sfx == sfx && VectorCompare( channels[i].origin, pos ))
			dupe++;
	}

	// check for duplicated static channels (same origin and same sfx)
	if( dupe > MAX_DUPLICATED_CHANNELS )
		return NULL;

	// check for replacement sound, or find the best one to replace
 	for( i = MAX_DYNAMIC_CHANNELS; i < total_channels; i++ )
 	{
		if( channels[i].sfx == NULL )
			break;
	}

	if( i < total_channels ) 
	{
		// reuse an empty static sound channel
		ch = &channels[i];
	}
	else
	{
		// no empty slots, alloc a new static sound channel
		if( total_channels == MAX_CHANNELS )
		{
			MsgDev( D_ERROR, "S_PickChannel: no free channels\n" );
			return NULL;
		}
		// get a channel for the static sound
		ch = &channels[total_channels];
		total_channels++;
	}
	return ch;
}

/*
=================
S_AlterChannel

search through all channels for a channel that matches this
soundsource, entchannel and sfx, and perform alteration on channel
as indicated by 'flags' parameter. If shut down request and
sfx contains a sentence name, shut off the sentence.
returns TRUE if sound was altered,
returns FALSE if sound was not found (sound is not playing)
=================
*/
int S_AlterChannel( int entnum, int channel, sfx_t *sfx, int vol, int pitch, int flags )
{
	channel_t	*ch;
	int	i;	

	if( S_TestSoundChar( sfx->name, '!' ))
	{
		// This is a sentence name.
		// For sentences: assume that the entity is only playing one sentence
		// at a time, so we can just shut off
		// any channel that has ch->isSentence >= 0 and matches the entnum.

		for( i = 0, ch = channels; i < total_channels; i++, ch++ )
		{
			if( ch->entnum == entnum && ch->entchannel == channel && ch->sfx && ch->isSentence )
			{
				if( flags & SND_CHANGE_PITCH )
					ch->basePitch = pitch;
				
				if( flags & SND_CHANGE_VOL )
					ch->master_vol = vol;
				
				if( flags & SND_STOP )
					S_FreeChannel( ch );

				return true;
			}
		}
		// channel not found
		return false;

	}

	// regular sound or streaming sound
	for( i = 0, ch = channels; i < total_channels; i++, ch++ )
	{
		if( ch->entnum == entnum && ch->entchannel == channel && ch->sfx == sfx )
		{
			if( flags & SND_CHANGE_PITCH )
				ch->basePitch = pitch;
				
			if( flags & SND_CHANGE_VOL )
				ch->master_vol = vol;

			if( flags & SND_STOP )
				S_FreeChannel( ch );

			return true;
		}
	}
	return false;
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

	if( !ch->staticsound )
	{
		if( !CL_GetEntitySpatialization( ch->entnum, ch->origin, NULL ))
			return;	// entity not exist on client
	}

	// calculate stereo seperation and distance attenuation
	snd = ch->sfx;
	VectorSubtract( ch->origin, s_listener.origin, source_vec );
	
	dist = VectorNormalizeLength( source_vec ) * ch->dist_mult;
	dot = DotProduct( s_listener.right, source_vec );

	rscale = 1.0f + dot;
	lscale = 1.0f - dot;

	// add in distance effect
	scale = ( 1.0f - dist ) * rscale;
	ch->rightvol = (int)( ch->master_vol * scale );
	if( ch->rightvol < 0 ) ch->rightvol = 0;

	scale = ( 1.0f - dist ) * lscale;
	ch->leftvol = (int)( ch->master_vol * scale );
	if( ch->leftvol < 0 ) ch->leftvol = 0;

	// if playing a word, set volume
	VOX_SetChanVol( ch );
}

/*
====================
S_StartSound

Start a sound effect for the given entity on the given channel (ie; voice, weapon etc).  
Try to grab a channel out of the 8 dynamic spots available.
Currently used for looping sounds, streaming sounds, sentences, and regular entity sounds.
NOTE: volume is 0.0 - 1.0 and attenuation is 0.0 - 1.0 when passed in.
Pitch changes playback pitch of wave by % above or below 100.  Ignored if pitch == 100

NOTE: it's not a good idea to play looping sounds through StartDynamicSound, because
if the looping sound starts out of range, or is bumped from the buffer by another sound
it will never be restarted.  Use StartStaticSound (pass CHAN_STATIC to EMIT_SOUND or
SV_StartSound.
====================
*/
void S_StartSound( const vec3_t pos, int ent, int chan, sound_t handle, float fvol, float attn, int pitch, int flags )
{
	wavdata_t	*pSource;
	sfx_t	*sfx = NULL;
	channel_t	*target_chan, *check;
	int	vol, ch_idx;

	if( pitch <= 1 ) pitch = PITCH_NORM; // Invasion issues

	if( !dma.initialized ) return;
	sfx = S_GetSfxByHandle( handle );
	if( !sfx ) return;

	vol = bound( 0, fvol * 255, 255 );

	if( flags & ( SND_STOP|SND_CHANGE_VOL|SND_CHANGE_PITCH ))
	{
		if( S_AlterChannel( ent, chan, sfx, vol, pitch, flags ))
			return;

		if( flags & SND_STOP ) return;
		// fall through - if we're not trying to stop the sound, 
		// and we didn't find it (it's not playing), go ahead and start it up
	}

	if( pitch == 0 )
	{
		MsgDev( D_WARN, "S_StartSound: ( %s ) ignored, called with pitch 0\n", sfx->name );
		return;
	}

	if( !pos ) pos = cl.refdef.vieworg;

	// pick a channel to play on
	if( chan == CHAN_STATIC ) target_chan = SND_PickStaticChannel( ent, sfx, pos );
	else target_chan = SND_PickDynamicChannel( ent, chan, sfx );

	if( !target_chan )
	{
		MsgDev( D_NOTE, "^1Error: ^7dropped sound \"sound/%s\"\n", sfx->name );
		return;
	}

	// spatialize
	Q_memset( target_chan, 0, sizeof( *target_chan ));

	VectorCopy( pos, target_chan->origin );
	target_chan->staticsound = ( ent == 0 ) ? true : false;
	target_chan->use_loop = (flags & SND_STOP_LOOPING) ? false : true;
	target_chan->localsound = (flags & SND_LOCALSOUND) ? true : false;
	target_chan->dist_mult = (attn / SND_CLIP_DISTANCE);
	target_chan->master_vol = vol;
	target_chan->entnum = ent;
	target_chan->entchannel = chan;
	target_chan->basePitch = pitch;
	target_chan->isSentence = false;
	target_chan->sfx = sfx;

	pSource = NULL;

	if( S_TestSoundChar( sfx->name, '!' ))
	{
		// this is a sentence
		// link all words and load the first word
		// NOTE: sentence names stored in the cache lookup are
		// prepended with a '!'.  Sentence names stored in the
		// sentence file do not have a leading '!'. 
		VOX_LoadSound( target_chan, S_SkipSoundChar( sfx->name ));

		sfx = target_chan->sfx; // TEST
		pSource = sfx->cache;
	}
	else
	{
		// regular or streamed sound fx
		pSource = S_LoadSound( sfx );
	}

	if( !pSource )
	{
		S_FreeChannel( target_chan );
		return;
	}

	SND_Spatialize( target_chan );

	// If a client can't hear a sound when they FIRST receive the StartSound message,
	// the client will never be able to hear that sound. This is so that out of 
	// range sounds don't fill the playback buffer. For streaming sounds, we bypass this optimization.
	if( !target_chan->leftvol && !target_chan->rightvol )
	{
		// looping sounds don't use this optimization because they should stick around until they're killed.
		if( !sfx->cache || sfx->cache->loopStart == -1 )
		{
			// if this is a streaming sound, play the whole thing.
			if( chan != CHAN_STREAM )
			{
				S_FreeChannel( target_chan );
				return; // not audible at all
			}
		}
	}

	// Init client entity mouth movement vars
	SND_InitMouth( ent, chan );

	for( ch_idx = 0, check = channels; ch_idx < MAX_DYNAMIC_CHANNELS; ch_idx++, check++ )
	{
		if( check == target_chan ) continue;

		if( check->sfx == sfx && !check->pMixer.sample )
		{
			// skip up to 0.1 seconds of audio
			int skip = Com_RandomLong( 0, (long)( 0.1f * check->sfx->cache->rate ));
                              
			S_SetSampleStart( check, sfx->cache, skip );
			break;
		}
	}
}

/*
=================
S_AmbientSound

Start playback of a sound, loaded into the static portion of the channel array.
Currently, this should be used for looping ambient sounds, looping sounds
that should not be interrupted until complete, non-creature sentences,
and one-shot ambient streaming sounds.  Can also play 'regular' sounds one-shot,
in case designers want to trigger regular game sounds.
Pitch changes playback pitch of wave by % above or below 100.  Ignored if pitch == 100

NOTE: volume is 0.0 - 1.0 and attenuation is 0.0 - 1.0 when passed in.
=================
*/
void S_AmbientSound( const vec3_t pos, int ent, sound_t handle, float fvol, float attn, int pitch, int flags )
{
	channel_t	*ch;
	wavdata_t	*pSource = NULL;
	sfx_t	*sfx = NULL;
	int	vol, fvox = 0;
	qboolean	looping = false;
	vec3_t	origin;		

	if( !dma.initialized ) return;
	sfx = S_GetSfxByHandle( handle );
	if( !sfx ) return;

	vol = bound( 0, fvol * 255, 255 );

	if( flags & (SND_STOP|SND_CHANGE_VOL|SND_CHANGE_PITCH))
	{
		if( S_AlterChannel( ent, CHAN_STATIC, sfx, vol, pitch, flags ))
			return;
		if( flags & SND_STOP ) return;
	}
	
	if( pitch == 0 )
	{
		MsgDev( D_WARN, "S_StartStaticSound: ( %s ) ignored, called with pitch 0\n", sfx->name );
		return;
	}

	if( !pos ) pos = origin;

	if( ent != 0 ) CL_GetEntitySpatialization( ent, origin, NULL );

	// pick a channel to play on from the static area
	ch = SND_PickStaticChannel( ent, sfx, pos );
	if( !ch ) return;

	if( S_TestSoundChar( sfx->name, '!' ))
	{
		// this is a sentence. link words to play in sequence.
		// NOTE: sentence names stored in the cache lookup are
		// prepended with a '!'.  Sentence names stored in the
		// sentence file do not have a leading '!'. 

		// link all words and load the first word
		VOX_LoadSound( ch, S_SkipSoundChar( sfx->name ));
		sfx = ch->sfx;
		pSource = sfx->cache;
		fvox = 1;
	}
	else
	{
		// load regular or stream sound
		pSource = S_LoadSound( sfx );
		ch->sfx = sfx;
		ch->isSentence = false;
	}

	if( !pSource )
	{
		S_FreeChannel( ch );
		return;
	}

	VectorCopy( pos, ch->origin );

	// never update positions if source entity is 0
	ch->staticsound = ( ent == 0 ) ? true : false;
	ch->use_loop = (flags & SND_STOP_LOOPING) ? false : true;
	ch->localsound = (flags & SND_LOCALSOUND) ? true : false;
	ch->master_vol = vol;
	ch->dist_mult = (attn / SND_CLIP_DISTANCE);
	ch->entchannel = CHAN_STATIC;
	ch->basePitch = pitch;
	ch->entnum = ent;

	SND_Spatialize( ch );
}

/*
==================
S_StartLocalSound
==================
*/
void S_StartLocalSound(  const char *name )
{
	sound_t	sfxHandle;
	int	flags = (SND_LOCALSOUND|SND_STOP_LOOPING);

	if( !dma.initialized ) return;	
	sfxHandle = S_RegisterSound( name );
	S_StartSound( NULL, s_listener.entnum, CHAN_AUTO, sfxHandle, VOL_NORM, ATTN_NONE, PITCH_NORM, flags );
}

/*
==================
S_GetCurrentStaticSounds

grab all static sounds playing at current channel
==================
*/
int S_GetCurrentStaticSounds( soundlist_t *pout, int size )
{
	int	sounds_left = size;
	int	i;

	if( !dma.initialized )
		return 0;

	for( i = MAX_DYNAMIC_CHANNELS; i < total_channels && sounds_left; i++ )
	{
		if( channels[i].entchannel == CHAN_STATIC && channels[i].sfx )
		{
			Q_strncpy( pout->name, channels[i].sfx->name, sizeof( pout->name ));
			pout->entnum = channels[i].entnum;
			VectorCopy( channels[i].origin, pout->origin );
			pout->volume = (float)channels[i].master_vol / 255.0f;
			pout->attenuation = channels[i].dist_mult * SND_CLIP_DISTANCE;
			pout->looping = ( channels[i].use_loop && channels[i].sfx->cache->loopStart != -1 );
			pout->pitch = channels[i].basePitch;

			sounds_left--;
			pout++;
		}
	}

	return ( size - sounds_left );
}

/*
==================
S_ClearBuffer
==================
*/
void S_ClearBuffer( void )
{
	s_rawend = 0;

	SNDDMA_BeginPainting ();
	if( dma.buffer ) Q_memset( dma.buffer, 0, dma.samples * 2 );
	SNDDMA_Submit ();

	MIX_ClearAllPaintBuffers( PAINTBUFFER_SIZE, true );
}

/*
==================
S_StopSound

stop all sounds for entity on a channel.
==================
*/
void S_StopSound( int entnum, int channel, const char *soundname )
{
	sfx_t	*sfx;

	if( !dma.initialized ) return;
	sfx = S_FindName( soundname, NULL );
	S_AlterChannel( entnum, channel, sfx, 0, 0, SND_STOP );
}

/*
==================
S_StopAllSounds
==================
*/
void S_StopAllSounds( void )
{
	int	i;

	if( !dma.initialized ) return;
	total_channels = MAX_DYNAMIC_CHANNELS;	// no statics

	for( i = 0; i < MAX_CHANNELS; i++ ) 
	{
		if( !channels[i].sfx ) continue;
		S_FreeChannel( &channels[i] );
	}

	DSP_ClearState();

	// clear all the channels
	Q_memset( channels, 0, sizeof( channels ));

	S_ClearBuffer ();

	// clear any remaining soundfade
	Q_memset( &soundfade, 0, sizeof( soundfade ));
}

//=============================================================================
void S_UpdateChannels( void )
{
	uint	endtime;
	int	samps;

	SNDDMA_BeginPainting();

	if( !dma.buffer ) return;

	// updates DMA time
	soundtime = SNDDMA_GetSoundtime();

	// soundtime - total samples that have been played out to hardware at dmaspeed
	// paintedtime - total samples that have been mixed at speed
	// endtime - target for samples in mixahead buffer at speed
	endtime = soundtime + s_mixahead->value * SOUND_DMA_SPEED;
	samps = dma.samples >> 1;

	if((int)( endtime - soundtime ) > samps )
		endtime = soundtime + samps;
	
	if(( endtime - paintedtime ) & 0x3 )
	{
		// The difference between endtime and painted time should align on 
		// boundaries of 4 samples. This is important when upsampling from 11khz -> 44khz.
		endtime -= ( endtime - paintedtime ) & 0x3;
	}

	MIX_PaintChannels( endtime );

	SNDDMA_Submit();
}

/*
=================
S_ExtraUpdate

Don't let sound skip if going slow
=================
*/
void S_ExtraUpdate( void )
{
	if( !dma.initialized ) return;
	S_UpdateChannels ();
}

/*
============
S_RenderFrame

Called once each time through the main loop
============
*/
void S_RenderFrame( ref_params_t *fd )
{
	int		i, total;
	con_nprint_t	info;
	channel_t		*ch;

	if( !dma.initialized ) return;
	if( !fd ) return;	// too early

	// if the loading plaque is up, clear everything
	// out to make sure we aren't looping a dirty
	// dma buffer while loading
	// update any client side sound fade
	S_UpdateSoundFade();

	s_listener.entnum = fd->viewentity;	// can be camera entity too
	s_listener.frametime = fd->frametime;
	s_listener.waterlevel = fd->waterlevel;
	s_listener.active = CL_IsInGame();
	s_listener.inmenu = CL_IsInMenu();
	s_listener.paused = fd->paused;

	VectorCopy( fd->vieworg, s_listener.origin );
	VectorCopy( fd->simvel, s_listener.velocity );
	AngleVectors( fd->viewangles, s_listener.forward, s_listener.right, s_listener.up );

	// update spatialization for static and dynamic sounds	
	for( i = 0, ch = channels; i < total_channels; i++, ch++ )
	{
		if( !ch->sfx ) continue;
		SND_Spatialize( ch ); // respatialize channel
	}

	// debugging output
	if( s_show->value )
	{
		info.color[0] = 1.0f;
		info.color[1] = 0.6f;
		info.color[2] = 0.0f;
		info.time_to_live = 0.5f;

		for( i = 0, total = 1, ch = channels; i < MAX_CHANNELS; i++, ch++ )
		{
			if( ch->sfx && ( ch->leftvol || ch->rightvol ))
			{
				info.index = total;
				Con_NXPrintf( &info, "%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name );
				total++;
			}
		}
		Con_NPrintf( 0, "----(%i)---- painted: %i\n", total - 1, paintedtime );
	}

	S_StreamBackgroundTrack ();
	S_StreamSoundTrack ();

	// mix some sound
	S_UpdateChannels ();
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
		Msg( "Usage: playsound <soundfile>\n" );
		return;
	}
	S_StartLocalSound( Cmd_Argv( 1 ));
}

/*
=================
S_Music_f
=================
*/
void S_Music_f( void )
{
	int	c = Cmd_Argc();

	// run background track
	if( c == 1 )
	{
		// blank name stopped last track
		S_StopBackgroundTrack();
	}
	else if( c == 2 )
	{
		string	intro, main, track;
		char	*ext[] = { "wav", "mp3" };
		int	i;

		Q_strncpy( track, Cmd_Argv( 1 ), sizeof( track ));
		Q_snprintf( intro, sizeof( intro ), "%s_intro", Cmd_Argv( 1 ));
		Q_snprintf( main, sizeof( main ), "%s_main", Cmd_Argv( 1 ));

		for( i = 0; i < 2; i++ )
		{
			if( FS_FileExists( va( "media/%s.%s", intro, ext[i] ), false )
				&& FS_FileExists( va( "media/%s.%s", main, ext[i] ), false ))
			{
				// combined track with introduction and main loop theme
				S_StartBackgroundTrack( intro, main );
				break;
			}
			else if( FS_FileExists( va( "media/%s.%s", track, ext[i] ), false ))
			{
				// single looped theme
				S_StartBackgroundTrack( track, NULL );
				break;
			}
		}

	}
	else if( c == 3 )
	{
		S_StartBackgroundTrack( Cmd_Argv( 1 ), Cmd_Argv( 2 ));
	}
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

	Msg( "%5d channel(s)\n", 2 );
	Msg( "%5d samples\n", dma.samples );
	Msg( "%5d bits/sample\n", 16 );
	Msg( "%5d bytes/sec\n", SOUND_DMA_SPEED );
	Msg( "%5d total_channels\n", total_channels );
}

/*
================
S_Init
================
*/
qboolean S_Init( void )
{
	if( Sys_CheckParm( "-nosound" ))
	{
		MsgDev( D_INFO, "Audio: Disabled\n" );
		return false;
	}

	s_volume = Cvar_Get( "volume", "0.7", CVAR_ARCHIVE, "sound volume" );
	s_musicvolume = Cvar_Get( "musicvolume", "1.0", CVAR_ARCHIVE, "background music volume" );
	s_mixahead = Cvar_Get( "_snd_mixahead", "0.1", CVAR_ARCHIVE, "how much sound to mix ahead of time" );
	s_show = Cvar_Get( "s_show", "0", CVAR_ARCHIVE, "show playing sounds" );
	s_check_errors = Cvar_Get( "s_check_errors", "1", CVAR_ARCHIVE, "ignore audio engine errors" );
	s_lerping = Cvar_Get( "s_lerping", "0", CVAR_ARCHIVE, "apply interpolation to sound output" );
	dsp_off = Cvar_Get( "dsp_off", "0", CVAR_ARCHIVE, "set to 1 to disable all dsp processing" );

	Cmd_AddCommand( "play", S_Play_f, "playing a specified sound file" );
	Cmd_AddCommand( "stopsound", S_StopSound_f, "stop all sounds" );
	Cmd_AddCommand( "music", S_Music_f, "starting a background track" );
	Cmd_AddCommand( "soundlist", S_SoundList_f, "display loaded sounds" );
	Cmd_AddCommand( "s_info", S_SoundInfo_f, "print sound system information" );

	if( !SNDDMA_Init( host.hWnd ))
	{
		MsgDev( D_INFO, "S_Init: sound system can't be initialized\n" );
		return false;
	}

	sndpool = Mem_AllocPool( "Sound Zone" );
	soundtime = 0;
	paintedtime = 0;

	MIX_InitAllPaintbuffers ();

	S_InitScaletable ();
	S_StopAllSounds ();
	VOX_Init ();
	AllocDsps ();

	return true;
}

// =======================================================================
// Shutdown sound engine
// =======================================================================
void S_Shutdown( void )
{
	if( !dma.initialized ) return;

	Cmd_RemoveCommand( "play" );
	Cmd_RemoveCommand( "stopsound" );
	Cmd_RemoveCommand( "music" );
	Cmd_RemoveCommand( "soundlist" );
	Cmd_RemoveCommand( "s_info" );

	S_StopAllSounds ();
	S_FreeSounds ();
	VOX_Shutdown ();
	FreeDsps ();

	SNDDMA_Shutdown ();
	MIX_FreeAllPaintbuffers ();
	Mem_FreePool( &sndpool );
}