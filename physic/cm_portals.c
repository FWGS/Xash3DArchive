//=======================================================================
//			Copyright XashXT Group 2007 �
//			cm_portals.c - server visibility
//=======================================================================

#include "cm_local.h"
#include "basefiles.h"

/*
===============================================================================

PVS / PHS

===============================================================================
*/
/*
===================
CM_DecompressVis
===================
*/
void CM_DecompressVis( byte *in, byte *out )
{
	byte	*out_p;
	int	c, row;

	row = (cm.numclusters + 7)>>3;	
	out_p = out;

	if( !in )
	{	
		// no vis info, so make all visible
		while (row)
		{
			*out_p++ = 0xff;
			row--;
		}
		return;		
	}
	do
	{
		if(*in)
		{
			*out_p++ = *in++;
			continue;
		}
	
		c = in[1];
		in += 2;
		if((out_p - out) + c > row)
		{
			c = row - (out_p - out);
			MsgDev(D_WARN, "CM_DecompressVis: decompression overrun\n");
		}
		while( c )
		{
			*out_p++ = 0;
			c--;
		}
	} while( out_p - out < row );
}

byte *CM_ClusterPVS( int cluster )
{
	if( cluster == -1 || !cm.vis ) memset( cm.pvsrow, 0, (cm.numclusters + 7)>>3 );
	else CM_DecompressVis( cm.visibility + cm.vis->bitofs[cluster][DVIS_PVS], cm.pvsrow );
	return cm.pvsrow;
}

byte *CM_ClusterPHS (int cluster)
{
	if( cluster == -1 || !cm.vis ) memset( cm.phsrow, 0, (cm.numclusters + 7)>>3 );
	else CM_DecompressVis( cm.visibility + cm.vis->bitofs[cluster][DVIS_PHS], cm.phsrow );
	return cm.phsrow;
}

/*
===============================================================================

AREAPORTALS

===============================================================================
*/
void FloodArea_r( carea_t *area, int floodnum )
{
	int		i;
	dareaportal_t	*p;

	if( area->floodvalid == cm.floodvalid )
	{
		if (area->floodnum == floodnum) return;
		Host_Error("FloodArea_r: reflooded\n");
	}

	area->floodnum = floodnum;
	area->floodvalid = cm.floodvalid;
	p = &cm.areaportals[area->firstareaportal];

	for (i = 0; i < area->numareaportals; i++, p++)
	{
		if (cm.portalopen[p->portalnum])
			FloodArea_r(&cm.areas[p->otherarea], floodnum);
	}
}

/*
====================
CM_FloodAreaConnections
====================
*/
void CM_FloodAreaConnections( void )
{
	carea_t		*area;
	int		i, floodnum = 0;

	// all current floods are now invalid
	cm.floodvalid++;

	// area 0 is not used
	for( i = 1; i < cm.numareas; i++ )
	{
		area = &cm.areas[i];
		if (area->floodvalid == cm.floodvalid)
			continue;	// already flooded into
		floodnum++;
		FloodArea_r( area, floodnum );
	}
}

void CM_SetAreaPortals ( byte *portals, size_t size )
{
	if(size == sizeof( cm.portalopen ))
	{ 
		Mem_Copy( cm.portalopen, portals, size );
		CM_FloodAreaConnections();
		return;
	}
	MsgDev( D_ERROR, "CM_SetAreaPortals: portals mismatch size (%i should be %i)\n", size, sizeof(cm.portalopen));
}

void CM_GetAreaPortals ( byte **portals, size_t *size )
{
	byte *prt = *portals;

	if( prt ) Mem_Copy( prt, cm.portalopen, sizeof(cm.portalopen ));
	if( size) *size = sizeof( cm.portalopen ); 
}

void CM_SetAreaPortalState( int portalnum, bool open )
{
	if( portalnum > cm.numareaportals )
		Host_Error("CM_SetAreaPortalState: areaportal > numareaportals\n");

	cm.portalopen[portalnum] = open;
	CM_FloodAreaConnections();
}

bool CM_AreasConnected( int area1, int area2 )
{
	if( cm_noareas->integer ) return true;
	if(area1 > cm.numareas || area2 > cm.numareas)
		Host_Error("CM_AreasConnected: area > numareas\n");

	if(cm.areas[area1].floodnum == cm.areas[area2].floodnum)
		return true;
	return false;
}

/*
=================
CM_WriteAreaBits

Writes a length byte followed by a bit vector of all the areas
that area in the same flood as the area parameter
This is used by the client refreshes to cull visibility
=================
*/
int CM_WriteAreaBits( byte *buffer, int area )
{
	int	floodnum;
	int	i, bytes;

	bytes = (cm.numareas + 7)>>3;

	if( cm_noareas->integer )
	{
		// for debugging, send everything
		memset( buffer, 255, bytes );
	}
	else
	{
		memset( buffer, 0, bytes );
		floodnum = cm.areas[area].floodnum;
		for( i = 0; i < cm.numareas; i++ )
		{
			if( cm.areas[i].floodnum == floodnum || !area )
				buffer[i>>3] |= 1<<(i&7);
		}
	}
	return bytes;
}