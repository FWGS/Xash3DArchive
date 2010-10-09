//=======================================================================
//			Copyright XashXT Group 2007 �
//		       cl_demo.c - demo record & playback
//=======================================================================

#include "common.h"
#include "client.h"
#include "byteorder.h"
#include "protocol.h"
#include "net_encode.h"

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length
====================
*/
void CL_WriteDemoMessage( sizebuf_t *msg, int head_size )
{
	int len, swlen;

	if( !cls.demofile ) return;
	if( cl.refdef.paused || cls.key_dest == key_menu )
		return;

	// the first eight bytes are just packet sequencing stuff
	len = BF_GetNumBytesWritten( msg ) - head_size;
	swlen = LittleLong( len );

	if( !swlen ) return; // ignore null messages
	FS_Write( cls.demofile, &swlen, 4 );
	FS_Write( cls.demofile, BF_GetData( msg ) + head_size, len );
}

void CL_WriteDemoHeader( const char *name )
{
	int		i, j, len;
	char		buf_data[MAX_MSGLEN];
	entity_state_t	*state, nullstate;
	movevars_t	nullmovevars;
	sizebuf_t		buf;
	delta_info_t	*dt;

	MsgDev( D_INFO, "recording to %s.\n", name );
	cls.demofile = FS_Open( name, "wb" );
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
	BF_WriteByte( &buf, cl.playernum );
	BF_WriteByte( &buf, cl.maxclients );
	BF_WriteWord( &buf, clgame.maxEntities );
	BF_WriteString( &buf, cl.configstrings[CS_NAME] );
	BF_WriteString( &buf, clgame.maptitle );

	// configstrings
	for( i = 0; i < MAX_CONFIGSTRINGS; i++ )
	{
		if( cl.configstrings[i][0] )
		{
			BF_WriteByte( &buf, svc_configstring );
			BF_WriteShort( &buf, i );
			BF_WriteString( &buf, cl.configstrings[i] );

			if( BF_GetNumBytesWritten( &buf ) > ( BF_GetMaxBytes( &buf ) / 2 ))
			{	
				// write it out
				len = LittleLong( BF_GetNumBytesWritten( &buf ));
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
			BF_WriteString( &buf, clgame.msg[i].name );
			BF_WriteByte( &buf, clgame.msg[i].number );
			BF_WriteByte( &buf, (byte)clgame.msg[i].size );

			if( BF_GetNumBytesWritten( &buf ) > ( BF_GetMaxBytes( &buf ) / 2 ))
			{	
				// write it out
				len = LittleLong( BF_GetNumBytesWritten( &buf ));
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
				len = LittleLong( BF_GetNumBytesWritten( &buf ));
				FS_Write( cls.demofile, &len, 4 );
				FS_Write( cls.demofile, BF_GetData( &buf ), len );
				BF_Clear( &buf );
			}
		}
	}

	// baselines
	Mem_Set( &nullstate, 0, sizeof( nullstate ));
	Mem_Set( &nullmovevars, 0, sizeof( nullmovevars ));

	// FIXME: use clgame.numEntities instead ?
	for( i = 0; i < clgame.maxEntities; i++ )
	{
		state = &clgame.entities[i].baseline;
		if( !state->modelindex || state->effects == EF_NODRAW )
			continue;

		BF_WriteByte( &buf, svc_spawnbaseline );		
		MSG_WriteDeltaEntity( &nullstate, state, &buf, true, true );

		if( BF_GetNumBytesWritten( &buf ) > ( BF_GetMaxBytes( &buf ) / 2 ))
		{	
			// write it out
			len = LittleLong( BF_GetNumBytesWritten( &buf ));
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
		BF_WriteByte( &buf, i );
		pi = &cl.players[i];

		if( pi->name[0] )
		{
			BF_WriteByte( &buf, true );
			BF_WriteString( &buf, pi->userinfo );
		}
		else BF_WriteByte( &buf, false );
	}

	MSG_WriteDeltaMovevars( &buf, &nullmovevars, &clgame.movevars );

	// write it to the demo file
	len = LittleLong( BF_GetNumBytesWritten( &buf ));
	FS_Write( cls.demofile, &len, 4 );
	FS_Write( cls.demofile, BF_GetData( &buf ), len );

	// force client.dll update
	Cmd_ExecuteString( "cmd fullupdate\n" );
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
	com.snprintf( string, sizeof( string ), "RECORDING %s: %ik", cls.demoname, pos / 1024 );

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
bool CL_NextDemo( void )
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

	com.snprintf( str, MAX_STRING, "playdemo %s\n", cls.demos[cls.demonum] );

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
	char	buf_data[MAX_MSGLEN];
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

	curSize = LittleLong( curSize );
	if( curSize == -1 )
	{
		CL_DemoCompleted();
		return;
	}

	if( curSize > sizeof( buf_data ))
	{
		Host_Error( "CL_ReadDemoMessage: demoMsglen > MAX_MSGLEN\n" );
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
	gameui.globals->demoname[0] = '\0';

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
	gameui.globals->demoname[0] = '\0';
}

/* 
================== 
CL_GetComment
================== 
*/  
bool CL_GetComment( const char *demoname, char *comment )
{
	file_t	*demfile;
	char     	buf_data[MAX_MSGLEN];
	int	r, maxClients, curSize;
	string	maptitle;
	sizebuf_t	buf;
	
	if( !comment ) return false;

	demfile = FS_Open( demoname, "rb" );
	if( !demfile )
	{
		com.strncpy( comment, "", MAX_STRING );
		return false;
          }

	// read the first demo packet. extract info from it
	// get the length
	r = FS_Read( demfile, &curSize, 4 );
	if( r != 4 )
	{
		FS_Close( demfile );
		com.strncpy( comment, "<corrupted>", MAX_STRING );
		return false;
	}

	curSize = LittleLong( curSize );

	if( curSize == -1 )
	{
		FS_Close( demfile );
		com.strncpy( comment, "<corrupted>", MAX_STRING );
		return false;
	}

	if( curSize > sizeof( buf_data ))
	{
		FS_Close( demfile );
		com.strncpy( comment, "<not compatible>", MAX_STRING );
		return false;
	}

	// init the message (set maxsize to cursize so overflow check will be working properly)
	BF_Init( &buf, "DemoRead", buf_data, curSize );
	r = FS_Read( demfile, buf.pData, curSize );

	if( r != curSize )
	{
		FS_Close( demfile );
		com.strncpy( comment, "<truncated file>", MAX_STRING );
		return false;
	}

	// skip server data ident
	BF_ReadByte( &buf );

	if( PROTOCOL_VERSION != BF_ReadLong( &buf ))
	{
		FS_Close( demfile );
		com.strncpy( comment, "<invalid protocol>", MAX_STRING );
		return false;
	}

	BF_ReadLong( &buf ); // server count
	BF_ReadByte( &buf );// playernum
	maxClients = BF_ReadByte( &buf );
	if( BF_ReadWord( &buf ) > GI->max_edicts )
	{
		FS_Close( demfile );
		com.strncpy( comment, "<too many edicts>", MAX_STRING );
		return false;
	}

	// split comment to sections
	com.strncpy( comment, BF_ReadString( &buf ), CS_SIZE );	// mapname
	com.strncpy( maptitle, BF_ReadString( &buf ), MAX_STRING );	// maptitle
	if( !com.strlen( maptitle )) com.strncpy( maptitle, "<no title>", MAX_STRING );
	com.strncpy( comment + CS_SIZE, maptitle, CS_SIZE );
	com.strncpy( comment + CS_SIZE * 2, va( "%i", maxClients ), CS_TIME );

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
		com.strcpy( filename, "demo99" );
		return;
	}

	a = lastnum / 10;
	b = lastnum % 10;

	com.sprintf( filename, "demo%i%i", a, b );
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

	if( !com.stricmp( name, "new" ))
	{
		// scan for a free filename
		for( n = 0; n < 100; n++ )
		{
			CL_DemoGetName( n, demoname );
			if( !FS_FileExists( va( "demos/%s.dem", demoname )))
				break;
		}
		if( n == 100 )
		{
			Msg( "^3ERROR: no free slots for demo recording\n" );
			return;
		}
	}
	else com.strncpy( demoname, name, sizeof( name ));

	// open the demo file
	com.sprintf( demopath, "demos/%s.dem", demoname );
	com.sprintf( demoshot, "demos/%s.%s", demoname, SI->savshot_ext );

	// make sure what old demo is removed
	if( FS_FileExists( demopath )) FS_Delete( demopath );
	if( FS_FileExists( demoshot )) FS_Delete( demoshot );

	// write demoshot for preview
	Cbuf_AddText( va( "demoshot \"%s\"\n", demoname ));
	com.strncpy( cls.demoname, demoname, sizeof( cls.demoname ));
	com.strncpy( gameui.globals->demoname, demoname, sizeof( gameui.globals->demoname ));
	
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

	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: playdemo <demoname>\n" );
		return;
	}

	// shutdown any game or cinematic server
	CL_Disconnect();
	Host_ShutdownServer();

	com.snprintf( filename, sizeof( filename ), "demos/%s.dem", Cmd_Argv( 1 ));
	if( !FS_FileExistsEx( filename, true ))
	{
		MsgDev( D_ERROR, "couldn't open %s\n", filename );
		cls.demonum = -1; // stop demo loop
		return;
	}

	cls.demofile = FS_OpenEx( filename, "rb", true );
	com.strncpy( cls.demoname, Cmd_Argv( 1 ), sizeof( cls.demoname ));
	com.strncpy( gameui.globals->demoname, Cmd_Argv( 1 ), sizeof( gameui.globals->demoname ));

	Con_Close();
	UI_SetActiveMenu( false );

	cls.demoplayback = true;
	cls.state = ca_connected;
	com.strncpy( cls.servername, Cmd_Argv( 1 ), sizeof( cls.servername ));

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
		com.strncpy( cls.demos[i-1], Cmd_Argv( i ), sizeof( cls.demos[0] ));

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
}