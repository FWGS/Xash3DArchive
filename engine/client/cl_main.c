//=======================================================================
//			Copyright XashXT Group 2009 �
//			cl_main.c - client main loop
//=======================================================================

#include "common.h"
#include "client.h"
#include "net_encode.h"
#include "cl_tent.h"
#include "input.h"

#define MAX_TOTAL_CMDS		16
#define MIN_CMD_RATE		10.0
#define MAX_CMD_BUFFER		4000
#define CONNECTION_PROBLEM_TIME	15.0	// 15 seconds

convar_t	*rcon_client_password;
convar_t	*rcon_address;

convar_t	*cl_smooth;
convar_t	*cl_timeout;
convar_t	*cl_predict;
convar_t	*cl_showfps;
convar_t	*cl_nodelta;
convar_t	*cl_crosshair;
convar_t	*cl_cmdbackup;
convar_t	*cl_draw_particles;
convar_t	*cl_lightstyle_lerping;
convar_t	*cl_idealpitchscale;
convar_t	*cl_solid_players;
convar_t	*cl_draw_beams;
convar_t	*cl_showmiss;
convar_t	*cl_cmdrate;
convar_t	*userinfo;

//
// userinfo
//
convar_t	*name;
convar_t	*model;
convar_t	*topcolor;
convar_t	*bottomcolor;
convar_t	*rate;

client_t		cl;
client_static_t	cls;
clgame_static_t	clgame;

//======================================================================
qboolean CL_Active( void )
{
	return ( cls.state == ca_active );
}

//======================================================================
qboolean CL_IsInGame( void )
{
	if( host.type == HOST_DEDICATED ) return true;	// always active for dedicated servers
	if( CL_GetMaxClients() > 1 ) return true;	// always active for multiplayer
	return ( cls.key_dest == key_game );		// active if not menu or console
}

qboolean CL_IsInMenu( void )
{
	return ( cls.key_dest == key_menu );
}

qboolean CL_IsPlaybackDemo( void )
{
	return cls.demoplayback;
}

/*
===============
CL_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
static float CL_LerpPoint( void )
{
	float	f, frac;

	f = cl.mtime[0] - cl.mtime[1];
	
	if( !f || SV_Active( ))
	{
		cl.time = cl.mtime[0];
		return 1.0f;
	}
		
	if( f > 0.1f )
	{	
		// dropped packet, or start of demo
		cl.mtime[1] = cl.mtime[0] - 0.1f;
		f = 0.1f;
	}

	frac = ( cl.time - cl.mtime[1] ) / f;

	if( frac < 0 )
	{
		if( frac < -0.01f )
			cl.time = cl.mtime[1];
		frac = 0.0f;
	}
	else if( frac > 1.0f )
	{
		if( frac > 1.01f )
			cl.time = cl.mtime[0];
		frac = 1.0f;
	}
		
	return frac;
}

/*
=================
CL_ComputePacketLoss

=================
*/
void CL_ComputePacketLoss( void )
{
	int	i, frm;
	frame_t	*frame;
	int	count = 0;
	int	lost = 0;

	if( host.realtime < cls.packet_loss_recalc_time )
		return;

	// recalc every second
	cls.packet_loss_recalc_time = host.realtime + 1.0;

	// compuate packet loss
	for( i = cls.netchan.incoming_sequence - CL_UPDATE_BACKUP+1; i <= cls.netchan.incoming_sequence; i++ )
	{
		frm = i;
		frame = &cl.frames[frm & CL_UPDATE_MASK];

		if( frame->receivedtime == -1 )
			lost++;
		count++;
	}

	if( count <= 0 ) cls.packet_loss = 0.0f;
	else cls.packet_loss = ( 100.0f * (float)lost ) / (float)count;
}

/*
=======================================================================

CLIENT RELIABLE COMMAND COMMUNICATION

=======================================================================
*/
/*
===================
Cmd_ForwardToServer

adds the current command line as a clc_stringcmd to the client message.
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
/*
==================
CL_ForwardToServer_f
==================
*/
void CL_ForwardToServer_f( void )
{
	char	*cmd;

	if( cls.demoplayback )
	{
		if( !com.stricmp( Cmd_Argv( 1 ), "pause" ))
			cl.refdef.paused ^= 1;
		return;
	}

	if( cls.state != ca_connected && cls.state != ca_active )
		return; // not connected

	cmd = Cmd_Argv( 0 );
	if( *cmd == '-' || *cmd == '+' )
	{
		Msg( "Unknown command \"%s\"\n", cmd );
		return;
	}	

	// don't forward the first argument
	if( Cmd_Argc() > 1 )
	{
		BF_WriteByte( &cls.netchan.message, clc_stringcmd );
		BF_WriteString( &cls.netchan.message, Cmd_Args( ));
	}
}

