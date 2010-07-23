//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_main.c - server main loop
//=======================================================================

#include "common.h"
#include "server.h"

#define HEARTBEAT_SECONDS	(300 * 1000) 	// 300 seconds

netadr_t	master_adr[MAX_MASTERS];		// address of group servers

cvar_t	*sv_fps;
cvar_t	*sv_zmax;
cvar_t	*sv_pausable;
cvar_t	*sv_newunit;
cvar_t	*sv_wateramp;
cvar_t	*timeout;				// seconds without any message
cvar_t	*zombietime;			// seconds to sink messages after disconnect
cvar_t	*rcon_password;			// password for remote server commands
cvar_t	*allow_download;
cvar_t	*sv_enforcetime;
cvar_t	*sv_airaccelerate;
cvar_t	*sv_wateraccelerate;
cvar_t	*sv_idealpitchscale;
cvar_t	*sv_maxvelocity;
cvar_t	*sv_gravity;
cvar_t	*sv_stepheight;
cvar_t	*sv_noreload;		// don't reload level state when reentering
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
cvar_t	*public_server;		// should heartbeats be sent
cvar_t	*sv_reconnect_limit;	// minimum seconds between connect messages
cvar_t	*serverinfo;
cvar_t	*physinfo;

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

	if( cl->edict->v.flags & FL_FAKECLIENT )
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
===================
SV_GiveMsec

Every few frames, gives all clients an allotment of milliseconds
for their command moves.  If they exceed it, assume cheating.
===================
*/
void SV_GiveMsec( void )
{
	int		i;
	sv_client_t	*cl;

	if( sv.framenum & 15 )
		return;

	for( i = 0; i < sv_maxclients->integer; i++ )
	{
		cl = &svs.clients[i];
		if( cl->state == cs_free )
			continue;
		
		cl->commandMsec = 1800; // 1600 + some slop
	}
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
	static int	oldserverflags = 0;
	string		tmp;

	if( svgame.globals->serverflags != oldserverflags )
	{
		// update serverflags
		SV_ConfigString( CS_SERVERFLAGS, va( "%i", svgame.globals->serverflags ));
		oldserverflags = svgame.globals->serverflags;		
	}

	if( sv_zmax->modified )
	{
		SV_ConfigString( CS_ZFAR, sv_zmax->string );
		sv_zmax->modified = false;
	}

	if( sv_wateramp->modified )
	{
		SV_ConfigString( CS_WATERAMP, sv_wateramp->string );
		sv_wateramp->modified = false;
	}

	if( sv_skyname->modified )
	{
		SV_ConfigString( CS_SKYNAME, sv_skyname->string );
		sv_skyname->modified = false;
	}

	if( sv_skycolor_r->modified || sv_skycolor_g->modified || sv_skycolor_g->modified )
	{
		com.snprintf( tmp, sizeof( tmp ), "%d %d %d", sv_skycolor_r->integer, sv_skycolor_g->integer, sv_skycolor_b->integer );
		sv_skycolor_r->modified = sv_skycolor_g->modified = sv_skycolor_g->modified = false;
		SV_ConfigString( CS_SKYCOLOR, tmp );
	}

	if( sv_skyvec_x->modified || sv_skyvec_y->modified || sv_skyvec_z->modified )
	{
		com.snprintf( tmp, sizeof( tmp ), "%f %f %f", sv_skyvec_x->value, sv_skyvec_y->value, sv_skyvec_z->value );
		sv_skyvec_x->modified = sv_skyvec_y->modified = sv_skyvec_z->modified = false;
		SV_ConfigString( CS_SKYVEC, tmp );
	}

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
	svgame.movevars.footsteps = Cvar_VariableInteger( "mp_footsteps" );
	svgame.movevars.rollangle = sv_rollangle->value;
	svgame.movevars.rollspeed = sv_rollspeed->value;

	MSG_Clear( &sv.multicast );

	if( MSG_WriteDeltaMovevars( &sv.multicast, &svgame.oldmovevars, &svgame.movevars ))
	{
		MSG_Send( MSG_ALL, vec3_origin, NULL );
		Mem_Copy( &svgame.oldmovevars, &svgame.movevars, sizeof( movevars_t )); // oldstate changed
	}
	physinfo->modified = false;
}

