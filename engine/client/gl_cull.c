/*
gl_cull.c - render culling routines
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
#include "entity_types.h"

/*
=============================================================

FRUSTUM AND PVS CULLING

=============================================================
*/
/*
=================
R_CullBox

Returns true if the box is completely outside the frustum
=================
*/
qboolean R_CullBox( const vec3_t mins, const vec3_t maxs )
{
	return GL_FrustumCullBox( &RI.frustum, mins, maxs, 0 );
}

/*
=================
R_CullSphere

Returns true if the sphere is completely outside the frustum
=================
*/
qboolean R_CullSphere( const vec3_t centre, const float radius )
{
	return GL_FrustumCullSphere( &RI.frustum, centre, radius, 0 );
}

/*
=============
R_CullModel
=============
*/
int R_CullModel( cl_entity_t *e, const vec3_t absmin, const vec3_t absmax )
{
	if( e == &clgame.viewent )
	{
		if( CL_IsDevOverviewMode( ))
			return 1;

		if( RP_NORMALPASS() && !RI.thirdPerson && cl.viewentity == ( cl.playernum + 1 ))
			return 0;

		return 1;
	}

	// don't reflect this entity in mirrors
	if( FBitSet( e->curstate.effects, EF_NOREFLECT ) && FBitSet( RI.params, RP_MIRRORVIEW ))
		return 1;

	// draw only in mirrors
	if( FBitSet( e->curstate.effects, EF_REFLECTONLY ) && !FBitSet( RI.params, RP_MIRRORVIEW ))
		return 1;

	// local client can't view himself if camera or thirdperson is not active
	if( RP_LOCALCLIENT( e ) && !RI.thirdPerson && cl.viewentity == ( cl.playernum + 1 ))
	{
		if( !FBitSet( RI.params, RP_MIRRORVIEW ))
			return 1;
	}

	if( R_CullBox( absmin, absmax ))
		return 1;

	return 0;
}

/*
=================
R_CullSurface

cull invisible surfaces
=================
*/
qboolean R_CullSurface( msurface_t *surf, gl_frustum_t *frustum, uint clipflags )
{
	cl_entity_t	*e = RI.currententity;

	if( !surf || !surf->texinfo || !surf->texinfo->texture )
		return true;

	if( surf->flags & SURF_WATERCSG && !( e->curstate.effects & EF_NOWATERCSG ))
		return true;

	// don't cull transparent surfaces because we should be draw decals on them
	if( surf->pdecals && ( e->curstate.rendermode == kRenderTransTexture || e->curstate.rendermode == kRenderTransAdd ))
		return false;

	if( r_nocull->value )
		return false;

	// world surfaces can be culled by vis frame too
	if( RI.currententity == clgame.entities && surf->visframe != tr.framecount )
		return true;

	if( r_faceplanecull->value && !FBitSet( surf->flags, SURF_DRAWTURB ))
	{
		if( !VectorIsNull( surf->plane->normal ))
		{
			float	dist;

			// can use normal.z for world (optimisation)
			if( RI.drawOrtho )
			{
				vec3_t	orthonormal;

				if( e == clgame.entities || R_StaticEntity( e ))
					orthonormal[2] = surf->plane->normal[2];
				else Matrix4x4_VectorRotate( RI.objectMatrix, surf->plane->normal, orthonormal );

				dist = orthonormal[2];
			}
			else dist = PlaneDiff( tr.modelorg, surf->plane );

			if( glState.faceCull == GL_FRONT || ( RI.params & RP_MIRRORVIEW ))
			{
				if( surf->flags & SURF_PLANEBACK )
				{
					if( dist >= -BACKFACE_EPSILON )
						return true; // wrong side
				}
				else
				{
					if( dist <= BACKFACE_EPSILON )
						return true; // wrong side
				}
			}
			else if( glState.faceCull == GL_BACK )
			{
				if( surf->flags & SURF_PLANEBACK )
				{
					if( dist <= BACKFACE_EPSILON )
						return true; // wrong side
				}
				else
				{
					if( dist >= -BACKFACE_EPSILON )
						return true; // wrong side
				}
			}
		}
	}

	if( frustum )
		return GL_FrustumCullBox( frustum, surf->info->mins, surf->info->maxs, clipflags );

	return false;
}