//=======================================================================
//			Copyright XashXT Group 2010 ©
//		       gl_draw.c - orthogonal drawing stuff
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"

/*
=================
R_ResampleRaw
=================
*/
static byte *R_ResampleRaw( const byte *source, int inWidth, int inHeight, int outWidth, int outHeight )
{
	uint		frac, fracStep;
	uint		*in = (uint *)source;
	uint		p1[0x1000], p2[0x1000];
	byte		*pix1, *pix2, *pix3, *pix4;
	uint		*inRow1, *inRow2, *out;
	static byte	*resampled = NULL;
	int		i, x, y;

	// clean frames doesn't contain raw buffer
	if( !source ) return NULL;

	resampled = Mem_Realloc( r_temppool, resampled, outWidth * outHeight * 4 );
	out = (uint *)resampled;

	fracStep = inWidth * 0x10000 / outWidth;

	frac = fracStep >> 2;
	for( i = 0; i < outWidth; i++ )
	{
		p1[i] = 4 * (frac >> 16);
		frac += fracStep;
	}

	frac = (fracStep >> 2) * 3;
	for( i = 0; i < outWidth; i++ )
	{
		p2[i] = 4 * (frac >> 16);
		frac += fracStep;
	}

	for( y = 0; y < outHeight; y++, out += outWidth )
	{
		inRow1 = in + inWidth * (int)(((float)y + 0.25) * inHeight/outHeight);
		inRow2 = in + inWidth * (int)(((float)y + 0.75) * inHeight/outHeight);

		for( x = 0; x < outWidth; x++ )
		{
			pix1 = (byte *)inRow1 + p1[x];
			pix2 = (byte *)inRow1 + p2[x];
			pix3 = (byte *)inRow2 + p1[x];
			pix4 = (byte *)inRow2 + p2[x];

			((byte *)(out+x))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			((byte *)(out+x))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			((byte *)(out+x))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			((byte *)(out+x))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
		}
	}
	return resampled;
}

/*
=============
R_DrawStretchRaw
=============
*/
void R_DrawStretchRaw( float x, float y, float w, float h, int cols, int rows, const byte *data, qboolean dirty )
{
	byte	*raw;

	if( !GL_Support( GL_ARB_TEXTURE_NPOT_EXT ))
	{
		int	width = 1, height = 1;
	
		// check the dimensions
		while( width < cols ) width <<= 1;
		while( height < rows ) height <<= 1;

		if( cols != width || rows != height )
		{
			raw = R_ResampleRaw( data, cols, rows, width, height );
			cols = width;
			rows = height;
		}
	}
	else
	{
		raw = (byte *)data;
	}

	if( cols > glConfig.max_2d_texture_size )
		Host_Error( "R_DrawStretchRaw: size exceeds hardware limits (%i > %i)\n", cols, glConfig.max_2d_texture_size ); 
	if( rows > glConfig.max_2d_texture_size )
		Host_Error( "R_DrawStretchRaw: size exceeds hardware limits (%i > %i)\n", rows, glConfig.max_2d_texture_size );

	GL_Bind( 0, tr.cinTexture );

	if( cols == tr.cinTexture->width && rows == tr.cinTexture->height )
	{
		if( dirty ) pglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_BGRA, GL_UNSIGNED_BYTE, raw );
	}
	else
	{
		tr.cinTexture->width = cols;
		tr.cinTexture->height = rows;
		if( dirty ) pglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, cols, rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, raw );
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