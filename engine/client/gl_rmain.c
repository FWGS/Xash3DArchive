/*
gl_rmain.c - renderer main loop
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

msurface_t	*r_debug_surface;
const char	*r_debug_hitbox;
float		gldepthmin, gldepthmax;
ref_params_t	r_lastRefdef;
ref_instance_t	RI, prevRI;

mleaf_t		*r_viewleaf, *r_oldviewleaf;
mleaf_t		*r_viewleaf2, *r_oldviewleaf2;

static int R_RankForRenderMode( cl_entity_t *ent )
{
	switch( ent->curstate.rendermode )
	{
	case kRenderTransTexture:
		return 1;	// draw second
	case kRenderTransAdd:
		return 2;	// draw third
	case kRenderGlow:
		return 3;	// must be last!
	}
	return 0;
}

/*
===============
R_StaticEntity

Static entity is the brush which has no custom origin and not rotated
typically is a func_wall, func_breakable, func_ladder etc
===============
*/
static qboolean R_StaticEntity( cl_entity_t *ent )
{
	if( !gl_allow_static->integer )
		return false;

	if( ent->curstate.rendermode != kRenderNormal )
		return false;

	if( ent->model->type != mod_brush )
		return false;

	if( ent->curstate.frame || ent->model->flags & MODEL_CONVEYOR )
		return false;

	if( !VectorIsNull( ent->origin ) || !VectorIsNull( ent->angles ))
		return false;

	return true;
}

/*
===============
R_OpaqueEntity

Opaque entity can be brush or studio model but sprite
===============
*/
static qboolean R_OpaqueEntity( cl_entity_t *ent )
{
	if( ent->curstate.rendermode == kRenderNormal )
		return true;

	if( ent->model->type == mod_sprite )
		return false;

	if( ent->curstate.rendermode == kRenderTransAlpha )
		return true;

	return false;
}

/*
===============
R_TransEntityCompare

Sorting translucent entities by rendermode then by distance
===============
*/
static int R_TransEntityCompare( const cl_entity_t **a, const cl_entity_t **b )
{
	cl_entity_t	*ent1, *ent2;
	vec3_t		vecLen, org;
	float		len1, len2;

	ent1 = (cl_entity_t *)*a;
	ent2 = (cl_entity_t *)*b;

	// now sort by rendermode
	if( R_RankForRenderMode( ent1 ) > R_RankForRenderMode( ent2 ))
		return 1;
	if( R_RankForRenderMode( ent1 ) < R_RankForRenderMode( ent2 ))
		return -1;

	// then by distance
	if( ent1->model->type == mod_brush )
	{
		VectorAverage( ent1->model->mins, ent1->model->maxs, org );
		VectorAdd( ent1->origin, org, org );
		VectorSubtract( RI.pvsorigin, org, vecLen );
	}
	else VectorSubtract( RI.pvsorigin, ent1->origin, vecLen );
	len1 = VectorLength( vecLen );

	if( ent2->model->type == mod_brush )
	{
		VectorAverage( ent2->model->mins, ent2->model->maxs, org );
		VectorAdd( ent2->origin, org, org );
		VectorSubtract( RI.pvsorigin, org, vecLen );
	}
	else VectorSubtract( RI.pvsorigin, ent2->origin, vecLen );
	len2 = VectorLength( vecLen );

	if( len1 > len2 )
		return -1;
	if( len1 < len2 )
		return 1;

	return 0;
}

qboolean R_WorldToScreen( const vec3_t point, vec3_t screen )
{
	matrix4x4	worldToScreen;
	qboolean behind;
	float w;

	Matrix4x4_Copy( worldToScreen, RI.worldviewProjectionMatrix );
	screen[0] = worldToScreen[0][0] * point[0] + worldToScreen[0][1] * point[1] + worldToScreen[0][2] * point[2] + worldToScreen[0][3];
	screen[1] = worldToScreen[1][0] * point[0] + worldToScreen[1][1] * point[1] + worldToScreen[1][2] * point[2] + worldToScreen[1][3];
//	z = worldToScreen[2][0] * point[0] + worldToScreen[2][1] * point[1] + worldToScreen[2][2] * point[2] + worldToScreen[2][3];
	w = worldToScreen[3][0] * point[0] + worldToScreen[3][1] * point[1] + worldToScreen[3][2] * point[2] + worldToScreen[3][3];

	// Just so we have something valid here
	screen[2] = 0.0f;

	if( w < 0.001f )
	{
		behind = true;
		screen[0] *= 100000;
		screen[1] *= 100000;
	}
	else
	{
		float invw = 1.0f / w;
		behind = false;
		screen[0] *= invw;
		screen[1] *= invw;
	}
	return behind;
}

