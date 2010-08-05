//=======================================================================
//			Copyright XashXT Group 2009 ©
//	      cl_parse.c  -- parse a message received from the server
//=======================================================================

#include "common.h"
#include "client.h"
#include "protocol.h"
#include "net_sound.h"
#include "net_encode.h"

#define MSG_COUNT		32		// last 32 messages parsed
#define MSG_MASK		(MSG_COUNT - 1)

int CL_UPDATE_BACKUP = SINGLEPLAYER_BACKUP;

char *svc_strings[256] =
{
	"svc_bad",
// user messages space
	"svc_nop",
	"svc_disconnect",
	"svc_reconnect",
	"svc_stufftext",
	"svc_serverdata",
	"svc_configstring",
	"svc_spawnbaseline",
	"svc_download",
	"svc_changing",
	"svc_deltatable",
	"svc_usermessage",
	"svc_packetentities",
	"svc_frame",
	"svc_sound",
	"svc_ambientsound",
	"svc_setangle",
	"svc_addangle",
	"svc_setview",
	"svc_print",
	"svc_centerprint",
	"svc_crosshairangle",
	"svc_setpause",
	"svc_movevars",
	"svc_particle",
	"svc_soundfade",
	"svc_bspdecal",
	"svc_event",
	"svc_event_reliable",
	"svc_updateuserinfo",
	"svc_serverinfo",
};

typedef struct
{
	int	command;
	int	starting_offset;
	int	frame_number;
} oldcmd_t;

typedef struct
{
	oldcmd_t	oldcmd[MSG_COUNT];   
	int	currentcmd;
	bool	parsing;
} msg_debug_t;

static msg_debug_t	cls_message_debug;
static int	starting_count;

const char *CL_MsgInfo( int cmd )
{
	static string	sz;

	com.strcpy( sz, "???" );

	if( cmd > 200 && cmd < 256 )
	{
		// get engine message name
		com.strncpy( sz, svc_strings[cmd - 200], sizeof( sz ));
	}
	else if( cmd >= 0 && cmd < MAX_USER_MESSAGES )
	{
		// get user message name
		if( clgame.msg[cmd].name[0] )
			com.strncpy( sz, clgame.msg[cmd].name, sizeof( sz ));
	}
	return sz;
}

/*
=====================
CL_Parse_RecordCommand

record new message params into debug buffer
=====================
*/
void CL_Parse_RecordCommand( int cmd, int startoffset )
{
	int	slot;

	if( cmd == svc_nop ) return;
	
	slot = ( cls_message_debug.currentcmd++ & MSG_MASK );
	cls_message_debug.oldcmd[slot].command = cmd;
	cls_message_debug.oldcmd[slot].starting_offset = startoffset;
	cls_message_debug.oldcmd[slot].frame_number = host.framecount;
}

/*
=====================
CL_WriteErrorMessage

write net_message into buffer.dat for debugging
=====================
*/
void CL_WriteErrorMessage( int current_count, bitbuf_t *msg )
{
	file_t		*fp;
	const char	*buffer_file = "buffer.dat";
	
	fp = FS_Open( buffer_file, "wb" );
	if( !fp ) return;

	FS_Write( fp, &starting_count, sizeof( int ));
	FS_Write( fp, &current_count, sizeof( int ));
	FS_Write( fp, BF_GetData( msg ), BF_GetMaxBytes( msg ));
	FS_Close( fp );

	MsgDev( D_INFO, "Wrote erroneous message to %s\n", buffer_file );
}

/*
=====================
CL_WriteMessageHistory

list last 32 messages for debugging net troubleshooting
=====================
*/
void CL_WriteMessageHistory( void )
{
	int	i, thecmd;
	oldcmd_t	*old, *failcommand;
	bitbuf_t	*msg = &net_message;

	if( !cls.initialized || cls.state == ca_disconnected )
		return;

	if( !cls_message_debug.parsing )
		return;

	MsgDev( D_INFO, "Last %i messages parsed.\n", MSG_COUNT );

	// finish here
	thecmd = cls_message_debug.currentcmd - 1;
	thecmd -= ( MSG_COUNT - 1 );	// back up to here

	for( i = 0; i < MSG_COUNT - 1; i++ )
	{
		thecmd &= CMD_MASK;
		old = &cls_message_debug.oldcmd[thecmd];

		MsgDev( D_INFO,"%i %04i %s\n", old->frame_number, old->starting_offset, CL_MsgInfo( old->command ));

		thecmd++;
	}

	failcommand = &cls_message_debug.oldcmd[thecmd];

	MsgDev( D_INFO, "BAD:  %3i:%s\n", BF_GetNumBytesRead( msg ) - 1, CL_MsgInfo( failcommand->command ));

	if( host.developer >= 3 )
	{
		CL_WriteErrorMessage( BF_GetNumBytesRead( msg ) - 1, msg );
	}
	cls_message_debug.parsing = false;
}

