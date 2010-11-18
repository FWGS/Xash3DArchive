//=======================================================================
//			Copyright XashXT Group 2010 ©
//			snd_mp3.c - mp3 format load & save
//=======================================================================

#include "soundlib.h"

/*
=======================================================================
		LIBMAD DEFINITION
=======================================================================
*/
#define BUFFER_GUARD		8
#define BUFFER_MDLEN		(511 + 2048 + BUFFER_GUARD)
#define BUFFER_SIZE			4096	// must be large than BUFFER_MDLEN

#define MPEG_F_FRACBITS		28
#define MPEG_F( x )			((int)( x##L ))
#define MPEG_F_ONE			MPEG_F( 0x10000000 )

enum
{
	MP3_ERROR_NONE		= 0x0000,	// no error
	MP3_ERROR_BUFLEN		= 0x0001,	// input buffer too small (or EOF)
	MP3_ERROR_BUFPTR		= 0x0002,	// invalid (null) buffer pointer
	MP3_ERROR_NOMEM		= 0x0031,	// not enough memory
	MP3_ERROR_LOSTSYNC		= 0x0101,	// lost synchronization
	MP3_ERROR_BADLAYER		= 0x0102,	// reserved header layer value
	MP3_ERROR_BADBITRATE	= 0x0103,	// forbidden bitrate value
	MP3_ERROR_BADSAMPLERATE	= 0x0104,	// reserved sample frequency value
	MP3_ERROR_BADEMPHASIS	= 0x0105,	// reserved emphasis value
	MP3_ERROR_BADCRC		= 0x0201,	// CRC check failed
	MP3_ERROR_BADBITALLOC	= 0x0211,	// forbidden bit allocation value
	MP3_ERROR_BADSCALEFACTOR	= 0x0221,	// bad scalefactor index
	MP3_ERROR_BADMODE		= 0x0222,	// bad bitrate/mode combination
	MP3_ERROR_BADFRAMELEN	= 0x0231,	// bad frame length
	MP3_ERROR_BADBIGVALUES	= 0x0232,	// bad big_values count
	MP3_ERROR_BADBLOCKTYPE	= 0x0233,	// reserved block_type
	MP3_ERROR_BADSCFSI		= 0x0234,	// bad scalefactor selection info
	MP3_ERROR_BADDATAPTR	= 0x0235,	// bad main_data_begin pointer
	MP3_ERROR_BADPART3LEN	= 0x0236,	// bad audio data length
	MP3_ERROR_BADHUFFTABLE	= 0x0237,	// bad Huffman table select
	MP3_ERROR_BADHUFFDATA	= 0x0238,	// Huffman data overrun
	MP3_ERROR_BADSTEREO		= 0x0239	// incompatible block_type for JS
};

enum
{
	MODE_SINGLE_CHANNEL = 0,	// single channel
	MODE_DUAL_CHANNEL,		// dual channel
	MODE_JOINT_STEREO,		// joint (MS/intensity) stereo
	MODE_STEREO		// normal LR stereo
};

typedef struct
{
	uint	samplerate;		// sampling frequency (Hz)
	word	channels;			// number of channels
	word	length;			// number of samples per channel
	int	samples[2][1152];		// PCM output samples [ch][sample]
} pcm_t;

typedef struct
{
	int	filter[2][2][2][16][8];	// polyphase filterbank outputs
	uint	phase;			// current processing phase
	pcm_t	pcm;			// PCM output
} synth_t;

typedef struct
{
	const byte	*byte;
	word		cache;
	word		left;
} bitptr_t;

typedef struct
{
	long		seconds;			// whole seconds
	dword		fraction;			// 1 / TIMER_RESOLUTION seconds
} mp3_timer_t;

typedef struct
{
	int		layer;			// audio layer (1, 2, or 3)
	int		mode;			// channel mode (see above)
	int		mode_extension;		// additional mode info
	int		emphasis;			// de-emphasis to use (see above)

	dword		bitrate;			// stream bitrate (bps)
	uint		samplerate;		// sampling frequency (Hz)

	word		crc_check;		// frame CRC accumulator
	word		crc_target;		// final target CRC checksum

	int		flags;			// flags (see below)
	int		private_bits;		// private bits (see below)
	mp3_timer_t	duration;			// audio playing time of frame
} mp3_header_t;

typedef struct
{
	mp3_header_t	header;			// MPEG audio header
	int		options;			// decoding options (from stream)

	int		sbsample[2][36][32];	// synthesis subband filter samples
	int		(*overlap)[2][32][18];	// Layer III block overlap data
} mp3_frame_t;

