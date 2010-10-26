//=======================================================================
//			Copyright XashXT Group 2007 ©
//			r_studio.c - render studio models
//=======================================================================

#include "r_local.h"
#include "byteorder.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "cl_entity.h"
#include "const.h"

#define DIST_EPSILON	(0.03125)
#define EVENT_CLIENT	5000		// less than this value it's a server-side studio events

/*
=============================================================

  STUDIO MODELS

=============================================================
*/
float		(*m_protationmatrix)[4];
matrix4x4		*m_pbonestransform;

// lighting stuff
vec3_t		*m_pxformverts;
vec3_t		*m_pxformnorms;
vec3_t		*m_pxformlight;

// chrome stuff
vec2_t		*m_pchrome;
int		*m_pchromeage;
vec3_t		*m_pchromeup;
vec3_t		*m_pchromeright;

// player gait sequence stuff
int		m_fGaitEstimation;
float		m_flGaitMovement;

// misc variables
int		m_fDoInterp;			
mstudiomodel_t	*m_pSubModel;
mstudiobodyparts_t	*m_pBodyPart;
player_info_t	*m_pPlayerInfo;
studiohdr_t	*m_pStudioHeader;
studiohdr_t	*m_pTextureHeader;
mplane_t		studio_planes[12];
pmtrace_t		studio_trace;
vec3_t		studio_mins, studio_maxs;
float		studio_radius;

// studio cvars
convar_t		*r_studio_lerping;

typedef struct studioverts_s
{
	vec3_t		*verts;
	vec3_t		*norms;
	vec3_t		*light;			// light values
	vec2_t		*chrome;			// size match with numverts
	int		numverts;
	int		numnorms;
	int		m_nCachedFrame;		// to avoid transform it twice
} studioverts_t;

typedef struct studiovars_s
{
	cl_entity_t	*lerp;			// duplicate e->lerp pointer for consistency

	// cached values, valid only for CURRENT frame
	char		bonenames[MAXSTUDIOBONES][32];// used for attached entities 
	studioverts_t	*mesh[MAXSTUDIOMODELS];
	matrix4x4		rotationmatrix;
	matrix4x4		*bonestransform;
	vec3_t		*chromeright;
	vec3_t		*chromeup;
	int		*chromeage;
	int		numbones;
} studiovars_t;

static vec3_t hullcolor[8] = 
{
{ 1.0, 1.0, 1.0 },
{ 1.0, 0.5, 0.5 },
{ 0.5, 1.0, 0.5 },
{ 1.0, 1.0, 0.5 },
{ 0.5, 0.5, 1.0 },
{ 1.0, 0.5, 1.0 },
{ 0.5, 1.0, 1.0 },
{ 1.0, 1.0, 1.0 }
};

void R_StudioInitBoxHull( void );

/*
====================
R_StudioInit

====================
*/
void R_StudioInit( void )
{
	int	i;

	m_pBodyPart = NULL;
	m_pStudioHeader = NULL;
	m_flGaitMovement = 1;

	r_studio_lerping = Cvar_Get( "r_studio_lerping", "1", CVAR_ARCHIVE, "enables studio model animation lerping" );

	for( i = 1; i < MAX_ENTITIES; i++ )
	{
		r_entities[i].mempool = NULL;
		r_entities[i].extradata = NULL;
	}

	R_StudioInitBoxHull();
}

/*
====================
R_StudioFreeAllExtradata

release all ref_entity mempools
====================
*/
void R_StudioFreeAllExtradata( void )
{
	int	i;

	for( i = 1; i < MAX_ENTITIES; i++ )
	{
		Mem_FreePool( &r_entities[i].mempool );
		r_entities[i].extradata = NULL;
	}
	Mem_Set( r_entities, 0, sizeof( r_entities ));
}

/*
====================
R_StudioShutdown

====================
*/
void R_StudioShutdown( void )
{
	R_StudioFreeAllExtradata ();
}

void R_StudioAllocExtradata( cl_entity_t *in, ref_entity_t *e )
{
	studiovars_t	*studio;
	qboolean		hasChrome = (((mstudiodata_t *)e->model->extradata)->phdr->flags & STUDIO_HAS_CHROME) ? true : false;
	int		numbones = ((mstudiodata_t *)e->model->extradata)->phdr->numbones;
			
	if( !e->mempool ) e->mempool = Mem_AllocPool( va( "Entity Pool %i", e - r_entities ));
	if( !e->extradata ) e->extradata = (void *)Mem_Alloc( e->mempool, sizeof( studiovars_t ));
	studio = (studiovars_t *)e->extradata;
	studio->lerp = e->lerp;

	// any stuidio model MUST have previous data for lerping
	ASSERT( studio->lerp );

	if( studio->numbones != numbones )
	{
		size_t	cache_size = sizeof( matrix4x4 ) * numbones;
		size_t	names_size = numbones * 32; // bonename length

		// allocate or merge bones cache
		studio->bonestransform = (matrix4x4 *)Mem_Realloc( e->mempool, studio->bonestransform, cache_size );
	}

	if( hasChrome )
	{
		if( studio->numbones != numbones || !studio->chromeage || !studio->chromeright || !studio->chromeup )
		{
			// allocate or merge chrome cache
			studio->chromeage = (int *)Mem_Realloc( e->mempool, studio->chromeage, numbones * sizeof( int ));
			studio->chromeright = (vec3_t *)Mem_Realloc( e->mempool, studio->chromeright, numbones * sizeof( vec3_t ));
			studio->chromeup = (vec3_t *)Mem_Realloc( e->mempool, studio->chromeup, numbones * sizeof( vec3_t ));
		}
	}
	else
	{
		if( studio->chromeage ) Mem_Free( studio->chromeage );
		if( studio->chromeright ) Mem_Free( studio->chromeright );
		if( studio->chromeup ) Mem_Free( studio->chromeup );
		studio->chromeright = studio->chromeup = NULL;
		studio->chromeage = NULL;
	}
	studio->numbones = numbones;
}

/*
=======================================================

	MISC STUDIO UTILS

=======================================================
*/
/*
====================
StudioSetupRender

====================
*/
static void R_StudioSetupRender( ref_entity_t *e, ref_model_t *mod )
{
	mstudiodata_t	*m_pRefModel = (mstudiodata_t *)mod->extradata;

	// set global pointers
	m_pStudioHeader = m_pRefModel->phdr;
          m_pTextureHeader = m_pRefModel->thdr;

	// GetPlayerInfo returns NULL for non player entities
	m_pPlayerInfo = ri.GetPlayerInfo( e->index - 1 );

	ASSERT( e->extradata );

	// set cached bones
	m_protationmatrix = ((studiovars_t *)e->extradata)->rotationmatrix;
	m_pbonestransform = ((studiovars_t *)e->extradata)->bonestransform;

	// set chrome bones
	m_pchromeup = ((studiovars_t *)e->extradata)->chromeup;
	m_pchromeage = ((studiovars_t *)e->extradata)->chromeage;
	m_pchromeright = ((studiovars_t *)e->extradata)->chromeright;

	// misc info
	if( e->ent_type == ET_VIEWENTITY )
	{
		// viewmodel can't properly animate without lerping
		m_fDoInterp = true;
	}
	else if( r_studio_lerping->integer )
	{
		m_fDoInterp = (e->flags & EF_NOINTERP) ? false : true;
	}
	else
	{
		m_fDoInterp = false;
	}
}

void R_StudioInitBoxHull( void )
{
	int	i, side;
	mplane_t	*p;

	for( i = 0; i < 6; i++ )
	{
		side = i & 1;

		// planes
		p = &studio_planes[i*2];
		p->type = i>>1;
		p->signbits = 0;
		VectorClear( p->normal );
		p->normal[i>>1] = 1.0f;

		p = &studio_planes[i*2+1];
		p->type = 3 + (i>>1);
		p->signbits = 0;
		VectorClear( p->normal );
		p->normal[i>>1] = -1;

		p->signbits = SignbitsForPlane( p->normal );
	}    
}

void R_StudioBoxHullFromBounds( const vec3_t mins, const vec3_t maxs ) 
{
	studio_planes[0].dist = maxs[0];
	studio_planes[1].dist = -maxs[0];
	studio_planes[2].dist = mins[0];
	studio_planes[3].dist = -mins[0];
	studio_planes[4].dist = maxs[1];
	studio_planes[5].dist = -maxs[1];
	studio_planes[6].dist = mins[1];
	studio_planes[7].dist = -mins[1];
	studio_planes[8].dist = maxs[2];
	studio_planes[9].dist = -maxs[2];
	studio_planes[10].dist = mins[2];
	studio_planes[11].dist = -mins[2];
}

qboolean R_StudioTraceBox( vec3_t start, vec3_t end ) 
{
	int	i;
	mplane_t	*plane, *clipplane;
	float	enterFrac, leaveFrac;
	qboolean	getout, startout;
	float	d1, d2;
	float	f;

	enterFrac = -1.0;
	leaveFrac = 1.0;
	clipplane = NULL;

	getout = false;
	startout = false;

	// compare the trace against all planes of the brush
	// find the latest time the trace crosses a plane towards the interior
	// and the earliest time the trace crosses a plane towards the exterior
	for( i = 0; i < 6; i++ ) 
	{
		plane = studio_planes + i * 2 + (i & 1);

		d1 = DotProduct( start, plane->normal ) - plane->dist;
		d2 = DotProduct( end, plane->normal ) - plane->dist;

		if( d2 > 0.0f ) getout = true; // endpoint is not in solid
		if( d1 > 0.0f ) startout = true;

		// if completely in front of face, no intersection with the entire brush
		if( d1 > 0 && ( d2 >= DIST_EPSILON || d2 >= d1 ))
			return false;

		// if it doesn't cross the plane, the plane isn't relevent
		if( d1 <= 0 && d2 <= 0 )
			continue;

		// crosses face
		if( d1 > d2 ) 
		{
			// enter
			f = ( d1 - DIST_EPSILON ) / ( d1 - d2 );
			if( f < 0.0f ) f = 0.0f;

			if( f > enterFrac ) 
			{
				enterFrac = f;
				clipplane = plane;
			}
		} 
		else 
		{
			// leave
			f = ( d1 + DIST_EPSILON ) / ( d1 - d2 );
			if( f > 1.0f ) f = 1.0f;

			if( f < leaveFrac )
			{
				leaveFrac = f;
			}
		}
	}

	// all planes have been checked, and the trace was not
	// completely outside the brush
	if( !startout ) 
	{
		// original point was inside brush
		if( !getout ) studio_trace.fraction = 0.0f;
		return true;
	}
    
	if( enterFrac < leaveFrac ) 
	{
		if( enterFrac > -1 && enterFrac < studio_trace.fraction ) 
		{
			if( enterFrac < 0.0f )
				enterFrac = 0.0f;

			studio_trace.fraction = enterFrac;
            		VectorCopy( clipplane->normal, studio_trace.plane.normal );
            		studio_trace.plane.dist = clipplane->dist;
			return true;
		}
	}
	return false;
}

