//=======================================================================
//			Copyright XashXT Group 2010 ©
//			gl_rmath.c - renderer mathlib
//=======================================================================

#include "common.h"
#include "gl_local.h"
#include "mathlib.h"

/*
====================
V_CalcFov
====================
*/
float V_CalcFov( float *fov_x, float width, float height )
{
	float	x, half_fov_y;

	if( *fov_x < 1 || *fov_x > 179 )
	{
		MsgDev( D_ERROR, "V_CalcFov: bad fov %g!\n", *fov_x );
		*fov_x = 90;
	}

	x = width / com.tan( DEG2RAD( *fov_x ) * 0.5f );
	half_fov_y = atan( height / x );

	return RAD2DEG( half_fov_y ) * 2;
}

/*
====================
V_AdjustFov
====================
*/
void V_AdjustFov( float *fov_x, float *fov_y, float width, float height, qboolean lock_x )
{
	float x, y;

	if( width * 3 == 4 * height || width * 4 == height * 5 )
	{
		// 4:3 or 5:4 ratio
		return;
	}

	if( lock_x )
	{
		*fov_y = 2 * atan((width * 3) / (height * 4) * com.tan( *fov_y * M_PI / 360.0 * 0.5 )) * 360 / M_PI;
		return;
	}

	y = V_CalcFov( fov_x, 640, 480 );
	x = *fov_x;

	*fov_x = V_CalcFov( &y, height, width );
	if( *fov_x < x ) *fov_x = x;
	else *fov_y = y;
}