/*
===================
Cmd_ForwardToServer

adds the current command line as a clc_stringcmd to the client message.
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void Cmd_ForwardToServer( void )
{
	char	*cmd;

	if( cls.demoplayback )
	{
		if( !com.stricmp( Cmd_Argv( 1 ), "pause" ))
			cl.refdef.paused ^= 1;
		return;
	}

	cmd = Cmd_Argv( 0 );
	if( cls.state <= ca_connected || *cmd == '-' || *cmd == '+' )
	{
		Msg( "Unknown command \"%s\"\n", cmd );
		return;
	}

	BF_WriteByte( &cls.netchan.message, clc_stringcmd );

	if( Cmd_Argc() > 1 )
		BF_WriteString( &cls.netchan.message, va( "%s %s", cmd, Cmd_Args( )));
	else BF_WriteString( &cls.netchan.message, cmd );
}

/*
=======================================================================

CLIENT MOVEMENT COMMUNICATION

=======================================================================
*/
/*
=================
CL_CreateCmd
=================
*/
usercmd_t CL_CreateCmd( void )
{
	usercmd_t		cmd;
	static double	extramsec = 0;
	color24		color;
	int		ms;

	// send milliseconds of time to apply the move
//	extramsec += ( cl.time - cl.oldtime ) * 1000;
	extramsec += ( host.frametime * 1000 );
	ms = extramsec;
	extramsec -= ms;		// fractional part is left for next frame
	if( ms > 250 ) ms = 100;	// time was unreasonable

	Mem_Set( &cmd, 0, sizeof( cmd ));

	VectorCopy( cl.frame.local.client.origin, cl.data.origin );
	VectorCopy( cl.refdef.cl_viewangles, cl.data.viewangles );
	cl.data.iWeaponBits = cl.frame.local.client.weapons;
	cl.data.fov = cl.frame.local.client.fov;

	clgame.dllFuncs.pfnUpdateClientData( &cl.data, cl.time );

	// grab changes
	VectorCopy( cl.data.viewangles, cl.refdef.cl_viewangles );
	cl.frame.local.client.weapons = cl.data.iWeaponBits;
	cl.frame.local.client.fov = cl.data.fov;

	// allways dump the first ten messages,
	// because it may contain leftover inputs
	// from the last level
	if( ++cl.movemessages <= 10 )
		return cmd;

	clgame.dllFuncs.CL_CreateMove( cl.time - cl.oldtime, &cmd, ( cls.state == ca_active && !cl.refdef.paused ));

	R_LightForPoint( cl.frame.local.client.origin, &color, false, 128.0f );
	cmd.lightlevel = (color.r + color.g + color.b) / 3;

	// random seed for predictable random values
	cl.random_seed = Com_RandomLong( 0, 0x7fffffff ); // full range

	// never let client.dll calc frametime for player
	// because is potential backdoor for cheating
	cmd.msec = ms;

	return cmd;
}

void CL_WriteUsercmd( sizebuf_t *msg, int from, int to )
{
	usercmd_t	nullcmd;
	usercmd_t	*f, *t;

	ASSERT( from == -1 || ( from >= 0 && from < MULTIPLAYER_BACKUP ) );
	ASSERT( to >= 0 && to < MULTIPLAYER_BACKUP );

	if( from == -1 )
	{
		Mem_Set( &nullcmd, 0, sizeof( nullcmd ));
		f = &nullcmd;
	}
	else
	{
		f = &cl.cmds[from];
	}

	t = &cl.cmds[to];

	// write it into the buffer
	MSG_WriteDeltaUsercmd( msg, f, t );
}

/*
===================
CL_WritePacket

Create and send the command packet to the server
Including both the reliable commands and the usercmds

During normal gameplay, a client packet will contain something like:

1 clc_move
<usercmd[-2]>
<usercmd[-1]>
<usercmd[-0]>
===================
*/
void CL_WritePacket( void )
{
	sizebuf_t		buf;
	qboolean		send_command = false;
	byte		data[MAX_CMD_BUFFER];
	int		i, from, to, key, size;
	int		numbackup = 2;
	int		numcmds;
	int		newcmds;
	int		cmdnumber;
	
	// don't send anything if playing back a demo
	if( cls.demoplayback || cls.state == ca_cinematic )
		return;

	if( cls.state == ca_disconnected || cls.state == ca_connecting )
		return;

/*
	if( cls.state == ca_connected )
	{
		// just update reliable
		if( cls.netchan.message.iCurBit || host.realtime - cls.netchan.last_sent > 1.0f )
			Netchan_Transmit( &cls.netchan, 0, NULL );
		return;
	}
*/
	CL_ComputePacketLoss ();

	if( cl_cmdrate->value < MIN_CMD_RATE )
	{
		Cvar_SetFloat( "cl_cmdrate", MIN_CMD_RATE );
	}

	BF_Init( &buf, "ClientData", data, sizeof( data ));

	// Determine number of backup commands to send along
	numbackup = bound( 0, cl_cmdbackup->integer, MAX_BACKUP_COMMANDS );
	if( cls.state == ca_connected ) numbackup = 0;

	// Check to see if we can actually send this command

	// In single player, send commands as fast as possible
	// Otherwise, only send when ready and when not choking bandwidth
	if(( cl.maxclients == 1 ) || ( NET_IsLocalAddress( cls.netchan.remote_address ) && !host_limitlocal->integer ))
		send_command = true;
	else if(( host.realtime >= cls.nextcmdtime ) && Netchan_CanPacket( &cls.netchan ))
		send_command = true;

	if( cl.force_send_usercmd )
	{
		send_command = true;
		cl.force_send_usercmd = false;
	}

	if(( cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged ) >= CL_UPDATE_MASK )
	{
		if(( host.realtime - cls.netchan.last_received ) > CONNECTION_PROBLEM_TIME && !cls.demoplayback )
		{
			MsgDev( D_WARN, "^1 Connection Problem^7\n" );
			cl.validsequence = 0;
		}
	}

	if( cl_nodelta->integer )
	{
		cl.validsequence = 0;
	}

	// send a userinfo update if needed
	if( userinfo->modified )
	{
		userinfo->modified = false;
		BF_WriteByte( &cls.netchan.message, clc_userinfo );
		BF_WriteString( &cls.netchan.message, Cvar_Userinfo( ));
	}

	if( send_command )
	{
		int	outgoing_sequence;
	
		if( cl_cmdrate->integer > 0 )
			cls.nextcmdtime = host.realtime + ( 1.0f / cl_cmdrate->value );
		else cls.nextcmdtime = host.realtime; // always able to send right away

		if( cls.lastoutgoingcommand == -1 )
		{
			outgoing_sequence = cls.netchan.outgoing_sequence;
			cls.lastoutgoingcommand = cls.netchan.outgoing_sequence;
		}
		else outgoing_sequence = cls.lastoutgoingcommand + 1;

		// begin a client move command
		BF_WriteByte( &buf, clc_move );

		// save the position for a checksum byte
		key = BF_GetRealBytesWritten( &buf );
		BF_WriteByte( &buf, 0 );

		// write packet lossage percentation
		BF_WriteByte( &buf, cls.packet_loss );

		// say how many backups we'll be sending
		BF_WriteByte( &buf, numbackup );

		// how many real commands have queued up
		newcmds = ( cls.netchan.outgoing_sequence - cls.lastoutgoingcommand );

		// put an upper/lower bound on this
		newcmds = bound( 0, newcmds, MAX_TOTAL_CMDS );
		if( cls.state == ca_connected ) newcmds = 0;
	
		BF_WriteByte( &buf, newcmds );

		numcmds = newcmds + numbackup;
		from = -1;

		for( i = numcmds - 1; i >= 0; i-- )
		{
			cmdnumber = ( cls.netchan.outgoing_sequence - i ) & CL_UPDATE_MASK;

			to = cmdnumber;
			CL_WriteUsercmd( &buf, from, to );
			from = to;

			if( BF_CheckOverflow( &buf ))
			{
				Host_Error( "CL_Move, overflowed command buffer (%i bytes)\n", MAX_CMD_BUFFER );
			}
		}

		// calculate a checksum over the move commands
		size = BF_GetRealBytesWritten( &buf ) - key - 1;
		buf.pData[key] = CRC32_Sequence( buf.pData + key + 1, size, cls.netchan.outgoing_sequence );

		// message we are constructing.
		i = cls.netchan.outgoing_sequence & CL_UPDATE_MASK;
	
		// determine if we need to ask for a new set of delta's.
		if( cl.validsequence && !( cls.demorecording && cls.demowaiting ))
		{
			cl.delta_sequence = cl.validsequence;

			BF_WriteByte( &buf, clc_delta );
			BF_WriteByte( &buf, cl.validsequence & 0xFF );
		}
		else
		{
			// request delta compression of entities
			cl.delta_sequence = -1;
		}

		if( BF_CheckOverflow( &buf ))
		{
			Host_Error( "CL_Move, overflowed command buffer (%i bytes)\n", MAX_CMD_BUFFER );
		}

		// remember outgoing command that we are sending
		cls.lastoutgoingcommand = cls.netchan.outgoing_sequence;

		// deliver the message (or update reliable)
		Netchan_Transmit( &cls.netchan, BF_GetNumBytesWritten( &buf ), BF_GetData( &buf ));
	}
	else
	{
		// Increment sequence number so we can detect that we've held back packets.
		cls.netchan.outgoing_sequence++;
	}

	// update download/upload slider.
	Netchan_UpdateProgress( &cls.netchan );
}

