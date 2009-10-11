/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// cl_parse.c  -- parse a message received from the server

#include "common.h"
#include "client.h"
#include "net_sound.h"

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
		MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
		MSG_Print( &cls.netchan.message, va("download %s %i", cls.downloadname, len ));
	}
	else
	{
		MsgDev( D_INFO, "Start download %s\n", cls.downloadname );
		MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
		MSG_Print( &cls.netchan.message, va("download %s", cls.downloadname ));
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
void CL_ParseDownload( sizebuf_t *msg )
{
	int		size, percent;
	string		name;
	int		r;

	// read the data
	size = MSG_ReadShort( msg );
	percent = MSG_ReadByte( msg );

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
			msg->readcount += size;
			Msg( "Failed to open %s\n", cls.downloadtempname );
			CL_RequestNextDownload();
			return;
		}
	}

	FS_Write( cls.download, msg->data + msg->readcount, size );
	msg->readcount += size;

	if( percent != 100 )
	{
		// request next block
		Cvar_SetValue("scr_download", percent );
		MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
		MSG_Print( &cls.netchan.message, "nextdl" );
	}
	else
	{
		string	oldn, newn;

		FS_Close( cls.download );

		// rename the temp file to it's final name
		com.strncpy( oldn, cls.downloadtempname, MAX_STRING );
		com.strncpy( newn, cls.downloadname, MAX_STRING );
		r = rename( oldn, newn );
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
void CL_ParseSoundPacket( sizebuf_t *msg )
{
	vec3_t	pos_;
	float	*pos = NULL;
	int 	channel, sound_num;
	float 	volume, attenuation;  
	int	flags, pitch, entnum;

	flags = MSG_ReadWord( msg );
	sound_num = MSG_ReadWord( msg );
	channel = MSG_ReadByte( msg );

	if( flags & SND_VOLUME )
		volume = MSG_ReadByte( msg ) / 255.0f;
	else volume = VOL_NORM;
	if( flags & SND_SOUNDLEVEL )
	{
		int soundlevel = MSG_ReadByte( msg );
		attenuation = SNDLVL_TO_ATTN( soundlevel );
	}
	else attenuation = ATTN_NONE;	
	if( flags & SND_PITCH )
		pitch = MSG_ReadByte( msg );
	else pitch = PITCH_NORM;

	// entity reletive
	entnum = MSG_ReadBits( msg, NET_WORD ); 

	// positioned in space
	if( flags & SND_FIXED_ORIGIN )
	{
		pos = pos_;
		MSG_ReadPos( msg, pos );
	}
	S_StartSound( pos, entnum, channel, cl.sound_precache[sound_num], volume, attenuation, pitch, flags );
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
void CL_ParseServerData( sizebuf_t *msg )
{
	string		str;
	const char	*levelshot_ext[] = { "tga", "jpg", "png" };
	int		i;

	MsgDev( D_NOTE, "Serverdata packet received.\n" );

	// wipe the client_t struct
	CL_ClearState();
	cls.state = ca_connected;

	// parse protocol version number
	i = MSG_ReadLong( msg );
	cls.serverProtocol = i;

	if( i != PROTOCOL_VERSION )
		Host_Error( "Server use invalid protocol (%i should be %i)\n", i, PROTOCOL_VERSION );

	cl.servercount = MSG_ReadLong( msg );
	cl.playernum = MSG_ReadShort( msg );
	com.strncpy( str, MSG_ReadString( msg ), MAX_STRING );
	com.strncpy( clgame.maptitle, MSG_ReadString( msg ), MAX_STRING );

	// get splash name
	Cvar_Set( "cl_levelshot_name", va( "levelshots/%s", str ));
	Cvar_SetValue( "scr_loading", 0.0f ); // reset progress bar

	for( i = 0; i < 3; i++ )
		if( FS_FileExists( va( "%s.%s", cl_levelshot_name->string, levelshot_ext[i] ))) 
			break;
	if( i == 3 )
	{
		Cvar_Set( "cl_levelshot_name", MAP_DEFAULT_SHADER );	// render a black screen
		cls.scrshot_request = scrshot_plaque;			// make levelshot
	}
	// seperate the printfs so the server message can have a color
	Msg("\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");
	Msg( "^2%s\n", clgame.maptitle );

	// need to prep refresh at next oportunity
	cl.video_prepped = false;
	cl.audio_prepped = false;
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline( sizebuf_t *msg )
{
	int		newnum;
	entity_state_t	nullstate;
	entity_state_t	*baseline;

	Mem_Set( &nullstate, 0, sizeof( nullstate ));
	newnum = MSG_ReadBits( msg, NET_WORD );

	if( !newnum ) CL_InitEdicts();

	baseline = &cl.entity_baselines[newnum];
	MSG_ReadDeltaEntity( msg, &nullstate, baseline, newnum );
}

void CL_ParseMoveVars( int number )
{
	float	value;

	if( number == CS_MAXVELOCITY )
	{
		value = com.atof( cl.configstrings[CS_MAXVELOCITY] );
		if( value > 0 ) clgame.movevars.maxvelocity = value;
		else clgame.movevars.maxvelocity = com.atof( DEFAULT_MAXVELOCITY );
	}
	else if( number == CS_GRAVITY )
	{
		value = com.atof( cl.configstrings[CS_GRAVITY] );
		if( value > 0 ) clgame.movevars.gravity = value;
		else clgame.movevars.gravity = com.atof( DEFAULT_GRAVITY );
	}
	else if( number == CS_ROLLSPEED )
	{
		value = com.atof( cl.configstrings[CS_ROLLSPEED] );
		if( value > 0 ) clgame.movevars.rollspeed = value;
		else clgame.movevars.rollspeed = com.atof( DEFAULT_ROLLSPEED );
	}
	else if( number == CS_ROLLANGLE )
	{
		value = com.atof( cl.configstrings[CS_ROLLANGLE] );
		if( value > 0 ) clgame.movevars.rollangle = value;
		else clgame.movevars.rollangle = com.atof( DEFAULT_ROLLANGLE );
	}
	else if( number == CS_MAXSPEED )
	{
		value = com.atof( cl.configstrings[CS_MAXSPEED] );
		if( value > 0 ) clgame.movevars.maxspeed = value;
		else clgame.movevars.maxspeed = com.atof( DEFAULT_MAXSPEED );
	}
	else if( number == CS_STEPHEIGHT )
	{
		value = com.atof( cl.configstrings[CS_STEPHEIGHT] );
		if( value > 0 ) clgame.movevars.stepheight = value;
		else clgame.movevars.stepheight = com.atof( DEFAULT_STEPHEIGHT );
	}
	else if( number == CS_AIRACCELERATE )
	{
		value = com.atof( cl.configstrings[CS_AIRACCELERATE] );
		if( value > 0 ) clgame.movevars.airaccelerate = value;
		else clgame.movevars.airaccelerate = com.atof( DEFAULT_AIRACCEL );
	}
	else if( number == CS_ACCELERATE )
	{
		value = com.atof( cl.configstrings[CS_ACCELERATE] );
		if( value > 0 ) clgame.movevars.accelerate = value;
		else clgame.movevars.accelerate = com.atof( DEFAULT_ACCEL );
	}
	else if( number == CS_FRICTION )
	{
		value = com.atof( cl.configstrings[CS_FRICTION] );
		if( value > 0 ) clgame.movevars.friction = value;
		else clgame.movevars.friction = com.atof( DEFAULT_FRICTION );
	}
}

/*
================
CL_ParseConfigString
================
*/
void CL_ParseConfigString( sizebuf_t *msg )
{
	int	i;

	i = MSG_ReadShort( msg );
	if( i < 0 || i >= MAX_CONFIGSTRINGS )
		Host_Error( "configstring > MAX_CONFIGSTRINGS\n" );
	com.strcpy( cl.configstrings[i], MSG_ReadString( msg ));
		
	// do something apropriate 
	if( i == CS_SKYNAME && cl.video_prepped )
	{
		re->RegisterShader( cl.configstrings[CS_SKYNAME], SHADER_SKY );
	}
	else if( i > CS_BACKGROUND_TRACK && i < CS_MODELS )
	{
		CL_ParseMoveVars( i );
	}
	else if( i == CS_BACKGROUND_TRACK && cl.audio_prepped )
	{
		CL_RunBackgroundTrack();
	}
	else if( i >= CS_MODELS && i < CS_MODELS+MAX_MODELS && cl.video_prepped )
	{
		re->RegisterModel( cl.configstrings[i], i-CS_MODELS );
		cl.models[i-CS_MODELS] = pe->RegisterModel( cl.configstrings[i] );
	}
	else if( i >= CS_SOUNDS && i < CS_SOUNDS+MAX_SOUNDS && cl.audio_prepped )
	{
		cl.sound_precache[i-CS_SOUNDS] = S_RegisterSound( cl.configstrings[i] );
	}
	else if( i >= CS_DECALS && i < CS_DECALS+MAX_DECALS && cl.video_prepped )
	{
		cl.decal_shaders[i-CS_DECALS] = re->RegisterShader( cl.configstrings[i], SHADER_GENERIC );
	}
	else if( i >= CS_USER_MESSAGES && i < CS_USER_MESSAGES+MAX_USER_MESSAGES )
	{
		CL_LinkUserMessage( cl.configstrings[i], i - CS_USER_MESSAGES );
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
void CL_ParseSetAngle( sizebuf_t *msg )
{
	cl.refdef.cl_viewangles[0] = MSG_ReadAngle32( msg );
	cl.refdef.cl_viewangles[1] = MSG_ReadAngle32( msg );
	cl.refdef.cl_viewangles[2] = MSG_ReadAngle32( msg );
}

/*
================
CL_ParseCrosshairAngle

offset crosshair angles
================
*/
void CL_ParseCrosshairAngle( sizebuf_t *msg )
{
	cl.refdef.crosshairangle[0] = MSG_ReadAngle8( msg );
	cl.refdef.crosshairangle[1] = MSG_ReadAngle8( msg );
	cl.refdef.crosshairangle[2] = 0; // not used for screen space
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
void CL_ParseServerMessage( sizebuf_t *msg )
{
	char	*s;
	int	cmd;

	// parse the message
	while( 1 )
	{
		if( msg->error )
		{
			Host_Error("CL_ParseServerMessage: Bad server message\n");
			return;
		}

		cmd = MSG_ReadByte( msg );
		if( cmd == -1 ) break;

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
		case svc_reconnect:
			Msg( "Server disconnected, reconnecting\n" );
			if( cls.download )
			{
				FS_Close( cls.download );
				cls.download = NULL;
			}
			cls.state = ca_connecting;
			cls.connect_time = MAX_HEARTBEAT; // CL_CheckForResend() will fire immediately
			break;
		case svc_stufftext:
			s = MSG_ReadString( msg );
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
		case svc_download:
			CL_ParseDownload( msg );
			break;
		case svc_sound:
			CL_ParseSoundPacket( msg );
			break;
		case svc_setangle:
			CL_ParseSetAngle( msg );
			break;
		case svc_setview:
			cl.refdef.viewentity = MSG_ReadWord( msg );
			break;
		case svc_crosshairangle:
			CL_ParseCrosshairAngle( msg );
			break;
		case svc_print:
			Con_Print( va( "^6%s\n", MSG_ReadString( msg )));
			break;
		case svc_frame:
			CL_ParseFrame( msg );
			break;
		case svc_playerinfo:
		case svc_packetentities:
			Host_Error( "CL_ParseServerMessage: out of place frame data\n" );
			break;
		case svc_bad:
			Host_Error( "CL_ParseServerMessage: svc_bad\n" );
			break;
		default:
			CL_ParseUserMessage( msg, cmd );
			break;
		}
	}
}