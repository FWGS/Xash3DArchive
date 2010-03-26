/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2002-2007 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#ifndef __R_LOCAL_H__
#define __R_LOCAL_H__

#include <windows.h>
#include "launch_api.h"
#include "qfiles_ref.h"
#include "engine_api.h"
#include "entity_def.h"
#include "render_api.h"
#include "entity_state.h"

#if defined( _MSC_VER ) && ( _MSC_VER >= 1400 )
# define ALIGN(x)	__declspec(align(16))
#else
# define ALIGN(x)
#endif

#include "r_opengl.h"

extern stdlib_api_t		com;		// engine toolbox
extern render_imp_t		ri;
extern byte		*r_temppool;
#define Host_Error		com.error

typedef unsigned int elem_t;
typedef enum { RT_NONE, RT_MODEL, RT_SPRITE, RT_PORTALSURFACE, NUM_RTYPES } refEntityType_t;

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
	TEX_UNKNOWN = 0,	// not passed with R_GetPixelFormat, ignore it
	TEX_SYSTEM,	// generated by engine
	TEX_NOMIP,	// hud pics, menu etc
	TEX_GENERIC,	// models, sprites etc
	TEX_ALPHA,	// with alpha-channel
	TEX_SKYBOX,	// skybox textures
} texType_t;

typedef enum
{
	TF_STATIC		= BIT(0),		// don't free until Shader_FreeUnused()
	TF_NOPICMIP	= BIT(1),		// ignore r_picmip resample rules
	TF_UNCOMPRESSED	= BIT(2),		// don't compress texture in video memory
	TF_CUBEMAP	= BIT(3),		// it's cubemap texture
	TF_NORMALMAP	= BIT(4),		// needs to be merged with depthmap
	TF_DEPTHMAP	= BIT(5),		// custom texture filter used
	TF_INTENSITY	= BIT(5),
	TF_ALPHA		= BIT(6),
	TF_SKYSIDE	= BIT(7),
	TF_CLAMP		= BIT(8),
	TF_NOMIPMAP	= BIT(9),
	TF_NEAREST	= BIT(10),	// disable texfilter
} texFlags_t;

#define TF_CINEMATIC		( TF_NOPICMIP|TF_UNCOMPRESSED|TF_CLAMP|TF_NOMIPMAP )
#define TF_PORTALMAP		( TF_NOMIPMAP|TF_UNCOMPRESSED|TF_NOPICMIP|TF_CLAMP )
#define TF_SHADOWMAP		( TF_NOMIPMAP|TF_UNCOMPRESSED|TF_NOPICMIP|TF_CLAMP|TF_DEPTHMAP )

typedef struct texture_s
{
	string		name;		// game path, including extension
	int		srcWidth;		// source dims, used for mipmap loading
	int		srcHeight;

	int		width;		// upload width\height
	int		height;
	int		depth;		// upload framecount or depth

	texType_t		texType;		// just for debug
	texFlags_t	flags;
	pixformat_t	type;		// PFDesc[type].glType
	size_t		size;		// upload size for debug targets

	GLint		format;		// PFDesc[type].glType
	GLuint		target;		// glTarget
	GLuint		texnum;		// gl texture binding
	GLint		samples;		// gl samples

	int		touchFrame;	// 0 = free
	struct texture_s	*nextHash;
} texture_t;

enum
{
	TEXTURE_UNIT0,
	TEXTURE_UNIT1,
	TEXTURE_UNIT2,
	TEXTURE_UNIT3,
	TEXTURE_UNIT4,
	TEXTURE_UNIT5,
	TEXTURE_UNIT6,
	TEXTURE_UNIT7,
	MAX_TEXTURE_UNITS
};

#define FOG_TEXTURE_WIDTH		256
#define FOG_TEXTURE_HEIGHT		32

#define VID_DEFAULTMODE		"0"

#define SHADOW_PLANAR		1
#define SHADOW_MAPPING		2

#define MAX_ENTITIES		2048	// per one frame
#define MAX_POLY_VERTS		3000
#define MAX_POLYS			2048

//===================================================================

#include "r_math.h"
#include "r_mesh.h"
#include "r_shader.h"
#include "r_backend.h"
#include "r_shadow.h"
#include "r_model.h"

#define BACKFACE_EPSILON		0.01

#define Z_NEAR			4

