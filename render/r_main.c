//=======================================================================
//			Copyright XashXT Group 2007 �
//			r_main.c - opengl render core
//=======================================================================

#include "r_local.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "const.h"

render_imp_t	ri;
stdlib_api_t	com;

byte		*r_temppool;
matrix4x4		r_worldMatrix;
matrix4x4		r_entityMatrix;

gl_matrix		gl_projectionMatrix;
gl_matrix		gl_entityMatrix;
gl_matrix		gl_textureMatrix;
cplane_t		r_frustum[4];
float		r_frameTime;
mesh_t		r_solidMeshes[MAX_MESHES];
int		r_numSolidMeshes;
mesh_t		r_transMeshes[MAX_MESHES];
int		r_numTransMeshes;
ref_entity_t	*r_nullModels[MAX_ENTITIES];
rmodel_t		*cl_models[MAX_MODELS];		// client replacement modeltable
lightstyle_t	r_lightStyles[MAX_LIGHTSTYLES];
ref_entity_t	r_entities[MAX_ENTITIES];
int		r_numEntities;
dlight_t		r_dlights[MAX_DLIGHTS];
int		r_numDLights;
particle_t	r_particles[MAX_PARTICLES];
int		r_numParticles;
poly_t		r_polys[MAX_POLYS];
int		r_numPolys;
polyVert_t	r_polyVerts[MAX_POLY_VERTS];
int		r_numPolyVerts;
int		r_numNullModels;
ref_params_t	r_refdef;
refstats_t	r_stats;
byte		*r_framebuffer;			// pause frame buffer
float		r_pause_alpha;
glconfig_t	gl_config;
glstate_t		gl_state;

// view matrix
vec3_t		r_forward;
vec3_t		r_right;
vec3_t		r_up;
vec3_t		r_origin;

cvar_t	*r_check_errors;
cvar_t	*r_himodels;
cvar_t	*r_norefresh;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_nobind;
cvar_t	*r_drawworld;
cvar_t	*r_drawentities;
cvar_t	*r_drawparticles;
cvar_t	*r_drawpolys;
cvar_t	*r_fullbright;
cvar_t	*r_lightmap;
cvar_t	*r_lockpvs;
cvar_t	*r_frontbuffer;
cvar_t	*r_showcluster;
cvar_t	*r_showtris;
cvar_t	*r_allow_software;
cvar_t	*r_shownormals;
cvar_t	*r_showtangentspace;
cvar_t	*r_showmodelbounds;
cvar_t	*r_showshadowvolumes;
cvar_t	*r_showtextures;
cvar_t	*r_offsetfactor;
cvar_t	*r_offsetunits;
cvar_t	*r_debugsort;
cvar_t	*r_speeds;
cvar_t	*r_singleshader;
cvar_t	*r_showlightmaps;
cvar_t	*r_skipbackend;
cvar_t	*r_skipfrontend;
cvar_t	*r_swapInterval;
cvar_t	*r_vertexbuffers;
cvar_t	*r_mode;
cvar_t	*r_stencilbits;
cvar_t	*r_colorbits;
cvar_t	*r_depthbits;
cvar_t	*r_testmode;
cvar_t	*r_fullscreen;
cvar_t	*r_caustics;
cvar_t	*r_minimap;
cvar_t	*r_minimap_size;
cvar_t	*r_minimap_zoom;
cvar_t	*r_minimap_style;
cvar_t	*r_pause;
cvar_t	*r_width;
cvar_t	*r_height;
cvar_t	*r_refreshrate;
cvar_t	*r_bitdepth;
cvar_t	*r_overbrightbits;
cvar_t	*r_shadows;
cvar_t	*r_caustics;
cvar_t	*r_dynamiclights;
cvar_t	*r_modulate;
cvar_t	*r_ambientscale;
cvar_t	*r_directedscale;
cvar_t	*r_intensity;
cvar_t	*r_texturebits;
cvar_t	*r_texturefilter;
cvar_t	*r_texturefilteranisotropy;
cvar_t	*r_texturelodbias;
cvar_t	*r_max_normal_texsize;
cvar_t	*r_max_texsize;
cvar_t	*r_round_down;
cvar_t	*r_detailtextures;
cvar_t	*r_compress_normal_textures;
cvar_t	*r_compress_textures;
cvar_t	*r_lefthand;
cvar_t	*r_bloom;
cvar_t	*r_bloom_alpha;
cvar_t	*r_bloom_diamond_size;
cvar_t	*r_bloom_intensity;
cvar_t	*r_bloom_darken;
cvar_t	*r_bloom_sample_size;
cvar_t	*r_bloom_fast_sample;
cvar_t	*r_motionblur_intens;
cvar_t	*r_motionblur;
cvar_t	*r_mirroralpha;
cvar_t	*r_interpolate;
cvar_t	*r_physbdebug;
cvar_t	*r_pause_bw;
cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level

cvar_t	*gl_finish;
cvar_t	*gl_clear;
cvar_t	*vid_gamma;


/*
=================
R_CullBox

Returns true if the box is completely outside the frustum
=================
*/
bool R_CullBox( const vec3_t mins, const vec3_t maxs, int clipFlags )
{
	cplane_t	*plane;
	int	i;
	
	if( r_nocull->integer )
		return false;

	for( i = 0, plane = r_frustum; i < 4; i++, plane++ )
	{
		if(!(clipFlags & (1<<i))) continue;
                    if( BoxOnPlaneSide( mins, maxs, plane ) == SIDE_ON )
			return true;
	}
	return false;
}

/*
=================
R_CullSphere

Returns true if the sphere is completely outside the frustum
=================
*/
bool R_CullSphere( const vec3_t origin, float radius, int clipFlags )
{
	cplane_t	*plane;
	int	i;
	
	if( r_nocull->integer )
		return false;

	for( i = 0, plane = r_frustum; i < 4; i++, plane++ )
	{
		if(!(clipFlags & (1<<i)))
			continue;
		if( DotProduct( origin, plane->normal ) - plane->dist <= -radius )
			return true;
	}
	return false;
}

