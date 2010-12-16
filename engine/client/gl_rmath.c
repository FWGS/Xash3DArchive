//=======================================================================
//			Copyright XashXT Group 2010 ©
//			gl_rmath.c - renderer mathlib
//=======================================================================

#include "common.h"
#include "gl_local.h"
#include "mathlib.h"

#define FTABLE_SIZE_POW		10
#define NOISE_SIZE			256
#define FTABLE_SIZE			( 1<<FTABLE_SIZE_POW )
#define NOISE_VAL( a )	  	r_noiseperm[( a ) & ( NOISE_SIZE - 1 )]
#define FTABLE_CLAMP( x )		(((uint)( ( x )*FTABLE_SIZE ) & ( FTABLE_SIZE-1 )))
#define FTABLE_EVALUATE( table, x )	(( table )[FTABLE_CLAMP( x )] )
#define NOISE_INDEX( x, y, z, t )	NOISE_VAL( x + NOISE_VAL( y + NOISE_VAL( z + NOISE_VAL( t ) ) ) )
#define NOISE_LERP( a, b, w )		( a * ( 1.0f - w ) + b * w )

static float	r_sintableByte[256];
static float	r_sintable[FTABLE_SIZE];
static float	r_triangletable[FTABLE_SIZE];
static float	r_squaretable[FTABLE_SIZE];
static float	r_sawtoothtable[FTABLE_SIZE];
static float	r_inversesawtoothtable[FTABLE_SIZE];
static float	r_noisetable[NOISE_SIZE];
static int	r_noiseperm[NOISE_SIZE];
static float	r_warpsintable[256] =
{
#include "warpsin.h"
};

/*
==============
R_InitMathlib
==============
*/
void R_InitMathlib( void )
{
	int	i;
	float	t;

	// build lookup tables
	for( i = 0; i < FTABLE_SIZE; i++ )
	{
		t = (float)i / (float)FTABLE_SIZE;

		r_sintable[i] = com.sin( t * M_PI2 );

		if( t < 0.25f ) r_triangletable[i] = t * 4.0f;
		else if( t < 0.75f ) r_triangletable[i] = 2.0f - 4.0f * t;
		else r_triangletable[i] = ( t - 0.75f ) * 4.0f - 1.0f;

		if( t < 0.5f ) r_squaretable[i] = 1.0f;
		else r_squaretable[i] = -1.0f;

		r_sawtoothtable[i] = t;
		r_inversesawtoothtable[i] = 1.0f - t;
	}

	for( i = 0; i < 256; i++ )
		r_sintableByte[i] = com.sin((float)i / 255.0f * M_PI2 );

	// init the noise table
	for( i = 0; i < NOISE_SIZE; i++ )
	{
		r_noisetable[i] = Com_RandomFloat( -1.0f, 1.0f );
		r_noiseperm[i] = Com_RandomLong( 0, 255 );
	}
}

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

/*
=============
R_NormToLatLong
=============
*/
void R_NormToLatLong( const vec3_t normal, byte latlong[2] )
{
	// can't do atan2 (normal[1], normal[0])
	if( normal[0] == 0 && normal[1] == 0 )
	{
		if( normal[2] > 0 )
		{
			latlong[0] = 0;	// acos ( 1 )
			latlong[1] = 0;
		}
		else
		{
			latlong[0] = 128;	// acos ( -1 )
			latlong[1] = 0;
		}
	}
	else
	{
		int	angle;

		angle = (int)(com.acos( normal[2]) * 255.0 / M_PI2 ) & 255;
		latlong[0] = angle;
		angle = (int)(com.atan2( normal[1], normal[0] ) * 255.0 / M_PI2 ) & 255;
		latlong[1] = angle;
	}
}

/*
=============
R_LatLongToNorm
=============
*/
void R_LatLongToNorm( const byte latlong[2], vec3_t normal )
{
	float	sin_a, sin_b, cos_a, cos_b;

	cos_a = r_sintableByte[(latlong[0] + 64) & 255];
	sin_a = r_sintableByte[latlong[0]];
	cos_b = r_sintableByte[(latlong[1] + 64) & 255];
	sin_b = r_sintableByte[latlong[1]];

	VectorSet( normal, cos_b * sin_a, sin_b * sin_a, cos_a );
}

byte R_FloatToByte( float x )
{
	union {
		float f;
		uint i;
	} f2i;

	// shift float to have 8bit fraction at base of number
	f2i.f = x + 32768.0f;
	f2i.i &= 0x7FFFFF;

	// then read as integer and kill float bits...
	return ( byte )min( f2i.i, 255 );
}