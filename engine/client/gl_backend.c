/*
gl_backend.c - rendering backend
Copyright (C) 2010 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"
#include "client.h"
#include "gl_local.h"
#include "mathlib.h"

char		r_speeds_msg[MAX_SYSPATH];
ref_speeds_t	r_stats;	// r_speeds counters

/*
===============
R_SpeedsMessage
===============
*/
qboolean R_SpeedsMessage( char *out, size_t size )
{
	if( clgame.drawFuncs.R_SpeedsMessage != NULL )
	{
		if( clgame.drawFuncs.R_SpeedsMessage( out, size ))
			return true;
		// otherwise pass to default handler
	}

	if( r_speeds->value <= 0 ) return false;
	if( !out || !size ) return false;

	Q_strncpy( out, r_speeds_msg, size );

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
}

/*
==============
GL_BackendEndFrame
==============
*/
void GL_BackendEndFrame( void )
{
	if( r_speeds->value <= 0 || !RI.drawWorld )
		return;

	switch( (int)r_speeds->value )
	{
	case 1:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i wpoly, %3i apoly\n%3i epoly, %3i spoly",
		r_stats.c_world_polys, r_stats.c_alias_polys, r_stats.c_studio_polys, r_stats.c_sprite_polys );
		break;		
	case 2:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "visible leafs:\n%3i leafs\ncurrent leaf %3i",
		r_stats.c_world_leafs, Mod_PointInLeaf( RI.pvsorigin, cl.worldmodel->nodes ) - cl.worldmodel->leafs );
		break;
	case 3:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i alias models drawn\n%3i studio models drawn\n%3i sprites drawn",
		r_stats.c_alias_models_drawn, r_stats.c_studio_models_drawn, r_stats.c_sprite_models_drawn );
		break;
	case 4:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i static entities\n%3i normal entities\n%3i server entities",
		r_numStatics, r_numEntities - r_numStatics, pfnNumberOfEntities( ));
		break;
	case 5:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i tempents\n%3i viewbeams\n%3i particles",
		r_stats.c_active_tents_count, r_stats.c_view_beams_count, r_stats.c_particle_count );
		break;
	}

	memset( &r_stats, 0, sizeof( r_stats ));
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
GL_LoadTexMatrixExt
=================
*/
void GL_LoadTexMatrixExt( const float *glmatrix )
{
	ASSERT( glmatrix != NULL );
	pglMatrixMode( GL_TEXTURE );
	pglLoadMatrixf( glmatrix );
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
void GL_SelectTexture( GLint tmu )
{
	if( !GL_Support( GL_ARB_MULTITEXTURE ))
		return;

	// don't allow negative texture units
	if( tmu < 0 ) return;

	if( tmu >= GL_MaxTextureUnits( ))
	{
		MsgDev( D_ERROR, "GL_SelectTexture: bad tmu state %i\n", tmu );
		return; 
	}

	if( glState.activeTMU == tmu )
		return;

	glState.activeTMU = tmu;

	if( pglActiveTextureARB )
	{
		pglActiveTextureARB( tmu + GL_TEXTURE0_ARB );

		if( tmu < glConfig.max_texture_coords )
			pglClientActiveTextureARB( tmu + GL_TEXTURE0_ARB );
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

	for( i = glState.activeTMU; i > (last - 1); i-- )
	{
		// disable upper units
		if( glState.currentTextureTargets[i] != GL_NONE )
		{
			pglDisable( glState.currentTextureTargets[i] );
			glState.currentTextureTargets[i] = GL_NONE;
			glState.currentTextures[i] = -1; // unbind texture
		}

		GL_SetTexCoordArrayMode( GL_NONE );
		GL_LoadIdentityTexMatrix();
		GL_DisableAllTexGens();
		GL_SelectTexture( i - 1 );
	}
}

/*
==============
GL_CleanupAllTextureUnits
==============
*/
void GL_CleanupAllTextureUnits( void )
{
	// force to cleanup all the units
	GL_SelectTexture( GL_MaxTextureUnits() - 1 );
	GL_CleanUpTextureUnits( 0 );
}

/*
=================
GL_MultiTexCoord2f
=================
*/
void GL_MultiTexCoord2f( GLenum texture, GLfloat s, GLfloat t )
{
	if( !GL_Support( GL_ARB_MULTITEXTURE ))
		return;

	if( pglMultiTexCoord2f )
	{
		pglMultiTexCoord2f( texture + GL_TEXTURE0_ARB, s, t );
	}
}

/*
=================
GL_TextureTarget
=================
*/
void GL_TextureTarget( uint target )
{
	if( glState.activeTMU < 0 || glState.activeTMU >= GL_MaxTextureUnits( ))
	{
		MsgDev( D_ERROR, "GL_TextureTarget: bad tmu state %i\n", glState.activeTMU );
		return; 
	}

	if( glState.currentTextureTargets[glState.activeTMU] != target )
	{
		if( glState.currentTextureTargets[glState.activeTMU] != GL_NONE )
			pglDisable( glState.currentTextureTargets[glState.activeTMU] );
		glState.currentTextureTargets[glState.activeTMU] = target;
		if( target != GL_NONE )
			pglEnable( glState.currentTextureTargets[glState.activeTMU] );
	}
}

/*
=================
GL_TexGen
=================
*/
void GL_TexGen( GLenum coord, GLenum mode )
{
	int	tmu = min( glConfig.max_texture_coords, glState.activeTMU );
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
		if( !( glState.genSTEnabled[tmu] & bit ))
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
GL_SetTexCoordArrayMode
=================
*/
void GL_SetTexCoordArrayMode( GLenum mode )
{
	int	tmu = min( glConfig.max_texture_coords, glState.activeTMU );
	int	bit, cmode = glState.texCoordArrayMode[tmu];

	if( mode == GL_TEXTURE_COORD_ARRAY )
		bit = 1;
	else if( mode == GL_TEXTURE_CUBE_MAP_ARB )
		bit = 2;
	else bit = 0;

	if( cmode != bit )
	{
		if( cmode == 1 ) pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		else if( cmode == 2 ) pglDisable( GL_TEXTURE_CUBE_MAP_ARB );

		if( bit == 1 ) pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		else if( bit == 2 ) pglEnable( GL_TEXTURE_CUBE_MAP_ARB );

		glState.texCoordArrayMode[tmu] = bit;
	}
}

/*
=================
GL_Cull
=================
*/
void GL_Cull( GLenum cull )
{
	if( !cull )
	{
		pglDisable( GL_CULL_FACE );
		glState.faceCull = 0;
		return;
	}

	pglEnable( GL_CULL_FACE );
	pglCullFace( cull );
	glState.faceCull = cull;
}

void GL_SetRenderMode( int mode )
{
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	switch( mode )
	{
	case kRenderNormal:
	default:
		pglDisable( GL_BLEND );
		pglDisable( GL_ALPHA_TEST );
		break;
	case kRenderTransColor:
	case kRenderTransTexture:
		pglEnable( GL_BLEND );
		pglDisable( GL_ALPHA_TEST );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		break;
	case kRenderTransAlpha:
		pglDisable( GL_BLEND );
		pglEnable( GL_ALPHA_TEST );
		break;
	case kRenderGlow:
	case kRenderTransAdd:
		pglEnable( GL_BLEND );
		pglDisable( GL_ALPHA_TEST );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE );
		break;
	}
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
{{ 90,   0,  90}, 0 }
};

/*
===============
VID_WriteOverviewScript

Create overview script file
===============
*/
void VID_WriteOverviewScript( void )
{
	ref_overview_t	*ov = &clgame.overView;
	string		filename;
	file_t		*f;

	Q_snprintf( filename, sizeof( filename ), "overviews/%s.txt", clgame.mapname );

	f = FS_Open( filename, "w", false );
	if( !f ) return;

	FS_Printf( f, "// overview description file for %s.bsp\n\n", clgame.mapname );
	FS_Print( f, "global\n{\n" );
	FS_Printf( f, "\tZOOM\t%.2f\n", ov->flZoom );
	FS_Printf( f, "\tORIGIN\t%.2f\t%.2f\t%.2f\n", ov->origin[0], ov->origin[1], ov->origin[2] );
	FS_Printf( f, "\tROTATED\t%i\n", ov->rotated ? 1 : 0 );
	FS_Print( f, "}\n\nlayer\n{\n" );
	FS_Printf( f, "\tIMAGE\t\"overviews/%s.bmp\"\n", clgame.mapname );
	FS_Printf( f, "\tHEIGHT\t%.2f\n", ov->zFar );	// ???
	FS_Print( f, "}\n" );

	FS_Close( f );
}

qboolean VID_ScreenShot( const char *filename, int shot_type )
{
	rgbdata_t *r_shot;
	uint	flags = IMAGE_FLIP_Y;
	int	width = 0, height = 0;
	qboolean	result;

	r_shot = Mem_Alloc( r_temppool, sizeof( rgbdata_t ));
	r_shot->width = (glState.width + 3) & ~3;
	r_shot->height = (glState.height + 3) & ~3;
	r_shot->flags = IMAGE_HAS_COLOR;
	r_shot->type = PF_RGB_24;
	r_shot->size = r_shot->width * r_shot->height * PFDesc[r_shot->type].bpp;
	r_shot->palette = NULL;
	r_shot->buffer = Mem_Alloc( r_temppool, r_shot->size );

	// get screen frame
	pglReadPixels( 0, 0, r_shot->width, r_shot->height, GL_RGB, GL_UNSIGNED_BYTE, r_shot->buffer );

	switch( shot_type )
	{
	case VID_SCREENSHOT:
		break;
	case VID_SNAPSHOT:
		FS_AllowDirectPaths( true );
		break;
	case VID_LEVELSHOT:
		flags |= IMAGE_RESAMPLE;
		if( glState.wideScreen )
		{
			height = 480;
			width = 800;
		}
		else
		{
			height = 480;
			width = 640;
		}
		break;
	case VID_MINISHOT:
		flags |= IMAGE_RESAMPLE;
		height = 200;
		width = 320;
		break;
	case VID_MAPSHOT:
		VID_WriteOverviewScript();		// store overview script too
		flags |= IMAGE_RESAMPLE|IMAGE_QUANTIZE;	// GoldSrc request overviews in 8-bit format
		height = 768;
		width = 1024;
		break;
	}

	Image_Process( &r_shot, width, height, flags, NULL );

	// write image
	result = FS_SaveImage( filename, r_shot );
	host.write_to_clipboard = false;		// disable write to clipboard
	FS_AllowDirectPaths( false );			// always reset after store screenshot
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
	if( !vieworg ) vieworg = RI.vieworg;

	for( i = 0; i < 6; i++ )
	{
		// go into 3d mode
		R_Set2DMode( false );

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

		pglReadPixels( 0, 0, size, size, GL_RGB, GL_UNSIGNED_BYTE, temp );
		r_side->flags = IMAGE_HAS_COLOR;
		r_side->width = r_side->height = size;
		r_side->type = PF_RGB_24;
		r_side->size = r_side->width * r_side->height * 3;
		r_side->buffer = temp;

		if( flags ) Image_Process( &r_side, 0, 0, flags, NULL );
		memcpy( buffer + (size * size * 3 * i), r_side->buffer, size * size * 3 );
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
	Q_strncpy( basename, base, MAX_STRING );
	FS_StripExtension( basename );
	FS_DefaultExtension( basename, ".tga" );

	// write image as 6 sides
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
	int		total, start, end;
	int		i, j, k, base_w, base_h;
	rgba_t		color = { 192, 192, 192, 255 };
	int		charHeight, numTries = 0;
	static qboolean	showHelp = true;
	string		shortname;

	if( !gl_showtextures->value )
		return;

	if( showHelp )
	{
		CL_CenterPrint( "use '<-' and '->' keys to change atlas page, ESC to quit", 0.25f );
		showHelp = false;
	}

	GL_SetRenderMode( kRenderNormal );
	pglClear( GL_COLOR_BUFFER_BIT );
	pglFinish();

	base_w = 8;	// textures view by horizontal
	base_h = 6;	// textures view by vertical

rebuild_page:
	total = base_w * base_h;
	start = total * (gl_showtextures->value - 1);
	end = total * gl_showtextures->value;
	if( end > MAX_TEXTURES ) end = MAX_TEXTURES;

	w = glState.width / base_w;
	h = glState.height / base_h;

	Con_DrawStringLen( NULL, NULL, &charHeight );

	for( i = j = 0; i < MAX_TEXTURES; i++ )
	{
		image = R_GetTexture( i );
		if( j == start ) break; // found start
		if( pglIsTexture( image->texnum )) j++;
	}

	if( i == MAX_TEXTURES && gl_showtextures->value != 1 )
	{
		// bad case, rewind to one and try again
		Cvar_SetValue( "r_showtextures", max( 1, gl_showtextures->value - 1 ));
		if( ++numTries < 2 ) goto rebuild_page;	// to prevent infinite loop
	}

	for( k = 0; i < MAX_TEXTURES; i++ )
	{
		if( j == end ) break; // page is full

		image = R_GetTexture( i );
		if( !pglIsTexture( image->texnum ))
			continue;

		x = k % base_w * w;
		y = k / base_w * h;

		pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		GL_Bind( GL_TEXTURE0, i ); // NOTE: don't use image->texnum here, because skybox has a 'wrong' indexes

		if( FBitSet( image->flags, TF_DEPTHMAP ) && !FBitSet( image->flags, TF_NOCOMPARE ))
			pglTexParameteri( image->target, GL_TEXTURE_COMPARE_MODE_ARB, GL_NONE );

		pglBegin( GL_QUADS );
		pglTexCoord2f( 0, 0 );
		pglVertex2f( x, y );
		if( image->target == GL_TEXTURE_RECTANGLE_EXT )
			pglTexCoord2f( image->width, 0 );
		else pglTexCoord2f( 1, 0 );
		pglVertex2f( x + w, y );
		if( image->target == GL_TEXTURE_RECTANGLE_EXT )
			pglTexCoord2f( image->width, image->height );
		else pglTexCoord2f( 1, 1 );
		pglVertex2f( x + w, y + h );
		if( image->target == GL_TEXTURE_RECTANGLE_EXT )
			pglTexCoord2f( 0, image->height );
		else pglTexCoord2f( 0, 1 );
		pglVertex2f( x, y + h );
		pglEnd();

		if( FBitSet( image->flags, TF_DEPTHMAP ) && !FBitSet( image->flags, TF_NOCOMPARE ))
			pglTexParameteri( image->target, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE_ARB );

		FS_FileBase( image->name, shortname );
		if( Q_strlen( shortname ) > 18 )
		{
			// cutoff too long names, it looks ugly
			shortname[16] = '.';
			shortname[17] = '.';
			shortname[18] = '\0';
		}
		Con_DrawString( x + 1, y + h - charHeight, shortname, color );
		j++, k++;
	}

	CL_DrawCenterPrint ();
	pglFinish();
}