/*
===============
CL_CheckOrDownloadFile

Returns true if the file exists, otherwise it attempts
to start a download from the server.
===============
*/
bool CL_CheckOrDownloadFile( const char *filename )
{
	string	name;
	file_t	*f;

	if( FS_FileExists( filename ))
	{
		// it exists, no need to download
		return true;
	}

	com.strncpy( cls.downloadname, filename, MAX_STRING );
	com.strncpy( cls.downloadtempname, filename, MAX_STRING );

	// download to a temp name, and only rename to the real name when done,
	// so if interrupted a runt file won't be left
	FS_StripExtension( cls.downloadtempname );
	FS_DefaultExtension( cls.downloadtempname, ".tmp" );
	com.strncpy( name, cls.downloadtempname, MAX_STRING );

	f = FS_Open( name, "a+b" );
	if( f )
	{
		// it exists
		size_t	len = FS_Tell( f );

		cls.download = f;
		// give the server an offset to start the download
		MsgDev( D_INFO, "Resume download %s at %i\n", cls.downloadname, len );
		BF_WriteByte( &cls.netchan.message, clc_stringcmd );
		BF_WriteString( &cls.netchan.message, va("download %s %i", cls.downloadname, len ));
	}
	else
	{
		MsgDev( D_INFO, "Start download %s\n", cls.downloadname );
		BF_WriteByte( &cls.netchan.message, clc_stringcmd );
		BF_WriteString( &cls.netchan.message, va("download %s", cls.downloadname ));
	}

	cls.downloadnumber++;
	return false;
}

/*
=====================
CL_ParseDownload

A download message has been received from the server
=====================
*/
void CL_ParseDownload( bitbuf_t *msg )
{
	int	size, percent;
	char	buffer[MAX_MSGLEN];
	string	name;
	int	r;

	// read the data
	size = BF_ReadShort( msg );
	percent = BF_ReadByte( msg );

	if( size == -1 )
	{
		Msg( "Server does not have this file.\n" );
		if( cls.download )
		{
			// if here, we tried to resume a file but the server said no
			FS_Close( cls.download );
			cls.download = NULL;
		}
		CL_RequestNextDownload();
		return;
	}

	// open the file if not opened yet
	if( !cls.download )
	{
		com.strncpy( name, cls.downloadtempname, MAX_STRING );
		cls.download = FS_Open ( name, "wb" );

		if( !cls.download )
		{
			msg->iCurBit += size << 3;	// FIXME!!!
			MsgDev( D_ERROR, "failed to open %s\n", cls.downloadtempname );
			CL_RequestNextDownload();
			return;
		}
	}

	ASSERT( size <= sizeof( buffer ));

	BF_ReadBytes( msg, buffer, size );
	FS_Write( cls.download, buffer, size );

	if( percent != 100 )
	{
		// request next block
		Cvar_SetValue("scr_download", percent );
		BF_WriteByte( &cls.netchan.message, clc_stringcmd );
		BF_WriteString( &cls.netchan.message, "nextdl" );
	}
	else
	{
		FS_Close( cls.download );

		// rename the temp file to it's final name
		r = FS_Rename( cls.downloadtempname, cls.downloadname );
		if( r ) MsgDev( D_ERROR, "failed to rename.\n" );

		cls.download = NULL;
		Cvar_SetValue( "scr_download", 0.0f );

		// get another file if needed
		CL_RequestNextDownload();
	}
}

