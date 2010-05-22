//=======================================================================
//		  temporare header to keep engine work
//=======================================================================
#ifndef BSPFILE_H
#define BSPFILE_H

/*
==============================================================================
BRUSH MODELS

.bsp contain level static geometry with including PVS, PHS, PHYS and lightning info
==============================================================================
*/

// header
#define Q3IDBSP_VERSION	46
#define RTCWBSP_VERSION	47
#define IGIDBSP_VERSION	48	// extended brushides
#define XRIDBSP_VERSION	48	// x-real bsp version (extended verts and lightgrid)
#define RFIDBSP_VERSION	1	// both raven bsp and qfusion bsp

#define IDBSPMODHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I') // little-endian "IBSP"
#define RBBSPMODHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'R') // little-endian "RBSP"
#define QFBSPMODHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'F') // little-endian "FBSP"
#define XRBSPMODHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'X') // little-endian "XBSP"

// 32 bit limits
#define MAX_MAP_AREA_BYTES		MAX_MAP_AREAS / 8	// bit vector of area visibility
#define MAX_MAP_AREAS		0x100		// don't increase this
#define MAX_MAP_FOGS		0x100
#define MAX_MAP_MODELS		0x2000		// mesh models and sprites too
#define MAX_MAP_AREAPORTALS		0x400
#define MAX_MAP_ENTITIES		0x2000
#define MAX_MAP_SHADERS		0x1000
#define MAX_MAP_TEXINFO		0x8000
#define MAX_MAP_BRUSHES		0x8000
#define MAX_MAP_PLANES		0x20000
#define MAX_MAP_NODES		0x20000
#define MAX_MAP_BRUSHSIDES		0x20000
#define MAX_MAP_LEAFS		0x20000
#define MAX_MAP_VERTS		0x80000
#define MAX_MAP_SURFACES		0x20000
#define MAX_MAP_LEAFFACES		0x20000
#define MAX_MAP_LEAFBRUSHES		0x40000
#define MAX_MAP_PORTALS		0x20000
#define MAX_MAP_INDICES		0x80000
#define MAX_MAP_ENTSTRING		0x80000
#define MAX_MAP_LIGHTING		0x800000
#define MAX_MAP_VISIBILITY		0x800000
#define MAX_MAP_COLLISION		0x800000

#define MAX_WORLD_COORD		( 128 * 1024 )
#define MIN_WORLD_COORD		(-128 * 1024 )
#define WORLD_SIZE			( MAX_WORLD_COORD - MIN_WORLD_COORD )
#define MAX_SHADERPATH		64

#define MAX_BUILD_SIDES		512	// per one brush. (don't touch!)
#define LM_STYLES			4	// MAXLIGHTMAPS
#define MAX_LIGHT_STYLES		64
#define MAX_SWITCHED_LIGHTS		32

// lump names
typedef struct
{
	int		fileofs, filelen;
} lump_t;

#define LUMP_ENTITIES		0
#define LUMP_SHADERS		1
#define LUMP_PLANES			2
#define LUMP_NODES			3
#define LUMP_LEAFS			4
#define LUMP_LEAFSURFACES		5
#define LUMP_LEAFBRUSHES		6
#define LUMP_MODELS			7
#define LUMP_BRUSHES		8
#define LUMP_BRUSHSIDES		9
#define LUMP_VERTEXES		10
#define LUMP_ELEMENTS		11
#define LUMP_FOGS			12
#define LUMP_SURFACES		13
#define LUMP_LIGHTING		14
#define LUMP_LIGHTGRID		15
#define LUMP_VISIBILITY		16
#define LUMP_LIGHTARRAY		17		// RBSP extra lump to save space
#define HEADER_LUMPS		18		// 17 for IDBSP

