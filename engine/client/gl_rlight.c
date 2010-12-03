//=======================================================================
//			Copyright XashXT Group 2010 ©
//		      gl_rlight.c - dynamic and static lights
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"

lightstyle_t	r_lightstyles[MAX_LIGHTSTYLES];
int		r_numdlights;
	
void R_LightForPoint( const vec3_t point, vec3_t ambientLight )
{
}

qboolean R_SetLightStyle( int stylenum, vec3_t color )
{
	return false;
}