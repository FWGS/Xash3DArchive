//=======================================================================
//			Copyright XashXT Group 2010 ©
//		     r_decals.c - render decals on brushmodels
//=======================================================================

#include "r_local.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "clgame_api.h"
#include "effects_api.h"
#include "bspfile.h"
#include "const.h"

#define DECAL_DISTANCE		4	// FIXME: revert to 10 ?
#define MAX_DECALCLIPVERT		32	// produced vertexes of fragmented decal
#define DECAL_CACHEENTRY		256	// MUST BE POWER OF 2 or code below needs to change!

// empirically determined constants for minimizing overalpping decals
#define MAX_OVERLAP_DECALS		4
#define DECAL_OVERLAP_DIST		8

// FIXME: move this out to the r_math.c ?
typedef struct
{
	bool	(*pfnInside)( decalvert_t *pVert );
	float	(*pfnClip)( decalvert_t *one, decalvert_t *two );
} decal_clip_t;

static bool Top_Inside( decalvert_t *pVert ){ return pVert->m_tCoords[1] < 1.0f; }
static float Top_Clip( decalvert_t *one, decalvert_t *two ){ return ( 1.0f - one->m_tCoords[1] ) / ( two->m_tCoords[1] - one->m_tCoords[1] ); }
static bool Left_Inside( decalvert_t *pVert ){ return pVert->m_tCoords[0] > 0.0f; }
static float Left_Clip( decalvert_t *one, decalvert_t *two ){ return one->m_tCoords[0] / ( one->m_tCoords[0] - two->m_tCoords[0] ); }
static bool Right_Inside( decalvert_t *pVert ){ return pVert->m_tCoords[0] < 1.0f; }
static float Right_Clip( decalvert_t *one, decalvert_t *two ){ return ( 1.0f - one->m_tCoords[0] ) / ( two->m_tCoords[0] - one->m_tCoords[0] ); }
static bool Bottom_Inside( decalvert_t *pVert ){ return pVert->m_tCoords[1] > 0.0f; }
static float Bottom_Clip( decalvert_t *one, decalvert_t *two ){ return one->m_tCoords[1] / ( one->m_tCoords[1] - two->m_tCoords[1] ); }

// clippanes
static decal_clip_t PlaneTop = { Top_Inside, Top_Clip };
static decal_clip_t PlaneLeft = { Left_Inside, Left_Clip };
static decal_clip_t PlaneRight = { Right_Inside, Right_Clip };
static decal_clip_t PlaneBottom = { Bottom_Inside, Bottom_Clip };

static void Intersect( decal_clip_t clipFunc, decalvert_t *one, decalvert_t *two, decalvert_t *out )
{
	float	t = clipFunc.pfnClip( one, two );
	
	VectorLerp( one->m_vPos, t, two->m_vPos, out->m_vPos );
	Vector2Lerp( one->m_LMCoords, t, two->m_LMCoords, out->m_LMCoords );
	Vector2Lerp( one->m_tCoords, t, two->m_tCoords, out->m_tCoords );
}
		
// This structure contains the information used to create new decals
typedef struct
{
	vec3_t		m_Position;	// world coordinates of the decal center
	vec3_t		m_SAxis;		// the s axis for the decal in world coordinates
	ref_model_t*	m_pModel;		// the model the decal is going to be applied in
	ref_shader_t	*m_pShader;	// The decal material
	int		m_Size;		// Size of the decal (in world coords)
	int		m_Flags;
	int		m_Entity;		// Entity the decal is applied to.
	float		m_scale;
	int		m_decalWidth;
	int		m_decalHeight;
	rgba_t		m_Color;
	vec3_t		m_Basis[3];
} decalinfo_t;

typedef struct
{
	int		decalIndex;
	decalvert_t	decalVert[4];
} decalcache_t;

decalcache_t	gDecalCache[DECAL_CACHEENTRY];

// list of actual decals, that we needs to rendering
uint		r_numDecals;
decal_t		*r_drawdecals[MAX_DECALS];

static decalvert_t	g_DecalClipVerts[MAX_DECALCLIPVERT];
static decalvert_t	g_DecalClipVerts2[MAX_DECALCLIPVERT];
static mesh_t	decal_mesh;

static decal_t	gDecalPool[MAX_DECALS];
static int	gDecalCount;

static int R_DecalIndex( decal_t *pdecal )
{
	return ( pdecal - gDecalPool );
}

static int R_DecalCacheIndex( int index )
{
	return index & (DECAL_CACHEENTRY-1);
}

static decalcache_t *R_DecalCacheSlot( int decalIndex )
{
	int	cacheIndex;

	cacheIndex = R_DecalCacheIndex( decalIndex );	// find the cache slot

	return gDecalCache + cacheIndex;
}


