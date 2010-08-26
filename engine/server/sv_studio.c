//=======================================================================
//			Copyright XashXT Group 2010 ©
//		      sv_studio.c - server studio utilities
//=======================================================================

// FIXME: these code needs to be some cleanup from lerping code

#include "common.h"
#include "server.h"
#include "matrix_lib.h"

static studiohdr_t	*sv_studiohdr;
static mplane_t	sv_hitboxplanes[6];	// there a temp hitbox
static matrix4x4	sv_studiomatrix;
static matrix4x4	sv_studiobones[MAXSTUDIOBONES];
typedef bool 	(*pfnHitboxTrace)( trace_t *trace );
static vec3_t	trace_startmins, trace_endmins;
static vec3_t	trace_startmaxs, trace_endmaxs;
static vec3_t	trace_absmins, trace_absmaxs;
static float	trace_realfraction;

/*
====================
SV_InitStudioHull
====================
*/
void SV_InitStudioHull( void )
{
	int	i, side;
	mplane_t	*p;

	for( i = 0; i < 6; i++ )
	{
		side = i & 1;

		// planes
		p = &sv_hitboxplanes[i];
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
SV_HullForHitbox
====================
*/
static void SV_HullForHitbox( const vec3_t mins, const vec3_t maxs )
{
	sv_hitboxplanes[0].dist = maxs[0];
	sv_hitboxplanes[1].dist = -mins[0];
	sv_hitboxplanes[2].dist = maxs[1];
	sv_hitboxplanes[3].dist = -mins[1];
	sv_hitboxplanes[4].dist = maxs[2];
	sv_hitboxplanes[5].dist = -mins[2];
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
static void SV_StudioSetUpTransform( edict_t *ent )
{
	float	*ang, *org;
	float	scale = 1.0f;

	org = ent->v.origin;
	ang = ent->v.angles;

	if( ent->v.scale != 0.0f ) scale = ent->v.scale;
	Matrix4x4_CreateFromEntity( sv_studiomatrix, org[0], org[1], org[2], -ang[PITCH], ang[YAW], ang[ROLL], scale );
}

/*
====================
StudioCalcBoneAdj

====================
*/
static void SV_StudioCalcBoneAdj( float dadt, float *adj, const byte *pcontroller1, const byte *pcontroller2 )
{
	int			i, j;
	float			value;
	mstudiobonecontroller_t	*pbonecontroller;
	
	pbonecontroller = (mstudiobonecontroller_t *)((byte *)sv_studiohdr + sv_studiohdr->bonecontrollerindex);

	for( j = 0; j < sv_studiohdr->numbonecontrollers; j++ )
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
					value = ((a * dadt) + (b * (1.0f - dadt)) - 128) * (360.0f / 256.0f) + pbonecontroller[j].start;
				}
				else 
				{
					value = ((pcontroller1[i] * dadt + (pcontroller2[i]) * (1.0 - dadt))) * (360.0/256.0) + pbonecontroller[j].start;
				}
			}
			else 
			{
				value = (pcontroller1[i] * dadt + pcontroller2[i] * (1.0f - dadt)) / 255.0f;
				if( value < 0.0f ) value = 0.0f;
				if( value > 1.0f ) value = 1.0f;
				value = (1.0f - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			}
		}

		switch( pbonecontroller[j].type & STUDIO_TYPES )
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = value * (M_PI / 180.0f);
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
static void SV_StudioCalcBoneQuaterion( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *q )
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
static void SV_StudioCalcBonePosition( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *pos )
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
static void SV_StudioCalcRotations( edict_t *ent, float pos[][3], vec4_t *q, mstudioseqdesc_t *pseqdesc, mstudioanim_t *panim, float f )
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
	pbone = (mstudiobone_t *)((byte *)sv_studiohdr + sv_studiohdr->boneindex);

	SV_StudioCalcBoneAdj( dadt, adj, ent->v.controller, ent->v.controller );

	for( i = 0; i < sv_studiohdr->numbones; i++, pbone++, panim++ ) 
	{
		SV_StudioCalcBoneQuaterion( frame, s, pbone, panim, adj, q[i] );
		SV_StudioCalcBonePosition( frame, s, pbone, panim, adj, pos[i] );
	}

	if( pseqdesc->motiontype & STUDIO_X ) pos[pseqdesc->motionbone][0] = 0.0f;
	if( pseqdesc->motiontype & STUDIO_Y ) pos[pseqdesc->motionbone][1] = 0.0f;
	if( pseqdesc->motiontype & STUDIO_Z ) pos[pseqdesc->motionbone][2] = 0.0f;

	s = 0 * ((1.0f - (f - (int)(f))) / (pseqdesc->numframes)) * ent->v.framerate;

	if( pseqdesc->motiontype & STUDIO_LX ) pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];
	if( pseqdesc->motiontype & STUDIO_LY ) pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];
	if( pseqdesc->motiontype & STUDIO_LZ ) pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];
}