/*
=================
CL_SendCmd

Called every frame to builds and sends a command packet to the server.
=================
*/
void CL_SendCmd( void )
{
	// we create commands even if a demo is playing,
	cl.refdef.cmd = &cl.cmds[cls.netchan.outgoing_sequence & CL_UPDATE_MASK];
	*cl.refdef.cmd = CL_CreateCmd();

	// clc_move, userinfo etc
	CL_WritePacket();
}

/*
==================
CL_Quit_f
==================
*/
void CL_Quit_f( void )
{
	CL_Disconnect();
	Sys_Quit();
}

/*
================
CL_Drop

Called after an Host_Error was thrown
================
*/
void CL_Drop( void )
{
	if( cls.state == ca_uninitialized )
		return;
	CL_Disconnect();
}

/*
=======================
CL_SendConnectPacket

We have gotten a challenge from the server, so try and
connect.
======================
*/
void CL_SendConnectPacket( void )
{
	netadr_t	adr;
	int	port;

	if( !NET_StringToAdr( cls.servername, &adr ))
	{
		MsgDev( D_INFO, "CL_SendConnectPacket: bad server address\n");
		cls.connect_time = 0;
		return;
	}

	if( adr.port == 0 ) adr.port = BF_BigShort( PORT_SERVER );
	port = Cvar_VariableValue( "net_qport" );

	userinfo->modified = false;
	Netchan_OutOfBandPrint( NS_CLIENT, adr, "connect %i %i %i \"%s\"\n", PROTOCOL_VERSION, port, cls.challenge, Cvar_Userinfo( ));
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
void CL_CheckForResend( void )
{
	netadr_t	adr;

	// if the local server is running and we aren't then connect
	if( cls.state == ca_disconnected && SV_Active( ))
	{
		cls.state = ca_connecting;
		com.strncpy( cls.servername, "localhost", sizeof( cls.servername ));
		// we don't need a challenge on the localhost
		CL_SendConnectPacket();
		return;
	}

	// resend if we haven't gotten a reply yet
	if( cls.demoplayback || cls.state != ca_connecting )
		return;

	if(( host.realtime - cls.connect_time ) < 3.0f )
		return;

	if( !NET_StringToAdr( cls.servername, &adr ))
	{
		MsgDev( D_ERROR, "CL_CheckForResend: bad server address\n" );
		cls.state = ca_disconnected;
		return;
	}

	if( adr.port == 0 ) adr.port = BF_BigShort( PORT_SERVER );
	cls.connect_time = host.realtime; // for retransmit requests

	MsgDev( D_NOTE, "Connecting to %s...\n", cls.servername );
	Netchan_OutOfBandPrint( NS_CLIENT, adr, "getchallenge\n" );
}


/*
================
CL_Connect_f

================
*/
void CL_Connect_f( void )
{
	char	*server;

	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: connect <server>\n" );
		return;	
	}
	
	if( Host_ServerState())
	{	
		// if running a local server, kill it and reissue
		com.strncpy( host.finalmsg, "Server quit\n", MAX_STRING );
		SV_Shutdown( false );
	}

	server = Cmd_Argv( 1 );
	NET_Config( true ); // allow remote
	
	Msg( "server %s\n", server );
	CL_Disconnect();

	cls.state = ca_connecting;
	com.strncpy( cls.servername, server, sizeof( cls.servername ));
	cls.connect_time = MAX_HEARTBEAT; // CL_CheckForResend() will fire immediately
}


