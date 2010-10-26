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
	byte	*scan, *visdata;
	uint	*dest, *src;
	int	hcount, vcount;
	double	timestart;
	int	bitbyte;

	if( !worldmodel || !cm.pvs )
		return;

	MsgDev( D_NOTE, "Building PAS...\n" );
	timestart = Sys_DoubleTime();

	num = worldmodel->numleafs;
	rowwords = (num + 31)>>5;
	rowbytes = rowwords * 4;

	// store off pointer to compressed visdata
	// for right freeing after building pas
	visdata = cm.pvs;

	// allocate pvs and phs data single array
	cm.pvs = Mem_Alloc( worldmodel->mempool, rowbytes * num * 2 );
	cm.phs = cm.pvs + rowbytes * num;

	scan = cm.pvs;
	vcount = 0;

	// uncompress pvs first
	for( i = 0; i < num; i++, scan += rowbytes )
	{
		byte	*visblock;

		visblock = CM_DecompressVis( worldmodel->leafs[i].visdata );
		Mem_Copy( scan, visblock, rowbytes );

		if( i == 0 ) continue;

		for( j = 0; j < num; j++ )
		{
			if( scan[j>>3] & (1<<( j & 7 )))
				vcount++;
		}
	}

	// free compressed pvsdata
	Mem_Free( visdata );

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

		// store new leaf pointers for pvs and pas data
		worldmodel->leafs[i].visdata = scan;
		worldmodel->leafs[i].pasdata = (byte *)dest; 

		if( i == 0 ) continue;

		for( j = 0; j < num; j++ )
		{
			if(((byte *)dest)[j>>3] & (1<<( j & 7 )))
				hcount++;
		}
	}

	MsgDev( D_NOTE, "Average leaves visible / audible / total: %i / %i / %i\n", vcount / num, hcount / num, num );
	MsgDev( D_NOTE, "PAS building time: %g secs\n", Sys_DoubleTime() - timestart );
}

/*
=================
CM_LeafPVS
=================
*/
byte *CM_LeafPVS( int leafnum )
{
	if( !worldmodel || leafnum <= 0 || leafnum >= worldmodel->numleafs || !cm.pvs || cm_novis->integer )
		return cm.nullrow;

	return worldmodel->leafs[leafnum+1].visdata;
}

/*
=================
CM_LeafPHS
=================
*/
byte *CM_LeafPHS( int leafnum )
{
	if( !worldmodel || leafnum <= 0 || leafnum >= worldmodel->numleafs || !cm.phs || cm_novis->integer )
		return cm.nullrow;

	return worldmodel->leafs[leafnum+1].pasdata;
}

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/
static void CM_AddToFatPVS( const vec3_t org, int type, mnode_t *node )
{
	byte	*vis;
	mplane_t	*plane;
	float	d;

	while( 1 )
	{
		// if this is a leaf, accumulate the pvs bits
		if( node->contents < 0 )
		{
			if( node->contents != CONTENTS_SOLID )
			{
				mleaf_t	*leaf;
				int	i;

				leaf = (mleaf_t *)node;			

				if( type == DVIS_PVS )
					vis = leaf->visdata;
				else if( type == DVIS_PHS )
					vis = leaf->pasdata;
				else vis = cm.nullrow;

				for( i = 0; i < fatbytes; i++ )
					bitvector[i] |= vis[i];
			}
			return;
		}
	
		plane = node->plane;
		d = DotProduct( org, plane->normal ) - plane->dist;
		if( d > 8 ) node = node->children[0];
		else if( d < -8 ) node = node->children[1];
		else
		{
			// go down both
			CM_AddToFatPVS( org, type, node->children[0] );
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
byte *CM_FatPVS( const vec3_t org, qboolean portal )
{
	if( !cm.pvs || cm_novis->integer )
		return cm.nullrow;

	bitvector = fatpvs;
	fatbytes = (worldmodel->numleafs+31)>>3;
	if( !portal ) Mem_Set( bitvector, 0, fatbytes );
	CM_AddToFatPVS( org, DVIS_PVS, worldmodel->nodes );

	return bitvector;
}

/*
============
CM_FatPHS

The client will interpolate the hear position,
so we can't use a single PHS point
===========
*/
byte *CM_FatPHS( const vec3_t org, qboolean portal )
{
	if( !cm.pvs || cm_novis->integer )
		return cm.nullrow;

	bitvector = fatphs;
	fatbytes = (worldmodel->numleafs+31)>>3;
	if( !portal ) Mem_Set( bitvector, 0, fatbytes );
	CM_AddToFatPVS( org, DVIS_PHS, worldmodel->nodes );

	return bitvector;
}