void R_ScreenToWorld( const vec3_t screen, vec3_t point )
{
	// TODO: implement
}

/*
===============
R_ComputeFxBlend
===============
*/
int R_ComputeFxBlend( cl_entity_t *e )
{
	int		blend = 0, renderAmt;
	float		offset, dist;
	vec3_t		tmp;

	offset = ((int)e->index ) * 363.0f; // Use ent index to de-sync these fx
	renderAmt = e->curstate.renderamt;

	switch( e->curstate.renderfx ) 
	{
	case kRenderFxPulseSlowWide:
		blend = renderAmt + 0x40 * sin( RI.refdef.time * 2 + offset );	
		break;
	case kRenderFxPulseFastWide:
		blend = renderAmt + 0x40 * sin( RI.refdef.time * 8 + offset );
		break;
	case kRenderFxPulseSlow:
		blend = renderAmt + 0x10 * sin( RI.refdef.time * 2 + offset );
		break;
	case kRenderFxPulseFast:
		blend = renderAmt + 0x10 * sin( RI.refdef.time * 8 + offset );
		break;
	// JAY: HACK for now -- not time based
	case kRenderFxFadeSlow:			
		if( renderAmt > 0 ) 
			renderAmt -= 1;
		else renderAmt = 0;
		blend = renderAmt;
		break;
	case kRenderFxFadeFast:
		if( renderAmt > 3 ) 
			renderAmt -= 4;
		else renderAmt = 0;
		blend = renderAmt;
		break;
	case kRenderFxSolidSlow:
		if( renderAmt < 255 ) 
			renderAmt += 1;
		else renderAmt = 255;
		blend = renderAmt;
		break;
	case kRenderFxSolidFast:
		if( renderAmt < 252 ) 
			renderAmt += 4;
		else renderAmt = 255;
		blend = renderAmt;
		break;
	case kRenderFxStrobeSlow:
		blend = 20 * sin( RI.refdef.time * 4 + offset );
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxStrobeFast:
		blend = 20 * sin( RI.refdef.time * 16 + offset );
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxStrobeFaster:
		blend = 20 * sin( RI.refdef.time * 36 + offset );
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxFlickerSlow:
		blend = 20 * (sin( RI.refdef.time * 2 ) + sin( RI.refdef.time * 17 + offset ));
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxFlickerFast:
		blend = 20 * (sin( RI.refdef.time * 16 ) + sin( RI.refdef.time * 23 + offset ));
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxHologram:
	case kRenderFxDistort:
		VectorCopy( e->origin, tmp );
		VectorSubtract( tmp, RI.refdef.vieworg, tmp );
		dist = DotProduct( tmp, RI.refdef.forward );
			
		// Turn off distance fade
		if( e->curstate.renderfx == kRenderFxDistort )
			dist = 1;

		if( dist <= 0 )
		{
			blend = 0;
		}
		else 
		{
			renderAmt = 180;
			if( dist <= 100 ) blend = renderAmt;
			else blend = (int) ((1.0f - ( dist - 100 ) * ( 1.0f / 400.0f )) * renderAmt );
			blend += Com_RandomLong( -32, 31 );
		}
		break;
	case kRenderFxDeadPlayer:
		blend = renderAmt;	// safe current renderamt because it's player index!
		break;
	case kRenderFxNone:
	case kRenderFxClampMinScale:
	default:
		if( e->curstate.rendermode == kRenderNormal )
			blend = 255;
		else blend = renderAmt;
		break;	
	}

	if( RI.currentmodel->type != mod_brush )
	{
		// NOTE: never pass sprites with rendercolor '0 0 0' it's a stupid Valve Hammer Editor bug
		if( !e->curstate.rendercolor.r && !e->curstate.rendercolor.g && !e->curstate.rendercolor.b )
			e->curstate.rendercolor.r = e->curstate.rendercolor.g = e->curstate.rendercolor.b = 255;
	}

	// apply scale to studiomodels and sprites only
	if( e->model && e->model->type != mod_brush && !e->curstate.scale )
		e->curstate.scale = 1.0f;

	blend = bound( 0, blend, 255 );

	return blend;
}