// release the cache entry for this decal
static void R_DecalCacheClear( decal_t *pdecal )
{
	int				index;
	decalcache_t	*pCache;

	index = R_DecalIndex( pdecal );
	pCache = R_DecalCacheSlot( index );		// Find the cache slot

	if( pCache->decalIndex == index )		// If this is the decal that's cached here, clear it.
		pCache->decalIndex = -1;
}

// unlink pdecal from any surface it's attached to
static void R_DecalUnlink( decal_t *pdecal )
{
	decal_t	*tmp;

	R_DecalCacheClear( pdecal );
	if( pdecal->psurf )
	{
		if( pdecal->psurf->pdecals == pdecal )
		{
			pdecal->psurf->pdecals = pdecal->pnext;
		}
		else 
		{
			tmp = pdecal->psurf->pdecals;
			if( !tmp ) Host_Error( "D_DecalUnlink: bad decal list\n" );

			while( tmp->pnext ) 
			{
				if( tmp->pnext == pdecal ) 
				{
					tmp->pnext = pdecal->pnext;
					break;
				}
				tmp = tmp->pnext;
			}
		}
	}
	pdecal->psurf = NULL;
}


// Just reuse next decal in list
// A decal that spans multiple surfaces will use multiple decal_t pool entries, as each surface needs
// it's own.
static decal_t *R_DecalAlloc( decal_t *pdecal )
{
	int	limit = MAX_DECALS;

	if( r_decals->integer < limit )
		limit = r_decals->integer;
	
	if( !limit ) return NULL;

	if( !pdecal ) 
	{
		int	count = 0;

		// check for the odd possiblity of infinte loop
		do 
		{
			gDecalCount++;
			if( gDecalCount >= limit )
				gDecalCount = 0;
			pdecal = gDecalPool + gDecalCount; // reuse next decal
			count++;
		} while(( pdecal->flags & FDECAL_PERMANENT ) && count < limit );
	}
	
	// If decal is already linked to a surface, unlink it.
	R_DecalUnlink( pdecal );

	return pdecal;	
}

//-----------------------------------------------------------------------------
// compute the decal basis based on surface normal, and preferred saxis
//-----------------------------------------------------------------------------
void R_DecalComputeBasis( vec3_t surfaceNormal, vec3_t pSAxis, vec3_t textureSpaceBasis[3] )
{
	// get the surface normal.
	VectorCopy( surfaceNormal, textureSpaceBasis[2] );

	if( pSAxis )
	{
		// T = S cross N
		CrossProduct( pSAxis, textureSpaceBasis[2], textureSpaceBasis[1] );

		// Name sure they aren't parallel or antiparallel
		// In that case, fall back to the normal algorithm.
		if( DotProduct( textureSpaceBasis[1], textureSpaceBasis[1] ) > 1e-6 )
		{
			// S = N cross T
			CrossProduct( textureSpaceBasis[2], textureSpaceBasis[1], textureSpaceBasis[0] );

			VectorNormalizeFast( textureSpaceBasis[0] );
			VectorNormalizeFast( textureSpaceBasis[1] );
			return;
		}
		// Fall through to the standard algorithm for parallel or antiparallel
	}

	// floor/ceiling?
	if( fabs( surfaceNormal[2] ) > 0.70710678118654752440084436210485f )	// sin( 45 ) degrees
	{
		textureSpaceBasis[0][0] = 1.0f;
		textureSpaceBasis[0][1] = 0.0f;
		textureSpaceBasis[0][2] = 0.0f;

		// T = S cross N
		CrossProduct( textureSpaceBasis[0], textureSpaceBasis[2], textureSpaceBasis[1] );

		// S = N cross T
		CrossProduct( textureSpaceBasis[2], textureSpaceBasis[1], textureSpaceBasis[0] );
	}
	else	// wall
	{
		textureSpaceBasis[1][0] = 0.0f;
		textureSpaceBasis[1][1] = 0.0f;
		textureSpaceBasis[1][2] = -1.0f;

		// S = N cross T
		CrossProduct( textureSpaceBasis[2], textureSpaceBasis[1], textureSpaceBasis[0] );
		// T = S cross N
		CrossProduct( textureSpaceBasis[0], textureSpaceBasis[2], textureSpaceBasis[1] );
	}

	VectorNormalizeFast( textureSpaceBasis[0] );
	VectorNormalizeFast( textureSpaceBasis[1] );
}

