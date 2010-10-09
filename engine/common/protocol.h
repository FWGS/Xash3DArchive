//=======================================================================
//			Copyright XashXT Group 2007 ©
//		      protocol.h - communications protocols
//=======================================================================
#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PROTOCOL_VERSION	39

// server to client
#define svc_bad		0	// immediately crash client when received
#define svc_nop		1	// end of user messages
#define svc_disconnect	2	// kick client from server
#define svc_changing	3	// changelevel by server request
#define svc_configstring	4	// [short] [string]
#define svc_setview		5	// [short] entity number
#define svc_sound		6	// <see code>
#define svc_time		7	// [float] server time
#define svc_print		8	// [byte] id [string] null terminated string
#define svc_stufftext	9	// [string] stuffed into client's console buffer, should be \n terminated
#define svc_setangle	10	// [angle angle] set the view angle to this absolute value
#define svc_serverdata	11	// [long] protocol ...
#define svc_addangle	12	// [angle] add angles when client turn on mover
#define svc_frame		13	// begin a new server frame
#define svc_clientdata	14	// [...]
#define svc_packetentities	15	// [...]
#define svc_download	16	// [short] size [size bytes]
#define svc_usermessage	17	// [string][byte] REG_USER_MSG stuff
#define svc_particle	18	// [float*3][char*3][byte][byte]
#define svc_ambientsound	19	// <see code>
#define svc_spawnstatic	20	// NOT IMPLEMENTED
#define svc_crosshairangle	21	// [short][short][short]
#define svc_spawnbaseline	22	// <see code>
#define svc_temp_entity	23	// <variable sized>
#define svc_setpause	24	// [byte] 0 = unpaused, 1 = paused
#define svc_deltamovevars	25	// [movevars_t]
#define svc_centerprint	26	// [string] to put in center of the screen
#define svc_event		27	// playback event queue
#define svc_event_reliable	28	// playback event directly from message, not queue
#define svc_updateuserinfo	29	// [byte] playernum, [string] userinfo
#define svc_intermission	30	// empty message (event)
#define svc_soundfade	31	// [float*4] sound fade parms
#define svc_cdtrack		32	// [byte] track [byte] looptrack
#define svc_serverinfo	33	// [string] key [string] value
#define svc_deltatable	34	// [table header][...]
#define svc_weaponanim	35	// [byte]iAnim [byte]body
#define svc_bspdecal	36	// [float*3][short][short][short]
#define svc_roomtype	37	// [short] room type
#define svc_restore		38	// restore saved game on the client

#define svc_director	51	// <variable sized>
#define svc_lastmsg		64	// start user messages at this point

// client to server
#define clc_bad		0	// immediately drop client when received
#define clc_nop		1 		
#define clc_move		2	// [[usercmd_t]
#define clc_delta		3	// [byte] sequence number, requests delta compression of message
#define clc_userinfo	4	// [[userinfo string]
#define clc_stringcmd	5	// [string] message
#define clc_random_seed	6	// [long] random seed

// additional protocol data
#define MAX_CLIENT_BITS	5
#define MAX_CLIENTS		(1<<MAX_CLIENT_BITS)

#endif//PROTOCOL_H