/*
===============
R_ClearScene
===============
*/
void R_ClearScene( void )
{
	tr.num_solid_entities = tr.num_trans_entities = 0;
	tr.num_static_entities = tr.num_mirror_entities = 0;
}

/*
===============
R_AddEntity
===============
*/
qboolean R_AddEntity( struct cl_entity_s *clent, int entityType )
{
	if( !r_drawentities->integer )
		return false; // not allow to drawing

	if( !clent || !clent->model )
		return false; // if set to invisible, skip

	if( clent->curstate.effects & EF_NODRAW )
		return false; // done

	if( clent->curstate.rendermode != kRenderNormal && clent->curstate.renderamt <= 0.0f )
		return true; // done

	clent->curstate.entityType = entityType;

	if( R_OpaqueEntity( clent ))
	{
		if( R_StaticEntity( clent ))
		{
			// opaque static
			if( tr.num_static_entities >= MAX_VISIBLE_PACKET )
				return false;

			tr.static_entities[tr.num_static_entities] = clent;
			tr.num_static_entities++;
		}
		else
		{
			// opaque moving
			if( tr.num_solid_entities >= MAX_VISIBLE_PACKET )
				return false;

			tr.solid_entities[tr.num_solid_entities] = clent;
			tr.num_solid_entities++;
		}
	}
	else
	{
		// translucent
		if( tr.num_trans_entities >= MAX_VISIBLE_PACKET )
			return false;

		tr.trans_entities[tr.num_trans_entities] = clent;
		tr.num_trans_entities++;
	}
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
	if( glState.stencilEnabled )
		bits |= GL_STENCIL_BUFFER_BIT;

	bits &= bitMask;

	if( bits & GL_STENCIL_BUFFER_BIT )
		pglClearStencil( 128 );

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
		return RI.refdef.movevars->zmax * 1.5f;
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

	yMax = zNear * tan( fd->fov_y * M_PI / 360.0 );
	yMin = -yMax;

	xMax = zNear * tan( fd->fov_x * M_PI / 360.0 );
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
=============
R_LoadIdentity
=============
*/
void R_LoadIdentity( void )
{
	if( tr.modelviewIdentity ) return;

	Matrix4x4_LoadIdentity( RI.objectMatrix );
	Matrix4x4_Copy( RI.modelviewMatrix, RI.worldviewMatrix );
	GL_LoadMatrix( RI.modelviewMatrix );
	tr.modelviewIdentity = true;
}

/*
=============
R_RotateForEntity
=============
*/
void R_RotateForEntity( cl_entity_t *e )
{
	float	scale = 1.0f;

	if( e == clgame.entities || R_StaticEntity( e ))
	{
		R_LoadIdentity();
		return;
	}

	if( e->model->type != mod_brush && e->curstate.scale > 0.0f )
		scale = e->curstate.scale;

	Matrix4x4_CreateFromEntity( RI.objectMatrix, e->angles, e->origin, scale );
	Matrix4x4_ConcatTransforms( RI.modelviewMatrix, RI.worldviewMatrix, RI.objectMatrix );

	GL_LoadMatrix( RI.modelviewMatrix );
	tr.modelviewIdentity = false;
}

/*
=============
R_TranslateForEntity
=============
*/
void R_TranslateForEntity( cl_entity_t *e )
{
	float	scale = 1.0f;

	if( e == clgame.entities || R_StaticEntity( e ))
	{
		R_LoadIdentity();
		return;
	}

	if( e->model->type != mod_brush && e->curstate.scale > 0.0f )
		scale = e->curstate.scale;

	Matrix4x4_CreateFromEntity( RI.objectMatrix, vec3_origin, e->origin, scale );
	Matrix4x4_ConcatTransforms( RI.modelviewMatrix, RI.worldviewMatrix, RI.objectMatrix );

	GL_LoadMatrix( RI.modelviewMatrix );
	tr.modelviewIdentity = false;
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

	R_AnimateLight();
	R_RunViewmodelEvents();
		
	tr.framecount++;

	// sort translucents entities by rendermode and distance
	qsort( tr.trans_entities, tr.num_trans_entities, sizeof( cl_entity_t* ), R_TransEntityCompare );

	// current viewleaf
	if( RI.drawWorld )
	{
		float	height;
		mleaf_t	*leaf;
		vec3_t	tmp;

		RI.waveHeight = RI.refdef.movevars->waveHeight * 2.0f;	// set global waveheight
		RI.isSkyVisible = false; // unknown at this moment

		if(!( RI.params & RP_OLDVIEWLEAF ))
		{
			r_oldviewleaf = r_viewleaf;
			r_oldviewleaf2 = r_viewleaf2;
			leaf = Mod_PointInLeaf( RI.pvsorigin, cl.worldmodel->nodes );
			r_viewleaf2 = r_viewleaf = leaf;
			height = RI.waveHeight ? RI.waveHeight : 16;

			// check above and below so crossing solid water doesn't draw wrong
			if( leaf->contents == CONTENTS_EMPTY )
			{
				// look down a bit
				VectorCopy( RI.pvsorigin, tmp );
				tmp[2] -= height;
				leaf = Mod_PointInLeaf( tmp, cl.worldmodel->nodes );
				if(( leaf->contents != CONTENTS_SOLID ) && ( leaf != r_viewleaf2 ))
					r_viewleaf2 = leaf;
			}
			else
			{
				// look up a bit
				VectorCopy( RI.pvsorigin, tmp );
				tmp[2] += height;
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
	if( RI.refdef.waterlevel >= 3 )
	{
		float	f;
		f = sin( cl.time * 0.4f * ( M_PI * 2.7f ));
		RI.refdef.fov_x += f;
		RI.refdef.fov_y -= f;
	}

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

	pglDisable( GL_BLEND );
	pglDisable( GL_ALPHA_TEST );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
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
=============
R_CheckFog

check for underwater fog
=============
*/
static void R_CheckFog( void )
{
	model_t		*model;
	gltexture_t	*tex;
	int		i, count;

	RI.fogEnabled = false;

	if( RI.refdef.waterlevel < 3 || !RI.drawWorld || !r_viewleaf )
		return;

	model = CL_GetWaterModel( cl.refdef.vieworg );
	tex = NULL;

	// check for water texture
	if( model && model->type == mod_brush )
	{
		msurface_t	*surf;
	
		count = model->nummodelsurfaces;

		for( i = 0, surf = &model->surfaces[model->firstmodelsurface]; i < count; i++, surf++ )
		{
			if( surf->flags & SURF_DRAWTURB && surf->texinfo && surf->texinfo->texture )
			{
				tex = R_GetTexture( surf->texinfo->texture->gl_texturenum );
				break;
			}
		}
	}
	else
	{
		msurface_t	**surf;

		count = r_viewleaf->nummarksurfaces;	

		for( i = 0, surf = r_viewleaf->firstmarksurface; i < count; i++, surf++ )
		{
			if((*surf)->flags & SURF_DRAWTURB && (*surf)->texinfo && (*surf)->texinfo->texture )
			{
				tex = R_GetTexture( (*surf)->texinfo->texture->gl_texturenum );
				break;
			}
		}
	}

	if( i == count || !tex )
		return;	// no valid fogs

	// copy fog params
	RI.fogColor[0] = tex->fogParams[0] / 255.0f;
	RI.fogColor[1] = tex->fogParams[1] / 255.0f;
	RI.fogColor[2] = tex->fogParams[2] / 255.0f;
	RI.fogDensity = tex->fogParams[3] * 0.000025f;
	RI.fogStart = RI.fogEnd = 0.0f;
	RI.fogCustom = false;
	RI.fogEnabled = true;
}

/*
=============
R_DrawFog

=============
*/
void R_DrawFog( void )
{
	if( !RI.fogEnabled || RI.refdef.onlyClientDraw )
		return;

	pglEnable( GL_FOG );
	pglFogi( GL_FOG_MODE, GL_EXP );
	pglFogf( GL_FOG_DENSITY, RI.fogDensity );
	pglFogfv( GL_FOG_COLOR, RI.fogColor );
	pglHint( GL_FOG_HINT, GL_NICEST );
}

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList( void )
{
	int	i;

	glState.drawTrans = false;

	// draw the solid submodels fog
	R_DrawFog ();

	// first draw solid entities
	for( i = 0; i < tr.num_solid_entities; i++ )
	{
		if( RI.refdef.onlyClientDraw )
			break;

		RI.currententity = tr.solid_entities[i];
		RI.currentmodel = RI.currententity->model;
	
		ASSERT( RI.currententity != NULL );
		ASSERT( RI.currententity->model != NULL );

		RI.currententity->curstate.renderamt = R_ComputeFxBlend( RI.currententity );

		switch( RI.currentmodel->type )
		{
		case mod_brush:
			R_DrawBrushModel( RI.currententity );
			break;
		case mod_studio:
			R_DrawStudioModel( RI.currententity );
			break;
		case mod_sprite:
			R_DrawSpriteModel( RI.currententity );
			break;
		default:
			break;
		}
	}

	CL_DrawBeams( false );

	if( RI.drawWorld )
		clgame.dllFuncs.pfnDrawNormalTriangles();

	// NOTE: some mods with custom renderer may generate glErrors
	// so we clear it here
	while( pglGetError() != GL_NO_ERROR );

	// don't fogging translucent surfaces
	if( !RI.fogCustom ) 
		pglDisable( GL_FOG );
	pglDepthMask( GL_FALSE );
	glState.drawTrans = true;

	// then draw translicent entities
	for( i = 0; i < tr.num_trans_entities; i++ )
	{
		if( RI.refdef.onlyClientDraw )
			break;

		RI.currententity = tr.trans_entities[i];
		RI.currentmodel = RI.currententity->model;
	
		ASSERT( RI.currententity != NULL );
		ASSERT( RI.currententity->model != NULL );

		RI.currententity->curstate.renderamt = R_ComputeFxBlend( RI.currententity );

		switch( RI.currentmodel->type )
		{
		case mod_brush:
			R_DrawBrushModel( RI.currententity );
			break;
		case mod_studio:
			R_DrawStudioModel( RI.currententity );
			break;
		case mod_sprite:
			R_DrawSpriteModel( RI.currententity );
			break;
		default:
			break;
		}
	}

	if( RI.drawWorld )
		clgame.dllFuncs.pfnDrawTransparentTriangles ();

	CL_DrawBeams( true );
	CL_DrawParticles();

	// NOTE: some mods with custom renderer may generate glErrors
	// so we clear it here
	while( pglGetError() != GL_NO_ERROR );

	glState.drawTrans = false;
	pglDepthMask( GL_TRUE );
	R_DrawViewModel();

	CL_ExtraUpdate();
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
	R_CheckFog();
	R_DrawWorld();

	CL_ExtraUpdate ();	// don't let sound get messed up if going slow

	R_DrawEntitiesOnList();

	R_DrawWaterSurfaces();

	R_EndGL();
}

/*
===============
R_BeginFrame
===============
*/
void R_BeginFrame( qboolean clearScene )
{
	if( gl_clear->integer && clearScene && cls.state != ca_cinematic )
	{
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

	CL_ExtraUpdate ();
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
	RI.clipFlags = 15;
	RI.drawWorld = drawWorld;
	RI.thirdPerson = cl.thirdperson;

	// adjust field of view for widescreen
	if( glState.wideScreen && r_adjust_fov->integer )
		V_AdjustFov( &RI.refdef.fov_x, &RI.refdef.fov_y, glState.width, glState.height, false );

	if( !r_lockcull->integer )
		VectorCopy( fd->vieworg, RI.cullorigin );
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
#ifdef MIRROR_TEST
	R_DrawMirrors ();
#endif
	GL_BackendEndFrame();

	// go into 2D mode (in case we draw PlayerSetup between two 2d calls)
	R_Set2DMode( true );
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

	if( !pwglSwapBuffers( glw_state.hDC ))
		Sys_Error( "wglSwapBuffers() failed!\n" );
}

/*
===============
R_DrawCubemapView
===============
*/
void R_DrawCubemapView( const vec3_t origin, const vec3_t angles, int size )
{
	ref_params_t *fd;

	fd = &RI.refdef;
	*fd = r_lastRefdef;
	fd->time = 0;
	fd->viewport[0] = RI.refdef.viewport[1] = 0;
	fd->viewport[2] = size;
	fd->viewport[3] = size;
	fd->fov_x = 90;
	fd->fov_y = 90;
	VectorCopy( origin, fd->vieworg );
	VectorCopy( angles, fd->viewangles );
	VectorCopy( fd->vieworg, RI.pvsorigin );

	R_Set2DMode( false );
		
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

	R_RenderScene( fd );

	r_oldviewleaf = r_viewleaf = NULL;		// force markleafs next frame
}