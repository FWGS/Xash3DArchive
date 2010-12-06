//=======================================================================
//			Copyright XashXT Group 2010 ©
//		      gl_rlight.c - dynamic and static lights
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"

int	r_numdlights;

/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

/*
=============
R_MarkLights
=============
*/
void R_MarkLights( dlight_t *light, int bit, mnode_t *node )
{
	mplane_t		*splitplane;
	float		dist;
	msurface_t	*surf;
	int		i;
	
	if( node->contents < 0 )
		return;

	splitplane = node->plane;
	dist = DotProduct( light->origin, splitplane->normal ) - splitplane->dist;
	
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