void R_SetupDecalTextureSpaceBasis( decal_t *pDecal, vec3_t vSurfNormal, ref_shader_t *pShader, vec3_t textureSpaceBasis[3], float decalWorldScale[2] )
{
	float	*sAxis = NULL;

	if( pDecal->flags & FDECAL_USESAXIS )
		sAxis = pDecal->saxis;

	// Compute the non-scaled decal basis
	R_DecalComputeBasis( vSurfNormal, sAxis, textureSpaceBasis );

	// world width of decal = ptexture->width / pDecal->scale
	// world height of decal = ptexture->height / pDecal->scale
	// scale is inverse, scales world space to decal u/v space [0,1]
	// OPTIMIZE: Get rid of these divides
	decalWorldScale[0] = pDecal->scale / pShader->stages[0].textures[0]->srcWidth;
	decalWorldScale[1] = pDecal->scale / pShader->stages[0].textures[0]->srcHeight;
	
	VectorScale( textureSpaceBasis[0], decalWorldScale[0], textureSpaceBasis[0] );
	VectorScale( textureSpaceBasis[1], decalWorldScale[1], textureSpaceBasis[1] );
}

// Build the initial list of vertices from the surface verts into the global array, 'verts'.
void R_SetupDecalVertsForMSurface( decal_t *pDecal, msurface_t *surf,	vec3_t textureSpaceBasis[3], decalvert_t *pVerts )
{
	int	j;
	
	for( j = 0; j < surf->mesh->numVerts; j++ )
	{
		Vector4Copy( surf->mesh->vertexArray[j], pVerts[j].m_vPos ); // copy model space coordinates
		pVerts[j].m_tCoords[0] = DotProduct( pVerts[j].m_vPos, textureSpaceBasis[0] ) - pDecal->dx + 0.5f;
		pVerts[j].m_tCoords[1] = DotProduct( pVerts[j].m_vPos, textureSpaceBasis[1] ) - pDecal->dy + 0.5f;
		pVerts[j].m_LMCoords[0] = pVerts[j].m_LMCoords[1] = 0.0f;
	}
}


// Figure out where the decal maps onto the surface.
void R_SetupDecalClip( decalvert_t *pOutVerts, decal_t *pDecal, vec3_t vSurfNormal, ref_shader_t *pMaterial, vec3_t textureSpaceBasis[3], float decalWorldScale[2] )
{
//	if ( pOutVerts == NULL )
//		pOutVerts = &g_DecalClipVerts[0];

	R_SetupDecalTextureSpaceBasis( pDecal, vSurfNormal, pMaterial, textureSpaceBasis, decalWorldScale );

	// Generate texture coordinates for each vertex in decal s,t space
	// probably should pre-generate this, store it and use it for decal-decal collisions
	// as in R_DecalsIntersect()
	pDecal->dx = DotProduct( pDecal->position, textureSpaceBasis[0] );
	pDecal->dy = DotProduct( pDecal->position, textureSpaceBasis[1] );
}

static int SHClip( decalvert_t *g_DecalClipVerts, int vertCount, decalvert_t *out, decal_clip_t clipFunc )
{
	int		j, outCount;
	decalvert_t	*s, *p;

	outCount = 0;

	s = &g_DecalClipVerts[vertCount - 1];

	for( j = 0; j < vertCount; j++ ) 
	{
		p = &g_DecalClipVerts[j];

		if( clipFunc.pfnInside( p )) 
		{
			if( clipFunc.pfnInside( s )) 
			{
				Mem_Copy( out, p, sizeof( *out ));
				outCount++;
				out++;
			}
			else 
			{
				Intersect( clipFunc, s, p, out );
				out++;
				outCount++;

				Mem_Copy( out, p, sizeof( *out ));
				outCount++;
				out++;
			}
		}
		else 
		{
			if( clipFunc.pfnInside( s )) 
			{
				Intersect( clipFunc, p, s, out );
				out++;
				outCount++;
			}
		}
		s = p;
	}
	return outCount;
}

decalvert_t *R_DoDecalSHClip( decalvert_t *pInVerts, decalvert_t *pOutVerts, decal_t *pDecal, int nStartVerts, int *pVertCount )
{
	int	outCount;

	if( pOutVerts == NULL )
		pOutVerts = &g_DecalClipVerts[0];

	// clip the polygon to the decal texture space
	outCount = SHClip( pInVerts, nStartVerts, &g_DecalClipVerts2[0], PlaneTop );
	outCount = SHClip( &g_DecalClipVerts2[0], outCount, &g_DecalClipVerts[0], PlaneLeft );
	outCount = SHClip( &g_DecalClipVerts[0], outCount, &g_DecalClipVerts2[0], PlaneRight );
	outCount = SHClip( &g_DecalClipVerts2[0], outCount, pOutVerts, PlaneBottom );

	if( outCount ) 
	{
		if( pDecal->flags & FDECAL_CLIPTEST )
		{
			pDecal->flags &= ~FDECAL_CLIPTEST;	// we're doing the test
			
			// If there are exactly 4 verts and they are all 0,1 tex coords, then we've got an unclipped decal
			// A more precise test would be to calculate the texture area and make sure it's one, but this
			// should work as well.
			if( outCount == 4 )
			{
				int		j, clipped = 0;
				decalvert_t	*v = pOutVerts;

				for( j = 0; j < outCount && !clipped; j++, v++ )
				{
					float s, t;
					s = v->m_tCoords[0];
					t = v->m_tCoords[1];
					
					if ( (s != 0.0 && s != 1.0) || (t != 0.0 && t != 1.0) )
						clipped = 1;
				}

				// We didn't need to clip this decal, it's a quad covering
				// the full texture space, optimize subsequent frames.
				if( !clipped ) pDecal->flags |= FDECAL_NOCLIP;
			}
		}
	}
	
	*pVertCount = outCount;
	return pOutVerts;
}

