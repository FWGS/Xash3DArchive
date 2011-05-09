/*
cl_demo.c - demo record & playback
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

#include "common.h"
#include "client.h"
#include "net_encode.h"

#define dem_cmd	0
#define dem_read	1
#define dem_set	2

/*
====================
CL_WriteDemoCmd

Writes the current user cmd
====================
*/
void CL_WriteDemoCmd( usercmd_t *pcmd )
{
	int	i;
	float	fl;
	usercmd_t cmd;
	byte	c;

	fl = (float)host.realtime;
	FS_Write( cls.demofile, &fl, sizeof( fl ));

	c = dem_cmd;
	FS_Write( cls.demofile, &c, sizeof( c ));

	// correct for byte order, bytes don't matter
	cmd = *pcmd;

	FS_Write( cls.demofile, &cmd, sizeof( cmd ));

	for( i = 0; i < 3; i++ )
	{
		fl = cl.refdef.cl_viewangles[i];
		FS_Write( cls.demofile, &fl, sizeof( fl ));
	}
}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length
====================
*/
void CL_WriteDemoMessage( sizebuf_t *msg, int head_size )
{
	int	swlen;

	if( !cls.demofile ) return;
	if( cl.refdef.paused || cls.key_dest != key_game )
		return;

	// the first eight bytes are just packet sequencing stuff
	swlen = BF_GetNumBytesWritten( msg ) - head_size;

	if( !swlen ) return; // ignore null messages
	FS_Write( cls.demofile, &swlen, 4 );
	FS_Write( cls.demofile, BF_GetData( msg ) + head_size, swlen );
}