/*
=====================
CL_Rcon_f

Send the rest of the command line over as
an unconnected command.
=====================
*/
void CL_Rcon_f( void )
{
	char	message[1024];
	netadr_t	to;
	int	i;

	if( !rcon_client_password->string )
	{
		Msg( "You must set 'rcon_password' before\n" "issuing an rcon command.\n" );
		return;
	}

	message[0] = (char)255;
	message[1] = (char)255;
	message[2] = (char)255;
	message[3] = (char)255;
	message[4] = 0;

	NET_Config( true );	// allow remote

	com.strcat( message, "rcon " );
	com.strcat( message, rcon_client_password->string );
	com.strcat( message, " " );

	for( i = 1; i < Cmd_Argc(); i++ )
	{
		com.strcat( message, Cmd_Argv( i ));
		com.strcat( message, " " );
	}

	if( cls.state >= ca_connected )
	{
		to = cls.netchan.remote_address;
	}
	else
	{
		if( !com.strlen( rcon_address->string ))
		{
			Msg( "You must either be connected,\n" "or set the 'rcon_address' cvar\n" "to issue rcon commands\n" );
			return;
		}

		NET_StringToAdr( rcon_address->string, &to );
		if( to.port == 0 ) to.port = BF_BigShort( PORT_SERVER );
	}
	
	NET_SendPacket( NS_CLIENT, com.strlen( message ) + 1, message, to );
}


/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState( void )
{
	if( !cls.changelevel ) 
	{
		// continue playing if we are changing level
		S_StopBackgroundTrack ();
	}

	S_StopAllSounds ();
	CL_ClearEffects ();
	CL_FreeEdicts ();

	// wipe the entire cl structure
	Mem_Set( &cl, 0, sizeof( cl ));
	BF_Clear( &cls.netchan.message );
	Mem_Set( &clgame.fade, 0, sizeof( clgame.fade ));
	Mem_Set( &clgame.shake, 0, sizeof( clgame.shake ));
	cl.refdef.movevars = &clgame.movevars;

	Cvar_SetFloat( "scr_download", 0.0f );
	Cvar_SetFloat( "scr_loading", 0.0f );
}

/*
=====================
CL_SendDisconnectMessage

Sends a disconnect message to the server
=====================
*/
void CL_SendDisconnectMessage( void )
{
	sizebuf_t	buf;
	byte	data[32];

	if( cls.state == ca_disconnected ) return;

	BF_Init( &buf, "LastMessage", data, sizeof( data ));
	BF_WriteByte( &buf, clc_stringcmd );
	BF_WriteString( &buf, "disconnect" );

	if( !cls.netchan.remote_address.type )
		cls.netchan.remote_address.type = NA_LOOPBACK;

	// make sure message will be delivered
	Netchan_Transmit( &cls.netchan, BF_GetNumBytesWritten( &buf ), BF_GetData( &buf ));
	Netchan_Transmit( &cls.netchan, BF_GetNumBytesWritten( &buf ), BF_GetData( &buf ));
	Netchan_Transmit( &cls.netchan, BF_GetNumBytesWritten( &buf ), BF_GetData( &buf ));
}

/*
=====================
CL_Disconnect

Goes from a connected state to full screen console state
Sends a disconnect message to the server
This is also called on Host_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect( void )
{
	if( cls.state == ca_disconnected )
		return;

	cls.connect_time = 0;
	CL_Stop_f();

	// send a disconnect message to the server
	CL_SendDisconnectMessage();
	CL_ClearState ();

	SCR_EndLoadingPlaque (); // get rid of loading plaque

	// clear the network channel, too.
	Netchan_Clear( &cls.netchan );

	// stop download
	if( cls.download )
	{
		FS_Close( cls.download );
		cls.download = NULL;
	}

	cls.state = ca_disconnected;

	// back to menu if developer mode set to "player" or "mapper"
	if( host.developer > 2 ) return;
	UI_SetActiveMenu( true );
}

void CL_Disconnect_f( void )
{
	Host_Error( "Disconnected from server\n" );
}

void CL_Crashed_f( void )
{
	// already freed
	if( host.state == HOST_CRASHED ) return;
	if( host.type != HOST_NORMAL ) return;
	if( !cls.initialized ) return;

	host.state = HOST_CRASHED;

	CL_Stop_f(); // stop any demos

	// send a disconnect message to the server
	CL_SendDisconnectMessage();

	// stop any downloads
	if( cls.download ) FS_Close( cls.download );

	Host_WriteOpenGLConfig();
	Host_WriteConfig();	// write config

	VID_RestoreGamma();
}

/*
=================
CL_LocalServers_f
=================
*/
void CL_LocalServers_f( void )
{
	netadr_t	adr;

	MsgDev( D_INFO, "Scanning for servers on the local network area...\n" );
	NET_Config( true ); // allow remote
	
	// send a broadcast packet
	adr.type = NA_BROADCAST;
	adr.port = BF_BigShort( PORT_SERVER );

	Netchan_OutOfBandPrint( NS_CLIENT, adr, "info %i", PROTOCOL_VERSION );
}

/*
====================
CL_Packet_f

packet <destination> <contents>

Contents allows \n escape character
====================
*/
void CL_Packet_f( void )
{
	char	send[2048];
	char	*in, *out;
	int	i, l;
	netadr_t	adr;

	if (Cmd_Argc() != 3)
	{
		Msg ("packet <destination> <contents>\n");
		return;
	}

	NET_Config( true ); // allow remote

	if( !NET_StringToAdr( Cmd_Argv( 1 ), &adr ))
	{
		Msg( "Bad address\n" );
		return;
	}

	if( !adr.port ) adr.port = BF_BigShort( PORT_SERVER );

	in = Cmd_Argv( 2 );
	out = send + 4;
	send[0] = send[1] = send[2] = send[3] = (char)0xff;

	l = com.strlen (in);
	for (i=0 ; i<l ; i++)
	{
		if (in[i] == '\\' && in[i+1] == 'n')
		{
			*out++ = '\n';
			i++;
		}
		else
			*out++ = in[i];
	}
	*out = 0;

	NET_SendPacket( NS_CLIENT, out - send, send, adr );
}

