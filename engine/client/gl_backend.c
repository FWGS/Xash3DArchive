//=======================================================================
//			Copyright XashXT Group 2010 �
//		        gl_backend.c - rendering backend
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"
#include "matrix_lib.h"

char		r_speeds_msg[MAX_SYSPATH];
ref_speeds_t	r_stats;	// r_speeds counters

/*
===============
R_SpeedsMessage
===============
*/
qboolean R_SpeedsMessage( char *out, size_t size )
{
	if( r_speeds->integer <= 0 ) return false;
	if( !out || !size ) return false;

	com.strncpy( out, r_speeds_msg, size );
	return true;
}

/*
==============
GL_BackendStartFrame
==============
*/
void GL_BackendStartFrame( void )
{
	r_speeds_msg[0] = '\0';
	Mem_Set( &r_stats, 0, sizeof( r_stats ));
}

/*
==============
GL_BackendEndFrame
==============
*/
void GL_BackendEndFrame( void )
{
	if( r_speeds->integer <= 0 || !RI.drawWorld )
		return;

	switch( r_speeds->integer )
	{
	case 5:
		com.snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i studio models drawn\n%3i sprites drawn",
		r_stats.c_studio_models_drawn, r_stats.c_sprite_models_drawn );
		break;
	case 6:
		com.snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i static entities\n%3i normal entities",
		r_numStatics, r_numEntities );
		break;
	}
}

/*
=================
GL_LoadTexMatrix
=================
*/
void GL_LoadTexMatrix( const matrix4x4 m )
{
	pglMatrixMode( GL_TEXTURE );
	GL_LoadMatrix( m );
	glState.texIdentityMatrix[glState.activeTMU] = false;
}

/*
=================
GL_LoadMatrix
=================
*/
void GL_LoadMatrix( const matrix4x4 source )
{
	GLfloat	dest[16];

	Matrix4x4_ToArrayFloatGL( source, dest );
	pglLoadMatrixf( dest );
}

/*
=================
GL_LoadIdentityTexMatrix
=================
*/
void GL_LoadIdentityTexMatrix( void )
{
	if( glState.texIdentityMatrix[glState.activeTMU] )
		return;

	pglMatrixMode( GL_TEXTURE );
	pglLoadIdentity();
	glState.texIdentityMatrix[glState.activeTMU] = true;
}

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
==============
GL_DisableAllTexGens
==============
*/
void GL_DisableAllTexGens( void )
{
	GL_TexGen( GL_S, 0 );
	GL_TexGen( GL_T, 0 );
	GL_TexGen( GL_R, 0 );
	GL_TexGen( GL_Q, 0 );
}

/*
==============
GL_CleanUpTextureUnits
==============
*/
void GL_CleanUpTextureUnits( int last )
{
	int	i;

	for( i = glState.activeTMU; i > last - 1; i-- )
	{
		GL_DisableAllTexGens();
		pglDisable( GL_TEXTURE_2D );
		GL_SelectTexture( i - 1 );
	}
}