void CL_WriteDemoHeader( const char *name )
{
	int		i, j, len;
	char		buf_data[NET_MAX_PAYLOAD];
	entity_state_t	*state, nullstate;
	movevars_t	nullmovevars;
	sizebuf_t		buf;
	delta_info_t	*dt;

	MsgDev( D_INFO, "recording to %s.\n", name );
	cls.demofile = FS_Open( name, "wb", false );
	if( !cls.demofile )
	{
		MsgDev(D_ERROR, "CL_Record: unable to create %s\n", name );
		return;
	}

	cls.demorecording = true;

	// don't start saving messages until a non-delta compressed message is received
	cls.demowaiting = true;

	// write out messages to hold the startup information
	BF_Init( &buf, "DemoWrite", buf_data, sizeof( buf_data ));

	// send the serverdata
	BF_WriteByte( &buf, svc_serverdata );
	BF_WriteLong( &buf, PROTOCOL_VERSION );
	BF_WriteLong( &buf, cl.servercount );
	BF_WriteLong( &buf, cl.checksum );
	BF_WriteByte( &buf, cl.playernum|( cl.spectator ? 128 : 0 ));
	BF_WriteByte( &buf, cl.maxclients );
	BF_WriteWord( &buf, clgame.maxEntities );
	BF_WriteString( &buf, clgame.mapname );
	BF_WriteString( &buf, clgame.maptitle );
	BF_WriteOneBit( &buf, cl.background );
	BF_WriteString( &buf, GI->gamefolder );

	// write modellist
	for( i = 0; i < MAX_MODELS; i++ )
	{
		if( cl.model_precache[i][0] )
		{
			BF_WriteByte( &buf, svc_modelindex );
			BF_WriteUBitLong( &buf, i, MAX_MODEL_BITS );
			BF_WriteString( &buf, cl.model_precache[i] );

			if( BF_GetNumBytesWritten( &buf ) > ( BF_GetMaxBytes( &buf ) / 2 ))
			{	
				// write it out
				len = BF_GetNumBytesWritten( &buf );
				FS_Write( cls.demofile, &len, 4 );
				FS_Write( cls.demofile, BF_GetData( &buf ), len );
				BF_Clear( &buf );
			}
		}
	}

	// write soundlist
	for( i = 0; i < MAX_SOUNDS; i++ )
	{
		if( cl.sound_precache[i][0] )
		{
			BF_WriteByte( &buf, svc_soundindex );
			BF_WriteUBitLong( &buf, i, MAX_SOUND_BITS );
			BF_WriteString( &buf, cl.sound_precache[i] );

			if( BF_GetNumBytesWritten( &buf ) > ( BF_GetMaxBytes( &buf ) / 2 ))
			{	
				// write it out
				len = BF_GetNumBytesWritten( &buf );
				FS_Write( cls.demofile, &len, 4 );
				FS_Write( cls.demofile, BF_GetData( &buf ), len );
				BF_Clear( &buf );
			}
		}
	}

	// write eventlist
	for( i = 0; i < MAX_EVENTS; i++ )
	{
		if( cl.event_precache[i][0] )
		{
			BF_WriteByte( &buf, svc_eventindex );
			BF_WriteUBitLong( &buf, i, MAX_EVENT_BITS );
			BF_WriteString( &buf, cl.event_precache[i] );

			if( BF_GetNumBytesWritten( &buf ) > ( BF_GetMaxBytes( &buf ) / 2 ))
			{	
				// write it out
				len = BF_GetNumBytesWritten( &buf );
				FS_Write( cls.demofile, &len, 4 );
				FS_Write( cls.demofile, BF_GetData( &buf ), len );
				BF_Clear( &buf );
			}
		}
	}

	// write lightstyles
	for( i = 0; i < MAX_LIGHTSTYLES; i++ )
	{
		if( cl.lightstyles[i].pattern[0] )
		{
			BF_WriteByte( &buf, svc_lightstyle );
			BF_WriteByte( &buf, i );
			BF_WriteString( &buf, cl.lightstyles[i].pattern );

			if( BF_GetNumBytesWritten( &buf ) > ( BF_GetMaxBytes( &buf ) / 2 ))
			{	
				// write it out
				len = BF_GetNumBytesWritten( &buf );
				FS_Write( cls.demofile, &len, 4 );
				FS_Write( cls.demofile, BF_GetData( &buf ), len );
				BF_Clear( &buf );
			}
		}
	}

	// user messages
	for( i = 0; i < MAX_USER_MESSAGES; i++ )
	{
		if( clgame.msg[i].name[0] && clgame.msg[i].number >= svc_lastmsg )
		{
			BF_WriteByte( &buf, svc_usermessage );
			BF_WriteByte( &buf, clgame.msg[i].number );
			BF_WriteByte( &buf, (byte)clgame.msg[i].size );
			BF_WriteString( &buf, clgame.msg[i].name );

			if( BF_GetNumBytesWritten( &buf ) > ( BF_GetMaxBytes( &buf ) / 2 ))
			{	
				// write it out
				len = BF_GetNumBytesWritten( &buf );
				FS_Write( cls.demofile, &len, 4 );
				FS_Write( cls.demofile, BF_GetData( &buf ), len );
				BF_Clear( &buf );
			}
		}
	}

	// delta tables
	for( i = 0; i < Delta_NumTables( ); i++ )
	{
		dt = Delta_FindStructByIndex( i );

		for( j = 0; j < dt->numFields; j++ )
		{
			Delta_WriteTableField( &buf, i, &dt->pFields[j] );

			if( BF_GetNumBytesWritten( &buf ) > ( BF_GetMaxBytes( &buf ) / 2 ))
			{	
				// write it out
				len = BF_GetNumBytesWritten( &buf );
				FS_Write( cls.demofile, &len, 4 );
				FS_Write( cls.demofile, BF_GetData( &buf ), len );
				BF_Clear( &buf );
			}
		}
	}

	// baselines
	Q_memset( &nullstate, 0, sizeof( nullstate ));
	Q_memset( &nullmovevars, 0, sizeof( nullmovevars ));

	MSG_WriteDeltaMovevars( &buf, &nullmovevars, &clgame.movevars );

	for( i = 0; i < clgame.maxEntities; i++ )
	{
		state = &clgame.entities[i].baseline;
		if( !state->number ) continue;
		if( !state->modelindex || state->effects == EF_NODRAW )
			continue;

		BF_WriteByte( &buf, svc_spawnbaseline );		
		MSG_WriteDeltaEntity( &nullstate, state, &buf, true, CL_IsPlayerIndex( state->number ), cl.mtime[0] );

		if( BF_GetNumBytesWritten( &buf ) > ( BF_GetMaxBytes( &buf ) / 2 ))
		{	
			// write it out
			len = BF_GetNumBytesWritten( &buf );
			FS_Write( cls.demofile, &len, 4 );
			FS_Write( cls.demofile, BF_GetData( &buf ), len );
			BF_Clear( &buf );
		}
	}

	BF_WriteByte( &buf, svc_stufftext );
	BF_WriteString( &buf, "precache\n" );

	BF_WriteByte( &buf, svc_setview );
	BF_WriteWord( &buf, cl.refdef.viewentity );

	// write all clients userinfo
	for( i = 0; i < cl.maxclients; i++ )
	{
		player_info_t	*pi;

		BF_WriteByte( &buf, svc_updateuserinfo );
		BF_WriteUBitLong( &buf, i, MAX_CLIENT_BITS );
		pi = &cl.players[i];

		if( pi->name[0] )
		{
			BF_WriteOneBit( &buf, 1 );
			BF_WriteString( &buf, pi->userinfo );
		}
		else BF_WriteOneBit( &buf, 0 );
	}

	// write it to the demo file
	len = BF_GetNumBytesWritten( &buf );
	FS_Write( cls.demofile, &len, 4 );
	FS_Write( cls.demofile, BF_GetData( &buf ), len );

	// force client.dll update
	Cmd_ExecuteString( "cmd fullupdate\n", src_command );
	if( clgame.hInstance ) clgame.dllFuncs.pfnReset();

	cl.validsequence = 0;		// haven't gotten a valid frame update yet
	cl.delta_sequence = -1;		// we'll request a full delta from the baseline
	cls.lastoutgoingcommand = -1;		// we don't have a backed up cmd history yet
	cls.nextcmdtime = host.realtime;	// we can send a cmd right away

	// FIXME: current demo implementation is completely wrong
	// it's support only uncompressed demos at he moment
	Cvar_SetFloat( "cl_nodelta", 1.0f );
}

