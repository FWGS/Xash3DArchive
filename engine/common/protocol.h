/*
protocol.h - communications protocols
Copyright (C) 2007 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PROTOCOL_VERSION		42

// server to client
#define svc_bad			0	// immediately crash client when received
#define svc_nop			1	// does nothing
#define svc_disconnect		2	// kick client from server
#define svc_changing		3	// changelevel by server request
#define svc_version			4	// [long] server version
#define svc_setview			5	// [short] entity number
#define svc_sound			6	// <see code>
#define svc_time			7	// [float] server time
#define svc_print			8	// [byte] id [string] null terminated string
#define svc_stufftext		9	// [string] stuffed into client's console buffer
#define svc_setangle		10	// [angle angle angle] set the view angle to this absolute value
#define svc_serverdata		11	// [long] protocol ...
#define svc_lightstyle		12	// [index][pattern]
#define svc_updateuserinfo		13	// [byte] playernum, [string] userinfo
#define svc_deltatable		14	// [table header][...]
#define svc_clientdata		15	// [...]
#define svc_stopsound		16	// <see code>
#define svc_updatepings		17	// [bit][idx][ping][packet_loss]
#define svc_particle		18	// [float*3][char*3][byte][byte]
#define svc_frame			19	// <OBSOLETE>
#define svc_spawnstatic		20	// creates a static client entity
#define svc_event_reliable		21	// playback event directly from message, not queue
#define svc_spawnbaseline		22	// <see code>
#define svc_temp_entity		23	// <variable sized>
#define svc_setpause		24	// [byte] 0 = unpaused, 1 = paused
#define svc_signonnum		25	// [byte] used for the signon sequence
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
#define svc_resourcelist		43	// [short][...]
#define svc_deltamovevars		44	// [movevars_t]
#define svc_customization		45
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
#define clc_userinfo		6	// [[userinfo string]
#define clc_fileconsistency		7
#define clc_voicedata		8

#define MAX_VISIBLE_PACKET		512	// 512 visible entities per frame (hl1 has 256)

// additional protocol data
#define MAX_CLIENT_BITS		5
#define MAX_CLIENTS			(1<<MAX_CLIENT_BITS)// 5 bits == 32 clients ( int32 limit )

#define MAX_WEAPON_BITS		5
#define MAX_WEAPONS			(1<<MAX_WEAPON_BITS)// 5 bits == 32 weapons ( int32 limit )

#define MAX_EVENT_BITS		10
#define MAX_EVENTS			(1<<MAX_EVENT_BITS)	// 10 bits == 1024 events (the original Half-Life limit)

#define MAX_MODEL_BITS		11
#define MAX_MODELS			(1<<MAX_MODEL_BITS)	// 11 bits == 2048 models

#define MAX_SOUND_BITS		11
#define MAX_SOUNDS			(1<<MAX_SOUND_BITS)	// 11 bits == 2048 sounds

#define MAX_CUSTOM			1024	// max custom resources per level

#define MAX_USER_MESSAGES		191	// another 63 messages reserved for engine routines

#define MAX_DLIGHTS			32	// dynamic lights (rendered per one frame)
#define MAX_ELIGHTS			64	// entity only point lights
#define MAX_LIGHTSTYLES		256	// a byte limit, don't modify
#define MAX_EDICTS			4096	// absolute limit, should be enough. (can be up to 32768)
#define MAX_RENDER_DECALS		4096	// max rendering decals per a level

// sound flags
#define SND_VOLUME			(1<<0)	// a scaled byte
#define SND_ATTENUATION		(1<<1)	// a byte
#define SND_LARGE_INDEX		(1<<2)	// a send sound as short
#define SND_PITCH			(1<<3)	// a byte
#define SND_SENTENCE		(1<<4)	// set if sound num is actually a sentence num
#define SND_STOP			(1<<5)	// stop the sound
#define SND_CHANGE_VOL		(1<<6)	// change sound vol
#define SND_CHANGE_PITCH		(1<<7)	// change sound pitch
#define SND_SPAWNING		(1<<8)	// we're spawning, used in some cases for ambients

// decal flags
#define FDECAL_PERMANENT		0x01	// This decal should not be removed in favor of any new decals
#define FDECAL_CUSTOM		0x02	// This is a custom clan logo and should not be saved/restored
#define FDECAL_DONTSAVE		0x04	// Decal was loaded from adjacent level, don't save it for this level
#define FDECAL_CLIPTEST		0x08	// Decal needs to be clip-tested
#define FDECAL_NOCLIP		0x10	// Decal is not clipped by containing polygon
#define FDECAL_USESAXIS		0x20	// Uses the s axis field to determine orientation (footprints)

// Max number of history commands to send ( 2 by default ) in case of dropped packets
#define NUM_BACKUP_COMMAND_BITS	4
#define MAX_BACKUP_COMMANDS		(1 << NUM_BACKUP_COMMAND_BITS)

// world size
#define MAX_COORD_INTEGER		(16384)	// world half-size, modify with precaution
#define MIN_COORD_INTEGER		(-MAX_COORD_INTEGER)
#define MAX_COORD_FRACTION		( 1.0 - ( 1.0 / 16.0 ))
#define MIN_COORD_FRACTION		(-1.0 + ( 1.0 / 16.0 ))

// network precision
#define COORD_INTEGER_BITS		14
#define COORD_FRACTIONAL_BITS		5
#define COORD_DENOMINATOR		( 1 << ( COORD_FRACTIONAL_BITS ))
#define COORD_RESOLUTION		(1.0 / ( COORD_DENOMINATOR ))

#define NORMAL_FRACTIONAL_BITS	11
#define NORMAL_DENOMINATOR		(( 1 << ( NORMAL_FRACTIONAL_BITS )) - 1 )
#define NORMAL_RESOLUTION		( 1.0 / ( NORMAL_DENOMINATOR ))

// verify that coordsize.h and worldsize.h are consistently defined
#if( MAX_COORD_INTEGER != ( 1 << COORD_INTEGER_BITS ))
#error MAX_COORD_INTEGER does not match COORD_INTEGER_BITS
#endif

#define MAX_RESOURCES	(MAX_MODELS+MAX_SOUNDS+MAX_CUSTOM+MAX_EVENTS)

typedef struct
{
	int	rescount;
	int	restype[MAX_RESOURCES];
	char	resnames[MAX_RESOURCES][CS_SIZE];
} resourcelist_t;

#endif//PROTOCOL_H