//-----------------------------------------------------------------------------
// Generate clipped vertex list for decal pdecal projected onto polygon psurf
//-----------------------------------------------------------------------------
decalvert_t *R_DecalVertsClip( decalvert_t *pOutVerts, decal_t *pDecal, msurface_t *surf, ref_shader_t *pShader, int *pVertCount )
{
	float	decalWorldScale[2];
	vec3_t	textureSpaceBasis[3]; 

	// figure out where the decal maps onto the surface.
	R_SetupDecalClip( pOutVerts, pDecal, surf->plane->normal, pShader, textureSpaceBasis, decalWorldScale );

	// build the initial list of vertices from the surface verts.
	R_SetupDecalVertsForMSurface( pDecal, surf, textureSpaceBasis, g_DecalClipVerts );

	return R_DoDecalSHClip( g_DecalClipVerts, pOutVerts, pDecal, surf->mesh->numVerts, pVertCount );
}

// Generate lighting coordinates at each vertex for decal vertices v[] on surface psurf
static void R_DecalVertsLight( decalvert_t *v, msurface_t *surf, int vertCount )
{
	float		s, t;
	mtexinfo_t	*tex;
	int		j;

	tex = surf->texinfo;

	for ( j = 0; j < vertCount; j++, v++ )
	{
		// lightmap texture coordinates
		s = DotProduct( v->m_vPos, tex->vecs[0] ) + tex->vecs[0][3] - surf->textureMins[0];
		s += surf->lmS * LM_SAMPLE_SIZE;
		s += LM_SAMPLE_SIZE >> 1;
		s /= LIGHTMAP_TEXTURE_WIDTH * LM_SAMPLE_SIZE;

		t = DotProduct( v->m_vPos, tex->vecs[1] ) + tex->vecs[1][3] - surf->textureMins[1];
		t += surf->lmT * LM_SAMPLE_SIZE;
		t += LM_SAMPLE_SIZE >> 1;
		t /= LIGHTMAP_TEXTURE_HEIGHT * LM_SAMPLE_SIZE;

		v->m_LMCoords[0] = s;
		v->m_LMCoords[1] = t;
	}
}

static decalvert_t* R_DecalVertsNoclip( decal_t *pdecal, msurface_t *surf, ref_shader_t *pShader )
{
	decalcache_t	*pCache;
	decalvert_t	*vlist;
	int		decalIndex;
	int		outCount;

	decalIndex = R_DecalIndex( pdecal );
	pCache = R_DecalCacheSlot( decalIndex );
	
	// Is the decal cached?
	if ( pCache->decalIndex == decalIndex )
	{
		return &pCache->decalVert[0];
	}

	pCache->decalIndex = decalIndex;
	vlist = &pCache->decalVert[0];

	// use the old code for now, and just cache them
	vlist = R_DecalVertsClip( vlist, pdecal, surf, pShader, &outCount );

	R_DecalVertsLight( vlist, surf, 4 );

	return vlist;
}

