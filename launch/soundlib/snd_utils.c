//=======================================================================
//			Copyright XashXT Group 2010 ©
//			snd_utils.c - sound common tools
//=======================================================================

#include "soundlib.h"
#include "mathlib.h"

/*
=============================================================================

	XASH3D LOAD SOUND FORMATS

=============================================================================
*/
// stub
static const loadwavformat_t load_null[] =
{
{ NULL, NULL, NULL }
};

// version0 - using only ogg sounds
static const loadwavformat_t load_stalker[] =
{
{ "sound/%s%s.%s", "ogg", Sound_LoadOGG },
{ "%s%s.%s", "ogg", Sound_LoadOGG },
{ NULL, NULL, NULL }
};

// version1 - using only Doom1 sounds
static const loadwavformat_t load_doom1[] =
{
{ "%s%s.%s", "snd", Sound_LoadSND },
{ NULL, NULL, NULL }
};

// version2 - using only Quake1 sounds
static const loadwavformat_t load_quake1[] =
{
{ "sound/%s%s.%s", "wav", Sound_LoadWAV },
{ "%s%s.%s", "wav", Sound_LoadWAV },
{ NULL, NULL, NULL }
};

// version3 - Xash3D default sound profile
static const loadwavformat_t load_xash[] =
{
{ "sound/%s%s.%s", "wav", Sound_LoadWAV },
{ "%s%s.%s", "wav", Sound_LoadWAV },
{ "sound/%s%s.%s", "ogg", Sound_LoadOGG },
{ "%s%s.%s", "ogg", Sound_LoadOGG },
{ NULL, NULL, NULL }
};

/*
=============================================================================

	XASH3D PROCESS STREAM FORMATS

=============================================================================
*/
// stub
static const streamformat_t stream_null[] =
{
{ NULL, NULL, NULL, NULL, NULL }
};

// version0 - using only ogg streams
static const streamformat_t stream_stalker[] =
{
{ "%s%s.%s", "ogg", Stream_OpenOGG, Stream_ReadOGG, Stream_FreeOGG },
{ NULL, NULL, NULL, NULL, NULL }
};

// version1 - using only wav streams
static const streamformat_t stream_quake3[] =
{
{ "%s%s.%s", "wav", Stream_OpenWAV, Stream_ReadWAV, Stream_FreeWAV },
{ NULL, NULL, NULL, NULL, NULL }
};

// version3 - Xash3D default stream profile
static const streamformat_t stream_xash[] =
{
{ "%s%s.%s", "ogg", Stream_OpenOGG, Stream_ReadOGG, Stream_FreeOGG },
{ "%s%s.%s", "wav", Stream_OpenWAV, Stream_ReadWAV, Stream_FreeWAV },
{ NULL, NULL, NULL, NULL, NULL }
};

/*
=============================================================================

	XASH3D SAVE SOUND FORMATS

=============================================================================
*/
// stub
static const savewavformat_t save_null[] =
{
{ NULL, NULL, NULL }
};

// version1 - extract all sounds into pcm wav
static const savewavformat_t save_extragen[] =
{
{ "%s%s.%s", "wav", Sound_SaveWAV },
{ NULL, NULL, NULL }
};

void Sound_Init( void )
{
	// init pools
	Sys.soundpool = Mem_AllocPool( "SoundLib Pool" );

	sound.baseformats = load_xash;

	// install image formats (can be re-install later by Sound_Setup)
	switch( Sys.app_name )
	{
	case HOST_NORMAL:
		Sound_Setup( "default", 0 ); // re-initialized later		
		sound.saveformats = save_null;
		break;
	case HOST_RIPPER:
		sound.loadformats = load_null;
		sound.streamformat = stream_null;
		sound.saveformats = save_extragen;
		break;
	default:	// all other instances not using soundlib or will be reinstalling later
		sound.loadformats = load_null;
		sound.streamformat = stream_null;
		sound.saveformats = save_null;
		break;
	}
	sound.tempbuffer = NULL;
}

void Sound_Setup( const char *formats, const uint flags )
{
	if( flags != -1 ) sound.cmd_flags = flags;
	if( formats == NULL ) return;	// used for change flags only

	MsgDev( D_NOTE, "Sound_Init( %s )\n", formats );

	// reinstall loadformats by magic keyword :)
	if( !com.stricmp( formats, "Xash3D" ) || !com.stricmp( formats, "Xash" ))
	{
		sound.loadformats = load_xash;
		sound.streamformat = stream_xash;
	}
	else if( !com.stricmp( formats, "stalker" ) || !com.stricmp( formats, "S.T.A.L.K.E.R" ))
	{
		sound.loadformats = load_stalker;
		sound.streamformat = stream_stalker;
	}
	else if( !com.stricmp( formats, "Doom1" ) || !com.stricmp( formats, "Doom2" ))
	{
		sound.loadformats = load_doom1;
	}
	else if( !com.stricmp( formats, "Quake1" ))
	{
		sound.loadformats = load_quake1; 
	}
	else if( !com.stricmp( formats, "Quake2" ))
	{
		sound.loadformats = load_quake1;
	}
	else if( !com.stricmp( formats, "Quake3" ))
	{
		sound.loadformats = load_quake1;
		sound.streamformat = stream_quake3;
	}
	else if( !com.stricmp( formats, "Quake4" ) || !com.stricmp( formats, "Doom3" ))
	{
		sound.loadformats = load_quake1;
	}
	else if( !com.stricmp( formats, "hl1" ) || !com.stricmp( formats, "Half-Life" ))
	{
		sound.loadformats = load_quake1;
		sound.streamformat = stream_xash;
	}
	else if( !com.stricmp( formats, "hl2" ) || !com.stricmp( formats, "Half-Life 2" ))
	{
		sound.loadformats = load_quake1;
	}
	else
	{
		sound.loadformats = load_xash; // unrecognized version, use default
		sound.streamformat = stream_xash;
	}

	if( Sys.app_name == HOST_RIPPER )
		sound.baseformats = sound.loadformats;
}

