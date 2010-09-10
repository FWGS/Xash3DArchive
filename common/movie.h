//=======================================================================
//			Copyright XashXT Group 2008 ©
//			  movie.h - RoQ video format
//=======================================================================
#ifndef MOVIE_H
#define MOVIE_H

/*
========================================================================
ROQ FILES

The .roq file are vector-compressed movies
========================================================================
*/

#define RoQ_HEADER1		4228
#define RoQ_HEADER2		-1
#define RoQ_HEADER3		30
#define RoQ_FRAMERATE	30

// RoQ markers
#define RoQ_INFO		0x1001
#define RoQ_QUAD_CODEBOOK	0x1002
#define RoQ_QUAD_VQ		0x1011
#define RoQ_SOUND_MONO	0x1020
#define RoQ_SOUND_STEREO	0x1021

// RoQ movie type
#define RoQ_ID_MOT		0x00
#define RoQ_ID_FCC		0x01
#define RoQ_ID_SLD		0x02
#define RoQ_ID_CCC		0x03

typedef struct 
{
	byte		y[4];
	byte		u, v;
} dcell_t;

typedef struct 
{
	byte		idx[4];
} dquadcell_t;

typedef struct 
{
	unsigned short	id;
	unsigned int	size;
	unsigned short	argument;
} droqchunk_t;

#endif//REF_DFILES_H