typedef struct
{
	const byte	*buffer;		// input bitstream buffer
	const byte	*bufend;		// end of buffer
	dword		skiplen;		// bytes to skip before next frame

	int		sync;		// stream sync found
	dword		freerate;		// free bitrate (fixed)

	const byte	*this_frame;	// start of current frame
	const byte	*next_frame;	// start of next frame
	bitptr_t		ptr;		// current processing bit pointer

	bitptr_t		anc_ptr;		// ancillary bits pointer
	uint		anc_bitlen;	// number of ancillary bits

	byte		(*data)[BUFFER_MDLEN];
					// Layer III data()
	uint		md_len;		// bytes in data

	int		options;		// decoding options
	int		error;		// error code
} mp3_stream_t;

typedef struct
{
	synth_t		synth;
	mp3_stream_t	stream;
	mp3_frame_t	frame;
	int		buffer_length;	// for reading
	byte		buffer[BUFFER_SIZE];// frame buffer
} mpegfile_t;

// libmad exports
extern void mad_synth_init( synth_t* );
extern void mad_synth_frame( synth_t*, const mp3_frame_t* );
extern void mad_stream_init( mp3_stream_t* );
extern void mad_stream_buffer( mp3_stream_t*, const byte*, dword );
extern void mad_stream_finish( mp3_stream_t* );
extern void mad_frame_init( mp3_frame_t* );
extern int mad_frame_decode( mp3_frame_t*, mp3_stream_t* );
extern void mad_frame_finish( mp3_frame_t* );


/*
=================================================================

	MPEG decompression

=================================================================
*/
static int mpeg_read( file_t *file, mpegfile_t *mpeg )
{
	int	ret;

	while( 1 )
	{
		ret = FS_Read( file, &mpeg->buffer[mpeg->buffer_length], BUFFER_SIZE - mpeg->buffer_length );

		// no more bytes are left
		if( ret <= 0 ) break;

		mpeg->buffer_length += ret;

		while( 1 )
		{
			mad_stream_buffer( &mpeg->stream, mpeg->buffer, mpeg->buffer_length );
			ret = mad_frame_decode( &mpeg->frame, &mpeg->stream );

			if( mpeg->stream.next_frame )
			{
				int	length;

				length = mpeg->buffer + mpeg->buffer_length - mpeg->stream.next_frame;
				memmove( mpeg->buffer, mpeg->stream.next_frame, length );
				mpeg->buffer_length = length;
			}

			if( !ret ) return 1;
			if( mpeg->stream.error == MP3_ERROR_BUFLEN )
				break;
		}
	}
	return 0;
}

static int mpeg_scale( int sample )
{
	sample += (1 << ( MPEG_F_FRACBITS - 16 ));

	if( sample >= MPEG_F_ONE ) sample = MPEG_F_ONE - 1;
	else if( sample < -MPEG_F_ONE ) sample = -MPEG_F_ONE;

	return sample >> ( MPEG_F_FRACBITS + 1 - 16 );
}

static int mpeg_size( mp3_frame_t *frame, long bytes )
{
	return bytes * 8 / frame->header.bitrate * sound.channels * sound.rate * 2;
}


/*
=================================================================

	MPEG decompression

=================================================================
*/

qboolean Sound_LoadMPG( const char *name, const byte *buffer, size_t filesize )
{
	synth_t		synth;
	mp3_stream_t	stream;
	mp3_frame_t	frame;
	byte		buf[BUFFER_SIZE];
	size_t		i, ret, pos = 0;
	size_t		bufsize = 0;
	size_t		bytesWrite = 0;
	word		*data;

	// load the file
	if( !buffer || filesize <= 0 )
		return false;

	mad_synth_init( &synth );
	mad_stream_init( &stream );
	mad_frame_init( &frame );

	sound.channels = ( frame.header.mode == MODE_SINGLE_CHANNEL ) ? 1 : 2;
	sound.rate = frame.header.samplerate;
	sound.width = 2; // always 16-bit PCM
	sound.loopstart = -1;
	sound.size = mpeg_size( &frame, filesize );

	if( !sound.size )
	{
		// bad ogg file
		MsgDev( D_ERROR, "Sound_LoadMPG: (%s) is probably corrupted\n", name );
		mad_stream_finish( &stream );
		mad_frame_finish( &frame );
		return false;
	}

	sound.type = WF_PCMDATA;
	sound.wav = (byte *)Mem_Alloc( Sys.soundpool, sound.size );

	// decompress mpg into pcm wav format
	while( 1 )
	{
		Mem_Copy( &buf[bufsize], buffer + pos, BUFFER_SIZE - bufsize );
		pos += ( BUFFER_SIZE - bufsize );
		if( pos >= filesize ) break;	// end

		bufsize += ( BUFFER_SIZE - bufsize );

		while( 1 )
		{
			mad_stream_buffer( &stream, buf, bufsize );
			ret = mad_frame_decode( &frame, &stream );

			if( stream.next_frame )
			{
				int	length;

				length = buf + bufsize - stream.next_frame;
				memmove( buf, stream.next_frame, length );
				bufsize = length;
			}

			if( !ret ) break;
			if( stream.error == MP3_ERROR_BUFLEN )
			{
				break;
			}
		}

		mad_synth_frame( &synth, &frame );
		data = (word *)(sound.wav + bytesWrite);

		for( i = 0; i < synth.pcm.length; i++ )
		{
			if( sound.channels == 2 )
			{
				*data++ = mpeg_scale( synth.pcm.samples[0][i] );
				*data++ = mpeg_scale( synth.pcm.samples[1][i] );
			}
			else
			{
				*data++ = mpeg_scale( synth.pcm.samples[0][i] );
			}
			bytesWrite += ( sound.width * sound.channels );

			if( bytesWrite >= sound.size )
			{
				ASSERT( 0 );
				break;
			}
		}
	}

	return false;
}

