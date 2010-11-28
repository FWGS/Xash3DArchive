//=======================================================================
//			Copyright XashXT Group 2010 ©
//		  gl_image.c - texture uploading and processing
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"

#define MAX_TEXTURES		1024
#define TEXTURES_HASH_SIZE		64

// used for 'env' and 'sky' shots
typedef struct envmap_s
{
	vec3_t	angles;
	int	flags;
} envmap_t;

static int	r_textureMinFilter = GL_LINEAR_MIPMAP_LINEAR;
static int	r_textureMagFilter = GL_LINEAR;
static int	r_textureDepthFilter = GL_LINEAR;

static gltexture_t	r_textures[MAX_TEXTURES];
static gltexture_t	*r_texturesHashTable[TEXTURES_HASH_SIZE];
static int	r_numTextures;
static byte	*r_imagepool;		// immediate buffers
static byte	*r_texpool;		// texture_t permanent chain
static byte	data2D[256*256*4];		// intermediate texbuffer
static rgbdata_t	r_image;			// generic pixelbuffer used for internal textures

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

static struct
{
	char		name[64];
	pixformat_t	format;
	int		width;
	int		height;
	int		bpp;
	int		bpc;
	int		bps;
	int		SizeOfPlane;
	int		SizeOfData;
	int		SizeOfFile;
	int		depth;
	int		numSides;
	int		MipCount;
	int		BitsCount;
	GLuint		glFormat;
	GLuint		glType;
	GLuint		glTarget;
	GLuint		glSamples;
	GLuint		texTarget;
	texType_t		texType;

	uint		tflags;	// TF_ flags	
	uint		flags;	// IMAGE_ flags
	byte		*pal;
	byte		*source;
	byte		*scaled;
} image_desc;