/*
=================
CL_DrawDemoRecording
=================
*/
void CL_DrawDemoRecording( void )
{
	char		string[64];
	rgba_t		color = { 255, 255, 255, 255 };
	fs_offset_t	pos;
	int		len;

	if(!( host.developer && cls.demorecording ))
		return;

	pos = FS_Tell( cls.demofile );
	Q_snprintf( string, sizeof( string ), "RECORDING %s: %ik", cls.demoname, pos / 1024 );

	Con_DrawStringLen( string, &len, NULL );
	Con_DrawString(( scr_width->integer - len) >> 1, scr_height->integer >> 2, string, color );
}

/*
=======================================================================

CLIENT SIDE DEMO PLAYBACK

=======================================================================
*/
/*
==================
CL_NextDemo

Called when a demo or cinematic finishes
If the "nextdemo" cvar is set, that command will be issued
==================
*/
qboolean CL_NextDemo( void )
{
	string	str;

	if( cls.demonum == -1 )
		return false; // don't play demos

	S_StopAllSounds();

	if( !cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS )
	{
		cls.demonum = 0;
		if( !cls.demos[cls.demonum][0] )
		{
			MsgDev( D_INFO, "no demos listed with startdemos\n" );
			cls.demonum = -1;
			return false;
		}
	}

	Q_snprintf( str, MAX_STRING, "playdemo %s\n", cls.demos[cls.demonum] );

	Cbuf_InsertText( str );
	cls.demonum++;

	return true;
}

/*
=================
CL_DemoCompleted
=================
*/
void CL_DemoCompleted( void )
{
	CL_StopPlayback();

	if( !CL_NextDemo() && host.developer <= 2 )
		UI_SetActiveMenu( true );
}

/*
=================
CL_ReadDemoMessage

reads demo data and write it to client
=================
*/
void CL_ReadDemoMessage( void )
{
	sizebuf_t	buf;
	char	buf_data[NET_MAX_PAYLOAD];
	int	r, curSize;

	if( !cls.demofile )
	{
		CL_DemoCompleted();
		return;
	}

	if( cl.refdef.paused || cls.key_dest != key_game )
		return;

	// don't need another message yet
	if(( cl.time + host.frametime ) <= cl.mtime[0] )
		return;

	// get the length
	r = FS_Read( cls.demofile, &curSize, 4 );
	if( r != 4 )
	{
		CL_DemoCompleted();
		return;
	}

	if( curSize == -1 )
	{
		CL_DemoCompleted();
		return;
	}

	if( curSize > sizeof( buf_data ))
	{
		Host_Error( "CL_ReadDemoMessage: demoMsglen > NET_MAX_PAYLOAD\n" );
		return;
	}

	// init the message (set maxsize to cursize so overflow check will be working properly)
	BF_Init( &buf, "DemoRead", buf_data, curSize );
	r = FS_Read( cls.demofile, buf.pData, curSize );

	if( r != curSize )
	{
		MsgDev( D_ERROR, "CL_ReadDemoMessage: demo file was truncated( %d )\n", cls.state );
		CL_DemoCompleted();
		return;
	}

	cls.connect_time = host.realtime;
	BF_Clear( &buf );	// reset curpos

	CL_ParseServerMessage( &buf );
}

