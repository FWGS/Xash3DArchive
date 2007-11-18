//=======================================================================
//			Copyright XashXT Group 2007 �
//		      ref_format.h - xash supported formats
//=======================================================================
#ifndef REF_FORMAT_H
#define REF_FORMAT_H

/*
==============================================================================

SPRITE MODELS
==============================================================================
*/

// header
#define SPRITE_VERSION_HALF	2
#define SPRITE_VERSION_XASH	3
#define IDSPRITEHEADER	(('P'<<24)+('S'<<16)+('D'<<8)+'I') // little-endian "IDSP"

// render format
#define SPR_VP_PARALLEL_UPRIGHT	0
#define SPR_FACING_UPRIGHT		1
#define SPR_VP_PARALLEL		2
#define SPR_ORIENTED		3
#define SPR_VP_PARALLEL_ORIENTED	4

#define SPR_NORMAL			0 // solid sprite
#define SPR_ADDITIVE		1
#define SPR_INDEXALPHA		2
#define SPR_ALPHTEST		3
#define SPR_ADDGLOW			4 // same as additive, but without depthtest

typedef struct
{
	int		ident;
	int		version;
	int		type;
	int		texFormat;
	float		boundingradius;
	int		width;
	int		height;
	int		numframes;
	float		framerate; //xash auto-animate
	uint		rgbacolor; //packed rgba color
} dsprite_t;

typedef struct
{
	int		origin[2];
	int		width;
	int		height;
} dspriteframe_t;

typedef struct
{
	int		type;
} frametype_t;

/*
==============================================================================

STUDIO MODELS

Studio models are position independent, so the cache manager can move them.
==============================================================================
*/

// header
#define STUDIO_VERSION	10
#define IDSTUDIOHEADER	(('T'<<24)+('S'<<16)+('D'<<8)+'I') // little-endian "IDST"
#define IDSEQGRPHEADER	(('Q'<<24)+('S'<<16)+('D'<<8)+'I') // little-endian "IDSQ"

// studio limits
#define MAXSTUDIOTRIANGLES		32768	// max triangles per model
#define MAXSTUDIOVERTS		4096	// max vertices per submodel
#define MAXSTUDIOSEQUENCES		256	// total animation sequences
#define MAXSTUDIOSKINS		128	// total textures
#define MAXSTUDIOSRCBONES		512	// bones allowed at source movement
#define MAXSTUDIOBONES		128	// total bones actually used
#define MAXSTUDIOMODELS		32	// sub-models per model
#define MAXSTUDIOBODYPARTS		32	// body parts per submodel
#define MAXSTUDIOGROUPS		16	// sequence groups (e.g. barney01.mdl, barney02.mdl, e.t.c)
#define MAXSTUDIOANIMATIONS		512	// max frames per sequence
#define MAXSTUDIOMESHES		256	// max textures per model
#define MAXSTUDIOEVENTS		1024	// events per model
#define MAXSTUDIOPIVOTS		256	// pivot points
#define MAXSTUDIOBLENDS		8	// max anim blends
#define MAXSTUDIOCONTROLLERS		32	// max controllers per model
#define MAXSTUDIOATTACHMENTS		32	// max attachments per model

// model global flags
#define STUDIO_STATIC		0x0001	// model without anims
#define STUDIO_RAGDOLL		0x0002	// ragdoll animation pose

// lighting & rendermode options
#define STUDIO_NF_FLATSHADE		0x0001
#define STUDIO_NF_CHROME		0x0002
#define STUDIO_NF_FULLBRIGHT		0x0004
#define STUDIO_NF_RESERVED		0x0008	// reserved
#define STUDIO_NF_BLENDED		0x0010	// rendering as semiblended
#define STUDIO_NF_ADDITIVE		0x0020	// rendering with additive mode
#define STUDIO_NF_TRANSPARENT		0x0040	// use texture with alpha channel

// motion flags
#define STUDIO_X			0x0001
#define STUDIO_Y			0x0002	
#define STUDIO_Z			0x0004
#define STUDIO_XR			0x0008
#define STUDIO_YR			0x0010
#define STUDIO_ZR			0x0020
#define STUDIO_LX			0x0040
#define STUDIO_LY			0x0080
#define STUDIO_LZ			0x0100
#define STUDIO_AX			0x0200
#define STUDIO_AY			0x0400
#define STUDIO_AZ			0x0800
#define STUDIO_AXR			0x1000
#define STUDIO_AYR			0x2000
#define STUDIO_AZR			0x4000
#define STUDIO_TYPES		0x7FFF
#define STUDIO_RLOOP		0x8000	// controller that wraps shortest distance