#define SIDE_FRONT			0
#define SIDE_BACK			1
#define SIDE_ON			2

#define RP_NONE			0x0
#define RP_MIRRORVIEW		0x1     // lock pvs at vieworg
#define RP_PORTALVIEW		0x2
#define RP_ENVVIEW			0x4
#define RP_NOSKY			0x8
#define RP_SKYPORTALVIEW		0x10
#define RP_PORTALCAPTURED		0x20
#define RP_PORTALCAPTURED2		0x40
#define RP_OLDVIEWCLUSTER		0x80
#define RP_SHADOWMAPVIEW		0x100
#define RP_FLIPFRONTFACE		0x200
#define RP_WORLDSURFVISIBLE		0x400
#define RP_CLIPPLANE		0x800
#define RP_TRISOUTLINES		0x1000
#define RP_SHOWNORMALS		0x2000

#define RP_NONVIEWERREF		( RP_PORTALVIEW|RP_MIRRORVIEW|RP_ENVVIEW|RP_SKYPORTALVIEW|RP_SHADOWMAPVIEW )
#define RP_LOCALCLIENT(e)		(ri.GetLocalPlayer() && ((e)->index == ri.GetLocalPlayer()->serialnumber))
#define RP_FOLLOWENTITY(e)		(((e)->movetype == MOVETYPE_FOLLOW && (e)->parent))
#define MOD_ALLOWBUMP()		(r_lighting_models_followdeluxe->integer ? mapConfig.deluxeMappingEnabled : GL_Support( R_SHADER_GLSL100_EXT ))

//====================================================

typedef struct
{
	// these values common with dlight_t so don't move them
	vec3_t		origin;
	union
	{
		vec3_t	color;		// dlight color
		vec3_t	angles;		// spotlight angles
	};
	float		intensity;	// cdlight->radius
	shader_t		texture;		// light image e.g. for flashlight
	vec2_t		cone;		// spotlight cone

	// dlight_t private starts here
	vec3_t		mins, maxs;
	const ref_shader_t	*shader;
} dlight_t;

typedef struct
{
	float		rgb[3];	// 0.0 - 2.0
} lightstyle_t;

typedef struct
{
	int		features;
	int		lightmapNum[LM_STYLES];
	int		lightmapStyles[LM_STYLES];
	int		vertexStyles[LM_STYLES];
	float		stOffset[LM_STYLES][2];
} superLightStyle_t;

typedef struct ref_entity_s
{
	edtype_t			ent_type;		// entity type
	uint			m_nCachedFrameCount;// keep current render frame
	int			index;		// viewmodel has entindex -1
	refEntityType_t		rtype;

	struct ref_model_s		*model;		// opaque type outside refresh
	struct ref_entity_s		*parent;		// link to parent entity (FOLLOW or weaponmodel)

	studioframe_t		*prev;		// previous frame values for lerping

	float			framerate;	// custom framerate
	float			frame;

	int			body;
	int			skin;

          int			movetype;		// entity moving type
	float			scale;

	byte			*mempool;		// studio mempool
	void			*extradata;	// studiomodel bones, etc

	// misc
	rgb_t			rendercolor;	// hl1 rendercolor
	byte			renderamt;	// hl1 alphavalues
	int			rendermode;	// hl1 rendermode
	int			renderfx;		// server will be translate hl1 values into flags
	int			colormap;		// q1 and hl1 model colormap (can applied for sprites)
	int			flags;		// q1 effect flags, EF_ROTATE, EF_DIMLIGHT etc

	// client gait sequence (local stuff)
	int			gaitsequence;	// client->sequence + yaw

	// most recent data
	vec3_t			axis[3];
	vec3_t			angles;
	vec3_t			movedir;		// forward vector that computed on a server
	vec3_t			origin, origin2;
	vec3_t			lightingOrigin;

	// RT_SPRITE stuff
	struct ref_shader_s		*customShader;	// client drawing stuff
	float			radius;		// used as RT_SPRITE's radius

	// outilne stuff
	float			outlineHeight;
	rgba_t			outlineColor;

} ref_entity_t;