/*
=================
R_RotateForEntity
=================
*/
void R_RotateForEntity( ref_entity_t *e )
{
	matrix4x4		rotMatrix;
#if 0
	// classic slow version (used for debug)
	Matrix4x4_LoadIdentity( rotMatrix );
	Matrix4x4_ConcatTranslate( rotMatrix, e->origin[0],  e->origin[1],  e->origin[2] );
	Matrix4x4_ConcatRotate( rotMatrix,  e->angles[1],  0, 0, 1 );
	Matrix4x4_ConcatRotate( rotMatrix, -e->angles[0],  0, 1, 0 );
	Matrix4x4_ConcatRotate( rotMatrix, -e->angles[2],  1, 0, 0 );
	Matrix4x4_Concat( r_entityMatrix, r_worldMatrix, rotMatrix );
#else
	Matrix4x4_FromMatrix3x3( rotMatrix, e->matrix );
	Matrix4x4_SetOrigin( rotMatrix, e->origin[0], e->origin[1], e->origin[2] );
	Matrix4x4_Concat( r_entityMatrix, r_worldMatrix, rotMatrix );
#endif
	GL_LoadMatrix( r_entityMatrix );
}


// =====================================================================

/*
=================
R_DrawBeam
=================
*/
void R_DrawBeam( void )
{
	vec3_t	axis[3];
	float	length;
	int		i;

	// find orientation vectors
	VectorSubtract( r_refdef.vieworg, m_pCurrentEntity->origin, axis[0] );
	VectorSubtract( m_pCurrentEntity->prev.origin, m_pCurrentEntity->origin, axis[1] );// FIXME

	CrossProduct( axis[0], axis[1], axis[2] );
	VectorNormalizeFast( axis[2] );

	// find normal
	CrossProduct( axis[1], axis[2], axis[0] );
	VectorNormalizeFast( axis[0] );

	// scale by radius
	VectorScale( axis[2], m_pCurrentEntity->frame / 2, axis[2] );

	// find segment length
	length = VectorLength( axis[1] ) / m_pCurrentEntity->prev.frame;

	// draw it
	RB_CheckMeshOverflow( 6, 4 );
	
	for( i = 2; i < 4; i++ )
	{
		ref.indexArray[ref.numIndex++] = ref.numVertex + 0;
		ref.indexArray[ref.numIndex++] = ref.numVertex + i-1;
		ref.indexArray[ref.numIndex++] = ref.numVertex + i;
	}

	ref.vertexArray[ref.numVertex+0][0] = m_pCurrentEntity->origin[0] + axis[2][0];
	ref.vertexArray[ref.numVertex+0][1] = m_pCurrentEntity->origin[1] + axis[2][1];
	ref.vertexArray[ref.numVertex+0][2] = m_pCurrentEntity->origin[2] + axis[2][2];
	ref.vertexArray[ref.numVertex+1][0] = m_pCurrentEntity->prev.origin[0] + axis[2][0];
	ref.vertexArray[ref.numVertex+1][1] = m_pCurrentEntity->prev.origin[1] + axis[2][1];
	ref.vertexArray[ref.numVertex+1][2] = m_pCurrentEntity->prev.origin[2] + axis[2][2];
	ref.vertexArray[ref.numVertex+2][0] = m_pCurrentEntity->prev.origin[0] - axis[2][0];
	ref.vertexArray[ref.numVertex+2][1] = m_pCurrentEntity->prev.origin[1] - axis[2][1];
	ref.vertexArray[ref.numVertex+2][2] = m_pCurrentEntity->prev.origin[2] - axis[2][2];
	ref.vertexArray[ref.numVertex+3][0] = m_pCurrentEntity->origin[0] - axis[2][0];
	ref.vertexArray[ref.numVertex+3][1] = m_pCurrentEntity->origin[1] - axis[2][1];
	ref.vertexArray[ref.numVertex+3][2] = m_pCurrentEntity->origin[2] - axis[2][2];

	ref.inTexCoordArray[ref.numVertex+0][0] = 0;
	ref.inTexCoordArray[ref.numVertex+0][1] = 0;
	ref.inTexCoordArray[ref.numVertex+1][0] = length;
	ref.inTexCoordArray[ref.numVertex+1][1] = 0;
	ref.inTexCoordArray[ref.numVertex+2][0] = length;
	ref.inTexCoordArray[ref.numVertex+2][1] = 1;
	ref.inTexCoordArray[ref.numVertex+3][0] = 0;
	ref.inTexCoordArray[ref.numVertex+3][1] = 1;

	for( i = 0; i < 4; i++ )
	{
		ref.normalArray[ref.numVertex][0] = axis[0][0];
		ref.normalArray[ref.numVertex][1] = axis[0][1];
		ref.normalArray[ref.numVertex][2] = axis[0][2];
		ref.colorArray[ref.numVertex][0] = m_pCurrentEntity->rendercolor[0];
		ref.colorArray[ref.numVertex][1] = m_pCurrentEntity->rendercolor[1];
		ref.colorArray[ref.numVertex][2] = m_pCurrentEntity->rendercolor[2];
		ref.colorArray[ref.numVertex][3] = m_pCurrentEntity->renderamt;
		ref.numVertex++;
	}
}

/*
=================
R_AddBeamToList
=================
*/
static void R_AddBeamToList( ref_entity_t *entity )
{
	R_AddMeshToList( MESH_BEAM, NULL, entity->shader, entity, 0 );
}

/*
=================
R_AddEntitiesToList
=================
*/
static void R_AddEntitiesToList( void )
{
	ref_entity_t	*entity;
	rmodel_t		*model;
	int		i;

	if( !r_drawentities->integer || r_numEntities == 1 )
		return;

	r_stats.numEntities += (r_numEntities - 1);

	for( i = 1, entity = &r_entities[1]; i < r_numEntities; i++, entity++ )
	{
		switch( entity->ent_type )
		{
		case ED_MOVER:
		case ED_NORMAL:
		case ED_CLIENT:
		case ED_MONSTER:
		case ED_BSPBRUSH:
		case ED_VIEWMODEL:
		case ED_RIGIDBODY:
			m_pCurrentEntity = entity;
			model = m_pRenderModel = entity->model;
			if( !model || model->type == mod_bad )
			{
				r_nullModels[r_numNullModels++] = entity;
				break;
			}

			switch( model->type )
			{
			case mod_brush:
				R_AddBrushModelToList( entity );
				break;
			case mod_studio:
				R_AddStudioModelToList( entity );
				break;
			case mod_sprite:
				R_AddSpriteModelToList( entity );
				break;
			default:
				Host_Error( "R_AddEntitiesToList: bad model type (%i)\n", model->type );
			}
			break;
		case ED_BEAM:
			R_AddBeamToList( entity );
			break;
		default:
			Host_Error( "R_AddEntitiesToList: bad entity type (%i)\n", entity->ent_type );
		}
	}
}