typedef enum
{
	SURF_NONE			= 0,		// just a mask for source tabulation
	SURF_NODAMAGE		= BIT(0),		// never give falling damage
	SURF_SLICK		= BIT(1),		// effects game physics
	SURF_SKY			= BIT(2),		// don't draw, but add to skybox
	SURF_LADDER		= BIT(3),		// this is ladder surface
	SURF_NOIMPACT		= BIT(4),		// don't make missile explosions
	SURF_NOMARKS		= BIT(5),		// don't leave missile marks
	SURF_WARP			= BIT(6),		// turbulent water warping
	SURF_NODRAW		= BIT(7),		// don't generate a drawsurface at all
	SURF_HINT			= BIT(8),		// make a primary bsp splitter
	SURF_SKIP			= BIT(9),		// completely ignore, allowing non-closed brushes
	SURF_NOLIGHTMAP		= BIT(10),	// don't place lightmap for this surface
	SURF_POINTLIGHT		= BIT(11),	// generate lighting info at vertexes
	SURF_METALSTEPS		= BIT(12),	// REMOVE? clanking footsteps
	SURF_NOSTEPS		= BIT(13),	// REMOVE? no footstep sounds
	SURF_NONSOLID		= BIT(14),	// don't collide against curves with this set
	SURF_LIGHTFILTER		= BIT(15),	// act as a light filter during q3map -light
	SURF_ALPHASHADOW		= BIT(16),	// do per-pixel light shadow casting in q3map
	SURF_NODLIGHT		= BIT(17),	// never add dynamic lights
	SURF_DUST			= BIT(18),	// REMOVE? leave a dust trail when walking on this surface
} surfaceFlags_t;

// bsp contents
typedef enum
{
	BASECONT_NONE		= 0, 		// just a mask for source tabulation
	BASECONT_SOLID		= BIT(0),		// an eye is never valid in a solid
                                                 		// reserved
	BASECONT_SKY		= BIT(2),
	BASECONT_LAVA		= BIT(3),
	BASECONT_SLIME		= BIT(4),
	BASECONT_WATER		= BIT(5),
	BASECONT_FOG		= BIT(6),
	
	// space for new user contents
	BASECONT_AREAPORTAL		= BIT(15),
	BASECONT_PLAYERCLIP		= BIT(16),
	BASECONT_MONSTERCLIP	= BIT(17),
	BASECONT_CLIP		= (BASECONT_PLAYERCLIP|BASECONT_MONSTERCLIP), // both type clip
	BASECONT_TELEPORTER		= BIT(18),
	BASECONT_JUMPPAD		= BIT(19),
	BASECONT_CLUSTERPORTAL	= BIT(20),
	BASECONT_DONOTENTER		= BIT(21),
	BASECONT_ORIGIN		= BIT(22),	// removed before bsping an entity
	BASECONT_BODY		= BIT(23),	// should never be on a brush, only in game
	BASECONT_CORPSE		= BIT(24),	// dead corpse
	BASECONT_DETAIL		= BIT(25),	// brushes not used for the bsp
	BASECONT_STRUCTURAL		= BIT(26),	// brushes used for the bsp
	BASECONT_TRANSLUCENT	= BIT(27),	// don't consume surface fragments inside
	BASECONT_LADDER		= BIT(28),	// ladder volume
	BASECONT_NODROP		= BIT(29),	// don't leave bodies or items (death fog, lava)

	// content masks
	MASK_SOLID		= (BASECONT_SOLID|BASECONT_BODY),
	MASK_PLAYERSOLID		= (BASECONT_SOLID|BASECONT_PLAYERCLIP|BASECONT_BODY),
	MASK_MONSTERSOLID		= (BASECONT_SOLID|BASECONT_MONSTERCLIP|BASECONT_BODY),
	MASK_DEADSOLID		= (BASECONT_SOLID|BASECONT_PLAYERCLIP),
	MASK_WATER		= (BASECONT_WATER|BASECONT_LAVA|BASECONT_SLIME),
	MASK_OPAQUE		= (BASECONT_SOLID|BASECONT_SLIME|BASECONT_LAVA),
	MASK_SHOT			= (BASECONT_SOLID|BASECONT_BODY|BASECONT_CORPSE)
} contentType_t;

typedef enum
{
	MST_BAD = 0,
	MST_PLANAR,
	MST_PATCH,
	MST_TRISURF,
	MST_FLARE,
	MST_FOLIAGE
} bspSurfaceType_t;

typedef struct
{
	int	ident;
	int	version;	
	lump_t	lumps[HEADER_LUMPS];
} dheader_t;

typedef struct
{
	vec3_t	mins;
	vec3_t	maxs;
	int	firstsurface;	// submodels just draw faces 
	int	numsurfaces;	// without walking the bsp tree
	int	firstbrush;	// physics stuff
	int	numbrushes;
} dmodel_t;