// sequence flags
#define STUDIO_LOOPING		0x0001

// render flags
#define STUDIO_RENDER		0x0001
#define STUDIO_EVENTS		0x0002

// bone flags
#define STUDIO_HAS_NORMALS		0x0001
#define STUDIO_HAS_VERTICES		0x0002
#define STUDIO_HAS_BBOX		0x0004
#define STUDIO_HAS_CHROME		0x0008	// if any of the textures have chrome on them

typedef struct 
{
	int		id;
	int		version;

	char		name[64];
	int		length;

	vec3_t		eyeposition;	// ideal eye position
	vec3_t		min;		// ideal movement hull size
	vec3_t		max;			

	vec3_t		bbmin;		// clipping bounding box
	vec3_t		bbmax;		

	int		flags;

	int		numbones;		// bones
	int		boneindex;

	int		numbonecontrollers;	// bone controllers
	int		bonecontrollerindex;

	int		numhitboxes;	// complex bounding boxes
	int		hitboxindex;			
	
	int		numseq;		// animation sequences
	int		seqindex;

	int		numseqgroups;	// demand loaded sequences
	int		seqgroupindex;

	int		numtextures;	// raw textures
	int		textureindex;
	int		texturedataindex;

	int		numskinref;	// replaceable textures
	int		numskinfamilies;
	int		skinindex;

	int		numbodyparts;		
	int		bodypartindex;

	int		numattachments;	// queryable attachable points
	int		attachmentindex;

	int		soundtable;
	int		soundindex;
	int		soundgroups;
	int		soundgroupindex;

	int		numtransitions;	// animation node to animation node transition graph
	int		transitionindex;
} studiohdr_t;

// header for demand loaded sequence group data
typedef struct 
{
	int		id;
	int		version;

	char		name[64];
	int		length;
} studioseqhdr_t;

// bones
typedef struct 
{
	char		name[32];		// bone name for symbolic links
	int		parent;		// parent bone
	int		flags;		// ??
	int		bonecontroller[6];	// bone controller index, -1 == none
	float		value[6];		// default DoF values
	float		scale[6];		// scale for delta DoF values
} mstudiobone_t;

// bone controllers
typedef struct 
{
	int		bone;		// -1 == 0
	int		type;		// X, Y, Z, XR, YR, ZR, M
	float		start;
	float		end;
	int		rest;		// byte index value at rest
	int		index;		// 0-3 user set controller, 4 mouth
} mstudiobonecontroller_t;

// intersection boxes
typedef struct
{
	int		bone;
	int		group;		// intersection group
	vec3_t		bbmin;		// bounding box
	vec3_t		bbmax;		
} mstudiobbox_t;

typedef struct cache_user_s
{
	void *data;
} cache_user_t;

// demand loaded sequence groups
typedef struct
{
	char		label[32];	// textual name
	char		name[64];		// file name
	void*		cache;		// cache index pointer
	int		data;		// hack for group 0
} mstudioseqgroup_t;

// sequence descriptions
typedef struct
{
	char		label[32];	// sequence label (name)

	float		fps;		// frames per second	
	int		flags;		// looping/non-looping flags

	int		activity;
	int		actweight;

	int		numevents;
	int		eventindex;

	int		numframes;	// number of frames per sequence

	int		numpivots;	// number of foot pivots
	int		pivotindex;

	int		motiontype;	
	int		motionbone;
	vec3_t		linearmovement;
	int		automoveposindex;
	int		automoveangleindex;

	vec3_t		bbmin;		// per sequence bounding box
	vec3_t		bbmax;		

	int		numblends;
	int		animindex;	// mstudioanim_t pointer relative to start of sequence group data
					// [blend][bone][X, Y, Z, XR, YR, ZR]

	int		blendtype[2];	// X, Y, Z, XR, YR, ZR
	float		blendstart[2];	// starting value
	float		blendend[2];	// ending value
	int		blendparent;

	int		seqgroup;		// sequence group for demand loading

	int		entrynode;	// transition node at entry
	int		exitnode;		// transition node at exit
	int		nodeflags;	// transition rules
	
	int		nextseq;		// auto advancing sequences
} mstudioseqdesc_t;