qboolean R_StudioTrace( ref_entity_t *e, const vec3_t start, const vec3_t end, pmtrace_t *tr )
{
	matrix4x4	m;
	vec3_t	start_l, end_l;
	int	i, outBone;

	R_StudioSetupRender( e, e->model );

	if( !m_pStudioHeader->numhitboxes )
	{
		tr->hitgroup = -1;
		return false;
	}

	// NOTE: we don't need to setup bones because
	// it's already setup by rendering code
	Mem_Set( &studio_trace, 0, sizeof( pmtrace_t ));
	VectorCopy( end, studio_trace.endpos );
	studio_trace.allsolid = true;
	studio_trace.fraction = 1.0f;
	studio_trace.hitgroup = -1;
	outBone = -1;

	for( i = 0; i < m_pStudioHeader->numhitboxes; i++ )
	{
		mstudiobbox_t	*phitbox = (mstudiobbox_t *)((byte*)m_pStudioHeader + m_pStudioHeader->hitboxindex) + i;

		Matrix4x4_Invert_Simple( m, m_pbonestransform[phitbox->bone] );
		Matrix4x4_VectorTransform( m, start, start_l );
		Matrix4x4_VectorTransform( m, end, end_l );

		R_StudioBoxHullFromBounds( phitbox->bbmin, phitbox->bbmax );

		if( R_StudioTraceBox( start_l, end_l ))
		{
			outBone = phitbox->bone;
			studio_trace.hitgroup = i;	// it's a hitbox, not a hitgroup!
		}

		if( studio_trace.fraction == 0.0f )
			break;
	}

	if( studio_trace.fraction > 0.0f )
		studio_trace.allsolid = false;

	// all hitboxes were swept, get trace result
	if( outBone >= 0 )
	{
		tr->fraction = studio_trace.fraction;
		tr->hitgroup = studio_trace.hitgroup;
		tr->allsolid = studio_trace.allsolid;
		tr->ent = e - r_entities;

		Matrix4x4_VectorRotate( m_pbonestransform[outBone], studio_trace.endpos, tr->endpos );
		if( tr->fraction == 1.0f )
		{
			VectorCopy( end, tr->endpos );
		}
		else
		{
			mstudiobone_t *pbone = (mstudiobone_t *)((byte*)m_pStudioHeader + m_pStudioHeader->boneindex) + outBone;

//			MsgDev( D_INFO, "Bone name %s\n", pbone->name ); // debug
			VectorLerp( start, tr->fraction, end, tr->endpos );
			r_debug_hitbox = pbone->name;
		}
		tr->plane.dist = DotProduct( tr->endpos, tr->plane.normal );

		return true;
	}
	return false;
}

// extract texture filename from modelname
char *R_StudioTexName( ref_model_t *mod )
{
	static string	texname;

	com.strncpy( texname, mod->name, MAX_STRING );
	FS_StripExtension( texname );
	com.strncat( texname, "T.mdl", MAX_STRING );
	return texname;
}

int R_StudioExtractBbox( studiohdr_t *phdr, int sequence, float *mins, float *maxs )
{
	mstudioseqdesc_t	*pseqdesc;
	pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);

	if( sequence < 0 || sequence >= phdr->numseq )
		return 0;
	
	VectorCopy( pseqdesc[sequence].bbmin, mins );
	VectorCopy( pseqdesc[sequence].bbmax, maxs );

	return 1;
}

void R_StudioModelLerpBBox( ref_entity_t *e, ref_model_t *mod )
{
	// FIXME: implement
}

void R_StudioModelBBox( ref_entity_t *e, vec3_t mins, vec3_t maxs )
{
	studiohdr_t	*hdr;

	if( !e->model ) return;
	R_StudioSetupRender( e, e->model );

	hdr = ((mstudiodata_t *)e->model->extradata)->phdr;
	if( !hdr ) return;
	R_StudioExtractBbox( hdr, e->lerp->curstate.sequence, mins, maxs );
}

/*
====================
Studio model loader
====================
*/
void R_StudioSurfaceParm( mstudiotexture_t *tex )
{
	if( tex->flags & STUDIO_NF_TRANSPARENT )
		R_ShaderSetRenderMode( kRenderTransAlpha, false );
	else if( tex->flags & STUDIO_NF_ADDITIVE )
		R_ShaderSetRenderMode( kRenderTransAdd, false );
	else if( tex->flags & STUDIO_NF_BLENDED )
		R_ShaderSetRenderMode( kRenderTransTexture, false );
	else R_ShaderSetRenderMode( kRenderNormal, false );
}

texture_t *R_StudioLoadTexture( ref_model_t *mod, studiohdr_t *phdr, mstudiotexture_t *ptexture )
{
	size_t	size;
	int	flags = 0;
	string	name;

	if( ptexture->flags & STUDIO_NF_TRANSPARENT )
		flags |= (TF_CLAMP|TF_NOMIPMAP);

	if( ptexture->flags & (STUDIO_NF_NORMALMAP|STUDIO_NF_HEIGHTMAP ))
		flags |= TF_NORMALMAP;

	// NOTE: replace index with pointer to start of imagebuffer, ImageLib expected it
	ptexture->index = (int)((byte *)phdr) + ptexture->index;
	size = sizeof( mstudiotexture_t ) + ptexture->width * ptexture->height + 768;

	// build the texname
	com.snprintf( name, sizeof( name ), "\"%s/%s\"", mod->name, ptexture->name );

	if( ptexture->flags & STUDIO_NF_HEIGHTMAP )
		return R_FindTexture( va( "mergeDepthmap( heightMap( Studio( %s ), 2.0 ), Studio( %s ));", name, name ), (byte *)ptexture, size, flags );
	return R_FindTexture( va( "Studio( %s );", name ), (byte *)ptexture, size, flags );
}

static int R_StudioLoadTextures( ref_model_t *mod, studiohdr_t *phdr )
{
	texture_t		*texs[4];
	mstudiotexture_t	*ptexture = (mstudiotexture_t *)(((byte *)phdr) + phdr->textureindex);
	string		modname, texname, shadername;
	string		normalmap, heightmap;
	int		nm_index, hm_index;
	int		i, j, numshaders = 0;

	FS_FileBase( mod->name, modname );
	
	for( i = 0; i < phdr->numtextures; i++ )
	{
		if( ptexture[i].flags & (STUDIO_NF_NORMALMAP|STUDIO_NF_HEIGHTMAP|STUDIO_NF_GLOSSMAP|STUDIO_NF_DECALMAP))
			continue; // doesn't produce dead shaders

		// misc
		R_StudioSurfaceParm( &ptexture[i] );
		FS_FileBase( ptexture[i].name, texname );
		if( ptexture[i].flags & STUDIO_NF_CHROME ) phdr->flags |= STUDIO_HAS_CHROME;
		com.snprintf( shadername, MAX_STRING, "%s.mdl/%s", modname, texname );

		if( R_ShaderCheckCache( shadername ))
 			goto load_shader;	// external shader found
 
		Mem_Set( texs, 0, sizeof( texs ));
		normalmap[0] = heightmap[0] = '\0';
		nm_index = hm_index = 0;

		texs[0] = R_StudioLoadTexture( mod, phdr, ptexture + i );
		if( !texs[0] ) texs[0] = tr.defaultTexture;
		R_ShaderAddStageTexture( texs[0] ); // load diffuse texture
				
		// check for normalmap
		for( j = 0; j < phdr->numtextures; j++ )
		{
			if( !com.strnicmp( texname, ptexture[j].name, com.strlen( texname )))
			{
				if( ptexture[j].flags & STUDIO_NF_HEIGHTMAP )
				{
					// determine heightmap name
					com.snprintf( heightmap, sizeof( heightmap ), "%s/%s", mod->name, ptexture[j].name );
        					hm_index = j;
        				}
				else if( ptexture[j].flags & STUDIO_NF_NORMALMAP )
				{
					// determine normalmap name
					com.snprintf( normalmap, sizeof( normalmap ), "%s/%s", mod->name, ptexture[j].name );
					nm_index = j;
				}
			}
		}

		// load it in
		if( heightmap[0] && normalmap[0] )
		{
			// merge both textures into single and turn parallax on
			texs[1] = R_FindTexture( va( "mergeDepthmap( Studio( \"%s\" ), Studio( \"%s\" ));", normalmap, heightmap ), NULL, 0, TF_NORMALMAP );
		}
		else if( normalmap[0] ) texs[1] = R_StudioLoadTexture( mod, phdr, ptexture + nm_index );
		else if( heightmap[0] ) texs[1] = R_StudioLoadTexture( mod, phdr, ptexture + hm_index );

		if( texs[1] == NULL ) goto load_shader;	// failed to load material
		R_ShaderAddStageTexture( texs[1] );

		// check for glossmap
		for( j = 0; j < phdr->numtextures; j++ )
		{
			if( ptexture[j].flags & (STUDIO_NF_GLOSSMAP) && !com.strnicmp( texname, ptexture[j].name, com.strlen( texname )))
			{
				texs[2] = R_StudioLoadTexture( mod, phdr, ptexture + j );
				break;
			}
		}
		R_ShaderAddStageTexture( texs[2] );

		// check for decalmap
		for( j = 0; j < phdr->numtextures; j++ )
		{
			if( ptexture[j].flags & (STUDIO_NF_DECALMAP) && !com.strnicmp( texname, ptexture[j].name, com.strlen( texname )))
			{
				texs[3] = R_StudioLoadTexture( mod, phdr, ptexture + j );
				break;
			}
		}
		R_ShaderAddStageTexture( texs[3] );
load_shader:
		mod->shaders[numshaders] = R_LoadShader( shadername, SHADER_STUDIO, 0, 0, SHADER_INVALID );
		ptexture[numshaders].index = mod->shaders[numshaders]->shadernum;
		numshaders++;
	}
	return numshaders;
}

studiohdr_t *R_StudioLoadHeader( ref_model_t *mod, const uint *buffer )
{
	byte		*pin;
	studiohdr_t	*phdr;
	mstudiotexture_t	*ptexture;
	string		modname;
	
	pin = (byte *)buffer;
	phdr = (studiohdr_t *)pin;

	if( phdr->version != STUDIO_VERSION )
	{
		MsgDev( D_ERROR, "%s has wrong version number (%i should be %i)\n", phdr->name, phdr->version, STUDIO_VERSION );
		return NULL;
	}	
	FS_FileBase( mod->name, modname );

	ptexture = (mstudiotexture_t *)(((byte *)phdr) + phdr->textureindex);
	if( phdr->textureindex > 0 && phdr->numtextures <= MAXSTUDIOSKINS )
	{
		mod->shaders = Mod_Malloc( mod, sizeof( shader_t* ) * phdr->numtextures );
		mod->numshaders = R_StudioLoadTextures( mod, phdr );

		if( mod->numshaders != phdr->numtextures ) // bump, gloss will be merged into single shader
			mod->shaders = Mod_Realloc( mod, mod->shaders, sizeof( shader_t* ) * mod->numshaders );
	}
	return (studiohdr_t *)buffer;
}

void Mod_StudioLoadModel( ref_model_t *mod, const void *buffer )
{
	studiohdr_t	*phdr = R_StudioLoadHeader( mod, buffer );
	studiohdr_t	*thdr = NULL;
	mstudiodata_t	*poutmodel;
	void		*texbuf;
	
	if( !phdr ) return; // there were problems
	mod->extradata = poutmodel = (mstudiodata_t *)Mod_Malloc( mod, sizeof( mstudiodata_t ));
	poutmodel->phdr = (studiohdr_t *)Mod_Malloc( mod, LittleLong( phdr->length ));
	Mem_Copy( poutmodel->phdr, buffer, LittleLong( phdr->length ));
	
	if( phdr->numtextures == 0 )
	{
		texbuf = FS_LoadFile( R_StudioTexName( mod ), NULL );
		if( texbuf ) thdr = R_StudioLoadHeader( mod, texbuf );
		else MsgDev( D_ERROR, "StudioLoadModel: %s missing textures file\n", mod->name ); 

		if( !thdr ) return; // there were problems
		poutmodel->thdr = (studiohdr_t *)Mod_Malloc( mod, LittleLong( thdr->length ));
          	Mem_Copy( poutmodel->thdr, texbuf, LittleLong( thdr->length ));
		if( texbuf ) Mem_Free( texbuf );
	}
	else poutmodel->thdr = poutmodel->phdr; // just make link
	poutmodel->phdr->flags |= poutmodel->thdr->flags;	// copy STUDIO_HAS_CHROME flag

	R_StudioExtractBbox( phdr, 0, mod->mins, mod->maxs );
	mod->radius = RadiusFromBounds( mod->mins, mod->maxs );
	mod->touchFrame = tr.registration_sequence;
	mod->type = mod_studio;
}

