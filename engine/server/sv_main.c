//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_main.c - server main loop
//=======================================================================

#include "common.h"
#include "server.h"
#include "net_encode.h"

#define HEARTBEAT_SECONDS	300.0 		// 300 seconds

netadr_t	master_adr[MAX_MASTERS];		// address of group servers

cvar_t	*sv_zmax;
cvar_t	*sv_unlag;
cvar_t	*sv_maxunlag;
cvar_t	*sv_unlagpush;
cvar_t	*sv_unlagsamples;
cvar_t	*sv_pausable;
cvar_t	*sv_newunit;
cvar_t	*sv_wateramp;
cvar_t	*timeout;				// seconds without any message
cvar_t	*zombietime;			// seconds to sink messages after disconnect
cvar_t	*rcon_password;			// password for remote server commands
cvar_t	*allow_download;
cvar_t	*sv_airaccelerate;
cvar_t	*sv_wateraccelerate;
cvar_t	*sv_maxvelocity;
cvar_t	*sv_gravity;
cvar_t	*sv_stepheight;
cvar_t	*sv_rollangle;
cvar_t	*sv_rollspeed;
cvar_t	*sv_wallbounce;
cvar_t	*sv_maxspeed;
cvar_t	*sv_spectatormaxspeed;
cvar_t	*sv_accelerate;
cvar_t	*sv_friction;
cvar_t	*sv_edgefriction;
cvar_t	*sv_waterfriction;
cvar_t	*sv_synchthink;
cvar_t	*sv_stopspeed;
cvar_t	*hostname;
cvar_t	*sv_maxclients;
cvar_t	*sv_check_errors;
cvar_t	*sv_footsteps;
cvar_t	*public_server;		// should heartbeats be sent
cvar_t	*sv_reconnect_limit;	// minimum seconds between connect messages
cvar_t	*sv_failuretime;
cvar_t	*serverinfo;
cvar_t	*physinfo;
cvar_t	*clockwindow;

// sky variables
cvar_t	*sv_skycolor_r;
cvar_t	*sv_skycolor_g;
cvar_t	*sv_skycolor_b;
cvar_t	*sv_skyvec_x;
cvar_t	*sv_skyvec_y;
cvar_t	*sv_skyvec_z;
cvar_t	*sv_skyname;

void Master_Shutdown( void );

//============================================================================

/*
===================
SV_CalcPings

Updates the cl->ping variables
===================
*/
void SV_CalcPings( void )
{
	int		i, j;
	sv_client_t	*cl;
	int		total, count;

	// clamp fps counter
	for( i = 0; i < sv_maxclients->integer; i++ )
	{
		cl = &svs.clients[i];
		if( cl->state != cs_spawned ) continue;

		total = count = 0;

		for( j = 0; j < (SV_UPDATE_BACKUP / 2); j++ )
		{
			client_frame_t	*frame;

			frame = &cl->frames[(cl->netchan.incoming_acknowledged - 1 - j) & SV_UPDATE_MASK];
			if( frame->latency > 0 )
			{
				count++;
				total += frame->latency;
			}
		}

		if( !count ) cl->ping = 0;
		else cl->ping = total / count;
	}
}

/*
===================
SV_CalcPacketLoss

determine % of packets that were not ack'd.
===================
*/
int SV_CalcPacketLoss( sv_client_t *cl )
{
	int	i, lost, count;
	float	losspercent;
	register client_frame_t *frame;
	int	numsamples;

	lost  = 0;
	count = 0;

	if( cl->fakeclient )
		return 0;

	numsamples = SV_UPDATE_BACKUP / 2;

	for( i = 0; i < numsamples; i++ )
	{
		frame = &cl->frames[(cl->netchan.incoming_acknowledged - 1 - i) & SV_UPDATE_MASK];
		count++;
		if( frame->latency == -1 )
			lost++;
	}

	if( !count ) return 100;
	losspercent = 100.0 * ( float )lost / ( float )count;

	return (int)losspercent;
}

/*
================
SV_HasActivePlayers

returns true if server have spawned players
================
*/
bool SV_HasActivePlayers( void )
{
	int	i;

	// server inactive
	if( !svs.clients ) return false;

	for( i = 0; i < sv_maxclients->integer; i++ )
	{
		if( svs.clients[i].state == cs_spawned )
			return true;
	}
	return false;
}

