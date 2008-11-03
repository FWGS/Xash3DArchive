//=======================================================================
//			Copyright XashXT Group 2007 �
//		        r_local.h - render internal types
//=======================================================================

#ifndef R_LOCAL_H
#define R_LOCAL_H

#include <windows.h>
#include "launch_api.h"
#include "ref_dllapi.h"
#include "r_opengl.h"

extern stdlib_api_t com;		// engine toolbox
extern render_imp_t	 ri;
extern byte *r_temppool;
#define Host_Error			com.error

// limits
#define MAX_TEXTURE_UNITS		8
#define MAX_LIGHTMAPS		128
#define MAX_PROGRAMS		512
#define MAX_SHADERS			1024
#define MAX_ENTITIES		1024
#define MAX_VERTEX_BUFFERS		2048
#define MAX_POLYS			4096
#define MAX_POLY_VERTS		16384
#define MAX_CLIPFLAGS		15	// all sides of bbox are valid

/*
=======================================================================

 TEXTURE MANAGER

=======================================================================
*/

#define MAX_TEXTURES		8192
#define TEXTURES_HASH_SIZE		2048

typedef enum
{
	TF_STATIC		= BIT(0),
	TF_NOPICMIP	= BIT(1),
	TF_UNCOMPRESSED	= BIT(2),
	TF_INTENSITY	= BIT(3),
	TF_ALPHA		= BIT(4),
	TF_NORMALMAP	= BIT(5),
	TF_GEN_MIPS	= BIT(6),
	TF_CUBEMAP	= BIT(7),
	TF_SKYBOX		= BIT(8),
} texFlags_t;

typedef enum
{
	TF_DEFAULT = 0,
	TF_NEAREST,
	TF_LINEAR
} texFilter_t;

typedef enum
{
	TW_REPEAT = 0,
	TW_CLAMP,
	TW_CLAMP_TO_ZERO,
	TW_CLAMP_TO_ZERO_ALPHA
} texWrap_t;

typedef struct texture_s
{
	string		name;		// game path, including extension
	int		srcWidth;		// source dims, used for mipmap loading
	int		srcHeight;
	int		numLayers;	// framecount

	int		width;		// upload width\height
	int		height;

	texFlags_t	flags;
	texFilter_t	filter;
	texWrap_t		wrap;
	pixformat_t	type;		// PFDesc[type].glType
	size_t		size;		// upload size for debug targets

	GLint		format;		// PFDesc[type].glType
	GLuint		target;		// glTarget
	GLint		texnum;		// gl texture binding
	GLint		samples;		// gl samples

	int		touchFrame;	// 0 = free
	struct texture_s	*nextHash;
} texture_t;

extern texture_t	*r_defaultTexture;
extern texture_t	*r_whiteTexture;
extern texture_t	*r_blackTexture;
extern texture_t	*r_rawTexture;
extern texture_t	*r_dlightTexture;
extern texture_t	*r_lightmapTextures[MAX_LIGHTMAPS];
extern texture_t	*r_normalizeTexture;
extern texture_t	*r_radarMap;
extern texture_t	*r_aroundMap;
extern byte	*r_framebuffer;

void		R_SetTextureParameters( void );
void		R_TextureList_f( void );
texture_t		*R_LoadTexture( const char *name, rgbdata_t *pic, int samples, texFlags_t flags, texFilter_t filter, texWrap_t wrap );
texture_t		*R_FindTexture( const char *name, const byte *buf, size_t size, texFlags_t flags, texFilter_t filter, texWrap_t wrap );
texture_t		*R_FindCubeMapTexture( const char *name, const byte *buf, size_t size, texFlags_t flags, texFilter_t filter, texWrap_t wrap, bool skybox );
void		R_InitTextures( void );
void		R_ShutdownTextures( void );
void		R_ImageFreeUnused( void );

/*
 =======================================================================

 PROGRAM MANAGER

 =======================================================================
*/

typedef struct program_s
{
	string		name;

	uint		target;
	uint		progNum;

	struct program_s	*nextHash;
} program_t;

extern program_t	*r_defaultVertexProgram;
extern program_t	*r_defaultFragmentProgram;

void		R_ProgramList_f( void );
program_t		*R_FindProgram( const char *name, uint target );
void		R_InitPrograms( void );
void		R_ShutdownPrograms( void );

/*
 =======================================================================

 SHADERS

 =======================================================================
*/
#define SHADER_MAX_EXPRESSIONS		16
#define SHADER_MAX_STAGES			8
#define SHADER_MAX_DEFORMVERTEXES		8
#define SHADER_MAX_TEXTURES			16
#define SHADER_MAX_TCMOD			8

// Shader types used for shader loading
typedef enum
{
	SHADER_SKY,			// sky box shader
	SHADER_FONT,			// speical case for displayed fonts
	SHADER_NOMIP,			// 2d images
	SHADER_TEXTURE,			// bsp polygon
	SHADER_STUDIO,			// studio skins
	SHADER_SPRITE,			// sprite frames
	SHADER_GENERIC			// generic shader
} shaderType_t;