//-----------------------------------------------------------------------------
// Purpose: Check for intersecting decals on this surface
// Input  : *psurf - 
//			*pcount - 
//			x - 
//			y - 
// Output : static decal_t
//-----------------------------------------------------------------------------
// UNDONE: This probably doesn't work quite right any more
// we should base overlap on the new decal basis matrix
// decal basis is constant per plane, perhaps we should store it (unscaled) in the shared plane struct
// BRJ: Note, decal basis is not constant when decals need to specify an s direction
// but that certainly isn't the majority case
static decal_t *R_DecalIntersect( decalinfo_t *decalinfo, msurface_t *surf, int *pcount )
{
	ref_shader_t	*pShader;
	decal_t		*plast, *pDecal;
	vec3_t		decalExtents[2];
	float		lastArea = 2;
	int		mapSize[2];

	plast = NULL;
	*pcount = 0;

	// (Same as R_SetupDecalClip).
	pShader = decalinfo->m_pShader;
	
	// precalculate the extents of decalinfo's decal in world space.
	mapSize[0] = pShader->stages[0].textures[0]->srcWidth;
	mapSize[1] = pShader->stages[0].textures[0]->srcHeight;
	VectorScale( decalinfo->m_Basis[0], ((mapSize[0] / decalinfo->m_scale) * 0.5f), decalExtents[0] );
	VectorScale( decalinfo->m_Basis[1], ((mapSize[1] / decalinfo->m_scale) * 0.5f), decalExtents[1] );

	pDecal = surf->pdecals;

	while( pDecal ) 
	{
		pShader = pDecal->shader;

		// Don't steal bigger decals and replace them with smaller decals
		// Don't steal permanent decals
		if(!( pDecal->flags & FDECAL_PERMANENT ))
		{
			vec3_t	testBasis[3];
			vec3_t	testPosition[2];
			float	testWorldScale[2];
			vec2_t	vDecalMin, vDecalMax;
			vec2_t	vUnionMin, vUnionMax;

			R_SetupDecalTextureSpaceBasis( pDecal, surf->plane->normal, pShader, testBasis, testWorldScale );

			VectorSubtract( decalinfo->m_Position, decalExtents[0], testPosition[0] );
			VectorSubtract( decalinfo->m_Position, decalExtents[1], testPosition[1] );

			// Here, we project the min and max extents of the decal that got passed in into
			// this decal's (pDecal's) [0,0,1,1] clip space, just like we would if we were
			// clipping a triangle into pDecal's clip space.
			Vector2Set( vDecalMin, DotProduct( testPosition[0], testBasis[0] ) - pDecal->dx + 0.5f,
				DotProduct( testPosition[1], testBasis[1] ) - pDecal->dy + 0.5f );

			VectorAdd( decalinfo->m_Position, decalExtents[0], testPosition[0] );
			VectorAdd( decalinfo->m_Position, decalExtents[1], testPosition[1] );

			Vector2Set( vDecalMax,
				DotProduct( testPosition[0], testBasis[0] ) - pDecal->dx + 0.5f,
				DotProduct( testPosition[1], testBasis[1] ) - pDecal->dy + 0.5f );	

			// Now figure out the part of the projection that intersects pDecal's
			// clip box [0,0,1,1].
			Vector2Set( vUnionMin, max( vDecalMin[0], 0 ), max( vDecalMin[1], 0 ));
			Vector2Set( vUnionMax, min( vDecalMax[0], 1 ), min( vDecalMax[1], 1 ));

			if( vUnionMin[0] < 1 && vUnionMin[1] < 1 && vUnionMax[0] > 0 && vUnionMax[1] > 0 )
			{
				// Figure out how much of this intersects the (0,0) - (1,1) bbox.			
				float	flArea = (vUnionMax[0] - vUnionMin[1]) * (vUnionMax[1] - vUnionMin[1]);

				if( flArea > 0.6f )
				{
					*pcount += 1;

					if( !plast || flArea <= lastArea ) 
					{
						plast = pDecal;
						lastArea =  flArea;
					}
				}
			}
		}
		pDecal = pDecal->pnext;
	}
	return plast;
}

// Add the decal to the surface's list of decals.
// If the surface is a displacement, let the displacement precalculate data for the decal.
static void R_AddDecalToSurface( decal_t *pdecal, msurface_t *surf, decalinfo_t *decalinfo )
{
	decal_t	*pold;

	pdecal->pnext = NULL;
	pold = surf->pdecals;

	if( pold ) 
	{
		while( pold->pnext ) 
			pold = pold->pnext;
		pold->pnext = pdecal;
	}
	else
	{
		surf->pdecals = pdecal;
	}

	// Tag surface
	pdecal->psurf = surf;
	pdecal->m_Size = decalinfo->m_Size;
}

static void R_DecalCreate( decalinfo_t *decalinfo, msurface_t *surf, float x, float y )
{
	decal_t	*pdecal, *pold;
	int	count, vertCount;

	if( !surf )
	{
		MsgDev( D_ERROR, "psurface NULL in R_DecalCreate!\n" );
		return;
	}
	
	pold = R_DecalIntersect( decalinfo, surf, &count );
	if( count < MAX_OVERLAP_DECALS ) pold = NULL;

	pdecal = R_DecalAlloc( pold );
	pdecal->flags = decalinfo->m_Flags;

	Vector4Copy( decalinfo->m_Color, pdecal->color );
	VectorCopy( decalinfo->m_Position, pdecal->position );

	if( pdecal->flags & FDECAL_USESAXIS )
		VectorCopy( decalinfo->m_SAxis, pdecal->saxis );

	pdecal->dx = x;
	pdecal->dy = y;
	pdecal->shader = decalinfo->m_pShader;
	Com_Assert( pdecal->shader == NULL );

	// set scaling
	pdecal->scale = decalinfo->m_scale;
	pdecal->entityIndex = decalinfo->m_Entity;

	// Get dynamic information from the material (fade start, fade time)
/*
	// pseudo-code for future expansions
	if( shader->decalFadeTime  )
	{
		pdecal->flags |= FDECAL_DYNAMIC;
		pdecal->fadeStartTime = RI.refdef.time + RANDOM_LONG( shader->decalTime / 2, shader->decalTime );
	}
*/
	// Is this a second-pass decal?
//	if( shader->decalSecondPass ) pdecal->flags |= FDECAL_SECONDPASS;

	// check to see if the decal actually intersects the surface
	// if not, then remove the decal
	R_DecalVertsClip( NULL, pdecal, surf, decalinfo->m_pShader, &vertCount );
	
	if( !vertCount )
	{
		R_DecalUnlink( pdecal );
		return;
	}

	Msg( "R_AddDecalToSurface( %i verts )\n", vertCount );

	// add to the surface's list
	R_AddDecalToSurface( pdecal, surf, decalinfo );
}