// events
typedef struct 
{
	int 		frame;
	int		event;
	int		type;
	char		options[64];
} mstudioevent_t;


// pivots
typedef struct 
{
	vec3_t		org;		// pivot point
	int		start;
	int		end;
} mstudiopivot_t;

// attachment
typedef struct 
{
	char		name[32];
	int		type;
	int		bone;
	vec3_t		org;		// attachment point
	vec3_t		vectors[3];
} mstudioattachment_t;

typedef struct
{
	unsigned short	offset[6];
} mstudioanim_t;

// animation frames
typedef union 
{
	struct
	{
		byte	valid;
		byte	total;
	} num;
	short		value;
} mstudioanimvalue_t;


// body part index
typedef struct
{
	char		name[64];
	int		nummodels;
	int		base;
	int		modelindex;	// index into models array
} mstudiobodyparts_t;


// skin info
typedef struct
{
	char		name[64];
	int		flags;
	int		width;
	int		height;
	int		index;
} mstudiotexture_t;

// skin families
// short	index[skinfamilies][skinref]		// skingroup info

// studio models
typedef struct
{
	char		name[64];

	int		type;
	float		boundingradius;

	int		nummesh;
	int		meshindex;

	int		numverts;		// number of unique vertices
	int		vertinfoindex;	// vertex bone info
	int		vertindex;	// vertex vec3_t
	int		numnorms;		// number of unique surface normals
	int		norminfoindex;	// normal bone info
	int		normindex;	// normal vec3_t

	int		numgroups;	// deformation groups
	int		groupindex;
} mstudiomodel_t;

// meshes
typedef struct 
{
	int		numtris;
	int		triindex;
	int		skinref;
	int		numnorms;		// per mesh normals
	int		normindex;	// normal vec3_t
} mstudiomesh_t;

/*
==============================================================================

BRUSH MODELS
==============================================================================
*/

//header
#define BSPMOD_VERSION	38
#define IDBSPMODHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I') // little-endian "IBSP"

// 16 bit short limits
#define MAX_MAP_SIZE		8192	// -/+ 4096
#define MAX_KEY			32
#define MAX_MAP_AREAS		256
#define MAX_VALUE			1024
#define MAX_MAP_MODELS		2048	// mesh models and sprites too
#define MAX_MAP_AREAPORTALS		1024
#define MAX_MAP_ENTITIES		2048
#define MAX_MAP_TEXINFO		8192
#define MAX_MAP_BRUSHES		16384
#define MAX_MAP_PLANES		65536
#define MAX_MAP_NODES		65536
#define MAX_MAP_BRUSHSIDES		65536
#define MAX_MAP_LEAFS		65536
#define MAX_MAP_VERTS		65536
#define MAX_MAP_FACES		65536
#define MAX_MAP_LEAFFACES		65536
#define MAX_MAP_LEAFBRUSHES		65536
#define MAX_MAP_PORTALS		65536
#define MAX_MAP_EDGES		128000
#define MAX_MAP_SURFEDGES		256000
#define MAX_MAP_ENTSTRING		0x40000
#define MAX_MAP_LIGHTING		0x400000
#define MAX_MAP_VISIBILITY		0x100000

//lump offset
#define LUMP_ENTITIES		0
#define LUMP_PLANES			1
#define LUMP_VERTEXES		2
#define LUMP_VISIBILITY		3
#define LUMP_NODES			4
#define LUMP_TEXINFO		5
#define LUMP_FACES			6
#define LUMP_LIGHTING		7
#define LUMP_LEAFS			8
#define LUMP_LEAFFACES		9
#define LUMP_LEAFBRUSHES		10
#define LUMP_EDGES			11
#define LUMP_SURFEDGES		12
#define LUMP_MODELS			13
#define LUMP_BRUSHES		14
#define LUMP_BRUSHSIDES		15
#define LUMP_POP			16	// unused
#define LUMP_AREAS			17
#define LUMP_AREAPORTALS		18
#define HEADER_LUMPS		19


// the visibility lump consists of a header with a count, then
// byte offsets for the PVS and PHS of each cluster, then the raw
// compressed bit vectors
#define DVIS_PVS			0
#define DVIS_PHS			1

//other limits
#define MAXLIGHTMAPS		4

typedef struct
{
	int	ident;
	int	version;	
	lump_t	lumps[HEADER_LUMPS];
} dheader_t;

