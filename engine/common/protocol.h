//=======================================================================
//			Copyright XashXT Group 2007 ©
//		      protocol.h - communications protocols
//=======================================================================
#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PROTOCOL_VERSION	39

// server to client
#define svc_bad			0	// immediately crash client when received
#define svc_nop			1	// does nothing
#define svc_disconnect		2	// kick client from server
#define svc_changing		3	// changelevel by server request
#define svc_configstring		4	// [short] [string]
#define svc_setview			5	// [short] entity number
#define svc_sound			6	// <see code>
#define svc_time			7	// [float] server time
#define svc_print			8	// [byte] id [string] null terminated string
#define svc_stufftext		9	// [string] stuffed into client's console buffer
#define svc_setangle		10	// [angle angle] set the view angle to this absolute value
#define svc_serverdata		11	// [long] protocol ...
#define svc_restore			12	// restore saved game on the client
#define svc_frame			13	// begin a new server frame
#define svc_usermessage		14	// [string][byte] REG_USER_MSG stuff
#define svc_clientdata		15	// [...]
#define svc_download		16	// [short] size [size bytes]
#define svc_updatepings		17	// [bit][idx][ping][packet_loss]
#define svc_particle		18	// [float*3][char*3][byte][byte]
#define svc_ambientsound		19	// <see code>
#define svc_spawnstatic		20	// NOT IMPLEMENTED
#define svc_crosshairangle		21	// [short][short][short]
#define svc_spawnbaseline		22	// <see code>
#define svc_temp_entity		23	// <variable sized>
#define svc_setpause		24	// [byte] 0 = unpaused, 1 = paused
#define svc_deltamovevars		25	// [movevars_t]
#define svc_centerprint		26	// [string] to put in center of the screen
#define svc_event			27	// playback event queue
#define svc_event_reliable		28	// playback event directly from message, not queue
#define svc_updateuserinfo		29	// [byte] playernum, [string] userinfo
#define svc_intermission		30	// empty message (event)
#define svc_soundfade		31	// [float*4] sound fade parms
#define svc_cdtrack			32	// [byte] track [byte] looptrack
#define svc_serverinfo		33	// [string] key [string] value
#define svc_deltatable		34	// [table header][...]
#define svc_weaponanim		35	// [byte]iAnim [byte]body
#define svc_bspdecal		36	// [float*3][short][short][short]
#define svc_roomtype		37	// [short] room type
#define svc_addangle		38	// [angle] add angles when client turn on mover

#define svc_packetentities		40	// [short][...]
#define svc_deltapacketentities	41	// [short][byte][...] 
#define svc_chokecount		42	// [byte]

#define svc_director		51	// <variable sized>
#define svc_lastmsg			64	// start user messages at this point

// client to server
#define clc_bad			0	// immediately drop client when received
#define clc_nop			1 		
#define clc_move			2	// [[usercmd_t]
#define clc_stringcmd		3	// [string] message
#define clc_delta			4	// [byte] sequence number, requests delta compression of message
#define clc_resourcelist		5
#define clc_userinfo		6	// [[userinfo string]	// FIXME: remove
#define clc_fileconsistency		7
#define clc_voicedata		8
#define clc_random_seed		9	// [long] random seed	// FIXME: remove

// additional protocol data
#define MAX_CLIENT_BITS		5
#define MAX_CLIENTS			(1<<MAX_CLIENT_BITS)

#define MAX_WEAPON_BITS		5
#define MAX_WEAPONS			(1<<MAX_WEAPON_BITS)

// Max number of history commands to send ( 2 by default ) in case of dropped packets
#define NUM_BACKUP_COMMAND_BITS	3
#define MAX_BACKUP_COMMANDS		(1 << NUM_BACKUP_COMMAND_BITS)

/*
==============================================================

DELTA-PACKET ENTITIES

==============================================================
*/

typedef struct
{
	entity_state_t	*entities;
	int		max_entities;	// this is a real allocated space
	int		num_entities;
} packet_entities_t;

#endif//PROTOCOL_H