void R_DecalSurface( msurface_t *surf, decalinfo_t *decalinfo )
{
	// get the texture associated with this surface
	mtexinfo_t	*tex = surf->texinfo;
	vec4_t		textureU, textureV;
	float		*sAxis = NULL;
	float		s, t, w, h;

	Vector4Copy( tex->vecs[0], textureU );
	Vector4Copy( tex->vecs[1], textureV );

	// project decal center into the texture space of the surface
	s = DotProduct( decalinfo->m_Position, textureU ) + textureU[3] - surf->textureMins[0];
	t = DotProduct( decalinfo->m_Position, textureV ) + textureV[3] - surf->textureMins[1];

	// Determine the decal basis (measured in world space)
	// Note that the decal basis vectors 0 and 1 will always lie in the same
	// plane as the texture space basis vectorstextureVecsTexelsPerWorldUnits.

	if( decalinfo->m_Flags & FDECAL_USESAXIS )
		sAxis = decalinfo->m_SAxis;

	R_DecalComputeBasis( surf->plane->normal, sAxis, decalinfo->m_Basis );

	// Compute an effective width and height (axis aligned) in the parent texture space
	// How does this work? decalBasis[0] represents the u-direction (width)
	// of the decal measured in world space, decalBasis[1] represents the 
	// v-direction (height) measured in world space.
	// textureVecsTexelsPerWorldUnits[0] represents the u direction of 
	// the surface's texture space measured in world space (with the appropriate
	// scale factor folded in), and textureVecsTexelsPerWorldUnits[1]
	// represents the texture space v direction. We want to find the dimensions (w,h)
	// of a square measured in texture space, axis aligned to that coordinate system.
	// All we need to do is to find the components of the decal edge vectors
	// (decalWidth * decalBasis[0], decalHeight * decalBasis[1])
	// in texture coordinates:

	w = fabs( decalinfo->m_decalWidth  * DotProduct( textureU, decalinfo->m_Basis[0] )) +
	    fabs( decalinfo->m_decalHeight * DotProduct( textureU, decalinfo->m_Basis[1] ));
	
	h = fabs( decalinfo->m_decalWidth  * DotProduct( textureV, decalinfo->m_Basis[0] )) +
	    fabs( decalinfo->m_decalHeight * DotProduct( textureV, decalinfo->m_Basis[1] ));

	// move s,t to upper left corner
	s -= ( w * 0.5f );
	t -= ( h * 0.5f );

	// Is this rect within the surface? -- tex width & height are unsigned
	if( s <= -w || t <= -h || s > (surf->extents[0] + w) || t > (surf->extents[1] + h))
	{
		return; // nope
	}

	// stamp it
	R_DecalCreate( decalinfo, surf, s, t );
}

//-----------------------------------------------------------------------------
// iterate over all surfaces on a node, looking for surfaces to decal
//-----------------------------------------------------------------------------
static void R_DecalNodeSurfaces( mnode_t* node, decalinfo_t *decalinfo )
{
	// iterate over all surfaces in the node
	msurface_t	*surf;
	int		i;

	for( i = 0, surf = node->firstface; i < node->numfaces; i++, surf++ ) 
	{
		// If this is a water decal, ONLY decal water, reject other surfaces
		if( decalinfo->m_Flags & FDECAL_WATER )
		{
			if(!( surf->flags & SURF_DRAWTURB ))
				continue;
		}
		R_DecalSurface( surf, decalinfo );
	}
}

void R_DecalLeaf( mleaf_t *pleaf, decalinfo_t *decalinfo )
{
	msurface_t	**mark, *surf;
	int		i;

	for( i = 0, mark = pleaf->firstMarkSurface; i < pleaf->numMarkSurfaces; i++, mark++ )		
	{
		float	dist;

		surf = *mark;

		// If this is a water decal, ONLY decal water, reject other surfaces
		if( decalinfo->m_Flags & FDECAL_WATER )
		{
			if(!( surf->flags & SURF_DRAWTURB ))
				continue;
		}

		dist = fabs( DotProduct( decalinfo->m_Position, surf->plane->normal ) - surf->plane->dist );

		if( dist < DECAL_DISTANCE )
		{
			R_DecalSurface( surf, decalinfo );
		}
	}
}

