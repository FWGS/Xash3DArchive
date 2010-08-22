//=======================================================================
//			Copyright XashXT Group 2010 ©
//			pm_local.h - pmove interface
//=======================================================================
#ifndef PM_LOCAL_H
#define PM_LOCAL_H

#include "pm_defs.h"

typedef int (*pfnIgnore)( physent_t *pe );	// custom trace filter

//
// pm_trace.c
//
void Pmove_Init( void );
void PM_InitBoxHull( void );
hull_t *PM_HullForBsp( physent_t *pe, const vec3_t mins, const vec3_t maxs, float *offset );
bool PM_TraceModel( physent_t *pe, const vec3_t p1, vec3_t mins, vec3_t maxs, const vec3_t p2, pmtrace_t *tr, int flags );
pmtrace_t PM_PlayerTrace( playermove_t *pm, vec3_t p1, vec3_t p2, int flags, int hull, int ignore_pe, pfnIgnore pfn );
int PM_HullPointContents( hull_t *hull, int num, const vec3_t p );

//
// pm_studio.c
//
void PM_InitStudioHull( void );
bool PM_StudioTrace( physent_t *pe, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, pmtrace_t *ptr );

//
// pm_surface.c
//
const char *PM_TraceTexture( physent_t *pe, vec3_t vstart, vec3_t vend );

#endif//PM_LOCAL_H