//=======================================================================
//			Copyright XashXT Group 2007 �
//		       r_mirror.h - realtime stencil mirror
//=======================================================================
#ifndef R_MODEL_H
#define R_MODEL_H

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

/*

  skins will be outline flood filled and mip mapped
  pics and sprites with alpha will be outline flood filled
  pic won't be mip mapped

  model skin
  sprite frame
  wall texture
  pic

*/

typedef enum 
{
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_sky,
	it_cubemap,
} imagetype_t;

// texnum cubemap order
// 0 = ft or normal image
// 1 = bk
// 2 = rt
// 3 = lf
// 4 = up
// 5 = dn

typedef struct image_s
{
	string		name;			// game path, including extension
	imagetype_t	type;			// image type
	int		width, height;		// source image
	int		registration_sequence;	// 0 = free
	struct msurface_s	*texturechain;		// for sort-by-texture world drawing
	int		texnum[6];		// gl texture binding
	int		texorder[6];		// drawing order pattern

	int		lumatex[6];		// luminescence cubemaps ?
	int		normtex[6];
} image_t;

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/
typedef struct mspriteframe_s
{
	int		width;
	int		height;
	float		up, down, left, right;
	int		texnum;
} mspriteframe_t;

typedef struct
{
	int		numframes;
	float		*intervals;
	mspriteframe_t	*frames[1];
} mspritegroup_t;

typedef struct
{
	frametype_t	type;
	mspriteframe_t	*frameptr;
} mspriteframedesc_t;

typedef struct
{
	int		type;
	int		rendermode;
	int		numframes;
	mspriteframedesc_t	frames[1];
} msprite_t;

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/


//
// in memory representation
//
typedef struct
{
	vec3_t		position;
} mvertex_t;

typedef struct
{
	vec3_t		mins, maxs;
	float		radius;
	int		headnode;
	int		visleafs;		// not including the solid leaf 0
	int		firstface, numfaces;
} msubmodel_t;

typedef struct
{
	unsigned short	v[2];
	unsigned int	cachededgeoffset;
} medge_t;

typedef struct mtexinfo_s
{
	float		vecs[2][4];
	int		size[2];
	int		flags;
	int		numframes;
	int		texid;		// save texture id
	struct mtexinfo_s	*next;		// animation chain
	image_t		*image;
} mtexinfo_t;

#define	VERTEXSIZE	7

typedef struct glpoly_s
{
	struct	glpoly_s	*next;
	struct	glpoly_s	*chain;
	int		numverts;
	int		flags;			// for SURF_UNDERWATER (not needed anymore?)
	float	verts[4][VERTEXSIZE];	// variable sized (xyz s1t1 s2t2)
} glpoly_t;

typedef struct msurface_s
{
	int			visframe;		// should be drawn when node is crossed

	cplane_t	*plane;
	int			flags;

	int			firstedge;	// look up in model->surfedges[], negative numbers
	int			numedges;	// are backwards edges
	
	short		texturemins[2];
	short		extents[2];

	int			light_s, light_t;	// gl lightmap coordinates
	int			dlight_s, dlight_t; // gl lightmap coordinates for dynamic lightmaps

	glpoly_t	*polys;				// multiple if warped
	struct	msurface_s	*texturechain;
	struct  msurface_s	*lightmapchain;

	mtexinfo_t	*texinfo;
	
// lighting info
	int			dlightframe;
	int			dlightbits;

	int			lightmaptexturenum;
	byte		styles[MAXLIGHTMAPS];
	float		cached_light[MAXLIGHTMAPS];	// values currently used in lightmap
	byte		*samples;		// [numstyles*surfsize]
} msurface_t;

typedef struct mnode_s
{
// common with leaf
	int			contents;		// -1, to differentiate from leafs
	int			visframe;		// node needs to be traversed if current
	
	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

	// node specific
	cplane_t	*plane;
	struct mnode_s	*children[2];	

	unsigned short		firstsurface;
	unsigned short		numsurfaces;
} mnode_t;



typedef struct mleaf_s
{
// common with node
	int			contents;		// wil be a negative contents number
	int			visframe;		// node needs to be traversed if current

	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

// leaf specific
	int			cluster;
	int			area;

	msurface_t	**firstmarksurface;
	int			nummarksurfaces;
} mleaf_t;

//===================================================================

//
// Whole model
//
typedef struct rmodel_s
{
	string		name;

	int		registration_sequence;

	modtype_t		type;
	byte		*mempool;
	
	int		numframes;
	
	int		flags;

//
// volume occupied by the model graphics
//		
	vec3_t		mins, maxs;
	float		radius;

//
// solid volume for clipping 
//
	bool		clipbox;
	vec3_t		clipmins, clipmaxs;

	// simple lighting for sprites and models
	vec3_t		lightcolor;

//
// brush model	//move this to brush_t struct
//
	int		firstmodelsurface, nummodelsurfaces;
	int		lightmap;		// only for submodels

	int		numsubmodels;
	msubmodel_t	*submodels;

	int		numplanes;
	cplane_t		*planes;

	int		numleafs;		// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int		numvertexes;
	mvertex_t		*vertexes;

	int		numedges;
	medge_t		*edges;

	int		numnodes;
	int		firstnode;
	mnode_t		*nodes;

	int		numtexinfo;
	mtexinfo_t	*texinfo;

	int		numsurfaces;
	msurface_t	*surfaces;

	int		numsurfedges;
	int		*surfedges;

	int		nummarksurfaces;
	msurface_t	**marksurfaces;

	dvis_t		*vis;

	byte		*lightdata;

	// stringtable system
	char		*stringdata;
	int		*stringtable;

	image_t		*skins[512];
          
          studiohdr_t	*phdr;
          studiohdr_t	*thdr;
	
	void	*extradata;

	//sprite auto animating
	float	frame;
	float	animtime;
	float	prevanimtime;
} rmodel_t;

//============================================================================

void	Mod_Init (void);
void	Mod_ClearAll (void);
rmodel_t *Mod_ForName ( const char *name, bool crash);
mleaf_t *Mod_PointInLeaf (float *p, rmodel_t *model);
byte	*Mod_ClusterPVS (int cluster, rmodel_t *model);
const char *Mod_GetStringFromTable( int index );

void	Mod_Modellist_f (void);
void	Mod_FreeAll (void);
void	Mod_Free (rmodel_t *mod);

extern rmodel_t *loadmodel;

int R_StudioExtractBbox( studiohdr_t *phdr, int sequence, float *mins, float *maxs );

#endif//R_MODEL_H