//=======================================================================
//			Copyright XashXT Group 2008 ©
//		         render_api.h - xash renderer api
//=======================================================================
#ifndef RENDER_API_H
#define RENDER_API_H

#include "ref_params.h"
#include "com_model.h"
#include "trace_def.h"

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

typedef struct
{
	vec3_t		position;
	char		name[64];		// same as CS_SIZE
	short		entityIndex;
	byte		depth;
	byte		flags;

	// this is the surface plane that we hit so that
	// we can move certain decals across
	// transitions if they hit similar geometry
	vec3_t		impactPlaneNormal;
} decallist_t;

typedef struct
{
	char		*name;
	byte		*mempool;

	droqchunk_t	chunk;
	dcell_t		cells[256];
	dquadcell_t	qcells[256];
	
	byte		*vid_buffer; 
	byte		*vid_pic[2]; 
	byte		*pic;
	byte		*pic_pending;

	bool		new_frame;

	int		s_rate;
	int		s_width;
	int		s_channels;

	int		width;
	int		height;

	file_t		*file;
	int		headerlen;

	float		time;		// curtime for first cinematic frame
	uint		frame;
} cinematics_t;

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
	bool	(*Init)( bool full );	// init all render systems
	void	(*Shutdown)( bool full );	// shutdown all render systems

	void	(*BeginRegistration)( const char *map );
	bool	(*RegisterModel)( const char *name, int cl_index ); // also build replacement index table
	shader_t	(*RegisterShader)( const char *name, int shaderType );
	void	(*EndRegistration)( const char *skyname );
	void	(*FreeShader)( const char *shadername );

	// prepare frame to rendering
	bool	(*AddRefEntity)( struct cl_entity_s *pRefEntity, int ed_type, shader_t customShader );
	bool	(*DecalShoot)( shader_t decal, int ent, int model, vec3_t pos, vec3_t saxis, int flags, rgba_t color, float fadeTime, float fadeDuration );
	bool	(*AddDLight)( vec3_t pos, rgb_t color, float radius, int flags );
	bool	(*AddPolygon)( const poly_t *poly );
	bool	(*AddLightStyle)( int stylenum, vec3_t color );
	void	(*ClearScene)( void );

	void	(*BeginFrame)( bool clearScene );
	void	(*RenderFrame)( const ref_params_t *fd );
	void	(*EndFrame)( void );

	// triapi implementation
	void	(*RenderMode)( const kRenderMode_t mode );
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
	void	(*SetParms)( shader_t handle, kRenderMode_t rendermode, int frame );
	void	(*GetParms)( int *w, int *h, int *frames, int frame, shader_t shader );
	bool	(*ScrShot)( const char *filename, int shot_type ); // write screenshot with same name 
	bool	(*EnvShot)( const char *filename, uint size, const float *vieworg, bool skyshot );
	void	(*LightForPoint)( const vec3_t point, vec3_t ambientLight );
	void	(*DrawStretchRaw)( int x, int y, int w, int h, int cols, int rows, byte *data, bool redraw );
	void	(*DrawStretchPic)( float x, float y, float w, float h, float s1, float t1, float s2, float t2, shader_t shader );
	int	(*WorldToScreen)( const float *world, float *screen );
	void	(*ScreenToWorld)( const float *screen, float *world );
	bool	(*CullBox)( const vec3_t mins, const vec3_t maxs );
	bool 	(*RSpeedsMessage)( char *out, size_t size );
	int	(*CreateDecalList)( decallist_t *pList, bool changelevel );	// helper to serialize decals
	byte	*(*GetCurrentVis)( void );
	void	(*RestoreGamma)( void );
} render_exp_t;

typedef struct render_imp_s
{
	// interface validator
	size_t	api_size;			// must matched with sizeof(render_imp_t)

	// client fundamental callbacks
	void	(*UpdateScreen)( void );	// update screen while loading
	void	(*StudioEvent)( mstudioevent_t *event, struct cl_entity_s *ent );
	void	(*StudioFxTransform)( struct cl_entity_s *ent, float matrix[4][4] );
	void	(*ShowCollision)( cmdraw_t callback );	// debug
	long	(*WndProc)( void *hWnd, uint uMsg, uint wParam, long lParam );
	struct cl_entity_s *(*GetClientEdict)( int index );		// get rid of this
	struct player_info_s *(*GetPlayerInfo)( int playerIndex );		// not an entityIndex!!!
	struct cl_entity_s *(*GetLocalPlayer)( void );
	int	(*GetMaxClients)( void );
	float	(*GetLerpFrac)( void );
	void	(*DrawTriangles)( int fTrans );

	// RoQ decoder imports
	void	(*RoQ_ReadChunk)( cinematics_t *cin );
	byte	*(*RoQ_ReadNextFrame)( cinematics_t *cin, bool silent );
} render_imp_t;

#endif//RENDER_API_H