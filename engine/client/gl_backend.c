//=======================================================================
//			Copyright XashXT Group 2010 �
//		        gl_backend.c - rendering backend
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"

/*
=================
GL_SelectTexture
=================
*/
void GL_SelectTexture( GLenum texture )
{
	if( !GL_Support( GL_ARB_MULTITEXTURE ))
		return;

	if( glState.activeTMU == texture )
		return;

	glState.activeTMU = texture;

	if( pglActiveTextureARB )
	{
		pglActiveTextureARB( texture + GL_TEXTURE0_ARB );
		pglClientActiveTextureARB( texture + GL_TEXTURE0_ARB );
	}
	else if( pglSelectTextureSGIS )
	{
		pglSelectTextureSGIS( texture + GL_TEXTURE0_SGIS );
	}
}

/*
=================
GL_TexEnv
=================
*/
void GL_TexEnv( GLenum mode )
{
	if( glState.currentEnvModes[glState.activeTMU] == mode )
		return;

	glState.currentEnvModes[glState.activeTMU] = mode;
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode );
}

/*
=================
GL_TexGen
=================
*/
void GL_TexGen( GLenum coord, GLenum mode )
{
	int	tmu = glState.activeTMU;
	int	bit, gen;

	switch( coord )
	{
	case GL_S:
		bit = 1;
		gen = GL_TEXTURE_GEN_S;
		break;
	case GL_T:
		bit = 2;
		gen = GL_TEXTURE_GEN_T;
		break;
	case GL_R:
		bit = 4;
		gen = GL_TEXTURE_GEN_R;
		break;
	case GL_Q:
		bit = 8;
		gen = GL_TEXTURE_GEN_Q;
		break;
	default: return;
	}

	if( mode )
	{
		if(!( glState.genSTEnabled[tmu] & bit ))
		{
			pglEnable( gen );
			glState.genSTEnabled[tmu] |= bit;
		}
		pglTexGeni( coord, GL_TEXTURE_GEN_MODE, mode );
	}
	else
	{
		if( glState.genSTEnabled[tmu] & bit )
		{
			pglDisable( gen );
			glState.genSTEnabled[tmu] &= ~bit;
		}
	}
}

/*
=================
GL_Cull
=================
*/
void GL_Cull( GLenum cull )
{
	if( glState.faceCull == cull )
		return;

	if( !cull )
	{
		pglDisable( GL_CULL_FACE );
		glState.faceCull = 0;
		return;
	}

	if( !glState.faceCull )
		pglEnable( GL_CULL_FACE );
	pglCullFace( cull );
	glState.faceCull = cull;
}

/*
=================
GL_FrontFace
=================
*/
void GL_FrontFace( GLenum front )
{
	pglFrontFace( front ? GL_CW : GL_CCW );
	glState.frontFace = front;
}

/*
==============================================================================

SCREEN SHOTS

==============================================================================
*/
// used for 'env' and 'sky' shots
typedef struct envmap_s
{
	vec3_t	angles;
	int	flags;
} envmap_t;

const envmap_t r_skyBoxInfo[6] =
{
{{   0, 270, 180}, IMAGE_FLIP_X },
{{   0,  90, 180}, IMAGE_FLIP_X },
{{ -90,   0, 180}, IMAGE_FLIP_X },
{{  90,   0, 180}, IMAGE_FLIP_X },
{{   0,   0, 180}, IMAGE_FLIP_X },
{{   0, 180, 180}, IMAGE_FLIP_X },
};

const envmap_t r_envMapInfo[6] =
{
{{  0,   0,  90}, 0 },
{{  0, 180, -90}, 0 },
{{  0,  90,   0}, 0 },
{{  0, 270, 180}, 0 },
{{-90, 180, -90}, 0 },
{{ 90, 180,  90}, 0 }
};

/*
================
VID_ImageAdjustGamma
================
*/
void VID_ImageAdjustGamma( byte *in, uint width, uint height )
{
	int	i, c = width * height;
	float	g = 1.0f / bound( 0.5f, vid_gamma->value, 3.0f );
	byte	r_gammaTable[256];	// adjust screenshot gamma
	byte	*p = in;

	// rebuild the gamma table	
	for( i = 0; i < 256; i++ )
	{
		if ( g == 1.0f ) r_gammaTable[i] = i;
		else r_gammaTable[i] = bound( 0, 255 * pow((i + 0.5)/255.5f, g ) + 0.5, 255 );
	}

	// adjust screenshots gamma
	for( i = 0; i < c; i++, p += 3 )
	{
		p[0] = r_gammaTable[p[0]];
		p[1] = r_gammaTable[p[1]];
		p[2] = r_gammaTable[p[2]];
	}
}

