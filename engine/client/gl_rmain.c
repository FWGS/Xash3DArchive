//=======================================================================
//			Copyright XashXT Group 2010 ©
//		         gl_rmain.c - renderer main loop
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"
#include "matrix_lib.h"

msurface_t	*r_debug_surface;
const char	*r_debug_hitbox;
float		gldepthmin, gldepthmax;
ref_params_t	r_lastRefdef;
ref_instance_t	RI, prevRI;

mleaf_t		*r_viewleaf, *r_oldviewleaf;
mleaf_t		*r_viewleaf2, *r_oldviewleaf2;

uint		r_numEntities;
cl_entity_t	*r_solid_entities[MAX_VISIBLE_PACKET];	// opaque edicts
cl_entity_t	*r_alpha_entities[MAX_VISIBLE_PACKET];	// edicts with rendermode kRenderTransAlpha
cl_entity_t	*r_trans_entities[MAX_VISIBLE_PACKET];	// edicts with rendermode kRenderTransTexture, Additive etc

qboolean R_CullBox( const vec3_t mins, const vec3_t maxs )
{
	uint		i, bit;
	const mplane_t	*p;

	if( r_nocull->integer )
		return false;

	for( i = sizeof( RI.frustum ) / sizeof( RI.frustum[0] ), bit = 1, p = RI.frustum; i > 0; i--, bit<<=1, p++ )
	{
		if( !( RI.clipFlags & bit ))
			continue;

		switch( p->signbits )
		{
		case 0:
			if( p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist )
				return true;
			break;
		case 1:
			if( p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist )
				return true;
			break;
		case 2:
			if( p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist )
				return true;
			break;
		case 3:
			if( p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist )
				return true;
			break;
		case 4:
			if( p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist )
				return true;
			break;
		case 5:
			if( p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist )
				return true;
			break;
		case 6:
			if( p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist )
				return true;
			break;
		case 7:
			if( p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist )
				return true;
			break;
		default:
			return false;
		}
	}
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
qboolean R_AddEntity( struct cl_entity_s *pRefEntity, int entityType )
{
	return true;
}

/*
=============
R_Clear
=============
*/
static void R_Clear( int bitMask )
{
	int bits;

	bits = GL_DEPTH_BUFFER_BIT;

	if( RI.drawWorld && r_fastsky->integer )
		bits |= GL_COLOR_BUFFER_BIT;
	if( glState.stencilEnabled && r_shadows->integer )
		bits |= GL_STENCIL_BUFFER_BIT;

	bits &= bitMask;

	if( bits & GL_STENCIL_BUFFER_BIT )
		pglClearStencil( 128 );

	if( bits & GL_COLOR_BUFFER_BIT )
	{
		pglClearColor( 0.5f, 0.5f, 0.5f, 1.0f );
	}

	pglClear( bits );

	gldepthmin = 0.0f;
	gldepthmax = 1.0f;
	pglDepthRange( gldepthmin, gldepthmax );
}

//=============================================================================
/*
===============
R_GetFarClip
===============
*/
static float R_GetFarClip( void )
{
	if( cl.worldmodel && RI.drawWorld )
		return RI.refdef.movevars->zmax;
	return 2048.0f;
}

/*
===============
R_SetupFrustum
===============
*/
static void R_SetupFrustum( void )
{
	vec3_t	farPoint;
	int	i;

	// 0 - left
	// 1 - right
	// 2 - down
	// 3 - up
	// 4 - farclip

	// rotate RI.vforward right by FOV_X/2 degrees
	RotatePointAroundVector( RI.frustum[0].normal, RI.vup, RI.vforward, -( 90 - RI.refdef.fov_x / 2 ));
	// rotate RI.vforward left by FOV_X/2 degrees
	RotatePointAroundVector( RI.frustum[1].normal, RI.vup, RI.vforward, 90 - RI.refdef.fov_x / 2 );
	// rotate RI.vforward up by FOV_X/2 degrees
	RotatePointAroundVector( RI.frustum[2].normal, RI.vright, RI.vforward, 90 - RI.refdef.fov_y / 2 );
	// rotate RI.vforward down by FOV_X/2 degrees
	RotatePointAroundVector( RI.frustum[3].normal, RI.vright, RI.vforward, -( 90 - RI.refdef.fov_y / 2 ));
	// negate forward vector
	VectorNegate( RI.vforward, RI.frustum[4].normal );

	for( i = 0; i < 4; i++ )
	{
		RI.frustum[i].type = PLANE_NONAXIAL;
		RI.frustum[i].dist = DotProduct( RI.vieworg, RI.frustum[i].normal );
		RI.frustum[i].signbits = SignbitsForPlane( RI.frustum[i].normal );
	}

	VectorMA( RI.vieworg, R_GetFarClip(), RI.vforward, farPoint );
	RI.frustum[i].type = PLANE_NONAXIAL;
	RI.frustum[i].dist = DotProduct( farPoint, RI.frustum[i].normal );
	RI.frustum[i].signbits = SignbitsForPlane( RI.frustum[i].normal );
}

/*
=============
R_SetupProjectionMatrix
=============
*/
static void R_SetupProjectionMatrix( const ref_params_t *fd, matrix4x4 m )
{
	GLdouble	xMin, xMax, yMin, yMax, zNear, zFar;

	RI.farClip = R_GetFarClip();

	zNear = 4.0f;
	zFar = max( 256.0f, RI.farClip );

	yMax = zNear * com.tan( fd->fov_y * M_PI / 360.0 );
	yMin = -yMax;

	xMax = zNear * com.tan( fd->fov_x * M_PI / 360.0 );
	xMin = -xMax;

	Matrix4x4_CreateProjection( m, xMax, xMin, yMax, yMin, zNear, zFar );
}

/*
=============
R_SetupModelviewMatrix
=============
*/
static void R_SetupModelviewMatrix( const ref_params_t *fd, matrix4x4 m )
{
#if 0
	Matrix4x4_LoadIdentity( m );
	Matrix4x4_ConcatRotate( m, -90, 1, 0, 0 );
	Matrix4x4_ConcatRotate( m, 90, 0, 0, 1 );
#else
	Matrix4x4_CreateModelview( m );
#endif
	Matrix4x4_ConcatRotate( m, -fd->viewangles[2], 1, 0, 0 );
	Matrix4x4_ConcatRotate( m, -fd->viewangles[0], 0, 1, 0 );
	Matrix4x4_ConcatRotate( m, -fd->viewangles[1], 0, 0, 1 );
	Matrix4x4_ConcatTranslate( m, -fd->vieworg[0], -fd->vieworg[1], -fd->vieworg[2] );
}

/*
===============
R_SetupFrame
===============
*/
static void R_SetupFrame( void )
{
	// build the transformation matrix for the given view angles
	VectorCopy( RI.refdef.vieworg, RI.vieworg );
	AngleVectors( RI.refdef.viewangles, RI.vforward, RI.vright, RI.vup );

	CL_RunLightStyles();

	tr.framecount++;

	// current viewleaf
	if( RI.drawWorld )
	{
		mleaf_t	*leaf;
		vec3_t	tmp;

		VectorCopy( cl.worldmodel->mins, RI.visMins );
		VectorCopy( cl.worldmodel->maxs, RI.visMaxs );

		if(!( RI.params & RP_OLDVIEWLEAF ))
		{
			r_oldviewleaf = r_viewleaf;
			r_oldviewleaf2 = r_viewleaf2;
			leaf = Mod_PointInLeaf( RI.pvsorigin, cl.worldmodel->nodes );
			r_viewleaf2 = r_viewleaf = leaf;

			// check above and below so crossing solid water doesn't draw wrong
			if( leaf->contents == CONTENTS_EMPTY )
			{
				// look down a bit
				VectorCopy( RI.pvsorigin, tmp );
				tmp[2] -= 16;
				leaf = Mod_PointInLeaf( tmp, cl.worldmodel->nodes );
				if(( leaf->contents != CONTENTS_SOLID ) && ( leaf != r_viewleaf2 ))
					r_viewleaf2 = leaf;
			}
			else
			{
				// look up a bit
				VectorCopy( RI.pvsorigin, tmp );
				tmp[2] += 16;
				leaf = Mod_PointInLeaf( tmp, cl.worldmodel->nodes );
				if(( leaf->contents != CONTENTS_SOLID ) && ( leaf != r_viewleaf2 ))
					r_viewleaf2 = leaf;
			}
		}
	}
}

/*
=============
R_SetupGL
=============
*/
static void R_SetupGL( void )
{
	R_SetupModelviewMatrix( &RI.refdef, RI.worldviewMatrix );
	R_SetupProjectionMatrix( &RI.refdef, RI.projectionMatrix );
	if( RI.params & RP_MIRRORVIEW ) RI.projectionMatrix[0][0] = -RI.projectionMatrix[0][0];

	Matrix4x4_Concat( RI.worldviewProjectionMatrix, RI.projectionMatrix, RI.worldviewMatrix );

	pglScissor( RI.scissor[0], RI.scissor[1], RI.scissor[2], RI.scissor[3] );
	pglViewport( RI.viewport[0], RI.viewport[1], RI.viewport[2], RI.viewport[3] );

	pglMatrixMode( GL_PROJECTION );
	GL_LoadMatrix( RI.projectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI.worldviewMatrix );

	if( RI.params & RP_CLIPPLANE )
	{
		GLdouble	clip[4];
		mplane_t	*p = &RI.clipPlane;

		clip[0] = p->normal[0];
		clip[1] = p->normal[1];
		clip[2] = p->normal[2];
		clip[3] = -p->dist;

		pglClipPlane( GL_CLIP_PLANE0, clip );
		pglEnable( GL_CLIP_PLANE0 );
	}

	if( RI.params & RP_FLIPFRONTFACE )
		GL_FrontFace( !glState.frontFace );

	GL_Cull( GL_FRONT );
	GL_SetState( GLSTATE_DEPTHWRITE );
}

/*
=============
R_EndGL
=============
*/
static void R_EndGL( void )
{
	if( RI.params & RP_FLIPFRONTFACE )
		GL_FrontFace( !glState.frontFace );

	if( RI.params & RP_CLIPPLANE )
		pglDisable( GL_CLIP_PLANE0 );
}

/*
================
R_RenderScene

RI.refdef must be set before the first call
================
*/
void R_RenderScene( const ref_params_t *fd )
{
	RI.refdef = *fd;

	if( !cl.worldmodel && RI.drawWorld )
		Host_Error( "R_RenderView: NULL worldmodel\n" );

	R_PushDlights();

	R_SetupFrame();
	R_SetupFrustum();
	R_SetupGL();
	R_Clear( ~0 );

	R_MarkLeaves();
	R_DrawWorld();

	CL_ExtraUpdate ();	// don't let sound get messed up if going slow

//	R_DrawEntitiesOnList();

	CL_DrawParticles ();

	R_EndGL();
}

/*
===============
R_BeginFrame
===============
*/
void R_BeginFrame( qboolean clearScene )
{
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
	pglDrawBuffer( GL_BACK );

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
	if( r_norefresh->integer )
		return;

	R_Set2DMode( false );
	GL_BackendStartFrame();

	if( drawWorld ) r_lastRefdef = *fd;

	RI.params = RP_NONE;
	RI.farClip = 0;
	RI.rdflags = 0;
	RI.clipFlags = 15;
	RI.drawWorld = drawWorld;
	RI.lerpFrac = cl.lerpFrac;
	RI.thirdPerson = cl.thirdperson;

	// adjust field of view for widescreen
	if( glState.wideScreen && r_adjust_fov->integer )
		V_AdjustFov( &RI.refdef.fov_x, &RI.refdef.fov_y, glState.width, glState.height, false );

	VectorCopy( fd->vieworg, RI.pvsorigin );

	// setup scissor
	RI.scissor[0] = fd->viewport[0];
	RI.scissor[1] = glState.height - fd->viewport[3] - fd->viewport[1];
	RI.scissor[2] = fd->viewport[2];
	RI.scissor[3] = fd->viewport[3];

	// setup viewport
	RI.viewport[0] = fd->viewport[0];
	RI.viewport[1] = glState.height - fd->viewport[3] - fd->viewport[1];
	RI.viewport[2] = fd->viewport[2];
	RI.viewport[3] = fd->viewport[3];

	if( gl_finish->integer && drawWorld )
		pglFinish();

	R_RenderScene( fd );

//	R_BloomBlend( fd );

	GL_BackendEndFrame();

	R_Set2DMode( true );

	R_ShowTextures();
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

	// check errors
	GL_CheckForErrors ();

	if( !pwglSwapBuffers( glw_state.hDC ))
		Sys_Break( "wglSwapBuffers() failed!\n" );
}

/*
===============
R_DrawCubemapView
===============
*/
void R_DrawCubemapView( const vec3_t origin, const vec3_t angles, int size )
{
}