/*
=================
R_DrawNullModels
=================
*/
static void R_DrawNullModels( void )
{
	ref_entity_t	*entity;
	vec3_t		points[3];
	int		i;

	if (!r_numNullModels)
		return;

	GL_LoadMatrix( r_worldMatrix );

	// Set the state
	GL_Enable(GL_CULL_FACE);
	GL_Disable(GL_POLYGON_OFFSET_FILL);
	GL_Disable(GL_VERTEX_PROGRAM_ARB);
	GL_Disable(GL_FRAGMENT_PROGRAM_ARB);
	GL_Disable(GL_ALPHA_TEST);
	GL_Enable(GL_BLEND);
	GL_Enable(GL_DEPTH_TEST);

	GL_CullFace(GL_FRONT);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthFunc(GL_LEQUAL);
	GL_DepthMask(GL_FALSE);

	// Draw them
	for( i = 0; i < r_numNullModels; i++ )
	{
		entity = r_nullModels[i];

		VectorMA( entity->origin, 15, entity->matrix[0], points[0] );
		VectorMA( entity->origin, -15, entity->matrix[1], points[1] );
		VectorMA( entity->origin, 15, entity->matrix[2], points[2] );

		pglBegin( GL_LINES );

		pglColor4f( 1.0f, 0.0f, 0.0f, 0.5f );
		pglVertex3fv( entity->origin );
		pglVertex3fv( points[0] );

		pglColor4f( 0, 1.0f, 0, 0.5f );
		pglVertex3fv( entity->origin );
		pglVertex3fv( points[1] );

		pglColor4f( 0, 0, 1.0f, 0.5f );
		pglVertex3fv( entity->origin );
		pglVertex3fv( points[2] );

		pglEnd();
	}
	r_numNullModels = 0;
}

/*
=================
R_DrawParticle
=================
*/
void R_DrawParticle( void )
{
	particle_t	*particle = m_pRenderMesh->mesh;
	vec3_t		axis[3];
	int		i;

	// Draw it
	RB_CheckMeshOverflow( 6, 4 );
	
	for( i = 2; i < 4; i++ )
	{
		ref.indexArray[ref.numIndex++] = ref.numVertex + 0;
		ref.indexArray[ref.numIndex++] = ref.numVertex + i-1;
		ref.indexArray[ref.numIndex++] = ref.numVertex + i;
	}

	if( particle->length != 1 )
	{
		// find orientation vectors
		VectorSubtract( r_refdef.vieworg, particle->origin1, axis[0] );
		VectorSubtract( particle->origin2, particle->origin1, axis[1] );
		CrossProduct( axis[0], axis[1], axis[2] );

		VectorNormalizeFast( axis[1] );
		VectorNormalizeFast( axis[2] );

		// find normal
		CrossProduct( axis[1], axis[2], axis[0] );
		VectorNormalizeFast( axis[0] );

		VectorMA( particle->origin1, -particle->length, axis[1], particle->origin2 );
		VectorScale( axis[2], particle->radius, axis[2] );

		ref.vertexArray[ref.numVertex+0][0] = particle->origin2[0] + axis[2][0];
		ref.vertexArray[ref.numVertex+0][1] = particle->origin2[1] + axis[2][1];
		ref.vertexArray[ref.numVertex+0][2] = particle->origin2[2] + axis[2][2];
		ref.vertexArray[ref.numVertex+1][0] = particle->origin1[0] + axis[2][0];
		ref.vertexArray[ref.numVertex+1][1] = particle->origin1[1] + axis[2][1];
		ref.vertexArray[ref.numVertex+1][2] = particle->origin1[2] + axis[2][2];
		ref.vertexArray[ref.numVertex+2][0] = particle->origin1[0] - axis[2][0];
		ref.vertexArray[ref.numVertex+2][1] = particle->origin1[1] - axis[2][1];
		ref.vertexArray[ref.numVertex+2][2] = particle->origin1[2] - axis[2][2];
		ref.vertexArray[ref.numVertex+3][0] = particle->origin2[0] - axis[2][0];
		ref.vertexArray[ref.numVertex+3][1] = particle->origin2[1] - axis[2][1];
		ref.vertexArray[ref.numVertex+3][2] = particle->origin2[2] - axis[2][2];
	}
	else
	{
		if( particle->rotation )
		{
			// Rotate it around its normal
			RotatePointAroundVector( axis[1], r_forward, r_right, particle->rotation );
			CrossProduct( r_forward, axis[1], axis[2] );

			// The normal should point at the viewer
			VectorNegate( r_forward, axis[0] );

			// Scale the axes by radius
			VectorScale( axis[1], particle->radius, axis[1] );
			VectorScale( axis[2], particle->radius, axis[2] );
		}
		else
		{
			// the normal should point at the viewer
			VectorNegate( r_forward, axis[0] );

			// scale the axes by radius
			VectorScale( r_right, particle->radius, axis[1] );
			VectorScale( r_up, particle->radius, axis[2] );
		}

		ref.vertexArray[ref.numVertex+0][0] = particle->origin1[0] + axis[1][0] + axis[2][0];
		ref.vertexArray[ref.numVertex+0][1] = particle->origin1[1] + axis[1][1] + axis[2][1];
		ref.vertexArray[ref.numVertex+0][2] = particle->origin1[2] + axis[1][2] + axis[2][2];
		ref.vertexArray[ref.numVertex+1][0] = particle->origin1[0] - axis[1][0] + axis[2][0];
		ref.vertexArray[ref.numVertex+1][1] = particle->origin1[1] - axis[1][1] + axis[2][1];
		ref.vertexArray[ref.numVertex+1][2] = particle->origin1[2] - axis[1][2] + axis[2][2];
		ref.vertexArray[ref.numVertex+2][0] = particle->origin1[0] - axis[1][0] - axis[2][0];
		ref.vertexArray[ref.numVertex+2][1] = particle->origin1[1] - axis[1][1] - axis[2][1];
		ref.vertexArray[ref.numVertex+2][2] = particle->origin1[2] - axis[1][2] - axis[2][2];
		ref.vertexArray[ref.numVertex+3][0] = particle->origin1[0] + axis[1][0] - axis[2][0];
		ref.vertexArray[ref.numVertex+3][1] = particle->origin1[1] + axis[1][1] - axis[2][1];
		ref.vertexArray[ref.numVertex+3][2] = particle->origin1[2] + axis[1][2] - axis[2][2];
	}

	ref.inTexCoordArray[ref.numVertex+0][0] = 0;
	ref.inTexCoordArray[ref.numVertex+0][1] = 0;
	ref.inTexCoordArray[ref.numVertex+1][0] = 1;
	ref.inTexCoordArray[ref.numVertex+1][1] = 0;
	ref.inTexCoordArray[ref.numVertex+2][0] = 1;
	ref.inTexCoordArray[ref.numVertex+2][1] = 1;
	ref.inTexCoordArray[ref.numVertex+3][0] = 0;
	ref.inTexCoordArray[ref.numVertex+3][1] = 1;

	for( i = 0; i < 4; i++ )
	{
		ref.normalArray[ref.numVertex][0] = axis[0][0];
		ref.normalArray[ref.numVertex][1] = axis[0][1];
		ref.normalArray[ref.numVertex][2] = axis[0][2];
		Vector4Copy( particle->modulate, ref.colorArray[ref.numVertex] ); 
		ref.numVertex++;
	}
}