/*
=================
CL_Reconnect_f

The server is changing levels
=================
*/
void CL_Reconnect_f( void )
{
	// if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if( cls.download || cls.state == ca_disconnected )
		return;

	S_StopAllSounds ();

	cls.changelevel = true;

	if( cls.demoplayback ) return;

	if( cls.state == ca_connected )
	{
		cls.demonum = cls.movienum = -1;	// not in the demo loop now
		cls.state = ca_connected;

		// clear channel and stuff
		Netchan_Clear( &cls.netchan );
		BF_Clear( &cls.netchan.message );

		BF_WriteByte( &cls.netchan.message, clc_stringcmd );
		BF_WriteString( &cls.netchan.message, "new" );
		return;
	}

	if( cls.servername[0] )
	{
		if( cls.state >= ca_connected )
		{
			CL_Disconnect();
			cls.connect_time = host.realtime - 1.5;
		}
		else cls.connect_time = MAX_HEARTBEAT; // fire immediately

		cls.demonum = cls.movienum = -1;	// not in the demo loop now
		cls.state = ca_connecting;
		Msg( "reconnecting...\n" );
	}
}

/*
=================
CL_ParseStatusMessage

Handle a reply from a info
=================
*/
void CL_ParseStatusMessage( netadr_t from, sizebuf_t *msg )
{
	char	*s;

	s = BF_ReadString( msg );
	UI_AddServerToList( from, s );
}

//===================================================================

/*
======================
CL_PrepSound

Call before entering a new level, or after changing dlls
======================
*/
void CL_PrepSound( void )
{
	int	i, sndcount;

	MsgDev( D_NOTE, "CL_PrepSound: %s\n", clgame.mapname );
	for( i = 0, sndcount = 0; i < MAX_SOUNDS && cl.sound_precache[i+1][0]; i++ )
		sndcount++; // total num sounds

	S_BeginRegistration();
	for( i = 0; i < MAX_SOUNDS && cl.sound_precache[i+1][0]; i++ )
	{
		cl.sound_index[i+1] = S_RegisterSound( cl.sound_precache[i+1] );
		Cvar_SetFloat( "scr_loading", scr_loading->value + 5.0f / sndcount );
		if( cl_allow_levelshots->integer || host.developer > 3 ) SCR_UpdateScreen();
	}

	S_EndRegistration();

	if( host.soundList )
	{
		// need to reapply all ambient sounds after restarting
		for( i = 0; i < host.numsounds; i++)
		{
			soundlist_t *entry = &host.soundList[i];
			if( entry->looping && entry->entnum != -1 )
			{
				MsgDev( D_NOTE, "Restarting sound %s...\n", entry->name );
				S_AmbientSound( entry->origin, entry->entnum,
				S_RegisterSound( entry->name ), entry->volume, entry->attenuation,
				entry->pitch, 0 );
			}
		}
	}

	host.soundList = NULL; 
	host.numsounds = 0;

	cl.audio_prepped = true;
}

/*
=================
CL_PrepVideo

Call before entering a new level, or after changing dlls
=================
*/
void CL_PrepVideo( void )
{
	string	name, mapname;
	int	i, mdlcount;
	int	map_checksum; // dummy

	if( !cl.model_precache[1][0] )
		return; // no map loaded

	Cvar_SetFloat( "scr_loading", 0.0f ); // reset progress bar
	MsgDev( D_NOTE, "CL_PrepVideo: %s\n", clgame.mapname );

	// let the render dll load the map
	com.strncpy( mapname, cl.model_precache[1], MAX_STRING ); 
	Mod_LoadWorld( mapname, &map_checksum );
	cl.worldmodel = CM_ClipHandleToModel( 1 ); // get world pointer
	Cvar_SetFloat( "scr_loading", 25.0f );

	SCR_RegisterShaders(); // update with new sequence
	SCR_UpdateScreen();

	// make sure what map is valid
	if( map_checksum != cl.checksum )
		Host_Error( "Local map version differs from server: %i != '%i'\n", map_checksum, cl.checksum );

	for( i = 0, mdlcount = 0; i < MAX_MODELS && cl.model_precache[i+1][0]; i++ )
		mdlcount++; // total num models

	for( i = 0; i < MAX_MODELS && cl.model_precache[i+1][0]; i++ )
	{
		com.strncpy( name, cl.model_precache[i+1], MAX_STRING );
		Mod_RegisterModel( name, i+1 );
		Cvar_SetFloat( "scr_loading", scr_loading->value + 75.0f / mdlcount );
		if( cl_allow_levelshots->integer || host.developer > 3 )
			SCR_UpdateScreen();
	}

	// update right muzzleflash indexes
	CL_RegisterMuzzleFlashes ();

	// invalidate all decal indexes
	Mem_Set( cl.decal_index, 0, sizeof( cl.decal_index ));

	// release unused SpriteTextures
	for( i = 1; i < MAX_IMAGES; i++ )
	{
		if( !clgame.sprites[i].name[0] ) continue; // free slot
		if( clgame.sprites[i].needload != clgame.load_sequence )
			Mod_UnloadSpriteModel( &clgame.sprites[i] );
	}

	Mod_FreeUnused ();
	CL_ClearWorld ();

	R_NewMap();		// tell the render about new map

	Cvar_SetFloat( "scr_loading", 100.0f );	// all done

	if( host.decalList )
	{
		// need to reapply all decals after restarting
		for( i = 0; i < host.numdecals; i++ )
		{
			decallist_t *entry = &host.decalList[i];
			cl_entity_t *pEdict = CL_GetEntityByIndex( entry->entityIndex );
			shader_t decalIndex = CL_DecalIndex( CL_DecalIndexFromName( entry->name ));
			int modelIndex = 0;

			if( pEdict ) modelIndex = pEdict->curstate.modelindex;
			CL_DecalShoot( decalIndex, entry->entityIndex, modelIndex, entry->position, entry->flags );
		}
		Z_Free( host.decalList );
	}

	host.decalList = NULL; 
	host.numdecals = 0;

	if( host.soundList )
	{
		// need to reapply all ambient sounds after restarting
		for( i = 0; i < host.numsounds; i++ )
		{
			soundlist_t *entry = &host.soundList[i];
			if( entry->looping && entry->entnum != -1 )
			{
				MsgDev( D_NOTE, "Restarting sound %s...\n", entry->name );
				S_AmbientSound( entry->origin, entry->entnum,
				S_RegisterSound( entry->name ), entry->volume, entry->attenuation,
				entry->pitch, 0 );
			}
		}
	}

	host.soundList = NULL; 
	host.numsounds = 0;
	
	if( host.developer <= 2 )
		Con_ClearNotify(); // clear any lines of console text

	SCR_UpdateScreen ();

	cl.video_prepped = true;
	cl.force_refdef = true;
}

