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
extern render_exp_t	*re;

#ifdef __cplusplus
}
#endif

qboolean R_Init( void );
void R_Shutdown( void );
int GL_LoadTexture( const char *name, const byte *buf, size_t size, int flags );
void GL_FreeImage( const char *name );
qboolean VID_ScreenShot( const char *filename, int shot_type );
qboolean VID_CubemapShot( const char *base, uint size, const float *vieworg, qboolean skyshot );
void VID_RestoreGamma( void );
void R_BeginFrame( qboolean clearScene );
void R_RenderFrame( const ref_params_t *fd, qboolean drawWorld );
void R_EndFrame( void );
void R_ClearScene( void );
void R_DrawStretchRaw( float x, float y, float w, float h, int cols, int rows, const byte *data, qboolean dirty );
void R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, int texnum );
void R_DrawSetColor( const rgba_t color );

		
typedef int	sound_t;

typedef struct
{
	string	name;
	int	entnum;
	int	entchannel;
	vec3_t	origin;
	float	volume;
	float	attenuation;
	qboolean	looping;
	int	pitch;
} soundlist_t;

#endif//COM_EXPORT_H