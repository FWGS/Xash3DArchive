//=======================================================================
//			Copyright XashXT Group 2010 ©
//		       gl_cull.c - render culling routines
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"

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
qboolean R_CullBox( const vec3_t mins, const vec3_t maxs, uint clipflags )
{
	uint		i, bit;
	const mplane_t	*p;

	// client.dll may use additional passes for render custom mirrors etc
	if( r_nocull->integer )
		return false;

	for( i = sizeof( RI.frustum ) / sizeof( RI.frustum[0] ), bit = 1, p = RI.frustum; i > 0; i--, bit<<=1, p++ )
	{
		if( !( clipflags & bit ))
			continue;

		switch( p->signbits )
		{
		case 0:
			if( p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist )
				return true;
			break;
		case 1:
			if( p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist )
				return true;
			break;
		case 2:
			if( p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist )
				return true;
			break;
		case 3:
			if( p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist )
				return true;
			break;
		case 4:
			if( p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist )
				return true;
			break;
		case 5:
			if( p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist )
				return true;
			break;
		case 6:
			if( p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist )
				return true;
			break;
		case 7:
			if( p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist )
				return true;
			break;
		default:
			return false;
		}
	}
	return false;
}

/*
=================
R_CullSphere

Returns true if the sphere is completely outside the frustum
=================
*/
qboolean R_CullSphere( const vec3_t centre, const float radius, const uint clipflags )
{
	uint	i, bit;
	const mplane_t *p;

	// client.dll may use additional passes for render custom mirrors etc
	if( r_nocull->integer )
		return false;

	for( i = sizeof( RI.frustum ) / sizeof( RI.frustum[0] ), bit = 1, p = RI.frustum; i > 0; i--, bit<<=1, p++ )
	{
		if(!( clipflags & bit )) continue;
		if( DotProduct( centre, p->normal ) - p->dist <= -radius )
			return true;
	}
	return false;
}

/*
===================
R_VisCullBox
===================
*/
qboolean R_VisCullBox( const vec3_t mins, const vec3_t maxs )
{
	int	s, stackdepth = 0;
	vec3_t	extmins, extmaxs;
	mnode_t	*node, *localstack[4096];

	if( !cl.worldmodel || !RI.drawWorld )
		return false;

	if( r_novis->integer )
		return false;

	for( s = 0; s < 3; s++ )
	{
		extmins[s] = mins[s] - 4;
		extmaxs[s] = maxs[s] + 4;
	}

	for( node = cl.worldmodel->nodes; node; )
	{
		if( node->visframe != tr.visframecount )
		{
			if( !stackdepth )
				return true;
			node = localstack[--stackdepth];
			continue;
		}

		if( node->contents < 0 )
			return false;

		s = BOX_ON_PLANE_SIDE( extmins, extmaxs, node->plane ) - 1;

		if( s < 2 )
		{
			node = node->children[s];
			continue;
		}

		// go down both sides
		if( stackdepth < sizeof( localstack ) / sizeof( mnode_t* ))
			localstack[stackdepth++] = node->children[0];
		node = node->children[1];
	}

	return true;
}

/*
===================
R_VisCullSphere
===================
*/
qboolean R_VisCullSphere( const vec3_t origin, float radius )
{
	float	dist;
	int	stackdepth = 0;
	mnode_t	*node, *localstack[4096];

	if( !cl.worldmodel || !RI.drawWorld )
		return false;
	if( r_novis->integer )
		return false;

	radius += 4;
	for( node = cl.worldmodel->nodes; ; )
	{
		if( node->visframe != tr.visframecount )
		{
			if( !stackdepth )
				return true;
			node = localstack[--stackdepth];
			continue;
		}

		if( node->contents < 0 )
			return false;

		dist = PlaneDiff( origin, node->plane );
		if( dist > radius )
		{
			node = node->children[0];
			continue;
		}
		else if( dist < -radius )
		{
			node = node->children[1];
			continue;
		}

		// go down both sides
		if( stackdepth < sizeof( localstack ) / sizeof( mnode_t* ))
			localstack[stackdepth++] = node->children[0];
		node = node->children[1];
	}
	return true;
}

/*
=============
R_CullModel
=============
*/
int R_CullModel( cl_entity_t *e, vec3_t origin, vec3_t mins, vec3_t maxs, float radius )
{
	if( e == &clgame.viewent )
	{
		if( RI.params & RP_NONVIEWERREF )
			return 1;
		return 0;
	}

	// don't reflect this entity in mirrors
	if( e->curstate.effects & EF_NOREFLECT && RI.params & RP_MIRRORVIEW )
		return 1;

	// draw only in mirrors
	if( e->curstate.effects & EF_REFLECTONLY && !( RI.params & RP_MIRRORVIEW ))
		return 1;

	if( RP_LOCALCLIENT( e ) && !RI.thirdPerson && cl.refdef.viewentity == ( cl.playernum + 1 ))
	{
		if(!( RI.params & ( RP_MIRRORVIEW|RP_SHADOWMAPVIEW )))
			return 1;
	}

	if( R_CullSphere( origin, radius, RI.clipFlags ))
		return 1;

	if( RI.rdflags & ( RDF_PORTALINVIEW|RDF_SKYPORTALINVIEW ) || ( RI.params & RP_SKYPORTALVIEW ))
	{
		if( R_VisCullSphere( origin, radius ))
			return 2;
	}

	return 0;
}