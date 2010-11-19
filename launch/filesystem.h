//=======================================================================
//			Copyright (C) XashXT Group 2006
//			     All Rights Reserved
//			zip32.h - zlib custom build
//=======================================================================
#ifndef FILESYSTEM_H
#define FILESYSTEM_H

/*
========================================================================
PAK FILES

The .pak files are just a linear collapse of a directory tree
========================================================================
*/
// header
#define IDPACKV1HEADER	(('K'<<24)+('C'<<16)+('A'<<8)+'P')	// little-endian "PACK"

#define MAX_FILES_IN_PACK	65536 // pak

typedef struct
{
	int		ident;
	int		dirofs;
	int		dirlen;
} dpackheader_t;

typedef struct
{
	char		name[56];		// total 64 bytes
	int		filepos;
	int		filelen;
} dpackfile_t;

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
#define IDIWADHEADER	(('D'<<24)+('A'<<16)+('W'<<8)+'I')	// little-endian "IWAD" doom1 game wad
#define IDPWADHEADER	(('D'<<24)+('A'<<16)+('W'<<8)+'P')	// little-endian "PWAD" doom1 game wad
#define IDWAD2HEADER	(('2'<<24)+('D'<<16)+('A'<<8)+'W')	// little-endian "WAD2" quake1 gfx.wad
#define IDWAD3HEADER	(('3'<<24)+('D'<<16)+('A'<<8)+'W')	// little-endian "WAD3" half-life wads

#define WAD3_NAMELEN	16
#define MAX_FILES_IN_WAD	8192

// hidden virtual lump types
#define TYP_ANY		-1	// any type can be accepted
#define TYP_NONE		0	// unknown lump type
#define TYP_FLMP		1	// doom1 hud picture (doom1 mapped lump)
#define TYP_SND		2	// doom1 wav sound (doom1 mapped lump)
#define TYP_MUS		3	// doom1 music file (doom1 mapped lump)
#define TYP_SKIN		4	// doom1 sprite model (doom1 mapped lump)
#define TYP_FLAT		5	// doom1 wall texture (doom1 mapped lump)
#define TYP_MAXHIDDEN	63	// after this number started typeing letters ( 'a', 'b' etc )

#include "const.h"

typedef struct
{
	int		ident;		// should be IWAD, WAD2 or WAD3
	int		numlumps;		// num files
	int		infotableofs;
} dwadinfo_t;

// doom1 and doom2 lump header
typedef struct
{
	int		filepos;
	int		size;
	char		name[8];		// null not included
} dlumpfile_t;

typedef struct
{
	int		filepos;
	int		disksize;
	int		size;		// uncompressed
	char		type;
	char		compression;	// probably not used
	char		pad1;
	char		pad2;
	char		name[16];		// must be null terminated
} dlumpinfo_t;

#include "custom.h"

#define IDCUSTOMHEADER	(('K'<<24)+('A'<<16)+('P'<<8)+'H') // little-endian "HPAK"
#define IDCUSTOM_VERSION	1

typedef struct hpak_s
{
	char		*name;
	resource_t	HpakResource;
	size_t		size;
	void		*data;
	struct hpak_s	*next;
} hpak_t;

typedef struct
{
	int		ident;		// should be equal HPAK
	int		version;
	int		seek;		// infotableofs ?
} hpak_header_t;

typedef struct
{
	resource_t	DirectoryResource;
	int		seek;		// filepos ?
	int		size;
} hpak_dir_t;

typedef struct
{
	int		count;
	hpak_dir_t	*dirs;		// variable sized.
} hpak_container_t;

#endif//FILESYSTEM_H