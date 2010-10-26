//=======================================================================
//			Copyright XashXT Group 2008 ©
//		         render_api.h - xash renderer api
//=======================================================================
#ifndef RENDER_API_H
#define RENDER_API_H

#include "ref_params.h"

#define MAP_DEFAULT_SHADER		"*black"	// engine built-in default shader

// shader types used for shader loading
#define SHADER_SKY			1	// sky box shader
#define SHADER_NOMIP		2	// 2d images
#define SHADER_DECAL		3	// used for decals only
#define SHADER_SPRITE		4	// hud sprites
#define SHADER_GENERIC		5	// generic shader

// dlight flags
#define DLIGHT_ONLYENTS		BIT( 0 )
#define DLIGHT_DARK			BIT( 1 )

// screenshot types
#define VID_SCREENSHOT		0
#define VID_LEVELSHOT		1
#define VID_MINISHOT		2

typedef struct
{
	int		numverts;
	vec3_t		*verts;
	vec2_t		*stcoords;
	rgba_t		*colors;

	union
	{
		struct ref_shader_s	*shader;
		shader_t		shadernum;
	};

	int		fognum;
	vec3_t		normal;
	vec3_t		lightingOrigin;
} poly_t;

#define MAX_SCOREBOARDNAME	32

// NOTE: share this with user SDK when StudioRenderer will be moved on the client.dll
typedef struct player_info_s
{
	int		userid;			// User id on server
	char		userinfo[MAX_INFO_STRING];	// User info string
	char		name[MAX_SCOREBOARDNAME];	// Name (extracted from userinfo)
	int		spectator;		// Spectator or not, unused

	int		ping;
	int		packet_loss;

	// skin information
	char		model[64];
	int		topcolor;
	int		bottomcolor;

	// last frame rendered
	int		renderframe;	

	// Gait frame estimation
	int		gaitsequence;
	float		gaitframe;
	float		gaityaw;
	vec3_t		prevgaitorigin;
} player_info_t;

typedef struct
{
	vec3_t		position;
	char		name[64];		// same as CS_SIZE
	short		entityIndex;	// FIXME: replace with pointer to entity ?
	byte		depth;
	byte		flags;

	// this is the surface plane that we hit so that
	// we can move certain decals across
	// transitions if they hit similar geometry
	vec3_t		impactPlaneNormal;
} decallist_t;

/*
==============================================================================

RENDER.DLL INTERFACE
==============================================================================
*/
typedef struct render_exp_s
{
	// interface validator
	size_t	api_size;			// must matched with sizeof(render_exp_t)
	size_t	com_size;			// must matched with sizeof(stdlib_api_t)

	// initialize
	qboolean	(*Init)( qboolean full );	// init all render systems
	void	(*Shutdown)( qboolean full );	// shutdown all render systems

	void	(*BeginRegistration)( const char *map );
	qboolean	(*RegisterModel)( const char *name, int cl_index ); // also build replacement index table
	shader_t	(*RegisterShader)( const char *name, int shaderType );
	shader_t	(*RegisterShaderInt)( const char *name, const byte *image_buf, size_t image_size );	// from memory
	void	(*EndRegistration)( const char *skyname );
	void	(*FreeShader)( const char *shadername );

	// prepare frame to rendering
	qboolean	(*AddRefEntity)( struct cl_entity_s *pRefEntity, int entityType, shader_t customShader );
	qboolean	(*DecalShoot)( shader_t decal, int ent, int model, vec3_t pos, vec3_t saxis, int flags, rgba_t color, float fadeTime, float fadeDuration );
	qboolean	(*AddDLight)( vec3_t pos, color24 color, float radius, int flags );
	qboolean	(*AddPolygon)( const poly_t *poly );
	qboolean	(*AddLightStyle)( int stylenum, vec3_t color );
	void	(*ClearScene)( void );

	void	(*BeginFrame)( qboolean clearScene );
	void	(*RenderFrame)( const ref_params_t *fd );
	void	(*EndFrame)( void );

	// triapi implementation
	void	(*RenderMode)( const int mode );
	shader_t	(*GetSpriteTexture)( int spriteIndex, int spriteFrame );
	void	(*Normal3f)( const float x, const float y, const float z );
	void	(*Vertex3f)( const float x, const float y, const float z );
	void	(*Color4ub)( const byte r, const byte g, const byte b, const byte a );
	void	(*Fog)( float flFogColor[3], float flStart, float flEnd, int bOn );
	void	(*TexCoord2f)( const float u, const float v );
	void	(*Bind)( shader_t shader, int frame );
	void	(*CullFace)( int mode );
	void	(*Enable)( int cap );
	void	(*Disable)( int cap );
	void	(*Begin)( int mode );
	void	(*End)( void );

	// misc utilities
	void	(*SetColor)( const rgba_t color );
	void	(*SetParms)( shader_t handle, int rendermode, int frame );
	void	(*GetParms)( int *w, int *h, int *frames, int frame, shader_t shader );
	qboolean	(*ScrShot)( const char *filename, int shot_type ); // write screenshot with same name 
	qboolean	(*EnvShot)( const char *filename, uint size, const float *vieworg, qboolean skyshot );
	void	(*LightForPoint)( const vec3_t point, vec3_t ambientLight );
	void	(*DrawStretchRaw)( float x, float y, float w, float h, int cols, int rows, byte *data, qboolean redraw );
	void	(*DrawStretchPic)( float x, float y, float w, float h, float s1, float t1, float s2, float t2, shader_t shader );
	int	(*WorldToScreen)( const float *world, float *screen );
	void	(*ScreenToWorld)( const float *screen, float *world );
	qboolean	(*CullBox)( const vec3_t mins, const vec3_t maxs );
	qboolean 	(*RSpeedsMessage)( char *out, size_t size );
	int	(*CreateDecalList)( decallist_t *pList, qboolean changelevel );	// helper to serialize decals
	byte	*(*GetCurrentVis)( void );
	void	(*RestoreGamma)( void );
} render_exp_t;

typedef struct render_imp_s
{
	// interface validator
	size_t	api_size;			// must matched with sizeof(render_imp_t)

	// client fundamental callbacks
	void	(*UpdateScreen)( void );	// update screen while loading
	void	(*StudioEvent)( struct mstudioevent_s *event, struct cl_entity_s *ent );
	void	(*StudioFxTransform)( struct cl_entity_s *ent, float matrix[4][4] );
	void	(*ShowCollision)( cmdraw_t callback );	// debug
	long	(*WndProc)( void *hWnd, uint uMsg, uint wParam, long lParam );
	struct cl_entity_s *(*GetClientEdict)( int index );		// get rid of this
	struct player_info_s *(*GetPlayerInfo)( int playerIndex );		// not an entityIndex!!!
	struct cl_entity_s *(*GetLocalPlayer)( void );
	int	(*GetMaxClients)( void );
	void	(*DrawTriangles)( int fTrans );
	void	(*ExtraUpdate)( void );				// call during RenderFrame
} render_imp_t;

#endif//RENDER_API_H