//=======================================================================
//			Copyright XashXT Group 2010 ©
//		       gl_studio.c - studio model renderer
//=======================================================================

#include "common.h"
#include "client.h"
#include "matrix_lib.h"
#include "const.h"
#include "r_studioint.h"
#include "studio.h"
#include "pm_local.h"
#include "gl_local.h"
#include "cl_tent.h"

#define EVENT_CLIENT	5000	// less than this value it's a server-side studio events

static vec3_t hullcolor[8] = 
{
{ 1.0f, 1.0f, 1.0f },
{ 1.0f, 0.5f, 0.5f },
{ 0.5f, 1.0f, 0.5f },
{ 1.0f, 1.0f, 0.5f },
{ 0.5f, 0.5f, 1.0f },
{ 1.0f, 0.5f, 1.0f },
{ 0.5f, 1.0f, 1.0f },
{ 1.0f, 1.0f, 1.0f },
};

convar_t			*r_studio_lerping;
char			model_name[64];
static r_studio_interface_t	*pStudioDraw;
static float		aliasXscale, aliasYscale;	// software renderer scale
static matrix3x4		g_aliastransform;		// software renderer transform
static matrix3x4		g_rotationmatrix;
static matrix3x4		g_bonestransform[MAXSTUDIOBONES];
static matrix3x4		g_lighttransform[MAXSTUDIOBONES];
static matrix3x4		g_rgCachedBonesTransform[MAXSTUDIOBONES];
static matrix3x4		g_rgCachedLightTransform[MAXSTUDIOBONES];
static vec3_t		g_xformverts[MAXSTUDIOVERTS];
static vec3_t		g_xformnorms[MAXSTUDIOVERTS];
static vec3_t		g_lightvalues[MAXSTUDIOVERTS];
char			g_nCachedBoneNames[MAXSTUDIOBONES][32];
int			g_nCachedBones;		// number of bones in cache
vec3_t			studio_mins, studio_maxs;
float			studio_radius;

// global variables
qboolean			m_fDoInterp;
mstudiomodel_t		*m_pSubModel;
mstudiobodyparts_t		*m_pBodyPart;
player_info_t		*m_pPlayerInfo;
studiohdr_t		*m_pStudioHeader;
int			g_nTopColor, g_nBottomColor;	// remap colors
int			g_nFaceFlags;

/*
====================
R_StudioInit

====================
*/
void R_StudioInit( void )
{
	float	pixelAspect;

	r_studio_lerping = Cvar_Get( "r_studio_lerping", "1", CVAR_ARCHIVE, "enables studio animation lerping" );

	// recalc software X and Y alias scale (this stuff is used only by HL software renderer but who knews...)
	pixelAspect = ((float)scr_height->integer / (float)scr_width->integer);
	if( scr_width->integer < 640 )
		pixelAspect *= (320.0f / 240.0f);
	else pixelAspect *= (640.0f / 480.0f);

	aliasXscale = (float)scr_width->integer / RI.refdef.fov_y;
	aliasYscale = aliasXscale * pixelAspect;

	Matrix3x4_LoadIdentity( g_aliastransform );
	Matrix3x4_LoadIdentity( g_rotationmatrix );
}

/*
===============
R_StudioTexName

extract texture filename from modelname
===============
*/
const char *R_StudioTexName( model_t *mod )
{
	static char	texname[64];

	com.strncpy( texname, mod->name, sizeof( texname ));
	FS_StripExtension( texname );
	com.strncat( texname, "T.mdl", sizeof( texname ));

	return texname;
}

/*
================
R_StudioBodyVariations

calc studio body variations
================
*/
static int R_StudioBodyVariations( model_t *mod )
{
	studiohdr_t	*pstudiohdr;
	mstudiobodyparts_t	*pbodypart;
	int		i, count;

	pstudiohdr = (studiohdr_t *)Mod_Extradata( mod );
	if( !pstudiohdr ) return 0;

	count = 1;
	pbodypart = (mstudiobodyparts_t *)((byte *)pstudiohdr + pstudiohdr->bodypartindex);

	// each body part has nummodels variations so there are as many total variations as there
	// are in a matrix of each part by each other part
	for( i = 0; i < pstudiohdr->numbodyparts; i++ )
		count = count * pbodypart[i].nummodels;

	return count;
}

/*
================
R_StudioExtractBbox

Extract bbox from current sequence
================
*/
int R_StudioExtractBbox( studiohdr_t *phdr, int sequence, float *mins, float *maxs )
{
	mstudioseqdesc_t	*pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
	if( sequence == -1 )
		return 0;
	
	VectorCopy( pseqdesc[sequence].bbmin, mins );
	VectorCopy( pseqdesc[sequence].bbmax, maxs );

	return 1;
}

/*
================
R_StudioComputeBBox

Compute a full bounding box for current sequence
================
*/
static qboolean R_StudioComputeBBox( cl_entity_t *e, vec3_t bbox[8] )
{
	vec3_t		tmp_mins, tmp_maxs;
	vec3_t		vectors[3], angles, p1, p2;
	int		i, seq = e->curstate.sequence;
	float		scale = 1.0f;

	if( !R_StudioExtractBbox( m_pStudioHeader, seq, tmp_mins, tmp_maxs ))
		return false;

	// copy original bbox
	VectorCopy( m_pStudioHeader->bbmin, studio_mins );
	VectorCopy( m_pStudioHeader->bbmin, studio_mins );

	// rotate the bounding box
	VectorScale( e->angles, -1, angles );

	if( e->player ) angles[PITCH] = 0; // don't rotate player model, only aim
	AngleVectors( angles, vectors[0], vectors[1], vectors[2] );
	VectorNegate( vectors[1], vectors[1] );

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

	if( e->curstate.scale > 0.0f )
		scale = e->curstate.scale;

	studio_radius = RadiusFromBounds( studio_mins, studio_maxs ) * scale;

	return true;
}

/*
===============
pfnGetCurrentEntity

===============
*/
static cl_entity_t *pfnGetCurrentEntity( void )
{
	return RI.currententity;
}

/*
===============
pfnPlayerInfo

===============
*/
static player_info_t *pfnPlayerInfo( int index )
{
	if( index < 0 || index > cl.maxclients )
		return NULL;
	return &cl.players[index];
}

/*
===============
pfnGetPlayerState

===============
*/
entity_state_t *R_StudioGetPlayerState( int index )
{
	if( index < 0 || index > cl.maxclients )
		return NULL;
	return &cl.frame.playerstate[index];
}

/*
===============
pfnGetViewEntity

===============
*/
static cl_entity_t *pfnGetViewEntity( void )
{
	return &clgame.viewent;
}

/*
===============
pfnGetEngineTimes

===============
*/
static void pfnGetEngineTimes( int *framecount, double *current, double *old )
{
	if( framecount ) *framecount = tr.framecount;
	if( current ) *current = cl.time;
	if( old ) *old = cl.oldtime;
}

/*
===============
pfnGetViewInfo

===============
*/
static void pfnGetViewInfo( float *origin, float *upv, float *rightv, float *forwardv )
{
	if( origin ) VectorCopy( RI.vieworg, origin );
	if( forwardv ) VectorCopy( RI.vforward, forwardv );
	if( rightv ) VectorCopy( RI.vright, rightv );
	if( upv ) VectorCopy( RI.vup, upv );
}