/*
===================
SV_UpdateMovevars

check movevars for changes every frame
send updates to client if changed
===================
*/
void SV_UpdateMovevars( void )
{
	if( !physinfo->modified ) return;

	svgame.movevars.gravity = sv_gravity->value;
	svgame.movevars.stopspeed = sv_stopspeed->value;
	svgame.movevars.maxspeed = sv_maxspeed->value;
	svgame.movevars.spectatormaxspeed = sv_spectatormaxspeed->value;
	svgame.movevars.accelerate = sv_accelerate->value;
	svgame.movevars.airaccelerate = sv_airaccelerate->value;
	svgame.movevars.wateraccelerate = sv_wateraccelerate->value;
	svgame.movevars.friction = sv_friction->value;
	svgame.movevars.edgefriction = sv_edgefriction->value;
	svgame.movevars.waterfriction = sv_waterfriction->value;
	svgame.movevars.bounce = sv_wallbounce->value;
	svgame.movevars.stepsize = sv_stepheight->value;
	svgame.movevars.maxvelocity = sv_maxvelocity->value;
	svgame.movevars.zmax = sv_zmax->value;
	svgame.movevars.waveHeight = sv_wateramp->value;
	com.strncpy( svgame.movevars.skyName, sv_skyname->string, sizeof( svgame.movevars.skyName ));
	svgame.movevars.footsteps = sv_footsteps->integer;
	svgame.movevars.rollangle = sv_rollangle->value;
	svgame.movevars.rollspeed = sv_rollspeed->value;
	svgame.movevars.skycolor_r = sv_skycolor_r->value;
	svgame.movevars.skycolor_g = sv_skycolor_g->value;
	svgame.movevars.skycolor_b = sv_skycolor_b->value;
	svgame.movevars.skyvec_x = sv_skyvec_x->value;
	svgame.movevars.skyvec_y = sv_skyvec_y->value;
	svgame.movevars.skyvec_z = sv_skyvec_z->value;

	if( MSG_WriteDeltaMovevars( &sv.reliable_datagram, &svgame.oldmovevars, &svgame.movevars ))
		Mem_Copy( &svgame.oldmovevars, &svgame.movevars, sizeof( movevars_t )); // oldstate changed

	physinfo->modified = false;
}

void pfnUpdateServerInfo( const char *szKey, const char *szValue, const char *unused, void *unused2 )
{
	cvar_t	*cv = Cvar_FindVar( szKey );

	if( !cv || !cv->modified ) return; // this cvar not changed

	BF_WriteByte( &sv.reliable_datagram, svc_serverinfo );
	BF_WriteString( &sv.reliable_datagram, szKey );
	BF_WriteString( &sv.reliable_datagram, szValue );
	cv->modified = false; // reset state
}

void SV_UpdateServerInfo( void )
{
	if( !serverinfo->modified ) return;

	Cvar_LookupVars( CVAR_SERVERINFO, NULL, NULL, pfnUpdateServerInfo ); 

	serverinfo->modified = false;
}

/*
=================
SV_CheckCmdTimes
=================
*/
void SV_CheckCmdTimes( void )
{
	sv_client_t	*cl;
	static double	lastreset = 0;
	double		timewindow;
	int		i;

	if( 1.0 > host.realtime - lastreset )
		return;

	lastreset = host.realtime;

	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( cl->state != cs_spawned )
			continue;

		if( cl->last_cmdtime == 0.0 )
		{
			cl->last_cmdtime = host.realtime;
		}

		timewindow = cl->last_movetime + cl->last_cmdtime - host.realtime;

		if( timewindow > clockwindow->value )
		{
			cl->next_movetime = clockwindow->value + host.realtime;
			cl->last_movetime = host.realtime - cl->last_cmdtime;
		}
		else if( timewindow < -clockwindow->value )
		{
			cl->last_movetime = host.realtime - cl->last_cmdtime;
		}
	}
}