typedef struct
{
	int		params;			// rendering parameters

	ref_params_t	refdef;
	int		scissor[4];
	int		viewport[4];
	float		lerpFrac;			// lerpfraction

	meshlist_t	*meshlist;		// meshes to be rendered
	meshbuffer_t	**surfmbuffers;		// pointers to meshbuffers of world surfaces

	uint		shadowBits;
	shadowGroup_t	*shadowGroup;

	ref_entity_t	*currententity;
	ref_model_t	*currentmodel;
	ref_entity_t	*previousentity;

	//
	// view origin
	//
	vec3_t		viewOrigin;
	vec3_t		viewAxis[3];
	vec_t		*vup, *vpn, *vright;
	cplane_t		frustum[6];
	float		farClip;
	uint		clipFlags;
	vec3_t		visMins, visMaxs;

	matrix4x4		objectMatrix;
	matrix4x4		worldviewMatrix;
	matrix4x4		modelviewMatrix;		// worldviewMatrix * objectMatrix

	matrix4x4		projectionMatrix;
	matrix4x4		worldviewProjectionMatrix;	// worldviewMatrix * projectionMatrix

	float		skyMins[2][6];
	float		skyMaxs[2][6];

	float		fog_dist_to_eye[MAX_MAP_FOGS];

	vec3_t		pvsOrigin;
	cplane_t		clipPlane;
	cplane_t		portalPlane;
} refinst_t;

//====================================================
extern int r_pvsframecount;
extern int r_framecount;
extern int r_framecount2;
extern int c_brush_polys, c_world_leafs;

extern int r_mark_leaves, r_world_node;
extern int r_add_polys, r_add_entities;
extern int r_sort_meshes, r_draw_meshes;

extern msurface_t *r_debug_surface;

extern int gl_filter_min, gl_filter_max;

#define MAX_RSPEEDSMSGSIZE	1024
extern char r_speeds_msg[MAX_RSPEEDSMSGSIZE];

extern ref_model_t *cl_models[MAX_MODELS];

extern float gldepthmin, gldepthmax;

//
// screen size info
//
extern unsigned int r_numEntities;
extern ref_entity_t	r_entities[MAX_ENTITIES];

extern unsigned int r_numDlights;
extern dlight_t	r_dlights[MAX_DLIGHTS];

extern unsigned int r_numPolys;
extern poly_t r_polys[MAX_POLYS];

extern lightstyle_t r_lightStyles[MAX_LIGHTSTYLES];

extern ref_params_t	r_lastRefdef;

extern int r_viewcluster, r_oldviewcluster;

extern float r_farclip_min, r_farclip_bias;

extern ref_entity_t	*r_worldent;
extern ref_model_t *r_worldmodel;
extern mbrushmodel_t *r_worldbrushmodel;

extern cvar_t *r_colorbits;
extern cvar_t *r_depthbits;
extern cvar_t *r_stencilbits;
extern cvar_t *r_norefresh;
extern cvar_t *r_drawentities;
extern cvar_t *r_drawworld;
extern cvar_t *r_speeds;
extern cvar_t *r_drawelements;
extern cvar_t *r_fullbright;
extern cvar_t *r_lightmap;
extern cvar_t *r_novis;
extern cvar_t *r_nocull;
extern cvar_t *r_ignorehwgamma;
extern cvar_t *r_overbrightbits;
extern cvar_t *r_mapoverbrightbits;
extern cvar_t *r_vertexbuffers;
extern cvar_t *r_lefthand;
extern cvar_t *r_physbdebug;
extern cvar_t *r_check_errors;
extern cvar_t *r_allow_software;
extern cvar_t *r_frontbuffer;
extern cvar_t *r_width;
extern cvar_t *r_height;

extern cvar_t *r_flares;
extern cvar_t *r_flaresize;
extern cvar_t *r_flarefade;
extern cvar_t *r_spriteflares;

extern cvar_t *r_dynamiclight;
extern cvar_t *r_coronascale;
extern cvar_t *r_detailtextures;
extern cvar_t *r_subdivisions;
extern cvar_t *r_faceplanecull;
extern cvar_t *gl_wireframe;
extern cvar_t *r_shownormals;
extern cvar_t *r_showtextures;
extern cvar_t *r_draworder;

extern cvar_t *r_fastsky;
extern cvar_t *r_portalonly;
extern cvar_t *r_portalmaps;
extern cvar_t *r_portalmaps_maxtexsize;

