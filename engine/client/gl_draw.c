//=======================================================================
//			Copyright XashXT Group 2010 �
//		       gl_draw.c - orthogonal drawing stuff
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"

/*
=============
R_GetImageParms
=============
*/
void R_GetTextureParms( int *w, int *h, int texnum )
{
	gltexture_t	*glt;

	glt = R_GetTexture( texnum );
	if( w ) *w = glt->srcWidth;
	if( h ) *h = glt->srcHeight;
}

/*
=============
R_GetSpriteParms

same as GetImageParms but used
for sprite models
=============
*/
void R_GetSpriteParms( int *frameWidth, int *frameHeight, int *numFrames, int currentFrame, const model_t *pSprite )
{
	mspriteframe_t	*pFrame;

	if( !pSprite || pSprite->type != mod_sprite ) return; // bad model ?
	pFrame = R_GetSpriteFrame( pSprite, currentFrame, 0.0f );

	if( frameWidth ) *frameWidth = pFrame->width;
	if( frameHeight ) *frameHeight = pFrame->height;
	if( numFrames ) *numFrames = pSprite->numframes;
}

int R_GetSpriteTexture( const model_t *m_pSpriteModel, int frame )
{
	if( !m_pSpriteModel || m_pSpriteModel->type != mod_sprite || !m_pSpriteModel->cache.data )
		return 0;

	return R_GetSpriteFrame( m_pSpriteModel, frame, 0.0f )->gl_texturenum;
}

/*
=============
R_DrawStretchPic
=============
*/
void R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, int texnum )
{
	GL_Bind( GL_TEXTURE0, texnum );

	pglBegin( GL_QUADS );
		pglTexCoord2f( s1, t1 );
		pglVertex2f( x, y );

		pglTexCoord2f( s2, t1 );
		pglVertex2f( x + w, y );

		pglTexCoord2f( s2, t2 );
		pglVertex2f( x + w, y + h );

		pglTexCoord2f( s1, t2 );
		pglVertex2f( x, y + h );
	pglEnd();
}

/*
=============
R_DrawStretchRaw
=============
*/
void R_DrawStretchRaw( float x, float y, float w, float h, int cols, int rows, const byte *data, qboolean dirty )
{
	byte		*raw;
	gltexture_t	*tex;

	if( !GL_Support( GL_ARB_TEXTURE_NPOT_EXT ))
	{
		int	width = 1, height = 1;
	
		// check the dimensions
		width = NearestPOW( cols, true );
		height = NearestPOW( rows, false );

		if( cols != width || rows != height )
		{
			raw = GL_ResampleTexture( data, cols, rows, width, height, false );
			cols = width;
			rows = height;
		}
	}
	else
	{
		raw = (byte *)data;
	}

	if( cols > glConfig.max_2d_texture_size )
		Host_Error( "R_DrawStretchRaw: size %i exceeds hardware limits\n", cols );
	if( rows > glConfig.max_2d_texture_size )
		Host_Error( "R_DrawStretchRaw: size %i exceeds hardware limits\n", rows );

	tex = R_GetTexture( tr.cinTexture );
	GL_Bind( GL_TEXTURE0, tr.cinTexture );

	if( cols == tex->width && rows == tex->height )
	{
		if( dirty )
		{
			pglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_BGRA, GL_UNSIGNED_BYTE, raw );
		}
	}
	else
	{
		tex->width = cols;
		tex->height = rows;
		if( dirty )
		{
			pglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, cols, rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, raw );
		}
	}

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
}

/*
===============
R_Set2DMode
===============
*/
void R_Set2DMode( qboolean enable )
{
	if( enable )
	{
		if( glState.in2DMode )
			return;

		// set 2D virtual screen size
		pglScissor( 0, 0, glState.width, glState.height );
		pglViewport( 0, 0, glState.width, glState.height );
		pglMatrixMode( GL_PROJECTION );
		pglLoadIdentity();
		pglOrtho( 0, glState.width, glState.height, 0, -99999, 99999 );
		pglMatrixMode( GL_MODELVIEW );
		pglLoadIdentity();

		GL_Cull( 0 );

		pglDepthMask( GL_FALSE );
		pglDisable( GL_DEPTH_TEST );
		pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

		glState.in2DMode = true;
		RI.currententity = NULL;
		RI.currentmodel = NULL;
	}
	else
	{
		pglEnable( GL_DEPTH_TEST );
		pglMatrixMode( GL_MODELVIEW );
		glState.in2DMode = false;
	}
}