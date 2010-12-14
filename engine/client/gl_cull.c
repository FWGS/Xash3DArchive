//=======================================================================
//			Copyright XashXT Group 2010 ©
//		       gl_cull.c - render culling routines
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"

qboolean R_CullBox( const vec3_t mins, const vec3_t maxs, uint clipflags )
{
	uint		i, bit;
	const mplane_t	*p;

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