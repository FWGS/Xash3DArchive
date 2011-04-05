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
hull_t *PM_HullForBsp( physent_t *pe, playermove_t *pmove, float *offset );
qboolean PM_TraceModel( physent_t *pe, const vec3_t p1, vec3_t mins, vec3_t maxs, const vec3_t p2, pmtrace_t *tr, int flags );
pmtrace_t PM_PlayerTrace( playermove_t *pm, vec3_t p1, vec3_t p2, int flags, int hull, int ignore_pe, pfnIgnore pfn );
int PM_TestPlayerPosition( playermove_t *pmove, vec3_t pos, pfnIgnore pmFilter );
int PM_HullPointContents( hull_t *hull, int num, const vec3_t p );

//
// pm_studio.c
//
void PM_InitStudioHull( void );
qboolean PM_StudioExtractBbox( model_t *mod, int sequence, float *mins, float *maxs );
qboolean PM_StudioTrace( physent_t *pe, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, pmtrace_t *ptr, qboolean allow_scale );

//
// pm_surface.c
//
const char *PM_TraceTexture( physent_t *pe, vec3_t vstart, vec3_t vend );

#endif//PM_LOCAL_H