/*
====================
R_StudioProcessEvents

====================
*/
void R_StudioProcessEvents( ref_entity_t *e, cl_entity_t *ent )
{
	mstudioseqdesc_t	*pseqdesc;
	mstudioevent_t	*pevent;
	float		flEventFrame;
	qboolean		bLooped = false;
	int		i;

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->lerp->curstate.sequence;
	pevent = (mstudioevent_t *)((byte *)m_pStudioHeader + pseqdesc->eventindex);

	// curstate.frame not used for viewmodel animating
	flEventFrame = e->lerp->latched.prevframe;

	if( pseqdesc->numevents == 0 )
		return;

	//Msg( "%i frame %f\n", r_framecount, e->lerp->latched.prevframe );

	if( e->lerp->m_iEventSequence != e->lerp->curstate.sequence )
	{
		flEventFrame = 0.0f;
		e->lerp->m_flPrevEventFrame = -0.01f; // back up to get 0'th frame animations
		e->lerp->m_iEventSequence = e->lerp->curstate.sequence;
	}

	// stalled?
	if( flEventFrame == e->lerp->m_flPrevEventFrame )
		return;

	//Msg( "(seq %d cycle %.3f ) evframe %.3f prevevframe %.3f (time %.3f)\n", e->lerp->curstate.sequence, e->lerp->latched.prevframe, flEventFrame, e->lerp->m_flPrevEventFrame, RI.refdef.time );

	// check for looping
	if( flEventFrame <= e->lerp->m_flPrevEventFrame )
	{
		if( e->lerp->m_flPrevEventFrame - flEventFrame > 0.5f )
		{
			bLooped = true;
		}
		else
		{
			// things have backed up, which is bad since it'll probably result in a hitch in the animation playback
			// but, don't play events again for the same time slice
			return;
		}
	}

	for( i = 0; i < pseqdesc->numevents; i++ )
	{
		// ignore all non-client-side events
		if( pevent[i].event < EVENT_CLIENT )
			continue;

		// looped
		if( bLooped )
		{
			if(( pevent[i].frame > e->lerp->m_flPrevEventFrame || pevent[i].frame <= flEventFrame ))
			{
				//Msg( "FE %i Looped frame %i, prev %f ev %f (time %.3f)\n", pevent[i].event, pevent[i].frame, e->lerp->m_flPrevEventFrame, flEventFrame, RI.refdef.time );
				ri.StudioEvent( &pevent[i], ent );
			}
		}
		else
		{
			if(( pevent[i].frame > e->lerp->m_flPrevEventFrame && pevent[i].frame <= flEventFrame ))
			{
				//Msg( "FE %i Normal frame %i, prev %f ev %f (time %.3f)\n", pevent[i].event, pevent[i].frame, e->lerp->m_flPrevEventFrame, flEventFrame, RI.refdef.time );
				ri.StudioEvent( &pevent[i], ent );
			}
		}
	}

	e->lerp->m_flPrevEventFrame = flEventFrame;
}

/*
====================
StudioCalcBoneAdj

====================
*/
void R_StudioCalcBoneAdj( float dadt, float *adj, const byte *pcontroller1, const byte *pcontroller2, byte mouthopen )
{
	int			i, j;
	float			value;
	mstudiobonecontroller_t	*pbonecontroller;
	
	pbonecontroller = (mstudiobonecontroller_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bonecontrollerindex);

	for (j = 0; j < m_pStudioHeader->numbonecontrollers; j++)
	{
		i = pbonecontroller[j].index;

		if( i == STUDIO_MOUTH )
		{
			// mouth hardcoded at controller 4
			value = mouthopen / 64.0f;
			if( value > 1.0f ) value = 1.0f;				
			value = (1.0f - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			//Msg( "%d %f\n", mouthopen, value );
		}
		else if( i <= MAXSTUDIOCONTROLLERS )
		{
			// check for 360% wrapping
			if( pbonecontroller[j].type & STUDIO_RLOOP )
			{
				if( abs( pcontroller1[i] - pcontroller2[i] ) > 128 )
				{
					float	a, b;
					a = fmod(( pcontroller1[j] + 128 ), 256 );
					b = fmod(( pcontroller2[j] + 128 ), 256 );
					value = ((a * dadt) + (b * (1 - dadt)) - 128) * (360.0/256.0) + pbonecontroller[j].start;
				}
				else 
				{
					value = ((pcontroller1[i] * dadt + (pcontroller2[i]) * (1.0 - dadt))) * (360.0/256.0) + pbonecontroller[j].start;
				}
			}
			else 
			{
				value = (pcontroller1[i] * dadt + pcontroller2[i] * (1.0 - dadt)) / 255.0;
				value = bound( 0.0f, value, 1.0f );
				value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			}
			//Msg( "%d %d %f : %f\n", RI.currententity->lerp->curstate.controller[j], RI.currententity->lerp->latched.prevcontroller[j], value, dadt );
		}

		switch( pbonecontroller[j].type & STUDIO_TYPES )
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = value * (M_PI / 180.0);
			break;
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			adj[j] = value;
			break;
		}
	}
}

/*
====================
StudioCalcBoneQuaterion

====================
*/
void R_StudioCalcBoneQuaterion( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *q )
{
	int		j, k;
	vec4_t		q1, q2;
	vec3_t		angle1, angle2;
	mstudioanimvalue_t	*panimvalue;

	for( j = 0; j < 3; j++ )
	{
		if( panim->offset[j+3] == 0 )
		{
			angle2[j] = angle1[j] = pbone->value[j+3]; // default;
		}
		else
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j+3]);
			k = frame;
			
			// debug
			if( panimvalue->num.total < panimvalue->num.valid ) k = 0;
			
			while( panimvalue->num.total <= k )
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;

				// debug
				if( panimvalue->num.total < panimvalue->num.valid )
					k = 0;
			}

			// bah, missing blend!
			if( panimvalue->num.valid > k )
			{
				angle1[j] = panimvalue[k+1].value;

				if( panimvalue->num.valid > k + 1 )
				{
					angle2[j] = panimvalue[k+2].value;
				}
				else
				{
					if( panimvalue->num.total > k + 1 )
						angle2[j] = angle1[j];
					else angle2[j] = panimvalue[panimvalue->num.valid+2].value;
				}
			}
			else
			{
				angle1[j] = panimvalue[panimvalue->num.valid].value;

				if( panimvalue->num.total > k + 1 )
				{
					angle2[j] = angle1[j];
				}
				else
				{
					angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}

			angle1[j] = pbone->value[j+3] + angle1[j] * pbone->scale[j+3];
			angle2[j] = pbone->value[j+3] + angle2[j] * pbone->scale[j+3];
		}

		if( pbone->bonecontroller[j+3] != -1 )
		{
			angle1[j] += adj[pbone->bonecontroller[j+3]];
			angle2[j] += adj[pbone->bonecontroller[j+3]];
		}
	}

	if( !VectorCompare( angle1, angle2 ))
	{
		AngleQuaternion( angle1, q1 );
		AngleQuaternion( angle2, q2 );
		QuaternionSlerp( q1, q2, s, q );
	}
	else
	{
		AngleQuaternion( angle1, q );
	}
}

