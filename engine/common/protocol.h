//=======================================================================
//			Copyright XashXT Group 2007 ©
//		      protocol.h - communications protocols
//=======================================================================
#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PROTOCOL_VERSION	38

// server to client
#define svc_bad		0	// immediately crash client when received
				// space 1 - 200 it's reserved for user messages
#define svc_nop		201	// end of user messages
#define svc_disconnect	202	// kick client from server
#define svc_reconnect	203	// reconnecting server request
#define svc_stufftext	204	// [string] stuffed into client's console buffer, should be \n terminated
#define svc_serverdata	205	// [long] protocol ...
#define svc_configstring	206	// [short] [string]
#define svc_spawnbaseline	207	// valid only at spawn		
#define svc_download	208	// [short] size [size bytes]
#define svc_changing	209	// changelevel by server request
#define svc_physinfo	210	// [physinfo string]
#define svc_usermessage	211	// [string][byte] REG_USER_MSG stuff
#define svc_packetentities	212	// [...]
#define svc_frame		213	// begin a new server frame
#define svc_sound		214	// <see code>
#define svc_ambientsound	215	// <see code>
#define svc_setangle	216	// [float float] set the view angle to this absolute value
#define svc_addangle	217	// [float] add angles when client turn on mover
#define svc_setview		218	// [short] entity number
#define svc_print		219	// [byte] id [string] null terminated string
#define svc_centerprint	220	// [string] to put in center of the screen
#define svc_crosshairangle	221	// [short][short][short]
#define svc_setpause	222	// [byte] 0 = unpaused, 1 = paused
#define svc_movevars	223	// [movevars_t]
#define svc_particle	224	// [float*3][char*3][byte][byte]
#define svc_soundfade	225	// [float*4] sound fade parms
#define svc_bspdecal	226	// [float*3][short][short][short]
#define svc_event		227	// playback event queue
#define svc_event_reliable	228	// playback event directly from message, not queue
#define svc_updateuserinfo	229	// [byte] playernum, [string] userinfo
#define svc_serverinfo	230	// [string] key [string] value
#define svc_clientdata	231	// [...]

// client to server
#define clc_bad		0	// immediately drop client when received
#define clc_nop		1 		
#define clc_move		2	// [[usercmd_t]
#define clc_delta		3	// [byte] sequence number, requests delta compression of message
#define clc_userinfo	4	// [[userinfo string]
#define clc_stringcmd	5	// [string] message
#define clc_random_seed	6	// [long] random seed

#define msg_end		255	// shared marker for end the message

#endif//PROTOCOL_H