/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
void CL_ConnectionlessPacket( netadr_t from, sizebuf_t *msg )
{
	char	*s, *c;
	
	BF_Clear( msg );
	BF_ReadLong( msg ); // skip the -1

	s = BF_ReadStringLine( msg );

	Cmd_TokenizeString( s );
	c = Cmd_Argv( 0 );

	MsgDev( D_NOTE, "CL_ConnectionlessPacket: %s : %s\n", NET_AdrToString( from ), c );

	// server connection
	if( !com.strcmp( c, "client_connect" ))
	{
		if( cls.state == ca_connected )
		{
			MsgDev( D_INFO, "dup connect received. ignored\n");
			return;
		}

		Netchan_Setup( NS_CLIENT, &cls.netchan, from, Cvar_VariableValue( "net_qport" ));
		BF_WriteByte( &cls.netchan.message, clc_stringcmd );
		BF_WriteString( &cls.netchan.message, "new" );
		cls.state = ca_connected;

		cl.validsequence = 0;		// haven't gotten a valid frame update yet
		cl.delta_sequence = -1;		// we'll request a full delta from the baseline
		cls.lastoutgoingcommand = -1;		// we don't have a backed up cmd history yet
		cls.nextcmdtime = host.realtime;	// we can send a cmd right away

		UI_SetActiveMenu( false );
	}
	else if( !com.strcmp( c, "info" ))
	{
		// server responding to a status broadcast
		CL_ParseStatusMessage( from, msg );
	}
	else if( !com.strcmp( c, "cmd" ))
	{
		// remote command from gui front end
		if( !NET_IsLocalAddress( from ))
		{
			Msg( "Command packet from remote host.  Ignored.\n" );
			return;
		}

		ShowWindow( host.hWnd, SW_RESTORE );
		SetForegroundWindow ( host.hWnd );
		s = BF_ReadString( msg );
		Cbuf_AddText( s );
		Cbuf_AddText( "\n" );
	}
	else if( !com.strcmp( c, "print" ))
	{
		// print command from somewhere
		s = BF_ReadString( msg );
		Msg( s );
	}
	else if( !com.strcmp( c, "ping" ))
	{
		// ping from somewhere
		Netchan_OutOfBandPrint( NS_CLIENT, from, "ack" );
	}
	else if( !com.strcmp( c, "challenge" ))
	{
		// challenge from the server we are connecting to
		cls.challenge = com.atoi( Cmd_Argv( 1 ));
		CL_SendConnectPacket();
		return;
	}
	else if( !com.strcmp( c, "echo" ))
	{
		// echo request from server
		Netchan_OutOfBandPrint( NS_CLIENT, from, "%s", Cmd_Argv( 1 ));
	}
	else if( !com.strcmp( c, "disconnect" ))
	{
		// a disconnect message from the server, which will happen if the server
		// dropped the connection but it is still getting packets from us
		CL_Disconnect();
	}
	else MsgDev( D_ERROR, "bad connectionless packet from %s:\n%s\n", NET_AdrToString( from ), s );
}

/*
=================
CL_ReadNetMessage
=================
*/
void CL_ReadNetMessage( void )
{
	int	curSize;

	while( NET_GetPacket( NS_CLIENT, &net_from, net_message_buffer, &curSize ))
	{
		if( host.type == HOST_DEDICATED || cls.demoplayback )
			return;

		BF_Init( &net_message, "ServerData", net_message_buffer, curSize );

		// check for connectionless packet (0xffffffff) first
		if( BF_GetMaxBytes( &net_message ) >= 4 && *(int *)net_message.pData == -1 )
		{
			CL_ConnectionlessPacket( net_from, &net_message );
			continue;
		}

		// can't be a valid sequenced packet	
		if( cls.state < ca_connected ) continue;

		if( BF_GetMaxBytes( &net_message ) < 8 )
		{
			MsgDev( D_WARN, "%s: runt packet\n", NET_AdrToString( net_from ));
			continue;
		}

		// packet from server
		if( !NET_CompareAdr( net_from, cls.netchan.remote_address ))
		{
			MsgDev( D_WARN, "CL_ReadPackets: %s:sequenced packet without connection\n", NET_AdrToString( net_from ));
			continue;
		}

		if( Netchan_Process( &cls.netchan, &net_message ))
		{
			// the header is different lengths for reliable and unreliable messages
			int headerBytes = BF_GetNumBytesRead( &net_message );

			CL_ParseServerMessage( &net_message );

			// we don't know if it is ok to save a demo message until
			// after we have parsed the frame
			if( cls.demorecording && !cls.demowaiting )
				CL_WriteDemoMessage( &net_message, headerBytes );
		}
	}

	// check for fragmentation/reassembly related packets.
	if( cls.state != ca_disconnected && Netchan_IncomingReady( &cls.netchan ))
	{
		// the header is different lengths for reliable and unreliable messages
		int headerBytes = BF_GetNumBytesRead( &net_message );

		// process the incoming buffer(s)
		if( Netchan_CopyNormalFragments( &cls.netchan, &net_message ))
		{
			CL_ParseServerMessage( &net_message );

			// we don't know if it is ok to save a demo message until
			// after we have parsed the frame
			if( cls.demorecording && !cls.demowaiting )
				CL_WriteDemoMessage( &net_message, headerBytes );
		}
		
		if( Netchan_CopyFileFragments( &cls.netchan, &net_message ))
		{
			// Remove from resource request stuff.
//			CL_ProcessFile( true, cls.netchan.incomingfilename );
		}
	}

	Netchan_UpdateProgress( &cls.netchan );
}

