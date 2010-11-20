//=======================================================================
//			Copyright XashXT Group 2007 �
//		      protocol.h - communications protocols
//=======================================================================
#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PROTOCOL_VERSION		40

// server to client
#define svc_bad			0	// immediately crash client when received
#define svc_nop			1	// does nothing
#define svc_disconnect		2	// kick client from server
#define svc_changing		3	// changelevel by server request

#define svc_setview			5	// [short] entity number
#define svc_sound			6	// <see code>
#define svc_time			7	// [float] server time
#define svc_print			8	// [byte] id [string] null terminated string
#define svc_stufftext		9	// [string] stuffed into client's console buffer
#define svc_setangle		10	// [angle angle] set the view angle to this absolute value
#define svc_serverdata		11	// [long] protocol ...
#define svc_lightstyle		12	// [index][pattern]
#define svc_updateuserinfo		13	// [byte] playernum, [string] userinfo
#define svc_deltatable		14	// [table header][...]
#define svc_clientdata		15	// [...]
#define svc_download		16	// [short] size [size bytes]
#define svc_updatepings		17	// [bit][idx][ping][packet_loss]
#define svc_particle		18	// [float*3][char*3][byte][byte]
#define svc_frame			19	// <OBSOLETE>
#define svc_spawnstatic		20	// creates a static client entity
#define svc_event_reliable		21	// playback event directly from message, not queue
#define svc_spawnbaseline		22	// <see code>
#define svc_temp_entity		23	// <variable sized>
#define svc_setpause		24	// [byte] 0 = unpaused, 1 = paused

#define svc_centerprint		26	// [string] to put in center of the screen
#define svc_event			27	// playback event queue
#define svc_soundindex		28	// [index][soundpath]
#define svc_ambientsound		29	// <see code>
#define svc_intermission		30	// empty message (event)
#define svc_modelindex		31	// [index][modelpath]
#define svc_cdtrack			32	// [string] trackname
#define svc_serverinfo		33	// [string] key [string] value
#define svc_eventindex		34	// [index][eventname]
#define svc_weaponanim		35	// [byte]iAnim [byte]body
#define svc_bspdecal		36	// [float*3][short][short][short]
#define svc_roomtype		37	// [short] room type
#define svc_addangle		38	// [angle] add angles when client turn on mover
#define svc_usermessage		39	// [byte][byte][string] REG_USER_MSG stuff
#define svc_packetentities		40	// [short][...]
#define svc_deltapacketentities	41	// [short][byte][...] 
#define svc_chokecount		42	// [byte]

#define svc_deltamovevars		44	// [movevars_t]

#define svc_crosshairangle		47	// [byte][byte]
#define svc_soundfade		48	// [float*4] sound fade parms

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
#define MAX_CLIENTS			(1<<MAX_CLIENT_BITS)// 5 bits == 32 clients ( int32 limit )

#define MAX_WEAPON_BITS		5
#define MAX_WEAPONS			(1<<MAX_WEAPON_BITS)// 5 bits == 32 weapons ( int32 limit )

#define MAX_EVENT_BITS		10
#define MAX_EVENTS			(1<<MAX_EVENT_BITS)	// 10 bits == 1024 events (the original Half-Life limit)

#define MAX_CUSTOM			1024	// max custom resources per level

#define MAX_USER_MESSAGES		191	// another 63 messages reserved for engine routines
					// FIXME: tune this
// sound flags
#define SND_VOLUME			(1<<0)	// a scaled byte
#define SND_ATTENUATION		(1<<1)	// a byte
#define SND_PITCH			(1<<2)	// a byte
#define SND_FIXED_ORIGIN		(1<<3)	// a vector
#define SND_SENTENCE		(1<<4)	// set if sound num is actually a sentence num
#define SND_STOP			(1<<5)	// stop the sound
#define SND_CHANGE_VOL		(1<<6)	// change sound vol
#define SND_CHANGE_PITCH		(1<<7)	// change sound pitch
#define SND_SPAWNING		(1<<8)	// we're spawning, used in some cases for ambients

// Max number of history commands to send ( 2 by default ) in case of dropped packets
#define NUM_BACKUP_COMMAND_BITS	3
#define MAX_BACKUP_COMMANDS		(1 << NUM_BACKUP_COMMAND_BITS)

#endif//PROTOCOL_H