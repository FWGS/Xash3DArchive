//=======================================================================
//			Copyright XashXT Group 2010 ©
//		      dlight.h - dynamic light declaration
//=======================================================================
#ifndef DLIGHT_H
#define DLIGHT_H

typedef struct dlight_s
{
	vec3_t		origin;
	float		radius;
	byte		color[3];
	float		die;	// stop lighting after this time
	float		decay;	// drop this each second
	float		minlight;	// don't add when contributing less
	int		key;
	int		dark;	// subtracts light instead of adding
	int		elight;	// true when calls with CL_AllocElight
} dlight_t;

#endif//DLIGHT_H