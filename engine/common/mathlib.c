//=======================================================================
//			Copyright XashXT Group 2010 ©
//			mathlib.c - internal mathlib
//=======================================================================

#include "common.h"
#include "mathlib.h"

/*
=================
anglemod
=================
*/
float anglemod( const float a )
{
	return (360.0f/65536) * ((int)(a*(65536/360.0f)) & 65535);
}

/*
=================
rsqrt
=================
*/
float rsqrt( float number )
{
	int	i;
	float	x, y;

	if( number == 0.0f )
		return 0.0f;

	x = number * 0.5f;
	i = *(int *)&number;	// evil floating point bit level hacking
	i = 0x5f3759df - (i >> 1);	// what the fuck?
	y = *(float *)&i;
	y = y * (1.5f - (x * y * y));	// first iteration

	return y;
}

float VectorNormalizeLength2( const vec3_t v, vec3_t out )
{
	float	length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = com.sqrt( length );

	if( length )
	{
		ilength = 1.0f / length;
		out[0] = v[0] * ilength;
		out[1] = v[1] * ilength;
		out[2] = v[2] * ilength;
	}
	else out[0] = out[1] = out[2] = 0.0f;

	return length;
}

void VectorVectors( vec3_t forward, vec3_t right, vec3_t up )
{
	float d;

	right[0] = forward[2];
	right[1] = -forward[0];
	right[2] = forward[1];

	d = DotProduct(forward, right);
	VectorMA(right, -d, forward, right);
	VectorNormalize(right);
	CrossProduct(right, forward, up);
}

void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up )
{
	float	angle, sr, sp, sy, cr, cp, cy;

	angle = angles[YAW] * (M_PI*2 / 360);
	com.sincos( angle, &sy, &cy );
	angle = angles[PITCH] * (M_PI*2 / 360);
	com.sincos( angle, &sp, &cp );
	if( forward )
	{
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;
	}
	if( right || up )
	{
		if( angles[ROLL] )
		{
			angle = angles[ROLL] * (M_PI*2 / 360);
			com.sincos( angle, &sr, &cr );
			if( right )
			{
				right[0] = -1*(sr*sp*cy+cr*-sy);
				right[1] = -1*(sr*sp*sy+cr*cy);
				right[2] = -1*(sr*cp);
			}
			if( up )
			{
				up[0] = (cr*sp*cy+-sr*-sy);
				up[1] = (cr*sp*sy+-sr*cy);
				up[2] = cr*cp;
			}
		}
		else
		{
			if( right )
			{
				right[0] = sy;
				right[1] = -cy;
				right[2] = 0;
			}
			if( up )
			{
				up[0] = (sp*cy);
				up[1] = (sp*sy);
				up[2] = cp;
			}
		}
	}
}

//
// bounds operations
//
/*
=================
ClearBounds
=================
*/
void ClearBounds( vec3_t mins, vec3_t maxs )
{
	// make bogus range
	mins[0] = mins[1] = mins[2] =  999999;
	maxs[0] = maxs[1] = maxs[2] = -999999;
}

/*
=================
AddPointToBounds
=================
*/
void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs )
{
	float	val;
	int	i;

	for( i = 0; i < 3; i++ )
	{
		val = v[i];
		if( val < mins[i] ) mins[i] = val;
		if( val > maxs[i] ) maxs[i] = val;
	}
}

/*
=================
BoundsIntersect
=================
*/
qboolean BoundsIntersect( const vec3_t mins1, const vec3_t maxs1, const vec3_t mins2, const vec3_t maxs2 )
{
	if( mins1[0] > maxs2[0] || mins1[1] > maxs2[1] || mins1[2] > maxs2[2] )
		return false;
	if( maxs1[0] < mins2[0] || maxs1[1] < mins2[1] || maxs1[2] < mins2[2] )
		return false;
	return true;
}

/*
=================
BoundsAndSphereIntersect
=================
*/
qboolean BoundsAndSphereIntersect( const vec3_t mins, const vec3_t maxs, const vec3_t origin, float radius )
{
	if( mins[0] > origin[0] + radius || mins[1] > origin[1] + radius || mins[2] > origin[2] + radius )
		return false;
	if( maxs[0] < origin[0] - radius || maxs[1] < origin[1] - radius || maxs[2] < origin[2] - radius )
		return false;
	return true;
}

//
// studio utils
//
/*
====================
AngleQuaternion

====================
*/
void AngleQuaternion( const vec3_t angles, vec4_t q )
{
	float	angle;
	float	sr, sp, sy, cr, cp, cy;

	angle = angles[2] * 0.5f;
	com.sincos( angle, &sy, &cy );
	angle = angles[1] * 0.5f;
	com.sincos( angle, &sp, &cp );
	angle = angles[0] * 0.5f;
	com.sincos( angle, &sr, &cr );

	q[0] = sr * cp * cy - cr * sp * sy; // X
	q[1] = cr * sp * cy + sr * cp * sy; // Y
	q[2] = cr * cp * sy - sr * sp * cy; // Z
	q[3] = cr * cp * cy + sr * sp * sy; // W
}

/*
====================
QuaternionSlerp

====================
*/
void QuaternionSlerp( const vec4_t p, vec4_t q, float t, vec4_t qt )
{
	float	omega, cosom, sinom, sclp, sclq;
	int	i;

	// decide if one of the quaternions is backwards
	float a = 0;
	float b = 0;

	for( i = 0; i < 4; i++ )
	{
		a += (p[i] - q[i]) * (p[i] - q[i]);
		b += (p[i] + q[i]) * (p[i] + q[i]);
	}

	if( a > b )
	{
		for( i = 0; i < 4; i++ )
		{
			q[i] = -q[i];
		}
	}

	cosom = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3];

	if(( 1.0 + cosom ) > 0.000001f )
	{
		if(( 1.0 - cosom ) > 0.000001f )
		{
			omega = com.acos( cosom );
			sinom = com.sin( omega );
			sclp = com.sin(( 1.0f - t ) * omega ) / sinom;
			sclq = com.sin( t * omega ) / sinom;
		}
		else
		{
			sclp = 1.0f - t;
			sclq = t;
		}

		for( i = 0; i < 4; i++ )
			qt[i] = sclp * p[i] + sclq * q[i];
	}
	else
	{
		qt[0] = -q[1];
		qt[1] = q[0];
		qt[2] = -q[3];
		qt[3] = q[2];
		sclp = com.sin(( 1.0f - t ) * ( 0.5f * M_PI ));
		sclq = com.sin( t * ( 0.5f * M_PI ));

		for( i = 0; i < 3; i++ )
			qt[i] = sclp * p[i] + sclq * qt[i];
	}
}

vec3_t	vec3_origin = { 0, 0, 0 };