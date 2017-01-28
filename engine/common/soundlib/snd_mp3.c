/*
snd_mp3.c - mp3 format loading and streaming
Copyright (C) 2010 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "soundlib.h"

/*
=======================================================================
		MPG123 DEFINITION
=======================================================================
*/
#define MP3_ERR		-1
#define MP3_OK		0
#define MP3_NEED_MORE	1

typedef struct mpeg_s
{
	void	*state;		// hidden decoder state
	void	*file;

	// custom stdio
	long	(*fread)( void *handle, void *buf, size_t count );
	long	(*fseek)( void *handle, long offset, int whence );
	void	(*close)( void *handle );

	// user info
	int	channels;		// num channels
	int	samples;		// per one second
	int	play_time;	// stream size in milliseconds
	int	rate;		// frequency
	int	outsize;		// current data size
	char	out[8192];	// temporary buffer
	size_t	streamsize;	// size in bytes
	char	error[256];	// error buffer
} mpeg_t;


// mpg123 exports
extern int create_decoder( mpeg_t *mpg );
extern int feed_mpeg_header( mpeg_t *mpg, const char *data, long bufsize, long streamsize );
extern int feed_mpeg_stream( mpeg_t *mpg, const char *data, long bufsize );
extern int open_mpeg_stream( mpeg_t *mpg, void *file );
extern int read_mpeg_stream( mpeg_t *mpg );
extern int get_stream_pos( mpeg_t *mpg );
extern int set_stream_pos( mpeg_t *mpg, int curpos );
extern void close_decoder( mpeg_t *mpg );

/*
=================================================================

	MPEG decompression

=================================================================
*/
qboolean Sound_LoadMPG( const char *name, const byte *buffer, size_t filesize )
{
	mpeg_t	mpeg;
	size_t	pos = 0;
	size_t	bytesWrite = 0;

	// load the file
	if( !buffer || filesize < FRAME_SIZE )
		return false;

	// couldn't create decoder
	if( !create_decoder( &mpeg ))
	{
		MsgDev( D_ERROR, "%s\n", mpeg.error );
		return false;
	}

	// trying to read header
	if( !feed_mpeg_header( &mpeg, buffer, FRAME_SIZE, filesize ))
	{
		MsgDev( D_ERROR, "Sound_LoadMPG: (%s) is probably corrupted (%s)\n", name, mpeg.error );
		close_decoder( &mpeg );
		return false;
	}

	sound.channels = mpeg.channels;
	sound.rate = mpeg.rate;
	sound.width = 2; // always 16-bit PCM
	sound.loopstart = -1;
	sound.size = ( sound.channels * sound.rate * sound.width ) * ( mpeg.play_time / 1000 ); // in bytes
	pos += FRAME_SIZE;	// evaluate pos

	if( !sound.size )
	{
		// bad mpeg file ?
		MsgDev( D_ERROR, "Sound_LoadMPG: (%s) is probably corrupted\n", name );
		close_decoder( &mpeg );
		return false;
	}

	sound.type = WF_PCMDATA;
	sound.wav = (byte *)Mem_Alloc( host.soundpool, sound.size );

	// decompress mpg into pcm wav format
	while( bytesWrite < sound.size )
	{
		int	outsize;

		if( feed_mpeg_stream( &mpeg, NULL, 0 ) != MP3_OK && mpeg.outsize <= 0 )
		{
			char	*data = (char *)buffer + pos;
			int	bufsize;

			// if there are no bytes remainig so we can decompress the new frame
			if( pos + FRAME_SIZE > filesize )
				bufsize = ( filesize - pos );
			else bufsize = FRAME_SIZE;
			pos += bufsize;

			if( feed_mpeg_stream( &mpeg, data, bufsize ) != MP3_OK )
				break; // there was end of the stream
		}

		if( bytesWrite + mpeg.outsize > sound.size )
			outsize = ( sound.size - bytesWrite );
		else outsize = mpeg.outsize;

		memcpy( &sound.wav[bytesWrite], mpeg.out, outsize );
		bytesWrite += outsize;
	}

	sound.samples = bytesWrite / ( sound.width * sound.channels );
	close_decoder( &mpeg );

	return true;
}