//-----------------------------------------------------------------------------
// Recursive routine to find surface to apply a decal to.  World coordinates of 
// the decal are passed in r_recalpos like the rest of the engine.  This should 
// be called through R_DecalShoot()
//-----------------------------------------------------------------------------
static void R_DecalNode( mnode_t *node, decalinfo_t *decalinfo )
{
	cplane_t	*splitplane;
	float	dist;
	
	if( !node ) return;

	if( !node->plane )
	{
		// hit a leaf
		R_DecalLeaf(( mleaf_t *)node, decalinfo );
		return;
	}

	splitplane = node->plane;
	dist = DotProduct( decalinfo->m_Position, splitplane->normal ) - splitplane->dist;

	// This is arbitrarily set to 10 right now. In an ideal world we'd have the 
	// exact surface but we don't so, this tells me which planes are "sort of 
	// close" to the gunshot -- the gunshot is actually 4 units in front of the 
	// wall (see dlls\weapons.cpp). We also need to check to see if the decal 
	// actually intersects the texture space of the surface, as this method tags
	// parallel surfaces in the same node always.
	// JAY: This still tags faces that aren't correct at edges because we don't 
	// have a surface normal
	if( dist > decalinfo->m_Size )
	{
		R_DecalNode( node->children[0], decalinfo );
	}
	else if( dist < -decalinfo->m_Size )
	{
		R_DecalNode( node->children[1], decalinfo );
	}
	else 
	{
		if( dist < DECAL_DISTANCE && dist > -DECAL_DISTANCE )
			R_DecalNodeSurfaces( node, decalinfo );

		R_DecalNode( node->children[0], decalinfo );
		R_DecalNode( node->children[1], decalinfo );
	}
}

// Shoots a decal onto the surface of the BSP.  position is the center of the decal in world coords
static void R_DecalShoot_( ref_shader_t *shader, int entity, ref_model_t *model, vec3_t pos, vec3_t saxis, int flags, rgba_t color )
{
	decalinfo_t	decalInfo;
	mbrushmodel_t	*bmodel;
	mnode_t		*pnodes;

	if( !shader || shader->type != SHADER_DECAL )
	{
		MsgDev( D_ERROR, "Decals has invalid shader!\n" );
		return;
	}

	if( !model || ( model->type != mod_brush && model->type != mod_world ))
	{
		MsgDev( D_ERROR, "Decals must hit mod_brush or mod_world!\n" );
		return;
	}

	VectorCopy( pos, decalInfo.m_Position ); // pass position in global

	if( model ) decalInfo.m_pModel = model;
	else decalInfo.m_pModel = NULL;

	// deal with the s axis if one was passed in
	if( saxis )
	{
		flags |= FDECAL_USESAXIS;
		VectorCopy( saxis, decalInfo.m_SAxis );
	}

	// more state used by R_DecalNode()
	decalInfo.m_pShader = shader;

	// don't optimize custom decals
	if(!( flags & FDECAL_CUSTOM ))
		flags |= FDECAL_CLIPTEST;
	
	decalInfo.m_Flags = flags;
	decalInfo.m_Entity = entity;
	decalInfo.m_Size = shader->stages[0].textures[0]->srcWidth >> 1;
	if((shader->stages[0].textures[0]->srcHeight >> 1) > decalInfo.m_Size )
		decalInfo.m_Size = shader->stages[0].textures[0]->srcHeight >> 1;

	// FIXME: grab scale from shader ?
	decalInfo.m_scale = 1.0f;

	// compute the decal dimensions in world space
	decalInfo.m_decalWidth = shader->stages[0].textures[0]->srcWidth / decalInfo.m_scale;
	decalInfo.m_decalHeight = shader->stages[0].textures[0]->srcHeight / decalInfo.m_scale;
	Vector4Copy( color, decalInfo.m_Color );

	bmodel = (mbrushmodel_t *)decalInfo.m_pModel->extradata;
	pnodes = bmodel->firstmodelnode;

	R_DecalNode( pnodes, &decalInfo );
}

bool R_DecalShoot( shader_t texture, int entity, model_t modelIndex, vec3_t pos, vec3_t saxis, int flags, rgba_t color )
{
	ref_shader_t	*shader;
	ref_model_t	*model;

	// setup shader
	if( texture < 0 || texture >= MAX_SHADERS || !( shader = &r_shaders[texture] ))
		return false;

	// setup model
	if( modelIndex <= 0 || modelIndex >= MAX_MODELS )
		model = r_worldmodel;
	else model = cl_models[modelIndex];

	R_DecalShoot_( shader, entity, model, pos, saxis, flags, color );

	return true;
}