/*
=================
SV_ReadPackets
=================
*/
void SV_ReadPackets( void )
{
	sv_client_t	*cl;
	int		i, qport, curSize;

	while( NET_GetPacket( NS_SERVER, &net_from, net_message_buffer, &curSize ))
	{
		BF_Init( &net_message, "ClientPacket", net_message_buffer, curSize );

		// check for connectionless packet (0xffffffff) first
		if( BF_GetMaxBytes( &net_message ) >= 4 && *(int *)net_message.pData == -1 )
		{
			SV_ConnectionlessPacket( net_from, &net_message );
			continue;
		}

		// read the qport out of the message so we can fix up
		// stupid address translating routers
		BF_Clear( &net_message );
		BF_ReadLong( &net_message );	// sequence number
		BF_ReadLong( &net_message );	// sequence number
		qport = (int)BF_ReadShort( &net_message ) & 0xffff;

		// check for packets from connected clients
		for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
		{
			if( cl->state == cs_free ) continue;
			if( cl->fakeclient ) continue;
			if( !NET_CompareBaseAdr( net_from, cl->netchan.remote_address )) continue;
			if( cl->netchan.qport != qport ) continue;
			if( cl->netchan.remote_address.port != net_from.port )
			{
				MsgDev( D_INFO, "SV_ReadPackets: fixing up a translated port\n");
				cl->netchan.remote_address.port = net_from.port;
			}

			if( Netchan_Process( &cl->netchan, &net_message ))
			{	
				cl->send_message = true; // reply at end of frame

				// this is a valid, sequenced packet, so process it
				if( cl->state != cs_zombie )
				{
					cl->lastmessage = host.realtime; // don't timeout
					SV_ExecuteClientMessage( cl, &net_message );
				}
			}

			// Fragmentation/reassembly sending takes priority over all game messages, want this in the future?
			if( Netchan_IncomingReady( &cl->netchan ))
			{
				if( Netchan_CopyNormalFragments( &cl->netchan, &net_message ))
				{
					BF_Clear( &net_message );
					SV_ExecuteClientMessage( cl, &net_message );
				}

				if( Netchan_CopyFileFragments( &cl->netchan, &net_message ))
				{
//					SV_ProcessFile( cl, cl->netchan.incomingfilename );
				}
			}

			break;
		}

		if( i != sv_maxclients->integer )
			continue;
	}
}

/*
==================
SV_CheckTimeouts

If a packet has not been received from a client for timeout->value
seconds, drop the conneciton.  Server frames are used instead of
realtime to avoid dropping the local client while debugging.

When a client is normally dropped, the sv_client_t goes into a zombie state
for a few seconds to make sure any final reliable message gets resent
if necessary
==================
*/
void SV_CheckTimeouts( void )
{
	sv_client_t	*cl;
	float		droppoint;
	float		zombiepoint;
	int		i, numclients = 0;

	droppoint = host.realtime - timeout->value;
	zombiepoint = host.realtime - zombietime->value;

	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( cl->state >= cs_connected )
		{
			if( cl->edict && !( cl->edict->v.flags & (FL_SPECTATOR|FL_FAKECLIENT)))
				numclients++;
                    }

		// fake clients do not timeout
		if( cl->fakeclient ) cl->lastmessage = host.realtime;

		// message times may be wrong across a changelevel
		if( cl->lastmessage > host.realtime )
			cl->lastmessage = host.realtime;

		if( cl->state == cs_zombie && cl->lastmessage < zombiepoint )
		{
			cl->state = cs_free; // can now be reused
			continue;
		}
		if(( cl->state == cs_connected || cl->state == cs_spawned ) && cl->lastmessage < droppoint )
		{
			SV_BroadcastPrintf( PRINT_HIGH, "%s timed out\n", cl->name );
			SV_DropClient( cl ); 
			cl->state = cs_free; // don't bother with zombie state
		}
	}

	if( sv.paused && !numclients )
	{
		// nobody left, unpause the server
		SV_TogglePause( "Pause released since no players are left.\n" );
	}
}

/*
================
SV_PrepWorldFrame

This has to be done before the world logic, because
player processing happens outside RunWorldFrame
================
*/
void SV_PrepWorldFrame( void )
{
	edict_t	*ent;
	int	i;

	for( i = 1; i < svgame.numEntities; i++ )
	{
		ent = EDICT_NUM( i );
		if( ent->free ) continue;

		ent->v.effects &= ~EF_MUZZLEFLASH;

		// clear NOINTERP flag automatically only for alive creatures			
		if( ent->v.flags & ( FL_MONSTER|FL_CLIENT|FL_FAKECLIENT ) && ent->v.deadflag < DEAD_DEAD )
			ent->v.effects &= ~EF_NOINTERP;
	}
}

/*
=================
SV_RunGameFrame
=================
*/
void SV_RunGameFrame( void )
{
	if( !SV_HasActivePlayers()) return;
	if( sv.frametime ) SV_Physics();
}