/*
===============
R_GetChromeSprite

===============
*/
static model_t *R_GetChromeSprite( void )
{
	if( cls.hChromeSprite <= 0 || cls.hChromeSprite > ( MAX_IMAGES - 1 ))
		return NULL; // bad sprite
	return &clgame.sprites[cls.hChromeSprite];
}

/*
===============
pfnGetModelCounters

===============
*/
static void pfnGetModelCounters( int **s, int **a )
{
	*s = &r_stats.c_studio_models_count;
	*a = &r_stats.c_studio_models_drawn;
}

/*
===============
pfnGetAliasScale

===============
*/
static void pfnGetAliasScale( float *x, float *y )
{
	if( x ) *x = aliasXscale;
	if( y ) *y = aliasYscale;
}

/*
===============
pfnStudioGetBoneTransform

===============
*/
static float ****pfnStudioGetBoneTransform( void )
{
	return (float ****)g_bonestransform;
}

/*
===============
pfnStudioGetLightTransform

===============
*/
static float ****pfnStudioGetLightTransform( void )
{
	return (float ****)g_lighttransform;
}

/*
===============
pfnStudioGetAliasTransform

===============
*/
static float ***pfnStudioGetAliasTransform( void )
{
	return (float ***)g_aliastransform;
}

/*
===============
pfnStudioGetRotationMatrix

===============
*/
static float ***pfnStudioGetRotationMatrix( void )
{
	return (float ***)g_rotationmatrix;
}

/*
====================
CullStudioModel

====================
*/
qboolean R_CullStudioModel( cl_entity_t *e )
{
	if( !e->model->cache.data )
		return true;

	if( e == &clgame.viewent && r_lefthand->integer >= 2 )
		return true; // hidden

	if( !R_StudioComputeBBox( e, NULL ))
		return true; // invalid sequence

	return R_CullModel( e, studio_mins, studio_maxs, studio_radius );
}

/*
====================
StudioSetUpTransform

====================
*/
void R_StudioSetUpTransform( cl_entity_t *e )
{
	vec3_t	origin, angles;
	float	scale = 1.0f;

	VectorCopy( e->origin, origin );
	VectorCopy( e->angles, angles );

	// interpolate monsters position
	if( e->curstate.movetype == MOVETYPE_STEP || e->curstate.movetype == MOVETYPE_FLY ) 
	{
		float		d, f = 0.0f;
		cl_entity_t	*m_pGroundEntity;
		int		i;

		// don't do it if the goalstarttime hasn't updated in a while.
		// NOTE: Because we need to interpolate multiplayer characters, the interpolation time limit
		// was increased to 1.0 s., which is 2x the max lag we are accounting for.

		if( m_fDoInterp && ( cl.time < e->curstate.animtime + 1.0f ) && ( e->curstate.animtime != e->latched.prevanimtime ))
		{
			f = ( cl.time - e->curstate.animtime ) / ( e->curstate.animtime - e->latched.prevanimtime );
			// Msg( "%4.2f %.2f %.2f\n", f, e->curstate.animtime, cl.time );
		}

		if( m_fDoInterp )
		{
			// ugly hack to interpolate angle, position.
			// current is reached 0.1 seconds after being set
			f = f - 1.0f;
		}

		m_pGroundEntity = CL_GetEntityByIndex( e->curstate.onground );

		if( m_pGroundEntity && m_pGroundEntity->curstate.movetype == MOVETYPE_PUSH && !VectorIsNull( m_pGroundEntity->curstate.velocity ))
		{
			mstudioseqdesc_t		*pseqdesc;
		
			pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->curstate.sequence;
			d = RI.lerpFrac;

			origin[0] += ( e->origin[0] - e->prevstate.origin[0] ) * d;
			origin[1] += ( e->origin[1] - e->prevstate.origin[1] ) * d;
			origin[2] += ( e->origin[2] - e->prevstate.origin[2] ) * d;

			d = f - d;

			// monster walking on moving platform
			if( pseqdesc->motiontype & STUDIO_LX )
			{
				origin[0] += ( e->curstate.origin[0] - e->latched.prevorigin[0] ) * d;
				origin[1] += ( e->curstate.origin[1] - e->latched.prevorigin[1] ) * d;
				origin[2] += ( e->curstate.origin[2] - e->latched.prevorigin[2] ) * d;
			}
		}
		else
		{
			origin[0] += ( e->curstate.origin[0] - e->latched.prevorigin[0] ) * f;
			origin[1] += ( e->curstate.origin[1] - e->latched.prevorigin[1] ) * f;
			origin[2] += ( e->curstate.origin[2] - e->latched.prevorigin[2] ) * f;
		}

		for( i = 0; i < 3; i++ )
		{
			float	ang1, ang2;

			ang1 = e->curstate.angles[i];
			ang2 = e->latched.prevangles[i];

			d = ang1 - ang2;

			if( d > 180 ) d -= 360;
			else if( d < -180 ) d += 360;

			angles[i] += d * f;
		}

		// update it so attachments always have right pos
		if( !RI.refdef.paused ) VectorCopy( origin, e->origin );
	}

	// stupid Half-Life bug
	angles[PITCH] = -angles[PITCH];

	// don't rotate clients, only aim
	if( e->player ) angles[PITCH] = 0;

	if( e->curstate.scale > 0.0f )
		scale = e->curstate.scale;

	Matrix3x4_CreateFromEntity( g_rotationmatrix, angles, origin, scale );

	if( e == &clgame.viewent && r_lefthand->integer == 1 )
	{
		// inverse the right vector
		g_rotationmatrix[0][1] = -g_rotationmatrix[0][1];
		g_rotationmatrix[1][1] = -g_rotationmatrix[1][1];
		g_rotationmatrix[2][1] = -g_rotationmatrix[2][1];
	}
}

/*
====================
StudioEstimateFrame

====================
*/
float R_StudioEstimateFrame( cl_entity_t *e, mstudioseqdesc_t *pseqdesc )
{
	double	dfdt, f;
	
	if( m_fDoInterp )
	{
		if( cl.time < e->curstate.animtime ) dfdt = 0;
		else dfdt = (cl.time - e->curstate.animtime) * e->curstate.framerate * pseqdesc->fps;
	}
	else dfdt = 0;

	if( pseqdesc->numframes <= 1 ) f = 0.0;
	else f = (e->curstate.frame * (pseqdesc->numframes - 1)) / 256.0;
 
	f += dfdt;

	if( pseqdesc->flags & STUDIO_LOOPING ) 
	{
		if( pseqdesc->numframes > 1 )
			f -= (int)(f / (pseqdesc->numframes - 1)) *  (pseqdesc->numframes - 1);
		if( f < 0 ) f += (pseqdesc->numframes - 1);
	}
	else 
	{
		if( f >= pseqdesc->numframes - 1.001 )
			f = pseqdesc->numframes - 1.001;
		if( f < 0.0 )  f = 0.0;
	}
	return f;
}

