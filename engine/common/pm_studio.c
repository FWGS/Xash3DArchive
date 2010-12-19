//=======================================================================
//			Copyright XashXT Group 2010 �
//		        pm_studio.c - stduio models tracing
//=======================================================================

#include "common.h"
#include "studio.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "cm_local.h"
#include "pm_local.h"
#include "world.h"

static studiohdr_t	*pm_studiohdr;
static mplane_t	pm_hitboxplanes[6];	// there a temp hitbox
static matrix4x4	pm_studiomatrix;
static matrix4x4	pm_studiobones[MAXSTUDIOBONES];
typedef qboolean 	(*pfnTrace)( pmtrace_t *trace );
static float	trace_realfraction;
static vec3_t	trace_startmins, trace_endmins;
static vec3_t	trace_startmaxs, trace_endmaxs;
static vec3_t	trace_absmins, trace_absmaxs;

/*
====================
PM_InitStudioHull
====================
*/
void PM_InitStudioHull( void )
{
	int	i, side;
	mplane_t	*p;

	for( i = 0; i < 6; i++ )
	{
		side = i & 1;

		// planes
		p = &pm_hitboxplanes[i];
		VectorClear( p->normal );

		if( side )
		{
			p->type = PLANE_NONAXIAL;
			p->normal[i>>1] = -1.0f;
			p->signbits = (1<<(i>>1));
		}
		else
		{
			p->type = i>>1;
			p->normal[i>>1] = 1.0f;
			p->signbits = 0;
		}
	}    
}

/*
====================
PM_HullForHitbox
====================
*/
static void PM_HullForHitbox( const vec3_t mins, const vec3_t maxs )
{
	pm_hitboxplanes[0].dist = maxs[0];
	pm_hitboxplanes[1].dist = -mins[0];
	pm_hitboxplanes[2].dist = maxs[1];
	pm_hitboxplanes[3].dist = -mins[1];
	pm_hitboxplanes[4].dist = maxs[2];
	pm_hitboxplanes[5].dist = -mins[2];
}

/*
===============================================================================

	STUDIO MODELS TRACING

===============================================================================
*/
/*
====================
StudioSetUpTransform
====================
*/
static void PM_StudioSetUpTransform( physent_t *pe )
{
	float	*ang, *org;
	float	scale = 1.0f;

	org = pe->origin;
	ang = pe->angles;

	// FIXME: apply scale to studiomodels
	Matrix4x4_CreateFromEntity( pm_studiomatrix, org[0], org[1], org[2], -ang[PITCH], ang[YAW], ang[ROLL], scale );
}