typedef struct
{
	float	mins[3], maxs[3];
	float	origin[3];	// for sounds or lights
	int	headnode;
	int	firstface;	// submodels just draw faces 
	int	numfaces;		// without walking the bsp tree
} dmodel_t;

typedef struct
{
	float	point[3];
} dvertex_t;

typedef struct
{
	float	normal[3];
	float	dist;
	int	type;		// PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate
} dplane_t;

typedef struct
{
	int	planenum;
	int	children[2];	// negative numbers are -(leafs+1), not nodes
	short	mins[3];		// for frustom culling
	short	maxs[3];
	word	firstface;
	word	numfaces;		// counting both sides
} dnode_t;

typedef struct texinfo_s
{
	float	vecs[2][4];	// [s/t][xyz offset]
	int	flags;		// miptex flags + overrides
	int	value;		// light emission, etc
	char	texture[32];	// texture name (textures/*.jpg)
	int	nexttexinfo;	// for animations, -1 = end of chain
} texinfo_t;

typedef struct
{
	word	v[2];		// vertex numbers
} dedge_t;


typedef struct
{
	word	planenum;
	short	side;
	int	firstedge;	// we must support > 64k edges
	short	numedges;	
	short	texinfo;

	// lighting info
	byte	styles[MAXLIGHTMAPS];
	int	lightofs;		// start of [numstyles*surfsize] samples
} dface_t;

typedef struct
{
	int	contents;		// OR of all brushes (not needed?)
	short	cluster;
	short	area;
	short	mins[3];		// for frustum culling
	short	maxs[3];

	word	firstleafface;
	word	numleaffaces;
	word	firstleafbrush;
	word	numleafbrushes;
} dleaf_t;

typedef struct
{
	word	planenum;		// facing out of the leaf
	short	texinfo;
} dbrushside_t;

typedef struct
{
	int	firstside;
	int	numsides;
	int	contents;
} dbrush_t;

typedef struct
{
	int	numclusters;
	int	bitofs[8][2];	// bitofs[numclusters][2]
} dvis_t;

typedef struct
{
	int	portalnum;
	int	otherarea;
} dareaportal_t;

typedef struct
{
	int	numareaportals;
	int	firstareaportal;
} darea_t;

/*
==============================================================================

MAP CONTENTS & SURFACES DESCRIPTION
==============================================================================
*/
#define PLANE_X			0	// 0-2 are axial planes
#define PLANE_Y			1
#define PLANE_Z			2
#define PLANE_ANYX			3
#define PLANE_ANYY			4
#define PLANE_ANYZ			5

// lower bits are stronger, and will eat weaker brushes completely
#define CONTENTS_NONE		0 	// just a mask for source tabulation
#define CONTENTS_SOLID		1 	// an eye is never valid in a solid
#define CONTENTS_WINDOW		2 	// translucent, but not watery
#define CONTENTS_AUX		4
#define CONTENTS_LAVA		8
#define CONTENTS_SLIME		16
#define CONTENTS_WATER		32
#define CONTENTS_MIST		64
#define LAST_VISIBLE_CONTENTS		64
#define CONTENTS_MUD		128	// not a "real" content property - used only for watertype

// remaining contents are non-visible, and don't eat brushes
#define CONTENTS_FOG		0x4000	//future expansion
#define CONTENTS_AREAPORTAL		0x8000
#define CONTENTS_PLAYERCLIP		0x10000
#define CONTENTS_MONSTERCLIP		0x20000
#define CONTENTS_CLIP		CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP //both type clip
#define CONTENTS_CURRENT_0		0x40000	// currents can be added to any other contents, and may be mixed
#define CONTENTS_CURRENT_90		0x80000
#define CONTENTS_CURRENT_180		0x100000
#define CONTENTS_CURRENT_270		0x200000
#define CONTENTS_CURRENT_UP		0x400000
#define CONTENTS_CURRENT_DOWN		0x800000
#define CONTENTS_ORIGIN		0x1000000	// removed before bsping an entity
#define CONTENTS_MONSTER		0x2000000	// should never be on a brush, only in game
#define CONTENTS_DEADMONSTER		0x4000000
#define CONTENTS_DETAIL		0x8000000	// brushes to be added after vis leafs
#define CONTENTS_TRANSLUCENT		0x10000000// auto set if any surface has trans
#define CONTENTS_LADDER		0x20000000