extern cvar_t *r_lighting_bumpscale;
extern cvar_t *r_lighting_deluxemapping;
extern cvar_t *r_lighting_diffuse2heightmap;
extern cvar_t *r_lighting_specular;
extern cvar_t *r_lighting_glossintensity;
extern cvar_t *r_lighting_glossexponent;
extern cvar_t *r_lighting_models_followdeluxe;
extern cvar_t *r_lighting_ambientscale;
extern cvar_t *r_lighting_directedscale;
extern cvar_t *r_lighting_packlightmaps;
extern cvar_t *r_lighting_maxlmblocksize;

extern cvar_t *r_offsetmapping;
extern cvar_t *r_offsetmapping_scale;
extern cvar_t *r_offsetmapping_reliefmapping;

extern cvar_t *r_occlusion_queries;
extern cvar_t *r_occlusion_queries_finish;

extern cvar_t *r_shadows;
extern cvar_t *r_shadows_alpha;
extern cvar_t *r_shadows_nudge;
extern cvar_t *r_shadows_projection_distance;
extern cvar_t *r_shadows_maxtexsize;
extern cvar_t *r_shadows_pcf;
extern cvar_t *r_shadows_self_shadow;

extern cvar_t *r_bloom_alpha;
extern cvar_t *r_bloom_diamond_size;
extern cvar_t *r_bloom_intensity;
extern cvar_t *r_bloom_darken;
extern cvar_t *r_bloom_sample_size;
extern cvar_t *r_bloom_fast_sample;
extern cvar_t *r_outlines_world;
extern cvar_t *r_outlines_scale;
extern cvar_t *r_outlines_cutoff;

extern cvar_t *r_himodels;
extern cvar_t *r_environment_color;
extern cvar_t *r_gamma;
extern cvar_t *r_texturebits;
extern cvar_t *gl_texturemode;
extern cvar_t *gl_texture_anisotropy;
extern cvar_t *gl_texture_lodbias;
extern cvar_t *gl_round_down;
extern cvar_t *gl_compress_textures;
extern cvar_t *r_mode;
extern cvar_t *r_nobind;
extern cvar_t *r_picmip;
extern cvar_t *r_skymip;
extern cvar_t *gl_clear;
extern cvar_t *r_polyblend;
extern cvar_t *r_lockpvs;
extern cvar_t *r_swapInterval;

extern cvar_t *gl_finish;
extern cvar_t *gl_delayfinish;
extern cvar_t *gl_cull;
extern cvar_t *gl_extensions;

extern cvar_t *vid_fullscreen;
extern cvar_t *vid_multiscreen_head;
extern cvar_t *vid_displayfrequency;

//====================================================================

static _inline byte R_FloatToByte( float x )
{
	union {
		float f;
		unsigned int i;
	} f2i;

	// shift float to have 8bit fraction at base of number
	f2i.f = x + 32768.0f;
	f2i.i &= 0x7FFFFF;

	// then read as integer and kill float bits...
	return ( byte )min( f2i.i, 255 );
}

float		R_FastSin( float t );
void		R_LatLongToNorm( const byte latlong[2], vec3_t out );
void		NormToLatLong( const vec3_t normal, byte latlong[2] );

//====================================================================

//
// r_bloom.c
//
void		R_InitBloomTextures( void );
void		R_BloomBlend( const ref_params_t *fd );

//
// r_cin.c
//
void		R_InitCinematics( void );
void		R_ShutdownCinematics( void );
uint		R_StartCinematics( const char *arg );
void		R_FreeCinematics( uint id );
void		R_RunAllCinematics( void );
texture_t		*R_UploadCinematics( uint id );

//
// r_cull.c
//
enum
{
	OQ_NONE = -1,
	OQ_ENTITY,
	OQ_PLANARSHADOW,
	OQ_SHADOWGROUP,
	OQ_CUSTOM
};

#define OCCLUSION_QUERIES_CVAR_HACK( RI ) ( !(r_occlusion_queries->integer == 2 && r_shadows->integer != SHADOW_MAPPING) \
											|| ((RI).refdef.flags & RDF_PORTALINVIEW) )
#define OCCLUSION_QUERIES_ENABLED( RI )	( GL_Support( R_OCCLUSION_QUERIES_EXT ) && r_occlusion_queries->integer && r_drawentities->integer \
											&& !((RI).params & RP_NONVIEWERREF) && !((RI).refdef.flags & RDF_NOWORLDMODEL) \
											&& OCCLUSION_QUERIES_CVAR_HACK( RI ) )