/*
====================
StudioEstimateInterpolant

====================
*/
float R_StudioEstimateInterpolant( cl_entity_t *e )
{
	float	dadt = 1.0f;

	if( m_fDoInterp && ( e->curstate.animtime >= e->latched.prevanimtime + 0.01f ))
	{
		dadt = ( cl.time - e->curstate.animtime ) / 0.1f;
		if( dadt > 2.0f ) dadt = 2.0f;
	}
	return dadt;
}

/*
====================
StudioGetAnim

====================
*/
mstudioanim_t *R_StudioGetAnim( model_t *m_pSubModel, mstudioseqdesc_t *pseqdesc )
{
	mstudioseqgroup_t	*pseqgroup;
	cache_user_t	*paSequences;
	size_t		filesize;
          byte		*buf;

	ASSERT( m_pSubModel );	

	pseqgroup = (mstudioseqgroup_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqgroupindex) + pseqdesc->seqgroup;
	if( pseqdesc->seqgroup == 0 )
		return (mstudioanim_t *)((byte *)m_pStudioHeader + pseqgroup->data + pseqdesc->animindex);

	paSequences = (cache_user_t *)m_pSubModel->submodels;

	if( paSequences == NULL )
	{
		paSequences = (cache_user_t *)Mem_Alloc( m_pSubModel->mempool, MAXSTUDIOGROUPS * sizeof( cache_user_t ));
		m_pSubModel->submodels = (void *)paSequences;
	}

	// check for already loaded
	if( !Mod_CacheCheck(( cache_user_t *)&( paSequences[pseqdesc->seqgroup] )))
	{
		string	filepath, modelname, modelpath;

		FS_FileBase( m_pSubModel->name, modelname );
		FS_ExtractFilePath( m_pSubModel->name, modelpath );
		com.snprintf( filepath, sizeof( filepath ), "%s/%s%i%i.mdl", modelpath, modelname, pseqdesc->seqgroup / 10, pseqdesc->seqgroup % 10 );

		buf = FS_LoadFile( filepath, &filesize );
		if( !buf || !filesize ) Host_Error( "StudioGetAnim: can't load %s\n", filepath );
		if( IDSEQGRPHEADER != *(uint *)buf )
			Host_Error( "StudioGetAnim: %s is corrupted\n", filepath );

		MsgDev( D_INFO, "loading: %s\n", filepath );
			
		paSequences[pseqdesc->seqgroup].data = Mem_Alloc( com_studiocache, filesize );
		Mem_Copy( paSequences[pseqdesc->seqgroup].data, buf, filesize );
		Mem_Free( buf );
	}
	return (mstudioanim_t *)((byte *)paSequences[pseqdesc->seqgroup].data + pseqdesc->animindex);
}

/*
====================
StudioFxTransform

====================
*/
void R_StudioFxTransform( cl_entity_t *ent, matrix3x4 transform )
{
	switch( ent->curstate.renderfx )
	{
	case kRenderFxDistort:
	case kRenderFxHologram:
		if( !Com_RandomLong( 0, 49 ))
		{
			int	axis = Com_RandomLong( 0, 1 );

			if( axis == 1 ) axis = 2; // choose between x & z
			VectorScale( transform[axis], Com_RandomFloat( 1.0f, 1.484f ), transform[axis] );
		}
		else if( !Com_RandomLong( 0, 49 ))
		{
			float offset;
			int axis = Com_RandomLong( 0, 1 );
			if( axis == 1 ) axis = 2; // choose between x & z
			offset = Com_RandomFloat( -10, 10 );
			transform[Com_RandomLong( 0, 2 )][3] += offset;
		}
		break;
	case kRenderFxExplode:
		{
			float	scale;

			scale = 1.0f + ( cl.time - ent->curstate.animtime ) * 10.0f;
			if( scale > 2 ) scale = 2; // don't blow up more than 200%

			transform[0][1] *= scale;
			transform[1][1] *= scale;
			transform[2][1] *= scale;
		}
		break;
	}
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
					value = ((a * dadt) + (b * (1 - dadt)) - 128) * (360.0f / 256.0f) + pbonecontroller[j].start;
				}
				else 
				{
					value = ((pcontroller1[i] * dadt + (pcontroller2[i]) * (1.0f - dadt))) * (360.0f / 256.0f) + pbonecontroller[j].start;
				}
			}
			else 
			{
				value = (pcontroller1[i] * dadt + pcontroller2[i] * (1.0f - dadt)) / 255.0f;
				value = bound( 0.0f, value, 1.0f );
				value = (1.0f - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			}
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
void R_StudioCalcBoneQuaterion( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, vec4_t q )
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
			if( panimvalue->num.total < panimvalue->num.valid )
				k = 0;
			
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
void R_StudioCalcBonePosition( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, vec3_t adj, vec3_t pos )
{
	mstudioanimvalue_t	*panimvalue;
	int		j, k;

	for( j = 0; j < 3; j++ )
	{
		pos[j] = pbone->value[j]; // default;

		if( panim->offset[j] != 0 )
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j]);
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
					pos[j] += (panimvalue[k+1].value * (1.0f - s) + s * panimvalue[k+2].value) * pbone->scale[j];
				else pos[j] += panimvalue[k+1].value * pbone->scale[j];
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if( panimvalue->num.total <= k + 1 )
					pos[j] += (panimvalue[panimvalue->num.valid].value * (1.0f - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[j];
				else pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
			}
		}

		if( pbone->bonecontroller[j] != -1 && adj )
			pos[j] += adj[pbone->bonecontroller[j]];
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
	s1 = 1.0f - s; // backlerp

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
StudioCalcRotations

====================
*/
void R_StudioCalcRotations( cl_entity_t *e, float pos[][3], vec4_t *q, mstudioseqdesc_t *pseqdesc, mstudioanim_t *panim, float f )
{
	int		i, frame;
	mstudiobone_t	*pbone;
	float		adj[MAXSTUDIOCONTROLLERS];
	float		s, dadt;

	if( f > pseqdesc->numframes - 1 )
	{
		f = 0.0f; // bah, fix this bug with changing sequences too fast
	}
	else if( f < -0.01f )
	{
		// BUGBUG ( somewhere else ) but this code should validate this data.
		// this could cause a crash if the frame # is negative, so we'll go ahead
		// and clamp it here
		MsgDev( D_ERROR, "StudioCalcRotations: f = %g\n", f );
		f = -0.01f;
	}

	frame = (int)f;

	dadt = R_StudioEstimateInterpolant( e );
	s = (f - frame);

	// add in programtic controllers
	pbone = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	R_StudioCalcBoneAdj( dadt, adj, e->curstate.controller, e->latched.prevcontroller, e->mouth.mouthopen );

	for( i = 0; i < m_pStudioHeader->numbones; i++, pbone++, panim++ ) 
	{
		R_StudioCalcBoneQuaterion( frame, s, pbone, panim, adj, q[i] );
		R_StudioCalcBonePosition( frame, s, pbone, panim, adj, pos[i] );
	}

	if( pseqdesc->motiontype & STUDIO_X ) pos[pseqdesc->motionbone][0] = 0.0f;
	if( pseqdesc->motiontype & STUDIO_Y ) pos[pseqdesc->motionbone][1] = 0.0f;
	if( pseqdesc->motiontype & STUDIO_Z ) pos[pseqdesc->motionbone][2] = 0.0f;

	// FIXME: enable this ? Half-Life 2 get rid of this code
	s = 0 * ((1.0f - (f - (int)(f))) / (pseqdesc->numframes)) * e->curstate.framerate;

	if( pseqdesc->motiontype & STUDIO_LX ) pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];
	if( pseqdesc->motiontype & STUDIO_LY ) pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];
	if( pseqdesc->motiontype & STUDIO_LZ ) pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];
}

