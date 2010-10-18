//=======================================================================
//			Copyright XashXT Group 2010 ©
//		cm_light.c - lighting info for GETENTITYILLUM
//=======================================================================

#include "cm_local.h"
#include "edict.h"
#include "mathlib.h"

static vec3_t	cm_pointColor;
static float	cm_modulate;

/*
=================
CM_RecursiveLightPoint
=================
*/
static bool CM_RecursiveLightPoint( mnode_t *node, const vec3_t start, const vec3_t end )
{
	int		side;
	mplane_t		*plane;
	msurface_t	*surf;
	mtexinfo_t	*tex;
	vec3_t		mid, scale;
	float		front, back, frac;
	int		i, map, size, s, t;
	byte		*lm;

	// didn't hit anything
	if( !node->plane ) return false;

	// calculate mid point
	plane = node->plane;
	if( plane->type < 3 )
	{
		front = start[plane->type] - plane->dist;
		back = end[plane->type] - plane->dist;
	}
	else
	{
		front = DotProduct( start, plane->normal ) - plane->dist;
		back = DotProduct( end, plane->normal ) - plane->dist;
	}

	side = front < 0;
	if(( back < 0 ) == side )
		return CM_RecursiveLightPoint( node->children[side], start, end );

	frac = front / ( front - back );

	VectorLerp( start, frac, end, mid );

	// co down front side	
	if( CM_RecursiveLightPoint( node->children[side], start, mid ))
		return true; // hit something

	if(( back < 0 ) == side )
		return false;// didn't hit anything

	// check for impact on this node
	surf = node->firstface;

	for( i = 0; i < node->numfaces; i++, surf++ )
	{
		tex = surf->texinfo;

		if( surf->flags & SURF_DRAWTILED )
			continue;	// no lightmaps

		s = DotProduct( mid, tex->vecs[0] ) + tex->vecs[0][3] - surf->texturemins[0];
		t = DotProduct( mid, tex->vecs[1] ) + tex->vecs[1][3] - surf->texturemins[1];

		if(( s < 0 || s > surf->extents[0] ) || ( t < 0 || t > surf->extents[1] ))
			continue;

		s >>= 4;
		t >>= 4;

		if( !surf->samples )
			return true;

		VectorClear( cm_pointColor );

		lm = surf->samples + 3 * (t * ((surf->extents[0] >> 4) + 1) + s);
		size = ((surf->extents[0] >> 4) + 1) * ((surf->extents[1] >> 4) + 1) * 3;

		for( map = 0; map < surf->numstyles; map++ )
		{
			VectorScale( cm.lightstyle[surf->styles[map]].rgb, cm_modulate, scale );

			cm_pointColor[0] += lm[0] * scale[0];
			cm_pointColor[1] += lm[1] * scale[1];
			cm_pointColor[2] += lm[2] * scale[2];

			lm += size; // skip to next lightmap
		}
		return true;
	}

	// go down back side
	return CM_RecursiveLightPoint( node->children[!side], mid, end );
}

void CM_RunLightStyles( float time )
{
	int		i, ofs;
	clightstyle_t	*ls;
	float		l;

	if( !worldmodel ) return;

	// run lightstyles animation
	ofs = (time * 10);

	if( ofs == cm.lastofs ) return;
	cm.lastofs = ofs;

	for( i = 0, ls = cm.lightstyle; i < MAX_LIGHTSTYLES; i++, ls++ )
	{
		if( ls->length == 0 ) l = 0.0f;
		else if( ls->length == 1 ) l = ls->map[0];
		else l = ls->map[ofs%ls->length];

		VectorSet( ls->rgb, l, l, l );
	}
}

/*
==================
CM_AddLightStyle

needs to get correct working SV_LightPoint
==================
*/
void CM_SetLightStyle( int style, const char* s )
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
		VectorSet( cm.lightstyle[i].rgb, 1.0f, 1.0f, 1.0f );
	cm.lastofs = -1;
}

/*
==================
CM_LightEntity

grab the ambient lighting color for current point
==================
*/
int CM_LightEntity( edict_t *pEdict )
{
	vec3_t	start, end;

	if( !pEdict ) return 0;
	if( pEdict->v.effects & EF_FULLBRIGHT || !worldmodel->lightdata )
	{
		return 255;
	}

	if( pEdict->v.flags & FL_CLIENT )
	{
		// client has more precision light level
		// that come from client
		return pEdict->v.light_level;
	}

	VectorCopy( pEdict->v.origin, start );
	VectorCopy( pEdict->v.origin, end );

	if( pEdict->v.effects & EF_INVLIGHT )
		end[2] = start[2] + 8192;
	else end[2] = start[2] - 8192;
	VectorSet( cm_pointColor, 1.0f, 1.0f, 1.0f );

	cm_modulate = cm_lighting_modulate->value * (1.0 / 255);
	CM_RecursiveLightPoint( worldmodel->nodes, start, end );

	return VectorAvg( cm_pointColor );
}