void CL_RunBackgroundTrack( void )
{
	string	intro, main, track;

	// run background track
	com.strncpy( track, cl.configstrings[CS_BACKGROUND_TRACK], MAX_STRING );
	com.snprintf( intro, MAX_STRING, "%s_intro", cl.configstrings[CS_BACKGROUND_TRACK] );
	com.snprintf( main, MAX_STRING, "%s_main", cl.configstrings[CS_BACKGROUND_TRACK] );

	if( FS_FileExists( va( "media/%s.ogg", intro )) && FS_FileExists( va( "media/%s.ogg", main )))
	{
		// combined track with introduction and main loop theme
		S_StartBackgroundTrack( intro, main );
	}
	else if( FS_FileExists( va( "media/%s.ogg", track )))
	{
		// single looped theme
		S_StartBackgroundTrack( track, track );
	}
	else if( !com.strcmp( track, "" ))
	{
		// blank name stopped last track
		S_StopBackgroundTrack();
	}
}

/*
==================
CL_ParseSoundPacket

==================
*/
void CL_ParseSoundPacket( bitbuf_t *msg, bool is_ambient )
{
	vec3_t	pos_;
	float	*pos = NULL;
	int 	chan, sound;
	float 	volume, attn;  
	int	flags, pitch, entnum;
	sound_t	handle;

	flags = BF_ReadWord( msg );
	sound = BF_ReadWord( msg );
	chan = BF_ReadByte( msg );

	if( flags & SND_VOLUME )
		volume = BF_ReadByte( msg ) / 255.0f;
	else volume = VOL_NORM;

	if( flags & SND_ATTENUATION )
		attn = BF_ReadByte( msg ) / 64.0f;
	else attn = ATTN_NONE;	

	if( flags & SND_PITCH )
		pitch = BF_ReadByte( msg );
	else pitch = PITCH_NORM;

	// entity reletive
	entnum = BF_ReadWord( msg ); 

	// positioned in space
	if( flags & SND_FIXED_ORIGIN )
	{
		pos = pos_;
		BF_ReadBitVec3Coord( msg, pos );
	}

	if( flags & SND_SENTENCE )
	{
		char	sentenceName[32];

		com.snprintf( sentenceName, sizeof( sentenceName ), "!%i", sound );
		handle = S_RegisterSound( sentenceName );
	}
	else handle = cl.sound_precache[sound];	// see precached sound

	if( is_ambient )
	{
		S_AmbientSound( pos, entnum, chan, handle, volume, attn, pitch, flags );
	}
	else
	{
		S_StartSound( pos, entnum, chan, handle, volume, attn, pitch, flags );
	}
}

/*
==================
CL_ParseMovevars

==================
*/
void CL_ParseMovevars( bitbuf_t *msg )
{
	BF_ReadDeltaMovevars( msg, &clgame.oldmovevars, &clgame.movevars );
	Mem_Copy( &clgame.oldmovevars, &clgame.movevars, sizeof( movevars_t ));
}

/*
==================
CL_ParseParticles

==================
*/
void CL_ParseParticles( bitbuf_t *msg )
{
	vec3_t		org, dir;
	int		i, count, color;
	
	BF_ReadBitVec3Coord( msg, org );	

	for( i = 0; i < 3; i++ )
		dir[i] = BF_ReadChar( msg ) * (1.0f / 16);

	count = BF_ReadByte( msg );
	color = BF_ReadByte( msg );
	if( count == 255 ) count = 1024;

	clgame.dllFuncs.pfnParticleEffect( org, dir, color, count );
}

/*
==================
CL_ParseStaticDecal

==================
*/
void CL_ParseStaticDecal( bitbuf_t *msg )
{
	vec3_t	origin;
	int	decalIndex, entityIndex, modelIndex;
	int	flags;

	BF_ReadBitVec3Coord( msg, origin );
	decalIndex = BF_ReadWord( msg );
	entityIndex = BF_ReadShort( msg );

	if( entityIndex > 0 )
		modelIndex = BF_ReadWord( msg );
	else modelIndex = 0;
	flags = BF_ReadByte( msg );

	CL_DecalShoot( cl.decal_shaders[decalIndex], entityIndex, modelIndex, origin, flags );
}

