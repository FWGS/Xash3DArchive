//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_trace.c - combined tracing
//=======================================================================

#include "cm_local.h"
#include "mathlib.h"

/*
==============
BoxOnPlaneSide (engine fast version)

Returns SIDE_FRONT, SIDE_BACK, or SIDE_ON
==============
*/
int CM_BoxOnPlaneSide( const vec3_t emins, const vec3_t emaxs, const cplane_t *p )
{
	if( p->type < 3 ) return ((emaxs[p->type] >= p->dist) | ((emins[p->type] < p->dist) << 1));
	switch( p->signbits )
	{
	default:
	case 0: return (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) >= p->dist) | (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 1: return (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) >= p->dist) | (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 2: return (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) >= p->dist) | (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 3: return (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) >= p->dist) | (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 4: return (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) >= p->dist) | (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	case 5: return (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) >= p->dist) | (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	case 6: return (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) >= p->dist) | (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	case 7: return (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) >= p->dist) | (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	}
}

/*
==================
CM_PointLeafnum_r

==================
*/
int CM_PointLeafnum_r( const vec3_t p, cnode_t *node )
{
	cleaf_t	*leaf;

	// find which leaf the point is in
	while( node->plane )
		node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct(p, node->plane->normal)) < node->plane->dist];
	leaf = (cleaf_t *)node;

	return leaf - worldmodel->leafs - 1;
}

int CM_PointLeafnum( const vec3_t p )
{
	// map not loaded
	if ( !worldmodel ) return 0;
	return CM_PointLeafnum_r( p, worldmodel->nodes );
}


/*
======================================================================

LEAF LISTING

======================================================================
*/
void CM_BoxLeafnums_r( leaflist_t *ll, cnode_t *node )
{
	cplane_t	*plane;
	int	s;

	while( 1 )
	{
		if( node->contents == CONTENTS_SOLID )
			return;

		if( node->contents < 0 )
		{
			cleaf_t	*leaf = (cleaf_t *)node;

			// it's a leaf!
			if( ll->count >= ll->maxcount )
			{
				ll->overflowed = true;
				return;
			}

			ll->list[ll->count++] = leaf - worldmodel->leafs - 1;
			return;
		}
	
		plane = node->plane;
		s = CM_BoxOnPlaneSide( ll->mins, ll->maxs, plane );

		if( s == 1 )
		{
			node = node->children[0];
		}
		else if( s == 2 )
		{
			node = node->children[1];
		}
		else
		{
			// go down both
			if( ll->topnode == -1 )
				ll->topnode = node - worldmodel->nodes;
			CM_BoxLeafnums_r( ll, node->children[0] );
			node = node->children[1];
		}
	}
}

/*
==================
CM_BoxLeafnums
==================
*/
int CM_BoxLeafnums( const vec3_t mins, const vec3_t maxs, short *list, int listsize, int *topnode )
{
	leaflist_t	ll;

	if( !worldmodel ) return 0;

	cm.checkcount++;
	VectorCopy( mins, ll.mins );
	VectorCopy( maxs, ll.maxs );
	ll.count = 0;
	ll.maxcount = listsize;
	ll.list = list;
	ll.topnode = -1;
	ll.overflowed = false;

	CM_BoxLeafnums_r( &ll, worldmodel->nodes );

	if( topnode ) *topnode = ll.topnode;
	return ll.count;
}

/*
=============
CM_HeadnodeVisible_r
=============
*/
bool CM_HeadnodeVisible_r( cnode_t *node, byte *visbits )
{
	cleaf_t	*leaf;
	int	leafnum;

	if( node->contents < 0 )
	{
		if( node->contents != CONTENTS_SOLID )
		{
			leaf = (cleaf_t *)node;
			leafnum = (leaf - worldmodel->leafs - 1);

			if( visbits[leafnum>>3] & (1<<( leafnum & 7 )))
				return true;
		}
		return false;
	}
	
	if( CM_HeadnodeVisible_r( node->children[0], visbits ))
		return true;
	return CM_HeadnodeVisible_r( node->children[1], visbits );
}

/*
=============
CM_HeadnodeVisible

returns true if any leaf under headnode 
is potentially visible
=============
*/
bool CM_HeadnodeVisible( int nodenum, byte *visbits )
{
	cnode_t	*node;

	if( !worldmodel ) return false;
	if( nodenum == -1 ) return false;

	node = (cnode_t *)worldmodel->nodes + nodenum;

	return CM_HeadnodeVisible_r( node, visbits );
}

/*
=============
CM_BoxVisible

Returns true if any leaf in boxspace
is potentially visible
=============
*/
bool CM_BoxVisible( const vec3_t mins, const vec3_t maxs, byte *visbits )
{
	short	leafList[MAX_BOX_LEAFS];
	int	i, count;

	if( !visbits || !mins || !maxs )
		return true;

	// FIXME: Could save a loop here by traversing the tree in this routine like the code above
	count = CM_BoxLeafnums( mins, maxs, leafList, MAX_BOX_LEAFS, NULL );

	for( i = 0; i < count; i++ )
	{
		int	leafnum = leafList[i];
		if( visbits[leafnum>>3] & (1<<(leafnum & 7)))
			return true;
	}
	return false;
}

/*
===============================================================================

POINT TESTING IN HULLS

===============================================================================
*/

/*
==================
CM_HullPointContents

==================
*/
int CM_HullPointContents( chull_t *hull, int num, const vec3_t p )
{
	while( num >= 0 )
		num = hull->clipnodes[num].children[(hull->planes[hull->clipnodes[num].planenum].type < 3 ? p[hull->planes[hull->clipnodes[num].planenum].type] : DotProduct (hull->planes[hull->clipnodes[num].planenum].normal, p)) < hull->planes[hull->clipnodes[num].planenum].dist];
	return num;
}

/*
==================
CM_PointContents

==================
*/
int CM_PointContents( const vec3_t p )
{
	if( !worldmodel ) return CONTENTS_NONE;
	return CM_HullPointContents( &worldmodel->hulls[0], 0, p );
}

/*
==================
CM_AmbientLevels

grab the ambient sound levels for current point
==================
*/
void CM_AmbientLevels( const vec3_t p, byte *pvolumes )
{
	cleaf_t	*leaf;

	if( !worldmodel || !p || !pvolumes )
		return;	

	leaf = worldmodel->leafs + CM_PointLeafnum( p );
	*(int *)pvolumes = *(int *)leaf->ambient_sound_level;
}