void CL_ReadPackets( void )
{
	if( cls.demoplayback )
		CL_ReadDemoMessage();
	else CL_ReadNetMessage();

	cl.lerpFrac = CL_LerpPoint();
	cl.thirdperson = clgame.dllFuncs.CL_IsThirdPerson();

	// build list of all solid entities per frame (exclude clients)
	CL_SetSolidEntities ();

	// singleplayer never has connection timeout
	if( NET_IsLocalAddress( cls.netchan.remote_address ))
		return;
          
	// check timeout
	if( cls.state >= ca_connected && !cls.demoplayback )
	{
		if( host.realtime - cls.netchan.last_received > cl_timeout->value )
		{
			if( ++cl.timeoutcount > 5 ) // timeoutcount saves debugger
			{
				Msg( "\nServer connection timed out.\n" );
				CL_Disconnect();
				return;
			}
		}
	}
	else cl.timeoutcount = 0;
	
}


//=============================================================================
/*
==============
CL_Userinfo_f
==============
*/
void CL_Userinfo_f( void )
{
	Msg( "User info settings:\n" );
	Info_Print( Cvar_Userinfo( ));
}

/*
==============
CL_Physinfo_f
==============
*/
void CL_Physinfo_f( void )
{
	Msg( "Phys info settings:\n" );
	Info_Print( cl.frame.local.client.physinfo );
}

void CL_RequestNextDownload( void )
{
	Host_Error( "CL_RequestNextDownload: not implemented (yet)\n" );
}

/*
=================
CL_Precache_f

The server will send this command right
before allowing the client into the server
=================
*/
void CL_Precache_f( void )
{
	int	spawncount;

	spawncount = com.atoi( Cmd_Argv( 1 ));

	CL_PrepSound();
	CL_PrepVideo();

	if( cls.demoplayback ) return; // not really connected

	BF_WriteByte( &cls.netchan.message, clc_stringcmd );
	BF_WriteString( &cls.netchan.message, va( "begin %i\n", spawncount ));
}

/*
=================
CL_Escape_f

Escape to menu from game
=================
*/
void CL_Escape_f( void )
{
	if( cls.key_dest == key_menu )
		return;

	if( cls.state == ca_cinematic )
		SCR_StopCinematic();
	else UI_SetActiveMenu( true );
}

/*
=================
CL_InitLocal
=================
*/
void CL_InitLocal( void )
{
	cls.state = ca_disconnected;

	// register our variables
	cl_predict = Cvar_Get( "cl_predict", "1", CVAR_ARCHIVE, "disables client movement prediction" );
	cl_crosshair = Cvar_Get( "crosshair", "1", CVAR_ARCHIVE, "show weapon chrosshair" );
	cl_nodelta = Cvar_Get ("cl_nodelta", "0", 0, "disable delta-compression for usercommnds" );
	cl_idealpitchscale = Cvar_Get( "cl_idealpitchscale", "0.8", 0, "how much to look up/down slopes and stairs when not using freelook" );
	cl_solid_players = Cvar_Get( "cl_solid_players", "1", 0, "Make all players not solid (can't traceline them)" );
	
	cl_showmiss = Cvar_Get( "cl_showmiss", "0", CVAR_ARCHIVE, "client show prediction errors" );
	cl_timeout = Cvar_Get( "cl_timeout", "60", 0, "connect timeout (in-seconds)" );

	rcon_client_password = Cvar_Get( "rcon_password", "", 0, "remote control client password" );
	rcon_address = Cvar_Get( "rcon_address", "", 0, "remote control address" );

	// userinfo
	Cvar_Get( "password", "", CVAR_USERINFO, "player password" );
	name = Cvar_Get( "name", SI->username, CVAR_USERINFO|CVAR_ARCHIVE|CVAR_PRINTABLEONLY, "player name" );
	model = Cvar_Get( "model", "player", CVAR_USERINFO|CVAR_ARCHIVE, "player model ('player' it's a single player model)" );
	topcolor = Cvar_Get( "topcolor", "0", CVAR_USERINFO|CVAR_ARCHIVE, "player top color" );
	bottomcolor = Cvar_Get( "bottomcolor", "0", CVAR_USERINFO|CVAR_ARCHIVE, "player bottom color" );
	rate = Cvar_Get( "rate", "25000", CVAR_USERINFO|CVAR_ARCHIVE, "player network rate" );
	userinfo = Cvar_Get( "@userinfo", "0", CVAR_READ_ONLY, "" ); // use ->modified value only
	cl_showfps = Cvar_Get( "cl_showfps", "1", CVAR_ARCHIVE, "show client fps" );
	cl_smooth = Cvar_Get ("cl_smooth", "0", CVAR_ARCHIVE, "smooth up stair climbing and interpolate position in multiplayer" );
	cl_cmdbackup = Cvar_Get( "cl_cmdbackup", "2", CVAR_ARCHIVE, "how many additional history commands are sent" );
	cl_cmdrate = Cvar_Get( "cl_cmdrate", "30", CVAR_ARCHIVE, "Max number of command packets sent to server per second" );
	cl_draw_particles = Cvar_Get( "cl_draw_particles", "1", CVAR_ARCHIVE, "Disable any particle effects" );
	cl_draw_beams = Cvar_Get( "cl_draw_beams", "1", CVAR_ARCHIVE, "Disable view beams" );
	cl_lightstyle_lerping = Cvar_Get( "cl_lightstyle_lerping", "0", CVAR_ARCHIVE, "enables animated light lerping (perfomance option)" );

	Cvar_Get( "hud_scale", "0", CVAR_ARCHIVE|CVAR_LATCH, "scale hud at current resolution" );
	Cvar_Get( "skin", "", CVAR_USERINFO, "player skin" ); // XDM 3.3 want this cvar

	// server commands
	Cmd_AddCommand ("noclip", NULL, "enable or disable no clipping mode" );
	Cmd_AddCommand ("notarget", NULL, "notarget mode (monsters do not see you)" );
	Cmd_AddCommand ("fullupdate", NULL, "re-init HUD on start demo recording" );
	Cmd_AddCommand ("give", NULL, "give specified item or weapon" );
	Cmd_AddCommand ("drop", NULL, "drop current/specified item or weapon" );
	Cmd_AddCommand ("intermission", NULL, "go to intermission" );
	Cmd_AddCommand ("spectate", NULL, "enable spectator mode" );
	Cmd_AddCommand ("gametitle", NULL, "show game logo" );
	Cmd_AddCommand ("god", NULL, "enable godmode" );
	Cmd_AddCommand ("fov", NULL, "set client field of view" );
		
	// register our commands
	Cmd_AddCommand ("cmd", CL_ForwardToServer_f, "send a console commandline to the server" );
	Cmd_AddCommand ("pause", NULL, "pause the game (if the server allows pausing)" );
	Cmd_AddCommand ("localservers", CL_LocalServers_f, "collect info about local servers" );
	Cmd_AddCommand ("cd", CL_PlayCDTrack_f, "Play cd-track (not real cd-player of course)" );

	Cmd_AddCommand ("@crashed",  CL_Crashed_f, "" );	// internal system command
	Cmd_AddCommand ("userinfo", CL_Userinfo_f, "print current client userinfo" );
	Cmd_AddCommand ("physinfo", CL_Physinfo_f, "print current client physinfo" );
	Cmd_AddCommand ("disconnect", CL_Disconnect_f, "disconnect from server" );
	Cmd_AddCommand ("record", CL_Record_f, "record a demo" );
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f, "playing a demo" );
	Cmd_AddCommand ("killdemo", CL_DeleteDemo_f, "delete a specified demo file and demoshot" );
	Cmd_AddCommand ("startdemos", CL_StartDemos_f, "start playing back the selected demos sequentially" );
	Cmd_AddCommand ("demos", CL_Demos_f, "restart looping demos defined by the last startdemos command" );
	Cmd_AddCommand ("movie", CL_PlayVideo_f, "playing a movie" );
	Cmd_AddCommand ("stop", CL_Stop_f, "stop playing or recording a demo" );
	Cmd_AddCommand ("info", NULL, "collect info about local servers with specified protocol" );
	Cmd_AddCommand ("escape", CL_Escape_f, "escape from game to menu" );
	
	Cmd_AddCommand ("quit", CL_Quit_f, "quit from game" );
	Cmd_AddCommand ("exit", CL_Quit_f, "quit from game" );

	Cmd_AddCommand ("screenshot", CL_ScreenShot_f, "takes a screenshot of the next rendered frame" );
	Cmd_AddCommand ("envshot", CL_EnvShot_f, "takes a six-sides cubemap shot with specified name" );
	Cmd_AddCommand ("skyshot", CL_SkyShot_f, "takes a six-sides envmap (skybox) shot with specified name" );
	Cmd_AddCommand ("levelshot", CL_LevelShot_f, "same as \"screenshot\", used for create plaque images" );
	Cmd_AddCommand ("saveshot", CL_SaveShot_f, "used for create save previews with LoadGame menu" );
	Cmd_AddCommand ("demoshot", CL_DemoShot_f, "used for create demo previews with PlayDemo menu" );

	Cmd_AddCommand ("connect", CL_Connect_f, "connect to a server by hostname" );
	Cmd_AddCommand ("reconnect", CL_Reconnect_f, "reconnect to current level" );

	Cmd_AddCommand ("rcon", CL_Rcon_f, "sends a command to the server console (rcon_password and rcon_address required)" );

	// this is dangerous to leave in