/*
====================
StudioCalcBonePosition

====================
*/
void R_StudioCalcBonePosition( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *pos )
{
	int		j, k;
	mstudioanimvalue_t	*panimvalue;

	for( j = 0; j < 3; j++ )
	{
		pos[j] = pbone->value[j]; // default;

		if( panim->offset[j] != 0 )
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j]);
			
			//if( j == 0 ) Msg( "%d  %d:%d  %f\n", frame, panimvalue->num.valid, panimvalue->num.total, s );
			k = frame;

			// debug
			if( panimvalue->num.total < panimvalue->num.valid )
				k = 0;

			// find span of values that includes the frame we want
			while( panimvalue->num.total <= k )
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;

  				// debug
				if( panimvalue->num.total < panimvalue->num.valid )
					k = 0;
			}

			// if we're inside the span
			if( panimvalue->num.valid > k )
			{
				// and there's more data in the span
				if( panimvalue->num.valid > k + 1 )
				{
					pos[j] += (panimvalue[k+1].value * (1.0f - s) + s * panimvalue[k+2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[k+1].value * pbone->scale[j];
				}
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if( panimvalue->num.total <= k + 1 )
				{
					pos[j] += (panimvalue[panimvalue->num.valid].value * (1.0 - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
				}
			}
		}
		if( pbone->bonecontroller[j] != -1 && adj )
		{
			pos[j] += adj[pbone->bonecontroller[j]];
		}
	}
}

/*
====================
StudioSlerpBones

====================
*/
void R_StudioSlerpBones( vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s )
{
	int	i;
	vec4_t	q3;
	float	s1;

	s = bound( 0.0f, s, 1.0f );

	s1 = 1.0f - s;

	for( i = 0; i < m_pStudioHeader->numbones; i++ )
	{
		QuaternionSlerp( q1[i], q2[i], s, q3 );
		q1[i][0] = q3[0];
		q1[i][1] = q3[1];
		q1[i][2] = q3[2];
		q1[i][3] = q3[3];
		pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s;
		pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s;
		pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s;
	}
}

/*
====================
Cache_Check

consistency check
====================
*/
void *Cache_Check( byte *mempool, cache_user_t *c )
{
	if( !c->data )
		return NULL;

	if( !Mem_IsAllocated( mempool, c->data ))
		return NULL;

	return c->data;
}

/*
====================
StudioGetAnim

====================
*/
mstudioanim_t *R_StudioGetAnim( ref_model_t *m_pRefModel, mstudioseqdesc_t *pseqdesc )
{
	mstudioseqgroup_t	*pseqgroup;
	cache_user_t	*paSequences;
	mstudiodata_t	*m_pSubModel = (mstudiodata_t *)m_pRefModel->extradata;
          size_t		filesize;
          byte		*buf;

	ASSERT( m_pSubModel );	

	pseqgroup = (mstudioseqgroup_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqgroupindex) + pseqdesc->seqgroup;
	if( pseqdesc->seqgroup == 0 )
		return (mstudioanim_t *)((byte *)m_pStudioHeader + pseqgroup->data + pseqdesc->animindex);

	paSequences = (cache_user_t *)m_pSubModel->submodels;

	if( paSequences == NULL )
	{
		paSequences = (cache_user_t *)Mem_Alloc( m_pRefModel->mempool, MAXSTUDIOGROUPS * sizeof( cache_user_t ));
		m_pSubModel->submodels = (void *)paSequences;
	}

	if( !Cache_Check( m_pRefModel->mempool, ( cache_user_t *)&( paSequences[pseqdesc->seqgroup] )))
	{
		string	filepath, modelname, modelpath;

		FS_FileBase( m_pRefModel->name, modelname );
		FS_ExtractFilePath( m_pRefModel->name, modelpath );
		com.snprintf( filepath, sizeof( filepath ), "%s/%s%i%i.mdl", modelpath, modelname, pseqdesc->seqgroup / 10, pseqdesc->seqgroup % 10 );

		buf = FS_LoadFile( filepath, &filesize );
		if( !buf || !filesize ) Host_Error( "R_StudioGetAnim: can't load %s\n", filepath );
		if( IDSEQGRPHEADER != LittleLong(*(uint *)buf ))
			Host_Error( "R_StudioGetAnim: %s is corrupted\n", filepath );

		MsgDev( D_NOTE, "R_StudioGetAnim: %s\n", filepath );
			
		paSequences[pseqdesc->seqgroup].data = Mem_Alloc( m_pRefModel->mempool, filesize );
		Mem_Copy( paSequences[pseqdesc->seqgroup].data, buf, filesize );
		Mem_Free( buf );
	}
	return (mstudioanim_t *)((byte *)paSequences[pseqdesc->seqgroup].data + pseqdesc->animindex);
}

/*
====================
StudioPlayerBlend

====================
*/
void R_StudioPlayerBlend( mstudioseqdesc_t *pseqdesc, int *pBlend, float *pPitch )
{
	// calc up/down pointing
	*pBlend = (*pPitch * 3);

	if( *pBlend < pseqdesc->blendstart[0] )
	{
		*pPitch -= pseqdesc->blendstart[0] / 3.0f;
		*pBlend = 0;
	}
	else if( *pBlend > pseqdesc->blendend[0] )
	{
		*pPitch -= pseqdesc->blendend[0] / 3.0f;
		*pBlend = 255;
	}
	else
	{
		if( pseqdesc->blendend[0] - pseqdesc->blendstart[0] < 0.1f ) // catch qc error
			*pBlend = 127;
		else *pBlend = 255.0f * (*pBlend - pseqdesc->blendstart[0]) / (pseqdesc->blendend[0] - pseqdesc->blendstart[0]);
		*pPitch = 0;
	}
}

/*
====================
StudioSetUpTransform

====================
*/
void R_StudioSetUpTransform( ref_entity_t *e, qboolean trivial_accept )
{
	vec3_t		angles, origin;
	int		i;

	if( trivial_accept )
	{ 
		Matrix4x4_Copy( RI.objectMatrix, ((studiovars_t *)e->extradata)->rotationmatrix );
		Matrix4x4_ConcatTransforms( RI.modelviewMatrix, RI.worldviewMatrix, RI.objectMatrix );
		GL_LoadMatrix( RI.modelviewMatrix );
		tr.modelviewIdentity = false;
		return;
	}

	if( RP_FOLLOWENTITY( e ))
	{
		Matrix4x4_Copy( m_protationmatrix, ((studiovars_t *)e->parent->extradata)->rotationmatrix );
		m_protationmatrix = matrix4x4_identity;
		return;
	}

	VectorCopy( e->origin, origin );
	VectorCopy( e->angles, angles );

	// interoplate monsters position
	if( e->movetype == MOVETYPE_STEP || e->movetype == MOVETYPE_FLY ) 
	{
		float		d, f = 0.0f;
		cl_entity_t	*m_pGroundEntity;

		// don't do it if the goalstarttime hasn't updated in a while.
		// NOTE: Because we need to interpolate multiplayer characters, the interpolation time limit
		// was increased to 1.0 s., which is 2x the max lag we are accounting for.

		if( m_fDoInterp && ( RI.refdef.time < e->lerp->curstate.animtime + 1.0f ) && ( e->lerp->curstate.animtime != e->lerp->latched.prevanimtime ))
		{
			f = ( RI.refdef.time - e->lerp->curstate.animtime ) / ( e->lerp->curstate.animtime - e->lerp->latched.prevanimtime );
			// Msg( "%4.2f %.2f %.2f\n", f, e->lerp->curstate.animtime, RI.refdef.time );
		}

		if( m_fDoInterp )
		{
			// ugly hack to interpolate angle, position.
			// current is reached 0.1 seconds after being set
			f = f - 1.0f;
		}

		m_pGroundEntity = e->lerp->onground;

		if( m_pGroundEntity && m_pGroundEntity->curstate.movetype == MOVETYPE_PUSH && !VectorIsNull( m_pGroundEntity->curstate.velocity ))
		{
			mstudioseqdesc_t		*pseqdesc;
		
			pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->lerp->curstate.sequence;
			d = RI.lerpFrac;

			origin[0] += ( e->origin[0] - e->lerp->prevstate.origin[0] ) * d;
			origin[1] += ( e->origin[1] - e->lerp->prevstate.origin[1] ) * d;
			origin[2] += ( e->origin[2] - e->lerp->prevstate.origin[2] ) * d;

			d = f - d;

			// monster walking on moving platform
			if( pseqdesc->motiontype & STUDIO_LX )
			{
				origin[0] += ( e->lerp->curstate.origin[0] - e->lerp->latched.prevorigin[0] ) * d;
				origin[1] += ( e->lerp->curstate.origin[1] - e->lerp->latched.prevorigin[1] ) * d;
				origin[2] += ( e->lerp->curstate.origin[2] - e->lerp->latched.prevorigin[2] ) * d;
			}
		}
		else
		{
			origin[0] += ( e->lerp->curstate.origin[0] - e->lerp->latched.prevorigin[0] ) * f;
			origin[1] += ( e->lerp->curstate.origin[1] - e->lerp->latched.prevorigin[1] ) * f;
			origin[2] += ( e->lerp->curstate.origin[2] - e->lerp->latched.prevorigin[2] ) * f;
		}

		for( i = 0; i < 3; i++ )
		{
			float	ang1, ang2;

			ang1 = e->lerp->curstate.angles[i];
			ang2 = e->lerp->latched.prevangles[i];

			d = ang1 - ang2;

			if( d > 180 ) d -= 360;
			else if( d < -180 ) d += 360;

			angles[i] += d * f;
		}

		// update it so attachments always have right pos
		if( !RI.refdef.paused ) VectorCopy( origin, e->origin );
	}

	// don't rotate clients, only aim
	if( e->ent_type == ET_PLAYER )
		angles[PITCH] = 0;

	if( e->ent_type == ET_VIEWENTITY )
		angles[PITCH] = -angles[PITCH]; // stupid Half-Life bug

	Matrix4x4_CreateFromEntity( m_protationmatrix, origin[0], origin[1], origin[2], -angles[PITCH], angles[YAW], angles[ROLL], e->scale );

	if( e->ent_type == ET_VIEWENTITY && r_lefthand->integer == 1 )
	{
		m_protationmatrix[0][1] = -m_protationmatrix[0][1];
		m_protationmatrix[1][1] = -m_protationmatrix[1][1];
		m_protationmatrix[2][1] = -m_protationmatrix[2][1];
	}

	// save matrix for gl_transform, use identity matrix for bones
	m_protationmatrix = matrix4x4_identity;
}


/*
====================
StudioEstimateInterpolant

====================
*/
float R_StudioEstimateInterpolant( void )
{
	float	dadt = 1.0f;

	if( m_fDoInterp && ( RI.currententity->lerp->curstate.animtime >= RI.currententity->lerp->latched.prevanimtime + 0.01f ))
	{
		dadt = ( RI.refdef.time - RI.currententity->lerp->curstate.animtime ) / 0.1f;
		if( dadt > 2.0f ) dadt = 2.0f;
	}
	return dadt;
}


/*
====================
StudioCalcRotations

====================
*/
void R_StudioCalcRotations( float pos[][3], vec4_t *q, mstudioseqdesc_t *pseqdesc, mstudioanim_t *panim, float f )
{
	int		i;
	int		frame;
	mstudiobone_t	*pbone;
	studiovars_t	*pstudio;
	float		s;
	float		adj[MAXSTUDIOCONTROLLERS];
	float		dadt;

	if( f > pseqdesc->numframes - 1 )
	{
		f = 0.0f; // bah, fix this bug with changing sequences too fast
	}
	else if( f < -0.01f )
	{
		// BUGBUG ( somewhere else ) but this code should validate this data.
		// This could cause a crash if the frame # is negative, so we'll go ahead
		// and clamp it here
		MsgDev( D_ERROR, "R_StudioCalcRotations: f = %g\n", f );
		f = -0.01f;
	}

	frame = (int)f;
	pstudio = RI.currententity->extradata;
	ASSERT( pstudio );

	//Msg( "%d %.4f %.4f %.4f %.4f %d\n", RI.currententity->lerp->curstate.sequence, RI.refdef.time, RI.currententity->lerp->curstate.animtime, RI.currententity->lerp->curstate.frame, f, frame );
	//Msg( "%f %f %f\n", RI.currententity->angles[ROLL], RI.currententity->angles[PITCH], RI.currententity->angles[YAW] );
	//Msg( "frame %d %d\n", frame1, frame2 );

	dadt = R_StudioEstimateInterpolant();
	s = (f - frame);

	// add in programtic controllers
	pbone = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	R_StudioCalcBoneAdj( dadt, adj, pstudio->lerp->curstate.controller, pstudio->lerp->latched.prevcontroller, pstudio->lerp->mouth.mouthopen );

	for( i = 0; i < m_pStudioHeader->numbones; i++, pbone++, panim++ ) 
	{
		R_StudioCalcBoneQuaterion( frame, s, pbone, panim, adj, q[i] );
		R_StudioCalcBonePosition( frame, s, pbone, panim, adj, pos[i] );
		// if( 0 && i == 0 ) Msg( "%d %d %d %d\n", RI.currententity->lerp->curstate.sequence, frame, j, k );
	}

	if( pseqdesc->motiontype & STUDIO_X ) pos[pseqdesc->motionbone][0] = 0.0f;
	if( pseqdesc->motiontype & STUDIO_Y ) pos[pseqdesc->motionbone][1] = 0.0f;
	if( pseqdesc->motiontype & STUDIO_Z ) pos[pseqdesc->motionbone][2] = 0.0f;

	// FIXME: enable this ? Half-Life 2 get rid of this code
	s = 0 * ((1.0f - (f - (int)(f))) / (pseqdesc->numframes)) * RI.currententity->framerate;

	if( pseqdesc->motiontype & STUDIO_LX ) pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];
	if( pseqdesc->motiontype & STUDIO_LY ) pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];
	if( pseqdesc->motiontype & STUDIO_LZ ) pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];
}

/*
====================
StudioEstimateFrame

====================
*/
float R_StudioEstimateFrame( mstudioseqdesc_t *pseqdesc )
{
	double	dfdt, f;
	
	if( m_fDoInterp )
	{
		if( RI.refdef.time < RI.currententity->lerp->curstate.animtime ) dfdt = 0;
		else dfdt = (RI.refdef.time - RI.currententity->lerp->curstate.animtime) * RI.currententity->framerate * pseqdesc->fps;
	}
	else dfdt = 0;

	if( pseqdesc->numframes <= 1 ) f = 0.0;
	else f = (RI.currententity->lerp->curstate.frame * (pseqdesc->numframes - 1)) / 256.0;
 
	f += dfdt;

	if( pseqdesc->flags & STUDIO_LOOPING ) 
	{
		if( pseqdesc->numframes > 1 ) f -= (int)(f / (pseqdesc->numframes - 1)) *  (pseqdesc->numframes - 1);
		if( f < 0 ) f += (pseqdesc->numframes - 1);
	}
	else 
	{
		if( f >= pseqdesc->numframes - 1.001 ) f = pseqdesc->numframes - 1.001;
		if( f < 0.0 )  f = 0.0;
	}
	return f;
}