void CL_ParseSoundFade( bitbuf_t *msg )
{
	float	fadePercent, fadeOutSeconds;
	float	holdTime, fadeInSeconds;

	fadePercent = BF_ReadFloat( msg );
	fadeOutSeconds = BF_ReadFloat( msg );
	holdTime = BF_ReadFloat( msg );
	fadeInSeconds = BF_ReadFloat( msg );

	S_FadeClientVolume( fadePercent, fadeOutSeconds, holdTime, fadeInSeconds );
}

void CL_ParseReliableEvent( bitbuf_t *msg, int flags )
{
	int		event_index;
	event_args_t	nullargs, args;
	float		delay;

	Mem_Set( &nullargs, 0, sizeof( nullargs ));
	event_index = BF_ReadWord( msg );		// read event index
	delay = BF_ReadWord( msg ) / 100.0f;		// read event delay
	MSG_ReadDeltaEvent( msg, &nullargs, &args );	// FIXME: zero-compressing

	CL_QueueEvent( flags, event_index, delay, &args );
}

void CL_ParseEvent( bitbuf_t *msg )
{
	int	i, num_events;

	num_events = BF_ReadByte( msg );

	// parse events queue
	for( i = 0 ; i < num_events; i++ )
		CL_ParseReliableEvent( msg, 0 );
}