/*
====================
StudioMergeBones

====================
*/
void R_StudioMergeBones( cl_entity_t *e, model_t *m_pSubModel )
{
	int		i, j;
	mstudiobone_t	*pbones;
	mstudioseqdesc_t	*pseqdesc;
	mstudioanim_t	*panim;
	matrix3x4		bonematrix;
	static vec4_t	q[MAXSTUDIOBONES];
	static float	pos[MAXSTUDIOBONES][3];
	double		f;

	if( e->curstate.sequence >=  m_pStudioHeader->numseq )
		e->curstate.sequence = 0;

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->curstate.sequence;

	f = R_StudioEstimateFrame( e, pseqdesc );

	panim = R_StudioGetAnim( m_pSubModel, pseqdesc );
	R_StudioCalcRotations( e, pos, q, pseqdesc, panim, f );
	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	for( i = 0; i < m_pStudioHeader->numbones; i++ ) 
	{
		for( j = 0; j < g_nCachedBones; j++ )
		{
			if( !com.stricmp( pbones[i].name, g_nCachedBoneNames[j] ))
			{
				Matrix3x4_Copy( g_bonestransform[i], g_rgCachedBonesTransform[j] );
				Matrix3x4_Copy( g_lighttransform[i], g_rgCachedLightTransform[j] );
				break;
			}
		}
		if( j >= g_nCachedBones )
		{
			Matrix3x4_FromOriginQuat( bonematrix, q[i], pos[i] );
			if( pbones[i].parent == -1 ) 
			{
				Matrix3x4_ConcatTransforms( g_bonestransform[i], g_rotationmatrix, bonematrix );
				Matrix3x4_Copy( g_lighttransform[i], g_bonestransform[i] );

				// apply client-side effects to the transformation matrix
				R_StudioFxTransform( e, g_bonestransform[i] );
			} 
			else 
			{
				Matrix3x4_ConcatTransforms( g_bonestransform[i], g_bonestransform[pbones[i].parent], bonematrix );
				Matrix3x4_ConcatTransforms( g_lighttransform[i], g_lighttransform[pbones[i].parent], bonematrix );
			}
		}
	}
}

/*
====================
StudioSetupBones

====================
*/
void R_StudioSetupBones( cl_entity_t *e )
{
	double		f;
	mstudiobone_t	*pbones;
	mstudioseqdesc_t	*pseqdesc;
	mstudioanim_t	*panim;
	matrix3x4		bonematrix;
	static vec3_t	pos[MAXSTUDIOBONES];
	static vec4_t	q[MAXSTUDIOBONES];
	static vec3_t	pos2[MAXSTUDIOBONES];
	static vec4_t	q2[MAXSTUDIOBONES];
	static vec3_t	pos3[MAXSTUDIOBONES];
	static vec4_t	q3[MAXSTUDIOBONES];
	static vec3_t	pos4[MAXSTUDIOBONES];
	static vec4_t	q4[MAXSTUDIOBONES];
	int		i;

	if( e->curstate.sequence >= m_pStudioHeader->numseq )
		e->curstate.sequence = 0;

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->curstate.sequence;

	f = R_StudioEstimateFrame( e, pseqdesc );

	panim = R_StudioGetAnim( e->model, pseqdesc );
	R_StudioCalcRotations( e, pos, q, pseqdesc, panim, f );

	if( pseqdesc->numblends > 1 )
	{
		float	s;
		float	dadt;

		panim += m_pStudioHeader->numbones;
		R_StudioCalcRotations( e, pos2, q2, pseqdesc, panim, f );

		dadt = R_StudioEstimateInterpolant( e );
		s = (e->curstate.blending[0] * dadt + e->latched.prevblending[0] * (1.0f - dadt)) / 255.0f;

		R_StudioSlerpBones( q, pos, q2, pos2, s );

		if( pseqdesc->numblends == 4 )
		{
			panim += m_pStudioHeader->numbones;
			R_StudioCalcRotations( e, pos3, q3, pseqdesc, panim, f );

			panim += m_pStudioHeader->numbones;
			R_StudioCalcRotations( e, pos4, q4, pseqdesc, panim, f );

			s = (e->curstate.blending[0] * dadt + e->latched.prevblending[0] * (1.0f - dadt)) / 255.0f;
			R_StudioSlerpBones( q3, pos3, q4, pos4, s );

			s = (e->curstate.blending[1] * dadt + e->latched.prevblending[1] * (1.0f - dadt)) / 255.0f;
			R_StudioSlerpBones( q, pos, q3, pos3, s );
		}
	}

	if( m_fDoInterp && e->latched.sequencetime && ( e->latched.sequencetime + 0.2f > cl.time) && ( e->latched.prevsequence < m_pStudioHeader->numseq ))
	{
		// blend from last sequence
		static vec3_t	pos1b[MAXSTUDIOBONES];
		static vec4_t	q1b[MAXSTUDIOBONES];
		float		s;

		pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->latched.prevsequence;
		panim = R_StudioGetAnim( e->model, pseqdesc );

		// clip prevframe
		R_StudioCalcRotations( e, pos1b, q1b, pseqdesc, panim, e->latched.prevframe );

		if( pseqdesc->numblends > 1 )
		{
			panim += m_pStudioHeader->numbones;
			R_StudioCalcRotations( e, pos2, q2, pseqdesc, panim, e->latched.prevframe );

			s = (e->latched.prevseqblending[0]) / 255.0f;
			R_StudioSlerpBones( q1b, pos1b, q2, pos2, s );

			if( pseqdesc->numblends == 4 )
			{
				panim += m_pStudioHeader->numbones;
				R_StudioCalcRotations( e, pos3, q3, pseqdesc, panim, e->latched.prevframe );

				panim += m_pStudioHeader->numbones;
				R_StudioCalcRotations( e, pos4, q4, pseqdesc, panim, e->latched.prevframe );

				s = (e->latched.prevseqblending[0]) / 255.0f;
				R_StudioSlerpBones( q3, pos3, q4, pos4, s );

				s = (e->latched.prevseqblending[1]) / 255.0f;
				R_StudioSlerpBones( q1b, pos1b, q3, pos3, s );
			}
		}

		s = 1.0f - ( cl.time - e->latched.sequencetime ) / 0.2f;
		R_StudioSlerpBones( q, pos, q1b, pos1b, s );
	}
	else
	{
		// store prevframe otherwise
		e->latched.prevframe = f;
	}

	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	// calc gait animation
	if( m_pPlayerInfo && m_pPlayerInfo->gaitsequence != 0 )
	{
		if( m_pPlayerInfo->gaitsequence >= m_pStudioHeader->numseq ) 
			m_pPlayerInfo->gaitsequence = 0;

		pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pPlayerInfo->gaitsequence;

		panim = R_StudioGetAnim( e->model, pseqdesc );
		R_StudioCalcRotations( e, pos2, q2, pseqdesc, panim, m_pPlayerInfo->gaitframe );

		for( i = 0; i < m_pStudioHeader->numbones; i++ )
		{
			if( !com.strcmp( pbones[i].name, "Bip01 Spine" ))
				break;
			VectorCopy( pos2[i], pos[i] );
			Vector4Copy( q2[i], q[i] );
		}
	}

	for( i = 0; i < m_pStudioHeader->numbones; i++ ) 
	{
		Matrix3x4_FromOriginQuat( bonematrix, q[i], pos[i] );

		if( pbones[i].parent == -1 ) 
		{
			Matrix3x4_ConcatTransforms( g_bonestransform[i], g_rotationmatrix, bonematrix );

			// apply client-side effects to the transformation matrix
			R_StudioFxTransform( e, g_bonestransform[i] );
		} 
		else Matrix3x4_ConcatTransforms( g_bonestransform[i], g_bonestransform[pbones[i].parent], bonematrix );
	}
}

