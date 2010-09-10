//=======================================================================
//			Copyright XashXT Group 2010 ©
//		     qfont.h - Quake font width variable widths
//=======================================================================
#ifndef QFONT_H
#define QFONT_H

/*
========================================================================

.QFONT image format

========================================================================
*/
#define	QCHAR_WIDTH	16
#define	QFONT_WIDTH	16	// valve fonts used contant sizes	
#define	QFONT_HEIGHT        ((128 - 32) / 16)

typedef struct
{
	short	startoffset;
	short	charwidth;
} charset_t;

typedef struct
{
	int 	width;
	int	height;
	int	rowcount;
	int	rowheight;
	charset_t	fontinfo[256];
	byte 	data[4];		// variable sized
} qfont_t;

#endif//QFONT_H