void Sound_Shutdown( void )
{
	Mem_Check(); // check for leaks
	Mem_FreePool( &Sys.soundpool );
}

byte *Sound_Copy( size_t size )
{
	byte	*out;

	out = Mem_Alloc( Sys.soundpool, size );
	Mem_Copy( out, sound.tempbuffer, size );
	return out; 
}

/*
=================
Sound_ByteSwapRawSamples
=================
*/
void Sound_ByteSwapRawSamples( int samples, int width, int s_channels, const byte *data )
{
	int	i;

	if( !big_endian ) return;
	if( width != 2 ) return;

	if( s_channels == 2 )
		samples <<= 1;

	for( i = 0; i < samples; i++ )
		((short *)data)[i] = LittleShort((( short *)data)[i] );
}

/*
================
Sound_ConvertToSigned

Convert unsigned data to signed
================
*/
void Sound_ConvertToSigned( const byte *data, int channels, int samples )
{
	int	i;

	if( channels == 2 )
	{
		for( i = 0; i < samples; i++ )
		{
			((signed char *)sound.tempbuffer)[i*2+0] = (int)((byte)(data[i*2+0]) - 128);
			((signed char *)sound.tempbuffer)[i*2+1] = (int)((byte)(data[i*2+1]) - 128);
		}
	}
	else
	{
		for( i = 0; i < samples; i++ )
			((signed char *)sound.tempbuffer)[i] = (int)((unsigned char)(data[i]) - 128);
	}
}

/*
================
Sound_ResampleInternal

We need convert sound to signed even if nothing to resample
================
*/
qboolean Sound_ResampleInternal( wavdata_t *sc, int inrate, int inwidth, int outrate, int outwidth )
{
	float	stepscale;
	int	outcount, srcsample;
	int	i, sample, sample2, samplefrac, fracstep;
	byte	*data;

	data = sc->buffer;
	stepscale = (float)inrate / outrate;	// this is usually 0.5, 1, or 2
	outcount = sc->samples / stepscale;
	sc->size = outcount * outwidth * sc->channels;

	sound.tempbuffer = (byte *)Mem_Realloc( Sys.soundpool, sound.tempbuffer, sc->size );

	sc->samples = outcount;
	if( sc->loopStart != -1 )
		sc->loopStart = sc->loopStart / stepscale;

	// resample / decimate to the current source rate
	if( stepscale == 1.0f && inwidth == 1 && outwidth == 1 )
	{
		Sound_ConvertToSigned( data, sc->channels, outcount );
	}
	else
	{
		// general case
		samplefrac = 0;
		fracstep = stepscale * 256;

		if( sc->channels == 2 )
		{
			for( i = 0; i < outcount; i++ )
			{
				srcsample = samplefrac >> 8;
				samplefrac += fracstep;

				if( inwidth == 2 )
				{
					sample = LittleShort(((short *)data)[srcsample*2+0] );
					sample2 = LittleShort(((short *)data)[srcsample*2+1] );
				}
				else
				{
					sample = (int)((char)(data[srcsample*2+0])) << 8;
					sample2 = (int)((char)(data[srcsample*2+1])) << 8;
				}

				if( outwidth == 2 )
				{
					((short *)sound.tempbuffer)[i*2+0] = sample;
					((short *)sound.tempbuffer)[i*2+1] = sample2;
				}
				else
				{
					((signed char *)sound.tempbuffer)[i*2+0] = sample >> 8;
					((signed char *)sound.tempbuffer)[i*2+1] = sample2 >> 8;
				}
			}
		}
		else
		{
			for( i = 0; i < outcount; i++ )
			{
				srcsample = samplefrac >> 8;
				samplefrac += fracstep;

				if( inwidth == 2 ) sample = LittleShort(((short *)data)[srcsample] );
				else sample = (int)( (char)(data[srcsample])) << 8;

				if( outwidth == 2 ) ((short *)sound.tempbuffer)[i] = sample;
				else ((signed char *)sound.tempbuffer)[i] = sample >> 8;
			}
		}

		MsgDev( D_NOTE, "Sound_Resample: from[%d bit %d kHz] to [%d bit %d kHz]\n",
			inwidth * 8, inrate, outwidth * 8, outrate );
	}

	sc->rate = outrate;
	sc->width = outwidth;

	return true;
}

qboolean Sound_Process( wavdata_t **wav, int rate, int width, uint flags )
{
	wavdata_t	*snd = *wav;
	qboolean	result = true;
				
	// check for buffers
	if( !snd || !snd->buffer )
	{
		MsgDev( D_WARN, "Sound_Process: NULL sound\n" );
		return false;
	}

	if( flags & SOUND_RESAMPLE && ( width > 0 || rate > 0 ))
	{
		if( Sound_ResampleInternal( snd, snd->rate, snd->width, rate, width ))
		{
			Mem_Free( snd->buffer );		// free original image buffer
			snd->buffer = Sound_Copy( snd->size );	// unzone buffer (don't touch image.tempbuffer)
		}
		else result = false; // not a resampled
	}
	*wav = snd;

	return false;
}