//=======================================================================
//			Copyright XashXT Group 2009 ©
//		trace_def.h - shared client\server trace struct
//=======================================================================
#ifndef TRACE_DEF_H
#define TRACE_DEF_H

#define FTRACE_SIMPLEBOX	(1<<0)		// traceline with a simple box
#define FTRACE_IGNORE_GLASS	(1<<1)		// traceline will be ignored entities with rendermode != kRenderNormal

typedef enum { point_hull = 0, human_hull = 1, large_hull = 2, head_hull = 3 };
typedef enum { ignore_monsters = 1, dont_ignore_monsters = 0, missile = 2 } IGNORE_MONSTERS;
typedef enum { ignore_glass = 1, dont_ignore_glass = 0 } IGNORE_GLASS;

typedef struct
{
	int		fAllSolid;	// if true, plane is not valid
	int		fStartSolid;	// if true, the initial point was in a solid area
	int		fInOpen;		// if true trace is open
	int		fInWater;		// if true trace is in water
	float		flFraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		vecEndPos;	// final position
	float		flPlaneDist;	// planes distance
	vec3_t		vecPlaneNormal;	// surface normal at impact
	edict_t		*pHit;		// entity the surface is on
	int		iHitgroup;	// 0 == generic, non zero is specific body part
} TraceResult;

/*
==============================================================================

ENGINE TRACE FORMAT
==============================================================================
*/
typedef struct clipnode_s
{
	int		planenum;
	short		children[2];	// negative numbers are contents
} clipnode_t;

typedef struct cplane_s
{
	vec3_t		normal;
	float		dist;
	short		type;		// for fast side tests
	short		signbits;		// signx + (signy<<1) + (signz<<1)
} cplane_t;

typedef struct
{
	clipnode_t	*clipnodes;
	cplane_t		*planes;
	int		firstclipnode;
	int		lastclipnode;
	vec3_t		clip_mins;
	vec3_t		clip_maxs;
} chull_t;

#endif//TRACE_DEF_H