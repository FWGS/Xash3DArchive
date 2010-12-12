//=======================================================================
//			Copyright XashXT Group 2010 ©
//		       gl_warp.c - sky and water polygons
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"
#include "com_model.h"
#include "wadfile.h"

#define TURBSCALE		( 256.0f / ( 2 * M_PI ))
static float		speedscale;

// speed up sin calculations
float r_turbsin[] =
{
	#include "warpsin.h"
};

/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox( void )
{
	int	i;

	RI.params |= RP_NOSKY;
	for( i = 0; i < 6; i++ )
	{
		RI.skyMins[0][i] = RI.skyMins[1][i] = 9999999;
		RI.skyMaxs[0][i] = RI.skyMaxs[1][i] = -9999999;
	}
}

/*
==============
R_DrawSkybox
==============
*/
void R_DrawSkyBox( void )
{
}

/*
===============
R_SetupSky
===============
*/
void R_SetupSky( const char *skyboxname )
{
}

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_InitSky( mip_t *mt, texture_t *tx )
{
	rgbdata_t	r_temp, *r_sky;
	uint	*trans, *rgba;
	uint	transpix;
	int	r, g, b;
	int	i, j, p;

	if( mt->offsets[0] > 0 )
	{
		// NOTE: imagelib detect miptex version by size
		// 770 additional bytes is indicated custom palette
		int size = (int)sizeof( mip_t ) + ((mt->width * mt->height * 85)>>6);
		if( world.version == HLBSP_VERSION ) size += sizeof( short ) + 768;

		r_sky = FS_LoadImage( tx->name, (byte *)mt, size );
	}
	else
	{
		// okay, loading it from wad
		r_sky = FS_LoadImage( tx->name, NULL, 0 );
	}

	// make sure what sky image is valid
	if( !r_sky || !r_sky->palette || r_sky->type != PF_INDEXED_32 )
	{
		MsgDev( D_ERROR, "R_InitSky: unable to load sky texture %s\n", tx->name );
		FS_FreeImage( r_sky );
		return;
	}

	// make an average value for the back to avoid
	// a fringe on the top level
	trans = Mem_Alloc( r_temppool, r_sky->height * r_sky->height * sizeof( *trans ));

	r = g = b = 0;
	for( i = 0; i < r_sky->width >> 1; i++ )
	{
		for( j = 0; j < r_sky->height; j++ )
		{
			p = r_sky->buffer[i * r_sky->width + j + r_sky->height];
			rgba = (uint *)r_sky->palette + p;
			trans[(i * r_sky->height) + j] = *rgba;
			r += ((byte *)rgba)[0];
			g += ((byte *)rgba)[1];
			b += ((byte *)rgba)[2];
		}
	}

	((byte *)&transpix)[0] = r / ( r_sky->height * r_sky->height );
	((byte *)&transpix)[1] = g / ( r_sky->height * r_sky->height );
	((byte *)&transpix)[2] = b / ( r_sky->height * r_sky->height );
	((byte *)&transpix)[3] = 0;

	// build a temporary image
	r_temp = *r_sky;
	r_temp.width = r_sky->width >> 1;
	r_temp.height = r_sky->height;
	r_temp.type = PF_RGBA_32;
	r_temp.flags = IMAGE_HAS_COLOR;
	r_temp.palette = NULL;
	r_temp.buffer = (byte *)trans;
	r_temp.size = r_temp.width * r_temp.height * 4;

	// load it in
	tr.solidskyTexture = GL_LoadTextureInternal( "solid_sky", &r_temp, TF_SKY, false );

	for( i = 0; i < r_sky->width >> 1; i++ )
	{
		for( j = 0; j < r_sky->height; j++ )
		{
			p = r_sky->buffer[i * r_sky->width + j];
			if( p == 0 )
			{
				trans[(i * r_sky->height) + j] = transpix;
			}
			else
			{         
				rgba = (uint *)r_sky->palette + p;
				trans[(i * r_sky->height) + j] = *rgba;
			}
		}
	}

	r_temp.flags = IMAGE_HAS_COLOR|IMAGE_HAS_ALPHA;

	// load it in
	tr.alphaskyTexture = GL_LoadTextureInternal( "alpha_sky", &r_temp, TF_SKY, false );

	GL_SetTextureType( tr.solidskyTexture, TEX_SKYBOX );
	GL_SetTextureType( tr.alphaskyTexture, TEX_SKYBOX );

	// clean up
	FS_FreeImage( r_sky );
	Mem_Free( trans );
}

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
void EmitWaterPolys( glpoly_t *polys )
{
	glpoly_t	*p;
	float	*v, nv, waveHeight;
	float	s, t, os, ot;
	int	i;

	// set the current waveheight
	waveHeight = RI.currentWaveHeight;

	for( p = polys; p; p = p->next )
	{
		pglBegin( GL_POLYGON );

		for( i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE )
		{
			if( waveHeight )
			{
				nv = v[2] + waveHeight + ( waveHeight * com.sin(v[0] * 0.02 + cl.time)
					* com.sin(v[1] * 0.02 + cl.time) * com.sin(v[2] * 0.02 + cl.time));
				nv -= waveHeight;
			}
			else nv = v[2];

			os = v[3];
			ot = v[4];

			s = os + r_turbsin[(int)((ot * 0.125f + cl.time ) * TURBSCALE) & 255];
			s *= ( 1.0f / SUBDIVIDE_SIZE );

			t = ot + r_turbsin[(int)((os * 0.125f + cl.time ) * TURBSCALE) & 255];
			t *= ( 1.0f / SUBDIVIDE_SIZE );

			pglTexCoord2f( s, t );
			pglVertex3f( v[0], v[1], nv );
		}
		pglEnd();
	}
}

