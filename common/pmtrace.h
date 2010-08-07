//=======================================================================
//			Copyright XashXT Group 2010 ©
//		         pmtrace.h - shared physent trace
//=======================================================================
#ifndef PM_TRACE_H
#define PM_TRACE_H

typedef struct
{
	vec3_t	normal;
	float	dist;
} pmplane_t;

typedef struct pmtrace_s
{
	int		allsolid;		// if true, plane is not valid
	int		startsolid;	// if true, the initial point was in a solid area
	int		inopen;
	int		inwater;		// end point is in empty space or in water
	float		fraction;		// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;		// final position
	pmplane_t		plane;		// surface normal at impact
	int		ent;		// entity at impact
	vec3_t		deltavelocity;	// change in player's velocity caused by impact.  
					// only run on server.
	int		hitgroup;
} pmtrace_t;

#endif//PM_TRACE_H