/*
=================
R_AddParticlesToList
=================
*/
static void R_AddParticlesToList( void )
{
	particle_t	*particle;
	vec3_t		vec;
	int			i;

	if( !r_drawparticles->integer || !r_numParticles )
		return;

	r_stats.numParticles += r_numParticles;

	for (i = 0, particle = r_particles; i < r_numParticles; i++, particle++ )
	{
		// cull
		if( !r_nocull->integer )
		{
			VectorSubtract( particle->origin1, r_refdef.vieworg, vec );
			VectorNormalizeFast( vec );

			if( DotProduct( vec, r_forward ) < 0 )
				continue;
		}

		// add it
		R_AddMeshToList( MESH_PARTICLE, particle, particle->shader, r_worldEntity, 0 );
	}
}

/*
=================
R_DrawPoly
=================
*/
void R_DrawPoly( void )
{
	poly_t		*poly = m_pRenderMesh->mesh;
	polyVert_t	*vert;
	int			i;

	RB_CheckMeshOverflow((poly->numVerts - 2) * 3, poly->numVerts );

	for( i = 2; i < poly->numVerts; i++ )
	{
		ref.indexArray[ref.numIndex++] = ref.numVertex + 0;
		ref.indexArray[ref.numIndex++] = ref.numVertex + i-1;
		ref.indexArray[ref.numIndex++] = ref.numVertex + i;
	}
	for( i = 0, vert = poly->verts; i < poly->numVerts; i++, vert++ )
	{
		ref.vertexArray[ref.numVertex][0] = vert->point[0];
		ref.vertexArray[ref.numVertex][1] = vert->point[1];
		ref.vertexArray[ref.numVertex][2] = vert->point[2];
		ref.inTexCoordArray[ref.numVertex][0] = vert->st[0];
		ref.inTexCoordArray[ref.numVertex][1] = vert->st[1];
		ref.colorArray[ref.numVertex][0] = vert->modulate[0];
		ref.colorArray[ref.numVertex][1] = vert->modulate[1];
		ref.colorArray[ref.numVertex][2] = vert->modulate[2];
		ref.colorArray[ref.numVertex][3] = vert->modulate[3];
		ref.numVertex++;
	}
}

/*
=================
R_AddPolysToList
=================
*/
static void R_AddPolysToList( void )
{
	poly_t	*poly;
	int	i;

	if( !r_drawpolys->integer || !r_numPolys )
		return;

	r_stats.numPolys += r_numPolys;
	for( i = 0, poly = r_polys; i < r_numPolys; i++, poly++ )
		R_AddMeshToList( MESH_POLY, poly, poly->shader, r_worldEntity, 0 );
}

// =====================================================================


/*
=================
R_QSortMeshes
=================
*/
static void R_QSortMeshes( mesh_t *meshes, int numMeshes )
{
	static mesh_t	tmp;
	static int64	stack[4096];
	int		depth = 0;
	int64		L, R, l, r, median;
	qword		pivot;

	if( !numMeshes ) return;

	L = 0;
	R = numMeshes - 1;

start:
	l = L;
	r = R;

	median = (L + R) >> 1;

	if( meshes[L].sortKey > meshes[median].sortKey )
	{
		if( meshes[L].sortKey < meshes[R].sortKey ) 
			median = L;
	} 
	else if( meshes[R].sortKey < meshes[median].sortKey )
		median = R;

	pivot = meshes[median].sortKey;

	while( l < r )
	{
		while( meshes[l].sortKey < pivot )
			l++;
		while( meshes[r].sortKey > pivot )
			r--;

		if( l <= r )
		{
			tmp = meshes[r];
			meshes[r] = meshes[l];
			meshes[l] = tmp;

			l++;
			r--;
		}
	}

	if((L < r) && (depth < 4096))
	{
		stack[depth++] = l;
		stack[depth++] = R;
		R = r;
		goto start;
	}

	if( l < R )
	{
		L = l;
		goto start;
	}

	if( depth )
	{
		R = stack[--depth];
		L = stack[--depth];
		goto start;
	}
}

/*
=================
R_ISortMeshes
=================
*/
static void R_ISortMeshes( mesh_t *meshes, int numMeshes )
{
	static mesh_t	tmp;
	int		i, j;

	if( !numMeshes ) return;

	for( i = 1; i < numMeshes; i++ )
	{
		tmp = meshes[i];
		j = i - 1;

		while((j >= 0) && (meshes[j].sortKey > tmp.sortKey))
		{
			meshes[j+1] = meshes[j];
			j--;
		}

		if( i != j+1 ) meshes[j+1] = tmp;
	}
}

void R_ClearMeshes( void )
{
	int	i;

	for( i = 0; i < r_numEntities; i++ )
	{
		m_pCurrentEntity = &r_entities[i];
		R_StudioClearMeshes();
	}
}

