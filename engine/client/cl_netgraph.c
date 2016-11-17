/*
cl_netgraph.c - Draw Net statistics (borrowed from Xash3D SDL code)
Copyright (C) 2016 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"
#include "client.h"
#include "gl_local.h"

#define NET_TIMINGS			1024
#define NET_TIMINGS_MASK		(NET_TIMINGS - 1)
#define LATENCY_AVG_FRAC		0.5
#define PACKETLOSS_AVG_FRAC		0.5
#define PACKETCHOKE_AVG_FRAC		0.5
#define NETGRAPH_LERP_HEIGHT		24
#define NUM_LATENCY_SAMPLES		8

convar_t	*net_graph;
convar_t	*net_graphpos;
convar_t	*net_graphwidth;
convar_t	*net_graphheight;
convar_t	*net_scale;
convar_t	*net_graphfillsegments;

static struct packet_latency_t
{
	int	latency;
	int	choked;
} netstat_packet_latency[NET_TIMINGS];

static struct cmdinfo_t
{
	float	cmd_lerp;
	int	size;
	qboolean	sent;
} netstat_cmdinfo[NET_TIMINGS];

static byte netcolors[NETGRAPH_LERP_HEIGHT+5][4] =
{
	{ 255, 0,   0,   255 },
	{ 0,   0,   255, 255 },
	{ 240, 127, 63,  255 },
	{ 255, 255, 0,   255 },
	{ 63,  255, 63,  150 }
	// other will be generated through NetGraph_InitColors()
};

static netbandwidthgraph_t	netstat_graph[NET_TIMINGS];
static float		packet_loss;
static float		packet_choke;

/*
==========
NetGraph_DrawLine

CL_FillRGBA shortcut
==========
*/
static void NetGraph_DrawRect( wrect_t *rect, byte colors[3], byte alpha )
{
	CL_FillRGBA( rect->left, rect->top, rect->right, rect->bottom, colors[0], colors[1], colors[2], alpha );
}

/*
==========
NetGraph_InitColors

init netgraph colors
==========
*/
void NetGraph_InitColors( void )
{
	int i;

	for( i = 0; i < NETGRAPH_LERP_HEIGHT; i++ )
	{
		if( i <= NETGRAPH_LERP_HEIGHT / 3 )
		{
			netcolors[i + 5][0] = i * -7.875f + 63;
			netcolors[i + 5][1] = i *  7.875f;
			netcolors[i + 5][2] = i * 19.375f + 100;
		}
		else
		{
			netcolors[i + 5][0] = ( i - 8 ) * -0.3125f + 255;
			netcolors[i + 5][1] = ( i - 8 ) * -7.9375f + 127;
			netcolors[i + 5][2] = 0;
		}
	}
}

/*
==========
NetGraph_GetFrameData

get frame data info, like chokes, packet losses, also update graph, packet and cmdinfo
==========
*/
void NetGraph_GetFrameData( int *choke_count, int *loss_count, int *biggest_message, float *latency, int *latency_count )
{
	int i;

	*choke_count = *loss_count = *biggest_message = *latency_count = 0;
	*latency = 0.0f;

	for( i = cls.netchan.incoming_sequence - CL_UPDATE_BACKUP + 1; i <= cls.netchan.incoming_sequence; i++ )
	{
		frame_t *f = cl.frames + ( i & CL_UPDATE_MASK );
		struct packet_latency_t *p = netstat_packet_latency + ( i & NET_TIMINGS_MASK );
		netbandwidthgraph_t *g = netstat_graph + ( i & NET_TIMINGS_MASK );

		p->choked = f->receivedtime == -2.0f ? true : false;
		if( p->choked )
			(*choke_count)++;

		if( !f->valid || f->receivedtime == -2.0f )
			p->latency = 9998; // broken delta
		else if( f->receivedtime == -1 )
			p->latency = 9999; // dropped
		else
		{
			int frame_latency = min( 1.0f, f->latency );
			p->latency = (( frame_latency + 0.1 ) / 1.1 ) * ( net_graphheight->value - NETGRAPH_LERP_HEIGHT - 2 );

			if( i > cls.netchan.incoming_sequence - NUM_LATENCY_SAMPLES )
			{
				*latency += 1000.0f * f->latency;
				(*latency_count)++;
			}
		}

		memcpy( g, &f->graphdata, sizeof( netbandwidthgraph_t ));

		if( *biggest_message < g->msgbytes )
			*biggest_message = g->msgbytes;
	}

	for( i = cls.netchan.outgoing_sequence - CL_UPDATE_BACKUP + 1; i <= cls.netchan.outgoing_sequence; i++ )
	{
		netstat_cmdinfo[i & NET_TIMINGS_MASK].cmd_lerp = cl.commands[i & CL_UPDATE_MASK].frame_lerp;
		netstat_cmdinfo[i & NET_TIMINGS_MASK].sent = cl.commands[i & CL_UPDATE_MASK].heldback ? false : true;
		netstat_cmdinfo[i & NET_TIMINGS_MASK].size = cl.commands[i & CL_UPDATE_MASK].sendsize;
	}
}