/*
==============
CL_StopPlayback

Called when a demo file runs out, or the user starts a game
==============
*/
void CL_StopPlayback( void )
{
	if( !cls.demoplayback ) return;

	// release demofile
	FS_Close( cls.demofile );
	cls.demoplayback = false;
	cls.demofile = NULL;

	// let game known about movie state	
	cls.state = ca_disconnected;
	cls.demoname[0] = '\0';	// clear demoname too
	menu.globals->demoname[0] = '\0';

	if( clgame.hInstance ) clgame.dllFuncs.pfnReset(); // end of demos, stop the client
}

void CL_StopRecord( void )
{
	int	len = -1;

	if( !cls.demorecording ) return;

	// finish up
	FS_Write( cls.demofile, &len, 4 );
	FS_Close( cls.demofile );
	cls.demofile = NULL;
	cls.demorecording = false;
	cls.demoname[0] = '\0';
	menu.globals->demoname[0] = '\0';

	// FIXME: current demo implementation is completely wrong
	// it's support only uncompressed demos at he moment
	// enable delta-compression here at end of the demo record
	Cvar_SetFloat( "cl_nodelta", 0.0f );
}

/* 
================== 
CL_GetComment
================== 
*/  
qboolean CL_GetComment( const char *demoname, char *comment )
{
	file_t	*demfile;
	char     	buf_data[2048];
	int	r, maxClients, curSize;
	string	maptitle;
	sizebuf_t	buf;
	
	if( !comment ) return false;

	demfile = FS_Open( demoname, "rb", false );
	if( !demfile )
	{
		Q_strncpy( comment, "", MAX_STRING );
		return false;
          }

	// read the first demo packet. extract info from it
	// get the length
	r = FS_Read( demfile, &curSize, 4 );
	if( r != 4 )
	{
		FS_Close( demfile );
		Q_strncpy( comment, "<corrupted>", MAX_STRING );
		return false;
	}

	if( curSize == -1 )
	{
		FS_Close( demfile );
		Q_strncpy( comment, "<corrupted>", MAX_STRING );
		return false;
	}

	if( curSize > sizeof( buf_data ))
	{
		FS_Close( demfile );
		Q_strncpy( comment, "<not compatible>", MAX_STRING );
		return false;
	}

	// init the message (set maxsize to cursize so overflow check will be working properly)
	BF_Init( &buf, "DemoRead", buf_data, curSize );
	r = FS_Read( demfile, buf.pData, curSize );

	if( r != curSize )
	{
		FS_Close( demfile );
		Q_strncpy( comment, "<truncated file>", MAX_STRING );
		return false;
	}

	// skip server data ident
	BF_ReadByte( &buf );

	if( PROTOCOL_VERSION != BF_ReadLong( &buf ))
	{
		FS_Close( demfile );
		Q_strncpy( comment, "<invalid protocol>", MAX_STRING );
		return false;
	}

	BF_ReadLong( &buf ); // server count
	BF_ReadLong( &buf ); // checksum
	BF_ReadByte( &buf ); // playernum
	maxClients = BF_ReadByte( &buf );
	if( BF_ReadWord( &buf ) > GI->max_edicts )
	{
		FS_Close( demfile );
		Q_strncpy( comment, "<too many edicts>", MAX_STRING );
		return false;
	}

	// split comment to sections
	Q_strncpy( comment, BF_ReadString( &buf ), CS_SIZE );	// mapname
	Q_strncpy( maptitle, BF_ReadString( &buf ), MAX_STRING );	// maptitle
	if( !Q_strlen( maptitle )) Q_strncpy( maptitle, "<no title>", MAX_STRING );
	Q_strncpy( comment + CS_SIZE, maptitle, CS_SIZE );
	Q_strncpy( comment + CS_SIZE * 2, va( "%i", maxClients ), CS_TIME );

	// all done
	FS_Close( demfile );
		
	return true;
}

/* 
================== 
CL_DemoGetName
================== 
*/  
void CL_DemoGetName( int lastnum, char *filename )
{
	int	a, b;

	if( !filename ) return;
	if( lastnum < 0 || lastnum > 99 )
	{
		// bound
		Q_strcpy( filename, "demo99" );
		return;
	}

	a = lastnum / 10;
	b = lastnum % 10;

	Q_sprintf( filename, "demo%i%i", a, b );
}

