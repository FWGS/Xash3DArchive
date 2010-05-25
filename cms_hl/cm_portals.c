//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_portals.c - server visibility
//=======================================================================

#include "cm_local.h"
#include "mathlib.h"

static byte fatpvs[MAX_MAP_LEAFS/8];
static byte fatphs[MAX_MAP_LEAFS/8];
static byte *bitvector;
static int fatbytes;

/*
===================
CM_DecompressVis
===================
*/
static byte *CM_DecompressVis( const byte *in )
{
	static byte	decompressed[MAX_MAP_LEAFS/8];
	int		c, row;
	byte		*out;

	if( !worldmodel )
	{
		Host_Error( "CM_DecompressVis: no worldmodel\n" );
		return NULL;
	}

	row = (worldmodel->numleafs + 7)>>3;	
	out = decompressed;

	if( !in )
	{	
		// no vis info, so make all visible
		while( row )
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;		
	}

	do
	{
		if( *in )
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		in += 2;

		while( c )
		{
			*out++ = 0;
			c--;
		}
	} while( out - decompressed < row );

	return decompressed;
}

/*
===============================================================================

PVS / PHS

===============================================================================
*/
/*
=================
CM_CalcPHS
=================
*/
void CM_CalcPHS( void )
{
	int	i, j, k, l, index, num;
	int	rowbytes, rowwords;
	int	bitbyte;
	uint	*dest, *src;
	byte	*scan;
	int	hcount, vcount;
	uint	timestart;

	if( !worldmodel ) return;

	if( !worldmodel->visdata )
	{
		cm.pvs = cm.phs = NULL;
		return;
	}

	MsgDev( D_NOTE, "Building PHS...\n" );
	timestart = Sys_Milliseconds();

	num = worldmodel->numleafs;
	rowwords = (num + 31)>>5;
	rowbytes = rowwords * 4;

	cm.pvs = Mem_Alloc( worldmodel->mempool, rowbytes * num );
	scan = cm.pvs;
	vcount = 0;

	// uncompress pvs first
	for( i = 0; i < num; i++, scan += rowbytes )
	{
		byte	*visblock = worldmodel->leafs[i].compressed_vis;

		Mem_Copy( scan, CM_DecompressVis( visblock ), rowbytes );

		if( i == 0 ) continue;

		for( j = 0; j < num; j++ )
		{
			if( scan[j>>3] & (1<<( j & 7 )))
			{
				vcount++;
			}
		}
	}

	MsgDev( D_INFO, "Average clusters visible: %i (build at %g secs)\n", vcount / num, (Sys_Milliseconds() - timestart) * 0.001f );
	timestart = Sys_Milliseconds();

	cm.phs = Mem_Alloc( worldmodel->mempool, rowbytes * num );
	scan = cm.pvs;
	hcount = 0;

	dest = (uint *)cm.phs;

	for( i = 0; i < num; i++, dest += rowwords, scan += rowbytes )
	{
		Mem_Copy( dest, scan, rowbytes );

		for( j = 0; j < rowbytes; j++ )
		{
			bitbyte = scan[j];
			if( !bitbyte ) continue;

			for( k = 0; k < 8; k++ )
			{
				if(!( bitbyte & ( 1<<k )))
					continue;
				// or this pvs row into the phs
				// +1 because pvs is 1 based
				index = ((j<<3) + k + 1);
				if( index >= num ) continue;

				src = (uint *)cm.pvs + index * rowwords;
				for( l = 0; l < rowwords; l++ )
					dest[l] |= src[l];
			}
		}

		if( i == 0 ) continue;

		for( j = 0; j < num; j++ )
		{
			if(((byte *)dest)[j>>3] & (1<<( j & 7 )))
			{
				hcount++;
			}
		}
	}

	MsgDev( D_INFO, "Average clusters hearable: %i (build at %g secs)\n", hcount / num, (Sys_Milliseconds() - timestart) * 0.001f );
	MsgDev( D_INFO, "Total clusters: %i\n", num );
}

/*
=================
CM_LeafPVS
=================
*/
byte *CM_LeafPVS( int leafnum )
{
	if( !worldmodel || leafnum < 0 || leafnum >= worldmodel->numleafs || !cm.pvs )
		return cm.nullrow;
	return cm.pvs + leafnum * sizeof( int ) * ((worldmodel->numleafs + 31)>>5);
}

/*
=================
CM_LeafPHS
=================
*/
byte *CM_LeafPHS( int leafnum )
{
	if( !worldmodel || leafnum < 0 || leafnum >= worldmodel->numleafs || !cm.phs )
		return cm.nullrow;
	return cm.phs + leafnum * sizeof( int ) * ((worldmodel->numleafs + 31)>>5);
}

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/
static void CM_AddToFatPVS( const vec3_t org, const float radius, cnode_t *node )
{
	byte	*pvs;
	cplane_t	*plane;
	float	d;

	while( 1 )
	{
		// if this is a leaf, accumulate the pvs bits
		if( node->contents < 0 )
		{
			if( node->contents != CONTENTS_SOLID )
			{
				cleaf_t	*leaf;
				int	i;

				leaf = (cleaf_t *)node;			
				pvs = CM_DecompressVis( leaf->compressed_vis );

				for( i = 0; i < fatbytes; i++ )
					bitvector[i] |= pvs[i];
			}
			return;
		}
	
		plane = node->plane;
		d = DotProduct( org, plane->normal ) - plane->dist;
		if( d > radius ) node = node->children[0];
		else if( d < -radius ) node = node->children[1];
		else
		{
			// go down both
			CM_AddToFatPVS( org, radius, node->children[0] );
			node = node->children[1];
		}
	}
}

/*
============
CM_FatPVS

The client will interpolate the view position,
so we can't use a single PVS point
===========
*/
byte *CM_FatPVS( const vec3_t org, bool portal )
{
	bitvector = fatpvs;
	fatbytes = (worldmodel->numleafs+31)>>3;
	if( !portal ) Mem_Set( bitvector, 0, fatbytes );
	CM_AddToFatPVS( org, 8, worldmodel->nodes );

	return bitvector;
}

/*
============
CM_FatPHS

The client will interpolate the hear position,
so we can't use a single PHS point
===========
*/
byte *CM_FatPHS( const vec3_t org, bool portal )
{
#if 1	// test
	bitvector = fatphs;
	fatbytes = (worldmodel->numleafs+31)>>3;
	if( !portal ) Mem_Set( bitvector, 0, fatbytes );
	CM_AddToFatPVS( org, 32, worldmodel->nodes );

	return bitvector;
#else
	int	longs, leafnum;

	longs = (worldmodel->numleafs + 31)>>5;
	leafnum = CM_PointLeafnum( org );

	if( !portal )
	{
		Mem_Copy( fatphs, CM_LeafPHS( leafnum ), longs << 2 );
	}
	else
	{
		byte	*src;
		int	i;

		// or in all the other leaf bits
		src = CM_LeafPHS( leafnum );

		for( i = 0; i < longs; i++ )
			((int *)fatphs)[i] |= ((int *)src)[i];
	}

	return fatphs;
#endif
}