// shader flags
#define SHADER_EXTERNAL			0x00000001
#define SHADER_DEFAULTED			0x00000002
#define SHADER_HASLIGHTMAP			0x00000004
#define SHADER_SURFACEPARM			0x00000008
#define SHADER_NOMIPMAPS			0x00000010
#define SHADER_NOPICMIP			0x00000020
#define SHADER_NOCOMPRESS			0x00000040
#define SHADER_NOSHADOWS			0x00000080
#define SHADER_NOFRAGMENTS			0x00000100
#define SHADER_ENTITYMERGABLE			0x00000200
#define SHADER_POLYGONOFFSET			0x00000400
#define SHADER_CULL				0x00000800
#define SHADER_SORT				0x00001000
#define SHADER_TESSSIZE			0x00002000
#define SHADER_SKYPARMS			0x00004000
#define SHADER_DEFORMVERTEXES			0x00008000

// shader stage flags
#define SHADERSTAGE_NEXTBUNDLE		0x00000001
#define SHADERSTAGE_VERTEXPROGRAM		0x00000002
#define SHADERSTAGE_FRAGMENTPROGRAM		0x00000004
#define SHADERSTAGE_ALPHAFUNC			0x00000008
#define SHADERSTAGE_BLENDFUNC			0x00000010
#define SHADERSTAGE_DEPTHFUNC			0x00000020
#define SHADERSTAGE_DEPTHWRITE		0x00000040
#define SHADERSTAGE_DETAIL			0x00000080
#define SHADERSTAGE_RGBGEN			0x00000100
#define SHADERSTAGE_ALPHAGEN			0x00000200

// stage bundle flags
#define STAGEBUNDLE_NOMIPMAPS			0x00000001
#define STAGEBUNDLE_NOPICMIP			0x00000002
#define STAGEBUNDLE_NOCOMPRESS		0x00000004
#define STAGEBUNDLE_CLAMPTEXCOORDS		0x00000008
#define STAGEBUNDLE_ANIMFREQUENCY		0x00000010
#define STAGEBUNDLE_MAP			0x00000020
#define STAGEBUNDLE_BUMPMAP			0x00000040
#define STAGEBUNDLE_CUBEMAP			0x00000080
#define STAGEBUNDLE_VIDEOMAP			0x00000100
#define STAGEBUNDLE_TEXENVCOMBINE		0x00000200
#define STAGEBUNDLE_TCGEN			0x00000400
#define STAGEBUNDLE_TCMOD			0x00000800

typedef enum
{
	WAVEFORM_SIN,
	WAVEFORM_TRIANGLE,
	WAVEFORM_SQUARE,
	WAVEFORM_SAWTOOTH,
	WAVEFORM_INVERSESAWTOOTH,
	WAVEFORM_NOISE
} waveForm_t;

typedef enum
{
	SORT_SKY			= 1,
	SORT_OPAQUE,
	SORT_DECAL,
	SORT_SEETHROUGH,
	SORT_BANNER,
	SORT_UNDERWATER,
	SORT_WATER,
	SORT_INNERBLEND,
	SORT_BLEND,
	SORT_BLEND2,
	SORT_BLEND3,
	SORT_BLEND4,
	SORT_OUTERBLEND,
	SORT_ADDITIVE,
	SORT_NEAREST
} sort_t;

typedef enum
{
	DEFORMVERTEXES_WAVE,
	DEFORMVERTEXES_MOVE,
	DEFORMVERTEXES_NORMAL,
} deformVertsType_t;

typedef enum
{
	TCGEN_BASE,
	TCGEN_LIGHTMAP,
	TCGEN_ENVIRONMENT,
	TCGEN_VECTOR,
	TCGEN_WARP,
	TCGEN_LIGHTVECTOR,
	TCGEN_HALFANGLE,
	TCGEN_REFLECTION,
	TCGEN_NORMAL
} tcGenType_t;

typedef enum
{
	TCMOD_TRANSLATE,
	TCMOD_SCALE,
	TCMOD_SCROLL,
	TCMOD_ROTATE,
	TCMOD_STRETCH,
	TCMOD_TURB,
	TCMOD_TRANSFORM
} tcModType_t;

typedef enum
{
	RGBGEN_IDENTITY,
	RGBGEN_IDENTITYLIGHTING,
	RGBGEN_WAVE,
	RGBGEN_COLORWAVE,
	RGBGEN_VERTEX,
	RGBGEN_ONEMINUSVERTEX,
	RGBGEN_ENTITY,
	RGBGEN_ONEMINUSENTITY,
	RGBGEN_LIGHTINGAMBIENT,
	RGBGEN_LIGHTINGDIFFUSE,
	RGBGEN_CONST
} rgbGenType_t;

typedef enum
{
	ALPHAGEN_IDENTITY,
	ALPHAGEN_WAVE,
	ALPHAGEN_ALPHAWAVE,
	ALPHAGEN_VERTEX,
	ALPHAGEN_ONEMINUSVERTEX,
	ALPHAGEN_ENTITY,
	ALPHAGEN_ONEMINUSENTITY,
	ALPHAGEN_DOT,
	ALPHAGEN_ONEMINUSDOT,
	ALPHAGEN_FADE,
	ALPHAGEN_ONEMINUSFADE,
	ALPHAGEN_LIGHTINGSPECULAR,
	ALPHAGEN_CONST
} alphaGenType_t;

typedef enum
{
	TEX_GENERIC,
	TEX_LIGHTMAP,
	TEX_CINEMATIC
} texType_t;

typedef struct
{
	waveForm_t	type;
	float		params[4];
} waveFunc_t;

