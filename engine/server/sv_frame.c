//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        sv_frame.c - server world snapshot
//=======================================================================

#include "common.h"
#include "server.h"
#include "const.h"
#include "protocol.h"
#include "net_encode.h"
#include "entity_types.h"

#define MAX_VISIBLE_PACKET	512

typedef struct
{
	int		num_entities;
	entity_state_t	entities[MAX_VISIBLE_PACKET];	
} sv_ents_t;

static byte *clientpvs;	// FatPVS
static byte *clientphs;	// FatPHS

int	c_fullsend;

/*
=======================
SV_EntityNumbers
=======================
*/
static int SV_EntityNumbers( const void *a, const void *b )
{
	int	ent1, ent2;

	ent1 = ((entity_state_t *)a)->number;
	ent2 = ((entity_state_t *)b)->number;

	if( ent1 == ent2 )
		Host_Error( "SV_SortEntities: duplicated entity\n" );

	if( ent1 < ent2 )
		return -1;
	return 1;
}

/*
=============================================================================

Encode a client frame onto the network channel

=============================================================================
*/
/*
=============
SV_EmitPacketEntities

Writes a delta update of an entity_state_t list to the message->
=============
*/
void SV_EmitPacketEntities( client_frame_t *from, client_frame_t *to, sizebuf_t *msg )
{
	entity_state_t	*oldent, *newent;
	int		oldindex, newindex;
	int		oldnum, newnum;
	int		from_num_entities;

	BF_WriteByte( msg, svc_packetentities );

	if( !from ) from_num_entities = 0;
	else from_num_entities = from->num_entities;

	newent = NULL;
	oldent = NULL;
	newindex = 0;
	oldindex = 0;

	while( newindex < to->num_entities || oldindex < from_num_entities )
	{
		if( newindex >= to->num_entities )
		{
			newnum = MAX_ENTNUMBER;
		}
		else
		{
			newent = &svs.client_entities[(to->first_entity+newindex)%svs.num_client_entities];
			newnum = newent->number;
		}

		if( oldindex >= from_num_entities )
		{
			oldnum = MAX_ENTNUMBER;
		}
		else
		{
			oldent = &svs.client_entities[(from->first_entity+oldindex)%svs.num_client_entities];
			oldnum = oldent->number;
		}

		if( newnum == oldnum )
		{	
			// delta update from old position
			// because the force parm is false, this will not result
			// in any bytes being emited if the entity has not changed at all
			MSG_WriteDeltaEntity( oldent, newent, msg, false, sv.time );
			oldindex++;
			newindex++;
			continue;
		}

		if( newnum < oldnum )
		{	
			// this is a new entity, send it from the baseline
			MSG_WriteDeltaEntity( &svs.baselines[newnum], newent, msg, true, sv.time );
			newindex++;
			continue;
		}

		if( newnum > oldnum )
		{	
			// remove from message
			MSG_WriteDeltaEntity( oldent, NULL, msg, false, sv.time );
			oldindex++;
			continue;
		}
	}
	BF_WriteWord( msg, 0 ); // end of packetentities
}

