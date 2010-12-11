//=======================================================================
//			Copyright XashXT Group 2010 ©
//		      gl_rlight.c - dynamic and static lights
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"

/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/
/*
==================
R_AnimateLight

==================
*/
void R_AnimateLight( void )
{
	int		i, k, flight, clight;
	float		l, lerpfrac, backlerp;
	lightstyle_t	*ls;

	// light animations
	// 'm' is normal light, 'a' is no light, 'z' is double bright
	flight = (int)floor( cl.time * 10 );
	clight = (int)ceil( cl.time * 10 );
	lerpfrac = ( cl.time * 10 ) - flight;
	backlerp = 1.0f - lerpfrac;

	for( i = 0, ls = cl.lightstyles; i < MAX_LIGHTSTYLES; i++, ls++ )
	{
		if( r_fullbright->integer || !cl.worldmodel->lightdata )
		{
			RI.lightstylevalue[i] = 256 * 256;
			continue;
		}

		if( !ls->length )
		{
			// was 256, changed to 264 for consistency
			RI.lightstylevalue[i] = 256 * r_lighting_modulate->value;
			continue;
		}
		else if( ls->length == 1 )
		{
			// single length style so don't bother interpolating
			RI.lightstylevalue[i] = ls->map[0] * 22 * r_lighting_modulate->value;
			continue;
		}
		else if( !ls->interp || !cl_lightstyle_lerping->integer )
		{
			RI.lightstylevalue[i] = ls->map[flight%ls->length] * 22 * r_lighting_modulate->value;
			continue;
		}

		// interpolate animating light
		// frame just gone
		k = ls->map[flight % ls->length];
		l = (float)( k * 22 ) * backlerp;

		// upcoming frame
		k = ls->map[clight % ls->length];
		l += (float)( k * 22 ) * lerpfrac;

		RI.lightstylevalue[i] = (int)l * r_lighting_modulate->value;
	}
}

/*
=============
R_MarkLights
=============
*/
void R_MarkLights( dlight_t *light, int bit, mnode_t *node )
{
	float		dist;
	msurface_t	*surf;
	int		i;
	
	if( node->contents < 0 )
		return;

	dist = PlaneDiff( light->origin, node->plane );

	if( dist > light->radius )
	{
		R_MarkLights( light, bit, node->children[0] );
		return;
	}
	if( dist < -light->radius )
	{
		R_MarkLights( light, bit, node->children[1] );
		return;
	}
		
	// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;

	for( i = 0; i < node->numsurfaces; i++, surf++ )
	{
		if( surf->dlightframe != tr.dlightframecount )
		{
			surf->dlightbits = 0;
			surf->dlightframe = tr.dlightframecount;
		}
		surf->dlightbits |= bit;
	}

	R_MarkLights( light, bit, node->children[0] );
	R_MarkLights( light, bit, node->children[1] );
}


/*
=============
R_PushDlights
=============
*/
void R_PushDlights( void )
{
	dlight_t	*l;
	int	i;

	tr.dlightframecount = tr.framecount + 1;	// because the count hasn't
						// advanced yet for this frame
	l = cl_dlights;

	for( i = 0; i < MAX_DLIGHTS; i++, l++ )
	{
		if( l->die < cl.time || !l->radius )
			continue;
		R_MarkLights( l, 1<<i, cl.worldmodel->nodes );
	}
}
	
void R_LightForPoint( const vec3_t point, vec3_t ambientLight )
{
}