/*
====================
StudioSetupBones

====================
*/
void R_StudioSetupBones( ref_entity_t *e )
{
	int		i;
	double		f;

	mstudiobone_t	*pbones;
	mstudioseqdesc_t	*pseqdesc;
	mstudioanim_t	*panim;
	studiovars_t	*pstudio;

	static float	pos[MAXSTUDIOBONES][3];
	static vec4_t	q[MAXSTUDIOBONES];
	matrix4x4		bonematrix;

	static float	pos2[MAXSTUDIOBONES][3];
	static vec4_t	q2[MAXSTUDIOBONES];
	static float	pos3[MAXSTUDIOBONES][3];
	static vec4_t	q3[MAXSTUDIOBONES];
	static float	pos4[MAXSTUDIOBONES][3];
	static vec4_t	q4[MAXSTUDIOBONES];

	ASSERT( e->model && e->model->extradata );

	// bones already cached for this frame
	if( e->m_nCachedFrameCount == r_framecount2 )
		return;

	RI.currententity = e;
	if( e->lerp->curstate.sequence >= m_pStudioHeader->numseq ) e->lerp->curstate.sequence = 0;
	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->lerp->curstate.sequence;

	f = R_StudioEstimateFrame( pseqdesc );

//	if( e->lerp->curstate.frame > f ) Msg( "%f %f\n", e->lerp->curstate.frame, f );

	pstudio = e->extradata;
	ASSERT( pstudio );

	panim = R_StudioGetAnim( e->model, pseqdesc );
	R_StudioCalcRotations( pos, q, pseqdesc, panim, f );

	if( pseqdesc->numblends > 1 )
	{
		float	s;
		float	dadt;

		panim += m_pStudioHeader->numbones;
		R_StudioCalcRotations( pos2, q2, pseqdesc, panim, f );

		dadt = R_StudioEstimateInterpolant();
		s = (pstudio->lerp->curstate.blending[0] * dadt + pstudio->lerp->latched.prevblending[0] * (1.0 - dadt)) / 255.0;

		R_StudioSlerpBones( q, pos, q2, pos2, s );

		if( pseqdesc->numblends == 4 )
		{
			panim += m_pStudioHeader->numbones;
			R_StudioCalcRotations( pos3, q3, pseqdesc, panim, f );

			panim += m_pStudioHeader->numbones;
			R_StudioCalcRotations( pos4, q4, pseqdesc, panim, f );

			s = (pstudio->lerp->curstate.blending[0] * dadt + pstudio->lerp->latched.prevblending[0] * (1.0 - dadt)) / 255.0;
			R_StudioSlerpBones( q3, pos3, q4, pos4, s );

			s = (pstudio->lerp->curstate.blending[1] * dadt + pstudio->lerp->latched.prevblending[1] * (1.0 - dadt)) / 255.0;
			R_StudioSlerpBones( q, pos, q3, pos3, s );
		}
	}

	if( m_fDoInterp && pstudio->lerp->latched.sequencetime && ( pstudio->lerp->latched.sequencetime + 0.2 > RI.refdef.time) && ( pstudio->lerp->latched.prevsequence < m_pStudioHeader->numseq ))
	{
		// blend from last sequence
		static float  pos1b[MAXSTUDIOBONES][3];
		static vec4_t q1b[MAXSTUDIOBONES];
		float s;

		pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + pstudio->lerp->latched.prevsequence;
		panim = R_StudioGetAnim( e->model, pseqdesc );

		// clip prevframe
		R_StudioCalcRotations( pos1b, q1b, pseqdesc, panim, pstudio->lerp->latched.prevframe );

		if( pseqdesc->numblends > 1 )
		{
			panim += m_pStudioHeader->numbones;
			R_StudioCalcRotations( pos2, q2, pseqdesc, panim, pstudio->lerp->latched.prevframe );

			s = (pstudio->lerp->latched.prevseqblending[0]) / 255.0;
			R_StudioSlerpBones( q1b, pos1b, q2, pos2, s );

			if( pseqdesc->numblends == 4 )
			{
				panim += m_pStudioHeader->numbones;
				R_StudioCalcRotations( pos3, q3, pseqdesc, panim, pstudio->lerp->latched.prevframe );

				panim += m_pStudioHeader->numbones;
				R_StudioCalcRotations( pos4, q4, pseqdesc, panim, pstudio->lerp->latched.prevframe );

				s = (pstudio->lerp->latched.prevseqblending[0]) / 255.0;
				R_StudioSlerpBones( q3, pos3, q4, pos4, s );

				s = (pstudio->lerp->latched.prevseqblending[1]) / 255.0;
				R_StudioSlerpBones( q1b, pos1b, q3, pos3, s );
			}
		}

		s = 1.0f - ( RI.refdef.time - pstudio->lerp->latched.sequencetime ) / 0.2f;
		R_StudioSlerpBones( q, pos, q1b, pos1b, s );
	}
	else
	{
		// store prevframe otherwise
		//Msg( "prevframe = %4.2f\n", f );
		pstudio->lerp->latched.prevframe = f;
	}

	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	// calc gait animation
	if( m_pPlayerInfo && m_pPlayerInfo->gaitsequence != 0 )
	{
		if( m_pPlayerInfo->gaitsequence >= m_pStudioHeader->numseq ) 
			m_pPlayerInfo->gaitsequence = 0;

		pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pPlayerInfo->gaitsequence;

		panim = R_StudioGetAnim( e->model, pseqdesc );
		R_StudioCalcRotations( pos2, q2, pseqdesc, panim, m_pPlayerInfo->gaitframe );

		for( i = 0; i < m_pStudioHeader->numbones; i++ )
		{
			// g-cont. hey, what a hell ?
			if( !com.strcmp( pbones[i].name, "Bip01 Spine" ))
				break;
			Mem_Copy( pos[i], pos2[i], sizeof( pos[i] ));
			Mem_Copy( q[i], q2[i], sizeof( q[i] ));
		}
	}

	for( i = 0; i < m_pStudioHeader->numbones; i++ ) 
	{
		Matrix4x4_FromOriginQuat( bonematrix, pos[i][0], pos[i][1], pos[i][2], q[i][0], q[i][1], q[i][2], q[i][3] );
		if( pbones[i].parent == -1 ) 
		{
			Matrix4x4_ConcatTransforms( m_pbonestransform[i], m_protationmatrix, bonematrix );

			// apply client-side effects to the transformation matrix
			ri.StudioFxTransform( e->lerp, m_pbonestransform[i] );
		} 
		else Matrix4x4_ConcatTransforms( m_pbonestransform[i], m_pbonestransform[pbones[i].parent], bonematrix );
	}
}

/*
====================
StudioSaveBones

====================
*/
static void R_StudioSaveBones( ref_entity_t *e )
{
	mstudiobone_t	*pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	int		i;

	for( i = 0; i < m_pStudioHeader->numbones; i++ ) 
		com.strncpy( ((studiovars_t *)e->extradata)->bonenames[i], pbones[i].name, 32 );
}

/*
====================
StudioMergeBones

====================
*/
void R_StudioMergeBones( ref_entity_t *e, ref_model_t *m_pSubModel )
{
	int		i, j;
	double		f;
	int		sequence = e->lerp->curstate.sequence;
	mstudiobone_t	*pbones;
	mstudioseqdesc_t	*pseqdesc;
	mstudioanim_t	*panim;
	studiovars_t	*pstudio;
	matrix4x4		bonematrix;
	static vec4_t	q[MAXSTUDIOBONES];
	static float	pos[MAXSTUDIOBONES][3];
	static matrix4x4	localbones[MAXSTUDIOBONES];
	cl_entity_t	*cl_entity;

	ASSERT( e->parent );
	cl_entity = e->parent->lerp;	// FIXME: rename lerp to cl_ent
	pstudio = ((studiovars_t *)e->extradata);

	ASSERT( pstudio );
	Mem_Copy( localbones, pstudio->bonestransform, sizeof( matrix4x4 ) * pstudio->numbones );
		
	pstudio = ((studiovars_t *)e->parent->extradata);

	// weaponmodel can't change e->sequence!
	if( sequence >= m_pStudioHeader->numseq ) sequence = 0;
	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + sequence;

	f = R_StudioEstimateFrame( pseqdesc );

//	if( e->lerp->curstate.frame > f ) Msg( "%f %f\n", e->lerp->latched.prevframe, f );

	panim = R_StudioGetAnim( m_pSubModel, pseqdesc );
	R_StudioCalcRotations( pos, q, pseqdesc, panim, f );
	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	for( i = 0; i < m_pStudioHeader->numbones; i++ ) 
	{
		for( j = 0; j < pstudio->numbones; j++ )
		{
			if( !com.stricmp( pbones[i].name, pstudio->bonenames[j] ))
			{
				Matrix4x4_Copy( localbones[i], pstudio->bonestransform[j] );
				break;
			}
		}
		if( j >= pstudio->numbones )
		{
			Matrix4x4_FromOriginQuat( bonematrix, pos[i][0], pos[i][1], pos[i][2], q[i][0], q[i][1], q[i][2], q[i][3] );
			if( pbones[i].parent == -1 ) 
			{
				Matrix4x4_ConcatTransforms( localbones[i], m_protationmatrix, bonematrix );

				// apply client-side effects to the transformation matrix
				if( cl_entity ) ri.StudioFxTransform( cl_entity, m_pbonestransform[i] );
			} 
			else Matrix4x4_ConcatTransforms( localbones[i], m_pbonestransform[pbones[i].parent], bonematrix );
		}
	}

	pstudio = ((studiovars_t *)e->extradata);
	ASSERT( pstudio );

	// copy bones back to the merged entity
	Mem_Copy( pstudio->bonestransform, localbones, sizeof( matrix4x4 ) * m_pStudioHeader->numbones );
}


/*
====================
StudioCalcAttachments

====================
*/
static void R_StudioCalcAttachments( ref_entity_t *e )
{
	int		i;
	matrix4x4		out;
	mstudioattachment_t	*pAtt;
	vec3_t		axis[3];
	vec3_t		localOrg, localAng, bonepos;

	if( m_pStudioHeader->numattachments <= 0 )
	{
		// clear attachments
		for( i = 0; i < MAXSTUDIOATTACHMENTS; i++ )
		{
			VectorClear( e->lerp->attachment[i] );
			VectorClear( e->lerp->attachment_angles[i] );
		}
		return;
	}
	else if( m_pStudioHeader->numattachments > MAXSTUDIOATTACHMENTS )
	{
		m_pStudioHeader->numattachments = MAXSTUDIOATTACHMENTS; // reduce it
		MsgDev( D_WARN, "R_StudioCalcAttahments: too many attachments on %s\n", e->model->name );
	}

	// calculate attachment points
	pAtt = (mstudioattachment_t *)((byte *)m_pStudioHeader + m_pStudioHeader->attachmentindex);
	for( i = 0; i < m_pStudioHeader->numattachments; i++ )
	{
		// NOTE: m_pbonestransform not contained rotate component so we need do it here
		Matrix4x4_Concat( out, ((studiovars_t *)e->extradata)->rotationmatrix, m_pbonestransform[pAtt[i].bone] );

		// compute pos and angles
		Matrix4x4_VectorTransform( out, pAtt[i].org, localOrg );
		Matrix4x4_OriginFromMatrix( out, bonepos );
		VectorSubtract( localOrg, bonepos, axis[0] );	// make forward
		VectorNormalizeFast( axis[0] );
		VectorVectors( axis[0], axis[1], axis[2] );	// make right and up

		Matrix3x3_ToAngles( axis, localAng, false );	// FIXME: dll's uses FLU ?

		VectorCopy( localOrg, e->lerp->attachment[i] );
		VectorCopy( localAng, e->lerp->attachment_angles[i] );
	}
}

qboolean R_StudioComputeBBox( vec3_t bbox[8] )
{
	vec3_t		tmp_mins, tmp_maxs;
	ref_entity_t	*e = RI.currententity;
	vec3_t		vectors[3], angles, p1, p2;
	int		i, seq = e->lerp->curstate.sequence;

	if( !R_StudioExtractBbox( m_pStudioHeader, seq, tmp_mins, tmp_maxs ))
		return false;

	// copy original bbox
	VectorCopy( m_pStudioHeader->bbmin, studio_mins );
	VectorCopy( m_pStudioHeader->bbmin, studio_mins );

	// rotate the bounding box
	VectorScale( e->angles, -1, angles );

	if( e->ent_type == ET_PLAYER )
		angles[PITCH] = 0; // don't rotate player model, only aim
	AngleVectorsFLU( angles, vectors[0], vectors[1], vectors[2] );

	// compute a full bounding box
	for( i = 0; i < 8; i++ )
	{
  		p1[0] = ( i & 1 ) ? tmp_mins[0] : tmp_maxs[0];
  		p1[1] = ( i & 2 ) ? tmp_mins[1] : tmp_maxs[1];
  		p1[2] = ( i & 4 ) ? tmp_mins[2] : tmp_maxs[2];

		p2[0] = DotProduct( p1, vectors[0] );
		p2[1] = DotProduct( p1, vectors[1] );
		p2[2] = DotProduct( p1, vectors[2] );

		if( bbox ) VectorCopy( p2, bbox[i] );

  		if( p2[0] < studio_mins[0] ) studio_mins[0] = p2[0];
  		if( p2[0] > studio_maxs[0] ) studio_maxs[0] = p2[0];
  		if( p2[1] < studio_mins[1] ) studio_mins[1] = p2[1];
  		if( p2[1] > studio_maxs[1] ) studio_maxs[1] = p2[1];
  		if( p2[2] < studio_mins[2] ) studio_mins[2] = p2[2];
  		if( p2[2] > studio_maxs[2] ) studio_maxs[2] = p2[2];
	}

	studio_radius = RadiusFromBounds( studio_mins, studio_maxs );

	return true;
}

