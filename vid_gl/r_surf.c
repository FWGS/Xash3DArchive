/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2002-2007 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// r_surf.c: surface-related refresh code

#include "r_local.h"
#include "mathlib.h"
#include "matrix_lib.h"

static vec3_t	modelorg;       // relative to viewpoint
static vec3_t	modelmins;
static vec3_t	modelmaxs;
static byte	fatpvs[MAX_MAP_LEAFS/8];

/*
=============================================================

BRUSH MODELS

=============================================================
*/
/*
=================
R_SurfPotentiallyVisible
=================
*/
bool R_SurfPotentiallyVisible( msurface_t *surf )
{
	if( !surf->texinfo )
		return false;
	if( !surf->mesh || R_InvalidMesh( surf->mesh ))
		return false;
	if( !surf->shader )
		return false;
	if( !surf->shader->num_stages )
		return false;
	return true;
}

/*
=================
R_CullSurface
=================
*/
bool R_CullSurface( msurface_t *surf, uint clipflags )
{
	ref_shader_t *shader = surf->shader;

	if(( shader->flags & SHADER_SKYPARMS ) && r_fastsky->integer )
		return true;

	if( r_nocull->integer )
		return false;

	if( shader->flags & SHADER_AUTOSPRITE )
		return false;

	// never cull turblent or warp surfaces
	// FIXME: this is probably not actual for sudivided surfaces. comment this ?
	if( shader->tessSize ) return false;

	if( r_faceplanecull->integer && !RI.currententity->outlineHeight && ( shader->flags & (SHADER_CULL_FRONT|SHADER_CULL_BACK)))
	{
		if( !VectorCompare( surf->plane->normal, vec3_origin ))
		{
			float dist;

			dist = PlaneDiff( modelorg, surf->plane );

			if( shader->flags & SHADER_CULL_FRONT || ( RI.params & RP_MIRRORVIEW ))
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
			else if( shader->flags & SHADER_CULL_BACK )
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
	return ( clipflags && R_CullBox( surf->mins, surf->maxs, clipflags ));
}

/*
=================
R_AddSurfaceToList
=================
*/
static meshbuffer_t *R_AddSurfaceToList( msurface_t *surf, unsigned int clipflags )
{
	ref_shader_t *shader;
	meshbuffer_t *mb;

	if( R_CullSurface( surf, clipflags ))
		return NULL;

	shader = ((r_drawworld->integer == 2) ? R_OcclusionShader() : surf->shader);

	if( shader->flags & SHADER_SKYPARMS )
	{
		bool vis = R_AddSkySurface( surf );
		if(( RI.params & RP_NOSKY ) && vis )
		{
			R_AddMeshToList( MB_MODEL, surf->fog, shader, surf - r_worldbrushmodel->surfaces + 1 );
			RI.params &= ~RP_NOSKY;
		}
		return NULL;
	}

	if( OCCLUSION_QUERIES_ENABLED( RI ))
	{
		if( shader->flags & SHADER_PORTAL )
			R_SurfOcclusionQueryKey( RI.currententity, surf );

		if( OCCLUSION_OPAQUE_SHADER( shader ) )
			R_AddOccludingSurface( surf, shader );
	}

	c_brush_polys++;
	mb = R_AddMeshToList( MB_MODEL, surf->fog, shader, surf - r_worldbrushmodel->surfaces + 1 );
	RI.surfmbuffers[surf - r_worldbrushmodel->surfaces] = mb;

	return mb;
}

/*
=================
R_CullBrushModel
=================
*/
bool R_CullBrushModel( ref_entity_t *e )
{
	bool		rotated;
	ref_model_t	*model = e->model;
	mbrushmodel_t	*bmodel = (mbrushmodel_t *)model->extradata;
	int		i;

	if( bmodel->nummodelsurfaces == 0 )
		return true;

	rotated = !Matrix3x3_Compare( e->axis, matrix3x3_identity );

	if( rotated )
	{
		for( i = 0; i < 3; i++ )
		{
			modelmins[i] = e->origin[i] - model->radius * e->scale;
			modelmaxs[i] = e->origin[i] + model->radius * e->scale;
		}

		if( R_CullSphere( e->origin, model->radius * e->scale, RI.clipFlags ))
			return true;
	}
	else
	{
		VectorMA( e->origin, e->scale, model->mins, modelmins );
		VectorMA( e->origin, e->scale, model->maxs, modelmaxs );

		if( R_CullBox( modelmins, modelmaxs, RI.clipFlags ))
			return true;
	}

	if( RI.refdef.flags & ( RDF_PORTALINVIEW|RDF_SKYPORTALINVIEW ) || ( RI.params & RP_SKYPORTALVIEW ))
	{
		if( rotated )
		{
			if( R_VisCullSphere( e->origin, model->radius * e->scale ))
				return true;
		}
		else
		{
			if( R_VisCullBox( modelmins, modelmaxs ))
				return true;
		}
	}

	return false;
}

void R_BmodelDrawDebug( void )
{
	vec3_t		bbox[8];
	ref_model_t	*model;
	int		i;

	if( r_drawentities->integer != 5 )
		return;

	model = RI.currententity->model;

	// compute a full bounding box
	for( i = 0; i < 8; i++ )
	{
		bbox[i][0] = (i & 1) ? model->mins[0] : model->maxs[0];
		bbox[i][1] = (i & 2) ? model->mins[1] : model->maxs[1];
		bbox[i][2] = (i & 4) ? model->mins[2] : model->maxs[2];
	}

	R_RotateForEntity( RI.currententity );
	pglColor4f( 1.0f, 1.0f, 0.0f, 1.0f );	// yellow bboxes for brushmodels

	pglBegin( GL_LINES );
	for( i = 0; i < 2; i += 1 )
	{
		pglVertex3fv( bbox[i+0] );
		pglVertex3fv( bbox[i+2] );
		pglVertex3fv( bbox[i+4] );
		pglVertex3fv( bbox[i+6] );
		pglVertex3fv( bbox[i+0] );
		pglVertex3fv( bbox[i+4] );
		pglVertex3fv( bbox[i+2] );
		pglVertex3fv( bbox[i+6] );
		pglVertex3fv( bbox[i*2+0] );
		pglVertex3fv( bbox[i*2+1] );
		pglVertex3fv( bbox[i*2+4] );
		pglVertex3fv( bbox[i*2+5] );
	}
	pglEnd();
}

/*
=================
R_AddBrushModelToList
=================
*/
void R_AddBrushModelToList( ref_entity_t *e )
{
	bool		rotated;
	ref_model_t	*model = e->model;
	mbrushmodel_t	*bmodel = (mbrushmodel_t *)model->extradata;
	msurface_t	*psurf;
	meshbuffer_t	*mb;
	dlight_t		*lt;
	uint		i;

	e->outlineHeight = r_worldent->outlineHeight;
	Vector4Copy( r_worldent->outlineColor, e->outlineColor );

	rotated = !Matrix3x3_Compare( e->axis, matrix3x3_identity );
	VectorSubtract( RI.refdef.vieworg, e->origin, modelorg );

	if( rotated )
	{
		vec3_t	temp;

		VectorCopy( modelorg, temp );
		Matrix3x3_Transform( e->axis, temp, modelorg );
	}

	for( i = 0, lt = r_dlights; i < r_numDlights; i++, lt++ )
	{
		VectorSubtract( lt->origin, modelorg, lt->origin );
		R_RecursiveLightNode( lt, BIT( i ), bmodel->firstmodelnode );
		VectorAdd( lt->origin, modelorg, lt->origin );
	}

	for( i = 0, psurf = bmodel->firstmodelsurface; i < bmodel->nummodelsurfaces; i++, psurf++ )
	{
		if( !R_SurfPotentiallyVisible( psurf ))
			continue;

		if( RI.params & RP_SHADOWMAPVIEW )
		{
			if( psurf->visframe != r_framecount )
				continue;

			if(( psurf->shader->sort >= SORT_OPAQUE ) && ( psurf->shader->sort <= SORT_BANNER ))
			{
				if( prevRI.surfmbuffers[psurf - r_worldbrushmodel->surfaces] )
				{
					if( !R_CullSurface( psurf, 0 ))
					{
						RI.params |= RP_WORLDSURFVISIBLE;
						prevRI.surfmbuffers[psurf - r_worldbrushmodel->surfaces]->shadowbits |= RI.shadowGroup->bit;
					}
				}
			}
			continue;
		}

		psurf->visframe = r_framecount;
		mb = R_AddSurfaceToList( psurf, 0 );
		if( mb )
		{
			mb->sortkey |= (( psurf->superLightStyle+1 ) << 10 );
			if( R_SurfPotentiallyLit( psurf ))
				mb->dlightbits = psurf->dlightBits;
		}
	}
}

/*
=============================================================

WORLD MODEL

=============================================================
*/
/*
================
R_RecursiveWorldNode
================
*/
static void R_RecursiveWorldNode( mnode_t *node, uint clipflags )
{
	const cplane_t	*clipplane;
	int		i, clipped;
	msurface_t	**mark, *surf;
	mleaf_t		*leaf;
	meshbuffer_t	*mb;

	if( node->contents == CONTENTS_SOLID )
		return;	// hit a solid leaf

	if( node->pvsframe != r_pvsframecount )
		return;	// not visible for this frame

	if( clipflags )
	{
		for( i = 0, clipplane = RI.frustum; i < 6; i++, clipplane++ )
		{
			if(!( clipflags & ( 1<<i )))
				continue;

			clipped = BoxOnPlaneSide( node->mins, node->maxs, clipplane );
			if( clipped == 2 ) return;
			if( clipped == 1 ) clipflags &= ~(1<<i);
		}
	}

	// recurse down the children
	if( node->contents == CONTENTS_NODE )
	{
		R_RecursiveWorldNode( node->children[0], clipflags );
		R_RecursiveWorldNode( node->children[1], clipflags );
		return;
	}

	// if a leaf node, draw stuff
	leaf = (mleaf_t *)node;
	leaf->visframe = r_framecount;

	if( !leaf->numMarkSurfaces )
		return;	// nothing to draw

	// add leaf bounds to view bounds
	for( i = 0; i < 3; i++ )
	{
		RI.visMins[i] = min( RI.visMins[i], leaf->mins[i] );
		RI.visMaxs[i] = max( RI.visMaxs[i], leaf->maxs[i] );
	}

	// add all the surfaces
	for( i = 0, mark = leaf->firstMarkSurface; i < leaf->numMarkSurfaces; i++, mark++ )
	{
		surf = *mark;

		// NOTE: that R_AddSurfaceToList may set meshBuffer to NULL
		// for world ALL surfaces to prevent referencing to freed memory region
		if( surf->visframe != r_framecount )
		{
			surf->visframe = r_framecount;
			mb = R_AddSurfaceToList( surf, clipflags );
			if( mb ) mb->sortkey |= (( surf->superLightStyle+1 ) << 10 );
		}
		else
		{
			mb = RI.surfmbuffers[surf - r_worldbrushmodel->surfaces];
		}

		if( mb && R_SurfPotentiallyLit( surf ))
			mb->dlightbits = surf->dlightBits;
	}

	c_world_leafs++;
}

/*
================
R_MarkShadowLeafSurfaces
================
*/
static void R_MarkShadowLeafSurfaces( msurface_t **mark, uint numfaces, uint clipflags )
{
	msurface_t	*surf;
	meshbuffer_t	*mb;
	const uint	bit = RI.shadowGroup->bit;
	int		i;

	for( i = 0; i < numfaces; i++ )
	{
		surf = *mark++;

		mb = prevRI.surfmbuffers[surf - r_worldbrushmodel->surfaces];
		if( !mb || ( mb->shadowbits & bit ))
			continue;

		// this surface is visible in previous RI, not marked as shadowed...
		if(( surf->shader->sort >= SORT_OPAQUE ) && ( surf->shader->sort <= SORT_ALPHATEST ))
		{	
			// ...is opaque
			if( !R_CullSurface( surf, clipflags ))
			{	
				// and is visible to the light source too
				RI.params |= RP_WORLDSURFVISIBLE;
				mb->shadowbits |= bit;
			}
		}
	}
}

/*
================
R_LinearShadowLeafs
================
*/
static void R_LinearShadowLeafs( void )
{
	uint		i, j, cpf;
	const cplane_t	*clipplane;
	int		clipped;
	mleaf_t		*pleaf;

	for( i = 0; i < r_worldbrushmodel->numleafs; i++ )
	{
		pleaf = &r_worldbrushmodel->leafs[i+1];

		if( pleaf->visframe != r_framecount )
			continue;

		if( !( RI.shadowGroup->vis[i>>3] & ( 1<<( i & 7 ))))
			continue;

		cpf = RI.clipFlags;

		for( j = 0, clipplane = RI.frustum; j < 6; j++, clipplane++ )
		{
			clipped = BoxOnPlaneSide( pleaf->mins, pleaf->maxs, clipplane );

			if( clipped == 2 ) break;
			if( clipped == 1 ) cpf &= ~(1<<j);
		}

		if( !j )
		{
			R_MarkShadowLeafSurfaces( pleaf->firstMarkSurface, pleaf->numMarkSurfaces, cpf );
			c_world_leafs++;
		}
	}
}

//==================================================================================

int r_surfQueryKeys[MAX_SURF_QUERIES];

/*
===============
R_ClearSurfOcclusionQueryKeys
===============
*/
void R_ClearSurfOcclusionQueryKeys( void )
{
	Mem_Set( r_surfQueryKeys, -1, sizeof( r_surfQueryKeys ));
}

/*
===============
R_SurfOcclusionQueryKey
===============
*/
int R_SurfOcclusionQueryKey( ref_entity_t *e, msurface_t *surf )
{
	int i;
	int *keys = r_surfQueryKeys;
	int key = surf - r_worldbrushmodel->surfaces;

	if( e != r_worldent )
		return -1;

	for( i = 0; i < MAX_SURF_QUERIES; i++ )
	{
		if( keys[i] >= 0 )
		{
			if( keys[i] == key )
				return i;
		}
		else
		{
			keys[i] = key;
			return i;
		}
	}

	return -1;
}

/*
===============
R_SurfIssueOcclusionQueries
===============
*/
void R_SurfIssueOcclusionQueries( void )
{
	int i, *keys = r_surfQueryKeys;
	msurface_t *surf;

	for( i = 0; keys[i] >= 0; i++ )
	{
		surf = &r_worldbrushmodel->surfaces[keys[i]];
		R_IssueOcclusionQuery( R_GetOcclusionQueryNum( OQ_CUSTOM, i ), r_worldent, surf->mins, surf->maxs );
	}
}

//==================================================================================

/*
=============
R_CalcDistancesToFogVolumes
=============
*/
static void R_CalcDistancesToFogVolumes( void )
{
	int	i;
	mfog_t	*fog;

	for( i = 0, fog = r_worldbrushmodel->fogs; i < r_worldbrushmodel->numfogs; i++, fog++ )
		RI.fog_dist_to_eye[fog - r_worldbrushmodel->fogs] = PlaneDiff( RI.viewOrigin, fog->visibleplane );
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld( void )
{
	int	clipflags;
	int	msec = 0;

	if( !r_drawworld->integer )
		return;

	if( RI.refdef.flags & RDF_NOWORLDMODEL )
		return;

	if( !r_worldmodel || !r_worldbrushmodel )
		return;

	VectorCopy( RI.refdef.vieworg, modelorg );

	RI.previousentity = NULL;
	RI.currententity = r_worldent;
	RI.currentmodel = RI.currententity->model;

	if( r_outlines_world->integer && r_viewleaf != NULL )
		RI.currententity->outlineHeight = max( 0.0f, r_outlines_world->value );
	else RI.currententity->outlineHeight = 0.0f;

	Vector4Copy( mapConfig.outlineColor, RI.currententity->outlineColor );

	if(!( RI.params & RP_SHADOWMAPVIEW ))
	{
		R_AllocMeshbufPointers( &RI );
		Mem_Set( RI.surfmbuffers, 0, r_worldbrushmodel->numsurfaces * sizeof( meshbuffer_t* ));

		R_CalcDistancesToFogVolumes();
	}

	ClearBounds( RI.visMins, RI.visMaxs );

	R_ClearSkyBox();

	if( r_nocull->integer ) clipflags = 0;
	else clipflags = RI.clipFlags;

	if( r_speeds->integer )
		msec = Sys_Milliseconds();

	if( RI.params & RP_SHADOWMAPVIEW )
	{
		R_LinearShadowLeafs ();
	}
	else
	{
		R_RecursiveWorldNode( r_worldbrushmodel->nodes, clipflags );
		R_MarkLights( clipflags );
	}

	if( r_speeds->integer )
		r_world_node += Sys_Milliseconds() - msec;
}

/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current leaf
===============
*/
void R_MarkLeaves( void )
{
	byte	*vis;
	mleaf_t	*leaf;
	mnode_t	*node;
	int	i;

	if( RI.refdef.flags & RDF_NOWORLDMODEL )
		return;

	if( RI.params & RP_SHADOWMAPVIEW )
		return;

	if( r_viewleaf == r_oldviewleaf && r_viewleaf2 == r_oldviewleaf2 && !r_novis->integer && r_viewleaf != NULL )
		return;

	// development aid to let you run around
	// and see exactly where the pvs ends
	if( r_lockpvs->integer )
		return;

	r_pvsframecount++;
	r_oldviewleaf = r_viewleaf;
	r_oldviewleaf2 = r_viewleaf2;
			
	if( r_novis->integer || r_viewleaf == NULL || !r_worldbrushmodel->visdata )
	{
		// mark everything
		for( i = 0, leaf = r_worldbrushmodel->leafs; i < r_worldbrushmodel->numleafs; i++, leaf++ )
			leaf->pvsframe = r_pvsframecount;
		for( i = 0, node = r_worldbrushmodel->nodes; i < r_worldbrushmodel->numnodes; i++, node++ )
			node->pvsframe = r_pvsframecount;
		return;
	}

	// may have to combine two clusters
	// because of solid water boundaries
	vis = Mod_LeafPVS( r_viewleaf, r_worldmodel );

	if( r_viewleaf != r_viewleaf2 )
	{
		int	longs = (r_worldbrushmodel->numleafs + 31)>>5;

		Mem_Copy( fatpvs, vis, longs << 2 );
		vis = Mod_LeafPVS( r_viewleaf2, r_worldmodel );

		for( i = 0; i < longs; i++ )
			((int *)fatpvs)[i] |= ((int *)vis)[i];

		vis = fatpvs;
	}

	for( i = 0; i < r_worldbrushmodel->numleafs; i++ )
	{
		if( vis[i>>3] & ( 1<<( i & 7 )))
		{
			node = (mnode_t *)&r_worldbrushmodel->leafs[i+1];
			do
			{
				if( node->pvsframe == r_pvsframecount )
					break;
				node->pvsframe = r_pvsframecount;
				node = node->parent;
			} while( node );
		}
	}
}