/*
==================
Host_ServerFrame

==================
*/
void Host_ServerFrame( void )
{
	// if server is not active, do nothing
	if( !svs.initialized ) return;

	// advances servertime
	if( !sv.paused && CL_IsInGame( ) && !sv.loadgame )
	{
		if(!( sv.hostflags & SVF_PLAYERSONLY ))
			sv.time += host.frametime;
		sv.frametime = host.frametime;
	}
	else sv.frametime = 0.0f;

	// check timeouts
	SV_CheckTimeouts ();

	// check clients timewindow
	SV_CheckCmdTimes ();

	// read packets from clients
	SV_ReadPackets ();

	// update ping based on the last known frame from all clients
	SV_CalcPings ();

	// refresh serverinfo on the client side
	SV_UpdateServerInfo ();

	// refresh physic movevars on the client side
	SV_UpdateMovevars ();

	// let everything in the world think and move
	SV_RunGameFrame ();

	// send messages back to the clients that had packets read this frame
	SV_SendClientMessages ();

	// clear edict flags for next frame
	SV_PrepWorldFrame ();

	// send a heartbeat to the master if needed
	Master_Heartbeat ();
}

//============================================================================

/*
================
Master_Heartbeat

Send a message to the master every few minutes to
let it know we are alive, and log information
================
*/
void Master_Heartbeat( void )
{
	char	*string;
	int	i;

	if( host.type != HOST_DEDICATED || !public_server->integer )
		return;	// only dedicated servers send heartbeats

	// check for time wraparound
	if( svs.last_heartbeat > host.realtime )
		svs.last_heartbeat = host.realtime;

	if(( host.realtime - svs.last_heartbeat ) < HEARTBEAT_SECONDS )
		return; // not time to send yet

	svs.last_heartbeat = host.realtime;

	// send the same string that we would give for a status OOB command
	string = SV_StatusString( );

	// send to group master
	for( i = 0; i < MAX_MASTERS; i++ )
	{
		if( master_adr[i].port )
		{
			MsgDev( D_INFO, "Sending heartbeat to %s\n", NET_AdrToString( master_adr[i] ));
			Netchan_OutOfBandPrint( NS_SERVER, master_adr[i], "heartbeat\n%s", string );
		}
	}
}

/*
=================
Master_Shutdown

Informs all masters that this server is going down
=================
*/
void Master_Shutdown( void )
{
	int	i;

	if( host.type != HOST_DEDICATED || !public_server->integer )
		return; // only dedicated servers send heartbeats

	// send to group master
	for( i = 0; i < MAX_MASTERS; i++ )
	{
		if( master_adr[i].port )
		{
			if( i ) MsgDev( D_INFO, "Sending heartbeat to %s\n", NET_AdrToString( master_adr[i] ));
			Netchan_OutOfBandPrint( NS_SERVER, master_adr[i], "shutdown" );
		}
	}
}

//============================================================================