static void SV_AddEntitiesToPacket( edict_t *pViewEnt, edict_t *pClient, client_frame_t *frame, sv_ents_t *ents )
{
	edict_t		*ent;
	byte		*pset;
	bool		fullvis = false;
	sv_client_t	*cl, *netclient;
	entity_state_t	*state;
	int		e;

	// during an error shutdown message we may need to transmit
	// the shutdown message after the server has shutdown, so
	// specfically check for it
	if( !sv.state ) return;

	if( pClient && !( sv.hostflags & SVF_PORTALPASS ))
	{
		// portals can't change hostflags
		sv.hostflags &= ~SVF_SKIPLOCALHOST;

		cl = SV_ClientFromEdict( pClient, true );
		ASSERT( cl );

		// setup hostflags
		if( com.atoi( Info_ValueForKey( cl->userinfo, "cl_lw" )) == 1 )
			sv.hostflags |= SVF_SKIPLOCALHOST;
	}

	svgame.dllFuncs.pfnSetupVisibility( pViewEnt, pClient, &clientpvs, &clientphs );
	if( !clientpvs ) fullvis = true;

	for( e = 1; e < svgame.numEntities; e++ )
	{
		ent = EDICT_NUM( e );
		if( ent->free ) continue;

		if( ent->serialnumber != e )
		{
			// this should never happens
			MsgDev( D_NOTE, "fixing ent->serialnumber from %i to %i\n", ent->serialnumber, e );
			ent->serialnumber = e;
		}

		// don't double add an entity through portals (already added)
		if( ent->framenum == sv.net_framenum )
			continue;

		if( ent->v.flags & FL_CHECK_PHS )
			pset = clientphs;
		else pset = clientpvs;

		state = &ents->entities[ents->num_entities];
		netclient = SV_ClientFromEdict( ent, true );

		// add entity to the net packet
		if( svgame.dllFuncs.pfnAddToFullPack( state, e, ent, pClient, sv.hostflags, ( netclient != NULL ), pset ))
		{
			// to prevent adds it twice through portals
			ent->framenum = sv.net_framenum;

			if( netclient && netclient->modelindex ) // apply custom model if present
				state->modelindex = netclient->modelindex;

			// if we are full, silently discard entities
			if( ents->num_entities < MAX_VISIBLE_PACKET )
			{
				ents->num_entities++;	// entity accepted
				c_fullsend++;		// debug counter
				
			}
			else MsgDev( D_ERROR, "too many entities in visible packet list\n" );
		}

		if( fullvis ) continue; // portal ents will be added anyway, ignore recursion

		// if its a portal entity, add everything visible from its camera position
		if( !( sv.hostflags & SVF_PORTALPASS ) && ent->v.effects & EF_MERGE_VISIBILITY )
		{
			sv.hostflags |= SVF_PORTALPASS;
			SV_AddEntitiesToPacket( ent, pClient, frame, ents );
			sv.hostflags &= ~SVF_PORTALPASS;
		}
	}
}

static void SV_EmitEvents( sv_client_t *cl, client_frame_t *frame, sizebuf_t *msg )
{
	int		i, ev;
	event_state_t	*es;
	event_info_t	*info;
	int		ev_count = 0;
	int		c;

	es = &cl->events;

	// count events
	for( ev = 0; ev < MAX_EVENT_QUEUE; ev++ )
	{
		info = &es->ei[ev];
		if( info->index == 0 )
			continue;
		ev_count++;
	}

	// nothing to send
	if( !ev_count ) return;

	if( ev_count >= MAX_EVENT_QUEUE )
		ev_count = MAX_EVENT_QUEUE - 1;

	BF_WriteByte( msg, svc_event );	// create message
	BF_WriteByte( msg, ev_count );	// Up to MAX_EVENT_QUEUE events

	for( i = c = 0 ; i < MAX_EVENT_QUEUE; i++ )
	{
		info = &es->ei[i];

		if( info->index == 0 )
			continue;

		// only send if there's room
		if ( c < ev_count )
			SV_PlaybackEvent( msg, info );

		info->index = 0;
		c++;
	}
}

/*
=============
SV_WriteClientData

=============
*/
void SV_WriteClientData( client_frame_t *from, client_frame_t *to, sizebuf_t *msg )
{
	clientdata_t	*cd, *ocd;
	clientdata_t	dummy;

	cd = &to->cd;

	if( !from )
	{
		Mem_Set( &dummy, 0, sizeof( dummy ));
		ocd = &dummy;
	}
	else ocd = &from->cd;

	BF_WriteByte( msg, svc_clientdata );
	MSG_WriteClientData( msg, ocd, cd, sv.time );
}

/*
==================
SV_WriteFrameToClient
==================
*/
void SV_WriteFrameToClient( sv_client_t *cl, sizebuf_t *msg )
{
	client_frame_t	*frame, *oldframe;
	int		lastframe;

	// this is the frame we are creating
	frame = &cl->frames[sv.framenum & SV_UPDATE_MASK];

	if( cl->lastframe <= 0 )
	{	
		// client is asking for a retransmit
		oldframe = NULL;
		lastframe = -1;
	}
	else if( sv.framenum - cl->lastframe >= (SV_UPDATE_BACKUP - 3))
	{
		// client hasn't gotten a good message through in a long time
		oldframe = NULL;
		lastframe = -1;
	}
	else
	{	// we have a valid message to delta from
		oldframe = &cl->frames[cl->lastframe & SV_UPDATE_MASK];
		lastframe = cl->lastframe;

		// the snapshot's entities may still have rolled off the buffer, though
		if( oldframe->first_entity <= svs.next_client_entities - svs.num_client_entities )
		{
			MsgDev( D_WARN, "%s: ^7delta request from out of date entities.\n", cl->name );
			oldframe = NULL;
			lastframe = 0;
		}
	}

	// delta encode the events
	SV_EmitEvents( cl, frame, msg );

	BF_WriteByte( msg, svc_frame );
	BF_WriteLong( msg, sv.framenum );
	BF_WriteLong( msg, sv.time );		// send a servertime each frame
	BF_WriteLong( msg, lastframe );	// what we are delta'ing from
	BF_WriteByte( msg, sv.frametime );
	BF_WriteByte( msg, cl->surpressCount );	// rate dropped packets
	cl->surpressCount = 0;

	// delta encode the clientdata
	SV_WriteClientData( oldframe, frame, msg );

	// delta encode the entities
	SV_EmitPacketEntities( oldframe, frame, msg );
}


