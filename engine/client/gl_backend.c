//=======================================================================
//			Copyright XashXT Group 2010 ©
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
GL_Bind
=================
*/
void GL_Bind( GLenum tmu, gltexture_t *texture )
{
	GL_SelectTexture( tmu );

	ASSERT( texture != NULL );

	if( gl_nobind->integer ) texture = tr.defaultTexture;	// performance evaluation option
	if( glState.currentTextures[tmu] == texture->texnum )
		return;

	glState.currentTextures[tmu] = texture->texnum;
	pglBindTexture( texture->target, texture->texnum );
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