#define OCCLUSION_OPAQUE_SHADER( s )	(((s)->sort == SORT_OPAQUE ) && ((s)->flags & SHADER_DEPTHWRITE ) && !(s)->numDeforms )
#define OCCLUSION_TEST_ENTITY( e )	(((e)->flags & EF_OCCLUSIONTEST) || ((e)->ent_type == ED_VIEWMODEL))

void		R_InitOcclusionQueries( void );
void		R_BeginOcclusionPass( void );
ref_shader_t	*R_OcclusionShader( void );
void		R_AddOccludingSurface( msurface_t *surf, ref_shader_t *shader );
int		R_GetOcclusionQueryNum( int type, int key );
int		R_IssueOcclusionQuery( int query, ref_entity_t *e, vec3_t mins, vec3_t maxs );
bool		R_OcclusionQueryIssued( int query );
uint		R_GetOcclusionQueryResult( int query, bool wait );
bool		R_GetOcclusionQueryResultBool( int type, int key, bool wait );
void		R_EndOcclusionPass( void );
void		R_ShutdownOcclusionQueries( void );

//
// r_draw.c
//
extern meshbuffer_t  pic_mbuffer;

void R_DrawSetColor( const void *data );
void R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, shader_t shadernum );
void R_DrawStretchRaw( int x, int y, int w, int h, int cols, int rows, const byte *data, bool redraw );
void R_DrawSetParms( shader_t handle, kRenderMode_t rendermode, int frame );
void R_DrawGetParms( int *w, int *h, int *f, int frame, shader_t shader );
void Tri_RenderMode( const kRenderMode_t mode );
void Tri_Normal3f( const float x, const float y, const float z );
void Tri_Vertex3f( const float x, const float y, const float z );
void Tri_Color4ub( const byte r, const byte g, const byte b, const byte a );
void Tri_Fog( float flFogColor[3], float flStart, float flEnd, int bOn );
void Tri_TexCoord2f( const float u, const float v );
void Tri_Bind( shader_t shader, int frame );
void Tri_RenderCallback( int fTrans );
void Tri_CullFace( int mode );
void Tri_Enable( int cap );
void Tri_Disable( int cap );
void Tri_Begin( int mode );
void Tri_End( void );

//
// r_image.c
//
void		GL_SelectTexture( GLenum tmu );
void		GL_Bind( GLenum tmu, texture_t *tex );
void		GL_TexEnv( GLenum mode );
void		GL_LoadMatrix( const matrix4x4 source );
void		GL_LoadTexMatrix( const matrix4x4 m );
void		GL_LoadIdentityTexMatrix( void );
void		GL_EnableTexGen( int coord, int mode );
void		GL_SetTexCoordArrayMode( int mode );

void		R_InitImages( void );
void		R_ShutdownImages( void );
void		R_InitPortalTexture( texture_t **texture, int id, int screenWidth, int screenHeight );
void		R_InitShadowmapTexture( texture_t **texture, int id, int screenWidth, int screenHeight );
void		R_FreeImage( texture_t *image );

void		R_TextureList_f( void );
void		R_SetTextureParameters( void );
void		R_ShowTextures( void );

texture_t		*R_LoadTexture( const char *name, rgbdata_t *pic, int samples, texFlags_t flags );
texture_t		*R_FindTexture( const char *name, const byte *buf, size_t size, texFlags_t flags );

bool		VID_ScreenShot( const char *filename, int shot_type );
bool		VID_CubemapShot( const char *base, uint size, const float *vieworg, bool skyshot );

//
// r_light.c
//
#define DLIGHT_SCALE	    0.5f
#define MAX_SUPER_STYLES    1023

extern int r_numSuperLightStyles;
extern superLightStyle_t r_superLightStyles[MAX_SUPER_STYLES];

void	R_LightBounds( const vec3_t origin, float intensity, vec3_t mins, vec3_t maxs );
bool	R_SurfPotentiallyLit( msurface_t *surf );
uint	R_AddSurfDlighbits( msurface_t *surf, unsigned int dlightbits );
void	R_AddDynamicLights( unsigned int dlightbits, int state );
void	R_LightForEntity( ref_entity_t *e, byte *bArray );
void	R_LightForOrigin( const vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius );
void	R_BuildLightmaps( int numLightmaps, int w, int h, const byte *data, mlightmapRect_t *rects );
void	R_InitLightStyles( void );
int	R_AddSuperLightStyle( const int *lightmaps, const byte *lightmapStyles, const byte *vertexStyles, 
								 mlightmapRect_t **lmRects );