/*
=============================================================================

Build a client frame structure

=============================================================================
*/
/*
=============
SV_BuildClientFrame

Decides which entities are going to be visible to the client,
and copies off the playerstate.
=============
*/
void SV_BuildClientFrame( sv_client_t *cl )
{
	edict_t		*clent;
	edict_t		*viewent;	// may be NULL
	client_frame_t	*frame;
	entity_state_t	*state;
	static sv_ents_t	frame_ents;
	int		i;

	clent = cl->edict;
	viewent = cl->pViewEntity;
	sv.net_framenum++;

	if( !sv.paused )
	{
		// update client fixangle
		switch( clent->v.fixangle )
		{
		case 1:
			BF_WriteByte( &sv.multicast, svc_setangle );
			BF_WriteBitAngle( &sv.multicast, clent->v.angles[0], 16 );
			BF_WriteBitAngle( &sv.multicast, clent->v.angles[1], 16 );
			SV_DirectSend( MSG_ONE, vec3_origin, clent );
			clent->v.effects |= EF_NOINTERP;
			break;
		case 2:
			BF_WriteByte( &sv.multicast, svc_addangle );
			BF_WriteBitAngle( &sv.multicast, cl->addangle, 16 );
			SV_DirectSend( MSG_ONE, vec3_origin, clent );
			cl->addangle = 0;
			break;
		}
		clent->v.fixangle = 0; // reset fixangle
	}

	// this is the frame we are creating
	frame = &cl->frames[sv.framenum & SV_UPDATE_MASK];
	frame->senttime = svs.realtime; // save it for ping calc later

	// clear everything in this snapshot
	frame_ents.num_entities = c_fullsend = 0;
	if( !SV_ClientFromEdict( clent, true )) return; // not in game yet

	// update clientdata_t
	svgame.dllFuncs.pfnUpdateClientData( clent, false, &frame->cd );

	// add all the entities directly visible to the eye, which
	// may include portal entities that merge other viewpoints
	sv.hostflags &= ~SVF_PORTALPASS;
	SV_AddEntitiesToPacket( viewent, clent, frame, &frame_ents );

	// if there were portals visible, there may be out of order entities
	// in the list which will need to be resorted for the delta compression
	// to work correctly.  This also catches the error condition
	// of an entity being included twice.
	qsort( frame_ents.entities, frame_ents.num_entities, sizeof( frame_ents.entities[0] ), SV_EntityNumbers );

	// copy the entity states out
	frame->num_entities = 0;
	frame->first_entity = svs.next_client_entities;

	for( i = 0; i < frame_ents.num_entities; i++ )
	{
		// add it to the circular client_entities array
		state = &svs.client_entities[svs.next_client_entities % svs.num_client_entities];
		*state = frame_ents.entities[i];
		svs.next_client_entities++;

		// this should never hit, map should always be restarted first in SV_Frame
		if( svs.next_client_entities >= 0x7FFFFFFE )
			Host_Error( "svs.next_client_entities wrapped\n" );
		frame->num_entities++;
	}
}