/*
====================
StudioSaveBones

====================
*/
static void R_StudioSaveBones( void )
{
	mstudiobone_t	*pbones;
	int		i;

	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	g_nCachedBones = m_pStudioHeader->numbones;

	for( i = 0; i < m_pStudioHeader->numbones; i++ ) 
	{
		strcpy( g_nCachedBoneNames[i], pbones[i].name );
		Matrix3x4_Copy( g_rgCachedBonesTransform[i], g_bonestransform[i] );
		Matrix3x4_Copy( g_rgCachedLightTransform[i], g_lighttransform[i] );
	}
}

/*
====================
StudioCalcAttachments

====================
*/
static void R_StudioCalcAttachments( void )
{
	mstudioattachment_t	*pAtt;
	vec3_t		forward, bonepos;
	vec3_t		localOrg, localAng;
	int		i;

	if( m_pStudioHeader->numattachments <= 0 )
	{
		// clear attachments
		for( i = 0; i < MAXSTUDIOATTACHMENTS; i++ )
			VectorClear( RI.currententity->attachment[i] );
		return;
	}
	else if( m_pStudioHeader->numattachments > MAXSTUDIOATTACHMENTS )
	{
		m_pStudioHeader->numattachments = MAXSTUDIOATTACHMENTS; // reduce it
		MsgDev( D_WARN, "R_StudioCalcAttahments: too many attachments on %s\n", RI.currentmodel->name );
	}

	// calculate attachment points
	pAtt = (mstudioattachment_t *)((byte *)m_pStudioHeader + m_pStudioHeader->attachmentindex);

	for( i = 0; i < m_pStudioHeader->numattachments; i++ )
	{
		Matrix3x4_VectorTransform( g_lighttransform[pAtt[i].bone], pAtt[i].org, localOrg );
		VectorAdd( RI.currententity->origin, localOrg, RI.currententity->attachment[i] );
		Matrix4x4_OriginFromMatrix( g_lighttransform[pAtt[i].bone], bonepos );
		VectorSubtract( localOrg, bonepos, forward );	// make forward
		VectorNormalizeFast( forward );
		VectorAngles( forward, localAng );

		// FIXME: store attachment angles
	}
}

/*
===============
pfnStudioSetupModel

===============
*/
static void R_StudioSetupModel( int bodypart, void **ppbodypart, void **ppsubmodel )
{
	int	index;

	if( bodypart > m_pStudioHeader->numbodyparts )
		bodypart = 0;

	m_pBodyPart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + bodypart;

	index = RI.currententity->curstate.body / m_pBodyPart->base;
	index = index % m_pBodyPart->nummodels;

	m_pSubModel = (mstudiomodel_t *)((byte *)m_pStudioHeader + m_pBodyPart->modelindex) + index;

	*ppbodypart = m_pBodyPart;
	*ppsubmodel = m_pSubModel;
}

/*
===============
R_StudioCheckBBox

===============
*/
static int R_StudioCheckBBox( void )
{
	if( R_CullStudioModel( RI.currententity ))
		return false;
	return true;
}

/*
===============
R_StudioDynamicLight

===============
*/
void R_StudioDynamicLight( cl_entity_t *ent, alight_t *plight )
{
	// FIXME: implement
}

/*
===============
pfnStudioEntityLight

===============
*/
void R_StudioEntityLight( alight_t *plight )
{
	// FIXME: implement
}

/*
===============
R_StudioSetupLighting

===============
*/
void R_StudioSetupLighting( alight_t *plighting )
{
	// FIXME: implement
}

/*
===============
R_StudioSetupSkin

FIXME: this is correct ?
===============
*/
static void R_StudioSetupSkin( mstudiotexture_t *ptexture, int index )
{
	short	*pskinref;
	int	m_skinnum;

	m_skinnum = RI.currententity->curstate.skin;
	pskinref = (short *)((byte *)m_pStudioHeader + m_pStudioHeader->skinindex);
	if( m_skinnum != 0 && m_skinnum < m_pStudioHeader->numskinfamilies )
		pskinref += (m_skinnum * m_pStudioHeader->numskinref);

	GL_Bind( GL_TEXTURE0, ptexture[pskinref[index]].index );
}

/*
===============
R_StudioDrawPoints

===============
*/
static void R_StudioDrawPoints( void )
{
	int		i, j, flags, m_skinnum;
	byte		*pvertbone;
	byte		*pnormbone;
	vec3_t		*pstudioverts;
	vec3_t		*pstudionorms;
	mstudiotexture_t	*ptexture;
	mstudiomesh_t	*pmesh;
	short		*pskinref;
	float		*av, *lv;

	m_skinnum = RI.currententity->curstate.skin;	    
	pvertbone = ((byte *)m_pStudioHeader + m_pSubModel->vertinfoindex);
	pnormbone = ((byte *)m_pStudioHeader + m_pSubModel->norminfoindex);
	ptexture = (mstudiotexture_t *)((byte *)m_pStudioHeader + m_pStudioHeader->textureindex);

	pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex);
	pstudioverts = (vec3_t *)((byte *)m_pStudioHeader + m_pSubModel->vertindex);
	pstudionorms = (vec3_t *)((byte *)m_pStudioHeader + m_pSubModel->normindex);

	pskinref = (short *)((byte *)m_pStudioHeader + m_pStudioHeader->skinindex);
	if( m_skinnum != 0 && m_skinnum < m_pStudioHeader->numskinfamilies )
		pskinref += (m_skinnum * m_pStudioHeader->numskinref);

	for( i = 0; i < m_pSubModel->numverts; i++ )
		Matrix3x4_VectorTransform( g_bonestransform[pvertbone[i]], pstudioverts[i], g_xformverts[i] );

