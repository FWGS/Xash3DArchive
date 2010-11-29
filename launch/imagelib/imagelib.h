//=======================================================================
//			Copyright XashXT Group 2008 �
//			imagelib.h - engine image lib
//=======================================================================
#ifndef IMAGELIB_H
#define IMAGELIB_H

#include "launch.h"

// skyorder_q2[6] = { 2, 3, 1, 0, 4, 5, }; // Quake, Half-Life skybox ordering
// skyorder_ms[6] = { 4, 5, 1, 0, 2, 3  }; // Microsoft DDS ordering (reverse)

// cubemap hints
typedef enum
{
	CB_HINT_NO = 0,

	// dds cubemap hints ( Microsoft sides order )
	CB_HINT_POSX,
	CB_HINT_NEGX,
	CB_HINT_POSZ,
	CB_HINT_NEGZ,
	CB_HINT_POSY,
	CB_HINT_NEGY,
	CB_FACECOUNT,
} side_hint_t;

typedef enum
{
	IL_HINT_NO = 0,
	IL_HINT_Q1,	// palette choosing
	IL_HINT_Q2,
	IL_HINT_HL,
} image_hint_t;

typedef struct loadformat_s
{
	const char *formatstring;
	const char *ext;
	qboolean (*loadfunc)( const char *name, const byte *buffer, size_t filesize );
	image_hint_t hint;
} loadpixformat_t;

typedef struct saveformat_s
{
	const char *formatstring;
	const char *ext;
	qboolean (*savefunc)( const char *name, rgbdata_t *pix );
} savepixformat_t;

typedef struct imglib_s
{
	const loadpixformat_t	*baseformats;	// used for loading internal images
	const loadpixformat_t	*loadformats;
	const savepixformat_t	*saveformats;

	// current 2d image state
	word		width;
	word		height;
	uint		type;		// main type switcher
	uint		flags;		// additional image flags
	size_t		size;		// image rgba size (for bounds checking)
	uint		ptr;		// safe image pointer
	byte		*rgba;		// image pointer (see image_type for details)

	// current cubemap state
	int		source_width;	// locked cubemap dims (all wrong sides will be automatically resampled)
	int		source_height;
	uint		source_type;	// shared image type for all mipmaps or cubemap sides
	int		num_sides;	// how much sides is loaded 
	byte		*cubemap;		// cubemap pack

	// indexed images state
	uint		*d_currentpal;	// installed version of internal palette
	int		d_rendermode;	// palette rendermode
	byte		*palette;		// palette pointer

	// global parms
	int		bpp;		// PFDesc[type].bpp
	int		SizeOfFile;	// total size
	qboolean		(*decompress)( uint, int, uint, uint, uint, const void* );

	rgba_t		fogParams;	// some water textures has info about underwater fog

	image_hint_t	hint;		// hint for some loaders
	byte		*tempbuffer;	// for convert operations
	int		cmd_flags;
} imglib_t;

/*
========================================================================

.PCX image format	(ZSoft Paintbrush)

========================================================================
*/
typedef struct
{
	char	manufacturer;
	char	version;
	char	encoding;
	char	bits_per_pixel;
	word	xmin,ymin,xmax,ymax;
	word	hres,vres;
	byte	palette[48];
	char	reserved;
	char	color_planes;
	word	bytes_per_line;
	word	palette_type;
	char	filler[58];
} pcx_t;

/*
========================================================================

.WAL image format	(Wally textures)

========================================================================
*/
typedef struct wal_s
{
	char	name[32];
	uint	width, height;
	uint	offsets[4];	// four mip maps stored
	char	animname[32];	// next frame in animation chain
	int	flags;
	int	contents;
	int	value;
} wal_t;

/*
========================================================================

.LMP image format	(Quake1 gfx lumps)

========================================================================
*/
typedef struct flat_s
{
	short	width;
	short	height;
	short	desc[2];		// probably not used
} flat_t;

/*
========================================================================

.BMP image format

========================================================================
*/
#pragma pack( 1 )
typedef struct
{
	char	id[2];		//bmfh.bfType
	dword	fileSize;		//bmfh.bfSize
	dword	reserved0;	//bmfh.bfReserved1 + bmfh.bfReserved2
	dword	bitmapDataOffset;	//bmfh.bfOffBits
	dword	bitmapHeaderSize;	//bmih.biSize
	int	width;		//bmih.biWidth
	int	height;		//bmih.biHeight
	word	planes;		//bmih.biPlanes
	word	bitsPerPixel;	//bmih.biBitCount
	dword	compression;	//bmih.biCompression
	dword	bitmapDataSize;	//bmih.biSizeImage
	dword	hRes;		//bmih.biXPelsPerMeter
	dword	vRes;		//bmih.biYPelsPerMeter
	dword	colors;		//bmih.biClrUsed
	dword	importantColors;	//bmih.biClrImportant
} bmp_t;
#pragma pack( )

