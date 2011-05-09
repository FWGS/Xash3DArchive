/*
sv_studio.c - server studio utilities
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
#include "server.h"
#include "studio.h"
#include "r_studioint.h"
#include "library.h"

typedef int (*STUDIOAPI)( int, sv_blending_interface_t*, server_studio_api_t*, matrix3x4, matrix3x4[MAXSTUDIOBONES] );

static studiohdr_t			*sv_studiohdr;
static mplane_t			sv_hitboxplanes[6];	// there a temp hitbox
static matrix3x4			sv_studiomatrix;
static matrix3x4			sv_studiobones[MAXSTUDIOBONES];
typedef qboolean 			(*pfnHitboxTrace)( trace_t *trace );
static vec3_t			trace_startmins, trace_endmins;
static vec3_t			trace_startmaxs, trace_endmaxs;
static vec3_t			trace_absmins, trace_absmaxs;
static float			trace_realfraction;
static sv_blending_interface_t	*pBlendAPI;

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
StudioPlayerBlend

====================
*/
void SV_StudioPlayerBlend( mstudioseqdesc_t *pseqdesc, int *pBlend, float *pPitch )
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
StudioCalcBoneAdj

====================
*/
static void SV_StudioCalcBoneAdj( float *adj, const byte *pcontroller )
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
				value = pcontroller[i] * (360.0f / 256.0f) + pbonecontroller[j].start;
			}
			else 
			{
				value = pcontroller[i] / 255.0f;
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
static void SV_StudioCalcRotations( const edict_t *ent, int boneused[], int numbones, const byte *pcontroller, float pos[][3], vec4_t *q, mstudioseqdesc_t *pseqdesc, mstudioanim_t *panim, float f )
{
	int		i, j, frame;
	mstudiobone_t	*pbone;
	float		adj[MAXSTUDIOCONTROLLERS];
	float		s;

	if( f > pseqdesc->numframes - 1 )
		f = 0;
	else if( f < -0.01f )
		f = -0.01f;

	frame = (int)f;
	s = (f - frame);

	// add in programtic controllers
	pbone = (mstudiobone_t *)((byte *)sv_studiohdr + sv_studiohdr->boneindex);

	SV_StudioCalcBoneAdj( adj, pcontroller );

	for( j = numbones - 1; j >= 0; j-- )
	{
		i = boneused[j];
		SV_StudioCalcBoneQuaterion( frame, s, &pbone[i], &panim[i], adj, q[i] );
		SV_StudioCalcBonePosition( frame, s, &pbone[i], &panim[i], adj, pos[i] );
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
static float SV_StudioEstimateFrame( float frame, mstudioseqdesc_t *pseqdesc )
{
	double	f;
	
	if( pseqdesc->numframes <= 1 )
		f = 0;
	else f = ( frame * ( pseqdesc->numframes - 1 )) / 256.0;
 
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
		paSequences = (cache_user_t *)Mem_Alloc( com_studiocache, MAXSTUDIOGROUPS * sizeof( cache_user_t ));
		m_pSubModel->submodels = (void *)paSequences;
	}

	// check for already loaded
	if( !Mod_CacheCheck(( cache_user_t *)&( paSequences[pseqdesc->seqgroup] )))
	{
		string	filepath, modelname, modelpath;

		FS_FileBase( m_pSubModel->name, modelname );
		FS_ExtractFilePath( m_pSubModel->name, modelpath );
		Q_snprintf( filepath, sizeof( filepath ), "%s/%s%i%i.mdl", modelpath, modelname, pseqdesc->seqgroup / 10, pseqdesc->seqgroup % 10 );

		buf = FS_LoadFile( filepath, &filesize, false );
		if( !buf || !filesize ) Host_Error( "StudioGetAnim: can't load %s\n", filepath );
		if( IDSEQGRPHEADER != *(uint *)buf )
			Host_Error( "StudioGetAnim: %s is corrupted\n", filepath );

		MsgDev( D_INFO, "loading: %s\n", filepath );

		paSequences[pseqdesc->seqgroup].data = Mem_Alloc( com_studiocache, filesize );
		Q_memcpy( paSequences[pseqdesc->seqgroup].data, buf, filesize );
		Mem_Free( buf );
	}
	return (mstudioanim_t *)((byte *)paSequences[pseqdesc->seqgroup].data + pseqdesc->animindex);
}

/*
====================
StudioSetupBones

NOTE: pEdict is unused
====================
*/
static void SV_StudioSetupBones( model_t *pModel,	float frame, int sequence, const vec3_t angles, const vec3_t origin,
	const byte *pcontroller, const byte *pblending, int iBone, const edict_t *pEdict )
{
	int		i, j, numbones;
	int		boneused[MAXSTUDIOBONES];
	float		scale = 1.0f;
	double		f;

	mstudiobone_t	*pbones;
	mstudioseqdesc_t	*pseqdesc;
	mstudioanim_t	*panim;

	static float	pos[MAXSTUDIOBONES][3];
	static vec4_t	q[MAXSTUDIOBONES];
	matrix3x4		bonematrix;

	static float	pos2[MAXSTUDIOBONES][3];
	static vec4_t	q2[MAXSTUDIOBONES];
	static float	pos3[MAXSTUDIOBONES][3];
	static vec4_t	q3[MAXSTUDIOBONES];
	static float	pos4[MAXSTUDIOBONES][3];
	static vec4_t	q4[MAXSTUDIOBONES];

	if( sequence >= sv_studiohdr->numseq ) sequence = 0;
	pseqdesc = (mstudioseqdesc_t *)((byte *)sv_studiohdr + sv_studiohdr->seqindex) + sequence;
	pbones = (mstudiobone_t *)((byte *)sv_studiohdr + sv_studiohdr->boneindex);

	if( iBone < -1 || iBone >= sv_studiohdr->numbones )
	{
		iBone = 0;
	}

	numbones = 0;

	if( iBone == -1 )
	{
		numbones = sv_studiohdr->numbones;
		for( i = 0; i < sv_studiohdr->numbones; i++ )
			boneused[(numbones - i) - 1] = i;
	}
	else
	{
		for( i = iBone; i != -1; i = pbones[i].parent )
		{
			boneused[numbones] = i;
			numbones++;
		}
	}

	f = SV_StudioEstimateFrame( frame, pseqdesc );

	panim = SV_StudioGetAnim( pModel, pseqdesc );
	SV_StudioCalcRotations( pEdict, boneused, numbones, pcontroller, pos, q, pseqdesc, panim, f );

	if( pseqdesc->numblends > 1 )
	{
		float	s;

		panim += sv_studiohdr->numbones;
		SV_StudioCalcRotations( pEdict, boneused, numbones, pcontroller, pos2, q2, pseqdesc, panim, f );

		s = (float)pblending[0] / 255.0f;

		SV_StudioSlerpBones( q, pos, q2, pos2, s );

		if( pseqdesc->numblends == 4 )
		{
			panim += sv_studiohdr->numbones;
			SV_StudioCalcRotations( pEdict, boneused, numbones, pcontroller, pos3, q3, pseqdesc, panim, f );

			panim += sv_studiohdr->numbones;
			SV_StudioCalcRotations( pEdict, boneused, numbones, pcontroller, pos4, q4, pseqdesc, panim, f );

			s = (float)pblending[0] / 255.0f;
			SV_StudioSlerpBones( q3, pos3, q4, pos4, s );

			s = (float)pblending[1] / 255.0f;
			SV_StudioSlerpBones( q, pos, q3, pos3, s );
		}
	}

	if( SV_IsValidEdict( pEdict ) && pEdict->v.scale != 0.0f && sv_allow_studio_scaling->integer )
		scale = pEdict->v.scale;
	else if( SV_IsValidEdict( pEdict ) && (pEdict->v.flags & FL_CLIENT) && sv_clienttrace->value != 0.0f )
		scale = sv_clienttrace->value * 0.5f; 

	Matrix3x4_CreateFromEntity( sv_studiomatrix, angles, origin, scale );

	for( j = numbones - 1; j >= 0; j-- )
	{
		i = boneused[j];

		Matrix3x4_FromOriginQuat( bonematrix, q[i], pos[i] );
		if( pbones[i].parent == -1 ) 
			Matrix3x4_ConcatTransforms( sv_studiobones[i], sv_studiomatrix, bonematrix );
		else Matrix3x4_ConcatTransforms( sv_studiobones[i], sv_studiobones[pbones[i].parent], bonematrix );
	}
}

/*
====================
StudioSetupModel

====================
*/
static qboolean SV_StudioSetupModel( edict_t *ent, int iBone, qboolean bInversePitch )
{
	model_t	*mod = Mod_Handle( ent->v.modelindex );
	void	*hdr = Mod_Extradata( mod );
	vec3_t	angles;
		
	if( !hdr ) return false;

	sv_studiohdr = (studiohdr_t *)hdr;
	VectorCopy( ent->v.angles, angles );

	if( bInversePitch )
	{
		angles[PITCH] = -angles[PITCH];
	}		

	// calc blending for player
	if( ent->v.flags & FL_CLIENT )
	{
		mstudioseqdesc_t	*pseqdesc;
		byte		controller[4];
		byte		blending[2];
		int		iBlend;

		pseqdesc = (mstudioseqdesc_t *)((byte *)sv_studiohdr + sv_studiohdr->seqindex) + ent->v.sequence;

		SV_StudioPlayerBlend( pseqdesc, &iBlend, &angles[PITCH] );

		controller[0] = controller[1] = controller[2] = controller[3] = 0x7F;
		blending[0] = (byte)iBlend;
		blending[1] = 0;

		pBlendAPI->SV_StudioSetupBones( mod, ent->v.frame, ent->v.sequence, angles, ent->v.origin,
			controller, blending, iBone, ent );
          }
          else
          {
		pBlendAPI->SV_StudioSetupBones( mod, ent->v.frame, ent->v.sequence, angles, ent->v.origin,
			ent->v.controller, ent->v.blending, iBone, ent );
	}

	return true;
}

qboolean SV_StudioExtractBbox( model_t *mod, int sequence, float *mins, float *maxs )
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
StudioTestToHitbox

test point trace in hitbox
================
*/
static qboolean SV_StudioTestToHitbox( trace_t *trace )
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
	trace->fraction = trace_realfraction = 0;
	trace->startsolid = trace->allsolid = true;

	return true;
}

/*
================
StudioClipToHitbox

trace hitbox
================
*/
static qboolean SV_StudioClipToHitbox( trace_t *trace )
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
SV_StudioIntersect

testing for potentially intersection of trace and animation bboxes
================
*/
static qboolean SV_StudioIntersect( edict_t *ent, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end )
{
	vec3_t	trace_mins, trace_maxs;
	vec3_t	anim_mins, anim_maxs;
	model_t	*mod = Mod_Handle( ent->v.modelindex );

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
	Q_memset( &trace, 0, sizeof( trace_t ));
	VectorCopy( end, trace.endpos );
	trace.fraction = trace_realfraction = 1.0f;
	trace.hitgroup = -1;

	if( !SV_StudioIntersect( ent, start, mins, maxs, end ))
		return trace;

	if( !SV_StudioSetupModel( ent, -1, false ))	// all bones used
		return trace;

	if( VectorCompare( start, end ))
		TraceHitbox = SV_StudioTestToHitbox;	// special case for test position
	else TraceHitbox = SV_StudioClipToHitbox;

	// go to check individual hitboxes		
	for( i = 0; i < sv_studiohdr->numhitboxes; i++ )
	{
		mstudiobbox_t	*phitbox = (mstudiobbox_t *)((byte*)sv_studiohdr + sv_studiohdr->hitboxindex) + i;
		matrix3x4		bonemat;

		// transform traceline into local bone space
		Matrix3x4_Invert_Simple( bonemat, sv_studiobones[phitbox->bone] );
		Matrix3x4_VectorTransform( bonemat, start, start_l );
		Matrix3x4_VectorTransform( bonemat, end, end_l );

		SV_HullForHitbox( phitbox->bbmin, phitbox->bbmax );

		VectorAdd( start_l, mins, trace_startmins );
		VectorAdd( start_l, maxs, trace_startmaxs );
		VectorAdd( end_l, mins, trace_endmins );
		VectorAdd( end_l, maxs, trace_endmaxs );

		if( TraceHitbox( &trace ))
		{
			outBone = phitbox->bone;
			trace.hitgroup = phitbox->group;
		}

		if( trace.allsolid )
			break;
	}

	// all hitboxes were swept, get trace result
	if( outBone >= 0 )
	{
		vec3_t	temp;

		trace.ent = ent;
		VectorCopy( trace.plane.normal, temp );
		trace.fraction = bound( 0, trace.fraction, 1.0f );
		VectorLerp( start, trace.fraction, end, trace.endpos );
		Matrix3x4_TransformPositivePlane( sv_studiobones[outBone], temp, trace.plane.dist, trace.plane.normal, &trace.plane.dist );
	}
	return trace;
}

void SV_StudioGetAttachment( edict_t *e, int iAttachment, float *org, float *ang )
{
	mstudioattachment_t		*pAtt;
	vec3_t			forward, bonepos;
	vec3_t			localOrg, localAng;
	void			*hdr;

	hdr = Mod_Extradata( Mod_Handle( e->v.modelindex ));
	if( !hdr ) return;

	sv_studiohdr = (studiohdr_t *)hdr;
	if( sv_studiohdr->numattachments <= 0 )
		return;

	if( sv_studiohdr->numattachments > MAXSTUDIOATTACHMENTS )
	{
		sv_studiohdr->numattachments = MAXSTUDIOATTACHMENTS; // reduce it
		MsgDev( D_WARN, "SV_StudioGetAttahment: too many attachments on %s\n", sv_studiohdr->name );
	}

	iAttachment = bound( 0, iAttachment, sv_studiohdr->numattachments );

	// calculate attachment origin and angles
	pAtt = (mstudioattachment_t *)((byte *)sv_studiohdr + sv_studiohdr->attachmentindex);

	SV_StudioSetupModel( e, pAtt[iAttachment].bone, true );

	// compute pos and angles
	Matrix3x4_VectorTransform( sv_studiobones[pAtt[iAttachment].bone], pAtt[iAttachment].org, localOrg );
	if( org ) VectorCopy( localOrg, org );

	if( sv_allow_studio_attachment_angles->integer )
	{
		Matrix3x4_OriginFromMatrix( sv_studiobones[pAtt[iAttachment].bone], bonepos );
		VectorSubtract( localOrg, bonepos, forward );	// make forward
		VectorNormalizeFast( forward );
		VectorAngles( forward, localAng );

		if( ang ) VectorCopy( localAng, ang );
	}
}

void SV_GetBonePosition( edict_t *e, int iBone, float *org, float *ang )
{
	if( !SV_StudioSetupModel( e, iBone, false ) || sv_studiohdr->numbones <= 0 )
		return;

	iBone = bound( 0, iBone, sv_studiohdr->numbones );
	if( org ) Matrix3x4_OriginFromMatrix( sv_studiobones[iBone], org );
	if( ang ) VectorAngles( sv_studiobones[iBone][0], ang ); // bone forward to angles
}

static sv_blending_interface_t gBlendAPI =
{
	SV_BLENDING_INTERFACE_VERSION,
	SV_StudioSetupBones,
};

static server_studio_api_t gStudioAPI =
{
	Mod_Calloc,
	Mod_CacheCheck,
	Mod_LoadCacheFile,
	Mod_Extradata,
};
   
/*
===============
SV_InitStudioAPI

Initialize server studio (blending interface)
===============
*/
qboolean SV_InitStudioAPI( void )
{
	static STUDIOAPI	pBlendIface;

	pBlendAPI = &gBlendAPI;

	pBlendIface = (STUDIOAPI)Com_GetProcAddress( svgame.hInstance, "Server_GetBlendingInterface" );
	if( pBlendIface && pBlendIface( SV_BLENDING_INTERFACE_VERSION, pBlendAPI, &gStudioAPI, sv_studiomatrix, sv_studiobones ))
		return true;

	// NOTE: we always return true even if game interface was not correct
	// because SetupBones is used for hitbox tracing on the server-side
	// just restore pointer to builtin function
	pBlendAPI = &gBlendAPI;

	return true;
}