/*
====================
StudioEstimateFrame

====================
*/
static float SV_StudioEstimateFrame( edict_t *ent, mstudioseqdesc_t *pseqdesc )
{
	double	f;
	
	if( pseqdesc->numframes <= 1 )
		f = 0;
	else f = (ent->v.frame * ( pseqdesc->numframes - 1 )) / 256.0;
 
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
static void SV_StudioSlerpBones( vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s )
{
	int	i;
	vec4_t	q3;
	float	s1;

	s = bound( 0.0f, s, 1.0f );
	s1 = 1.0f - s;

	for( i = 0; i < sv_studiohdr->numbones; i++ )
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
StudioGetAnim

====================
*/
static mstudioanim_t *SV_StudioGetAnim( model_t *m_pSubModel, mstudioseqdesc_t *pseqdesc )
{
	mstudioseqgroup_t	*pseqgroup;
	cache_user_t	*paSequences;
	size_t		filesize;
          byte		*buf;

	pseqgroup = (mstudioseqgroup_t *)((byte *)sv_studiohdr + sv_studiohdr->seqgroupindex) + pseqdesc->seqgroup;
	if( pseqdesc->seqgroup == 0 )
		return (mstudioanim_t *)((byte *)sv_studiohdr + pseqgroup->data + pseqdesc->animindex);

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
			Host_Error( "SV_StudioGetAnim: %s is corrupted\n", modelpath );

		paSequences[pseqdesc->seqgroup].data = Mem_Alloc( m_pSubModel->mempool, filesize );
		Mem_Copy( paSequences[pseqdesc->seqgroup].data, buf, filesize );
		Mem_Free( buf );
	}
	return (mstudioanim_t *)((byte *)paSequences[pseqdesc->seqgroup].data + pseqdesc->animindex);
}

/*
====================
StudioSetupBones
====================
*/
static void SV_StudioSetupBones( edict_t *ent )
{
	int		i;
	double		f;

	mstudiobone_t	*pbones;
	mstudioseqdesc_t	*pseqdesc;
	mstudioanim_t	*panim;
	model_t		*m_pModel;

	static float	pos[MAXSTUDIOBONES][3];
	static vec4_t	q[MAXSTUDIOBONES];
	matrix4x4		bonematrix;

	static float	pos2[MAXSTUDIOBONES][3];
	static vec4_t	q2[MAXSTUDIOBONES];
	static float	pos3[MAXSTUDIOBONES][3];
	static vec4_t	q3[MAXSTUDIOBONES];
	static float	pos4[MAXSTUDIOBONES][3];
	static vec4_t	q4[MAXSTUDIOBONES];

	m_pModel = CM_ClipHandleToModel( ent->v.modelindex );

	if( ent->v.sequence >= sv_studiohdr->numseq ) ent->v.sequence = 0;
	pseqdesc = (mstudioseqdesc_t *)((byte *)sv_studiohdr + sv_studiohdr->seqindex) + ent->v.sequence;

	f = SV_StudioEstimateFrame( ent, pseqdesc );

	panim = SV_StudioGetAnim( m_pModel, pseqdesc );
	SV_StudioCalcRotations( ent, pos, q, pseqdesc, panim, f );

	if( pseqdesc->numblends > 1 )
	{
		float	s;
		float	dadt = 1.0f;

		panim += sv_studiohdr->numbones;
		SV_StudioCalcRotations( ent, pos2, q2, pseqdesc, panim, f );

		s = (ent->v.blending[0] * dadt + ent->v.blending[0] * ( 1.0f - dadt )) / 255.0f;

		SV_StudioSlerpBones( q, pos, q2, pos2, s );

		if( pseqdesc->numblends == 4 )
		{
			panim += sv_studiohdr->numbones;
			SV_StudioCalcRotations( ent, pos3, q3, pseqdesc, panim, f );

			panim += sv_studiohdr->numbones;
			SV_StudioCalcRotations( ent, pos4, q4, pseqdesc, panim, f );

			s = ( ent->v.blending[0] * dadt + ent->v.blending[0] * ( 1.0f - dadt )) / 255.0f;
			SV_StudioSlerpBones( q3, pos3, q4, pos4, s );

			s = ( ent->v.blending[1] * dadt + ent->v.blending[1] * ( 1.0f - dadt )) / 255.0f;
			SV_StudioSlerpBones( q, pos, q3, pos3, s );
		}
	}

	pbones = (mstudiobone_t *)((byte *)sv_studiohdr + sv_studiohdr->boneindex);

	for( i = 0; i < sv_studiohdr->numbones; i++ ) 
	{
		Matrix4x4_FromOriginQuat( bonematrix, pos[i][0], pos[i][1], pos[i][2], q[i][0], q[i][1], q[i][2], q[i][3] );
		if( pbones[i].parent == -1 ) 
			Matrix4x4_ConcatTransforms( sv_studiobones[i], sv_studiomatrix, bonematrix );
		else Matrix4x4_ConcatTransforms( sv_studiobones[i], sv_studiobones[pbones[i].parent], bonematrix );
	}
}

/*
====================
StudioCalcAttachments

====================
*/
static bool SV_StudioCalcAttachments( edict_t *e, int iAttachment, float *org, float *ang )
{
	int			i;
	mstudioattachment_t		*pAtt;
	vec3_t			axis[3];
	vec3_t			localOrg, localAng;
	void			*hdr = Mod_Extradata( e->v.modelindex );

	if( !hdr ) return false;

	sv_studiohdr = (studiohdr_t *)hdr;
	if( sv_studiohdr->numattachments <= 0 )
		return false;

	SV_StudioSetUpTransform( e );
	SV_StudioSetupBones( e );

	if( sv_studiohdr->numattachments > MAXSTUDIOATTACHMENTS )
	{
		sv_studiohdr->numattachments = MAXSTUDIOATTACHMENTS; // reduce it
		MsgDev( D_WARN, "SV_StudioCalcAttahments: too many attachments on %s\n", sv_studiohdr->name );
	}

	iAttachment = bound( 0, iAttachment, sv_studiohdr->numattachments );

	// calculate attachment points
	pAtt = (mstudioattachment_t *)((byte *)sv_studiohdr + sv_studiohdr->attachmentindex);

	for( i = 0; i < sv_studiohdr->numattachments; i++ )
	{
		if( i == iAttachment )
		{
			// compute pos and angles
			Matrix4x4_VectorTransform( sv_studiobones[pAtt[i].bone], pAtt[i].org, localOrg );
			Matrix4x4_VectorTransform( sv_studiobones[pAtt[i].bone], pAtt[i].vectors[0], axis[0] );
			Matrix4x4_VectorTransform( sv_studiobones[pAtt[i].bone], pAtt[i].vectors[1], axis[1] );
			Matrix4x4_VectorTransform( sv_studiobones[pAtt[i].bone], pAtt[i].vectors[2], axis[2] );
			Matrix3x3_ToAngles( axis, localAng, true ); // FIXME: dll's uses FLU ?
			if( org ) VectorCopy( localOrg, org );
			if( ang ) VectorCopy( localAng, ang );
			break; // done
		}
	}
	return true;
}

static bool SV_StudioSetupModel( edict_t *ent )
{
	void	*hdr = Mod_Extradata( ent->v.modelindex );

	if( !hdr ) return false;

	sv_studiohdr = (studiohdr_t *)hdr;
	SV_StudioSetUpTransform( ent );
	SV_StudioSetupBones( ent );

	return true;
}

bool SV_StudioExtractBbox( model_t *mod, int sequence, float *mins, float *maxs )
{
	mstudioseqdesc_t	*pseqdesc;
	studiohdr_t	*phdr;

	ASSERT( mod != NULL );

	if( mod->type != mod_studio || !mod->extradata )
		return false;

	phdr = (studiohdr_t *)mod->extradata;
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
StudioTestToHitbox

test point trace in hitbox
================
*/
static bool SV_StudioTestToHitbox( trace_t *trace )
{
	int		i;
	mplane_t		*p;

	for( i = 0; i < 6; i++ )
	{
		p = &sv_hitboxplanes[i];

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
	trace->flFraction = trace_realfraction = 0;
	trace->fStartSolid = trace->fAllSolid = true;

	return true;
}

/*
================
StudioClipToHitbox

trace hitbox
================
*/
static bool SV_StudioClipToHitbox( trace_t *trace )
{
	int	i;
	mplane_t	*p, *clipplane;
	float	enterfrac, leavefrac, distfrac;
	float	d, d1, d2;
	bool	getout, startout;
	float	f;

	enterfrac = -1.0f;
	leavefrac = 1.0f;
	clipplane = NULL;

	getout = false;
	startout = false;

	for( i = 0; i < 6; i++ )
	{
		p = &sv_hitboxplanes[i];

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
		trace->fStartSolid = true;
		if( !getout ) trace->fAllSolid = true;
		return true;
	}

	if( enterfrac - FRAC_EPSILON <= leavefrac )
	{
		if( enterfrac > -1.0f && enterfrac < trace_realfraction )
		{
			if( enterfrac < 0 )
				enterfrac = 0;
			trace_realfraction = enterfrac;
			trace->flFraction = enterfrac - DIST_EPSILON * distfrac;
            		VectorCopy( clipplane->normal, trace->vecPlaneNormal );
            		trace->flPlaneDist = clipplane->dist;
			return true;
		}
	}
	return false;
}

/*
================
SV_StudioIntersect

testing for potentially intersection of trace and animation bboxes
================
*/
static bool SV_StudioIntersect( edict_t *ent, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end )
{
	vec3_t	trace_mins, trace_maxs;
	vec3_t	anim_mins, anim_maxs;
	model_t	*mod = CM_ClipHandleToModel( ent->v.modelindex );

	// create the bounding box of the entire move
	World_MoveBounds( start, mins, maxs, end, trace_mins, trace_maxs );

	if( !SV_StudioExtractBbox( mod, ent->v.sequence, anim_mins, anim_maxs ))
		return false; // invalid sequence

	if( !VectorIsNull( ent->v.angles ))
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
			anim_mins[i] = ent->v.origin[i] - max;
			anim_maxs[i] = ent->v.origin[i] + max;
		}
	}
	else
	{
		VectorAdd( anim_mins, ent->v.origin, anim_mins );
		VectorAdd( anim_maxs, ent->v.origin, anim_maxs );
	}

	// check intersection with trace entire move and animation bbox
	return BoundsIntersect( trace_mins, trace_maxs, anim_mins, anim_maxs );
}

trace_t SV_TraceHitbox( edict_t *ent, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end )
{
	vec3_t		start_l, end_l;
	int		i, outBone = -1;
	pfnHitboxTrace	TraceHitbox = NULL;
	trace_t		trace;

	// assume we didn't hit anything
	Mem_Set( &trace, 0, sizeof( trace_t ));
	VectorCopy( end, trace.vecEndPos );
	trace.flFraction = trace_realfraction = 1.0f;
	trace.iHitgroup = -1;

	if( !SV_StudioIntersect( ent, start, mins, maxs, end ))
		return trace;

	if( !SV_StudioSetupModel( ent ))
		return trace;

	if( VectorCompare( start, end ))
		TraceHitbox = SV_StudioTestToHitbox;	// special case for test position
	else TraceHitbox = SV_StudioClipToHitbox;

	// go to check individual hitboxes		
	for( i = 0; i < sv_studiohdr->numhitboxes; i++ )
	{
		mstudiobbox_t	*phitbox = (mstudiobbox_t *)((byte*)sv_studiohdr + sv_studiohdr->hitboxindex) + i;
		matrix4x4		bonemat;

		// transform traceline into local bone space
		Matrix4x4_Invert_Simple( bonemat, sv_studiobones[phitbox->bone] );
		Matrix4x4_VectorTransform( bonemat, start, start_l );
		Matrix4x4_VectorTransform( bonemat, end, end_l );

		SV_HullForHitbox( phitbox->bbmin, phitbox->bbmax );

		VectorAdd( start_l, mins, trace_startmins );
		VectorAdd( start_l, maxs, trace_startmaxs );
		VectorAdd( end_l, mins, trace_endmins );
		VectorAdd( end_l, maxs, trace_endmaxs );

		if( TraceHitbox( &trace ))
		{
			outBone = phitbox->bone;
			trace.iHitgroup = phitbox->group;
		}

		if( trace.fAllSolid )
			break;
	}

	// all hitboxes were swept, get trace result
	if( outBone >= 0 )
	{
		vec3_t	temp;

		trace.pHit = ent;
		VectorCopy( trace.vecPlaneNormal, temp );
		trace.flFraction = bound( 0, trace.flFraction, 1.0f );
		VectorLerp( start, trace.flFraction, end, trace.vecEndPos );
		Matrix4x4_TransformPositivePlane( sv_studiobones[outBone], temp, trace.flPlaneDist, trace.vecPlaneNormal, &trace.flPlaneDist );
	}
	return trace;
}

void SV_StudioGetAttachment( edict_t *e, int iAttachment, float *org, float *ang )
{
	if( !SV_StudioCalcAttachments( e, iAttachment, org, ang ))
	{
		// reset attachments
		if( org ) VectorCopy( e->v.origin, org );
		if( ang ) VectorCopy( e->v.angles, ang );
		return;
	}
}

void SV_GetBonePosition( edict_t *e, int iBone, float *org, float *ang )
{
	matrix3x3	axis;

	if( !SV_StudioSetupModel( e ) || sv_studiohdr->numbones <= 0 )
	{
		// reset bones
		if( org ) VectorCopy( e->v.origin, org );
		if( ang ) VectorCopy( e->v.angles, ang );
		return;
	}

	iBone = bound( 0, iBone, sv_studiohdr->numbones );
	Matrix3x3_FromMatrix4x4( axis, sv_studiobones[iBone] );
	if( org ) Matrix4x4_OriginFromMatrix( sv_studiobones[iBone], org );
	if( ang ) Matrix3x3_ToAngles( axis, ang, true );
}