//	for( i = 0; i < m_pSubModel->numnorms; i++ )
//		Matrix3x4_VectorRotate( g_bonestransform[pnormbone[i]], pstudionorms[i], g_xformnorms[i] );

	for( j = 0; j < m_pSubModel->nummesh; j++ ) 
	{
		float	s, t;
		short	*ptricmds;

		pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + j;
		ptricmds = (short *)((byte *)m_pStudioHeader + pmesh->triindex);

		s = 1.0f / (float)ptexture[pskinref[pmesh->skinref]].width;
		t = 1.0f / (float)ptexture[pskinref[pmesh->skinref]].height;

		if( ptexture[pskinref[pmesh->skinref]].index < 0 || ptexture[pskinref[pmesh->skinref]].index > MAX_TEXTURES )
			ptexture[pskinref[pmesh->skinref]].index = tr.defaultTexture;

		GL_Bind( GL_TEXTURE0, ptexture[pskinref[pmesh->skinref]].index );

		while( i = *( ptricmds++ ))
		{
			if( i < 0 )
			{
				pglBegin( GL_TRIANGLE_FAN );
				i = -i;
			}
			else
			{
				pglBegin( GL_TRIANGLE_STRIP );
			}

			for(; i > 0; i--, ptricmds += 4)
			{
//				if (flags & STUDIO_NF_CHROME)
//					pglTexCoord2f(g_chrome[ptricmds[1]][0]*s, g_chrome[ptricmds[1]][1]*t);
//				else
					pglTexCoord2f(ptricmds[2]*s, ptricmds[3]*t);
				av = g_xformverts[ptricmds[0]];
				pglVertex3f( av[0], av[1], av[2] );
			}
			pglEnd();
		}
	}
}

/*
===============
R_StudioDrawHulls

===============
*/
static void R_StudioDrawHulls( void )
{
	// FIXME: implement
}

/*
===============
R_StudioDrawAbsBBox

===============
*/
static void R_StudioDrawAbsBBox( void )
{
	// FIXME: implement
}

/*
===============
R_StudioDrawBones

===============
*/
static void R_StudioDrawBones( void )
{
	// FIXME: implement
}

/*
===============
R_StudioSetRemapColors

===============
*/
void R_StudioSetRemapColors( int top, int bottom )
{
	// FIXME: implement
}

/*
===============
R_StudioSetupPlayerModel

===============
*/
static model_t *R_StudioSetupPlayerModel( int index )
{
	player_info_t	*info;
	string		modelpath;
	int		modelIndex;

	if( index < 0 || index > cl.maxclients )
		return NULL; // bad client ?

	info = &cl.players[index];

	com.snprintf( modelpath, sizeof( modelpath ), "models/player/%s/%s.mdl", info->model, info->model );
	modelIndex = CL_FindModelIndex( modelpath );	

	return CM_ClipHandleToModel( modelIndex );
}

/*
===============
R_StudioClientEvents

===============
*/
static void R_StudioClientEvents( void )
{
	mstudioseqdesc_t	*pseqdesc;
	mstudioevent_t	*pevent;
	float		flEventFrame;
	qboolean		bLooped = false;
	cl_entity_t	*e = RI.currententity;
	int		i;

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->curstate.sequence;
	pevent = (mstudioevent_t *)((byte *)m_pStudioHeader + pseqdesc->eventindex);

	// curstate.frame not used for viewmodel animating
	flEventFrame = e->latched.prevframe;

	if( pseqdesc->numevents == 0 )
		return;

	if( e->syncbase == -0.01f )
	{
		flEventFrame = 0.0f;
		//Msg( "Sequence changed\n" );
	}

	// stalled?
	if( flEventFrame == e->syncbase )
		return;

	//Msg( "(seq %d cycle %.3f ) evframe %.3f prevevframe %.3f (time %.3f)\n", e->curstate.sequence, e->latched.prevframe, flEventFrame, e->syncbase, cl.time );

	// check for looping
	if( flEventFrame <= e->syncbase )
	{
		if( e->syncbase - flEventFrame > 0.5f )
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
			if(( pevent[i].frame > e->syncbase || pevent[i].frame <= flEventFrame ))
			{
				//Msg( "FE %i Looped frame %i, prev %f ev %f (time %.3f)\n", pevent[i].event, pevent[i].frame, e->syncbase, flEventFrame, cl.time );
				clgame.dllFuncs.pfnStudioEvent( &pevent[i], e );
			}
		}
		else
		{
			if(( pevent[i].frame > e->syncbase && pevent[i].frame <= flEventFrame ))
			{
				//Msg( "FE %i Normal frame %i, prev %f ev %f (time %.3f)\n", pevent[i].event, pevent[i].frame, e->syncbase, flEventFrame, cl.time );
				clgame.dllFuncs.pfnStudioEvent( &pevent[i], e );
			}
		}
	}

	e->syncbase = flEventFrame;
}

/*
===============
R_StudioGetForceFaceFlags

===============
*/
int R_StudioGetForceFaceFlags( void )
{
	return g_nFaceFlags;
}

/*
===============
R_StudioSetForceFaceFlags

===============
*/
void R_StudioSetForceFaceFlags( int flags )
{
	g_nFaceFlags = flags;
}

/*
===============
pfnStudioSetHeader

===============
*/
void R_StudioSetHeader( studiohdr_t *pheader )
{
	m_pStudioHeader = pheader;
}

/*
===============
R_StudioSetRenderModel

===============
*/
void R_StudioSetRenderModel( model_t *model )
{
	RI.currentmodel = model;
}

/*
===============
R_StudioSetupRenderer

===============
*/
static void R_StudioSetupRenderer( int rendermode )
{
	GL_SetRenderMode( rendermode );
}

/*
===============
R_StudioRestoreRenderer

===============
*/
static void R_StudioRestoreRenderer( void )
{
	GL_SetRenderMode( kRenderNormal );
}

/*
===============
R_StudioSetChromeOrigin

===============
*/
void R_StudioSetChromeOrigin( void )
{
	// FIXME: implement
}

/*
===============
pfnIsHardware

Xash3D is always works in hadrware mode
===============
*/
static int pfnIsHardware( void )
{
	return true;
}
	
/*
===============
GL_StudioDrawShadow

===============
*/
static void GL_StudioDrawShadow( void )
{
	// in GoldSrc shadow call is dsiabled with 'return' at start of the function
	// some mods used a hack with calling DrawShadow ahead of 'return'
	// this code is for HL compatibility.
	return;

	// FIXME: implement
	MsgDev( D_INFO, "GL_StudioDrawShadow()\n" );	// just a debug
}

/*
====================
StudioRenderFinal

====================
*/
void R_StudioRenderFinal( void )
{
	int	i, rendermode;

	rendermode = R_StudioGetForceFaceFlags() ? kRenderTransAdd : RI.currententity->curstate.rendermode;
	R_StudioSetupRenderer( rendermode );
	
	if( r_drawentities->integer == 2 )
	{
		R_StudioDrawBones();
	}
	else if( r_drawentities->integer == 3 )
	{
		R_StudioDrawHulls();
	}
	else
	{
		for( i = 0; i < m_pStudioHeader->numbodyparts; i++ )
		{
			R_StudioSetupModel( i, (void **)&m_pBodyPart, (void **)&m_pSubModel );

			if( m_fDoInterp )
			{
				// interpolation messes up bounding boxes.
				RI.currententity->trivial_accept = 0; 
			}

			GL_SetRenderMode( rendermode );
			R_StudioDrawPoints();
			GL_StudioDrawShadow();
		}
	}

	if( r_drawentities->integer == 4 )
	{
		GL_SetRenderMode( kRenderTransAdd );
		R_StudioDrawHulls( );
		GL_SetRenderMode( kRenderNormal );
	}

	R_StudioRestoreRenderer();
}

