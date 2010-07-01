//=======================================================================
//			Copyright XashXT Group 2010 ©
//		cm_light.c - lighting info for GETENTITYILLUM
//=======================================================================

#include "cm_local.h"
#include "mathlib.h"

int CM_RecursiveLightPoint( vec3_t color, cnode_t *node, const vec3_t start, const vec3_t end )
{
	float	front, back, frac;
	vec3_t	mid;

loc0:
	if( node->contents < 0 )
		return false;	// didn't hit anything

	// calculate mid point
	if( node->plane->type < 3 )
	{
		front = start[node->plane->type] - node->plane->dist;
		back = end[node->plane->type] - node->plane->dist;
	}
	else
	{
		front = DotProduct( start, node->plane->normal ) - node->plane->dist;
		back = DotProduct( end, node->plane->normal ) - node->plane->dist;
	}

	// optimized recursion
	if(( back < 0 ) == ( front < 0 ))
	{
		node = node->children[front < 0];
		goto loc0;
	}

	frac = front / ( front - back );
	VectorLerp( start, frac, end, mid );

	// go down front side
	if( CM_RecursiveLightPoint( color, node->children[front < 0], start, mid ))
	{
		// hit something
		return true;
	}
	else
	{
		int		i, ds, dt;
		csurface_t	*surf;

		// check for impact on this node
		for( i = 0, surf = node->firstface; i < node->numfaces; i++, surf++ )
		{
			if( surf->flags & SURF_DRAWTILED )
				continue;	// no lightmaps

			ds = (int)((float)DotProduct( mid, surf->texinfo->vecs[0] ) + surf->texinfo->vecs[0][3] );
			dt = (int)((float)DotProduct( mid, surf->texinfo->vecs[1] ) + surf->texinfo->vecs[1][3] );

			if( ds < surf->textureMins[0] || dt < surf->textureMins[1] )
				continue;

			ds -= surf->textureMins[0];
			dt -= surf->textureMins[1];

			if( ds > surf->extents[0] || dt > surf->extents[1] )
				continue;

			if( surf->samples )
			{
				// enhanced to interpolate lighting
				byte	*lightmap;
				int	maps, line3, dsfrac = ds & 15, dtfrac = dt & 15;
				int	r00 = 0, g00 = 0, b00 = 0, r01 = 0, g01 = 0, b01 = 0;
				int	r10 = 0, g10 = 0, b10 = 0, r11 = 0, g11 = 0, b11 = 0;
				float	scale;

				line3 = ((surf->extents[0] >> 4) + 1) * 3;
				lightmap = surf->samples + ((dt >> 4) * ((surf->extents[0] >> 4) + 1) + (ds >> 4)) * 3;

				for( maps = 0; maps < LM_STYLES && surf->styles[maps] != 255; maps++ )
				{
					scale = (float)cm.lightstyle[surf->styles[maps]].value;
					r00 += (float)lightmap[0] * scale;
					g00 += (float)lightmap[1] * scale;
					b00 += (float)lightmap[2] * scale;

					r01 += (float)lightmap[3] * scale;
					g01 += (float)lightmap[4] * scale;
					b01 += (float)lightmap[5] * scale;

					r10 += (float)lightmap[line3+0] * scale;
					g10 += (float)lightmap[line3+1] * scale;
					b10 += (float)lightmap[line3+2] * scale;

					r11 += (float)lightmap[line3+3] * scale;
					g11 += (float)lightmap[line3+4] * scale;
					b11 += (float)lightmap[line3+5] * scale;

					lightmap += ((surf->extents[0] >> 4) + 1) * ((surf->extents[1] >> 4) + 1) * 3;
				}
				color[0] += (float)((int)((((((((r11 - r10) * dsfrac) >> 4) + r10) 
					- ((((r01 - r00) * dsfrac) >> 4) + r00)) * dtfrac) >> 4) 
					+ ((((r01 - r00) * dsfrac) >> 4) + r00)));
				color[1] += (float)((int)((((((((g11 - g10) * dsfrac) >> 4) + g10) 
					- ((((g01 - g00) * dsfrac) >> 4) + g00)) * dtfrac) >> 4) 
					+ ((((g01 - g00) * dsfrac) >> 4) + g00)));
				color[2] += (float)((int)((((((((b11 - b10) * dsfrac) >> 4) + b10) 
					- ((((b01 - b00) * dsfrac) >> 4) + b00)) * dtfrac) >> 4) 
					+ ((((b01 - b00) * dsfrac) >> 4) + b00)));
			}
			return true; // success
		}

		// go down back side
		return CM_RecursiveLightPoint( color, node->children[front >= 0], mid, end );
	}
}

void CM_RunLightStyles( float time )
{
	int		i, ofs;
	clightstyle_t	*ls;

	if( !sv_models[1] )
		return;	// no world

	// run lightstyles animation
	ofs = (time * 10);

	if( ofs == cm.lastofs ) return;
	cm.lastofs = ofs;

	for( i = 0, ls = cm.lightstyle; i < MAX_LIGHTSTYLES; i++, ls++ )
	{
		if( ls->length == 0 ) ls->value = 0.0f;
		else if( ls->length == 1 ) ls->value = ls->map[0];
		else ls->value = ls->map[ofs%ls->length];
	}
}

/*
==================
CM_AddLightstyle

needs to get correct working SV_LightPoint
==================
*/
void CM_AddLightstyle( int style, const char* s )
{
	int	j, k;

	j = com.strlen( s );
	cm.lightstyle[style].length = j;

	for( k = 0; k < j; k++ )
		cm.lightstyle[style].map[k] = (float)( s[k]-'a' ) / (float)( 'm'-'a' );
}

void CM_ClearLightStyles( void )
{
	clightstyle_t	*ls;	
	int		i;

	Mem_Set( cm.lightstyle, 0, sizeof( cm.lightstyle ));

	for( i = 0, ls = cm.lightstyle; i < MAX_LIGHTSTYLES; i++, ls++ )
		cm.lightstyle[i].value = 1.0f;
	cm.lastofs = -1;
}

/*
==================
CM_LightPoint

grab the ambient lighting color for current point
==================
*/
int CM_LightPoint( edict_t *pEdict )
{
	vec3_t	start, end, color;

	if( !pEdict ) return 0;
	if( pEdict->v.effects & EF_FULLBRIGHT || !worldmodel->lightdata )
	{
		return 255;
	}

	VectorCopy( pEdict->v.origin, start );
	VectorCopy( pEdict->v.origin, end );

	if( pEdict->v.effects & EF_INVLIGHT )
		end[2] = start[2] + 4096;
	else end[2] = start[2] - 4096;

	VectorClear( color );	
	CM_RecursiveLightPoint( color, worldmodel->nodes, start, end );

	return VectorAvg( color );
}