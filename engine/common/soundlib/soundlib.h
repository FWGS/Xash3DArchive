/*
soundlib.h - engine sound lib
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

#ifndef SOUNDLIB_H
#define SOUNDLIB_H

#include "common.h"

typedef struct loadwavformat_s
{
	const char *formatstring;
	const char *ext;
	qboolean (*loadfunc)( const char *name, const byte *buffer, size_t filesize );
} loadwavformat_t;

typedef struct streamformat_s
{
	const char *formatstring;
	const char *ext;

	stream_t *(*openfunc)( const char *filename );
	long (*readfunc)( stream_t *stream, long bytes, void *buffer );
	void (*freefunc)( stream_t *stream );
} streamformat_t;

typedef struct sndlib_s
{
	const loadwavformat_t	*loadformats;
	const streamformat_t	*streamformat;	// music stream

	// current sound state
	int		type;		// sound type
	int		rate;		// num samples per second (e.g. 11025 - 11 khz)
	int		width;		// resolution - bum bits divided by 8 (8 bit is 1, 16 bit is 2)
	int		channels;		// num channels (1 - mono, 2 - stereo)
	int		loopstart;	// start looping from
	uint		samples;		// total samplecount in sound
	uint		flags;		// additional sound flags
	size_t		size;		// sound unpacked size (for bounds checking)
	byte		*wav;		// sound pointer (see sound_type for details)

	byte		*tempbuffer;	// for convert operations
	int		cmd_flags;
} sndlib_t;

typedef struct stream_s
{
	const streamformat_t	*format;	// streamformat to operate

	// current stream state
	file_t			*file;	// stream file
	int			width;	// resolution - num bits divided by 8 (8 bit is 1, 16 bit is 2)
	int			rate;	// stream rate
	int			channels;	// stream channels
	int			type;	// wavtype
	size_t			size;	// total stream size
	int			pos;	// keep track wav position
	void			*ptr;
};

extern sndlib_t sound;
//
// formats load
//
qboolean Sound_LoadWAV( const char *name, const byte *buffer, size_t filesize );
qboolean Sound_LoadMPG( const char *name, const byte *buffer, size_t filesize );

//
// stream operate
//
stream_t *Stream_OpenWAV( const char *filename );
long Stream_ReadWAV( stream_t *stream, long bytes, void *buffer );
void Stream_FreeWAV( stream_t *stream );
stream_t *Stream_OpenMPG( const char *filename );
long Stream_ReadMPG( stream_t *stream, long bytes, void *buffer );
void Stream_FreeMPG( stream_t *stream );

#endif//SOUNDLIB_H