/*
=================
R_AddMeshToList

Calculates sort key and stores info used for sorting and batching.
All 3D geometry passes this function.
=================
*/
void R_AddMeshToList( meshType_t meshType, void *mesh, ref_shader_t *shader, ref_entity_t *entity, int infoKey )
{
	mesh_t	*m;

	if( entity && (shader->flags & SHADER_RENDERMODE))
	{
		switch( entity->rendermode )
		{
		case kRenderTransColor:
			shader->sort = SORT_DECAL;
			break;
		case kRenderTransTexture:
			shader->sort = SORT_BLEND;
			break;
		case kRenderGlow:
			shader->sort = SORT_ADDITIVE;
			break;
		case kRenderTransAlpha:
			shader->sort = SORT_SEETHROUGH;
			break;
		case kRenderTransAdd:
			shader->sort = SORT_ADDITIVE;
			break;
		case kRenderNormal:
			shader->sort = SORT_OPAQUE;
			break;
		default: break; // leave unchanged
		}
	}

	if( shader->sort <= SORT_DECAL )
	{
		if( r_numSolidMeshes == MAX_MESHES )
			Host_Error( "R_AddMeshToList: MAX_MESHES hit\n" );
		m = &r_solidMeshes[r_numSolidMeshes++];
	}
	else
	{
		if( r_numTransMeshes == MAX_MESHES )
			Host_Error( "R_AddMeshToList: MAX_MESHES hit\n" );
		m = &r_transMeshes[r_numTransMeshes++];
	}

	m->sortKey = (shader->sort<<36) | (shader->shadernum<<20) | ((entity - r_entities)<<8) | (infoKey);
	m->meshType = meshType;
	m->mesh = mesh;
}


// =====================================================================
/*
=================
R_SetFrustum
=================
*/
static void R_SetFrustum( void )
{
	int	i;

	// build the transformation matrix for the given view angles
	VectorCopy( r_refdef.vieworg, r_origin );

	AngleVectorsFLU( r_refdef.viewangles, r_forward, r_right, r_up );

	RotatePointAroundVector( r_frustum[0].normal, r_up, r_forward, -(90 - r_refdef.fov_x / 2));
	RotatePointAroundVector( r_frustum[1].normal, r_up, r_forward, 90 - r_refdef.fov_x / 2);
	RotatePointAroundVector( r_frustum[2].normal, r_right, r_forward, 90 - r_refdef.fov_y / 2);
	RotatePointAroundVector( r_frustum[3].normal, r_right, r_forward, -(90 - r_refdef.fov_y / 2));

	for( i = 0; i < 4; i++ )
	{
		r_frustum[i].dist = DotProduct( r_refdef.vieworg, r_frustum[i].normal );
		PlaneClassify( &r_frustum[i] );
	}
}

/*
=================
R_SetFarClip
=================
*/
static float R_SetFarClip( void )
{
	float	farDist, dirDist, worldDist = 0;
	int	i;

	if( r_refdef.onlyClientDraw ) return 4096.0f;

	dirDist = DotProduct( r_refdef.vieworg, r_forward );
	farDist = dirDist + 256.0;

	for( i = 0; i < 3; i++ )
	{
		if( r_forward[i] < 0 )
			worldDist += (r_worldMins[i] * r_forward[i]);
		else worldDist += (r_worldMaxs[i] * r_forward[i]);
	}

	if( farDist < worldDist ) farDist = worldDist;
	return farDist - dirDist + 256.0;
}

/*
=================
R_SetMatrices
=================
*/
static void R_SetMatrices( void )
{
	float	xMax, xMin, yMax, yMin;
	float	xDiv, yDiv, zDiv;
	float	zNear, zFar;
	
	zNear = 4.0;
	zFar = R_SetFarClip();

	xMax = zNear * tan(r_refdef.fov_x * M_PI / 360.0);
	xMin = -xMax;

	yMax = zNear * tan(r_refdef.fov_y * M_PI / 360.0);
	yMin = -yMax;

	xDiv = 1.0 / (xMax - xMin);
	yDiv = 1.0 / (yMax - yMin);
	zDiv = 1.0 / (zFar - zNear);

	gl_projectionMatrix[ 0] = (2.0 * zNear) * xDiv;
	gl_projectionMatrix[ 1] = 0.0;
	gl_projectionMatrix[ 2] = 0.0;
	gl_projectionMatrix[ 3] = 0.0;
	gl_projectionMatrix[ 4] = 0.0;
	gl_projectionMatrix[ 5] = (2.0 * zNear) * yDiv;
	gl_projectionMatrix[ 6] = 0.0;
	gl_projectionMatrix[ 7] = 0.0;
	gl_projectionMatrix[ 8] = (xMax + xMin) * xDiv;
	gl_projectionMatrix[ 9] = (yMax + yMin) * yDiv;
	gl_projectionMatrix[10] = -(zNear + zFar) * zDiv;
	gl_projectionMatrix[11] = -1.0;
	gl_projectionMatrix[12] = 0.0;
	gl_projectionMatrix[13] = 0.0;
	gl_projectionMatrix[14] = -(2.0 * zNear * zFar) * zDiv;
	gl_projectionMatrix[15] = 0.0;

#if 0
	// classic slow version (used for debug)
	Matrix4x4_LoadIdentity( r_worldMatrix );
	Matrix4x4_ConcatRotate( r_worldMatrix, -90, 1, 0, 0 );	    // put Z going up
	Matrix4x4_ConcatRotate( r_worldMatrix,	90, 0, 0, 1 );	    // put Z going up
	Matrix4x4_ConcatRotate( r_worldMatrix, -r_refdef.viewangles[2],  1, 0, 0 );
	Matrix4x4_ConcatRotate( r_worldMatrix, -r_refdef.viewangles[0],  0, 1, 0 );
	Matrix4x4_ConcatRotate( r_worldMatrix, -r_refdef.viewangles[1],  0, 0, 1 );
	Matrix4x4_ConcatTranslate( r_worldMatrix, -r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2] );
#else
	Matrix4x4_CreateModelview_FromAxis( r_worldMatrix, r_forward, r_right, r_up, r_origin );