/*
===========
NetGraph_DrawTimes

===========
*/
void NetGraph_DrawTimes( wrect_t rect, struct cmdinfo_t *cmdinfo, int x, int w )
{
	int	i, j, extrap_point = NETGRAPH_LERP_HEIGHT / 3, a, h;
	POINT	pt = { max( x + w - 1 - 25, 1 ), max( rect.top + rect.bottom - 4 - NETGRAPH_LERP_HEIGHT + 1, 1 ) };
	rgba_t	colors = { 0.9 * 255, 0.9 * 255, 0.7 * 255, 255 };
	wrect_t	fill;

	Con_DrawString( pt.x, pt.y, va( "%i/s", cl_cmdrate->integer ), colors );

	for( a = 0; a < w; a++ )
	{
		i = ( cls.netchan.outgoing_sequence - a ) & NET_TIMINGS_MASK;
		h = ( cmdinfo[i].cmd_lerp / 3.0f ) * NETGRAPH_LERP_HEIGHT;

		fill.left = x + w - a - 1;
		fill.right = fill.bottom = 1;
		fill.top = rect.top + rect.bottom - 4;

		if( h >= extrap_point )
		{
			int	start = 0;

			h -= extrap_point;
			fill.top = extrap_point;

			for( j = start; j < h; j++ )
			{
				NetGraph_DrawRect( &fill, netcolors[j + extrap_point], 255 );
				fill.top--;
			}
		}
		else
		{
			int	oldh = h;

			fill.top -= h;
			h = extrap_point - h;

			for( j = 0; j < h; j++ )
			{
				NetGraph_DrawRect( &fill, netcolors[j + oldh], 255 );
				fill.top--;
			}
		}

		fill.top = rect.top + rect.bottom - 4 - extrap_point;

		CL_FillRGBA( fill.left, fill.top, fill.right, fill.bottom, 255, 255, 255, 255 );

		fill.top = rect.top + rect.bottom - 3;

		if( !cmdinfo[i].sent )
			CL_FillRGBA( fill.left, fill.top, fill.right, fill.bottom, 255, 0, 0, 255 );
	}
}

/*
===========
NetGraph_DrawHatches

===========
*/
void NetGraph_DrawHatches( int x, int y, int maxmsgbytes )
{
	int	ystep = max((int)( 10.0 / net_scale->value ), 1 );
	wrect_t	hatch = { x, 4, y, 1 };
	int	starty;

	for( starty = hatch.top; hatch.top > 0 && ((starty - hatch.top) * net_scale->value < (maxmsgbytes + 50)); hatch.top -= ystep )
	{
		CL_FillRGBA( hatch.left, hatch.top, hatch.right, hatch.bottom, 63, 63, 0, 200 );
	}
}

