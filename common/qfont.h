/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#ifndef QFONT_H
#define QFONT_H

/*
========================================================================

.QFONT image format

========================================================================
*/
#define QCHAR_WIDTH		16
#define QFONT_WIDTH		16	// valve fonts used contant sizes	
#define QFONT_HEIGHT	((128 - 32) / 16)
#define NUM_GLYPHS		256

typedef struct
{
	short	startoffset;
	short	charwidth;
} charinfo;

typedef struct
{
	int 	width;
	int	height;
	int	rowcount;
	int	rowheight;
	charinfo	fontinfo[NUM_GLYPHS];
	byte 	data[4];		// variable sized
} qfont_t;

#endif//QFONT_H