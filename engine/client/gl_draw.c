//=======================================================================
//			Copyright XashXT Group 2010 ©
//		       gl_draw.c - orthogonal drawing stuff
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"

/*
===============
R_DrawSetColor
===============
*/
void R_DrawSetColor( const rgba_t color )
{
	if( color ) Vector4Copy( color, glState.draw_color );
	else Vector4Set( glState.draw_color, 255, 255, 255, 255 );
}

/*
=============
R_DrawStretchPic
=============
*/
void R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, int texnum )
{
	GL_Bind( GL_TEXTURE0, texnum );
	pglColor4ubv( glState.draw_color ); 

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
		while( width < cols ) width <<= 1;
		while( height < rows ) height <<= 1;

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
	GL_Bind( 0, tr.cinTexture );

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
//		GL_SetState( GLSTATE_NO_DEPTH_TEST );

		pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

		glState.in2DMode = true;
		RI.currententity = NULL;
		RI.currentmodel = NULL;
	}
	else
	{
		glState.in2DMode = false;
	}
}