void R_AddSurfaceDecals( msurface_t *surf )
{
	decal_t	*plist;

	Com_Assert( surf == NULL );

	plist = surf->pdecals;

	while( plist ) 
	{
		// Store off the next pointer, DecalUpdateAndDrawSingle could unlink
		decal_t	*pnext = plist->pnext;

		if( r_numDecals >= MAX_DECALS )
		{
			MsgDev( D_ERROR, "R_AddSurfaceDecals: too many decals\n" ); 	
			break;
		}

		// add decals into list
		r_drawdecals[r_numDecals++] = plist;
		plist = pnext;
	}
}

//-----------------------------------------------------------------------------
// Updates all decals, returns true if the decal should be retired
//-----------------------------------------------------------------------------
bool DecalUpdate( decal_t *pDecal )
{
	// retire the decal if it's time has come
	if( pDecal->fadeDuration > 0.0f )
	{
		return ( RI.refdef.time >= pDecal->fadeStartTime + pDecal->fadeDuration );
	}
	return false;
}

// Build the vertex list for a decal on a surface and clip it to the surface.
// This is a template so it can work on world surfaces and dynamic displacement 
// triangles the same way.
decalvert_t* R_DecalSetupVerts( decal_t *pDecal, msurface_t *surf, ref_shader_t *pShader, int *outCount )
{
	decalvert_t	*v;

	if( pDecal->flags & FDECAL_NOCLIP )
	{
		v = R_DecalVertsNoclip( pDecal, surf, pShader );
		*outCount = 4;
	}
	else
	{
		v = R_DecalVertsClip( NULL, pDecal, surf, pShader, outCount );
		if( outCount ) R_DecalVertsLight( v, surf, *outCount );
	}
	return v;
}

/*
=================
R_PushDecal
=================
*/
void R_PushDecal( const meshbuffer_t *mb )
{
	decal_t		*pDecal;
	ref_shader_t	*shader;
	bool		retire = false;
	int		i, outCount, features;
	decalvert_t	*v;

	MB_NUM2SHADER( mb->shaderkey, shader );
	pDecal = r_drawdecals[-mb->infokey-1];

	Com_Assert( pDecal == NULL );

	if( !pDecal->shader )
		return;

	// update dynamic decals
	if( pDecal->flags & FDECAL_DYNAMIC )
		retire = DecalUpdate( pDecal );

	v = R_DecalSetupVerts( pDecal, pDecal->psurf, pDecal->shader, &outCount );

	if( !outCount ) return; // nothing to draw
	if( retire ) R_DecalUnlink( pDecal );

	features = ( shader->features|MF_TRIFAN );
	

	decal_mesh.numVerts = outCount;
	decal_mesh.vertexArray = inVertsArray;
	decal_mesh.normalsArray = inNormalsArray;
	decal_mesh.stCoordArray = inCoordsArray;
	decal_mesh.colorsArray = inColorsArray;
	decal_mesh.lmCoordArray = inLightmapCoordsArray;

	for( i = 0; i < outCount; i++, v++ )
	{
		Vector4Copy( v->m_vPos, inVertsArray[r_backacc.numVerts+i] );
		Vector4Copy( pDecal->color, inColorsArray[r_backacc.numVerts+i] );
		Vector2Copy( v->m_tCoords, inCoordsArray[r_backacc.numVerts+i] );
		Vector2Copy( v->m_LMCoords, inLightmapCoordsArray[r_backacc.numVerts+i] );
		Vector4Set( inNormalsArray[r_backacc.numVerts+i], 0.0f, 0.0f, 1.0f, 1.0f );
	}

	R_PushMesh( &decal_mesh, features );
}

/*
================
R_AddDecalsToList

list contain only decals that passed culling
================
*/
void R_AddDecalsToList( void )
{
	decal_t		*decal;
	meshbuffer_t	*mb;
	int		i;

	RI.currententity = r_worldent;
	RI.currentmodel = r_worldmodel;
		
	for( i = 0; i < r_numDecals; i++ )
	{
		decal = r_drawdecals[i];
		Com_Assert( decal == NULL );
		Com_Assert( decal->shader == NULL );
		mb = R_AddMeshToList( MB_DECAL, NULL, decal->shader, -((signed int)i + 1 ));
//		if( mb ) mb->lastPoly = i;
	}
}

void R_DrawSingleDecal( const meshbuffer_t *mb )
{
	RI.currententity = r_worldent;
	RI.currentmodel = r_worldmodel;

	// decals are already batched at this point
	R_PushDecal( mb );

	if( RI.previousentity != RI.currententity )
		R_LoadIdentity();
	R_RenderMeshBuffer( mb );
}