// 	Cmd_AddCommand ("packet", CL_Packet_f, "send a packet with custom contents" );

	Cmd_AddCommand ("precache", CL_Precache_f, "precache specified resource (by index)" );
	Cmd_AddCommand ("download", CL_Download_f, "download specified resource (by name)" );
}

//============================================================================

/*
==================
CL_SendCommand

==================
*/
void CL_SendCommand( void )
{
	// send intentions now
	CL_SendCmd ();

	// resend a connection request if necessary
	CL_CheckForResend ();
}

/*
==================
Host_ClientFrame

==================
*/
void Host_ClientFrame( void )
{
	// if client is not active, do nothing
	if( !cls.initialized ) return;

	// decide the simulation time
	cl.oldtime = cl.time;
	cl.time += host.frametime;

	// menu time (not paused, not clamped)
	menu.globals->time = host.realtime;
	menu.globals->frametime = host.realframetime;
	menu.globals->demoplayback = cls.demoplayback;
	menu.globals->demorecording = cls.demorecording;

	// if in the debugger last frame, don't timeout
	if( host.frametime > 5.0f ) cls.netchan.last_received = Sys_DoubleTime();

	// fetch results from server
	CL_ReadPackets();

	// send a new command message to the server
	CL_SendCommand();

	// predict all unacknowledged movements
	CL_PredictMovement();

	VID_CheckChanges();

	// allow sound and video DLL change
	if( cls.state == ca_active )
	{
		if( !cl.video_prepped ) CL_PrepVideo();
		if( !cl.audio_prepped ) CL_PrepSound();
	}

	// update the screen
	SCR_UpdateScreen ();

	// update audio
	S_RenderFrame( &cl.refdef );

	// decay dynamic lights
	CL_DecayLights ();

	SCR_RunCinematic();
	Con_RunConsole();

	cls.framecount++;
}


//============================================================================

/*
====================
CL_Init
====================
*/
void CL_Init( void )
{
	if( host.type == HOST_DEDICATED )
		return; // nothing running on the client

	Con_Init();	
	CL_InitLocal();

	R_Init();	// init renderer
	S_Init();	// init sound

	if( !CL_LoadProgs( va( "%s/client.dll", GI->dll_path )))
		Host_Error( "CL_InitGame: can't initialize client.dll\n" );

	cls.initialized = true;

	// g-cont. disable for now
	Cvar_SetFloat( "cl_lw", 0 );
	Cvar_SetFloat( "cl_predict", 0 );
}


/*
===============
CL_Shutdown

FIXME: this is a callback from Sys_Quit and Host_Error. It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void CL_Shutdown( void )
{
	// already freed
	if( host.state == HOST_ERROR ) return;
	if( !cls.initialized ) return;

	Host_WriteOpenGLConfig ();
	Host_WriteConfig (); 

	IN_Shutdown ();
	SCR_Shutdown ();
	CL_UnloadProgs ();
	cls.initialized = false;
}