typedef struct
{
	GLenum		mode;
} cull_t;

typedef struct
{
	texture_t		*farBox[6];
	float		cloudHeight;
	texture_t		*nearBox[6];
} skyParms_t;

typedef struct
{
	deformVertsType_t	type;
	waveFunc_t	func;
	float		params[3];
} deformVerts_t;

typedef struct
{
	GLint		rgbCombine;
	GLint		rgbSource[3];
	GLint		rgbOperand[3];
	GLint		rgbScale;

	GLint		alphaCombine;
	GLint		alphaSource[3];
	GLint		alphaOperand[3];
	GLint		alphaScale;

	GLfloat		constColor[4];
} texEnvCombine_t;

typedef struct
{
	tcGenType_t	type;
	float		params[6];
} tcGen_t;

typedef struct
{
	tcModType_t	type;
	waveFunc_t	func;
	float		params[6];
} tcMod_t;

typedef struct
{
	GLenum		func;
	GLclampf		ref;
} alphaFunc_t;

typedef struct
{
	GLenum		src;
	GLenum		dst;
} blendFunc_t;

typedef struct
{
	GLenum		func;
} depthFunc_t;

typedef struct
{
	rgbGenType_t	type;
	waveFunc_t	func;
	float		params[3];
} rgbGen_t;

typedef struct
{
	alphaGenType_t	type;
	waveFunc_t	func;
	float		params[3];
} alphaGen_t;

typedef struct
{
	texType_t		texType;
	uint		flags;

	texture_t		*textures[SHADER_MAX_TEXTURES];
	uint		numTextures;

	float		animFrequency;
	video_t		cinematicHandle;

	GLint		texEnv;
	texEnvCombine_t	texEnvCombine;

	tcGen_t		tcGen;
	tcMod_t		tcMod[SHADER_MAX_TCMOD];
	uint		tcModNum;
} stageBundle_t;

typedef struct
{
	bool		ignore;
	uint		flags;

	stageBundle_t	*bundles[MAX_TEXTURE_UNITS];
	uint		numBundles;

	program_t		*vertexProgram;
	program_t		*fragmentProgram;

	alphaFunc_t	alphaFunc;
	blendFunc_t	blendFunc;
	depthFunc_t	depthFunc;

	rgbGen_t		rgbGen;
	alphaGen_t	alphaGen;
} shaderStage_t;

typedef struct ref_shader_s
{
	string		name;
	int		shaderNum;
	shaderType_t	shaderType;
	uint		surfaceParm;
	uint		contentFlags;
	uint		flags;

	cull_t		cull;
	sort_t		sort;
	uint		tessSize;
	skyParms_t	skyParms;
	deformVerts_t	deformVertexes[SHADER_MAX_DEFORMVERTEXES];
	uint		deformVertexesNum;
	shaderStage_t	*stages[SHADER_MAX_STAGES];
	uint	   	numStages;
	struct ref_shader_s	*nextHash;
} ref_shader_t;

extern ref_shader_t	*r_shaders[MAX_SHADERS];
extern int	r_numShaders;
extern ref_shader_t	*r_defaultShader;
extern ref_shader_t	*r_lightmapShader;
extern ref_shader_t	*r_waterCausticsShader;
extern ref_shader_t	*r_slimeCausticsShader;
extern ref_shader_t	*r_lavaCausticsShader;

shader_t	R_FindShader( const char *name, shaderType_t shaderType, uint surfaceParm );
void	R_SetInternalMap( texture_t *mipTex );		// internal textures (skins, spriteframes, etc)
void	R_ShaderList_f( void );
void	R_InitShaders( void );
void	R_ShutdownShaders (void);

/*
=======================================================================

BRUSH MODELS

=======================================================================
*/
#define SURF_PLANEBACK		1 // fast surface culling
#define CONTENTS_NODE		-1
#define SKY_SIZE			8
#define SKY_INDICES			(SKY_SIZE * SKY_SIZE * 6)
#define SKY_VERTICES		((SKY_SIZE+1) * (SKY_SIZE+1))

#define SURF_WATERCAUSTICS		2
#define SURF_SLIMECAUSTICS		4
#define SURF_LAVACAUSTICS		8

typedef struct dlight_s
{
	vec3_t	origin;
	vec3_t	color;
	float	intensity;
} dlight_t;

typedef struct lightstyle_s
{
	float		rgb[3];		// 0.0 - 2.0
	float		white;		// highest of rgb
} lightstyle_t;

typedef struct particle_s
{
	ref_shader_t		*shader;
	vec3_t		origin;
	vec3_t		old_origin;
	float		radius;
	float		length;
	float		rotation;
	vec4_t		modulate;
} particle_t;

typedef struct
{
	vec3_t		xyz;
	vec2_t		st;
	vec2_t		lightmap;
	vec4_t		color;
} surfPolyVert_t;

typedef struct surfPoly_s
{
	struct surfPoly_s	*next;

	int		numIndices;
	int		numVertices;

	uint		*indices;
	surfPolyVert_t	*vertices;
} surfPoly_t;

typedef struct
{
	vec3_t		xyz;
	vec2_t		st;
	vec4_t		modulate;
} polyVert_t;

typedef struct
{
	ref_shader_t		*shader;
	int		numVerts;
	polyVert_t	*verts;
} poly_t;