/*
====================
StudioRenderModel

====================
*/
void R_StudioRenderModel( void )
{
	R_StudioSetChromeOrigin();
	R_StudioSetForceFaceFlags( 0 );

	if( RI.currententity->curstate.renderfx == kRenderFxGlowShell )
	{
		RI.currententity->curstate.renderfx = kRenderFxNone;

		R_StudioRenderFinal( );

		R_StudioSetForceFaceFlags( STUDIO_NF_CHROME );
		TriSpriteTexture( R_GetChromeSprite(), 0 );
		RI.currententity->curstate.renderfx = kRenderFxGlowShell;

		R_StudioRenderFinal( );
	}
	else
	{
		R_StudioRenderFinal( );
	}
}

/*
===============
R_StudioDrawPlayer

===============
*/
static int R_StudioDrawPlayer( int flags, entity_state_t *pplayer )
{
	return 0;
}

/*
===============
R_StudioDrawModel

===============
*/
static int R_StudioDrawModel( int flags )
{
	alight_t	lighting;
	vec3_t	dir;

	if( RI.currententity->curstate.renderfx == kRenderFxDeadPlayer )
	{
		entity_state_t	deadplayer;
		int		save_interp;
		int		result;

		if( RI.currententity->curstate.renderamt <= 0 || RI.currententity->curstate.renderamt > cl.maxclients )
			return 0;

		// get copy of player
		deadplayer = *R_StudioGetPlayerState( RI.currententity->curstate.renderamt - 1 );

		// clear weapon, movement state
		deadplayer.number = RI.currententity->curstate.renderamt;
		deadplayer.weaponmodel = 0;
		deadplayer.gaitsequence = 0;

		deadplayer.movetype = MOVETYPE_NONE;
		VectorCopy( RI.currententity->curstate.angles, deadplayer.angles );
		VectorCopy( RI.currententity->curstate.origin, deadplayer.origin );

		save_interp = m_fDoInterp;
		m_fDoInterp = 0;
		
		// draw as though it were a player
		result = R_StudioDrawPlayer( flags, &deadplayer );
		
		m_fDoInterp = save_interp;
		return result;
	}

	m_pStudioHeader = (studiohdr_t *)Mod_Extradata( RI.currentmodel );

	R_StudioSetUpTransform( RI.currententity );

	if( flags & STUDIO_RENDER )
	{
		// see if the bounding box lets us trivially reject, also sets
		if( !R_StudioCheckBBox( ))
			return 0;

		r_stats.c_studio_models_drawn++;
		r_stats.c_studio_models_count++; // render data cache cookie

		if( m_pStudioHeader->numbodyparts == 0 )
			return 1;
	}

	if( RI.currententity->curstate.movetype == MOVETYPE_FOLLOW )
		R_StudioMergeBones( RI.currententity, RI.currentmodel );
	else R_StudioSetupBones( RI.currententity );

	R_StudioSaveBones();

	if( flags & STUDIO_EVENTS )
	{
		R_StudioCalcAttachments( );
		R_StudioClientEvents( );

		// copy attachments into global entity array
		if( RI.currententity->index > 0 )
		{
			cl_entity_t *ent = CL_GetEntityByIndex( RI.currententity->index );
			Mem_Copy( ent->attachment, RI.currententity->attachment, sizeof( vec3_t ) * 4 );
		}
	}

	if( flags & STUDIO_RENDER )
	{
		lighting.plightvec = dir;
		R_StudioDynamicLight( RI.currententity, &lighting );

		R_StudioEntityLight( &lighting );

		// model and frame independant
		R_StudioSetupLighting( &lighting );

		// get remap colors
		g_nTopColor = RI.currententity->curstate.colormap & 0xFF;
		g_nBottomColor = (RI.currententity->curstate.colormap & 0xFF00) >> 8;

		R_StudioSetRemapColors( g_nTopColor, g_nBottomColor );

//		R_StudioRenderModel( RI.currententity );
	}

	return 1;
}

/*
=================
R_DrawStudioModel
=================
*/
void R_DrawStudioModel( cl_entity_t *e )
{
	int	flags, result;

	ASSERT( pStudioDraw != NULL );

	if( e == &clgame.viewent )
		flags = STUDIO_RENDER;	
	else flags = STUDIO_RENDER|STUDIO_EVENTS;

	if( e == &clgame.viewent )
		m_fDoInterp = true;	// viewmodel can't properly animate without lerping
	else if( r_studio_lerping->integer )
		m_fDoInterp = (e->curstate.effects & EF_NOINTERP) ? false : true;
	else m_fDoInterp = false;

	// select the properly method
	if( e->player )
		result = pStudioDraw->StudioDrawPlayer( flags, &cl.frame.playerstate[e->index-1] );
	else result = pStudioDraw->StudioDrawModel( flags );
}

/*
=================
R_RunViewmodelEvents
=================
*/
void R_RunViewmodelEvents( void )
{
	RI.currententity = &clgame.viewent;
	RI.currentmodel = RI.currententity->model;
	if( !RI.currentmodel ) return;

	R_StudioDrawModel( STUDIO_EVENTS );

	RI.currententity = NULL;
	RI.currentmodel = NULL;
}

/*
====================
R_StudioLoadTexture

load model texture with unique name
====================
*/
static void R_StudioLoadTexture( model_t *mod, studiohdr_t *phdr, mstudiotexture_t *ptexture )
{
	size_t	size;
	int	flags = 0;
	char	texname[64], name[64];

	if( ptexture->flags & STUDIO_NF_TRANSPARENT )
		flags |= (TF_CLAMP|TF_NOMIPMAP);

	if( ptexture->flags & ( STUDIO_NF_NORMALMAP|STUDIO_NF_HEIGHTMAP ))
		flags |= TF_NORMALMAP;

	// NOTE: replace index with pointer to start of imagebuffer, ImageLib expected it
	ptexture->index = (int)((byte *)phdr) + ptexture->index;
	size = sizeof( mstudiotexture_t ) + ptexture->width * ptexture->height + 768;
	if( !model_name[0] ) FS_FileBase( mod->name, model_name );
	FS_FileBase( ptexture->name, name );

	// build the texname
	com.snprintf( texname, sizeof( texname ), "%s/%s.mdl", model_name, name );
	ptexture->index = GL_LoadTexture( texname, (byte *)ptexture, size, flags );

	if( !ptexture->index )
	{
		MsgDev( D_WARN, "%s has null texture %s\n", mod->name, ptexture->name );
		ptexture->index = tr.defaultTexture;
	}
	else
	{
		GL_SetTextureType( ptexture->index, TEX_STUDIO );
	}
}

