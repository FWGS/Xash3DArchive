//=======================================================================
//			Copyright XashXT Group 2009 ©
//			s_stream.c - sound streaming
//=======================================================================

#include "sound.h"
#include "byteorder.h"

portable_samplepair_t	s_rawsamples[MAX_RAW_SAMPLES];
int			s_rawend;
static bg_track_t		s_bgTrack;

/*
=================
S_StartBackgroundTrack
=================
*/
void S_StartBackgroundTrack( const char *introTrack, const char *mainTrack )
{
	S_StopBackgroundTrack();

	if(( !introTrack || !*introTrack ) && ( !mainTrack || !*mainTrack ))
		return;

	if( !introTrack )
	{
		introTrack = "";
	}
	if( !mainTrack || !*mainTrack )
	{
		mainTrack = introTrack;
	}
	if( !*introTrack ) return;

	if( mainTrack )
	{
		com.strncpy( s_bgTrack.loopName, mainTrack, sizeof( s_bgTrack.loopName ));
	}
	else s_bgTrack.loopName[0] = 0;

	S_StartStreaming();

	// close the background track, but DON'T reset s_rawend
	// if restarting the same back ground track
	if( s_bgTrack.stream )
	{
		FS_CloseStream( s_bgTrack.stream );
		s_bgTrack.stream = NULL;
	}

	// open stream
	s_bgTrack.stream = FS_OpenStream( va( "media/%s", introTrack ));
}

void S_StopBackgroundTrack( void )
{
	S_StopStreaming();

	if( !s_bgTrack.stream ) return;

	FS_CloseStream( s_bgTrack.stream );
	Mem_Set( &s_bgTrack, 0, sizeof( bg_track_t ));
	s_rawend = 0;
}

/*
=================
S_StreamBackgroundTrack
=================
*/
void S_StreamBackgroundTrack( void )
{
	int	bufferSamples;
	int	fileSamples;
	byte	raw[MAX_RAW_SAMPLES];
	int	r, fileBytes;

	if( !s_bgTrack.stream ) return;

	// don't bother playing anything if musicvolume is 0
	if( !s_musicvolume->value ) return;

	// see how many samples should be copied into the raw buffer
	if( s_rawend < soundtime )
		s_rawend = soundtime;

	while( s_rawend < soundtime + MAX_RAW_SAMPLES )
	{
		wavdata_t	*info = FS_StreamInfo( s_bgTrack.stream );

		bufferSamples = MAX_RAW_SAMPLES - (s_rawend - soundtime);

		// decide how much data needs to be read from the file
		fileSamples = bufferSamples * info->rate / dma.speed;

		// our max buffer size
		fileBytes = fileSamples * ( info->width * info->channels );

		if( fileBytes > sizeof( raw ))
		{
			fileBytes = sizeof( raw );
			fileSamples = fileBytes / ( info->width * info->channels );
		}

		// read
		r = FS_ReadStream( s_bgTrack.stream, fileBytes, raw );

		if( r < fileBytes )
		{
			fileBytes = r;
			fileSamples = r / ( info->width * info->channels );
		}

		if( r > 0 )
		{
			// add to raw buffer
			S_StreamRawSamples( fileSamples, info->rate, info->width, info->channels, raw );
		}
		else
		{
			// loop
			if( s_bgTrack.loopName[0] )
			{
				FS_CloseStream( s_bgTrack.stream );
				s_bgTrack.stream = NULL;
				S_StartBackgroundTrack( s_bgTrack.loopName, NULL );
				if( !s_bgTrack.stream ) return;
			}
			else
			{
				S_StopBackgroundTrack();
				return;
			}
		}

	}
}

/*
=================
S_StartStreaming
=================
*/
void S_StartStreaming( void )
{
}

/*
=================
S_StopStreaming
=================
*/
void S_StopStreaming( void )
{
	// clear s_rawend here ?
}

/*
============
S_StreamRawSamples

Cinematic streaming and voice over network
============
*/
void S_StreamRawSamples( int samples, int rate, int width, int channels, const byte *data )
{
	int	i, snd_vol;
	int	a, b, src, dst;
	int	fracstep, samplefrac;
	int	incount, outcount;

	snd_vol = (int)(s_musicvolume->value * 256);
	if( snd_vol < 0 ) snd_vol = 0;

	src = 0;
	samplefrac = 0;
	fracstep = (((double)rate) / (double)dma.speed) * 256.0;
	outcount = (double)samples * (double) dma.speed / (double)rate;
	incount = samples * channels;

#define TAKE_SAMPLE( s )	(sizeof(*in) == 1 ? (a = (in[src+(s)]-128)<<8,\
			b = (src < incount - channels) ? (in[src+channels+(s)]-128)<<8 : 128) : \
			(a = in[src+(s)],\
			b = (src < incount - channels) ? (in[src+channels+(s)]) : 0))

#define LERP_SAMPLE		((((((b - a) * (samplefrac & 255)) >> 8) + a) * snd_vol))

#define RESAMPLE_RAW \
	if( channels == 2 ) { \
		for( i = 0; i < outcount; i++, samplefrac += fracstep, src = (samplefrac >> 8) << 1 ) { \
			dst = s_rawend++ & (MAX_RAW_SAMPLES - 1); \
			TAKE_SAMPLE(0); \
			s_rawsamples[dst].left = LERP_SAMPLE; \
			TAKE_SAMPLE(1); \
			s_rawsamples[dst].right = LERP_SAMPLE; \
		} \
	} else { \
		for( i = 0; i < outcount; i++, samplefrac += fracstep, src = (samplefrac >> 8) << 0 ) { \
			dst = s_rawend++ & (MAX_RAW_SAMPLES - 1); \
			TAKE_SAMPLE(0); \
			s_rawsamples[dst].left = LERP_SAMPLE; \
			s_rawsamples[dst].right = s_rawsamples[dst].left; \
		} \
	}
		
	if( s_rawend < paintedtime )
		s_rawend = paintedtime;

	if( width == 2 )
	{
		short *in = (short *)data;
		RESAMPLE_RAW
	}
	else
	{
		byte *in = (unsigned char *)data;
		RESAMPLE_RAW
	}
}