#endif
	gl_textureMatrix[ 0] = r_right[0];
	gl_textureMatrix[ 1] = -r_right[1];
	gl_textureMatrix[ 2] = -r_right[2];
	gl_textureMatrix[ 3] = 0.0;
	gl_textureMatrix[ 4] = -r_up[0];
	gl_textureMatrix[ 5] = r_up[1];
	gl_textureMatrix[ 6] = r_up[2];
	gl_textureMatrix[ 7] = 0.0;
	gl_textureMatrix[ 8] = r_forward[0];
	gl_textureMatrix[ 9] = -r_forward[1];
	gl_textureMatrix[10] = -r_forward[2];
	gl_textureMatrix[11] = 0.0;
	gl_textureMatrix[12] = 0.0;
	gl_textureMatrix[13] = 0.0;
	gl_textureMatrix[14] = 0.0;
	gl_textureMatrix[15] = 1.0;
}

/*
=================
R_RenderView
=================
*/
void R_RenderView( const ref_params_t *fd )
{
	if( r_skipfrontend->integer )
		return;

	r_numSolidMeshes = 0;
	r_numTransMeshes = 0;

	r_refdef = *fd;
	// set up frustum
	R_SetFrustum();

	// build mesh lists
	R_AddWorldToList();
	R_AddEntitiesToList();
	R_AddParticlesToList();
	R_AddPolysToList();

	// sort mesh lists
	R_QSortMeshes( r_solidMeshes, r_numSolidMeshes );
	R_ISortMeshes( r_transMeshes, r_numTransMeshes );

	// set up matrices
	R_SetMatrices();

	// go into 3D mode
	GL_Setup3D();

	// render everything
	RB_RenderMeshes( r_solidMeshes, r_numSolidMeshes );

	// R_RenderShadows();

	RB_RenderMeshes( r_transMeshes, r_numTransMeshes );

	// finish up
	R_DrawNullModels();

	RB_DebugGraphics();
	R_ClearMeshes();

	R_BloomBlend( fd );
}

// FIXME: copy screen into texRecatngle then draw as normal quad
void RB_DrawPauseScreen( void )
{
	// don't apply post effects for custom window
	if( r_refdef.onlyClientDraw ) return;

	if( r_pause->modified )
	{
		// reset saturation value
		if( !r_pause->value )
			r_pause_alpha = 0.0f;
		r_pause->modified = false;
	}
	if( !r_pause->value ) return;          
	if( r_pause_alpha < 1.0f ) r_pause_alpha += 0.03f;

	if( r_pause_alpha <= 1.0f || r_lefthand->modified )
	{
		int	k = r_pause_alpha * 255.0f;
		int	i, s, r, g, b;

		pglFlush();
		pglReadPixels( 0, 0, r_width->integer, r_height->integer, GL_RGB, GL_UNSIGNED_BYTE, r_framebuffer );
		for (i = 0; i < r_width->integer * r_height->integer * 3; i+=3)
		{
			r = r_framebuffer[i+0];
			g = r_framebuffer[i+1];
			b = r_framebuffer[i+2];
			s = (r + 2 * g + b) * k>>2; // simply bw recomputing
			r_framebuffer[i+0] = (r*(255-k)+s)>>8;
			r_framebuffer[i+1] = (g*(255-k)+s)>>8;
			r_framebuffer[i+2] = (b*(255-k)+s)>>8;
		}
		r_lefthand->modified = false;
	}

	// set custom orthogonal mode
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity ();
	pglOrtho( 0, r_width->integer, 0, r_height->integer, 0, 1.0f );
	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity ();

	pglDisable( GL_TEXTURE_2D );
	pglRasterPos2f( 0, 0 );
	pglDrawPixels( r_width->integer, r_height->integer, GL_RGB, GL_UNSIGNED_BYTE, r_framebuffer );
	pglFlush();
	pglEnable( GL_TEXTURE_2D );
}

/*
=================
R_ClearScene
=================
*/
void R_ClearScene( void )
{
	r_numEntities = 1;
	r_numDLights = 0;
	r_numParticles = 0;
	r_numPolys = 0;
	r_numPolyVerts = 0;
}

/*
====================
R_SetLightLevel

HACKHACK
====================
*/
void R_SetLightLevel( void )
{
	vec3_t	shadelight;

	if( r_refdef.onlyClientDraw ) return;

	// save off light value for server to look at (BIG HACK!)
	R_LightForPoint( r_refdef.vieworg, shadelight );

	// pick the greatest component, which should be the same
	// as the mono value returned by software
	if( shadelight[0] > shadelight[1] )
	{
		if( shadelight[0] > shadelight[2] )
			r_lightlevel->value = 150 * shadelight[0];
		else r_lightlevel->value = 150 * shadelight[2];
	}
	else
	{
		if( shadelight[1] > shadelight[2] )
			r_lightlevel->value = 150 * shadelight[1];
		else r_lightlevel->value = 150 * shadelight[2];
	}

}