//surfaces
#define SURF_NONE			0 	// just a mask for source tabulation
#define SURF_LIGHT			0x1	// value will hold the light strength
#define SURF_SLICK			0x2	// effects game physics
#define SURF_SKY			0x4	// don't draw, but add to skybox
#define SURF_WARP			0x8	// turbulent water warp
#define SURF_TRANS33		0x10
#define SURF_TRANS66		0x20
#define SURF_FLOWING		0x40	// scroll towards angle
#define SURF_NODRAW			0x80	// don't bother referencing the texture
#define SURF_HINT			0x100	// make a primary bsp splitter
#define SURF_SKIP			0x200	// completely ignore, allowing non-closed brushes
#define SURF_NULL			0x400	// remove face after compile
#define SURF_MIRROR			0x800	// remove face after compile
#define SURF_METAL			0x00000400// metal floor
#define SURF_DIRT			0x00000800// dirt, sand, rock
#define SURF_VENT			0x00001000// ventillation duct
#define SURF_GRATE			0x00002000// metal grating
#define SURF_TILE			0x00004000// floor tiles
#define SURF_GRASS      		0x00008000// grass
#define SURF_SNOW       		0x00010000// snow
#define SURF_CARPET     		0x00020000// carpet
#define SURF_FORCE      		0x00040000// forcefield
#define SURF_GRAVEL     		0x00080000// gravel
#define SURF_ICE			0x00100000// ice
#define SURF_STANDARD   		0x00200000// normal footsteps
#define SURF_STEPMASK		0x000FFC00

/*
==============================================================================

ENGINE TRACE FORMAT
==============================================================================
*/
#define SURF_PLANEBACK		2
#define SURF_DRAWSKY		4
#define SURF_DRAWTURB		0x10
#define SURF_DRAWBACKGROUND		0x40
#define SURF_UNDERWATER		0x80

typedef struct cplane_s
{
	vec3_t	normal;
	float	dist;
	byte	type;		// for fast side tests
	byte	signbits;		// signx + (signy<<1) + (signz<<1)
	byte	pad[2];
} cplane_t;


typedef struct cmodel_s
{
	int	modidx;		//edict index
	char	name[64];		//model name

	vec3_t	mins, maxs;	// boundbox
	vec3_t	origin;		// for sounds or lights
	int	headnode;		// bsp info

	int	numframes;	// sprite framecount
	void	*extradata;	// for studio models
} cmodel_t;

typedef struct csurface_s
{
	char	name[16];
	int	flags;
	int	value;

	// physics stuff
	int	firstedge;	// look up in model->surfedges[], negative numbers
	int	numedges;		// are backwards edges
} csurface_t;

typedef struct mapsurface_s
{
	csurface_t c;
	char	rname[32];
} mapsurface_t;

typedef struct trace_s
{
	bool		allsolid;		// if true, plane is not valid
	bool		startsolid;	// if true, the initial point was in a solid area
	bool		startstuck;	// trace started from solid entity
	float		fraction;		// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;		// final position
	cplane_t		plane;		// surface normal at impact
	csurface_t	*surface;		// surface hit
	int		contents;		// contents on other side of surface hit
	int		hitgroup;		// hit a studiomodel hitgroup #
	int		flags;		// misc trace flags
	edict_t		*ent;		// not set by CM_*() functions
} trace_t;

/*
==============================================================================

SAVE FILE
==============================================================================
*/
#define SAVE_VERSION	3
#define IDSAVEHEADER	(('E'<<24)+('V'<<16)+('A'<<8)+'S') // little-endian "SAVE"

#define LUMP_COMMENTS	0 // map comments
#define LUMP_CFGSTRING	1 // client info strings
#define LUMP_AREASTATE	2 // area portals state
#define LUMP_GAMESTATE	3 // progs global state (deflated)
#define LUMP_MAPNAME	4 // map name
#define LUMP_GAMECVARS	5 // contain game comment and all cvar state
#define LUMP_GAMEENTS	6 // ents state (deflated)
#define LUMP_SNAPSHOT	7 // tga image snapshot (128x128)
#define SAVE_NUMLUMPS	8 // header size

typedef struct
{
	int	ident;
	int	version;	
	lump_t	lumps[SAVE_NUMLUMPS];
} dsavehdr_t;

typedef struct
{
	char	name[MAX_QPATH];
	char	value[MAX_QPATH];
} dsavecvar_t;

#endif//REF_FORMAT_H