typedef struct
{
	char	name[64];		// shader name
	int	surfaceFlags;	// surface flags (can be replaced)
	int	contentFlags;	// texture contents (can be replaced)
} dshader_t;

typedef struct
{
	vec3_t	point;
	vec2_t	tex_st;		// texture coords
	vec2_t	lm_st;		// lightmap texture coords
	vec3_t	normal;		// normal
	byte	color[4];		// color used for vertex lighting
} dvertexq_t;	// IBSP

typedef struct
{
	vec3_t	point;
	vec2_t	tex_st;
	vec2_t	lm_st[LM_STYLES];
	vec3_t	normal;
	byte	color[LM_STYLES][4];
} dvertexr_t;	// RBSP

typedef struct
{
	vec3_t	point;
	vec2_t	tex_st;
	vec2_t	lm_st;
	vec3_t	normal;
	vec4_t	paintcolor;
	vec4_t	lightcolor;
	vec3_t	lightdir;
} dvertexx_t;	// XBSP

typedef struct
{
	float	normal[3];
	float	dist;
} dplane_t;

typedef struct
{
	int	planenum;
	int	children[2];	// negative numbers are -(leafs+1), not nodes
	int	mins[3];		// for frustum culling
	int	maxs[3];
} dnode_t;

typedef dword	dleafface_t;
typedef dword	dleafbrush_t;

typedef struct
{
	int	cluster;
	int	area;
	int	mins[3];		// for frustum culling
	int	maxs[3];
	int	firstleafsurface;
	int	numleafsurfaces;
	int	firstleafbrush;
	int	numleafbrushes;
} dleaf_t;

typedef struct
{
	int	planenum;		// facing out of the leaf
	int	shadernum;	// surface description
} dbrushsideq_t;

typedef struct
{
	int	planenum;		// facing out of the leaf
	int	shadernum;	// surface description
	int	surfacenum;
} dbrushsider_t;

typedef struct
{
	int	firstside;
	int	numsides;
	int	shadernum;	// brush global shader (e.g. contents)
} dbrush_t;

typedef struct
{
	char	shader[64];
	int	brushnum;
	int	visibleside;
} dfog_t;

typedef struct
{
	int	numclusters;
	int	rowsize;
	byte	data[1];		// unbounded
} dvis_t;

typedef struct
{
	byte	ambient[3];
	byte	diffuse[3];
	byte	direction[2];
} dlightgridq_t;	// IBSP

typedef struct
{
	byte	ambient[LM_STYLES][3];
	byte	diffuse[LM_STYLES][3];
	byte	styles[LM_STYLES];
	byte	direction[2];
} dlightgridr_t;	// RBSP

typedef struct
{
	float	ambient[3];
	float	diffuse[3];
	byte	direction[2];
} dlightgridx_t;	// XBSP

typedef struct
{
	int	shadernum;
	int	fognum;
	int	facetype;

	int	firstvert;
	int	numverts;
	uint	firstelem;
	int	numelems;

	int	lm_texnum;	// lightmap info
	int	lm_offset[2];
	int	lm_size[2];

	vec3_t	origin;		// MST_FLARE only

	vec3_t	mins;
	vec3_t	maxs;		// MST_PATCH and MST_TRISURF only
	vec3_t	normal;		// MST_PLANAR only

	int	patch_cp[2];	// patch control point dimensions
} dsurfaceq_t;	// IBSP, XBSP

typedef struct
{
	int	shadernum;
	int	fognum;
	int	facetype;

	int	firstvert;
	int	numverts;
	uint	firstelem;
	int	numelems;

	byte	lightmapStyles[LM_STYLES];
	byte	vertexStyles[LM_STYLES];

	int	lm_texnum[LM_STYLES];	// lightmap info
	int	lm_offset[LM_STYLES][2];
	int	lm_size[2];

	vec3_t	origin;		// FACETYPE_FLARE only

	vec3_t	mins;
	vec3_t	maxs;		// FACETYPE_PATCH and FACETYPE_TRISURF only
	vec3_t	normal;		// FACETYPE_PLANAR only

	int	patch_cp[2];	// patch control point dimensions
} dsurfacer_t;	// RBSP

#endif//BSPFILE_H