/*
=================
R_AddEntityToScene
=================
*/
static bool R_AddEntityToScene( edict_t *pRefEntity, int ed_type, float lerpfrac )
{
	ref_entity_t	*refent;
	int		i;

	if( !pRefEntity || !pRefEntity->v.modelindex )
		return false; // if set to invisible, skip
	if( r_numEntities >= MAX_ENTITIES ) return false;

	refent = &r_entities[r_numEntities];

	if( pRefEntity->v.effects & EF_NODRAW )
		return true; // done

	// filter ents
	switch( ed_type )
	{
	case ED_MOVER:
	case ED_CLIENT:
	case ED_NORMAL:
	case ED_MONSTER:
	case ED_BSPBRUSH:
	case ED_RIGIDBODY:
	case ED_VIEWMODEL: break;
	default: return false;
	}

	// copy state to render
	refent->index = pRefEntity->serialnumber;
	refent->ent_type = ed_type;
	refent->backlerp = 1.0f - lerpfrac;
	refent->rendermode = pRefEntity->v.rendermode;
	refent->body = pRefEntity->v.body;
	refent->scale = pRefEntity->v.scale;
	refent->renderamt = pRefEntity->v.renderamt;
	refent->colormap = pRefEntity->v.colormap;
	refent->effects = pRefEntity->v.effects;
	if( VectorIsNull( pRefEntity->v.rendercolor ))
		VectorSet( refent->rendercolor, 255, 255, 255 );
	else VectorCopy( pRefEntity->v.rendercolor, refent->rendercolor );
	refent->model = cl_models[pRefEntity->v.modelindex];
	refent->movetype = pRefEntity->v.movetype;
	refent->framerate = pRefEntity->v.framerate;
	refent->prev.sequencetime = refent->animtime - refent->prev.animtime;

	// check model
	if( !refent->model ) return false;
	switch( refent->model->type )
	{
	case mod_brush: break;
	case mod_studio:
		if( !refent->model->phdr )
			return false;
		break;
	case mod_sprite:		
		if( !refent->model->extradata )
			return false;
		break;
	case mod_bad: // let the render drawing null model
		break;
	}

	// setup latchedvars
	VectorCopy( pRefEntity->v.oldorigin, refent->prev.origin );
	VectorCopy( pRefEntity->v.oldangles, refent->prev.angles );
		
	// interpolate origin
	for( i = 0; i < 3; i++ )
		refent->origin[i] = LerpPoint( pRefEntity->v.oldorigin[i], pRefEntity->v.origin[i], lerpfrac );

	refent->skin = pRefEntity->v.skin;
	refent->renderfx = pRefEntity->v.renderfx;
	
	// do animate
	if( refent->effects & EF_ANIMATE )
	{
		switch( refent->model->type )
		{
		case mod_studio:
			if( pRefEntity->v.frame == -1 )
			{
				pRefEntity->v.frame = refent->frame = 0;
				refent->sequence = pRefEntity->v.sequence;
				R_StudioResetSequenceInfo( refent, refent->model->phdr );
			}
			else
			{
				R_StudioFrameAdvance( refent, 0 );

				if( refent->m_fSequenceFinished )
				{
					if( refent->m_fSequenceLoops )
						pRefEntity->v.frame = -1;
					// hold at last frame
				}
				else
				{
					// copy current frame back to let user grab it on a client-side
					pRefEntity->v.frame = refent->frame;
				}
			}
			break;
		case mod_sprite:
		case mod_brush:
			break;
		}
          }
	else
	{
		refent->prev.frame = refent->frame;
		refent->frame = pRefEntity->v.frame;
		refent->prev.sequence = refent->sequence;
		refent->prev.animtime = refent->animtime;
		refent->animtime = pRefEntity->v.animtime;
		refent->sequence = pRefEntity->v.sequence;
	}

	refent->weaponmodel = cl_models[pRefEntity->v.weaponmodel];
	if( refent->ent_type == ED_MOVER || refent->ent_type == ED_BSPBRUSH )
	{
		// store conveyor movedir in pev->velocity
		VectorNormalize2( pRefEntity->v.velocity, refent->movedir );
	}

	// calculate angles
	if( refent->effects & EF_ROTATE )
	{	
		// some bonus items auto-rotate
		VectorSet( refent->angles, 0, anglemod( r_refdef.time / 10), 0 );
	}
	else
	{	
		// interpolate angles
		for( i = 0; i < 3; i++ )
			refent->angles[i] = LerpAngle( pRefEntity->v.oldangles[i], pRefEntity->v.angles[i], lerpfrac );
	}

	Matrix3x3_FromAngles( refent->angles, refent->matrix );

	if(( refent->ent_type == ED_VIEWMODEL ) && ( r_lefthand->integer == 1 ))
		VectorNegate( refent->matrix[1], refent->matrix[1] ); 

	// copy controllers
	for( i = 0; i < MAXSTUDIOCONTROLLERS; i++ )
	{
		refent->prev.controller[i] = refent->controller[i];
		refent->controller[i] = pRefEntity->v.controller[i];
	}

	// copy blends
	for( i = 0; i < MAXSTUDIOBLENDS; i++ )
	{
		refent->prev.blending[i] = refent->blending[i];
		refent->blending[i] = pRefEntity->v.blending[i];
	}

	if( refent->ent_type == ED_CLIENT )
		refent->gaitsequence = pRefEntity->v.gaitsequence;
	else refent->gaitsequence = 0;

	// because entity without models never added to scene
	if( !refent->ent_type )
	{
		switch( refent->model->type )
		{
		case mod_brush:
			refent->ent_type = ED_BSPBRUSH;
			break;
		case mod_studio:
		case mod_sprite:		
			refent->ent_type = ED_NORMAL;
          		break;
		// and ignore all other unset ents
		}
	}

	if( !refent->shader ) refent->shader = tr.defaultShader;

	// add entity
	r_numEntities++;

	return true;
}

/*
=================
R_AddLightToScene
=================
*/
static bool R_AddDynamicLight( vec3_t org, vec3_t color, float intensity )
{
	dlight_t	*dl;

	if( r_numDLights >= MAX_DLIGHTS )
		return false;

	dl = &r_dlights[r_numDLights];
	VectorCopy( org, dl->origin );
	VectorCopy( color, dl->color );
	dl->intensity = intensity;
	r_numDLights++;

	return true;
}

/*
=================
R_AddParticleToScene
=================
*/
bool R_AddParticleToScene( shader_t shader, const vec3_t org1, const vec3_t org2, float radius, float length, float rotate, rgba_t color )
{
	particle_t	*p;

	if( r_numParticles >= MAX_PARTICLES )
		return false;

	p = &r_particles[r_numParticles];

	if( shader > 0 )
		p->shader = &r_shaders[shader];
	else p->shader = tr.particleShader;

	VectorCopy( org1, p->origin1 );
	VectorCopy( org2, p->origin2 );
	p->radius = radius;
	p->length = length;
	p->rotation = rotate;
	Vector4Copy( color, p->modulate );
	r_numParticles++;

	return true;
}

static bool R_AddLightStyle( int style, vec3_t color )
{
	lightstyle_t	*ls;

	if( style < 0 || style > MAX_LIGHTSTYLES )
		return false; // invalid lightstyle

	ls = &r_lightStyles[style];
	ls->white = color[0] + color[1] + color[2];
	VectorCopy( color, ls->rgb );

	return true;
}

/*
=================
R_AddPolyToScene
=================
*/
bool R_AddPolyToScene( shader_t shader, int numVerts, const polyVert_t *verts )
{
	poly_t	*poly;

	if( r_numPolys >= MAX_POLYS || r_numPolyVerts + numVerts > MAX_POLY_VERTS )
		return false;

	poly = &r_polys[r_numPolys++];
	poly->shader = r_shaders + shader;
	poly->numVerts = numVerts;
	poly->verts = &r_polyVerts[r_numPolyVerts];
	Mem_Copy( poly->verts, verts, numVerts * sizeof( polyVert_t ));
	r_numPolyVerts += numVerts;
	return true;
}