typedef struct mipTex_s
{
	string		name;		// original miptex name
	ref_shader_t	*shader;		// texture shader
	int		contents;
	int		flags;
} mipTex_t;

typedef struct
{
	int		flags;

	int		firstIndex;	// Look up in model->edges[]. Negative
	int		numIndexes;		// numbers are backwards edges

	int		firstVertex;
	int		numVertexes;

	cplane_t		*plane;

	vec3_t		mins;
	vec3_t		maxs;

	surfPoly_t	*poly;			// multiple if subdivided

	vec3_t		tangent;
	vec3_t		binormal;
	vec3_t		normal;

	mipTex_t		*texInfo;

	int		visFrame;
	int		fragmentFrame;

	// lighting info
	int		dlightFrame;
	int		dlightBits;

	int		lmWidth;
	int		lmHeight;
	int		lmS;
	int		lmT;
	int		lmNum;
	float		lmVecs[2][3];	// lightmap vecs
	byte		*lmSamples;
	int		numStyles;
	byte		styles[MAX_LIGHTSTYLES];
	float		cachedLight[MAX_LIGHTSTYLES];	// values currently used in lightmap
} surface_t;

typedef struct node_s
{
	// common with leaf and node
	int		contents;		// -1, to differentiate from leafs
	int		visFrame;		// Node needs to be traversed if current
	vec3_t		mins, maxs;	// for bounding box culling
	struct node_s	*parent;

	// node specific
	cplane_t		*plane;
	struct node_s	*children[2];
	
	// common with leaf
	// leaf specific
	int		cluster;
	int		area;
	surface_t		**firstMarkSurface;
	int		numMarkSurfaces;
} node_t;

typedef struct
{
	int		numClusters;
	int		bitOfs[8][2];
} vis_t;

typedef struct
{

	vec3_t		point;
	vec3_t		normal;
	float		st[2];
	float		lm[2];
	vec4_t		color;	
} vertex_t;

typedef struct
{
	vec3_t		mins;
	vec3_t		maxs;
	vec3_t		origin;		// for sounds or lights
	float		radius;
	int		firstFace;
	int		numFaces;
} submodel_t;

typedef struct
{
	vec3_t		xyz;
	vec3_t		normal;
	vec2_t		st;
	vec2_t		sphere;
} skySideVert_t;

typedef struct
{
	int		numIndices;
	int		numVertices;
	uint		indices[SKY_INDICES];
	skySideVert_t	vertices[SKY_VERTICES];
} skySide_t;

typedef struct
{
	ref_shader_t	*shader;
	float		rotate;
	vec3_t		axis;
	float		mins[2][6];
	float		maxs[2][6];
	uint		vbo[6];
	skySide_t		skySides[6];
} sky_t;

typedef struct
{
	vec3_t		lightDir;
} lightGrid_t;

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
	float		radius;
	shader_t		shader;
	texture_t		*texture;
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

STUDIO MODELS

==============================================================================
*/
typedef struct
{
	uint		index[3];
} mstudiotriangle_t;

typedef struct
{
	int		index[3];
} mstudioneighbor_t;

typedef struct
{
	short		point[3];
	byte		tangent[2];
	byte		binormal[2];
	byte		normal[2];
} mstudiopoint_t;

typedef struct
{
	vec2_t		st;
} mstudiost_t;

typedef struct
{
	mstudiotriangle_t	*triangles;
	mstudioneighbor_t	*neighbors;
	mstudiopoint_t	*points;
	mstudiost_t	*st;
	ref_shader_t		*shaders;

	int		numTriangles;
	int		numVertices;
	int		numShaders;
} mstudiosurface_t;

typedef struct rmodel_s
{
	string		name;
	int		registration_sequence;

	modtype_t		type;
	byte		*mempool;

	// simple lighting for sprites and models
	vec3_t		lightcolor;
	int		flags;

	// volume occupied by the model
	vec3_t		mins;
	vec3_t		maxs;
	float		radius;

	// brush model
	int		numModelSurfaces;
	int		firstModelSurface;

	int		numSubmodels;
	submodel_t	*submodels;

	int		numVertexes;
	vertex_t		*vertexes;

	int		numIndexes;
	int		*indexes;

	int		numShaders;
	mipTex_t		*shaders;

	int		numSurfaces;
	surface_t		*surfaces;

	int		numMarkSurfaces;
	surface_t		**markSurfaces;

	int		numPlanes;
	cplane_t		*planes;

	int		numNodes;
	node_t		*nodes;			// also included leafs

	int		numClusters;		// used for create novis lump
	vis_t		*vis;			// may be passed in by CM_LoadMap to save space
	byte		*novis;			// clusterBytes of 0xff

	sky_t		*sky;
	byte		*lightMaps;
	int		numLightmaps;

	vec3_t		gridMins;
	vec3_t		gridSize;
	int		gridBounds[4];
	int		gridPoints;
	lightGrid_t	*lightGrid;

	// studio model
	dstudiohdr_t	*phdr;
          dstudiohdr_t	*thdr;
	mstudiosurface_t	*studiofaces;

	void		*extradata;	// model buffer

} rmodel_t;

typedef struct
{
	byte		open;		// 0 = mouth closed, 255 = mouth agape
	byte		sndcount;		// counter for running average
	int		sndavg;		// running average
} mouth_t;