/*
===============
SV_Init

Only called at startup, not for each game
===============
*/
void SV_Init( void )
{
	SV_InitOperatorCommands();

	Cvar_Get ("skill", "1", CVAR_LATCH, "game skill level" );
	Cvar_Get ("deathmatch", "0", CVAR_LATCH, "displays deathmatch state" );
	Cvar_Get ("teamplay", "0", CVAR_LATCH, "displays teamplay state" );
	Cvar_Get ("coop", "0", CVAR_LATCH, "displays cooperative state" );
	Cvar_Get ("protocol", va( "%i", PROTOCOL_VERSION ), CVAR_INIT, "displays server protocol version" );
	Cvar_Get ("defaultmap", "", 0, "holds the multiplayer mapname" );
	Cvar_Get ("showtriggers", "0", CVAR_LATCH, "debug cvar shows triggers" );
	Cvar_Get ("sv_aim", "0", CVAR_ARCHIVE, "enable auto-aiming" );
	Cvar_Get ("mapcyclefile", "mapcycle.txt", 0, "name of multiplayer map cycle configuration file" );
	Cvar_Get ("servercfgfile","server.cfg", 0, "name of dedicated server configuration file" );
	Cvar_Get ("lservercfgfile","listenserver.cfg", 0, "name of listen server configuration file" );
	Cvar_Get ("motdfile", "motd.txt", 0, "name of 'message of the day' file" );
	Cvar_Get ("sv_language", "0", 0, "game language (currently unused)" );
	Cvar_Get ("suitvolume", "0.25", CVAR_ARCHIVE, "HEV suit volume" );
	
	// half-life shared variables
	sv_zmax = Cvar_Get ("sv_zmax", "0", CVAR_PHYSICINFO, "zfar server value" );
	sv_wateramp = Cvar_Get ("sv_wateramp", "0", CVAR_PHYSICINFO, "global water wave height" );
	sv_skycolor_r = Cvar_Get ("sv_skycolor_r", "127", CVAR_PHYSICINFO, "skycolor red (hl1 compatibility)" );
	sv_skycolor_g = Cvar_Get ("sv_skycolor_g", "127", CVAR_PHYSICINFO, "skycolor green (hl1 compatibility)" );
	sv_skycolor_b = Cvar_Get ("sv_skycolor_b", "127", CVAR_PHYSICINFO, "skycolor blue (hl1 compatibility)" );
	sv_skyvec_x = Cvar_Get ("sv_skyvec_x", "1", CVAR_PHYSICINFO, "sky direction x (hl1 compatibility)" );
	sv_skyvec_y = Cvar_Get ("sv_skyvec_y", "0", CVAR_PHYSICINFO, "sky direction y (hl1 compatibility)" );
	sv_skyvec_z = Cvar_Get ("sv_skyvec_z", "-1", CVAR_PHYSICINFO, "sky direction z (hl1 compatibility)" );
	sv_skyname = Cvar_Get ("sv_skyname", "2desert", CVAR_PHYSICINFO, "skybox name (can be dynamically changed in-game)" );
	sv_footsteps = Cvar_Get ("mp_footsteps", "1", CVAR_PHYSICINFO, "can hear footsteps from other players" );

	rcon_password = Cvar_Get( "rcon_password", "", 0, "remote connect password" );
	sv_stepheight = Cvar_Get( "sv_stepheight", "18", CVAR_ARCHIVE|CVAR_PHYSICINFO, "how high you can step up" );
	sv_newunit = Cvar_Get( "sv_newunit", "0", 0, "sets to 1 while new unit is loading" );
	hostname = Cvar_Get( "hostname", "unnamed", CVAR_SERVERINFO|CVAR_ARCHIVE, "host name" );
	timeout = Cvar_Get( "timeout", "125", 0, "connection timeout" );
	zombietime = Cvar_Get( "zombietime", "2", 0, "timeout for clients-zombie (who died but not respawned)" );
	sv_pausable = Cvar_Get( "pausable", "1", 0, "allow players to pause or not" );
	allow_download = Cvar_Get( "allow_download", "0", CVAR_ARCHIVE, "allow download resources" );
	sv_wallbounce = Cvar_Get( "sv_wallbounce", "1.0", CVAR_PHYSICINFO, "bounce factor for client with MOVETYPE_BOUNCE" );
	sv_spectatormaxspeed = Cvar_Get( "sv_spectatormaxspeed", "500", CVAR_PHYSICINFO, "spectator maxspeed" );
	sv_waterfriction = Cvar_Get( "sv_waterfriction", "4", CVAR_PHYSICINFO, "how fast you slow down in water" );
	sv_wateraccelerate = Cvar_Get( "sv_wateraccelerate", "10", CVAR_PHYSICINFO, "rate at which a player accelerates to sv_maxspeed while in the water" );
	sv_rollangle = Cvar_Get( "sv_rollangle", "2", CVAR_PHYSICINFO, "how much to tilt the view when strafing" );
	sv_rollspeed = Cvar_Get( "sv_rollspeed", "200", CVAR_PHYSICINFO, "how much strafing is necessary to tilt the view" );
	sv_airaccelerate = Cvar_Get("sv_airaccelerate", "1", CVAR_PHYSICINFO, "player accellerate in air" );
	sv_maxvelocity = Cvar_Get( "sv_maxvelocity", "2000", CVAR_PHYSICINFO, "max world velocity" );
          sv_gravity = Cvar_Get( "sv_gravity", "800", CVAR_PHYSICINFO, "world gravity" );
	sv_maxspeed = Cvar_Get( "sv_maxspeed", "320", CVAR_PHYSICINFO, "maximum speed a player can accelerate to when on ground");
	sv_accelerate = Cvar_Get( "sv_accelerate", "10", CVAR_PHYSICINFO, "rate at which a player accelerates to sv_maxspeed" );
	sv_friction = Cvar_Get( "sv_friction", "4", CVAR_PHYSICINFO, "how fast you slow down" );
	sv_edgefriction = Cvar_Get( "sv_edgefriction", "1", CVAR_PHYSICINFO, "how much you slow down when nearing a ledge you might fall off" );
	sv_stopspeed = Cvar_Get( "sv_stopspeed", "100", CVAR_PHYSICINFO, "how fast you come to a complete stop" );
	sv_maxclients = Cvar_Get( "sv_maxclients", "1", CVAR_LATCH, "server clients limit" );
	sv_check_errors = Cvar_Get( "sv_check_errors", "0", CVAR_ARCHIVE, "ignore physic engine errors" );
	sv_synchthink = Cvar_Get( "sv_fast_think", "0", CVAR_ARCHIVE, "allows entities to think more often than the server framerate" );
	physinfo = Cvar_Get( "@physinfo", "0", CVAR_READ_ONLY, "" ); // use ->modified value only
	serverinfo = Cvar_Get( "@serverinfo", "0", CVAR_READ_ONLY, "" ); // use ->modified value only
	public_server = Cvar_Get ("public", "0", 0, "change server type from private to public" );
	sv_reconnect_limit = Cvar_Get ("sv_reconnect_limit", "3", CVAR_ARCHIVE, "max reconnect attempts" );
	sv_failuretime = Cvar_Get( "sv_failuretime", "0.5", 0, "after this long without a packet from client, don't send any more until client starts sending again" );
	sv_unlag = Cvar_Get( "sv_unlag", "1", 0, "allow lag compensation on server-side" );
	sv_maxunlag = Cvar_Get( "sv_maxunlag", "0.5", 0, "max latency which can be interpolated" );
	sv_unlagpush = Cvar_Get( "sv_unlagpush", "0.0", 0, "unlag push bias" );
	sv_unlagsamples = Cvar_Get( "sv_unlagsamples", "1", 0, "max samples to interpolate" );
	clockwindow = Cvar_Get( "clockwindow", "0.5", 0, "timewindow to execute client moves" );

	SV_ClearSaveDir ();	// delete all temporary *.hl files
	BF_Init( &net_message, "NetMessage", net_message_buffer, sizeof( net_message_buffer ));
}