/*
=================
R_SetTextureParameters
=================
*/
void R_SetTextureParameters( void )
{
	gltexture_t	*texture;
	int		i;

	if( !com.stricmp( gl_texturemode->string, "GL_NEAREST" ))
	{
		r_textureMinFilter = GL_NEAREST;
		r_textureMagFilter = GL_NEAREST;
	}
	else if( !com.stricmp( gl_texturemode->string, "GL_LINEAR" ))
	{
		r_textureMinFilter = GL_LINEAR;
		r_textureMagFilter = GL_LINEAR;
	}
	else if( !com.stricmp( gl_texturemode->string, "GL_NEAREST_MIPMAP_NEAREST" ))
	{
		r_textureMinFilter = GL_NEAREST_MIPMAP_NEAREST;
		r_textureMagFilter = GL_NEAREST;
	}
	else if( !com.stricmp( gl_texturemode->string, "GL_LINEAR_MIPMAP_NEAREST" ))
	{
		r_textureMinFilter = GL_LINEAR_MIPMAP_NEAREST;
		r_textureMagFilter = GL_LINEAR;
	}
	else if( !com.stricmp( gl_texturemode->string, "GL_NEAREST_MIPMAP_LINEAR" ))
	{
		r_textureMinFilter = GL_NEAREST_MIPMAP_LINEAR;
		r_textureMagFilter = GL_NEAREST;
	}
	else if( !com.stricmp( gl_texturemode->string, "GL_LINEAR_MIPMAP_LINEAR" ))
	{
		r_textureMinFilter = GL_LINEAR_MIPMAP_LINEAR;
		r_textureMagFilter = GL_LINEAR;
	}
	else
	{
		MsgDev( D_ERROR, "gl_texturemode invalid mode %s, defaulting to GL_LINEAR_MIPMAP_LINEAR\n", gl_texturemode->string );
		Cvar_Set( "gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR" );
		r_textureMinFilter = GL_LINEAR_MIPMAP_LINEAR;
		r_textureMagFilter = GL_LINEAR;
	}

	gl_texturemode->modified = false;

	if( GL_Support( GL_ANISOTROPY_EXT ))
	{
		if( gl_texture_anisotropy->value > glConfig.max_texture_anisotropy )
			Cvar_SetFloat( "r_anisotropy", glConfig.max_texture_anisotropy );
		else if( gl_texture_anisotropy->value < 1.0f )
			Cvar_SetFloat( "r_anisotropy", 1.0f );
	}
	gl_texture_anisotropy->modified = false;

	if( GL_Support( GL_TEXTURE_LODBIAS ))
	{
		if( gl_texture_lodbias->value > glConfig.max_texture_lodbias )
			Cvar_SetFloat( "r_texture_lodbias", glConfig.max_texture_lodbias );
		else if( gl_texture_lodbias->value < -glConfig.max_texture_lodbias )
			Cvar_SetFloat( "r_texture_lodbias", -glConfig.max_texture_lodbias );
	}
	gl_texture_lodbias->modified = false;

	// change all the existing mipmapped texture objects
	for( i = 0, texture = r_textures; i < r_numTextures; i++, texture++ )
	{
		if( !texture->texnum ) continue;	// free slot
		GL_Bind( GL_TEXTURE0, texture );

		// set texture filter
		if( texture->flags & TF_DEPTHMAP )
		{
			// set texture filter
			pglTexParameteri( texture->target, GL_TEXTURE_MIN_FILTER, r_textureDepthFilter );
			pglTexParameteri( texture->target, GL_TEXTURE_MAG_FILTER, r_textureDepthFilter );

			// set texture anisotropy if available
			if( GL_Support( GL_ANISOTROPY_EXT ))
				pglTexParameterf( texture->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f );
		}
		else if( texture->flags & TF_NOMIPMAP )
		{
			if( texture->flags & TF_NEAREST )
			{
				pglTexParameteri( texture->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
				pglTexParameteri( texture->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			}
			else
			{
				pglTexParameteri( texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
				pglTexParameteri( texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			}
		}
		else
		{
			if( texture->flags & TF_NEAREST )
			{
				pglTexParameteri( texture->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST );
				pglTexParameteri( texture->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			}
			else
			{
				pglTexParameteri( texture->target, GL_TEXTURE_MIN_FILTER, r_textureMinFilter );
				pglTexParameteri( texture->target, GL_TEXTURE_MAG_FILTER, r_textureMagFilter );
                              }

			// set texture anisotropy if available
			if( GL_Support( GL_ANISOTROPY_EXT ))
				pglTexParameterf( texture->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_anisotropy->value );

			// set texture LOD bias if available
			if( GL_Support( GL_TEXTURE_LODBIAS ))
				pglTexParameterf( texture->target, GL_TEXTURE_LOD_BIAS_EXT, gl_texture_lodbias->value );
		}
	}
}

/*
===============
R_TextureList_f
===============
*/
void R_TextureList_f( void )
{
	gltexture_t	*image;
	int		i, texCount, bytes = 0;

	Msg( "\n" );
	Msg("      -w-- -h-- -size- -fmt- type -filter -wrap-- -name--------\n" );

	for( i = texCount = 0, image = r_textures; i < r_numTextures; i++, image++ )
	{
		if( !image->texnum ) continue;

		bytes += image->size;
		texCount++;

		Msg( "%4i: ", i );
		Msg( "%4i %4i ", image->width, image->height );
		Msg( "%5ik ", image->size >> 10 );

		switch( image->format )
		{
		case GL_COMPRESSED_RGBA_ARB:
			Msg( "CRGBA " );
			break;
		case GL_COMPRESSED_RGB_ARB:
			Msg( "CRGB  " );
			break;
		case GL_COMPRESSED_LUMINANCE_ALPHA_ARB:
			Msg( "CLA   " );
			break;
		case GL_COMPRESSED_LUMINANCE_ARB:
			Msg( "CL    " );
			break;
		case GL_COMPRESSED_ALPHA_ARB:
			Msg( "CA    " );
			break;
		case GL_COMPRESSED_INTENSITY_ARB:
			Msg( "CI    " );
			break;
		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			Msg( "DXT1  " );
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
			Msg( "DXT3  " );
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
			Msg( "DXT5  " );
			break;
		case GL_RGBA:
			Msg( "RGBA  " );
			break;
		case GL_RGBA8:
			Msg( "RGBA8 " );
			break;
		case GL_RGBA4:
			Msg( "RGBA4 " );
			break;
		case GL_RGB:
			Msg( "RGB   " );
			break;
		case GL_RGB8:
			Msg( "RGB8  " );
			break;
		case GL_RGB5:
			Msg( "RGB5  " );
			break;
		case GL_LUMINANCE8_ALPHA8:
			Msg( "L8A8  " );
			break;
		case GL_LUMINANCE8:
			Msg( "L8    " );
			break;
		case GL_ALPHA8:
			Msg( "A8    " );
			break;
		case GL_INTENSITY8:
			Msg( "I8    " );
			break;
		default:
			Msg( "????? " );
			break;
		}

		switch( image->target )
		{
		case GL_TEXTURE_2D:
			Msg( " 2D  " );
			break;
		case GL_TEXTURE_3D:
			Msg( " 3D  " );
			break;
		case GL_TEXTURE_CUBE_MAP_ARB:
			Msg( "CUBE " );
			break;
		default:
			Msg( "???? " );
			break;
		}

		if( image->flags & TF_NOMIPMAP )
			Msg( "linear " );
		if( image->flags & TF_NEAREST )
			Msg( "nearest" );
		else Msg( "default" );

		if( image->flags & TF_CLAMP )
			Msg( " clamp  " );
		else Msg( " repeat " );
		Msg( "  %s\n", image->name );
	}

	Msg( "---------------------------------------------------------\n" );
	Msg( "%i total textures\n", texCount );
	Msg( "%s total memory used\n", memprint( bytes ));
	Msg( "\n" );
}