/*
=================
Stream_OpenMPG
=================
*/
stream_t *Stream_OpenMPG( const char *filename )
{
	mpeg_t	*mpegFile;
	stream_t	*stream;
	file_t	*file;

	file = FS_Open( filename, "rb", false );
	if( !file ) return NULL;

	// at this point we have valid stream
	stream = Mem_Alloc( host.soundpool, sizeof( stream_t ));
	stream->file = file;
	stream->pos = 0;

	mpegFile = Mem_Alloc( host.soundpool, sizeof( mpeg_t ));

	// couldn't create decoder
	if( !create_decoder( mpegFile ))
	{
		MsgDev( D_ERROR, "Stream_OpenMPG: couldn't create decoder: %s\n", mpegFile->error );
		Mem_Free( mpegFile );
		Mem_Free( stream );
		FS_Close( file );
		return NULL;
	}

	mpegFile->fread = FS_Read;
	mpegFile->fseek = FS_Seek;
	mpegFile->close = FS_Close;

	// trying to open stream and read header
	if( !open_mpeg_stream( mpegFile, file ))
	{
		MsgDev( D_ERROR, "Sound_LoadMPG: (%s) is probably corrupted: %s\n", filename, mpegFile->error );
		close_decoder( mpegFile );
		Mem_Free( mpegFile );
		Mem_Free( stream );
		FS_Close( file );
		return NULL;
	}

	stream->buffsize = 0; // how many samples left from previous frame
	stream->channels = mpegFile->channels;
	stream->pos += mpegFile->outsize;
	stream->rate = mpegFile->rate;
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
long Stream_ReadMPG( stream_t *stream, long needBytes, void *buffer )
{
	// buffer handling
	int	bytesWritten = 0;
	int	result;
	mpeg_t	*mpg;

	mpg = (mpeg_t *)stream->ptr;
	Assert( mpg != NULL );

	while( 1 )
	{
		long	outsize;
		byte	*data;

		if( !stream->buffsize )
		{
			result = read_mpeg_stream( mpg );
			stream->pos += mpg->outsize;

			if( result != MP3_OK )
			{
				// if there are no bytes remainig so we can decompress the new frame
				result = read_mpeg_stream( mpg );
				stream->pos += mpg->outsize;

				if( result != MP3_OK )
					break; // there was end of the stream
			}
		}

		// check remaining size
		if( bytesWritten + mpg->outsize > needBytes )
			outsize = ( needBytes - bytesWritten ); 
		else outsize = mpg->outsize;

		// copy raw sample to output buffer
		data = (byte *)buffer + bytesWritten;
		memcpy( data, &mpg->out[stream->buffsize], outsize );
		bytesWritten += outsize;
		mpg->outsize -= outsize;
		stream->buffsize += outsize;

		// continue from this sample on a next call
		if( bytesWritten >= needBytes )
			return bytesWritten;

		stream->buffsize = 0; // no bytes remaining
	}

	return 0;
}

/*
=================
Stream_SetPosMPG

assume stream is valid
=================
*/
long Stream_SetPosMPG( stream_t *stream, long newpos )
{
	int newPos = set_stream_pos( stream->ptr, newpos );

	if( newPos != -1 )
	{
		stream->pos = newPos;
		stream->buffsize = 0;
		return true;
	}

	// failed to seek for some reasons
	return false;
}

/*
=================
Stream_GetPosMPG

assume stream is valid
=================
*/
long Stream_GetPosMPG( stream_t *stream )
{
	return get_stream_pos( stream->ptr );
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
		mpeg_t	*mpg;

		mpg = (mpeg_t *)stream->ptr;
		close_decoder( mpg );
		Mem_Free( stream->ptr );
	}

	Mem_Free( stream );
}