void	R_SortSuperLightStyles( void );

void	R_InitCoronas( void );
void	R_DrawCoronas( void );

//
// r_main.c
//
void	GL_Cull( int cull );
void	GL_SetState( int state );
void	GL_FrontFace( int front );
void	R_Set2DMode( bool enable );

void	R_BeginFrame( bool clearScene );
void	R_EndFrame( void );
void	R_RenderScene( const ref_params_t *fd );
void	R_RenderView( const ref_params_t *fd );
void	R_ClearScene( void );

bool	R_CullBox( const vec3_t mins, const vec3_t maxs, const uint clipflags );
bool	R_CullSphere( const vec3_t centre, const float radius, const uint clipflags );
bool	R_VisCullBox( const vec3_t mins, const vec3_t maxs );
bool	R_VisCullSphere( const vec3_t origin, float radius );
int	R_CullModel( ref_entity_t *e, vec3_t mins, vec3_t maxs, float radius );

mfog_t	*R_FogForSphere( const vec3_t centre, const float radius );
bool	R_CompletelyFogged( mfog_t *fog, vec3_t origin, float radius );

void	R_LoadIdentity( void );
void	R_RotateForEntity( ref_entity_t *e );
void	R_TranslateForEntity( ref_entity_t *e );
bool	R_TransformToScreen_Vec3( const vec3_t in, vec3_t out );
void	R_TransformVectorToScreen( const ref_params_t *rd, const vec3_t in, vec2_t out );
void	R_TransformEntityBBox( ref_entity_t *e, vec3_t mins, vec3_t maxs, vec3_t bbox[8], bool local );

bool	R_SpriteOverflow( void );
bool	R_PushSpritePoly( const meshbuffer_t *mb );
bool	R_PushSprite( const meshbuffer_t *mb, int type, float right, float left, float up, float down );

#define NUM_CUSTOMCOLORS	16
void		R_InitCustomColors( void );
void		R_SetCustomColor( int num, int r, int g, int b );
int		R_GetCustomColor( int num );

void		R_InitOutlines( void );
void		R_AddModelMeshOutline( unsigned int modhandle, mfog_t *fog, int meshnum );

msurface_t *R_TraceLine( trace_t *tr, const vec3_t start, const vec3_t end, int surfumask );

//
// r_mesh.c
//

extern meshlist_t r_worldlist, r_shadowlist;

void		R_InitMeshLists( void );
void		R_FreeMeshLists( void );
void		R_ClearMeshList( meshlist_t *meshlist );
int			R_ReAllocMeshList( meshbuffer_t **mb, int minMeshes, int maxMeshes );
meshbuffer_t *R_AddMeshToList( int type, mfog_t *fog, ref_shader_t *shader, int infokey );
void		R_AddModelMeshToList( unsigned int modhandle, mfog_t *fog, ref_shader_t *shader, int meshnum );
void		R_AllocMeshbufPointers( refinst_t *RI );

void		R_SortMeshes( void );
void		R_DrawMeshes( void );
void		R_DrawTriangleOutlines( bool showTris, bool showNormals );
void		R_DrawPortals( void );

void		R_DrawCubemapView( const vec3_t origin, const vec3_t angles, int size );
void		R_DrawSkyPortal( skyportal_t *skyportal, vec3_t mins, vec3_t maxs );

void		R_BuildTangentVectors( int numVertexes, vec4_t *xyzArray, vec4_t *normalsArray, vec2_t *stArray, 
								  int numTris, elem_t *elems, vec4_t *sVectorsArray );

//
// r_program.c
//
#define DEFAULT_GLSL_PROGRAM		"*r_defaultProgram"
#define DEFAULT_GLSL_DISTORTION_PROGRAM	"*r_defaultDistortionProgram"
#define DEFAULT_GLSL_SHADOWMAP_PROGRAM	"*r_defaultShadowmapProgram"
#define DEFAULT_GLSL_OUTLINE_PROGRAM "*r_defaultOutlineProgram"

enum
{
	PROGRAM_TYPE_NONE,
	PROGRAM_TYPE_MATERIAL,
	PROGRAM_TYPE_DISTORTION,
	PROGRAM_TYPE_SHADOWMAP,
	PROGRAM_TYPE_OUTLINE
};