/*
=====================================================================

  SERVER CONNECTING MESSAGES

=====================================================================
*/
/*
==================
CL_ParseServerData
==================
*/
void CL_ParseServerData( bitbuf_t *msg )
{
	string	str;
	int	i;

	MsgDev( D_NOTE, "Serverdata packet received.\n" );

	// wipe the client_t struct
	CL_ClearState();
	UI_SetActiveMenu( false );
	cls.state = ca_connected;

	// parse protocol version number
	i = BF_ReadLong( msg );
	cls.serverProtocol = i;

	if( i != PROTOCOL_VERSION )
		Host_Error( "Server use invalid protocol (%i should be %i)\n", i, PROTOCOL_VERSION );

	cl.servercount = BF_ReadLong( msg );
	cl.playernum = BF_ReadByte( msg );
	clgame.globals->maxClients = BF_ReadByte( msg );
	clgame.globals->maxEntities = BF_ReadWord( msg );
	com.strncpy( str, BF_ReadString( msg ), MAX_STRING );
	com.strncpy( clgame.maptitle, BF_ReadString( msg ), MAX_STRING );

	gameui.globals->maxClients = clgame.globals->maxClients;
	com.strncpy( gameui.globals->maptitle, clgame.maptitle, sizeof( gameui.globals->maptitle ));

	// no effect for local client
	// merge entcount only for remote clients 
	GI->max_edicts = clgame.globals->maxEntities;

	CL_InitEdicts (); // re-arrange edicts

	// get splash name
	Cvar_Set( "cl_levelshot_name", va( "levelshots/%s", str ));
	Cvar_SetValue( "scr_loading", 0.0f ); // reset progress bar

	if( cl_allow_levelshots->integer && !cls.changelevel )
	{
		// FIXME: Quake3 may be use both 'jpg' and 'tga' levelshot types
		if( !FS_FileExists( va( "†%s.%s", cl_levelshot_name->string, SI->levshot_ext ))) 
		{
			Cvar_Set( "cl_levelshot_name", MAP_DEFAULT_SHADER );	// render a black screen
			cls.scrshot_request = scrshot_plaque;			// make levelshot
		}
	}

	// need to prep refresh at next oportunity
	cl.video_prepped = false;
	cl.audio_prepped = false;

	// initialize world and clients
	CL_InitWorld ();
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline( bitbuf_t *msg )
{
	int		newnum, timebase;
	entity_state_t	nullstate;
	entity_state_t	*baseline;
	edict_t		*ent;

	Mem_Set( &nullstate, 0, sizeof( nullstate ));
	newnum = BF_ReadWord( msg );

	if( newnum < 0 ) Host_Error( "CL_SpawnEdict: invalid number %i\n", newnum );
	if( newnum > clgame.globals->maxEntities ) Host_Error( "CL_AllocEdict: no free edicts\n" );

	// increase edicts
	while( newnum >= clgame.globals->numEntities )
		clgame.globals->numEntities++;

	ent = EDICT_NUM( newnum );
	if( ent->free ) CL_InitEdict( ent ); // initialize edict

	if( cls.state == ca_active )
		timebase = cl.frame.servertime;
	else timebase = 1000; // sv.state == ss_loading

	baseline = &clgame.baselines[newnum];
	MSG_ReadDeltaEntity( msg, &nullstate, baseline, newnum, cl.time );
	CL_LinkEdict( ent, false ); // first entering, link always
}

/*
================
CL_ParseConfigString
================
*/
void CL_ParseConfigString( bitbuf_t *msg )
{
	int	i;

	i = BF_ReadShort( msg );
	if( i < 0 || i >= MAX_CONFIGSTRINGS )
		Host_Error( "configstring > MAX_CONFIGSTRINGS\n" );
	com.strcpy( cl.configstrings[i], BF_ReadString( msg ));
		
	// do something apropriate 
	if( i == CS_SKYNAME && cl.video_prepped )
	{
		re->RegisterShader( cl.configstrings[CS_SKYNAME], SHADER_SKY );
	}
	else if( i == CS_SERVERFLAGS )
	{
		clgame.globals->serverflags = com.atoi( cl.configstrings[CS_SERVERFLAGS] );
	}
	else if( i == CS_ZFAR )
	{
		cl.refdef.zFar = com.atof( cl.configstrings[CS_ZFAR] );
	}
	else if( i == CS_SKYCOLOR )
	{
		com.atov( cl.refdef.skyColor, cl.configstrings[CS_SKYCOLOR], 3 );
	}
	else if( i == CS_WATERAMP )
	{
		edict_t	*world = CL_GetEdictByIndex( 0 );
		world->v.scale = com.atof( cl.configstrings[CS_WATERAMP] );
	}
	else if( i == CS_SKYVEC )
	{
		com.atov( cl.refdef.skyVec, cl.configstrings[CS_SKYVEC], 3 );
	}
	else if( i > CS_WATERAMP && i < CS_MODELS )
	{
		Host_Error( "CL_ParseConfigString: reserved configstring #%i are used\n", i );
	}
	else if( i == CS_BACKGROUND_TRACK && cl.audio_prepped )
	{
		CL_RunBackgroundTrack();
	}
	else if( i >= CS_MODELS && i < CS_MODELS+MAX_MODELS && cl.video_prepped )
	{
		re->RegisterModel( cl.configstrings[i], i-CS_MODELS );
		CM_RegisterModel( cl.configstrings[i], i-CS_MODELS );
	}
	else if( i >= CS_SOUNDS && i < CS_SOUNDS+MAX_SOUNDS && cl.audio_prepped )
	{
		cl.sound_precache[i-CS_SOUNDS] = S_RegisterSound( cl.configstrings[i] );
	}
	else if( i >= CS_DECALS && i < CS_DECALS+MAX_DECALNAMES && cl.video_prepped )
	{
		cl.decal_shaders[i-CS_DECALS] = re->RegisterShader( cl.configstrings[i], SHADER_DECAL );
	}
	else if( i >= CS_EVENTS && i < CS_EVENTS+MAX_EVENTS )
	{
		CL_SetEventIndex( cl.configstrings[i], i - CS_EVENTS );
	}
	else if( i >= CS_CLASSNAMES && i < CS_CLASSNAMES+MAX_CLASSNAMES )
	{
		// edicts classnames for search by classname on client
		cl.edict_classnames[i-CS_CLASSNAMES] = MAKE_STRING( cl.configstrings[i] );
	}
	else if( i >= CS_LIGHTSTYLES && i < CS_LIGHTSTYLES+MAX_LIGHTSTYLES )
	{
		CL_SetLightstyle( i - CS_LIGHTSTYLES );
	}
}

/*
================
CL_ParseSetAngle

set the view angle to this absolute value
================
*/
void CL_ParseSetAngle( bitbuf_t *msg )
{
	cl.refdef.cl_viewangles[0] = BF_ReadBitAngle( msg, 16 );
	cl.refdef.cl_viewangles[1] = BF_ReadBitAngle( msg, 16 );
}

/*
================
CL_ParseAddAngle

add the view angle yaw
================
*/
void CL_ParseAddAngle( bitbuf_t *msg )
{
	float	add_angle;
	
	add_angle = BF_ReadBitAngle( msg, 16 );
	cl.refdef.cl_viewangles[1] += add_angle;
}
/*
================
CL_ParseCrosshairAngle

offset crosshair angles
================
*/
void CL_ParseCrosshairAngle( bitbuf_t *msg )
{
	cl.refdef.crosshairangle[0] = BF_ReadBitAngle( msg, 8 );
	cl.refdef.crosshairangle[1] = BF_ReadBitAngle( msg, 8 );
	cl.refdef.crosshairangle[2] = 0.0f; // not used for screen space
}

/*
================
CL_RegisterUserMessage

register new user message or update existing
================
*/
void CL_RegisterUserMessage( bitbuf_t *msg )
{
	char	*pszName;
	int	svc_num, size;
	
	pszName = BF_ReadString( msg );
	svc_num = BF_ReadByte( msg );
	size = BF_ReadByte( msg );

	// important stuff
	if( size == 0xFF ) size = -1;

	CL_LinkUserMessage( pszName, svc_num, size );
}

/*
================
CL_UpdateUserinfo

collect userinfo from all players
================
*/
void CL_UpdateUserinfo( bitbuf_t *msg )
{
	int		slot;
	bool		active;
	player_info_t	*player;

	slot = BF_ReadByte( msg );

	if( slot >= MAX_CLIENTS )
		Host_Error( "CL_ParseServerMessage: svc_updateuserinfo > MAX_CLIENTS\n" );

	player = &cl.players[slot];
	active = BF_ReadByte( msg ) ? true : false;

	if( active )
	{
		com.strncpy( player->userinfo, BF_ReadString( msg ), sizeof( player->userinfo ));
		com.strncpy( player->name, Info_ValueForKey( player->userinfo, "name" ), sizeof( player->name ));
		com.strncpy( player->model, Info_ValueForKey( player->userinfo, "model" ), sizeof( player->model ));
	}
	else Mem_Set( player, 0, sizeof( *player ));
}

/*
==============
CL_ServerInfo

change serverinfo
==============
*/
void CL_ServerInfo( bitbuf_t *msg )
{
	char	key[MAX_MSGLEN];
	char	value[MAX_MSGLEN];

	com.strncpy( key, BF_ReadString( msg ), sizeof( key ));
	com.strncpy( value, BF_ReadString( msg ), sizeof( value ));
	Info_SetValueForKey( cl.serverinfo, key, value );
}

/*
==============
CL_ParseUserMessage

handles all user messages
==============
*/
void CL_ParseUserMessage( bitbuf_t *msg, int svc_num )
{
	int	i, iSize;
	byte	*pbuf;

	// NOTE: any user message parse on engine, not in client.dll
	if( svc_num < 0 || svc_num >= MAX_USER_MESSAGES )
	{
		// out or range
		Host_Error( "CL_ParseUserMessage: illegible server message %d\n", svc_num );
		return;
	}

	// unregister message can't be parsed
	if( clgame.msg[svc_num].number != svc_num )
	{
		for( i = 0; i < MAX_USER_MESSAGES; i++ )
		{
			// across-transition problems... this is temporary solution
			if( clgame.msg[i].number == svc_num )
			{
				MsgDev( D_WARN, "CL_RemapUserMsg: from %i to %i\n", svc_num, i );
				svc_num = i;
				break;
			}
		}

		if( i == MAX_USER_MESSAGES )
			Host_Error( "CL_ParseUserMessage: illegible server message %d\n", svc_num );
	}

	iSize = clgame.msg[svc_num].size;
	pbuf = NULL;

	// message with variable sizes receive an actual size as first byte
	if( iSize == -1 ) iSize = BF_ReadByte( msg );
	if( iSize > 0 ) pbuf = Mem_Alloc( clgame.private, iSize );

	// parse user message into buffer
	BF_ReadBytes( msg, pbuf, iSize );

	if( clgame.msg[svc_num].func ) clgame.msg[svc_num].func( clgame.msg[svc_num].name, iSize, pbuf );
	else MsgDev( D_WARN, "CL_ParseUserMessage: %s not hooked\n", clgame.msg[svc_num].name );
	if( pbuf ) Mem_Free( pbuf );
}

/*
=====================================================================

ACTION MESSAGES

=====================================================================
*/
/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage( bitbuf_t *msg )
{
	char	*s;
	int	i, cmd;
	int	bufStart;

	cls_message_debug.parsing = true;		// begin parsing
	starting_count = BF_GetNumBytesRead( msg );	// updates each frame
	
	// parse the message
	while( 1 )
	{
		if( BF_CheckOverflow( msg ))
		{
			Host_Error( "CL_ParseServerMessage: bad server message\n" );
			return;
		}

		// mark start position
		bufStart = BF_GetNumBytesRead( msg );

		// end of message
		if( BF_GetNumBitsLeft( msg ) < 8 )
			break;		

		cmd = BF_ReadByte( msg );

		// record command for debugging spew on parse problem
		CL_Parse_RecordCommand( cmd, bufStart );

		// other commands
		switch( cmd )
		{
		case svc_nop:
			MsgDev( D_ERROR, "CL_ParseServerMessage: user message out of bounds\n" );
			break;
		case svc_disconnect:
			CL_Drop ();
			Host_AbortCurrentFrame();
			break;
		case svc_changing:
			cls.changelevel = true;
			Cmd_ExecuteString( "hud_changelevel\n" );
			S_StopAllSounds();
			// intentional fallthrough
		case svc_reconnect:
			if( !cls.changelevel )
				MsgDev( D_INFO, "Server disconnected, reconnecting\n" );
			if( cls.download )
			{
				FS_Close( cls.download );
				cls.download = NULL;
			}
			cls.state = ca_connecting;
			cls.connect_time = MAX_HEARTBEAT; // CL_CheckForResend() will fire immediately
			break;
		case svc_stufftext:
			s = BF_ReadString( msg );
			Cbuf_AddText( s );
			break;
		case svc_serverdata:
			Cbuf_Execute(); // make sure any stuffed commands are done
			CL_ParseServerData( msg );
			break;
		case svc_configstring:
			CL_ParseConfigString( msg );
			break;
		case svc_spawnbaseline:
			CL_ParseBaseline( msg );
			break;
		case svc_deltatable:
			Delta_ParseTableField( msg );
			break;
		case svc_download:
			CL_ParseDownload( msg );
			break;
		case svc_sound:
			CL_ParseSoundPacket( msg, false );
			break;
		case svc_ambientsound:
			CL_ParseSoundPacket( msg, true );
			break;
		case svc_setangle:
			CL_ParseSetAngle( msg );
			break;
		case svc_addangle:
			CL_ParseAddAngle( msg );
			break;
		case svc_setview:
			cl.refdef.viewentity = BF_ReadWord( msg );
			break;
		case svc_crosshairangle:
			CL_ParseCrosshairAngle( msg );
			break;
		case svc_print:
			i = BF_ReadByte( msg );
			if( i == PRINT_CHAT ) // chat
				S_StartLocalSound( "misc/talk.wav" );	// FIXME: INTRESOURCE
			MsgDev( D_INFO, "^6%s\n", BF_ReadString( msg ));
			break;
		case svc_centerprint:
			CL_CenterPrint( BF_ReadString( msg ), 0.35f );
			break;
		case svc_setpause:
			cl.refdef.paused = (BF_ReadByte( msg ) != 0 );
			break;
		case svc_movevars:
			CL_ParseMovevars( msg );
			break;
		case svc_particle:
			CL_ParseParticles( msg );
			break;
		case svc_bspdecal:
			CL_ParseStaticDecal( msg );
			break;
		case svc_soundfade:
			CL_ParseSoundFade( msg );
			break;
		case svc_event:
			CL_ParseEvent( msg );
			break;
		case svc_event_reliable:
			CL_ParseReliableEvent( msg, FEV_RELIABLE );
			break;
		case svc_updateuserinfo:
			CL_UpdateUserinfo( msg );
			break;
		case svc_serverinfo:
			CL_ServerInfo( msg );
			break;
		case svc_usermessage:
			CL_RegisterUserMessage( msg );
			break;
		case svc_frame:
			CL_ParseFrame( msg );
			break;
		case svc_packetentities:
			Host_Error( "CL_ParseServerMessage: out of place frame data\n" );
			break;
		default:
			CL_ParseUserMessage( msg, cmd );
			break;
		}
	}
	cls_message_debug.parsing = false;	// done
}