/*
====================
StudioSetupSubModel

====================
*/
static void R_StudioSetupSubModel( int body, int bodypart )
{
	int	index;

	if( bodypart > m_pStudioHeader->numbodyparts ) bodypart = 0;
	m_pBodyPart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + bodypart;

	index = body / m_pBodyPart->base;
	index = index % m_pBodyPart->nummodels;

	m_pSubModel = (mstudiomodel_t *)((byte *)m_pStudioHeader + m_pBodyPart->modelindex) + index;
}

void R_StudioSetupChrome( float *pchrome, int modelnum, int bone, float *normal )
{
	float	n;

	if( m_pchromeage[bone] != ( r_framecount2 + modelnum ))
	{
		vec3_t	chromeupvec;		// g_chrome t vector in world reference frame
		vec3_t	chromerightvec;		// g_chrome s vector in world reference frame
		vec3_t	tmp;			// vector pointing at bone in world reference frame
		vec3_t	v_right = { 0, -1, 0 };	// inverse right vector
		
		// calculate vectors from the viewer to the bone. This roughly adjusts for position
		Matrix4x4_OriginFromMatrix( m_pbonestransform[bone], tmp );
		VectorNormalize( tmp );
		CrossProduct( tmp, v_right, chromeupvec );

		VectorNormalize( chromeupvec );
		CrossProduct( tmp, chromeupvec, chromerightvec );
		VectorNormalize( chromerightvec );

		Matrix4x4_VectorIRotate( m_pbonestransform[bone], chromeupvec, m_pchromeup[bone] );
		Matrix4x4_VectorIRotate( m_pbonestransform[bone], chromerightvec, m_pchromeright[bone] );
		m_pchromeage[bone] = (r_framecount2+modelnum);
	}

	// calc s coord
	n = DotProduct( normal, m_pchromeright[bone] );
	pchrome[0] = (n + 1.0f) * 32.0f;

	// calc t coord
	n = DotProduct( normal, m_pchromeup[bone] );
	pchrome[1] = (n + 1.0f) * 32.0f;
}

void R_StudioDrawMesh( const meshbuffer_t *mb, short *ptricmds, float s, float t, int features, int flags )
{
	int		i;
	float		*av, *lv;
	ref_shader_t	*shader;

	MB_NUM2SHADER( mb->shaderkey, shader );

	while( i = *( ptricmds++ ))
	{
		int	vertexState = 0;
		qboolean	tri_strip;
		
		if( i < 0 )
		{
			i = -i;
			tri_strip = false;
		}
		else tri_strip = true;
			
		for( ; i > 0; i--, ptricmds += 4 )
		{
			if( r_backacc.numVerts + 256 > MAX_ARRAY_VERTS || r_backacc.numElems + 784 > MAX_ARRAY_ELEMENTS )
			{
 				MsgDev( D_ERROR, "StudioDrawMesh: %s has too many vertices per submodel ( %i )\n", RI.currentmodel->name, r_backacc.numVerts );
 				goto render_now;				
			}

			if( vertexState++ < 3 )
			{
				inElemsArray[r_backacc.numElems++] = r_backacc.numVerts;
			}
			else if( tri_strip )
			{
				// flip triangles between clockwise and counter clockwise
				if( vertexState & 1 )
				{
					// draw triangle [n-2 n-1 n]
					inElemsArray[r_backacc.numElems++] = r_backacc.numVerts - 2;
					inElemsArray[r_backacc.numElems++] = r_backacc.numVerts - 1;
					inElemsArray[r_backacc.numElems++] = r_backacc.numVerts;
				}
				else
				{
					// draw triangle [n-1 n-2 n]
					inElemsArray[r_backacc.numElems++] = r_backacc.numVerts - 1;
					inElemsArray[r_backacc.numElems++] = r_backacc.numVerts - 2;
					inElemsArray[r_backacc.numElems++] = r_backacc.numVerts;
				}
			}
			else
			{
				// draw triangle fan [0 n-1 n]
				inElemsArray[r_backacc.numElems++] = r_backacc.numVerts - ( vertexState - 1 );
				inElemsArray[r_backacc.numElems++] = r_backacc.numVerts - 1;
				inElemsArray[r_backacc.numElems++] = r_backacc.numVerts;
			}

			if( flags & STUDIO_NF_CHROME )
				Vector2Set( inCoordsArray[r_backacc.numVerts], m_pchrome[ptricmds[1]][0] * s, m_pchrome[ptricmds[1]][1] * t );
			else Vector2Set( inCoordsArray[r_backacc.numVerts], ptricmds[2] * s, ptricmds[3] * t );

			if( features & MF_NORMALS )
			{
				lv = m_pxformnorms[ptricmds[1]]; // normals
				Vector4Set( inNormalsArray[r_backacc.numVerts], lv[0], lv[1], lv[2], 1.0f );
			}

			av = m_pxformverts[ptricmds[0]]; // verts
			Vector4Set( inVertsArray[r_backacc.numVerts], av[0], av[1], av[2], 1.0f );
			r_backacc.c_totalVerts++;	// r_speeds issues
			r_backacc.numVerts++;
		}
	}
render_now:
	if( features & MF_SVECTORS )
		R_BuildTangentVectors( r_backacc.numVerts, inVertsArray, inNormalsArray, inCoordsArray, r_backacc.numElems / 3, inElemsArray, inSVectorsArray );
	
	R_StudioSetUpTransform( RI.currententity, true );
	r_features = features;
	R_RenderMeshBuffer( mb );
}