enum
{
	PROGRAM_APPLY_LIGHTSTYLE0			= 1 << 0,
	PROGRAM_APPLY_LIGHTSTYLE1			= 1 << 1,
	PROGRAM_APPLY_LIGHTSTYLE2			= 1 << 2,
	PROGRAM_APPLY_LIGHTSTYLE3			= 1 << 3,
	PROGRAM_APPLY_SPECULAR				= 1 << 4,
	PROGRAM_APPLY_DIRECTIONAL_LIGHT	    = 1 << 5,
	PROGRAM_APPLY_FB_LIGHTMAP			= 1 << 6,
	PROGRAM_APPLY_OFFSETMAPPING			= 1 << 7,
	PROGRAM_APPLY_RELIEFMAPPING			= 1 << 8,
	PROGRAM_APPLY_AMBIENT_COMPENSATION  = 1 << 9,
	PROGRAM_APPLY_DECAL					= 1 << 10,
	PROGRAM_APPLY_BASETEX_ALPHA_ONLY	= 1 << 11,

	PROGRAM_APPLY_EYEDOT				= 1 << 12,
	PROGRAM_APPLY_DISTORTION_ALPHA		= 1 << 13,

	PROGRAM_APPLY_PCF2x2				= 1 << 14,
	PROGRAM_APPLY_PCF3x3				= 1 << 15,

	PROGRAM_APPLY_BRANCHING				= 1 << 16,
	PROGRAM_APPLY_CLIPPING				= 1 << 17,
	PROGRAM_APPLY_NO_HALF_TYPES			= 1 << 18
};

void		R_InitGLSLPrograms( void );
int			R_FindGLSLProgram( const char *name );
int			R_RegisterGLSLProgram( const char *name, const char *string, unsigned int features );
int			R_GetProgramObject( int elem );
void		R_UpdateProgramUniforms( int elem, vec3_t eyeOrigin, vec3_t lightOrigin, vec3_t lightDir,
									vec4_t ambient, vec4_t diffuse, superLightStyle_t *superLightStyle,
									bool frontPlane, int TexWidth, int TexHeight,
									float projDistance, float offsetmappingScale );
void		R_ShutdownGLSLPrograms( void );
void		R_ProgramList_f( void );
void		R_ProgramDump_f( void );

//
// r_poly.c
//
void	R_PushPoly( const meshbuffer_t *mb );
void	R_AddPolysToList( void );
bool	R_SurfPotentiallyFragmented( msurface_t *surf );
int	R_GetClippedFragments( const vec3_t origin, float radius, vec3_t axis[3], int maxfverts, vec3_t *fverts, int maxfragments, fragment_t *fragments );
msurface_t *R_TransformedTraceLine( trace_t *tr, const vec3_t start, const vec3_t end, ref_entity_t *test, int surfumask );

//
// r_sprite.c
//

void R_SpriteInit( void );
mspriteframe_t *R_GetSpriteFrame( ref_model_t *pModel, int frame, float yawAngle );
void R_DrawSpriteModel( const meshbuffer_t *mb );
bool R_SpriteOccluded( ref_entity_t *e );
bool R_CullSpriteModel( ref_entity_t *e );

//
// r_studio.c
//

void R_AddStudioModelToList( ref_entity_t *e );
void R_DrawStudioModel( const meshbuffer_t *mb );
void R_StudioResetSequenceInfo( ref_entity_t *ent );
float R_StudioFrameAdvance( ref_entity_t *ent, float flInterval );
void R_StudioModelBBox( ref_entity_t *e, vec3_t mins, vec3_t maxs );
bool R_CullStudioModel( ref_entity_t *e );
void R_StudioRunEvents( ref_entity_t *e );
void R_StudioDrawDebug( void );
void R_StudioInit( void );
void R_StudioAllocExtradata( edict_t *in, ref_entity_t *e );
void R_StudioAllocTentExtradata( struct tempent_s *in, ref_entity_t *e );
void R_StudioFreeAllExtradata( void );
void R_StudioShutdown( void );

//
//
// r_register.c
//
void R_NewMap( void );
bool R_Init( bool full );
void R_Shutdown( bool full );

//
// r_opengl.c
//
void R_RestoreGamma( void );
void R_CheckForErrors_( const char *filename, const int fileline );
#define R_CheckForErrors() R_CheckForErrors_( __FILE__, __LINE__ )