typedef struct latchedvars_s
{
	float		animtime;
	float		sequencetime;
	vec3_t		origin;
	vec3_t		angles;		

	vec3_t		gaitorigin;
	int		sequence;
	float		frame;

	float		blending[MAXSTUDIOBLENDS];
	byte		seqblending[MAXSTUDIOBLENDS];
	float		controller[MAXSTUDIOCONTROLLERS];

} latchedvars_t;

// client entity
typedef struct ref_entity_s
{
	edtype_t		ent_type;		// entity type
	int		index;		// entity index
	rmodel_t		*model;		// opaque type outside refresh
	rmodel_t		*weaponmodel;	// opaque type outside refresh	

	latchedvars_t	prev;		// previous frame values for lerping
	
	vec3_t		angles;
	vec3_t		origin;		// position

	float		framerate;	// custom framerate
          float		animtime;		// lerping animtime	
	float		frame;		// also used as RF_BEAM's diameter

	int		body;
	int		skin;
	
	float		blending[MAXSTUDIOBLENDS];
	vec3_t		attachment[MAXSTUDIOATTACHMENTS];
	float		controller[MAXSTUDIOCONTROLLERS];
	mouth_t		mouth;		// for synchronizing mouth movements.
	
          int		movetype;		// entity moving type
	int		sequence;
	float		scale;
	
	// misc
	float		backlerp;		// 0.0 = current, 1.0 = old
	vec3_t		rendercolor;	// hl1 rendercolor
	float		renderamt;	// hl1 alphavalues
	int		renderfx;		// server will be translate hl1 values into flags
	int		colormap;		// q1 and hl1 model colormap (can applied for sprites)
	int		effects;		// q1 effect flags, EF_ROTATE, EF_DIMLIGHT etc

	// these values will be calculated locally, not from entity_state
	vec3_t		realangles;
	int		m_fSequenceLoops;
	int		m_fSequenceFinished;
	int		renderframe;	// using for gait cycle	
	int		gaitsequence;	// client->sequence + yaw
	float		gaitframe;	// client->frame + yaw
	float		gaityaw;		// local value

	// shader information
	ref_shader_t		*shader;
	float		shaderTime;	// subtracted from refdef time to control effect start times
	float		radius;		// bbox approximate radius
	vec3_t		axis[3];		// Rotation vectors
	float		rotation;		// what the hell ???
} ref_entity_t;

const char *R_GetStringFromTable( int index );
node_t	*R_PointInLeaf( const vec3_t p );
byte	*R_ClusterPVS( int cluster );

void	R_ModelList_f( void );
void	R_StudioInit( void );
void	R_StudioShutdown( void );
bool	R_StudioComputeBBox( vec3_t bbox[8] );	// for drawing bounds
void	R_StudioSetupModel( int body, int bodypart );
void	R_InitModels( void );
void	R_ShutdownModels( void );
rmodel_t	*Mod_ForName( const char *name, bool crash );
void	Mod_Free( rmodel_t *mod );

#define RENDERPASS_SOLID		1
#define RENDERPASS_ALPHA		2

/*
=======================================================================

DOOM1 STYLE AUTOMAP

=======================================================================
*/
#define MAX_RADAR_ENTS		1024

typedef struct radar_ent_s
{
	vec4_t	color;
	vec3_t	origin;  
	vec3_t	angles;
} radar_ent_t;

extern int numRadarEnts;
extern radar_ent_t RadarEnts[MAX_RADAR_ENTS];

/*
 =======================================================================

 GL STATE MANAGER

 =======================================================================
*/

typedef enum
{
	R_OPENGL_110 = 0,		// base
	R_SGIS_MIPMAPS_EXT,
	R_WGL_SWAPCONTROL,
	R_COMBINE_EXT,
	R_DRAW_RANGEELEMENTS_EXT,
	R_ARB_MULTITEXTURE,
	R_LOCKARRAYS_EXT,
	R_TEXTURE_3D_EXT,
	R_TEXTURECUBEMAP_EXT,
	R_DOT3_ARB_EXT,
	R_CLAMPTOEDGE_EXT,
	R_ANISOTROPY_EXT,
	R_BLEND_MINMAX_EXT,
	R_STENCILTWOSIDE_EXT,
	R_BLEND_SUBTRACT_EXT,
	R_SHADER_OBJECTS_EXT,
	R_SHADER_GLSL100_EXT,
	R_VERTEX_SHADER_EXT,	// glsl vertex program
	R_FRAGMENT_SHADER_EXT,	// glsl fragment program	
	R_VERTEX_PROGRAM_EXT,	// cg vertex program
	R_FRAGMENT_PROGRAM_EXT,	// cg fragment program
	R_EXT_POINTPARAMETERS,
	R_SEPARATESTENCIL_EXT,
	R_ARB_TEXTURE_NPOT_EXT,
	R_ARB_VERTEX_BUFFER_OBJECT_EXT,
	R_CUSTOM_VERTEX_ARRAY_EXT,
	R_TEXTURE_COMPRESSION_EXT,
	R_TEXTURE_ENV_ADD_EXT,
	R_CLAMP_TEXBORDER_EXT,
	R_TEXTURE_LODBIAS,
	R_EXTCOUNT
} r_opengl_extensions;

