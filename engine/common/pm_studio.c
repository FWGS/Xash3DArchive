//=======================================================================
//			Copyright XashXT Group 2010 �
//		        pm_studio.c - stduio models tracing
//=======================================================================

#include "common.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "pm_local.h"

static mplane_t	pm_studioplanes[12];
static matrix4x4	pm_studiomatrix;
static studiohdr_t	*pm_studiohdr;
static matrix4x4	pm_studiobones[MAXSTUDIOBONES];

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
		p = &pm_studioplanes[i*2];
		p->type = i>>1;
		p->signbits = 0;
		VectorClear( p->normal );
		p->normal[i>>1] = 1.0f;

		p = &pm_studioplanes[i*2+1];
		p->type = 3 + (i>>1);
		p->signbits = 0;
		VectorClear( p->normal );
		p->normal[i>>1] = -1.0f;

		p->signbits = SignbitsForPlane( p->normal );
	}    
}

/*
====================
PM_HullForStudio
====================
*/
static void PM_HullForStudio( const vec3_t mins, const vec3_t maxs )
{
	pm_studioplanes[0].dist = maxs[0];
	pm_studioplanes[1].dist = -maxs[0];
	pm_studioplanes[2].dist = mins[0];
	pm_studioplanes[3].dist = -mins[0];
	pm_studioplanes[4].dist = maxs[1];
	pm_studioplanes[5].dist = -maxs[1];
	pm_studioplanes[6].dist = mins[1];
	pm_studioplanes[7].dist = -mins[1];
	pm_studioplanes[8].dist = maxs[2];
	pm_studioplanes[9].dist = -maxs[2];
	pm_studioplanes[10].dist = mins[2];
	pm_studioplanes[11].dist = -mins[2];
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

	if( pe->scale != 0.0f ) scale = pe->scale;
	Matrix4x4_CreateFromEntity( pm_studiomatrix, org[0], org[1], org[2], ang[PITCH], ang[YAW], ang[ROLL], scale );
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

	s = 0 * ((1.0 - (f - (int)(f))) / (pseqdesc->numframes)) * pe->framerate;

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
		paSequences = (cache_user_t *)Mem_Alloc( m_pSubModel->mempool, MAXSTUDIOGROUPS * sizeof( cache_user_t ));
		m_pSubModel->submodels = (void *)paSequences;
	}

	// check for already loaded
	if( !Cache_Check( m_pSubModel->mempool, ( cache_user_t *)&( paSequences[pseqdesc->seqgroup] )))
	{
		string	filepath, modelname, modelpath;

		FS_FileBase( m_pSubModel->name, modelname );
		FS_ExtractFilePath( m_pSubModel->name, modelpath );
		com.snprintf( filepath, sizeof( filepath ), "%s/%s%i%i.mdl", modelpath, modelname, pseqdesc->seqgroup / 10, pseqdesc->seqgroup % 10 );

		buf = FS_LoadFile( filepath, &filesize );
		if( !buf || !filesize ) Host_Error( "CM_StudioGetAnim: can't load %s\n", modelpath );
		if( IDSEQGRPHEADER != *(uint *)buf )
			Host_Error( "PM_StudioGetAnim: %s is corrupted\n", modelpath );

		paSequences[pseqdesc->seqgroup].data = Mem_Alloc( m_pSubModel->mempool, filesize );
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

static bool PM_StudioSetupModel( physent_t *pe )
{
	model_t	*mod = pe->studiomodel;

	if( !mod || !mod->extradata )
		return false;

	pm_studiohdr = (studiohdr_t *)mod->extradata;
	PM_StudioSetUpTransform( pe );
	PM_StudioSetupBones( pe );
	return true;
}

static bool PM_StudioTraceBox( vec3_t start, vec3_t end, pmtrace_t *ptr ) 
{
	mplane_t	*plane, *clipplane;
	float	enterFrac, leaveFrac;
	bool	getout, startout;
	float	d1, d2;
	float	f;
	int	i;

	enterFrac =-1.0f;
	leaveFrac = 1.0f;
	clipplane = NULL;

	getout = false;
	startout = false;

	// compare the trace against all planes of the brush
	// find the latest time the trace crosses a plane towards the interior
	// and the earliest time the trace crosses a plane towards the exterior
	for( i = 0; i < 6; i++ ) 
	{
		plane = pm_studioplanes + i * 2 + (i & 1);

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
		if( !getout ) ptr->fraction = 0.0f;
		return true;
	}
    
	if( enterFrac < leaveFrac ) 
	{
		if( enterFrac > -1 && enterFrac < ptr->fraction ) 
		{
			if( enterFrac < 0.0f )
				enterFrac = 0.0f;

			ptr->fraction = enterFrac;
            		VectorCopy( clipplane->normal, ptr->plane.normal );
            		ptr->plane.dist = clipplane->dist;
			return true;
		}
	}
	return false;
}

bool PM_StudioTrace( physent_t *pe, const vec3_t start, const vec3_t end, pmtrace_t *ptr )
{
	matrix4x4	m;
	vec3_t	start_l, end_l;
	int	i, outBone = -1;

	// assume we didn't hit anything
	Mem_Set( ptr, 0, sizeof( pmtrace_t ));
	VectorCopy( end, ptr->endpos );
	ptr->fraction = 1.0f;
	ptr->allsolid = true;
	ptr->hitgroup = -1;
	ptr->ent = -1;

	if( !PM_StudioSetupModel( pe ) || !pm_studiohdr->numhitboxes )
	{
		ptr->hitgroup = -1;
		return false;
	}

	for( i = 0; i < pm_studiohdr->numhitboxes; i++ )
	{
		mstudiobbox_t	*phitbox = (mstudiobbox_t *)((byte*)pm_studiohdr + pm_studiohdr->hitboxindex) + i;

		Matrix4x4_Invert_Simple( m, pm_studiobones[phitbox->bone] );
		Matrix4x4_VectorTransform( m, start, start_l );
		Matrix4x4_VectorTransform( m, end, end_l );

		PM_HullForStudio( phitbox->bbmin, phitbox->bbmax );

		if( PM_StudioTraceBox( start_l, end_l, ptr ))
		{
			outBone = phitbox->bone;
			ptr->hitgroup = phitbox->group;
		}

		if( ptr->fraction == 0.0f )
			break;
	}

	if( ptr->fraction > 0.0f )
		ptr->allsolid = false;

	// all hitboxes were swept, get trace result
	if( outBone >= 0 )
	{
		vec3_t	temp;

		VectorCopy( ptr->plane.normal, temp );
		Matrix4x4_VectorRotate( pm_studiobones[outBone], temp, ptr->plane.normal );

		if( ptr->fraction == 1.0f ) VectorCopy( end, ptr->endpos );
		else VectorLerp( start, ptr->fraction, end, ptr->endpos );
		ptr->plane.dist = DotProduct( ptr->endpos, ptr->plane.normal );

		return true;
	}
	return false;
}