/*
===============================================================================

FRAME UPDATES

===============================================================================
*/
/*
=======================
SV_SendClientDatagram
=======================
*/
bool SV_SendClientDatagram( sv_client_t *cl )
{
	byte    	msg_buf[MAX_MSGLEN];
	sizebuf_t	msg;

	SV_BuildClientFrame( cl );

	BF_Init( &msg, "Datagram", msg_buf, sizeof( msg_buf ));

	// send over all the relevant entity_state_t
	// and the player state
	SV_WriteFrameToClient( cl, &msg );

	// copy the accumulated reliable datagram
	// for this client out to the message
	// it is necessary for this to be after the WriteEntities
	// so that entity references will be current
	if( BF_CheckOverflow( &cl->reliable )) MsgDev( D_ERROR, "reliable datagram overflowed for %s\n", cl->name );
	else BF_WriteBits( &msg, BF_GetData( &cl->reliable ), BF_GetNumBitsWritten( &cl->reliable ));
	BF_Clear( &cl->reliable );

	if( BF_CheckOverflow( &msg ))
	{	
		// must have room left for the packet header
		MsgDev( D_WARN, "msg overflowed for %s\n", cl->name );
		BF_Clear( &msg );
	}

	// copy the accumulated multicast datagram
	// for this client out to the message
	// it is necessary for this to be after the WriteEntities
	// so that entity references will be current
	if( BF_CheckOverflow( &cl->datagram )) MsgDev( D_WARN, "datagram overflowed for %s\n", cl->name );
	else BF_WriteBits( &msg, BF_GetData( &cl->datagram ), BF_GetNumBitsWritten( &cl->datagram ));
	BF_Clear( &cl->datagram );

	if( BF_CheckOverflow( &msg ))
	{	
		// must have room left for the packet header
		MsgDev( D_WARN, "msg overflowed for %s\n", cl->name );
		BF_Clear( &msg );
	}

	// send the datagram
	Netchan_TransmitBits( &cl->netchan, BF_GetNumBitsWritten( &msg ), BF_GetData( &msg ));

	// record the size for rate estimation
	cl->message_size[sv.framenum % RATE_MESSAGES] = BF_GetNumBytesWritten( &msg );

	return true;
}

/*
=======================
SV_RateDrop

Returns true if the client is over its current
bandwidth estimation and should not be sent another packet
=======================
*/
bool SV_RateDrop( sv_client_t *cl )
{
	int	i, total = 0;

	// never drop over the loopback
	if( NET_IsLocalAddress( cl->netchan.remote_address ))
		return false;

	for( i = 0; i < RATE_MESSAGES; i++ )
		total += cl->message_size[i];

	if( total > cl->rate )
	{
		cl->surpressCount++;
		cl->message_size[sv.framenum % RATE_MESSAGES] = 0;
		return true;
	}
	return false;
}

/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages( void )
{
	sv_client_t	*cl;
	int		i;

	svs.currentPlayer = NULL;

	if( sv.state == ss_dead )
		return;

	// send a message to each connected client
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( !cl->state ) continue;
			
		if( !cl->edict || (cl->edict->v.flags & ( FL_FAKECLIENT|FL_SPECTATOR )))
			continue;

		svs.currentPlayer = cl;

		// update any userinfo packets that have changed
		if( cl->sendinfo )
		{
			cl->sendinfo = false;
			SV_FullClientUpdate( cl, &sv.multicast );
		}
                    
		if( cl->sendmovevars )
		{
			cl->sendmovevars = false;
			SV_UpdatePhysinfo( cl, &sv.multicast );
                    }

		// if the reliable message overflowed, drop the client
		if( BF_CheckOverflow( &cl->netchan.message ))
		{
			BF_Clear( &cl->netchan.message );
			BF_Clear( &cl->reliable );
			BF_Clear( &cl->datagram );
			SV_BroadcastPrintf( PRINT_HIGH, "%s overflowed\n", cl->name );
			SV_DropClient( cl );
			cl->send_message = true;
		}

		// only send messages if the client has sent one
		if( !cl->send_message ) continue;

		if( cl->state == cs_spawned )
		{
			// don't overrun bandwidth
			if( SV_RateDrop( cl )) continue;
			SV_SendClientDatagram( cl );
		}
		else
		{
			if( BF_GetNumBytesWritten( &cl->netchan.message ) || svs.realtime - cl->netchan.last_sent > 1000 )
				Netchan_Transmit( &cl->netchan, 0, NULL );
		}
		// yes, message really sended 
		cl->send_message = false;
	}

	// reset current client
	svs.currentPlayer = NULL;
}

/*
=======================
SV_SendMessagesToAll

e.g. before changing level
=======================
*/
void SV_SendMessagesToAll( void )
{
	int		i;
	sv_client_t	*cl;

	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( cl->state >= cs_connected )
			cl->send_message = true;
	}	
	SV_SendClientMessages();
}

/*
=======================
SV_InactivateClients

Purpose: Prepare for level transition, etc.
=======================
*/
void SV_InactivateClients( void )
{
	int		i;
	sv_client_t	*cl;

	if( sv.state == ss_dead )
		return;

	// send a message to each connected client
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( !cl->state || !cl->edict ) continue;
			
		if( !cl->edict || (cl->edict->v.flags & ( FL_FAKECLIENT|FL_SPECTATOR )))
			continue;

		if( svs.clients[i].state > cs_connected )
			svs.clients[i].state = cs_connected;

		// clear netchan message (but keep other buffers)
		BF_Clear( &cl->netchan.message );
	}
}