/*
===========
NetGraph_DrawTextFields

===========
*/
void NetGraph_DrawTextFields( int x, int y, int count, float avg, float *framerate, int packet_loss, int packet_choke )
{
	static int lastout;
	float latency;
	rgba_t colors = { 0.9 * 255, 0.9 * 255, 0.7 * 255, 255 };


	latency = count > 0 ? max( 0,  avg / count - 0.5 * host.frametime - 1000.0 / cl_updaterate->value ) : 0;

	*framerate = 0.5 * host.realframetime + 0.5 * *framerate;

	Con_DrawString( x, y - net_graphheight->integer, va( "%.1ffps" , 0.5 / *framerate ), colors );
	Con_DrawString( x + 75, y - net_graphheight->integer, va( "%i ms" , (int)latency ), colors );
	Con_DrawString( x + 150, y - net_graphheight->integer, va( "%i/s" , cl_updaterate->integer ), colors );

	if( netstat_cmdinfo[( cls.netchan.outgoing_sequence - 1 ) & NET_TIMINGS_MASK].size )
		lastout = netstat_cmdinfo[( cls.netchan.outgoing_sequence - 1 ) & NET_TIMINGS_MASK].size;

	Con_DrawString( x, y - net_graphheight->integer + 15, va( "in :  %i %.2f k/s", netstat_graph[cls.netchan.incoming_sequence & NET_TIMINGS_MASK].msgbytes, cls.netchan.flow[FLOW_INCOMING].avgkbytespersec ), colors );

	Con_DrawString( x, y - net_graphheight->integer + 30, va( "out:  %i %.2f k/s", lastout, cls.netchan.flow[FLOW_OUTGOING].avgkbytespersec ), colors );

	if( net_graph->integer > 2 )
		Con_DrawString( x, y - net_graphheight->integer + 45, va( "loss: %i choke: %i", packet_loss, packet_choke ), colors );

}

/*
===========
NetGraph_DrawDataSegment

===========
*/
int NetGraph_DrawDataSegment( wrect_t *fill, int bytes, byte r, byte g, byte b, byte alpha )
{
	int h = bytes / net_scale->value;
	byte colors[3] = { r, g, b };
	fill->top -= h;
	if( net_graphfillsegments->integer )
		fill->bottom = h;
	else
		fill->bottom = 1;

	if( fill->top > 1 )
	{
		NetGraph_DrawRect( fill, colors, alpha );
		return 1;
	}
	return 0;
}

/*
===========
NetGraph_ColorForHeight

color based on packet latency
===========
*/
void NetGraph_ColorForHeight( struct packet_latency_t *packet, byte color[3], byte *alpha, int *ping )
{
	switch( packet->latency )
	{
	case 9999:
		memcpy( color, netcolors[0], sizeof( byte ) * 3 ); // dropped
		*ping = 0;
		*alpha = netcolors[0][3];
		break;
	case 9998:
		memcpy( color, netcolors[1], sizeof( byte ) * 3 ); // invalid
		*alpha = netcolors[1][3];
		*ping = 0;
		break;
	case 9997:
		memcpy( color, netcolors[2], sizeof( byte ) * 3 ); // skipped
		*alpha = netcolors[2][3];
		*ping = 0;
		break;
	default:
		*ping = 1;
		if( packet->choked )
		{
			memcpy( color, netcolors[3], sizeof( byte ) * 3 );
			*alpha = netcolors[3][3];
		}
		else
		{
			memcpy( color, netcolors[4], sizeof( byte ) * 3 );
			*alpha = netcolors[4][3];
		}
	}
}

/*
===========
NetGraph_DrawDataUsage

===========
*/
void NetGraph_DrawDataUsage( int x, int y, int w, int maxmsgbytes )
{
	int	a, i, h, lastvalidh = 0, ping;
	byte	color[3], alpha;
	wrect_t	fill = { 0 };

	for( a = 0; a < w; a++ )
	{
		i = (cls.netchan.incoming_sequence - a) & NET_TIMINGS_MASK;
		h = netstat_packet_latency[i].latency;

		NetGraph_ColorForHeight( &netstat_packet_latency[i], color, &alpha, &ping );

		if( !ping ) h = lastvalidh;
		else lastvalidh = h;

		if( h > net_graphheight->value - NETGRAPH_LERP_HEIGHT - 2 )
			h = net_graphheight->value - NETGRAPH_LERP_HEIGHT - 2;

		fill.left = x + w - a - 1;
		fill.top = y - h;
		fill.right = 1;
		fill.bottom = ping ? 1: h;

		NetGraph_DrawRect( &fill, color, alpha );

		fill.top = y;
		fill.bottom = 1;

		color[0] = 0; color[1] = 255; color[2] = 0;

		NetGraph_DrawRect( &fill, color, 160 );

		if( net_graph->integer < 2 )
			continue;

		fill.top = y - net_graphheight->value - 1;
		fill.bottom = 1;
		color[0] = color[1] = color[2] = 255;

		NetGraph_DrawRect( &fill, color, 255 );

		fill.top -= 1;

		if( netstat_packet_latency[i].latency > 9995 )
			continue; // skip invalid

		if( !NetGraph_DrawDataSegment( &fill, netstat_graph[i].client, 255, 0, 0, 128 ))
			continue;

		if( !NetGraph_DrawDataSegment( &fill, netstat_graph[i].players, 255, 255, 0, 128 ))
			continue;

		if( !NetGraph_DrawDataSegment( &fill, netstat_graph[i].entities, 255, 0, 255, 128 ))
			continue;

		if( !NetGraph_DrawDataSegment( &fill, netstat_graph[i].tentities, 0, 0, 255, 128 ))
			continue;

		if( !NetGraph_DrawDataSegment( &fill, netstat_graph[i].sound, 0, 255, 0, 128 ))
			continue;

		if( !NetGraph_DrawDataSegment( &fill, netstat_graph[i].event, 0, 255, 255, 128 ))
			continue;

		if( !NetGraph_DrawDataSegment( &fill, netstat_graph[i].usr, 200, 200, 200, 128 ))
			continue;

		// special case for maximum usage
		h = netstat_graph[i].msgbytes / net_scale->value;

		color[0] = color[1] = color[2] = 240;

		if( net_graphfillsegments->integer )
			fill.bottom = h;
		else fill.bottom = 1;

		fill.top = y - net_graphheight->value - 1 - h;

		if( fill.top < 2 ) continue;

		NetGraph_DrawRect( &fill, color, 255 );
	}

	if( net_graph->integer >= 2 )
		NetGraph_DrawHatches( x, y - net_graphheight->value - 1, maxmsgbytes );
}