void R_StudioDrawBones( void )
{
	mstudiobone_t	*pbones = (mstudiobone_t *) ((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	vec3_t		point;
	int		i;

	for( i = 0; i < m_pStudioHeader->numbones; i++ )
	{
		if( pbones[i].parent >= 0 )
		{
			pglPointSize( 3.0f );
			pglColor3f( 1, 0.7f, 0 );
			pglBegin( GL_LINES );
			
			Matrix4x4_OriginFromMatrix( m_pbonestransform[pbones[i].parent], point );
			pglVertex3fv( point );
			Matrix4x4_OriginFromMatrix( m_pbonestransform[i], point );
			pglVertex3fv( point );
			
			pglEnd();

			pglColor3f( 0, 0, 0.8f );
			pglBegin( GL_POINTS );
			if( pbones[pbones[i].parent].parent != -1 )
			{
				Matrix4x4_OriginFromMatrix( m_pbonestransform[pbones[i].parent], point );
				pglVertex3fv( point );
			}
			Matrix4x4_OriginFromMatrix( m_pbonestransform[i], point );
			pglVertex3fv( point );
			pglEnd();
		}
		else
		{
			// draw parent bone node
			pglPointSize( 5.0f );
			pglColor3f( 0.8f, 0, 0 );
			pglBegin( GL_POINTS );
			Matrix4x4_OriginFromMatrix( m_pbonestransform[i], point );
			pglVertex3fv( point );
			pglEnd();
		}
	}
	pglPointSize( 1.0f );
}

void R_StudioDrawHitboxes( int iHitbox )
{
	int	i, j;

	pglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

	for( i = 0; i < m_pStudioHeader->numhitboxes; i++ )
	{
		mstudiobbox_t	*pbboxes = (mstudiobbox_t *)((byte *)m_pStudioHeader + m_pStudioHeader->hitboxindex);
		vec3_t		v[8], v2[8], bbmin, bbmax;

		if( iHitbox >= 0 && iHitbox != i )
			continue;

		VectorCopy( pbboxes[i].bbmin, bbmin );
		VectorCopy( pbboxes[i].bbmax, bbmax );

		v[0][0] = bbmin[0];
		v[0][1] = bbmax[1];
		v[0][2] = bbmin[2];

		v[1][0] = bbmin[0];
		v[1][1] = bbmin[1];
		v[1][2] = bbmin[2];

		v[2][0] = bbmax[0];
		v[2][1] = bbmax[1];
		v[2][2] = bbmin[2];

		v[3][0] = bbmax[0];
		v[3][1] = bbmin[1];
		v[3][2] = bbmin[2];

		v[4][0] = bbmax[0];
		v[4][1] = bbmax[1];
		v[4][2] = bbmax[2];

		v[5][0] = bbmax[0];
		v[5][1] = bbmin[1];
		v[5][2] = bbmax[2];

		v[6][0] = bbmin[0];
		v[6][1] = bbmax[1];
		v[6][2] = bbmax[2];

		v[7][0] = bbmin[0];
		v[7][1] = bbmin[1];
		v[7][2] = bbmax[2];

		Matrix4x4_VectorTransform( m_pbonestransform[pbboxes[i].bone], v[0], v2[0] );
		Matrix4x4_VectorTransform( m_pbonestransform[pbboxes[i].bone], v[1], v2[1] );
		Matrix4x4_VectorTransform( m_pbonestransform[pbboxes[i].bone], v[2], v2[2] );
		Matrix4x4_VectorTransform( m_pbonestransform[pbboxes[i].bone], v[3], v2[3] );
		Matrix4x4_VectorTransform( m_pbonestransform[pbboxes[i].bone], v[4], v2[4] );
		Matrix4x4_VectorTransform( m_pbonestransform[pbboxes[i].bone], v[5], v2[5] );
		Matrix4x4_VectorTransform( m_pbonestransform[pbboxes[i].bone], v[6], v2[6] );
		Matrix4x4_VectorTransform( m_pbonestransform[pbboxes[i].bone], v[7], v2[7] );

		if( iHitbox >= 0 ) j = 0;
		else j = (pbboxes[i].group % 8);

		// set properly color for hull
		pglColor4f( hullcolor[j][0], hullcolor[j][1], hullcolor[j][2], 1.0f );

		pglBegin( GL_QUAD_STRIP );
		for( j = 0; j < 10; j++ )
			pglVertex3fv( v2[j & 7] );
		pglEnd( );
	
		pglBegin( GL_QUAD_STRIP );
		pglVertex3fv( v2[6] );
		pglVertex3fv( v2[0] );
		pglVertex3fv( v2[4] );
		pglVertex3fv( v2[2] );
		pglEnd( );

		pglBegin( GL_QUAD_STRIP );
		pglVertex3fv( v2[1] );
		pglVertex3fv( v2[7] );
		pglVertex3fv( v2[3] );
		pglVertex3fv( v2[5] );
		pglEnd( );			
	}

	pglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
}

void R_StudioDrawAttachments( void )
{
	int	i;
	
	for( i = 0; i < m_pStudioHeader->numattachments; i++ )
	{
		mstudioattachment_t	*pattachments = (mstudioattachment_t *)((byte *)m_pStudioHeader + m_pStudioHeader->attachmentindex);
		vec3_t		v[4];
		
		Matrix4x4_VectorTransform( m_pbonestransform[pattachments[i].bone], pattachments[i].org, v[0] );
		Matrix4x4_VectorTransform( m_pbonestransform[pattachments[i].bone], pattachments[i].vectors[0], v[1] );
		Matrix4x4_VectorTransform( m_pbonestransform[pattachments[i].bone], pattachments[i].vectors[1], v[2] );
		Matrix4x4_VectorTransform( m_pbonestransform[pattachments[i].bone], pattachments[i].vectors[2], v[3] );
		
		pglBegin( GL_LINES );
		pglColor3f( 1, 0, 0 );
		pglVertex3fv( v[0] );
		pglColor3f( 1, 1, 1 );
		pglVertex3fv( v[1] );
		pglColor3f( 1, 0, 0 );
		pglVertex3fv( v[0] );
		pglColor3f( 1, 1, 1 );
		pglVertex3fv( v[2] );
		pglColor3f( 1, 0, 0 );
		pglVertex3fv( v[0] );
		pglColor3f( 1, 1, 1 );
		pglVertex3fv( v[3] );
		pglEnd( );

		pglPointSize( 5.0f );
		pglColor3f( 0, 1, 0 );
		pglBegin( GL_POINTS );
		pglVertex3fv( v[0] );
		pglEnd( );
		pglPointSize( 1.0f );
	}
}

void R_StudioDrawHulls( void )
{
	int	i;
	vec3_t	bbox[8];

	// looks ugly, skip
	if( RI.currententity->ent_type == ET_VIEWENTITY )
		return;

	if(!R_StudioComputeBBox( bbox )) return;

	pglColor4f( 1.0f, 0.0f, 0.0f, 1.0f );	// red bboxes for studiomodels
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
====================
StudioDrawDebug

====================
*/
void R_StudioDrawDebug( void )
{
	R_StudioSetupRender( RI.currententity, RI.currententity->model );
	R_StudioSetUpTransform( RI.currententity, true );
		
	switch( r_drawentities->integer )
	{
	case 2: R_StudioDrawBones(); break;
	case 3: R_StudioDrawHitboxes(-1 ); break;
	case 4: R_StudioDrawAttachments(); break;
	case 5: R_StudioDrawHulls(); break;
	}
}

/*
====================
StudioDrawHitbox

render a selected hitbox
====================
*/
void R_StudioDrawHitbox( ref_entity_t *e, int iHitbox )
{
	R_StudioSetupRender( e, e->model );
	R_StudioSetUpTransform( e, true );
	R_StudioDrawHitboxes( iHitbox );
}


/*
====================
StudioEstimateGait

====================
*/
void R_StudioEstimateGait( ref_entity_t *e, entity_state_t *pplayer )
{
	float		dt;
	vec3_t		est_velocity;
	studiovars_t	*pstudio = (studiovars_t *)e->extradata;

	ASSERT( pstudio );	
	dt = bound( 0.0f, RI.refdef.frametime, 1.0f );

	if( dt == 0 || m_pPlayerInfo->renderframe == r_framecount2 )
	{
		m_flGaitMovement = 0;
		return;
	}

	// VectorAdd( pplayer->v.velocity, pplayer->v.prediction_error, est_velocity );
	if( m_fGaitEstimation )
	{
		VectorSubtract( e->origin, m_pPlayerInfo->prevgaitorigin, est_velocity );
		VectorCopy( e->origin, m_pPlayerInfo->prevgaitorigin );

		m_flGaitMovement = VectorLength( est_velocity );
		if( dt <= 0 || m_flGaitMovement / dt < 5 )
		{
			m_flGaitMovement = 0;
			est_velocity[0] = 0;
			est_velocity[1] = 0;
		}
	}
	else
	{
		VectorCopy( pplayer->velocity, est_velocity );
		m_flGaitMovement = VectorLength( est_velocity ) * dt;
	}

	if( est_velocity[1] == 0 && est_velocity[0] == 0 )
	{
		float flYawDiff = e->angles[YAW] - m_pPlayerInfo->gaityaw;

		flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;
		if( flYawDiff > 180 ) flYawDiff -= 360;
		if( flYawDiff < -180 ) flYawDiff += 360;

		if( dt < 0.25f ) flYawDiff *= dt * 4;
		else flYawDiff *= dt;

		m_pPlayerInfo->gaityaw += flYawDiff;
		m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw - (int)(m_pPlayerInfo->gaityaw / 360) * 360;
		m_flGaitMovement = 0;
	}
	else
	{
		m_pPlayerInfo->gaityaw = (atan2(est_velocity[1], est_velocity[0]) * 180 / M_PI);
		if( m_pPlayerInfo->gaityaw > 180 ) m_pPlayerInfo->gaityaw = 180;
		if( m_pPlayerInfo->gaityaw < -180 ) m_pPlayerInfo->gaityaw = -180;
	}

}

/*
====================
StudioProcessGait

====================
*/
void R_StudioProcessGait( ref_entity_t *e, entity_state_t *pplayer, studiovars_t *pstudio )
{
	mstudioseqdesc_t	*pseqdesc;
	float		dt, flYaw;	// view direction relative to movement
	int		iBlend;

	if( e->lerp->curstate.sequence >= m_pStudioHeader->numseq ) 
		e->lerp->curstate.sequence = 0;

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->lerp->curstate.sequence;
	R_StudioPlayerBlend( pseqdesc, &iBlend, &e->lerp->angles[PITCH] );

	pstudio->lerp->latched.prevangles[PITCH] = e->lerp->angles[PITCH];
	pstudio->lerp->curstate.blending[0] = iBlend;
	pstudio->lerp->latched.prevblending[0] = pstudio->lerp->curstate.blending[0];
	pstudio->lerp->latched.prevseqblending[0] = pstudio->lerp->curstate.blending[0];

	//Msg( "%f %d\n", e->angles[PITCH], pstudio->lerp->curstate.blending[0] );

	dt = bound( 0.0f, RI.refdef.frametime, 1.0f );

	R_StudioEstimateGait( e, pplayer );

	//Msg( "%f %f\n", e->angles[YAW], pstudio->lerp->gaityaw );

	// calc side to side turning
	flYaw = e->angles[YAW] - m_pPlayerInfo->gaityaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;
	if( flYaw < -180 ) flYaw = flYaw + 360;
	if( flYaw > 180 ) flYaw = flYaw - 360;

	if( flYaw > 120 )
	{
		m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw - 180;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw = flYaw - 180;
	}
	else if( flYaw < -120 )
	{
		m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw + 180;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw = flYaw + 180;
	}

	// adjust torso
	pstudio->lerp->curstate.controller[0] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	pstudio->lerp->curstate.controller[1] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	pstudio->lerp->curstate.controller[2] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	pstudio->lerp->curstate.controller[3] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	pstudio->lerp->latched.prevcontroller[0] = pstudio->lerp->curstate.controller[0];
	pstudio->lerp->latched.prevcontroller[1] = pstudio->lerp->curstate.controller[1];
	pstudio->lerp->latched.prevcontroller[2] = pstudio->lerp->curstate.controller[2];
	pstudio->lerp->latched.prevcontroller[3] = pstudio->lerp->curstate.controller[3];

	e->lerp->angles[YAW] = m_pPlayerInfo->gaityaw;
	if( e->lerp->angles[YAW] < -0 ) e->angles[YAW] += 360;
	e->lerp->latched.prevangles[YAW] = e->lerp->angles[YAW];

	if( pplayer->gaitsequence >= m_pStudioHeader->numseq ) 
		pplayer->gaitsequence = 0;

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + pplayer->gaitsequence;

	// calc gait frame
	if( pseqdesc->linearmovement[0] > 0 )
	{
		m_pPlayerInfo->gaitframe += (m_flGaitMovement / pseqdesc->linearmovement[0]) * pseqdesc->numframes;
	}
	else
	{
		m_pPlayerInfo->gaitframe += pseqdesc->fps * dt;
	}

	// do modulo
	m_pPlayerInfo->gaitframe = m_pPlayerInfo->gaitframe - (int)(m_pPlayerInfo->gaitframe / pseqdesc->numframes) * pseqdesc->numframes;
	if( m_pPlayerInfo->gaitframe < 0 ) m_pPlayerInfo->gaitframe += pseqdesc->numframes;
}

static qboolean R_StudioSetupModel( ref_entity_t *e, ref_model_t *mod )
{
	studiovars_t	*pstudio = e->extradata;
	int		i, m_nPlayerIndex;

	if( e->m_nCachedFrameCount == r_framecount2 )
		return 1;

	ASSERT( pstudio );

	// special handle for player model
	if( e->ent_type == ET_PLAYER || e->renderfx == kRenderFxDeadPlayer )
	{
		//Msg( "DrawPlayer %d\n", pstudio->lerp->blending[0] );
		//Msg( "DrawPlayer %d %d (%d)\n", r_framecount2, m_pEntity->serialnumber, e->lerp->curstate.sequence );
		//Msg( "Player %.2f %.2f %.2f\n", m_pEntity->v.velocity[0], m_pEntity->v.velocity[1], m_pEntity->v.velocity[2] );

		if( e->renderfx == kRenderFxDeadPlayer )
		{
			// prepare to draw dead player
			if( m_pPlayerInfo ) m_pPlayerInfo->gaitsequence = 0;
		}

		if( !m_pPlayerInfo ) return 0;
		m_nPlayerIndex = e->index - 1;

		if( m_nPlayerIndex < 0 || m_nPlayerIndex >= ri.GetMaxClients( ))
			return 0;	// weird client ?

		if( e->gaitsequence )
		{
			vec3_t	orig_angles;

			VectorCopy( e->angles, orig_angles );

			R_StudioProcessGait( e, &e->lerp->curstate, pstudio );
			m_pPlayerInfo->gaitsequence = e->gaitsequence;
//			m_pPlayerInfo = NULL;
		
			R_StudioSetUpTransform ( e, false );
			VectorCopy( orig_angles, e->angles );
		}
		else
		{
			for( i = 0; i < 4; i++ )		// clear torso controllers
				pstudio->lerp->latched.prevcontroller[i] = pstudio->lerp->curstate.controller[i] = 0x7F;
			m_pPlayerInfo->gaitsequence = 0;	// StudioSetupBones() issuses

			R_StudioSetUpTransform ( e, false );
		}


		if( r_himodels->integer ) e->body = 255; // show highest resolution multiplayer model
		if( !( glw_state.developer == 0 && ri.GetMaxClients() == 1 )) e->body = 1; // force helmet

	}
	else R_StudioSetUpTransform ( e, false );

	if( e->movetype == MOVETYPE_FOLLOW && e->parent )
	{
		R_StudioMergeBones( e, mod );
	}
	else
	{
		R_StudioSetupBones( e );	
		R_StudioSaveBones( e );
	}

	if( e->movetype != MOVETYPE_FOLLOW && !RI.refdef.paused && e->m_nCachedFrameCount != r_framecount2 )
	{
		R_StudioCalcAttachments( e );
		R_StudioProcessEvents( e, e->lerp );
	}

	e->m_nCachedFrameCount = r_framecount2;	// cached frame
	if( m_pPlayerInfo ) m_pPlayerInfo->renderframe = r_framecount2;

	return 1;
}

void R_StudioDrawPoints( const meshbuffer_t *mb, ref_entity_t *e )
{
	int		i, j, m_skinnum = RI.currententity->skin;
	int		modelnum = ((-mb->infokey - 1) & 0xFF);
	int		meshnum = (((-mb->infokey - 1) & 0xFF00)>>8);
	ref_model_t	*model = Mod_ForHandle( mb->modhandle );
	vec3_t		*pstudioverts, *pstudionorms;
	byte		*pvertbone, *pnormbone;
	short		*pskinref, *ptricmds;
	int		flags, features;
	mstudiotexture_t	*ptexture;
	studiovars_t	*studio;
	mstudiomesh_t	*pmesh;
	ref_shader_t	*shader;
	float		s, t;

	MB_NUM2SHADER( mb->shaderkey, shader );

	R_StudioSetupRender( e, model );
	R_StudioSetupSubModel( RI.currententity->body, modelnum );

	if( meshnum > m_pSubModel->nummesh ) return; // invalid mesh

	// setup all mesh pointers
	pstudioverts = (vec3_t *)((byte *)m_pStudioHeader + m_pSubModel->vertindex);
	pstudionorms = (vec3_t *)((byte *)m_pStudioHeader + m_pSubModel->normindex);
	pvertbone = ((byte *)m_pStudioHeader + m_pSubModel->vertinfoindex);
	pnormbone = ((byte *)m_pStudioHeader + m_pSubModel->norminfoindex);
	pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + meshnum;
	ptexture = (mstudiotexture_t *)((byte *)m_pTextureHeader + m_pTextureHeader->textureindex);
	pskinref = (short *)((byte *)m_pTextureHeader + m_pTextureHeader->skinindex);
	ptricmds = (short *)((byte *)m_pStudioHeader + pmesh->triindex);
	if( m_skinnum != 0 && m_skinnum < m_pTextureHeader->numskinfamilies )
		pskinref += (m_skinnum * m_pTextureHeader->numskinref);
	s = 1.0f / (float)ptexture[pskinref[pmesh->skinref]].width;
	t = 1.0f / (float)ptexture[pskinref[pmesh->skinref]].height;
	flags = ptexture[pskinref[pmesh->skinref]].flags;
	studio = (studiovars_t *)e->extradata;

	// detect mesh features
	features = MF_NONBATCHED | shader->features;
	if( RI.params & RP_SHADOWMAPVIEW )
	{
		features &= ~( MF_COLORS|MF_SVECTORS|MF_ENABLENORMALS );
		if(!( shader->features & MF_DEFORMVS ))
			features &= ~MF_NORMALS;
	}
	else
	{
		if(( features & MF_SVECTORS ) || r_shownormals->integer )
			features |= MF_NORMALS;
	}

	ASSERT( studio );
	
	// initialize vertex cache
	if( !studio->mesh[modelnum] )
		studio->mesh[modelnum] = Mem_Alloc( e->mempool, sizeof( studioverts_t ));		
				
	if( studio->mesh[modelnum]->numverts != m_pSubModel->numverts )
		studio->mesh[modelnum]->verts = Mem_Realloc( e->mempool, studio->mesh[modelnum]->verts, sizeof( vec3_t ) * m_pSubModel->numverts );

	if( studio->mesh[modelnum]->numnorms != m_pSubModel->numnorms )
	{
		studio->mesh[modelnum]->norms = Mem_Realloc( e->mempool, studio->mesh[modelnum]->norms, sizeof( vec3_t ) * m_pSubModel->numnorms );
		studio->mesh[modelnum]->chrome = Mem_Realloc( e->mempool, studio->mesh[modelnum]->chrome, sizeof( vec2_t ) * m_pSubModel->numnorms );
	}

	studio->mesh[modelnum]->numverts = m_pSubModel->numverts;
	studio->mesh[modelnum]->numnorms = m_pSubModel->numnorms;

	m_pxformverts = studio->mesh[modelnum]->verts;
	m_pxformnorms = studio->mesh[modelnum]->norms;
	m_pxformlight = studio->mesh[modelnum]->light;
	m_pchrome = studio->mesh[modelnum]->chrome;

	// cache transforms
	if( studio->mesh[modelnum]->m_nCachedFrame != r_framecount2 )
	{
		mstudiomesh_t	*pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex);
		float		*lv = (float *)m_pxformlight;
		int		vertspermesh = 0;

		for( i = 0; i < m_pSubModel->numverts; i++ )
			Matrix4x4_VectorTransform( m_pbonestransform[pvertbone[i]], pstudioverts[i], m_pxformverts[i] );

		for( i = 0; ( i < m_pSubModel->numnorms ) && ( features & MF_NORMALS ); i++ )
		{
			Matrix4x4_VectorRotate( m_pbonestransform[pnormbone[i]], pstudionorms[i], m_pxformnorms[i] );
		}

		if( m_pStudioHeader->flags & STUDIO_HAS_CHROME )
		{
			for( i = 0; i < m_pSubModel->nummesh; i++ ) 
			{
				int	texflags = ptexture[pskinref[pmesh[i].skinref]].flags;

				for( j = 0; j < pmesh[i].numnorms; j++, lv += 3, pstudionorms++, pnormbone++ )
				{
					if(!( texflags & STUDIO_NF_CHROME )) continue;
					R_StudioSetupChrome( m_pchrome[(float (*)[3])lv - m_pxformlight], modelnum, *pnormbone, (float *)pstudionorms );
				}
			}
		}
		studio->mesh[modelnum]->m_nCachedFrame = r_framecount2;
	}

	R_StudioDrawMesh( mb, ptricmds, s, t, features, flags );
}

void R_DrawStudioModel( const meshbuffer_t *mb )
{
	ref_entity_t	*e;
	int		meshnum = -mb->infokey - 1;

	MB_NUM2ENTITY( mb->sortkey, e );

	if( OCCLUSION_QUERIES_ENABLED( RI ) && OCCLUSION_TEST_ENTITY( e ))
	{
		ref_shader_t	*shader;

		MB_NUM2SHADER( mb->shaderkey, shader );
		if( !R_GetOcclusionQueryResultBool( shader->type == 1 ? OQ_PLANARSHADOW : OQ_ENTITY, e - r_entities, true ))
			return;
	}

	// hack the depth range to prevent view model from poking into walls
	if( e->ent_type == ET_VIEWENTITY )
	{
		pglDepthRange( gldepthmin, gldepthmin + 0.3 * ( gldepthmax - gldepthmin ) );

		// backface culling for left-handed weapons
		if( r_lefthand->integer == 1 ) GL_FrontFace( !glState.frontFace );
          }

	R_StudioDrawPoints( mb, e );

	if( e->ent_type == ET_VIEWENTITY )
	{
		pglDepthRange( gldepthmin, gldepthmax );

		// backface culling for left-handed weapons
		if( r_lefthand->integer == 1 ) GL_FrontFace( !glState.frontFace );
	}
}

qboolean R_CullStudioModel( ref_entity_t *e )
{
	int		i, j, clipped;
	int		sequence;
	qboolean		frustum, query;
	uint		modhandle;
	mstudiotexture_t	*ptexture;
	short		*pskinref;
	meshbuffer_t	*mb;

	if( !e->model->extradata )
		return true;

	if( e->ent_type == ET_VIEWENTITY && r_lefthand->integer >= 2 )
		return true; // hidden

	modhandle = Mod_Handle( e->model );

	if( RP_FOLLOWENTITY( e ))
	{
		// cull child entities with parent volume
		R_StudioSetupRender( e->parent, e->parent->model );
		sequence = e->parent->lerp->curstate.sequence;
	}
	else
	{
		R_StudioSetupRender( e, e->model );
		sequence = e->lerp->curstate.sequence;
	}

	if( !R_StudioComputeBBox( NULL ))
		return true; // invalid sequence

	clipped = R_CullModel( e, studio_mins, studio_maxs, studio_radius );
	frustum = clipped & 1;
	if( clipped & 2 ) return true;

	query = OCCLUSION_QUERIES_ENABLED( RI ) && OCCLUSION_TEST_ENTITY( e ) ? true : false;
	if( !frustum && query ) R_IssueOcclusionQuery( R_GetOcclusionQueryNum( OQ_ENTITY, e - r_entities ), e, studio_mins, studio_maxs );

	if(( RI.refdef.flags & RDF_NOWORLDMODEL ) || ( r_shadows->integer != 1 ) || R_CullPlanarShadow( e, studio_mins, studio_maxs, query ))
		return frustum; // entity is not in PVS or shadow is culled away by frustum culling

	R_StudioSetupRender( e, e->model );

	// setup bones, play events etc
	R_StudioSetupModel( e, e->model );

	// add it
	ptexture = (mstudiotexture_t *)((byte *)m_pTextureHeader + m_pTextureHeader->textureindex);
	for( i = 0; i < m_pStudioHeader->numbodyparts; i++ )
	{
		R_StudioSetupSubModel( RI.currententity->body, i );
		pskinref = (short *)((byte *)m_pTextureHeader + m_pTextureHeader->skinindex);
		if( e->skin != 0 && e->skin < m_pTextureHeader->numskinfamilies )
			pskinref += (e->skin * m_pTextureHeader->numskinref);

		for( j = 0; j < m_pSubModel->nummesh; j++ ) 
		{
			ref_shader_t	*shader;
			mstudiomesh_t	*pmesh;
	
			pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + j;
			if( e->customShader ) shader = e->customShader;
			else shader = &r_shaders[ptexture[pskinref[pmesh->skinref]].index];

			if( shader && ( shader->sort <= SORT_ALPHATEST ))
			{
				mb = R_AddMeshToList( MB_MODEL, NULL, R_PlanarShadowShader(), -(((j<<8)|i)+1 ));
				if( mb ) mb->modhandle = modhandle;
			}
		}
	}
	return frustum;
}

void R_AddStudioModelToList( ref_entity_t *e )
{
	uint		modhandle;
	uint		entnum = e - r_entities;
	ref_model_t	*mod = e->model;
	mfog_t		*fog = NULL;
	mstudiotexture_t	*ptexture;
	short		*pskinref;
	int		sequence;
	int		i, j;

	if( !e->model->extradata )
		return;

	if( RP_FOLLOWENTITY( e ))
	{
		// cull child entities with parent volume
		R_StudioSetupRender( e->parent, e->parent->model );
		sequence = e->parent->lerp->curstate.sequence;
	}
	else
	{
		R_StudioSetupRender( e, e->model );
		sequence = e->lerp->curstate.sequence;
	}

	if( !R_StudioComputeBBox( NULL ))
		return; // invalid sequence

	modhandle = Mod_Handle( mod );

	R_StudioSetupRender( e, e->model );
	if( m_pStudioHeader->numbodyparts == 0 ) return; // nothing to draw

	if( RI.params & RP_SHADOWMAPVIEW )
	{
		if( r_entShadowBits[entnum] & RI.shadowGroup->bit )
		{
			if( !r_shadows_self_shadow->integer )
				r_entShadowBits[entnum] &= ~RI.shadowGroup->bit;
			if( e->ent_type == ET_VIEWENTITY )
				return;
		}
		else
		{
			R_StudioModelLerpBBox( e, mod );
			if( !R_CullModel( e, studio_mins, studio_maxs, studio_radius ))
				r_entShadowBits[entnum] |= RI.shadowGroup->bit;
			return; // mark as shadowed, proceed with caster otherwise
		}
	}
	else
	{
		fog = R_FogForSphere( e->origin, studio_radius );
#if 0
		if( !( e->ent_type == ET_VIEWENTITY ) && fog )
		{
			R_StudioModelLerpBBox( e, mod );
			if( R_CompletelyFogged( fog, e->origin, studio_radius ))
				return;
		}
#endif
	}

	// setup bones, play events etc
	R_StudioSetupModel( e, e->model );

	// add base model
	ptexture = (mstudiotexture_t *)((byte *)m_pTextureHeader + m_pTextureHeader->textureindex);
	for( i = 0; i < m_pStudioHeader->numbodyparts; i++ )
	{
		R_StudioSetupSubModel( e->body, i );
		pskinref = (short *)((byte *)m_pTextureHeader + m_pTextureHeader->skinindex);
		if( e->skin != 0 && e->skin < m_pTextureHeader->numskinfamilies )
			pskinref += (e->skin * m_pTextureHeader->numskinref);

		for( j = 0; j < m_pSubModel->nummesh; j++ ) 
		{
			ref_shader_t	*shader;
			mstudiomesh_t	*pmesh;
	
			pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + j;

			if( e->customShader ) shader = e->customShader;
			else shader = &r_shaders[ptexture[pskinref[pmesh->skinref]].index];

			if( shader ) R_AddModelMeshToList( modhandle, fog, shader, ((j<<8)|i));
		}
	}
}

void R_StudioRunEvents( ref_entity_t *e )
{
	if( !e || !e->model || !e->model->extradata )
		return;

	if( RP_FOLLOWENTITY( e )) return;

	RI.currententity = e;
	R_StudioSetupRender( e, e->model );
	if( m_pStudioHeader->numbodyparts == 0 )
		return; // nothing to draw

	// setup bones, play events etc
	R_StudioSetupModel( e, e->model );
}