/*
==================
SV_FinalMessage

Used by SV_Shutdown to send a final message to all
connected clients before the server goes down.  The messages are sent immediately,
not just stuck on the outgoing message list, because the server is going
to totally exit after returning from this function.
==================
*/
void SV_FinalMessage( char *message, bool reconnect )
{
	sv_client_t	*cl;
	byte		msg_buf[MAX_MSGLEN];
	sizebuf_t		msg;
	int		i;
	
	BF_Init( &msg, "FinalMessage", msg_buf, sizeof( msg_buf ));
	BF_WriteByte( &msg, svc_print );
	BF_WriteByte( &msg, PRINT_HIGH );
	BF_WriteString( &msg, message );

	if( reconnect )
	{
		BF_WriteByte( &msg, svc_changing );

		if( sv.loadgame || sv_maxclients->integer > 1 )
			BF_WriteOneBit( &msg, 1 ); // changelevel
		else BF_WriteOneBit( &msg, 0 );
	}
	else
	{
		BF_WriteByte( &msg, svc_disconnect );
	}

	// send it twice
	// stagger the packets to crutch operating system limited buffers
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
		if( cl->state >= cs_connected && !cl->fakeclient )
			Netchan_Transmit( &cl->netchan, BF_GetNumBytesWritten( &msg ), BF_GetData( &msg ));

	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
		if( cl->state >= cs_connected && !cl->fakeclient )
			Netchan_Transmit( &cl->netchan, BF_GetNumBytesWritten( &msg ), BF_GetData( &msg ));
}

/*
================
SV_Shutdown

Called when each game quits,
before Sys_Quit or Sys_Error
================
*/
void SV_Shutdown( bool reconnect )
{
	// already freed
	if( host.state == HOST_ERROR ) return;
	if( !SV_Active()) return;

	MsgDev( D_INFO, "SV_Shutdown: %s\n", host.finalmsg );
	if( svs.clients ) SV_FinalMessage( host.finalmsg, reconnect );

	Master_Shutdown();

	if( !reconnect ) SV_UnloadProgs ();
	else SV_DeactivateServer ();

	// free current level
	Mem_Set( &sv, 0, sizeof( sv ));
	Host_SetServerState( sv.state );

	// free server static data
	if( svs.clients ) Z_Free( svs.clients );
	if( svs.baselines ) Z_Free( svs.baselines );

	Mem_Set( &svs, 0, sizeof( svs ));
}