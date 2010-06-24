//=======================================================================
//			Copyright XashXT Group 2009 ©
//			s_stream.c - sound streaming
//=======================================================================

#include "sound.h"
#include "byteorder.h"

portable_samplepair_t	rawsamples[MAX_RAW_SAMPLES];
static bg_track_t		bgTrack;
int			rawend;

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
		introTrack = "";

	if( !mainTrack || !*mainTrack )
		mainTrack = introTrack;

	if( !*introTrack ) return;

	if( mainTrack )
		com.snprintf( bgTrack.loopName, sizeof( bgTrack.loopName ), "media/%s", mainTrack );
	else bgTrack.loopName[0] = 0;

	S_StartStreaming();

	// close the background track, but DON'T reset rawend
	// if restarting the same back ground track
	if( bgTrack.stream )
	{
		FS_CloseStream( bgTrack.stream );
		bgTrack.stream = NULL;
	}

	// open stream
	bgTrack.stream = FS_OpenStream( va( "media/%s", introTrack ));
}

void S_StopBackgroundTrack( void )
{
	S_StopStreaming();

	if( !bgTrack.stream ) return;

	FS_CloseStream( bgTrack.stream );
	Mem_Set( &bgTrack, 0, sizeof( bg_track_t ));
	rawend = 0;
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
	float	musicVolume = 0.5f;
	int	r, fileBytes;

	if( !bgTrack.stream ) return;

	// graeme see if this is OK
	musicVolume = ( musicVolume + ( musicvolume->value * 2 )) / 4.0f;

	// don't bother playing anything if musicvolume is 0
	if( musicVolume <= 0 ) return;

	// see how many samples should be copied into the raw buffer
	if( rawend < soundtime ) rawend = soundtime;

	while( rawend < soundtime + MAX_RAW_SAMPLES )
	{
		wavdata_t	*info = FS_StreamInfo( bgTrack.stream );

		bufferSamples = MAX_RAW_SAMPLES - ( rawend - soundtime );

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
		r = FS_ReadStream( bgTrack.stream, fileBytes, raw );

		if( r < fileBytes )
		{
			fileBytes = r;
			fileSamples = r / ( info->width * info->channels );
		}

		if( r > 0 )
		{
			// add to raw buffer
			// FIXME: apply musicVolume
			S_StreamRawSamples( fileSamples, info->rate, info->width, info->channels, raw );
		}
		else
		{
			// loop
			if( bgTrack.loopName[0] )
			{
				FS_CloseStream( bgTrack.stream );
				bgTrack.stream = NULL;
				S_StartBackgroundTrack( bgTrack.loopName, bgTrack.loopName );
				if( !bgTrack.stream ) return;
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
	// clear rawend here ?
}

/*
============
S_StreamRawSamples

Cinematic streaming and voice over network
============
*/
void S_StreamRawSamples( int samples, int rate, int width, int channels, const byte *data )
{
	int	i, src, dst;
	float	scale;

	if( rawend < paintedtime )
		rawend = paintedtime;
	scale = (float)rate / dma.speed;

	if( channels == 2 && width == 2 )
	{
		if( scale == 1.0f )
		{	
			// optimized case
			for( i = 0; i < samples; i++ )
			{
				dst = rawend & (MAX_RAW_SAMPLES - 1);
				rawend++;
				rawsamples[dst].left = LittleShort(((short *)data)[i*2]) << 8;
				rawsamples[dst].right = LittleShort(((short *)data)[i*2+1]) << 8;
			}
		}
		else
		{
			for( i = src = 0; src < samples; i++ )
			{
				src = i * scale;
				if( src >= samples ) break;

				dst = rawend & (MAX_RAW_SAMPLES - 1);
				rawend++;
				rawsamples[dst].left = LittleShort(((short *)data)[src*2]) << 8;
				rawsamples[dst].right = LittleShort(((short *)data)[src*2+1]) << 8;
			}
		}
	}
	else if( channels == 1 && width == 2 )
	{
		for( i = src = 0; src < samples; i++ )
		{
			src = i * scale;
			if( src >= samples ) break;

			dst = rawend & (MAX_RAW_SAMPLES - 1);
			rawend++;
			rawsamples[dst].left = LittleShort(((short *)data)[src]) << 8;
			rawsamples[dst].right = LittleShort(((short *)data)[src]) << 8;
		}
	}
	else if( channels == 2 && width == 1 )
	{
		for( i = src = 0; src < samples; i++ )
		{
			src = i * scale;
			if( src >= samples ) break;

			dst = rawend & (MAX_RAW_SAMPLES - 1);
			rawend++;
			rawsamples[dst].left = ((char *)data)[src*2] << 16;
			rawsamples[dst].right = ((char *)data)[src*2+1] << 16;
		}
	}
	else if( channels == 1 && width == 1 )
	{
		for( i = src = 0; src < samples; i++ )
		{
			src = i * scale;
			if( src >= samples ) break;

			dst = rawend & (MAX_RAW_SAMPLES - 1);
			rawend++;
			rawsamples[dst].left = (((byte *)data)[src]-128) << 16;
			rawsamples[dst].right = (((byte *)data)[src]-128) << 16;
		}
	}
}