qboolean VID_ScreenShot( const char *filename, int shot_type )
{
	rgbdata_t *r_shot;
	uint	flags = IMAGE_FLIP_Y;
	int	width = 0, height = 0;
	qboolean	result;

	r_shot = Mem_Alloc( r_temppool, sizeof( rgbdata_t ));
	r_shot->width = glState.width;
	r_shot->height = glState.height;
	r_shot->flags = IMAGE_HAS_COLOR;
	r_shot->type = PF_RGB_24;
	r_shot->size = r_shot->width * r_shot->height * PFDesc( r_shot->type )->bpp;
	r_shot->palette = NULL;
	r_shot->buffer = Mem_Alloc( r_temppool, glState.width * glState.height * 3 );

	// get screen frame
	pglReadPixels( 0, 0, glState.width, glState.height, GL_RGB, GL_UNSIGNED_BYTE, r_shot->buffer );

	switch( shot_type )
	{
	case VID_SCREENSHOT:
		VID_ImageAdjustGamma( r_shot->buffer, r_shot->width, r_shot->height ); // adjust brightness
		break;
	case VID_LEVELSHOT:
		flags |= IMAGE_RESAMPLE;
		height = 384;
		width = 512;
		break;
	case VID_MINISHOT:
		flags |= IMAGE_RESAMPLE;
		height = 200;
		width = 320;
		break;
	}

	Image_Process( &r_shot, width, height, flags );

	// write image
	result = FS_SaveImage( filename, r_shot );
	FS_FreeImage( r_shot );

	return result;
}

/*
=================
VID_CubemapShot
=================
*/
qboolean VID_CubemapShot( const char *base, uint size, const float *vieworg, qboolean skyshot )
{
	rgbdata_t		*r_shot, *r_side;
	byte		*temp = NULL;
	byte		*buffer = NULL;
	string		basename;
	int		i = 1, flags, result;

	if( !RI.drawWorld || !cl.worldmodel )
		return false;

	// make sure the specified size is valid
	while( i < size ) i<<=1;

	if( i != size ) return false;
	if( size > glState.width || size > glState.height )
		return false;

	// setup refdef
	RI.params |= RP_ENVVIEW;	// do not render non-bmodel entities

	// alloc space
	temp = Mem_Alloc( r_temppool, size * size * 3 );
	buffer = Mem_Alloc( r_temppool, size * size * 3 * 6 );
	r_shot = Mem_Alloc( r_temppool, sizeof( rgbdata_t ));
	r_side = Mem_Alloc( r_temppool, sizeof( rgbdata_t ));

	// use client vieworg
	if( !vieworg ) vieworg = r_lastRefdef.vieworg;

	for( i = 0; i < 6; i++ )
	{
		if( skyshot )
		{
			R_DrawCubemapView( vieworg, r_skyBoxInfo[i].angles, size );
			flags = r_skyBoxInfo[i].flags;
		}
		else
		{
			R_DrawCubemapView( vieworg, r_envMapInfo[i].angles, size );
			flags = r_envMapInfo[i].flags;
                    }

		pglReadPixels( 0, glState.height - size, size, size, GL_RGB, GL_UNSIGNED_BYTE, temp );
		r_side->flags = IMAGE_HAS_COLOR;
		r_side->width = r_side->height = size;
		r_side->type = PF_RGB_24;
		r_side->size = r_side->width * r_side->height * 3;
		r_side->buffer = temp;

		if( flags ) Image_Process( &r_side, 0, 0, flags );
		Mem_Copy( buffer + (size * size * 3 * i), r_side->buffer, size * size * 3 );
	}

	RI.params &= ~RP_ENVVIEW;

	r_shot->flags = IMAGE_HAS_COLOR;
	r_shot->flags |= (skyshot) ? IMAGE_SKYBOX : IMAGE_CUBEMAP;
	r_shot->width = size;
	r_shot->height = size;
	r_shot->type = PF_RGB_24;
	r_shot->size = r_shot->width * r_shot->height * 3 * 6;
	r_shot->palette = NULL;
	r_shot->buffer = buffer;

	// make sure what we have right extension
	com.strncpy( basename, base, MAX_STRING );
	FS_StripExtension( basename );
	FS_DefaultExtension( basename, va( ".%s", SI->envshot_ext ));

	// write image as dds packet
	result = FS_SaveImage( basename, r_shot );
	FS_FreeImage( r_shot );
	FS_FreeImage( r_side );

	return result;
}

//=======================================================

/*
===============
R_ShowTextures

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.
===============
*/
void R_ShowTextures( void )
{
	gltexture_t	*image;
	float		x, y, w, h;
	int		i, j, base_w, base_h;

	if( !gl_showtextures->integer )
		return;

	if( !glState.in2DMode )
	{
		R_Set2DMode( true );
	}

	pglClear( GL_COLOR_BUFFER_BIT );
	pglFinish();

	if( gl_showtextures->integer == TEX_LIGHTMAP )
	{
		// draw lightmaps as big images
		base_w = 5;
		base_h = 4;
	}
	else
	{
		base_w = 16;
		base_h = 12;
	}

	for( i = j = 0; i < MAX_TEXTURES; i++ )
	{
		image = R_GetTexture( i );
		if( !image->texnum ) continue;

		if( image->texType != gl_showtextures->integer )
			continue;

		w = glState.width / base_w;
		h = glState.height / base_h;
		x = j % base_w * w;
		y = j / base_w * h;

		pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		GL_Bind( GL_TEXTURE0, image->texnum );
		GL_CheckForErrors();

		pglBegin( GL_QUADS );
		pglTexCoord2f( 0, 0 );
		pglVertex2f( x, y );
		pglTexCoord2f( 1, 0 );
		pglVertex2f( x + w, y );
		pglTexCoord2f( 1, 1 );
		pglVertex2f( x + w, y + h );
		pglTexCoord2f( 0, 1 );
		pglVertex2f( x, y + h );
		pglEnd();
		j++;
	}
	pglFinish();

	GL_CheckForErrors ();
}

//=======================================================