typedef struct glstate_s
{
	bool		orthogonal;
	word		gammaRamp[768];		// current gamma ramp
	word		stateRamp[768];		// original gamma ramp
	uint		screenTexture;

	vec4_t		draw_color;		// current color
	uint		activeTMU;
	int		texNum[MAX_TEXTURE_UNITS];
	int		texEnv[MAX_TEXTURE_UNITS];

	// render current state
	bool		cullFace;
	bool		polygonOffsetFill;
	bool		vertexProgram;
	bool		fragmentProgram;
	bool		alpha_test;
	bool		depth_test;
	bool		blend;

	// OpenGL current state
	GLenum		cullMode;
	GLfloat		offsetFactor;
	GLfloat		offsetUnits;
	GLenum		alphaFunc;
	GLclampf		alphaRef;
	GLenum		blendSrc;
	GLenum		blendDst;
	GLenum		depthFunc;
	GLboolean		depthMask;
	GLfloat		polygonoffset[2];
} glstate_t;

// contains constant values that are always
// setting by R_Init and using as read-only
typedef struct glconfig_s
{
	const char	*renderer_string;
	const char	*vendor_string;
	const char	*version_string;

	// list of supported extensions
	const char	*extensions_string;
	byte		extension[R_EXTCOUNT];
	uint		max_entities;

	int		textureunits;
	int		texturecoords;
	int		imageunits;
	GLint		max_2d_texture_size;
	GLint		max_2d_rectangle_size;
	GLint		max_3d_texture_size;
	GLint		max_cubemap_texture_size;
	GLfloat		max_anisotropy;
	GLfloat		max_lodbias;
	GLint		texRectangle;

	bool		deviceSupportsGamma;
	bool		fullscreen;
	int		prev_mode;
} glconfig_t;

extern glconfig_t  gl_config;
extern glstate_t   gl_state;

void		GL_InitBackend( void );
bool		GL_Support( int r_ext );
void		GL_InitExtensions( void );
void		GL_ShutdownBackend( void );
void		GL_SetExtension( int r_ext, int enable );
void		GL_SelectTexture( uint tmu );
void		GL_BindTexture( texture_t *texture );
void		GL_TexEnv( GLint texEnv );
void		GL_Enable( GLenum cap );
void		GL_Disable( GLenum cap );
void		GL_CullFace( GLenum mode );
void		GL_PolygonOffset( GLfloat factor, GLfloat units );
void		GL_AlphaFunc( GLenum func, GLclampf ref );
void		GL_BlendFunc( GLenum src, GLenum dst );
void		GL_DepthFunc( GLenum func );
void		GL_DepthMask( GLboolean mask );
void		GL_SetColor( const void *data );
void		GL_LoadMatrix( matrix4x4 source );
void		GL_SaveMatrix( GLenum target, matrix4x4 dest );
void		GL_SetDefaultState( void );
void		GL_BuildGammaTable( void );
void		GL_UpdateGammaRamp( void );
void		GL_Setup3D( void );
void		GL_Setup2D( void );

// simple gl interface
void		GL_Begin( GLuint drawMode );
void		GL_End( void );
void		GL_Vertex2f( GLfloat x, GLfloat y );
void		GL_Vertex3f( GLfloat x, GLfloat y, GLfloat z );
void		GL_Vertex3fv( const GLfloat *v );
void		GL_Normal3f( GLfloat x, GLfloat y, GLfloat z );
void		GL_Normal3fv( const GLfloat *v );
void		GL_TexCoord2f( GLfloat s, GLfloat t );
void		GL_TexCoord4f( GLfloat s, GLfloat t, GLfloat ls, GLfloat lt );
void		GL_TexCoord4fv( const GLfloat *v );
void		GL_Color3f( GLfloat r, GLfloat g, GLfloat b );
void		GL_Color3fv( const GLfloat *v );
void		GL_Color4f( GLfloat r, GLfloat g, GLfloat b, GLfloat a );
void		GL_Color4fv( const GLfloat *v );
void		GL_Color4ub( GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha );
void		GL_Color4ubv( const GLubyte *v );

void		R_CheckForErrors( void );
void		R_EndFrame( void );
bool		R_Init_OpenGL( void );
void		R_Free_OpenGL( void );
void		R_CheckForErrors( void );

/*
 =======================================================================

 BACKEND

 =======================================================================
*/

#define VBO_OFFSET(i)		((char *)NULL + (i))
#define MAX_INDICES			16384 * 3
#define MAX_VERTICES		8192
#define MAX_MESHES			32768

typedef struct
{
	uint		indexBuffer;
	uint		vertexBuffer;
	uint		normalBuffer;
	uint		colorBuffer;
	uint		texCoordBuffer[MAX_TEXTURE_UNITS];
} vbo_t;

typedef enum
{
	MESH_SKY,
	MESH_SURFACE,
	MESH_STUDIO,
	MESH_SPRITE,
	MESH_BEAM,
	MESH_PARTICLE,
	MESH_POLY
} meshType_t;

typedef struct
{
	uint		sortKey;
	meshType_t	meshType;
	void		*mesh;
} mesh_t;

extern vbo_t	rb_vbo;
extern int	m_iInfoKey;
extern float	m_fShaderTime;
extern rmodel_t	*m_pLoadModel;
extern mesh_t	*m_pRenderMesh;
extern rmodel_t	*m_pRenderModel;
extern ref_shader_t	*m_pCurrentShader;
extern ref_entity_t *m_pCurrentEntity;
extern int	m_iStudioModelCount;
extern int	registration_sequence;

