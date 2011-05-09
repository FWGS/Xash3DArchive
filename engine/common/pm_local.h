/*
pm_local.h - player move interface
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
qboolean PM_TraceModel( playermove_t *pmove, physent_t *pe, const vec3_t p1, vec3_t mins, vec3_t maxs, const vec3_t p2, pmtrace_t *tr, int flags );
pmtrace_t PM_PlayerTrace( playermove_t *pm, vec3_t p1, vec3_t p2, int flags, int hull, int ignore_pe, pfnIgnore pfn );
int PM_TestPlayerPosition( playermove_t *pmove, vec3_t pos, pfnIgnore pmFilter );
int PM_HullPointContents( hull_t *hull, int num, const vec3_t p );

//
// pm_studio.c
//
void PM_InitStudioHull( void );
qboolean PM_StudioExtractBbox( model_t *mod, int sequence, float *mins, float *maxs );
qboolean PM_StudioTrace( playermove_t *pmove, physent_t *pe, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, pmtrace_t *ptr );

//
// pm_surface.c
//
const char *PM_TraceTexture( physent_t *pe, vec3_t vstart, vec3_t vend );

#endif//PM_LOCAL_H