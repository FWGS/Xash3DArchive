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

#ifndef ALIAS_H
#define ALIAS_H

/*
==============================================================================

ALIAS MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/

#define IDALIASHEADER	(('O'<<24)+('P'<<16)+('D'<<8)+'I')	// little-endian "IDPO"

#define ALIAS_VERSION	6

// must match definition in sprite.h
#ifndef SYNCTYPE_T
#define SYNCTYPE_T
typedef enum
{
	ST_SYNC = 0,
	ST_RAND
} synctype_t;
#endif

typedef enum
{
	ALIAS_SINGLE = 0,
	ALIAS_GROUP
} aliasframetype_t;

typedef enum
{
	ALIAS_SKIN_SINGLE = 0,
	ALIAS_SKIN_GROUP
} aliasskintype_t;

typedef struct
{
	int		ident;
	int		version;
	vec3_t		scale;
	vec3_t		scale_origin;
	float		boundingradius;
	vec3_t		eyeposition;
	int		numskins;
	int		skinwidth;
	int		skinheight;
	int		numverts;
	int		numtris;
	int		numframes;
	synctype_t	synctype;
	int		flags;
	float		size;
} daliashdr_t;

// TODO: could be shorts
typedef struct
{
	int		onseam;
	int		s;
	int		t;
} stvert_t;

typedef struct dtriangle_s
{
	int		facesfront;
	int		vertindex[3];
} dtriangle_t;

#define DT_FACES_FRONT	0x0010
#define ALIAS_ONSEAM	0x0020

// This mirrors trivert_t in trilib.h, is present so Quake knows how to
// load this data
typedef struct
{
	byte		v[3];
	byte		lightnormalindex;
} dtrivertex_t;

typedef struct
{
	dtrivertex_t	bboxmin;	// lightnormal isn't used
	dtrivertex_t	bboxmax;	// lightnormal isn't used
	char		name[16];	// frame name from grabbing
} daliasframe_t;

typedef struct
{
	int		numframes;
	dtrivertex_t	bboxmin;	// lightnormal isn't used
	dtrivertex_t	bboxmax;	// lightnormal isn't used
} daliasgroup_t;

typedef struct
{
	int		numskins;
} daliasskingroup_t;

typedef struct
{
	float		interval;
} daliasinterval_t;

typedef struct
{
	float		interval;
} daliasskininterval_t;

typedef struct
{
	aliasframetype_t	type;
} daliasframetype_t;

typedef struct
{
	aliasskintype_t	type;
} daliasskintype_t;

#endif//ALIAS_H