extern uint	indexArray[MAX_INDICES * 4];
extern vec3_t	vertexArray[MAX_VERTICES * 2];
extern vec3_t	tangentArray[MAX_VERTICES];
extern vec3_t	binormalArray[MAX_VERTICES];
extern vec3_t	normalArray[MAX_VERTICES];
extern vec4_t	colorArray[MAX_VERTICES];
extern vec3_t	texCoordArray[MAX_TEXTURE_UNITS][MAX_VERTICES];
extern vec4_t	inColorArray[MAX_VERTICES];
extern vec4_t	inTexCoordArray[MAX_VERTICES];
extern int	numIndex;
extern int	numVertex;

void		RB_DebugGraphics( void );
void		RB_CheckMeshOverflow( int numIndices, int numVertices );
void		RB_RenderMesh( void );
void		RB_RenderMeshes( mesh_t *meshes, int numMeshes );
void		RB_DrawStretchPic( float x, float y, float w, float h, float sl, float tl, float sh, float th, ref_shader_t *shader );

void		RB_VBOInfo_f( void );
uint		RB_AllocStaticBuffer( uint target, int size );
uint		RB_AllocStreamBuffer( uint target, int size );
void		RB_InitBackend( void );
void		RB_ShutdownBackend( void );

typedef struct
{
	int	numShaders;
	int	numStages;
	int	numMeshes;
	int	numLeafs;
	int	numVertices;
	int	numIndices;
	int	totalIndices;

	int	numEntities;
	int	numDLights;
	int	numParticles;
	int	numPolys;
} refstats_t;

extern rmodel_t    	*r_worldModel;
extern ref_entity_t	*r_worldEntity;

extern vec3_t      	r_worldMins, r_worldMaxs;

extern int         	r_frameCount;
extern int         	r_visFrameCount;
extern int         	r_viewCluster;
extern int	r_areabitsChanged;
extern vec3_t	r_origin;				// same as r_refdef.vieworg
extern vec3_t	r_forward;
extern vec3_t	r_right;
extern vec3_t	r_up;

extern matrix4x4	r_worldMatrix;
extern matrix4x4	r_entityMatrix;

extern gl_matrix   	gl_projectionMatrix;
extern gl_matrix   	gl_entityMatrix;
extern gl_matrix	gl_textureMatrix;

extern cplane_t	r_frustum[4];
extern vec3_t	axisDefault[3];
extern float	r_frameTime;

extern mesh_t	r_solidMeshes[MAX_MESHES];
extern int	r_numSolidMeshes;
extern mesh_t	r_transMeshes[MAX_MESHES];
extern int	r_numTransMeshes;
extern ref_entity_t	r_entities[MAX_ENTITIES];
extern int	r_numEntities;
extern dlight_t	r_dlights[MAX_DLIGHTS];
extern int	r_numDLights;
extern particle_t	r_particles[MAX_PARTICLES];
extern int	r_numParticles;
extern poly_t	r_polys[MAX_POLYS];
extern int	r_numPolys;
extern polyVert_t	r_polyVerts[MAX_POLY_VERTS];
extern int	r_numPolyVerts;
extern ref_entity_t	*r_nullModels[MAX_ENTITIES];
extern int	r_numNullModels;

extern lightstyle_t	r_lightStyles[MAX_LIGHTSTYLES];
extern refdef_t	r_refdef;
extern refstats_t	r_stats;

void		R_DrawStudioModel( void );
void		R_AddStudioModelToList( ref_entity_t *entity );
void		R_StudioLoadModel( rmodel_t *mod, const void *buffer );

void		R_DrawSpriteModel( void );
void		R_AddSpriteModelToList( ref_entity_t *entity );
void		R_SpriteLoadModel( rmodel_t *mod, const void *buffer );
mspriteframe_t	*R_GetSpriteFrame( ref_entity_t *ent );

void		R_LightDir( const vec3_t origin, vec3_t lightDir );
void		R_LightForPoint( const vec3_t point, vec3_t ambientLight );
void		R_LightingAmbient( void );
void		R_LightingDiffuse( void );
void		R_BeginBuildingLightmaps( void );
void		R_EndBuildingLightmaps( void );
void		R_BuildSurfaceLightmap( surface_t *surf );
void		R_UpdateSurfaceLightmap( surface_t *surf );
bool		R_CullBox( const vec3_t mins, const vec3_t maxs, int clipFlags );
bool		R_CullSphere( const vec3_t origin, float radius, int clipFlags );
void		R_RotateForEntity( ref_entity_t *entity );
void		R_AddMeshToList( meshType_t meshType, void *mesh, ref_shader_t *shader, ref_entity_t *entity, int infoKey );
void		R_DrawSprite( void );
void		R_DrawBeam( void );
void		R_DrawParticle( void );
void		R_DrawPoly( void );
void		R_RenderView( const refdef_t *fd );
void		R_AddShadowToList( ref_entity_t *entity );
void		R_RenderShadows( void );
void		R_BloomBlend ( const refdef_t *fd );
void		R_DrawSky( void );
void		R_ClearSky( void );
void		R_ClipSkySurface( surface_t *surf );
void		R_AddSkyToList( void );

