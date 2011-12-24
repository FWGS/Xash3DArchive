/*
render_api.h - Xash3D extension for client interface
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef RENDER_API_H
#define RENDER_API_H

#include "lightstyle.h"
#include "dlight.h"

#define CL_RENDER_INTERFACE_VERSION		13
#define MAX_STUDIO_DECALS			4096	// + unused space of BSP decals

#define SURF_INFO( surf, mod )	((mextrasurf_t *)mod->cache.data + (surf - mod->surfaces)) 
#define INFO_SURF( surf, mod )	(mod->surfaces + (surf - (mextrasurf_t *)mod->cache.data)) 

// render info parms
#define PARM_TEX_WIDTH	1	// all parms with prefix 'TEX_' receive arg as texnum
#define PARM_TEX_HEIGHT	2	// otherwise it's not used
#define PARM_TEX_SRC_WIDTH	3
#define PARM_TEX_SRC_HEIGHT	4
#define PARM_TEX_SKYBOX	5	// second arg as skybox ordering num
#define PARM_TEX_SKYTEXNUM	6	// skytexturenum for quake sky
#define PARM_TEX_LIGHTMAP	7	// second arg as number 0 - 128
#define PARM_SKY_SPHERE	8	// sky is quake sphere ?
#define PARM_WORLD_VERSION	9	// return the version of bsp
#define PARM_WIDESCREEN	10
#define PARM_FULLSCREEN	11
#define PARM_SCREEN_WIDTH	12
#define PARM_SCREEN_HEIGHT	13
#define PARM_MAP_HAS_MIRRORS	14	// current map has mirorrs
#define PARM_CLIENT_INGAME	15
#define PARM_MAX_ENTITIES	16

enum
{
	// skybox ordering
	SKYBOX_RIGHT	= 0,
	SKYBOX_BACK,
	SKYBOX_LEFT,
	SKYBOX_FORWARD,
	SKYBOX_UP,
	SKYBOX_DOWN,
};

typedef enum
{
	TF_NEAREST	= (1<<0),		// disable texfilter
	TF_KEEP_RGBDATA	= (1<<1),		// some images keep source
	TF_NOFLIP_TGA	= (1<<2),		// Steam background completely ignore tga attribute 0x20
	TF_KEEP_8BIT	= (1<<3),		// keep original 8-bit image (if present)
	TF_NOPICMIP	= (1<<4),		// ignore r_picmip resample rules
	TF_UNCOMPRESSED	= (1<<5),		// don't compress texture in video memory
	TF_CUBEMAP	= (1<<6),		// it's cubemap texture
	TF_DEPTHMAP	= (1<<7),		// custom texture filter used
	TF_INTENSITY	= (1<<8),
	TF_LUMINANCE	= (1<<9),		// force image to grayscale
	TF_SKYSIDE	= (1<<10),
	TF_CLAMP		= (1<<11),
	TF_NOMIPMAP	= (1<<12),
	TF_HAS_LUMA	= (1<<13),	// sets by GL_UploadTexture
	TF_MAKELUMA	= (1<<14),	// create luma from quake texture
	TF_NORMALMAP	= (1<<15),	// is a normalmap
	TF_HAS_ALPHA	= (1<<16),	// image has alpha (used only for GL_CreateTexture)
	TF_FORCE_COLOR	= (1<<17),	// force upload monochrome textures as RGB (detail textures)
} texFlags_t;

typedef struct beam_s BEAM;
typedef struct particle_s particle_t;

// 10 bytes here
typedef struct modelstate_s
{
	short		sequence;
	short		frame;		// 10 bits multiple by 4, should be enough
	byte		blending[2];
	byte		controller[4];
} modelstate_t;

typedef struct decallist_s
{
	vec3_t		position;
	char		name[16];
	short		entityIndex;
	byte		depth;
	byte		flags;

	// this is the surface plane that we hit so that
	// we can move certain decals across
	// transitions if they hit similar geometry
	vec3_t		impactPlaneNormal;

	modelstate_t	studio_state;	// studio decals only
} decallist_t;

typedef struct render_api_s
{
	void		(*DrawSingleDecal)( struct decal_s *pDecal, struct msurface_s *fa );
	void		(*GetDetailScaleForTexture)( int texture, float *xScale, float *yScale );
	void		(*GetFogParamsForTexture)( int texture, byte *red, byte *green, byte *blue, byte *density );
	int		(*RenderGetParm)( int parm, int arg );
	void		(*EnvShot)( const float *vieworg, const char *name, qboolean skyshot );
	int		(*GL_LoadTexture)( const char *name, const byte *buf, size_t size, texFlags_t flags );
	int		(*GL_CreateTexture)( const char *name, int width, int height, const void *buffer, int flags ); 
	int		(*GL_FindTexture)( const char *name );
	void		(*GL_FreeTexture)( unsigned int texnum );
	void		(*R_SetCurrentEntity)( struct cl_entity_s *ent ); // tell engine about current entity
	void		(*R_SetCurrentModel)( struct model_s *mod );
	void		(*R_StoreEfrags)( struct efrag_s **ppefrag );
	void		(*Host_Error)( const char *error, ... );		// cause Host Error
	lightstyle_t*	(*GetLightStyle)( int number ); 
	dlight_t*		(*GetDynamicLight)( int number );
	dlight_t*		(*GetEntityLight)( int number );
	void		(*GL_Bind)( unsigned int tmu, unsigned int texnum );
	unsigned char	(*TextureToTexGamma)( unsigned char b );
	void		(*GetBeamChains)( BEAM ***active_beams, BEAM ***free_beams, particle_t ***free_trails );
	void		(*GL_DrawParticles)( const float *vieworg, const float *fwd, const float *rt, const float *up, unsigned int clipFlags );
	void		(*GL_SetWorldviewProjectionMatrix)( const float *glmatrix );
	int		(*COM_CompareFileTime)( const char *filename1, const char *filename2, int *iCompare );

	// AVIkit support
	void		*(*AVI_LoadVideo)( const char *filename, int load_audio, int ignore_hwgamma );
	int		(*AVI_GetVideoInfo)( void *Avi, long *xres, long *yres, float *duration );
	int		(*AVI_GetAudioInfo)( void *Avi, void *snd_info );
	long		(*AVI_GetAudioChunk)( void *Avi, char *audiodata, long offset, long length );
	long		(*AVI_GetVideoFrameNumber)( void *Avi, float time );
	byte		*(*AVI_GetVideoFrame)( void *Avi, long frame );
	void		(*AVI_UploadRawFrame)( int texture, int cols, int rows, int width, int height, const byte *data );
	void		(*AVI_FreeVideo)( void *Avi );
	int		(*AVI_IsActive)( void *Avi );

	const char*	(*GL_TextureName)( unsigned int texnum );
} render_api_t;

// render callbacks
typedef struct render_interface_s
{
	int		version;
	// passed through R_RenderFrame (0 - use engine renderer, 1 - use custom client renderer)
	int		(*GL_RenderFrame)( const struct ref_params_s *pparams, qboolean drawWorld );
	// build all the lightmaps on new level or when gamma is changed
	void		(*GL_BuildLightmaps)( void );
	// setup map bounds for ortho-projection when we in dev_overview mode
	void		(*GL_OrthoBounds)( const float *mins, const float *maxs );
	// handle decals which hit mod_studio or mod_sprite
	void		(*R_StudioDecalShoot)( int decalTexture, struct cl_entity_s *ent, const float *start, const float *pos, int flags, modelstate_t *state );
	// prepare studio decals for save
	int		(*R_CreateStudioDecalList)( decallist_t *pList, int count, qboolean changelevel );
} render_interface_t;

#endif//RENDER_API_H