/*
=================
R_StudioLoadHeader
=================
*/
studiohdr_t *R_StudioLoadHeader( model_t *mod, const void *buffer )
{
	byte		*pin;
	studiohdr_t	*phdr;
	mstudiotexture_t	*ptexture;
	int		i;

	if( !buffer ) return NULL;

	pin = (byte *)buffer;
	phdr = (studiohdr_t *)pin;
	i = phdr->version;

	if( i != STUDIO_VERSION )
	{
		MsgDev( D_ERROR, "%s has wrong version number (%i should be %i)\n", mod->name, i, STUDIO_VERSION );
		return NULL;
	}	

	model_name[0] = '\0';

	ptexture = (mstudiotexture_t *)(((byte *)phdr) + phdr->textureindex);
	if( phdr->textureindex > 0 && phdr->numtextures <= MAXSTUDIOSKINS )
	{
		for( i = 0; i < phdr->numtextures; i++ )
			R_StudioLoadTexture( mod, phdr, &ptexture[i] );
	}
	return (studiohdr_t *)buffer;
}

/*
=================
Mod_LoadStudioModel
=================
*/
void Mod_LoadStudioModel( model_t *mod, const void *buffer )
{
	studiohdr_t	*phdr;

	phdr = R_StudioLoadHeader( mod, buffer );
	if( !phdr ) return;	// bad model

	loadmodel->mempool = Mem_AllocPool( va("^2%s^7", loadmodel->name ));

	if( phdr->numtextures == 0 )
	{
		studiohdr_t	*thdr;
		byte		*in, *out;
		void		*buffer2 = NULL;
		size_t		size1, size2;
	
		buffer2 = FS_LoadFile( R_StudioTexName( mod ), NULL );
		thdr = R_StudioLoadHeader( mod, buffer2 );

		if( !thdr )
		{
			MsgDev( D_ERROR, "Mod_LoadStudioModel: %s missing textures file\n", mod->name ); 
			if( buffer2 ) Mem_Free( buffer2 );
			return; // there were problems
		}

		// give space for textures and skinrefs
		size1 = thdr->numtextures * sizeof( mstudiotexture_t );
		size2 = thdr->numskinfamilies * thdr->numskinref * sizeof( short );
		mod->cache.data = Mem_Alloc( loadmodel->mempool, phdr->length + size1 + size2 );
		Mem_Copy( loadmodel->cache.data, buffer, phdr->length ); // copy main mdl buffer
		phdr = (studiohdr_t *)loadmodel->cache.data; // get the new pointer on studiohdr
		phdr->numskinfamilies = thdr->numskinfamilies;
		phdr->numtextures = thdr->numtextures;
		phdr->numskinref = thdr->numskinref;
		phdr->textureindex = phdr->length;
		phdr->skinindex = phdr->textureindex + size1;

		in = (byte *)thdr + thdr->textureindex;
		out = (byte *)phdr + phdr->textureindex;
		Mem_Copy( out, in, size1 + size2 );	// copy textures + skinrefs
		phdr->length += size1 + size2;
		Mem_Free( buffer2 ); // release T.mdl
	}
	else
	{
		// NOTE: we wan't keep raw textures in memory. just cutoff model pointer above texture base
		loadmodel->cache.data = Mem_Alloc( loadmodel->mempool, phdr->texturedataindex );
		Mem_Copy( loadmodel->cache.data, buffer, phdr->texturedataindex );
		phdr->length = phdr->texturedataindex;	// update model size
	}

	// setup bounding box
	VectorCopy( phdr->bbmin, loadmodel->mins );
	VectorCopy( phdr->bbmax, loadmodel->maxs );
	loadmodel->type = mod_studio;	// all done

	loadmodel->numframes = R_StudioBodyVariations( loadmodel );
}

/*
=================
Mod_UnloadStudioModel
=================
*/
void Mod_UnloadStudioModel( model_t *mod )
{
	studiohdr_t	*pstudio;
	mstudiotexture_t	*ptexture;
	int		i;

	ASSERT( mod != NULL );

	if( mod->type != mod_studio )
		return; // not a studio

	pstudio = mod->cache.data;
	if( !pstudio ) return; // already freed

	ptexture = (mstudiotexture_t *)(((byte *)pstudio) + pstudio->textureindex);

	// release all textures
	for( i = 0; i < pstudio->numtextures; i++ )
	{
		if( ptexture[i].index == tr.defaultTexture )
			continue;
		GL_FreeTexture( ptexture[i].index );
	}

	Mem_FreePool( &mod->mempool );
	Mem_Set( mod, 0, sizeof( *mod ));
}
		
static engine_studio_api_t gStudioAPI =
{
	Mod_Calloc,
	Mod_CacheCheck,
	Mod_LoadCacheFile,
	Mod_ForName,
	Mod_Extradata,
	CM_ClipHandleToModel,
	pfnGetCurrentEntity,
	pfnPlayerInfo,
	R_StudioGetPlayerState,
	pfnGetViewEntity,
	pfnGetEngineTimes,
	pfnCVarGetPointer,
	pfnGetViewInfo,
	R_GetChromeSprite,
	pfnGetModelCounters,
	pfnGetAliasScale,
	pfnStudioGetBoneTransform,
	pfnStudioGetLightTransform,
	pfnStudioGetAliasTransform,
	pfnStudioGetRotationMatrix,
	R_StudioSetupModel,
	R_StudioCheckBBox,
	R_StudioDynamicLight,
	R_StudioEntityLight,
	R_StudioSetupLighting,
	R_StudioDrawPoints,
	R_StudioDrawHulls,
	R_StudioDrawAbsBBox,
	R_StudioDrawBones,
	R_StudioSetupSkin,
	R_StudioSetRemapColors,
	R_StudioSetupPlayerModel,
	R_StudioClientEvents,
	R_StudioGetForceFaceFlags,
	R_StudioSetForceFaceFlags,
	R_StudioSetHeader,
	R_StudioSetRenderModel,
	R_StudioSetupRenderer,
	R_StudioRestoreRenderer,
	R_StudioSetChromeOrigin,
	pfnIsHardware,
	GL_StudioDrawShadow,
	GL_SetRenderMode,
};

static r_studio_interface_t gStudioDraw =
{
	STUDIO_INTERFACE_VERSION,
	R_StudioDrawModel,
	R_StudioDrawPlayer,
};

/*
===============
CL_InitStudioAPI

Initialize client studio
===============
*/
qboolean CL_InitStudioAPI( void )
{
	pStudioDraw = &gStudioDraw;

	// external renderer temporary disabled
	return true;

	// Xash will be used internal StudioModelRenderer
	if( !clgame.dllFuncs.pfnGetStudioModelInterface )
		return true;

	if( clgame.dllFuncs.pfnGetStudioModelInterface( STUDIO_INTERFACE_VERSION, &pStudioDraw, &gStudioAPI ))
		return true;

	// NOTE: we always return true even if game interface was not correct
	// because we need Draw our StudioModels
	// just restore pointer to builtin function
	pStudioDraw = &gStudioDraw;

	return true;
}