//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        qfiles_ref.h - xash supported formats
//=======================================================================
#ifndef REF_DFILES_H
#define REF_DFILES_H

/*
========================================================================
.WAD archive format	(WhereAllData - WAD)

List of compressed files, that can be identify only by TYPE_*

<format>
header:	dwadinfo_t[dwadinfo_t]
file_1:	byte[dwadinfo_t[num]->disksize]
file_2:	byte[dwadinfo_t[num]->disksize]
file_3:	byte[dwadinfo_t[num]->disksize]
...
file_n:	byte[dwadinfo_t[num]->disksize]
infotable	dlumpinfo_t[dwadinfo_t->numlumps]
========================================================================
*/

#define IDWAD3HEADER	(('3'<<24)+('D'<<16)+('A'<<8)+'W')

// dlumpinfo_t->compression
#define CMP_NONE			0	// compression none
#define CMP_LZSS			1	// currently not used
#define CMP_ZLIB			2	// zip-archive compression

// dlumpinfo_t->type
#define TYPE_QPAL			64	// quake palette
#define TYPE_QTEX			65	// probably was never used
#define TYPE_QPIC			66	// quake1 and hl pic (lmp_t)
#define TYPE_MIPTEX			67	// half-life (mip_t) previous was TYP_SOUND but never used in quake1
#define TYPE_QMIP			68	// quake1 (mip_t) (replaced with TYPE_MIPTEX while loading)
#define TYPE_RAW			69	// raw data
#define TYPE_QFONT			70	// half-life font (qfont_t)
#define TYPE_SOUND			71	// hl2 type
#define TYPE_SCRIPT			73	// .qc scripts (xash ext)

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

/*
==============================================================================

SPRITE MODELS

.spr extended version (Half-Life compatible sprites with some xash extensions)
==============================================================================
*/

#define IDSPRITEHEADER	(('P'<<24)+('S'<<16)+('D'<<8)+'I')	// little-endian "IDSP"
#define SPRITE_VERSION	2				// Half-Life sprites

typedef enum
{
	ST_SYNC = 0,
	ST_RAND
} synctype_t;

typedef enum
{
	FRAME_SINGLE = 0,
	FRAME_GROUP,
	FRAME_ANGLED			// xash ext
} frametype_t;

typedef enum
{
	SPR_NORMAL = 0,
	SPR_ADDITIVE,
	SPR_INDEXALPHA,
	SPR_ALPHTEST,
	SPR_ADDGLOW			// xash ext
} drawtype_t;

typedef enum
{
	SPR_FWD_PARALLEL_UPRIGHT = 0,
	SPR_FACING_UPRIGHT,
	SPR_FWD_PARALLEL,
	SPR_ORIENTED,
	SPR_FWD_PARALLEL_ORIENTED,
} angletype_t; 

typedef enum
{
	SPR_CULL_FRONT = 0,			// oriented sprite will be draw with one face
	SPR_CULL_NONE,			// oriented sprite will be draw back face too
} facetype_t;

typedef struct
{
	int		ident;		// LittleLong 'ISPR'
	int		version;		// current version 3
	angletype_t	type;		// camera align
	drawtype_t	texFormat;	// rendering mode (Xash3D ext)
	int		boundingradius;	// quick face culling
	int		bounds[2];	// minsmaxs
	int		numframes;	// including groups
	facetype_t	facetype;		// cullface (Xash3D ext)
	synctype_t	synctype;		// animation synctype
} dsprite_t;

typedef struct
{
	int		origin[2];
	int		width;
	int		height;
} dspriteframe_t;

typedef struct
{
	int		numframes;
} dspritegroup_t;

typedef struct
{
	float		interval;
} dspriteinterval_t;

typedef struct
{
	frametype_t	type;
} dframetype_t;

/*
========================================================================

.LMP image format	(Half-Life gfx.wad lumps)

========================================================================
*/
typedef struct lmp_s
{
	uint	width;
	uint	height;
} lmp_t;

/*
========================================================================

.MIP image format	(half-Life textures)

========================================================================
*/
typedef struct mip_s
{
	char	name[16];
	uint	width;
	uint	height;
	uint	offsets[4];	// four mip maps stored
} mip_t;

#include "studio.h"

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
	byte		u;
	byte		v;
} dcell_t;

typedef struct 
{
	byte		idx[4];
} dquadcell_t;

typedef struct 
{
	word		id;
	uint		size;
	word		argument;
} droqchunk_t;

// other limits
#define MAX_KEY		128
#define MAX_VALUE		1024

#endif//REF_DFILES_H