/*
====================
StudioCalcBoneAdj

====================
*/
static void PM_StudioCalcBoneAdj( float dadt, float *adj, const byte *pcontroller1, const byte *pcontroller2 )
{
	int			i, j;
	float			value;
	mstudiobonecontroller_t	*pbonecontroller;
	
	pbonecontroller = (mstudiobonecontroller_t *)((byte *)pm_studiohdr + pm_studiohdr->bonecontrollerindex);

	for( j = 0; j < pm_studiohdr->numbonecontrollers; j++ )
	{
		i = pbonecontroller[j].index;

		if( i == 4 ) continue; // ignore mouth
		if( i <= MAXSTUDIOCONTROLLERS )
		{
			// check for 360% wrapping
			if( pbonecontroller[j].type & STUDIO_RLOOP )
			{
				if( abs( pcontroller1[i] - pcontroller2[i] ) > 128 )
				{
					int	a, b;

					a = (pcontroller1[j] + 128) % 256;
					b = (pcontroller2[j] + 128) % 256;
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
				if( value < 0 ) value = 0;
				if( value > 1.0 ) value = 1.0;
				value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
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
static void PM_StudioCalcBoneQuaterion( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *q )
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
				// DEBUG
				if( panimvalue->num.total < panimvalue->num.valid )
					k = 0;
			}
			// Bah, missing blend!
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
static void PM_StudioCalcBonePosition( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *pos )
{
	int		j, k;
	mstudioanimvalue_t	*panimvalue;

	for( j = 0; j < 3; j++ )
	{
		pos[j] = pbone->value[j]; // default;
		if( panim->offset[j] != 0.0f )
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

  				// DEBUG
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
					pos[j] += (panimvalue[panimvalue->num.valid].value * (1.0f - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[j];
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
StudioCalcRotations

====================
*/
static void PM_StudioCalcRotations( physent_t *pe, float pos[][3], vec4_t *q, mstudioseqdesc_t *pseqdesc, mstudioanim_t *panim, float f )
{
	int		i, frame;
	mstudiobone_t	*pbone;
	float		adj[MAXSTUDIOCONTROLLERS];
	float		s, dadt = 1.0f; // noInterp

	if( f > pseqdesc->numframes - 1 )
		f = 0;
	else if( f < -0.01f )
		f = -0.01f;

	frame = (int)f;
	s = (f - frame);

	// add in programtic controllers
	pbone = (mstudiobone_t *)((byte *)pm_studiohdr + pm_studiohdr->boneindex);

	PM_StudioCalcBoneAdj( dadt, adj, pe->controller, pe->controller );

	for( i = 0; i < pm_studiohdr->numbones; i++, pbone++, panim++ ) 
	{
		PM_StudioCalcBoneQuaterion( frame, s, pbone, panim, adj, q[i] );
		PM_StudioCalcBonePosition( frame, s, pbone, panim, adj, pos[i] );
	}

	if( pseqdesc->motiontype & STUDIO_X ) pos[pseqdesc->motionbone][0] = 0.0f;
	if( pseqdesc->motiontype & STUDIO_Y ) pos[pseqdesc->motionbone][1] = 0.0f;
	if( pseqdesc->motiontype & STUDIO_Z ) pos[pseqdesc->motionbone][2] = 0.0f;

	s = 0 * ((1.0 - (f - (int)(f))) / (pseqdesc->numframes)) * 1.0f; // framerate

	if( pseqdesc->motiontype & STUDIO_LX ) pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];
	if( pseqdesc->motiontype & STUDIO_LY ) pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];
	if( pseqdesc->motiontype & STUDIO_LZ ) pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];
}

/*
====================
StudioEstimateFrame

====================
*/
static float PM_StudioEstimateFrame( physent_t *pe, mstudioseqdesc_t *pseqdesc )
{
	double	f;
	
	if( pseqdesc->numframes <= 1 )
		f = 0;
	else f = (pe->frame * (pseqdesc->numframes - 1)) / 256.0;
 
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
StudioSlerpBones

====================
*/
static void PM_StudioSlerpBones( vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s )
{
	int	i;
	vec4_t	q3;
	float	s1;

	s = bound( 0.0f, s, 1.0f );
	s1 = 1.0f - s;

	for( i = 0; i < pm_studiohdr->numbones; i++ )
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
PM_StudioGetAnim

====================
*/
static mstudioanim_t *PM_StudioGetAnim( model_t *m_pSubModel, mstudioseqdesc_t *pseqdesc )
{
	mstudioseqgroup_t	*pseqgroup;
	cache_user_t	*paSequences;
	size_t		filesize;
          byte		*buf;

	pseqgroup = (mstudioseqgroup_t *)((byte *)pm_studiohdr + pm_studiohdr->seqgroupindex) + pseqdesc->seqgroup;
	if( pseqdesc->seqgroup == 0 )
		return (mstudioanim_t *)((byte *)pm_studiohdr + pseqgroup->data + pseqdesc->animindex);

	paSequences = (cache_user_t *)m_pSubModel->submodels;

	if( paSequences == NULL )
	{
		paSequences = (cache_user_t *)Mem_Alloc( com_studiocache, MAXSTUDIOGROUPS * sizeof( cache_user_t ));
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
PM_StudioSetupBones
====================
*/
static void PM_StudioSetupBones( physent_t *pe )
{
	int		i, oldseq;
	double		f;

	mstudiobone_t	*pbones;
	mstudioseqdesc_t	*pseqdesc;
	mstudioanim_t	*panim;

	static float	pos[MAXSTUDIOBONES][3];
	static vec4_t	q[MAXSTUDIOBONES];
	matrix4x4		bonematrix;

	static float	pos2[MAXSTUDIOBONES][3];
	static vec4_t	q2[MAXSTUDIOBONES];
	static float	pos3[MAXSTUDIOBONES][3];
	static vec4_t	q3[MAXSTUDIOBONES];
	static float	pos4[MAXSTUDIOBONES][3];
	static vec4_t	q4[MAXSTUDIOBONES];

	oldseq = pe->sequence; // TraceCode can't change sequence

	if( pe->sequence >= pm_studiohdr->numseq ) pe->sequence = 0;
	pseqdesc = (mstudioseqdesc_t *)((byte *)pm_studiohdr + pm_studiohdr->seqindex) + pe->sequence;

	f = PM_StudioEstimateFrame( pe, pseqdesc );

	panim = PM_StudioGetAnim( pe->studiomodel, pseqdesc );
	PM_StudioCalcRotations( pe, pos, q, pseqdesc, panim, f );

	if( pseqdesc->numblends > 1 )
	{
		float	s;
		float	dadt = 1.0f;

		panim += pm_studiohdr->numbones;
		PM_StudioCalcRotations( pe, pos2, q2, pseqdesc, panim, f );

		s = (pe->blending[0] * dadt + pe->blending[0] * ( 1.0f - dadt )) / 255.0f;

		PM_StudioSlerpBones( q, pos, q2, pos2, s );

		if( pseqdesc->numblends == 4 )
		{
			panim += pm_studiohdr->numbones;
			PM_StudioCalcRotations( pe, pos3, q3, pseqdesc, panim, f );

			panim += pm_studiohdr->numbones;
			PM_StudioCalcRotations( pe, pos4, q4, pseqdesc, panim, f );

			s = ( pe->blending[0] * dadt + pe->blending[0] * ( 1.0f - dadt )) / 255.0f;
			PM_StudioSlerpBones( q3, pos3, q4, pos4, s );

			s = ( pe->blending[1] * dadt + pe->blending[1] * ( 1.0f - dadt )) / 255.0f;
			PM_StudioSlerpBones( q, pos, q3, pos3, s );
		}
	}

	pbones = (mstudiobone_t *)((byte *)pm_studiohdr + pm_studiohdr->boneindex);

	for( i = 0; i < pm_studiohdr->numbones; i++ ) 
	{
		Matrix4x4_FromOriginQuat( bonematrix, pos[i][0], pos[i][1], pos[i][2], q[i][0], q[i][1], q[i][2], q[i][3] );
		if( pbones[i].parent == -1 ) 
			Matrix4x4_ConcatTransforms( pm_studiobones[i], pm_studiomatrix, bonematrix );
		else Matrix4x4_ConcatTransforms( pm_studiobones[i], pm_studiobones[pbones[i].parent], bonematrix );
	}

	pe->sequence = oldseq; // restore original value
}

static qboolean PM_StudioSetupModel( physent_t *pe )
{
	model_t	*mod = pe->studiomodel;

	if( !mod || !mod->cache.data )
		return false;

	pm_studiohdr = (studiohdr_t *)mod->cache.data;
	PM_StudioSetUpTransform( pe );
	PM_StudioSetupBones( pe );
	return true;
}

qboolean PM_StudioExtractBbox( model_t *mod, int sequence, float *mins, float *maxs )
{
	mstudioseqdesc_t	*pseqdesc;
	studiohdr_t	*phdr;

	ASSERT( mod != NULL );

	if( mod->type != mod_studio || !mod->cache.data )
		return false;

	phdr = (studiohdr_t *)mod->cache.data;
	if( !phdr->numhitboxes ) return false;

	pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);

	if( sequence < 0 || sequence >= phdr->numseq )
		return false;
	
	VectorCopy( pseqdesc[sequence].bbmin, mins );
	VectorCopy( pseqdesc[sequence].bbmax, maxs );

	return true;
}

/*
================
PM_ClipBoxToHitbox

trace hitbox
================
*/
qboolean PM_ClipBoxToHitbox( pmtrace_t *trace )
{
	int	i;
	mplane_t	*p, *clipplane;
	float	enterfrac, leavefrac, distfrac;
	float	d, d1, d2;
	qboolean	getout, startout;
	float	f;

	enterfrac = -1.0f;
	leavefrac = 1.0f;
	clipplane = NULL;

	getout = false;
	startout = false;

	for( i = 0; i < 6; i++ )
	{
		p = &pm_hitboxplanes[i];

		// push the plane out apropriately for mins/maxs
		if( p->type < 3 )
		{
			d1 = trace_startmins[p->type] - p->dist;
			d2 = trace_endmins[p->type] - p->dist;
		}
		else
		{
			switch( p->signbits )
			{
			case 0:
				d1 = p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmins[2] - p->dist;
				d2 = p->normal[0]*trace_endmins[0] + p->normal[1]*trace_endmins[1] + p->normal[2]*trace_endmins[2] - p->dist;
				break;
			case 1:
				d1 = p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmins[2] - p->dist;
				d2 = p->normal[0]*trace_endmaxs[0] + p->normal[1]*trace_endmins[1] + p->normal[2]*trace_endmins[2] - p->dist;
				break;
			case 2:
				d1 = p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmins[2] - p->dist;
				d2 = p->normal[0]*trace_endmins[0] + p->normal[1]*trace_endmaxs[1] + p->normal[2]*trace_endmins[2] - p->dist;
				break;
			case 3:
				d1 = p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmins[2] - p->dist;
				d2 = p->normal[0]*trace_endmaxs[0] + p->normal[1]*trace_endmaxs[1] + p->normal[2]*trace_endmins[2] - p->dist;
				break;
			case 4:
				d1 = p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmaxs[2] - p->dist;
				d2 = p->normal[0]*trace_endmins[0] + p->normal[1]*trace_endmins[1] + p->normal[2]*trace_endmaxs[2] - p->dist;
				break;
			case 5:
				d1 = p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmaxs[2] - p->dist;
				d2 = p->normal[0]*trace_endmaxs[0] + p->normal[1]*trace_endmins[1] + p->normal[2]*trace_endmaxs[2] - p->dist;
				break;
			case 6:
				d1 = p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmaxs[2] - p->dist;
				d2 = p->normal[0]*trace_endmins[0] + p->normal[1]*trace_endmaxs[1] + p->normal[2]*trace_endmaxs[2] - p->dist;
				break;
			case 7:
				d1 = p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmaxs[2] - p->dist;
				d2 = p->normal[0]*trace_endmaxs[0] + p->normal[1]*trace_endmaxs[1] + p->normal[2]*trace_endmaxs[2] - p->dist;
				break;
			default:
				d1 = d2 = 0;	// shut up compiler
				break;
			}
		}

		if( d2 > 0 ) getout = true;	// endpoint is not in solid
		if( d1 > 0 ) startout = true;

		// if completely in front of face, no intersection
		if( d1 > 0 && d2 >= d1 )
			return false;

		if( d1 <= 0 && d2 <= 0 )
			continue;

		// crosses face
		d = 1.0f / ( d1 - d2 );
		f = d1 * d;

		if( d > 0 )
		{
			// enter
			if( f > enterfrac )
			{
				distfrac = d;
				enterfrac = f;
				clipplane = p;
			}
		}
		else if( d < 0 )
		{	
			// leave
			if( f < leavefrac )
				leavefrac = f;
		}
	}

	if( !startout )
	{	
		// original point was inside hitbox
		trace->startsolid = true;
		if( !getout ) trace->allsolid = true;
		return true;
	}

	if( enterfrac - FRAC_EPSILON <= leavefrac )
	{
		if( enterfrac > -1.0f && enterfrac < trace_realfraction )
		{
			if( enterfrac < 0 )
				enterfrac = 0;
			trace_realfraction = enterfrac;
			trace->fraction = enterfrac - DIST_EPSILON * distfrac;
            		VectorCopy( clipplane->normal, trace->plane.normal );
            		trace->plane.dist = clipplane->dist;
			return true;
		}
	}
	return false;
}

/*
================
PM_TestBoxInHitbox

test point trace in hibox
================
*/
qboolean PM_TestBoxInHitbox( pmtrace_t *trace )
{
	int		i;
	mplane_t		*p;

	for( i = 0; i < 6; i++ )
	{
		p = &pm_hitboxplanes[i];

		// push the plane out apropriately for mins/maxs
		// if completely in front of face, no intersection
		if( p->type < 3 )
		{
			if( trace_startmins[p->type] > p->dist )
				return false;
		}
		else
		{
			switch( p->signbits )
			{
			case 0:
				if( p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmins[2] > p->dist )
					return false;
				break;
			case 1:
				if( p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmins[2] > p->dist )
					return false;
				break;
			case 2:
				if( p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmins[2] > p->dist )
					return false;
				break;
			case 3:
				if( p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmins[2] > p->dist )
					return false;
				break;
			case 4:
				if( p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmaxs[2] > p->dist )
					return false;
				break;
			case 5:
				if( p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmaxs[2] > p->dist )
					return false;
				break;
			case 6:
				if( p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmaxs[2] > p->dist )
					return false;
				break;
			case 7:
				if( p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmaxs[2] > p->dist )
					return false;
				break;
			default:
				return false;
			}
		}
	}

	// inside this hitbox
	trace->fraction = trace_realfraction = 0;
	trace->startsolid = trace->allsolid = true;

	return true;
}

/*
================
PM_StudioIntersect

testing for potentially intersection of trace and animation bboxes
================
*/
static qboolean PM_StudioIntersect( physent_t *pe, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end )
{
	vec3_t	trace_mins, trace_maxs;
	vec3_t	anim_mins, anim_maxs;

	// create the bounding box of the entire move
	World_MoveBounds( start, mins, maxs, end, trace_mins, trace_maxs );

	if( !PM_StudioExtractBbox( pe->studiomodel, pe->sequence, anim_mins, anim_maxs ))
		return false; // invalid sequence

	if( !VectorIsNull( pe->angles ))
	{
		// expand for rotation
		float	max, v;
		int	i;

		for( i = 0, max = 0.0f; i < 3; i++ )
		{
			v = fabs( anim_mins[i] );
			if( v > max ) max = v;
			v = fabs( anim_maxs[i] );
			if( v > max ) max = v;
		}

		for( i = 0; i < 3; i++ )
		{
			anim_mins[i] = pe->origin[i] - max;
			anim_maxs[i] = pe->origin[i] + max;
		}
	}
	else
	{
		VectorAdd( anim_mins, pe->origin, anim_mins );
		VectorAdd( anim_maxs, pe->origin, anim_maxs );
	}

	// check intersection with trace entire move and animation bbox
	return BoundsIntersect( trace_mins, trace_maxs, anim_mins, anim_maxs );
}

qboolean PM_StudioTrace( physent_t *pe, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, pmtrace_t *ptr )
{
	vec3_t	start_l, end_l;
	int	i, outBone = -1;
	pfnTrace	StudioTrace = NULL;

	// assume we didn't hit anything
	Mem_Set( ptr, 0, sizeof( pmtrace_t ));
	VectorCopy( end, ptr->endpos );
	ptr->fraction = trace_realfraction = 1.0f;
	ptr->hitgroup = -1;
	ptr->ent = -1;

	if( !PM_StudioIntersect( pe, start, mins, maxs, end ))
		return false;

	if( !PM_StudioSetupModel( pe ))
		return false;

	if( VectorCompare( start, end ))
		StudioTrace = PM_TestBoxInHitbox;
	else StudioTrace = PM_ClipBoxToHitbox;

	// go to check individual hitboxes		
	for( i = 0; i < pm_studiohdr->numhitboxes; i++ )
	{
		mstudiobbox_t	*phitbox = (mstudiobbox_t *)((byte*)pm_studiohdr + pm_studiohdr->hitboxindex) + i;
		matrix4x4		bonemat;

		// transform traceline into local bone space
		Matrix4x4_Invert_Simple( bonemat, pm_studiobones[phitbox->bone] );
		Matrix4x4_VectorTransform( bonemat, start, start_l );
		Matrix4x4_VectorTransform( bonemat, end, end_l );

		PM_HullForHitbox( phitbox->bbmin, phitbox->bbmax );

		VectorAdd( start_l, mins, trace_startmins );
		VectorAdd( start_l, maxs, trace_startmaxs );
		VectorAdd( end_l, mins, trace_endmins );
		VectorAdd( end_l, maxs, trace_endmaxs );

		if( StudioTrace( ptr ))
		{
			outBone = phitbox->bone;
			ptr->hitgroup = phitbox->group;
		}

		if( ptr->allsolid )
			break;
	}

	// all hitboxes were swept, get trace result
	if( outBone >= 0 )
	{
		vec3_t	temp;

		VectorCopy( ptr->plane.normal, temp );
		ptr->fraction = bound( 0, ptr->fraction, 1.0f );
		VectorLerp( start, ptr->fraction, end, ptr->endpos );
		Matrix4x4_TransformPositivePlane( pm_studiobones[outBone], temp, ptr->plane.dist, ptr->plane.normal, &ptr->plane.dist );
		return true;
	}
	return false;
}