/*
=================
Stream_OpenMPG
=================
*/
stream_t *Stream_OpenMPG( const char *filename )
{
	mpegfile_t	*mpegFile;
	stream_t		*stream;
	file_t		*file;

	file = FS_Open( filename, "rb", false );
	if( !file ) return NULL;

	// at this point we have valid stream
	stream = Mem_Alloc( Sys.soundpool, sizeof( stream_t ));
	stream->file = file;

	mpegFile = Mem_Alloc( Sys.soundpool, sizeof( mpegfile_t ));

	mad_synth_init( &mpegFile->synth );
	mad_stream_init( &mpegFile->stream );
	mad_frame_init( &mpegFile->frame );

	if( mpeg_read( file, mpegFile ) == 0 )
	{
		MsgDev( D_ERROR, "Stream_OpenMPG: couldn't open %s\n", filename );
		mad_stream_finish( &mpegFile->stream );
		mad_frame_finish( &mpegFile->frame );
		Mem_Free( mpegFile );
		Mem_Free( stream );
		FS_Close( file );
		return NULL;
	}

	stream->pos = 0;	// how many samples left from previous frame
	stream->channels = ( mpegFile->frame.header.mode == MODE_SINGLE_CHANNEL ) ? 1 : 2;
	stream->rate = mpegFile->frame.header.samplerate;
	stream->width = 2;	// always 16 bit
	stream->ptr = mpegFile;
	stream->type = WF_MPGDATA;

	return stream;
}

/*
=================
Stream_ReadMPG

assume stream is valid
=================
*/
long Stream_ReadMPG( stream_t *stream, long bytes, void *buffer )
{
	// buffer handling
	int		bytesRead = 0;
	mpegfile_t	*mpg;

	mpg = (mpegfile_t *)stream->ptr;
	ASSERT( mpg != NULL );

	while( 1 )
	{
		pcm_t	*wav;
		word	*data;
		int	i;

		if( !stream->pos )
		{
			// if there are no bytes remainig so we can synth new frame
			mad_synth_frame( &mpg->synth, &mpg->frame );
		}
		wav = &mpg->synth.pcm;
		data = (word *)((byte *)buffer + bytesRead);

		for( i = stream->pos; i < wav->length; i++ )
		{
			if( stream->channels == 2 )
			{
				*data++ = mpeg_scale( wav->samples[0][i] );
				*data++ = mpeg_scale( wav->samples[1][i] );
			}
			else
			{
				*data++ = mpeg_scale( wav->samples[0][i] );
			}
			bytesRead += ( stream->width * stream->channels );

			if( bytesRead >= bytes )
			{
				// continue from this sample on a next call
				stream->pos = i;
				return bytesRead;
			}
		}

		stream->pos = 0;	// no bytes remainig
		if( !mpeg_read( stream->file, mpg )) break;
	}
	return 0;
}

/*
=================
Stream_FreeMPG

assume stream is valid
=================
*/
void Stream_FreeMPG( stream_t *stream )
{
	if( stream->ptr )
	{
		mpegfile_t	*mpg;

		mpg = (mpegfile_t *)stream->ptr;

		mad_stream_finish( &mpg->stream );
		mad_frame_finish( &mpg->frame );
		Mem_Free( stream->ptr );
	}

	if( stream->file )
	{
		FS_Close( stream->file );
	}

	Mem_Free( stream );
}