/*
====================
CL_Record_f

record <demoname>
Begins recording a demo from the current position
====================
*/
void CL_Record_f( void )
{
	const char	*name;
	string		demoname, demopath, demoshot;
	int		n;

	if( Cmd_Argc() == 1 )
		name = "new";
	else if( Cmd_Argc() == 2 )
		name = Cmd_Argv( 1 );
	else
	{
		Msg( "Usage: record <demoname>\n" );
		return;
	}

	if( cls.demorecording )
	{
		Msg("CL_Record: already recording.\n");
		return;
	}

	if( cls.state != ca_active )
	{
		Msg ("You must be in a level to record.\n");
		return;
	}

	if( !Q_stricmp( name, "new" ))
	{
		// scan for a free filename
		for( n = 0; n < 100; n++ )
		{
			CL_DemoGetName( n, demoname );
			if( !FS_FileExists( va( "demos/%s.dem", demoname ), false ))
				break;
		}
		if( n == 100 )
		{
			Msg( "^3ERROR: no free slots for demo recording\n" );
			return;
		}
	}
	else Q_strncpy( demoname, name, sizeof( demoname ));

	// open the demo file
	Q_sprintf( demopath, "demos/%s.dem", demoname );
	Q_sprintf( demoshot, "demos/%s.bmp", demoname );

	// make sure what old demo is removed
	if( FS_FileExists( demopath, false )) FS_Delete( demopath );
	if( FS_FileExists( demoshot, false )) FS_Delete( demoshot );

	// write demoshot for preview
	Cbuf_AddText( va( "demoshot \"%s\"\n", demoname ));
	Q_strncpy( cls.demoname, demoname, sizeof( cls.demoname ));
	Q_strncpy( menu.globals->demoname, demoname, sizeof( menu.globals->demoname ));
	
	CL_WriteDemoHeader( demopath );
}

/*
====================
CL_PlayDemo_f

playdemo <demoname>
====================
*/
void CL_PlayDemo_f( void )
{
	string	filename;
	string	demoname;

	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: playdemo <demoname>\n" );
		return;
	}

	Q_strncpy( demoname, Cmd_Argv( 1 ), sizeof( demoname ) - 1 );

	// shutdown any game or cinematic server
	CL_Disconnect();
	Host_ShutdownServer();

	Q_snprintf( filename, sizeof( filename ), "demos/%s.dem", demoname );
	if( !FS_FileExists( filename, true ))
	{
		MsgDev( D_ERROR, "couldn't open %s\n", filename );
		cls.demonum = -1; // stop demo loop
		return;
	}

	cls.demofile = FS_Open( filename, "rb", true );
	Q_strncpy( cls.demoname, demoname, sizeof( cls.demoname ));
	Q_strncpy( menu.globals->demoname, demoname, sizeof( menu.globals->demoname ));

	Con_Close();
	UI_SetActiveMenu( false );

	cls.demoplayback = true;
	cls.state = ca_connected;
	Q_strncpy( cls.servername, demoname, sizeof( cls.servername ));

	// begin a playback demo
}

/*
==================
CL_StartDemos_f
==================
*/
void CL_StartDemos_f( void )
{
	int	i, c;

	c = Cmd_Argc() - 1;
	if( c > MAX_DEMOS )
	{
		MsgDev( D_WARN, "Host_StartDemos: max %i demos in demoloop\n", MAX_DEMOS );
		c = MAX_DEMOS;
	}

	MsgDev( D_INFO, "%i demo%s in loop\n", c, (c > 1) ? "s" : "" );

	for( i = 1; i < c + 1; i++ )
		Q_strncpy( cls.demos[i-1], Cmd_Argv( i ), sizeof( cls.demos[0] ));

	if( !SV_Active() && cls.demonum != -1 && !cls.demoplayback )
	{
		cls.demonum = 0;
		CL_NextDemo ();
	}
	else cls.demonum = -1;
}

/*
==================
CL_Demos_f

Return to looping demos
==================
*/
void CL_Demos_f( void )
{
	if( cls.demonum == -1 )
		cls.demonum = 1;

	CL_Disconnect ();
	CL_NextDemo ();
}


/*
====================
CL_Stop_f

stop any client activity
====================
*/
void CL_Stop_f( void )
{
	// stop all
	CL_StopRecord();
	CL_StopPlayback();
	SCR_StopCinematic();
	S_StopBackgroundTrack();
}