/*
===========
SCR_DrawNetGraph

===========
*/
void SCR_DrawNetGraph( void )
{
	wrect_t	rect = { 0, clgame.scrInfo.iWidth, 0, clgame.scrInfo.iHeight };
	float	loss = 0, choke = 0, avg_ping = 0;
	int	loss_count = 0, choke_count = 0, ping_count = 0, maxmsgbytes = 0;
	int	w = min( NET_TIMINGS, net_graphwidth->integer ), x, y;
	float	framerate = 0;

	if( !net_graph->integer )
		return;

	if( net_scale->value <= 0 )
		Cvar_SetFloat( "net_scale", 0.1f );

	if( rect.right < w + 10 )
		w = rect.right - 10;

	NetGraph_GetFrameData( &choke_count, &loss_count, &maxmsgbytes, &avg_ping, &ping_count );

	// packet loss
	loss = 100.0 * (float)loss_count / CL_UPDATE_BACKUP;
	packet_loss = PACKETLOSS_AVG_FRAC * packet_loss + ( 1.0 - PACKETLOSS_AVG_FRAC ) * loss;

	// packet choke
	choke = 100.0 * (float)choke_count / CL_UPDATE_BACKUP;
	packet_choke = PACKETCHOKE_AVG_FRAC * packet_choke + ( 1.0 - PACKETCHOKE_AVG_FRAC ) * choke;

	// detect x and y position
	switch( net_graphpos->integer )
	{
	case 1: // right sided
		x = rect.left + rect.right - 5 - w;
		break;
	case 2: // center
		x = rect.left + ( rect.right - 10 - w ) / 2;
		break;
	default: // left sided
		x = rect.left + 5;
		break;
	}
	y = rect.bottom + rect.top - NETGRAPH_LERP_HEIGHT - 5;

	if( net_graph->integer < 3 )
	{
		NetGraph_DrawTimes( rect, netstat_cmdinfo, x, w );
		NetGraph_DrawDataUsage( x, y, w, maxmsgbytes );
	}

	NetGraph_DrawTextFields( x, y, ping_count, avg_ping, &framerate, packet_loss, packet_choke );
}

void CL_InitNetgraph( void )
{
	net_graph = Cvar_Get( "net_graph", "0", CVAR_ARCHIVE, "draw network usage graph" );
	net_graphpos = Cvar_Get( "net_graphpos", "1", CVAR_ARCHIVE, "network usage graph position" );
	net_scale = Cvar_Get( "net_scale", "5", CVAR_ARCHIVE, "network usage graph scale level" );
	net_graphwidth = Cvar_Get( "net_graphwidth", "192", CVAR_ARCHIVE, "network usage graph width" );
	net_graphheight = Cvar_Get( "net_graphheight", "64", CVAR_ARCHIVE, "network usage graph height" );
	net_graphfillsegments = Cvar_Get( "net_graphfillsegments", "1", CVAR_ARCHIVE, "fill segments in network usage graph" );
	packet_loss = packet_choke = 0.0;

	NetGraph_InitColors();
}