/*
=============
EmitSkyPolys
=============
*/
void EmitSkyPolys( msurface_t *fa )
{
	glpoly_t	*p;
	float	*v;
	int	i;
	float	s, t;
	vec3_t	dir;
	float	length;

	for( p = fa->polys; p; p = p->next )
	{
		pglBegin( GL_POLYGON );

		for( i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE )
		{
			VectorSubtract( v, RI.vieworg, dir );
			dir[2] *= 3; // flatten the sphere

			length = VectorLength( dir );
			length = 6 * 63 / length;

			dir[0] *= length;
			dir[1] *= length;

			s = ( speedscale + dir[0] ) * (1.0f / 128);
			t = ( speedscale + dir[1] ) * (1.0f / 128);

			pglTexCoord2f( s, t );
			pglVertex3fv( v );
		}
		pglEnd ();
	}
}

/*
=================
R_DrawSkyChain
=================
*/
void R_DrawSkyChain( msurface_t *s )
{
	msurface_t	*fa;

	GL_Bind( GL_TEXTURE0, tr.solidskyTexture );

	speedscale = cl.time * 8;
	speedscale -= (int)speedscale & ~127;

	for( fa = s; fa; fa = fa->texturechain )
		EmitSkyPolys( fa );

	pglEnable( GL_BLEND );
	GL_Bind( GL_TEXTURE0, tr.alphaskyTexture );

	speedscale = cl.time * 16;
	speedscale -= (int)speedscale & ~127;

	for( fa = s; fa; fa = fa->texturechain )
		EmitSkyPolys( fa );

	pglDisable( GL_BLEND );
}

/*
===============
EmitBothSkyLayers

Does a sky warp on the pre-fragmented glpoly_t chain
This will be called for brushmodels, the world
will have them chained together.
===============
*/
void EmitSkyLayers( msurface_t *fa )
{
	GL_Bind( GL_TEXTURE0, tr.solidskyTexture );

	speedscale = cl.time * 8;
	speedscale -= (int)speedscale & ~127;

	EmitSkyPolys( fa );

	pglEnable( GL_BLEND );
	GL_Bind( GL_TEXTURE0, tr.alphaskyTexture );

	speedscale = cl.time * 16;
	speedscale -= (int)speedscale & ~127;

	EmitSkyPolys( fa );

	pglDisable( GL_BLEND );
}