/*
=================
R_RenderFrame
=================
*/
void R_RenderFrame( ref_params_t *rd )
{
	if( r_norefresh->integer )
		return;

	r_refdef = *rd;

	if( !r_refdef.onlyClientDraw )
	{
		if( !r_worldModel ) Host_Error( "R_RenderScene: NULL worldmodel\n" );
	}

	// Make sure all 2D stuff is flushed
	RB_RenderMesh();

	// render view
	R_RenderView( rd );

	// go into 2D mode
	GL_Setup2D();
}

/*
=================
R_BeginFrame
=================
*/
void R_BeginFrame( void )
{
	// clear r_speeds statistics
	Mem_Set( &r_stats, 0, sizeof( refstats_t ));

	if( vid_gamma->modified )
	{
		vid_gamma->modified = false;
		GL_UpdateGammaRamp();
	}

	// update texture parameters
	if( r_texturefilter->modified || r_texturefilteranisotropy->modified || r_texturelodbias->modified )
		R_SetTextureParameters();

	// set draw buffer
	if( r_frontbuffer->integer )
		pglDrawBuffer( GL_FRONT );
	else pglDrawBuffer( GL_BACK );

	// clear screen if desired
	if( gl_clear->integer )
	{
		GL_DepthMask( GL_TRUE );

		pglClearColor( 1.0, 0.0, 0.5, 0.5 );
		pglClear( GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT );
	}

	// Go into 2D mode
	GL_Setup2D();

	// check for errors
	if( r_check_errors->integer ) R_CheckForErrors();
}

/*
=================
R_EndFrame
=================
*/
void R_EndFrame( void )
{
	// make sure all 2D stuff is flushed
	RB_RenderMesh();

	// Swap the buffers
	if( !r_frontbuffer->integer )
	{
		if( !pwglSwapBuffers( glw_state.hDC ))
			Sys_Break("R_EndFrame() - SwapBuffers() failed!\n" );
	}

	// print r_speeds statistics
	if( r_speeds->integer )
	{
		switch( r_speeds->integer )
		{
		case 1:
			Msg("%i/%i shaders/stages %i meshes %i leafs %i verts %i/%i tris\n", r_stats.numShaders, r_stats.numStages, r_stats.numMeshes, r_stats.numLeafs, r_stats.numVertices, (r_stats.numIndices / 3), (r_stats.totalIndices / 3));
			break;
		case 2:
			Msg("%i entities %i dlights %i particles %i polys\n", r_stats.numEntities, r_stats.numDLights, r_stats.numParticles, r_stats.numPolys);
			break;
		}
	}

	// check for errors
	if( r_check_errors->integer ) R_CheckForErrors();
}

bool R_UploadModel( const char *name, int index )
{
	rmodel_t	*mod;

	// this array used by AddEntityToScene
	mod = R_RegisterModel( name );
	cl_models[index] = mod;

	return (mod != NULL);	
}

shader_t Mod_RegisterShader( const char *name, int shaderType )
{
	shader_t	shader = tr.defaultShader->shadernum;

	if( shaderType >= SHADER_SKY && shaderType <= SHADER_GENERIC )
		shader = R_FindShader( name, shaderType, 0 )->shadernum;
	else MsgDev( D_WARN, "Mod_RegisterShader: invalid shader type (%i)\n", shaderType );
	if( shaderType == SHADER_SKY ) R_SetupSky( name, 0, vec3_origin );

	return shader;
}

/*
=================
R_Init
=================
*/
bool R_Init( bool full )
{
	if( full )
	{
		GL_InitBackend();

		// create the window and set up the context
		if( !R_Init_OpenGL())
		{
			R_Free_OpenGL();
			return false;
		}
		GL_InitExtensions();
	}

	RB_InitBackend();

	R_InitTextures();
	R_InitPrograms();
	R_InitShaders();
	R_InitModels();
	R_CheckForErrors();

	return true;
}

/*
=================
R_Shutdown
=================
*/
void R_Shutdown( bool full )
{
	R_ShutdownModels();
	R_ShutdownShaders();
	R_ShutdownPrograms();
	R_ShutdownTextures();

	RB_ShutdownBackend();

	if( full )
	{
		GL_ShutdownBackend();

		// shut down OS specific OpenGL stuff like contexts, etc.
		R_Free_OpenGL();
	}
}

/*
@@@@@@@@@@@@@@@@@@@@@
CreateAPI

@@@@@@@@@@@@@@@@@@@@@
*/
render_exp_t DLLEXPORT *CreateAPI(stdlib_api_t *input, render_imp_t *engfuncs )
{
	static render_exp_t re;

	com = *input;
	// Sys_LoadLibrary can create fake instance, to check
	// api version and api size, but second argument will be 0
	// and always make exception, run simply check for avoid it
	if( engfuncs ) ri = *engfuncs;

	// generic functions
	re.api_size = sizeof(render_exp_t);

	re.Init = R_Init;
	re.Shutdown = R_Shutdown;
	
	re.BeginRegistration = R_BeginRegistration;
	re.RegisterModel = R_UploadModel;
	re.RegisterShader = Mod_RegisterShader;
	re.EndRegistration = R_EndRegistration;

	re.AddLightStyle = R_AddLightStyle;
	re.AddRefEntity = R_AddEntityToScene;
	re.AddDynLight = R_AddDynamicLight;
	re.AddParticle = R_AddParticleToScene;
	re.AddPolygon = R_AddPolyToScene;
	re.ClearScene = R_ClearScene;

	re.BeginFrame = R_BeginFrame;
	re.RenderFrame = R_RenderFrame;
	re.EndFrame = R_EndFrame;

	re.SetColor = GL_SetColor;
	re.GetParms = R_DrawGetParms;
	re.SetParms = R_DrawSetParms;
	re.ScrShot = VID_ScreenShot;
	re.EnvShot = VID_CubemapShot;
	re.LightForPoint = R_LightForPoint;
	re.DrawFill = R_DrawFill;
	re.DrawStretchRaw = R_DrawStretchRaw;
	re.DrawStretchPic = R_DrawStretchPic;
	re.ImpactMark = R_ImpactMark;

	return &re;
}