void pfnUpdateServerInfo( const char *szKey, const char *szValue, const char *unused, void *unused2 )
{
	cvar_t	*cv = Cvar_FindVar( szKey );

	if( !cv || !cv->modified ) return; // this cvar not changed

	MSG_WriteByte( &sv.multicast, svc_serverinfo );
	MSG_WriteString( &sv.multicast, szKey );
	MSG_WriteString( &sv.multicast, szValue );
	MSG_Send( MSG_ALL, vec3_origin, NULL );
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
SV_CalcFrameTime
=================
*/
void SV_CalcFrameTime( void )
{
	if( sv_fps->modified )
	{
		if( sv_fps->value < 10 ) Cvar_Set( "sv_fps", "10" ); // too slow, also, netcode uses a byte
		else if( sv_fps->value > 90 ) Cvar_Set( "sv_fps", "90" ); // abusive
		sv_fps->modified = false;
	}

	// calc sv.frametime
	sv.frametime = ( 1000 / sv_fps->integer );
}

/*
=================
SV_ReadPackets
=================
*/
void SV_ReadPackets( void )
{
	int		i;
	sv_client_t	*cl;
	int		qport;

	while( NET_GetPacket( NS_SERVER, &net_from, &net_message ))
	{
		// check for connectionless packet (0xffffffff) first
		if( net_message.cursize >= 4 && *(int *)net_message.data == -1 )
		{
			SV_ConnectionlessPacket( net_from, &net_message );
			continue;
		}

		// read the qport out of the message so we can fix up
		// stupid address translating routers
		MSG_BeginReading( &net_message );
		MSG_ReadLong( &net_message );	// sequence number
		MSG_ReadLong( &net_message );	// sequence number
		qport = (int)MSG_ReadShort( &net_message ) & 0xffff;

		// check for packets from connected clients
		for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
		{
			if( cl->state == cs_free ) continue;
			if( cl->edict && (cl->edict->v.flags & FL_FAKECLIENT )) continue;
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
					cl->lastmessage = svs.realtime; // don't timeout
					SV_ExecuteClientMessage( cl, &net_message );
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
	int		droppoint;
	int		zombiepoint;
	int		i, numclients = 0;

	droppoint = svs.realtime - ( timeout->value * 1000 );
	zombiepoint = svs.realtime - ( zombietime->value * 1000 );

	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( cl->state >= cs_connected )
		{
			if( cl->edict && !( cl->edict->v.flags & (FL_SPECTATOR|FL_FAKECLIENT)))
				numclients++;
                    }

		// fake clients do not timeout
		if( cl->edict && (cl->edict->v.flags & FL_FAKECLIENT ))
			cl->lastmessage = svs.realtime;

		// message times may be wrong across a changelevel
		if( cl->lastmessage > svs.realtime )
			cl->lastmessage = svs.realtime;

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

	for( i = 1; i < svgame.globals->numEntities; i++ )
	{
		ent = EDICT_NUM( i );
		if( ent->free ) continue;

		ent->pvServerData->s.ed_flags = 0;
		ent->v.effects &= ~EF_MUZZLEFLASH;

		// clear NOINTERP flag automatically only for alive creatures			
		if( ent->v.flags & ( FL_MONSTER|FL_CLIENT|FL_FAKECLIENT ) && ent->v.deadflag < DEAD_DEAD )
			ent->v.effects &= ~EF_NOINTERP;
	}
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
=================
SV_RunGameFrame
=================
*/
void SV_RunGameFrame( void )
{
	if( !SV_HasActivePlayers()) return;

	// we always need to bump framenum, even if we
	// don't run the world, otherwise the delta
	// compression can get confused when a client
	// has the "current" frame
	sv.framenum++;

	// don't run if paused or not in game
	if( !sv.paused && CL_IsInGame( ))
		SV_Physics();

	// never get more than one tic behind
	if( sv.time < svs.realtime )
		svs.realtime = sv.time;
}

/*
==================
SV_Frame

==================
*/
void SV_Frame( int time )
{
	// if server is not active, do nothing
	if( !svs.initialized ) return;

	svs.realtime += time;

	// keep the random time dependent
	rand ();

	// calc sv.frametime
	SV_CalcFrameTime ();

	// check timeouts
	SV_CheckTimeouts ();

	// read packets from clients
	SV_ReadPackets ();

	// move autonomous things around if enough time has passed
	if( svs.realtime < sv.time )
	{
		// never let the time get too far off
		if( sv.time - svs.realtime > sv.frametime )
			svs.realtime = sv.time - sv.frametime;
		NET_Sleep( sv.time - svs.realtime );
		return;
	}

	// update ping based on the last known frame from all clients
	SV_CalcPings ();

	// give the clients some timeslices
	SV_GiveMsec ();

	// let everything in the world think and move
	SV_RunGameFrame ();

	// refresh serverinfo on the client side
	SV_UpdateServerInfo ();

	// refresh physic movevars on the client side
	SV_UpdateMovevars ();

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
	if( svs.last_heartbeat > svs.realtime )
		svs.last_heartbeat = svs.realtime;

	if(( svs.realtime - svs.last_heartbeat ) < HEARTBEAT_SECONDS )
		return; // not time to send yet

	svs.last_heartbeat = svs.realtime;

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
	Cvar_Get ("deathmatch", "0", CVAR_SERVERINFO|CVAR_LATCH, "displays deathmatch state" );
	Cvar_Get ("teamplay", "0", CVAR_SERVERINFO|CVAR_LATCH, "displays teamplay state" );
	Cvar_Get ("coop", "0", CVAR_SERVERINFO|CVAR_LATCH, "displays cooperative state" );
	Cvar_Get ("dmflags", "0", CVAR_SERVERINFO, "setup deathmatch flags" );
	Cvar_Get ("fraglimit", "0", CVAR_SERVERINFO, "multiplayer fraglimit" );
	Cvar_Get ("timelimit", "0", CVAR_SERVERINFO, "multiplayer timelimit" );
	Cvar_Get ("protocol", va("%i", PROTOCOL_VERSION), CVAR_SERVERINFO|CVAR_INIT, "displays server protocol version" );
	Cvar_Get ("defaultmap", "", 0, "holds the multiplayer mapname" );
	Cvar_Get ("showtriggers", "0", CVAR_LATCH, "debug cvar shows triggers" );
	Cvar_Get ("sv_aim", "0", CVAR_ARCHIVE, "enable auto-aiming" );

	// half-life shared variables
	sv_zmax = Cvar_Get ("sv_zmax", "0", 0, "zfar server value" );
	sv_wateramp = Cvar_Get ("sv_wateramp", "0", 0, "global water wave height" );
	sv_skycolor_r = Cvar_Get ("sv_skycolor_r", "127", 0, "skycolor red (hl1 compatibility)" );
	sv_skycolor_g = Cvar_Get ("sv_skycolor_g", "127", 0, "skycolor green (hl1 compatibility)" );
	sv_skycolor_b = Cvar_Get ("sv_skycolor_b", "127", 0, "skycolor blue (hl1 compatibility)" );
	sv_skyvec_x = Cvar_Get ("sv_skyvec_x", "1", 0, "sky direction x (hl1 compatibility)" );
	sv_skyvec_y = Cvar_Get ("sv_skyvec_y", "0", 0, "sky direction y (hl1 compatibility)" );
	sv_skyvec_z = Cvar_Get ("sv_skyvec_z", "-1", 0, "sky direction z (hl1 compatibility)" );
	sv_skyname = Cvar_Get ("sv_skyname", "", 0, "skybox name (can be dynamically changed in-game)" );

	rcon_password = Cvar_Get( "rcon_password", "", 0, "remote connect password" );
	sv_fps = Cvar_Get( "sv_fps", "72.1", CVAR_ARCHIVE, "running server physics at" );
	sv_stepheight = Cvar_Get( "sv_stepheight", "18", CVAR_ARCHIVE|CVAR_PHYSICINFO, "how high you can step up" );
	sv_newunit = Cvar_Get( "sv_newunit", "0", 0, "sets to 1 while new unit is loading" );
	hostname = Cvar_Get( "sv_hostname", "unnamed", CVAR_SERVERINFO | CVAR_ARCHIVE, "host name" );
	timeout = Cvar_Get( "timeout", "125", 0, "connection timeout" );
	zombietime = Cvar_Get( "zombietime", "2", 0, "timeout for clients-zombie (who died but not respawned)" );
	sv_pausable = Cvar_Get( "pausable", "1", 0, "allow players to pause or not" );
	sv_enforcetime = Cvar_Get( "sv_enforcetime", "0", 0, "enable client enforce time (anti-cheating)" );
	allow_download = Cvar_Get( "allow_download", "0", CVAR_ARCHIVE, "allow download resources" );
	sv_noreload = Cvar_Get( "sv_noreload", "0", 0, "ignore savepoints for singleplayer" );
	sv_wallbounce = Cvar_Get( "sv_wallbounce", "1.0", CVAR_PHYSICINFO, "bounce factor for client with MOVETYPE_BOUNCE" );
	sv_spectatormaxspeed = Cvar_Get( "sv_spectatormaxspeed", "500", CVAR_PHYSICINFO, "spectator maxspeed" );
	sv_waterfriction = Cvar_Get( "sv_waterfriction", "4", CVAR_PHYSICINFO, "how fast you slow down in water" );
	sv_wateraccelerate = Cvar_Get( "sv_wateraccelerate", "10", CVAR_PHYSICINFO, "rate at which a player accelerates to sv_maxspeed while in the water" );
	sv_rollangle = Cvar_Get( "sv_rollangle", "2", CVAR_PHYSICINFO, "how much to tilt the view when strafing" );
	sv_rollspeed = Cvar_Get( "sv_rollspeed", "200", CVAR_PHYSICINFO, "how much strafing is necessary to tilt the view" );
	sv_airaccelerate = Cvar_Get("sv_airaccelerate", "1", CVAR_PHYSICINFO, "player accellerate in air" );
	sv_idealpitchscale = Cvar_Get( "sv_idealpitchscale", "0.8", 0, "how much to look up/down slopes and stairs when not using freelook" );
	sv_maxvelocity = Cvar_Get( "sv_maxvelocity", "2000", CVAR_PHYSICINFO, "max world velocity" );
          sv_gravity = Cvar_Get( "sv_gravity", "800", CVAR_PHYSICINFO, "world gravity" );
	sv_maxspeed = Cvar_Get( "sv_maxspeed", "320", CVAR_PHYSICINFO, "maximum speed a player can accelerate to when on ground");
	sv_accelerate = Cvar_Get( "sv_accelerate", "10", CVAR_PHYSICINFO, "rate at which a player accelerates to sv_maxspeed" );
	sv_friction = Cvar_Get( "sv_friction", "4", CVAR_PHYSICINFO, "how fast you slow down" );
	sv_edgefriction = Cvar_Get( "sv_edgefriction", "1", CVAR_PHYSICINFO, "how much you slow down when nearing a ledge you might fall off" );
	sv_stopspeed = Cvar_Get( "sv_stopspeed", "100", CVAR_PHYSICINFO, "how fast you come to a complete stop" );
	sv_maxclients = Cvar_Get( "sv_maxclients", "1", CVAR_SERVERINFO|CVAR_LATCH, "server clients limit" );
	sv_check_errors = Cvar_Get( "sv_check_errors", "0", CVAR_ARCHIVE, "ignore physic engine errors" );
	sv_synchthink = Cvar_Get( "sv_fast_think", "0", CVAR_ARCHIVE, "allows entities to think more often than the server framerate" );
	physinfo = Cvar_Get( "@physinfo", "0", CVAR_READ_ONLY, "" ); // use ->modified value only
	serverinfo = Cvar_Get( "@serverinfo", "0", CVAR_READ_ONLY, "" ); // use ->modified value only
	public_server = Cvar_Get ("public", "0", 0, "change server type from private to public" );
	sv_reconnect_limit = Cvar_Get ("sv_reconnect_limit", "3", CVAR_ARCHIVE, "max reconnect attempts" );

	SV_ClearSaveDir ();	// delete all temporary *.hl files
	MSG_Init( &net_message, net_message_buffer, sizeof( net_message_buffer ));
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
	
	MSG_Init( &msg, msg_buf, sizeof( msg_buf ));
	MSG_WriteByte( &msg, svc_print );
	MSG_WriteByte( &msg, PRINT_HIGH );
	MSG_WriteString( &msg, message );

	if( reconnect )
	{
		if( sv.loadgame )
			MSG_WriteByte( &msg, svc_changing );
		else MSG_WriteByte( &msg, svc_reconnect );
	}
	else
	{
		MSG_WriteByte( &msg, svc_disconnect );
	}

	// send it twice
	// stagger the packets to crutch operating system limited buffers
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
		if( cl->state >= cs_connected && !(cl->edict && cl->edict->v.flags & FL_FAKECLIENT ))
			Netchan_Transmit( &cl->netchan, msg.cursize, msg.data );

	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
		if( cl->state >= cs_connected && !(cl->edict && cl->edict->v.flags & FL_FAKECLIENT ))
			Netchan_Transmit( &cl->netchan, msg.cursize, msg.data );
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
	if( svs.client_entities ) Z_Free( svs.client_entities );
	Mem_Set( &svs, 0, sizeof( svs ));
}