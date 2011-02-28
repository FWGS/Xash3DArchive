//=======================================================================
//			Copyright XashXT Group 2009 ©
//		com_export.h - safe calls exports from other libraries
//=======================================================================
#ifndef COM_EXPORT_H
#define COM_EXPORT_H

#ifdef __cplusplus
extern "C" {
#endif

// linked interfaces
extern stdlib_api_t	com;

#ifdef __cplusplus
}
#endif

// MD5 Hash
typedef struct
{
	uint	buf[4];
	uint	bits[2];
	byte	in[64];
} MD5Context_t;

#define TF_SKY	(TF_SKYSIDE|TF_UNCOMPRESSED|TF_NOMIPMAP|TF_NOPICMIP)
#define TF_FONT	(TF_UNCOMPRESSED|TF_NOPICMIP|TF_NOMIPMAP|TF_CLAMP)
#define TF_IMAGE	(TF_UNCOMPRESSED|TF_NOPICMIP|TF_NOMIPMAP)
#define TF_DECAL	(TF_CLAMP|TF_UNCOMPRESSED)

typedef enum
{
	TF_STATIC		= BIT(0),		// don't free until Shader_FreeUnused()
	TF_NOPICMIP	= BIT(1),		// ignore r_picmip resample rules
	TF_UNCOMPRESSED	= BIT(2),		// don't compress texture in video memory
	TF_CUBEMAP	= BIT(3),		// it's cubemap texture
	TF_DEPTHMAP	= BIT(4),		// custom texture filter used
	TF_INTENSITY	= BIT(5),
	TF_LUMINANCE	= BIT(6),		// force image to grayscale
	TF_SKYSIDE	= BIT(7),
	TF_CLAMP		= BIT(8),
	TF_NOMIPMAP	= BIT(9),
	TF_NEAREST	= BIT(10),	// disable texfilter
	TF_HAS_LUMA	= BIT(11),	// sets by GL_UploadTexture
	TF_MAKELUMA	= BIT(12),	// create luma from quake texture
	TF_NORMALMAP	= BIT(13),	// is a normalmap
	TF_LIGHTMAP	= BIT(14),	// is a lightmap
} texFlags_t;

typedef struct
{
	vec3_t		position;
	char		name[64];
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
	string	name;
	int	entnum;
	vec3_t	origin;
	float	volume;
	float	attenuation;
	qboolean	looping;
	int	pitch;
} soundlist_t;

qboolean R_Init( void );
void R_Shutdown( void );
void VID_CheckChanges( void );
int GL_LoadTexture( const char *name, const byte *buf, size_t size, int flags );
void GL_FreeImage( const char *name );
qboolean VID_ScreenShot( const char *filename, int shot_type );
qboolean VID_CubemapShot( const char *base, uint size, const float *vieworg, qboolean skyshot );
void VID_RestoreGamma( void );
void R_BeginFrame( qboolean clearScene );
void R_RenderFrame( const ref_params_t *fd, qboolean drawWorld );
void R_EndFrame( void );
void R_ClearScene( void );
void R_GetTextureParms( int *w, int *h, int texnum );
void R_GetSpriteParms( int *frameWidth, int *frameHeight, int *numFrames, int curFrame, const struct model_s *pSprite );
void R_DrawStretchRaw( float x, float y, float w, float h, int cols, int rows, const byte *data, qboolean dirty );
void R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, int texnum );
qboolean R_SpeedsMessage( char *out, size_t size );
void R_SetupSky( const char *skyboxname );
qboolean R_CullBox( const vec3_t mins, const vec3_t maxs, uint clipflags );
qboolean R_WorldToScreen( const vec3_t point, vec3_t screen );
void R_ScreenToWorld( const vec3_t screen, vec3_t point );
qboolean R_AddEntity( struct cl_entity_s *pRefEntity, int entityType );
void Mod_LoadSpriteModel( struct model_s *mod, const void *buffer );
void Mod_LoadMapSprite( struct model_s *mod, const void *buffer, size_t size );
void Mod_UnloadSpriteModel( struct model_s *mod );
void Mod_UnloadStudioModel( struct model_s *mod );
void Mod_UnloadBrushModel( struct model_s *mod );
void GL_SetRenderMode( int mode );
void R_RunViewmodelEvents( void );
void R_DrawViewModel( void );
int R_GetSpriteTexture( const struct model_s *m_pSpriteModel, int frame );
void R_LightForPoint( const vec3_t point, color24 *ambientLight, qboolean invLight, float radius );
void R_DecalShoot( int textureIndex, int entityIndex, int modelIndex, vec3_t pos, int flags, vec3_t saxis );
void R_RemoveEfrags( struct cl_entity_s *ent );
void R_AddEfrags( struct cl_entity_s *ent );
void R_DecalRemoveAll( int texture );
byte *Mod_GetCurrentVis( void );
void R_NewMap( void );

#endif//COM_EXPORT_H