/*
=================
GL_MultiTexCoord2f
=================
*/
void GL_MultiTexCoord2f( GLenum texture, GLfloat s, GLfloat t )
{
	if( pglMultiTexCoord2f )
	{
		pglMultiTexCoord2f( texture + GL_TEXTURE0_ARB, s, t );
	}
	else if( pglMTexCoord2fSGIS )
	{
		pglMTexCoord2fSGIS( texture + GL_TEXTURE0_SGIS, s, t );
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
=================
GL_SetState
=================
*/
void GL_SetState( int state )
{
	int diff;

	if( glState.in2DMode )
		state |= GLSTATE_NO_DEPTH_TEST;

	if( state & GLSTATE_NO_DEPTH_TEST )
		state &= ~( GLSTATE_DEPTHWRITE|GLSTATE_DEPTHFUNC_EQ );

	diff = glState.flags ^ state;
	if( !diff ) return;

	if( diff & ( GLSTATE_BLEND_MTEX|GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK ))
	{
		if( state & ( GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK ))
		{
			int	blendsrc, blenddst;

			switch( state & GLSTATE_SRCBLEND_MASK )
			{
			case GLSTATE_SRCBLEND_ZERO:
				blendsrc = GL_ZERO;
				break;
			case GLSTATE_SRCBLEND_DST_COLOR:
				blendsrc = GL_DST_COLOR;
				break;
			case GLSTATE_SRCBLEND_ONE_MINUS_DST_COLOR:
				blendsrc = GL_ONE_MINUS_DST_COLOR;
				break;
			case GLSTATE_SRCBLEND_SRC_ALPHA:
				blendsrc = GL_SRC_ALPHA;
				break;
			case GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				blendsrc = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLSTATE_SRCBLEND_DST_ALPHA:
				blendsrc = GL_DST_ALPHA;
				break;
			case GLSTATE_SRCBLEND_ONE_MINUS_DST_ALPHA:
				blendsrc = GL_ONE_MINUS_DST_ALPHA;
				break;
			case GLSTATE_SRCBLEND_ONE:
			default:
				blendsrc = GL_ONE;
				break;
			}

			switch( state & GLSTATE_DSTBLEND_MASK )
			{
			case GLSTATE_DSTBLEND_ONE:
				blenddst = GL_ONE;
				break;
			case GLSTATE_DSTBLEND_SRC_COLOR:
				blenddst = GL_SRC_COLOR;
				break;
			case GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR:
				blenddst = GL_ONE_MINUS_SRC_COLOR;
				break;
			case GLSTATE_DSTBLEND_SRC_ALPHA:
				blenddst = GL_SRC_ALPHA;
				break;
			case GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				blenddst = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLSTATE_DSTBLEND_DST_ALPHA:
				blenddst = GL_DST_ALPHA;
				break;
			case GLSTATE_DSTBLEND_ONE_MINUS_DST_ALPHA:
				blenddst = GL_ONE_MINUS_DST_ALPHA;
				break;
			case GLSTATE_DSTBLEND_ZERO:
			default:
				blenddst = GL_ZERO;
				break;
			}

			if( state & GLSTATE_BLEND_MTEX )
			{
				if( glState.currentEnvModes[glState.activeTMU] != GL_REPLACE )
					pglEnable( GL_BLEND );
				else pglDisable( GL_BLEND );
			}
			else
			{
				pglEnable( GL_BLEND );
			}

			pglBlendFunc( blendsrc, blenddst );
		}
		else
		{
			pglDisable( GL_BLEND );
		}
	}

	if( diff & GLSTATE_ALPHAFUNC )
	{
		if( state & GLSTATE_ALPHAFUNC )
		{
			if(!( glState.flags & GLSTATE_ALPHAFUNC ))
				pglEnable( GL_ALPHA_TEST );

			if( state & GLSTATE_AFUNC_GT0 )
				pglAlphaFunc( GL_GREATER, 0 );
			else if( state & GLSTATE_AFUNC_LT128 )
				pglAlphaFunc( GL_LESS, 0.5f );
			else pglAlphaFunc( GL_GEQUAL, 0.5f );
		}
		else
		{
			pglDisable( GL_ALPHA_TEST );
		}
	}

	if( diff & GLSTATE_DEPTHFUNC_EQ )
	{
		if( state & GLSTATE_DEPTHFUNC_EQ )
			pglDepthFunc( GL_EQUAL );
		else pglDepthFunc( GL_LEQUAL );
	}

	if( diff & GLSTATE_DEPTHWRITE )
	{
		if( state & GLSTATE_DEPTHWRITE )
			pglDepthMask( GL_TRUE );
		else pglDepthMask( GL_FALSE );
	}

	if( diff & GLSTATE_NO_DEPTH_TEST )
	{
		if( state & GLSTATE_NO_DEPTH_TEST )
			pglDisable( GL_DEPTH_TEST );
		else pglEnable( GL_DEPTH_TEST );
	}

	if( diff & GLSTATE_OFFSET_FILL )
	{
		if( state & GLSTATE_OFFSET_FILL )
			pglEnable( GL_POLYGON_OFFSET_FILL );
		else pglDisable( GL_POLYGON_OFFSET_FILL );
	}

	glState.flags = state;
}

void GL_SetRenderMode( int mode )
{
	int	state, texEnv;

	switch( mode )
	{
	case kRenderNormal:
	default:
		state = GLSTATE_DEPTHWRITE;
		texEnv = GL_MODULATE;
		break;
	case kRenderTransColor:
		state = GLSTATE_SRCBLEND_DST_COLOR|GLSTATE_DSTBLEND_SRC_COLOR|GLSTATE_DEPTHWRITE;
		texEnv = GL_MODULATE;
		break;
	case kRenderTransAlpha:
		state = GLSTATE_AFUNC_GE128|GLSTATE_DEPTHWRITE;
		texEnv = GL_MODULATE;
		break;
	case kRenderTransTexture:
		state = GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		texEnv = GL_MODULATE;
		break;
	case kRenderGlow:
		state = GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE|GLSTATE_NO_DEPTH_TEST;
		texEnv = GL_MODULATE;
		break;
	case kRenderTransAdd:
		state = GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE;
		texEnv = GL_MODULATE;
		break;
	case kRenderTransInverse:
		state = GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA|GLSTATE_DSTBLEND_SRC_ALPHA;
		texEnv = GL_MODULATE;
		break;
	}

	GL_TexEnv( texEnv );
	GL_SetState( state );
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