//
// r_surf.c
//
#define MAX_SURF_QUERIES		0x1E0

void		R_MarkLeaves( void );
void		R_DrawWorld( void );
bool	R_SurfPotentiallyVisible( msurface_t *surf );
bool	R_CullBrushModel( ref_entity_t *e );
void		R_AddBrushModelToList( ref_entity_t *e );

void		R_ClearSurfOcclusionQueryKeys( void );
int			R_SurfOcclusionQueryKey( ref_entity_t *e, msurface_t *surf );
void		R_SurfIssueOcclusionQueries( void );

//
// r_sky.c
//
skydome_t		*R_CreateSkydome( byte *mempool, float skyheight, ref_shader_t **farboxShaders, ref_shader_t **nearboxShaders );
void		R_FreeSkydome( skydome_t *skydome );
void		R_ClearSkyBox( void );
void		R_DrawSky( ref_shader_t *shader );
bool		R_AddSkySurface( msurface_t *fa );
ref_shader_t	*R_SetupSky( const char *name );
void		R_FreeSky( void );

//====================================================================

enum
{
	GLSTATE_NONE = 0,

	//
	// glBlendFunc args
	//
	GLSTATE_SRCBLEND_ZERO					= 1,
	GLSTATE_SRCBLEND_ONE					= 2,
	GLSTATE_SRCBLEND_DST_COLOR				= 1|2,
	GLSTATE_SRCBLEND_ONE_MINUS_DST_COLOR	= 4,
	GLSTATE_SRCBLEND_SRC_ALPHA				= 1|4,
	GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA	= 2|4,
	GLSTATE_SRCBLEND_DST_ALPHA				= 1|2|4,
	GLSTATE_SRCBLEND_ONE_MINUS_DST_ALPHA	= 8,

	GLSTATE_DSTBLEND_ZERO					= 16,
	GLSTATE_DSTBLEND_ONE					= 32,
	GLSTATE_DSTBLEND_SRC_COLOR				= 16|32,
	GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR	= 64,
	GLSTATE_DSTBLEND_SRC_ALPHA				= 16|64,
	GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA	= 32|64,
	GLSTATE_DSTBLEND_DST_ALPHA				= 16|32|64,
	GLSTATE_DSTBLEND_ONE_MINUS_DST_ALPHA	= 128,

	GLSTATE_BLEND_MTEX						= 0x100,

	GLSTATE_AFUNC_GT0						= 0x200,
	GLSTATE_AFUNC_LT128						= 0x400,
	GLSTATE_AFUNC_GE128						= 0x800,

	GLSTATE_DEPTHWRITE						= 0x1000,
	GLSTATE_DEPTHFUNC_EQ					= 0x2000,

	GLSTATE_OFFSET_FILL						= 0x4000,
	GLSTATE_NO_DEPTH_TEST					= 0x8000,

	GLSTATE_MARK_END						= 0x10000 // SHADERPASS_MARK_BEGIN
};

#define GLSTATE_MASK		( GLSTATE_MARK_END-1 )

// #define SHADERPASS_SRCBLEND_MASK (((GLSTATE_SRCBLEND_DST_ALPHA)<<1)-GLSTATE_SRCBLEND_ZERO)
#define GLSTATE_SRCBLEND_MASK	0xF

// #define SHADERPASS_DSTBLEND_MASK (((GLSTATE_DSTBLEND_DST_ALPHA)<<1)-GLSTATE_DSTBLEND_ZERO)
#define GLSTATE_DSTBLEND_MASK	0xF0

#define GLSTATE_BLENDFUNC		( GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK )
#define GLSTATE_ALPHAFUNC		( GLSTATE_AFUNC_GT0|GLSTATE_AFUNC_LT128|GLSTATE_AFUNC_GE128 )

typedef struct
{
	int		pow2MapOvrbr;

	float		ambient[3];
	rgba_t		outlineColor;
	rgba_t		environmentColor;

	bool		lightmapsPacking;
	bool		deluxeMaps;            // true if there are valid deluxemaps in the .bsp
	bool		deluxeMappingEnabled;  // true if deluxeMaps is true and r_lighting_deluxemaps->integer != 0
} mapconfig_t;

extern mapconfig_t	mapConfig;
extern refinst_t	RI, prevRI;

#endif /*__R_LOCAL_H__*/