/*
========================================================================

.TGA image format	(Truevision Targa)

========================================================================
*/
typedef struct tga_s
{
	byte	id_length;
	byte	colormap_type;
	byte	image_type;
	word	colormap_index;
	word	colormap_length;
	byte	colormap_size;
	word	x_origin;
	word	y_origin;
	word	width;
	word	height;
	byte	pixel_size;
	byte	attributes;
} tga_t;

/*
========================================================================

.JPG image format

========================================================================
*/
// defined in img_jpg.c

// imagelib definitions
#define IMAGE_MAXWIDTH	4096
#define IMAGE_MAXHEIGHT	4096
#define LUMP_MAXWIDTH	1024	// WorldCraft limits
#define LUMP_MAXHEIGHT	1024

#define TRANS_THRESHOLD	10	// in pixels
enum
{
	LUMP_NORMAL = 0,
	LUMP_TRANSPARENT,
	LUMP_DECAL,
	LUMP_QFONT,
	LUMP_EXTENDED		// bmp images have extened palette with alpha-channel
};

enum
{
	PAL_INVALID = -1,
	PAL_CUSTOM = 0,
	PAL_DOOM1,
	PAL_QUAKE1,
	PAL_QUAKE2,
	PAL_HALFLIFE
};

extern imglib_t image;
extern byte *fs_mempool;
extern const bpc_desc_t PFDesc[];

void Image_RoundDimensions( int *scaled_width, int *scaled_height );
byte *Image_ResampleInternal( const void *indata, int in_w, int in_h, int out_w, int out_h, int intype, qboolean *done );
byte *Image_FlipInternal( const byte *in, word *srcwidth, word *srcheight, int type, int flags );
void Image_FreeImage( rgbdata_t *pack );
void Image_Save( const char *filename, rgbdata_t *pix );
rgbdata_t *Image_Load(const char *filename, const byte *buffer, size_t buffsize );
qboolean Image_Copy8bitRGBA( const byte *in, byte *out, int pixels );
qboolean Image_AddIndexedImageToPack( const byte *in, int width, int height );
qboolean Image_AddRGBAImageToPack( uint target, uint imageSize, const void* data );
void Image_ConvertPalTo24bit( rgbdata_t *pic );
void Image_GetPaletteLMP( const byte *pal, int rendermode );
void Image_GetPalettePCX( const byte *pal );
void Image_GetPaletteBMP( const byte *pal );
int Image_ComparePalette( const byte *pal );
void Image_CopyPalette24bit( void );
void Image_CopyPalette32bit( void );
void Image_SetPixelFormat( void );
void Image_GetPaletteQ2( void );
void Image_GetPaletteQ1( void );
void Image_GetPaletteD1( void );	// doom 2 on TNT :)
void Image_GetPaletteHL( void );

//
// formats load
//
qboolean Image_LoadMIP( const char *name, const byte *buffer, size_t filesize );
qboolean Image_LoadMDL( const char *name, const byte *buffer, size_t filesize );
qboolean Image_LoadSPR( const char *name, const byte *buffer, size_t filesize );
qboolean Image_LoadTGA( const char *name, const byte *buffer, size_t filesize );
qboolean Image_LoadBMP( const char *name, const byte *buffer, size_t filesize );
qboolean Image_LoadFNT( const char *name, const byte *buffer, size_t filesize );
qboolean Image_LoadJPG( const char *name, const byte *buffer, size_t filesize );
qboolean Image_LoadPCX( const char *name, const byte *buffer, size_t filesize );
qboolean Image_LoadLMP( const char *name, const byte *buffer, size_t filesize );
qboolean Image_LoadWAL( const char *name, const byte *buffer, size_t filesize );
qboolean Image_LoadFLT( const char *name, const byte *buffer, size_t filesize );
qboolean Image_LoadPAL( const char *name, const byte *buffer, size_t filesize );

//
// formats save
//
qboolean Image_SaveTGA( const char *name, rgbdata_t *pix );
qboolean Image_SaveBMP( const char *name, rgbdata_t *pix );
qboolean Image_SaveJPG( const char *name, rgbdata_t *pix );
qboolean Image_SavePCX( const char *name, rgbdata_t *pix );

//
// img_utils.c
//
void Image_Reset( void );
rgbdata_t *ImagePack( void );
byte *Image_Copy( size_t size );
qboolean Image_ValidSize( const char *name );
qboolean Image_LumpValidSize( const char *name );

#endif//IMAGELIB_H