void		R_DrawSurface( void );
void		R_AddBrushModelToList( ref_entity_t *entity );
void		R_AddWorldToList( void );

void		RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );
void		R_DXTReadColor( word data, color32* out );
void		R_DXTReadColors( const byte* data, color32* out );
void		R_GetBitsFromMask( uint Mask, uint *ShiftLeft, uint *ShiftRight );

// exported funcs
void		R_BeginRegistration( const char *map );
rmodel_t		*R_RegisterModel( const char *name );
void		R_SetupSky( const char *name, float rotate, const vec3_t axis );
void		R_EndRegistration( void );
void		R_ShaderRegisterImages( rmodel_t *mod );	// prolonge registration
ref_shader_t		*R_RegisterShader( const char *name );
ref_shader_t		*R_RegisterShaderSkin( const char *name );
ref_shader_t		*R_RegisterShaderNoMip( const char *name );
bool		VID_ScreenShot( const char *filename, bool levelshot );
void		R_DrawFill( float x, float y, float w, float h );
void		R_DrawStretchRaw( int x, int y, int w, int h, int width, int height, const byte *raw, bool dirty );
void		R_DrawStretchPic( float x, float y, float w, float h, float sl, float tl, float sh, float th, const char *name );
void		R_GetPicSize( int *w, int *h, const char *pic );

// r_utils.c (test)
void AxisClear( vec3_t axis[3] );
void AnglesToAxis ( const vec3_t angles );
void AxisCopy( const vec3_t in[3], vec3_t out[3] );
void AnglesToAxisPrivate (const vec3_t angles, vec3_t axis[3]);
bool AxisCompare( const vec3_t axis1[3], const vec3_t axis2[3] );
void VectorRotate ( const vec3_t v, const vec3_t matrix[3], vec3_t out );
void MatrixGL_MultiplyFast (const gl_matrix m1, const gl_matrix m2, gl_matrix out);

// cvars
extern cvar_t	*r_check_errors;
extern cvar_t	*r_himodels;
extern cvar_t	*r_norefresh;
extern cvar_t	*r_novis;
extern cvar_t	*r_nocull;
extern cvar_t	*r_nobind;
extern cvar_t	*r_drawworld;
extern cvar_t	*r_drawentities;
extern cvar_t	*r_drawparticles;
extern cvar_t	*r_drawpolys;
extern cvar_t	*r_fullbright;
extern cvar_t	*r_lightmap;
extern cvar_t	*r_lockpvs;
extern cvar_t	*r_frontbuffer;
extern cvar_t	*r_showcluster;
extern cvar_t	*r_showtris;
extern cvar_t	*r_shownormals;
extern cvar_t	*r_showtangentspace;
extern cvar_t	*r_showmodelbounds;
extern cvar_t	*r_showshadowvolumes;
extern cvar_t	*r_offsetfactor;
extern cvar_t	*r_offsetunits;
extern cvar_t	*r_debugsort;
extern cvar_t	*r_speeds;
extern cvar_t	*r_singleshader;
extern cvar_t	*r_skipbackend;
extern cvar_t	*r_skipfrontend;
extern cvar_t	*r_swapInterval;
extern cvar_t	*r_mode;
extern cvar_t	*r_testmode;
extern cvar_t	*r_fullscreen;
extern cvar_t	*r_minimap;
extern cvar_t	*r_minimap_size;
extern cvar_t	*r_minimap_zoom;
extern cvar_t	*r_minimap_style;
extern cvar_t	*r_pause;
extern cvar_t	*r_width;
extern cvar_t	*r_height;
extern cvar_t	*r_refreshrate;
extern cvar_t	*r_bitdepth;
extern cvar_t	*r_overbrightbits;
extern cvar_t	*r_shadows;
extern cvar_t	*r_caustics;
extern cvar_t	*r_dynamiclights;
extern cvar_t	*r_modulate;
extern cvar_t	*r_ambientscale;
extern cvar_t	*r_directedscale;
extern cvar_t	*r_intensity;
extern cvar_t	*r_texturebits;
extern cvar_t	*r_texturefilter;
extern cvar_t	*r_texturefilteranisotropy;
extern cvar_t	*r_texturelodbias;
extern cvar_t	*r_max_normal_texsize;
extern cvar_t	*r_max_texsize;
extern cvar_t	*r_round_down;
extern cvar_t	*r_detailtextures;
extern cvar_t	*r_compress_normal_textures;
extern cvar_t	*r_compress_textures;
extern cvar_t	*r_lefthand;
extern cvar_t	*r_bloom;
extern cvar_t	*r_bloom_alpha;
extern cvar_t	*r_bloom_diamond_size;
extern cvar_t	*r_bloom_intensity;
extern cvar_t	*r_bloom_darken;
extern cvar_t	*r_bloom_sample_size;
extern cvar_t	*r_bloom_fast_sample;
extern cvar_t	*r_motionblur_intens;
extern cvar_t	*r_motionblur;
extern cvar_t	*r_mirroralpha;
extern cvar_t	*r_interpolate;
extern cvar_t	*r_physbdebug;
extern cvar_t	*r_pause_bw;
extern cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level

extern cvar_t	*gl_finish;
extern cvar_t	*gl_clear;
extern cvar_t	*vid_gamma;

#endif//R_LOCAL_H