//=======================================================================
//			Copyright XashXT Group 2010 ©
//		         gl_rmain.c - renderer main loop
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"

char		r_speeds_msg[MAX_SYSPATH];
ref_params_t	r_lastRefdef;
ref_instance_t	RI;

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

qboolean R_CullBox( const vec3_t mins, const vec3_t maxs )
{
	return false;
}

qboolean R_WorldToScreen( const vec3_t point, vec3_t screen )
{
	return false;
}

void R_ScreenToWorld( const vec3_t screen, vec3_t point )
{
}

/*
===============
R_ClearScene
===============
*/
void R_ClearScene( void )
{
}

/*
===============
R_AddEntity
===============
*/
qboolean R_AddEntity( struct cl_entity_s *pRefEntity, int entityType, int customTexture )
{
	return false;
}

/*
===============
R_BeginFrame
===============
*/
void R_BeginFrame( qboolean clearScene )
{
	if( gl_finish->integer && gl_delayfinish->integer )
	{
		pglFinish();

		GL_CheckForErrors ();

		// swap the buffers
		if( !gl_frontbuffer->integer )
		{
			if( !pwglSwapBuffers( glw_state.hDC ))
				Sys_Break( "SwapBuffers() failed!\n" );
		}
	}

	if( gl_colorbits->modified )
	{
		gl_colorbits->modified = false;
	}

	if( gl_clear->integer && clearScene )
	{
		pglClearColor( 0.5f, 0.5f, 0.5f, 1.0f );
		pglClear( GL_COLOR_BUFFER_BIT );
	}

	// update gamma
	if( vid_gamma->modified )
	{
		vid_gamma->modified = false;
		GL_UpdateGammaRamp();
	}

	// go into 2D mode
	R_Set2DMode( true );

	// draw buffer stuff
	if( gl_frontbuffer->modified )
	{
		gl_frontbuffer->modified = false;

		if( gl_frontbuffer->integer )
			pglDrawBuffer( GL_FRONT );
		else pglDrawBuffer( GL_BACK );
	}

	// texturemode stuff
	// update texture parameters
	if( gl_texturemode->modified || gl_texture_anisotropy->modified || gl_texture_lodbias ->modified )
		R_SetTextureParameters();

	// swapinterval stuff
	GL_UpdateSwapInterval();
}

/*
===============
R_RenderFrame
===============
*/
void R_RenderFrame( const ref_params_t *fd, qboolean drawWorld )
{
}

/*
===============
R_EndFrame
===============
*/
void R_EndFrame( void )
{
	// flush any remaining 2D bits
	R_Set2DMode( false );

	// cleanup texture units
//	R_BackendCleanUpTextureUnits();

	// apply software gamma
//	R_ApplySoftwareGamma();

	GL_CheckForErrors ();

	if( gl_finish->integer && gl_delayfinish->integer )
	{
		pglFlush();
		return;
	}

	// swap the buffers
	if( !gl_frontbuffer->integer )
	{
		if( !pwglSwapBuffers( glw_state.hDC ))
			Sys_Break( "SwapBuffers() failed!\n" );
	}
}

/*
===============
R_DrawCubemapView
===============
*/
void R_DrawCubemapView( const vec3_t origin, const vec3_t angles, int size )
{
}