/*
sv_cmds.c - server console commands
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
#include "server.h"

/*
=================
SV_ClientPrintf

Sends text across to be displayed if the level passes
=================
*/
void SV_ClientPrintf( sv_client_t *cl, int level, char *fmt, ... )
{
	char	string[MAX_SYSPATH];
	va_list	argptr;

	if( level < cl->messagelevel || FBitSet( cl->flags, FCL_FAKECLIENT ))
		return;
	
	va_start( argptr, fmt );
	Q_vsprintf( string, fmt, argptr );
	va_end( argptr );
	
	MSG_BeginServerCmd( &cl->netchan.message, svc_print );
	MSG_WriteByte( &cl->netchan.message, level );
	MSG_WriteString( &cl->netchan.message, string );
}

/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void SV_BroadcastPrintf( sv_client_t *ignore, int level, char *fmt, ... )
{
	char		string[MAX_SYSPATH];
	va_list		argptr;
	sv_client_t	*cl;
	int		i;

	if( sv.state == ss_dead )
		return;

	va_start( argptr, fmt );
	Q_vsprintf( string, fmt, argptr );
	va_end( argptr );
	
	// echo to console
	if( host.type == HOST_DEDICATED ) Msg( "%s", string );

	for( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		if( level < cl->messagelevel || FBitSet( cl->flags, FCL_FAKECLIENT ))
			continue;

		if( cl == ignore || cl->state != cs_spawned )
			continue;

		MSG_BeginServerCmd( &cl->netchan.message, svc_print );
		MSG_WriteByte( &cl->netchan.message, level );
		MSG_WriteString( &cl->netchan.message, string );
	}
}

/*
=================
SV_BroadcastCommand

Sends text to all active clients
=================
*/
void SV_BroadcastCommand( const char *fmt, ... )
{
	char	string[MAX_SYSPATH];
	va_list	argptr;	

	if( sv.state == ss_dead )
		return;

	va_start( argptr, fmt );
	Q_vsprintf( string, fmt, argptr );
	va_end( argptr );

	MSG_BeginServerCmd( &sv.reliable_datagram, svc_stufftext );
	MSG_WriteString( &sv.reliable_datagram, string );
}

/*
==================
SV_SetPlayer

Sets sv_client and sv_player to the player with idnum Cmd_Argv(1)
==================
*/
qboolean SV_SetPlayer( void )
{
	char		*s;
	sv_client_t	*cl;
	int		i, idnum;

	if( !svs.clients || sv.background )
	{
		Msg( "^3no server running.\n" );
		return false;
          }

	if( svs.maxclients == 1 || Cmd_Argc() < 2 )
	{
		// special case for local client
		svs.currentPlayer = svs.clients;
		svs.currentPlayerNum = 0;
		return true;
	}

	s = Cmd_Argv( 1 );

	// numeric values are just slot numbers
	if( Q_isdigit( s ) || (s[0] == '-' && Q_isdigit( s + 1 )))
	{
		idnum = Q_atoi( s );
		if( idnum < 0 || idnum >= svs.maxclients )
		{
			Msg( "Bad client slot: %i\n", idnum );
			return false;
		}

		svs.currentPlayer = &svs.clients[idnum];
		svs.currentPlayerNum = idnum;

		if( !svs.currentPlayer->state )
		{
			Msg( "Client %i is not active\n", idnum );
			return false;
		}
		return true;
	}

	// check for a name match
	for( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		if( !cl->state ) continue;
		if( !Q_strcmp( cl->name, s ))
		{
			svs.currentPlayer = cl;
			svs.currentPlayerNum = (cl - svs.clients);
			return true;
		}
	}

	Msg( "Userid %s is not on the server\n", s );
	svs.currentPlayer = NULL;
	svs.currentPlayerNum = 0;

	return false;
}

/*
==================
SV_Map_f

Goes directly to a given map without any savegame archiving.
For development work
==================
*/
void SV_Map_f( void )
{
	char	*spawn_entity;
	string	mapname;
	int	flags;

	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: map <mapname>\n" );
		return;
	}

	// hold mapname to other place
	Q_strncpy( mapname, Cmd_Argv( 1 ), sizeof( mapname ));
	FS_StripExtension( mapname );
	
	// determine spawn entity classname
	if( svs.maxclients == 1 )
		spawn_entity = GI->sp_entity;
	else spawn_entity = GI->mp_entity;

	flags = SV_MapIsValid( mapname, spawn_entity, NULL );

	if( FBitSet( flags, MAP_INVALID_VERSION ))
	{
		MsgDev( D_ERROR, "map %s is invalid or not supported\n", mapname );
		return;
	}

	if( !FBitSet( flags, MAP_IS_EXIST ))
	{
		MsgDev( D_ERROR, "map %s doesn't exist\n", mapname );
		return;
	}

	if( !FBitSet( flags, MAP_HAS_SPAWNPOINT ))
	{
		MsgDev( D_ERROR, "map %s doesn't have a valid spawnpoint\n", mapname );
		return;
	}

	// changing singleplayer to multiplayer or back. refresh the player count
	if( FBitSet( sv_maxclients->flags, FCVAR_CHANGED ))
		Host_ShutdownServer();

	SCR_BeginLoadingPlaque( false );

	sv.changelevel = false;
	sv.background = false;
	sv.loadgame = false; // set right state
	SV_ClearSaveDir ();	// delete all temporary *.hl files

	Cvar_DirectSet( sv_hostmap, mapname );

	SV_DeactivateServer();
	SV_SpawnServer( mapname, NULL );
	SV_LevelInit( mapname, NULL, NULL, false );
	SV_ActivateServer ();
}

/*
==================
SV_MapBackground_f

Set background map (enable physics in menu)
==================
*/
void SV_MapBackground_f( void )
{
	string	mapname;
	int	flags;

	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: map_background <mapname>\n" );
		return;
	}

	if( sv.state == ss_active && !sv.background )
	{
		MsgDev( D_ERROR, "can't set background map while game is active\n" );
		return;
	}

	// hold mapname to other place
	Q_strncpy( mapname, Cmd_Argv( 1 ), sizeof( mapname ));
	FS_StripExtension( mapname );

	flags = SV_MapIsValid( mapname, GI->sp_entity, NULL );

	if( FBitSet( flags, MAP_INVALID_VERSION ))
	{
		MsgDev( D_ERROR, "map %s is invalid or not supported\n", mapname );
		return;
	}

	if( !FBitSet( flags, MAP_IS_EXIST ))
	{
		MsgDev( D_ERROR, "map %s doesn't exist\n", mapname );
		return;
	}

	// background maps allow without spawnpoints (just throw warning)
	if( !FBitSet( flags, MAP_HAS_SPAWNPOINT ))
		MsgDev( D_WARN, "map %s doesn't have a valid spawnpoint\n", mapname );
		
	Q_strncpy( host.finalmsg, "", MAX_STRING );
	SV_Shutdown( true );
	NET_Config ( false ); // close network sockets

	sv.changelevel = false;
	sv.background = true;
	sv.loadgame = false; // set right state

	// reset all multiplayer cvars
	Cvar_FullSet( "maxplayers", "1", FCVAR_LATCH );
	Cvar_SetValue( "deathmatch", 0 );
	Cvar_SetValue( "coop", 0 );

	SCR_BeginLoadingPlaque( true );

	SV_SpawnServer( mapname, NULL );
	SV_LevelInit( mapname, NULL, NULL, false );
	SV_ActivateServer ();
}

/*
==============
SV_NewGame_f

==============
*/
void SV_NewGame_f( void )
{
	if( Cmd_Argc() != 1 )
	{
		Msg( "Usage: newgame\n" );
		return;
	}

	Host_NewGame( GI->startmap, false );
}

/*
==============
SV_HazardCourse_f

==============
*/
void SV_HazardCourse_f( void )
{
	if( Cmd_Argc() != 1 )
	{
		Msg( "Usage: hazardcourse\n" );
		return;
	}

	// special case for Gunman Chronicles: playing avi-file
	if( FS_FileExists( va( "media/%s.avi", GI->trainmap ), false ))
	{
		Cbuf_AddText( va( "wait; movie %s\n", GI->trainmap ));
		Host_EndGame( "The End" );
	}
	else Host_NewGame( GI->trainmap, false );
}

/*
==============
SV_Load_f

==============
*/
void SV_Load_f( void )
{
	string	path;

	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: load <savename>\n" );
		return;
	}

	Q_snprintf( path, sizeof( path ), "save/%s.sav", Cmd_Argv( 1 ));
	SV_LoadGame( path );
}

/*
==============
SV_QuickLoad_f

==============
*/
void SV_QuickLoad_f( void )
{
	Cbuf_AddText( "echo Quick Loading...; wait; load quick" );
}

/*
==============
SV_Save_f

==============
*/
void SV_Save_f( void )
{
	const char *name;

	switch( Cmd_Argc() )
	{
	case 1: name = "new"; break;
	case 2: name = Cmd_Argv( 1 ); break;
	default:
		Msg( "Usage: save <savename>\n" );
		return;
	}

	SV_SaveGame( name );
}

/*
==============
SV_QuickSave_f

==============
*/
void SV_QuickSave_f( void )
{
	Cbuf_AddText( "echo Quick Saving...; wait; save quick" );
}

/*
==============
SV_DeleteSave_f

==============
*/
void SV_DeleteSave_f( void )
{
	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: killsave <name>\n" );
		return;
	}

	// delete save and saveshot
	FS_Delete( va( "save/%s.sav", Cmd_Argv( 1 )));
	FS_Delete( va( "save/%s.bmp", Cmd_Argv( 1 )));
}

/*
==============
SV_AutoSave_f

==============
*/
void SV_AutoSave_f( void )
{
	if( Cmd_Argc() != 1 )
	{
		Msg( "Usage: autosave\n" );
		return;
	}

	SV_SaveGame( "autosave" );
}

/*
==================
SV_ChangeLevel_f

Saves the state of the map just being exited and goes to a new map.
==================
*/
void SV_ChangeLevel_f( void )
{
	string	mapname;
	char	*spawn_entity;
	int	flags, c = Cmd_Argc();

	if( c < 2 )
	{
		Msg( "Usage: changelevel <map> [landmark]\n" );
		return;
	}

	// hold mapname to other place
	Q_strncpy( mapname, Cmd_Argv( 1 ), sizeof( mapname ));
	FS_StripExtension( mapname );

	// determine spawn entity classname
	if( svs.maxclients == 1 )
		spawn_entity = GI->sp_entity;
	else spawn_entity = GI->mp_entity;

	flags = SV_MapIsValid( mapname, spawn_entity, Cmd_Argv( 2 ));

	if( FBitSet( flags, MAP_INVALID_VERSION ))
	{
		MsgDev( D_ERROR, "map %s is invalid or not supported\n", mapname );
		return;
	}
	
	if( !FBitSet( flags, MAP_IS_EXIST ))
	{
		MsgDev( D_ERROR, "map %s doesn't exist\n", mapname );
		return;
	}

	if( c >= 3 && !FBitSet( flags, MAP_HAS_LANDMARK ))
	{
		if( sv_validate_changelevel->value )
		{
			// NOTE: we find valid map but specified landmark it's doesn't exist
			// run simple changelevel like in q1, throw warning
			MsgDev( D_WARN, "map %s is exist but doesn't contain landmark with name %s. smooth transition disabled\n",
			mapname, Cmd_Argv( 2 ));
			c = 2; // reduce args
		}
	}

	if( c >= 3 && !Q_stricmp( sv.name, Cmd_Argv( 1 )))
	{
		MsgDev( D_ERROR, "can't changelevel with same map. Ignored.\n" );
		return;	
	}

	if( c == 2 && !FBitSet( flags, MAP_HAS_SPAWNPOINT ))
	{
		if( sv_validate_changelevel->value )
		{
			MsgDev( D_ERROR, "map %s doesn't have a valid spawnpoint. Ignored.\n", mapname );
			return;	
		}
	}

	// bad changelevel position invoke enables in one-way transition
	if( sv.net_framenum < 15 )
	{
		if( sv_validate_changelevel->value )
		{
			MsgDev( D_WARN, "an infinite changelevel detected and will be disabled until a next save\\restore\n" );
			return; // lock with svs.spawncount here
		}
	}

	if( sv.state != ss_active )
	{
		MsgDev( D_ERROR, "only the server may changelevel\n" );
		return;
	}

	SCR_BeginLoadingPlaque( false );

	if( sv.background )
	{
		// just load map
		Cbuf_AddText( va( "map %s\n", mapname ));
		return;
	}

	if( c == 2 ) SV_ChangeLevel( false, Cmd_Argv( 1 ), NULL );
	else SV_ChangeLevel( true, Cmd_Argv( 1 ), Cmd_Argv( 2 ));
}

/*
==================
SV_Restart_f

restarts current level
==================
*/
void SV_Restart_f( void )
{
	if( sv.state != ss_active )
		return;

	// just sending console command
	if( sv.background )
	{
		Cbuf_AddText( va( "map_background %s\n", sv.name ));
	}
	else
	{
		Cbuf_AddText( va( "map %s\n", sv.name ));
	}
}

/*
==================
SV_Reload_f

continue from latest savedgame
==================
*/
void SV_Reload_f( void )
{
	const char	*savefile;

	if( sv.state != ss_active || sv.background )
		return;

	savefile = SV_GetLatestSave();

	if( savefile ) SV_LoadGame( savefile );
	else SV_NewGame( sv_hostmap->string, false );
}

/*
==================
SV_Kick_f

Kick a user off of the server
==================
*/
void SV_Kick_f( void )
{
	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: kick <userid> | <name>\n" );
		return;
	}

	if( !SV_SetPlayer( )) return;

	if( NET_IsLocalAddress( svs.currentPlayer->netchan.remote_address ))
	{
		Msg( "The local player cannot be kicked!\n" );
		return;
	}

	Log_Printf( "Kick: \"%s<%i>\" was kicked\n", svs.currentPlayer->name, svs.currentPlayer->userid );
	SV_BroadcastPrintf( svs.currentPlayer, PRINT_HIGH, "%s was kicked\n", svs.currentPlayer->name );
	SV_ClientPrintf( svs.currentPlayer, PRINT_HIGH, "You were kicked from the game\n" );
	SV_DropClient( svs.currentPlayer );
}

/*
==================
SV_Kill_f
==================
*/
void SV_Kill_f( void )
{
	if( !SV_SetPlayer( )) return;

	if( !svs.currentPlayer || !SV_IsValidEdict( svs.currentPlayer->edict ))
		return;

	if( svs.currentPlayer->edict->v.health <= 0.0f )
	{
		SV_ClientPrintf( svs.currentPlayer, PRINT_HIGH, "Can't suicide - already dead!\n");
		return;
	}

	svgame.dllFuncs.pfnClientKill( svs.currentPlayer->edict );	
}

/*
==================
SV_EntPatch_f
==================
*/
void SV_EntPatch_f( void )
{
	const char	*mapname;

	if( Cmd_Argc() < 2 )
	{
		if( sv.state != ss_dead )
		{
			mapname = sv.name;
		}
		else
		{
			Msg( "Usage: entpatch <mapname>\n" );
			return;
		}
	}
	else mapname = Cmd_Argv( 1 );

	SV_WriteEntityPatch( mapname );
}

/*
================
SV_Status_f
================
*/
void SV_Status_f( void )
{
	sv_client_t	*cl;
	int		i;

	if( !svs.clients || sv.background )
	{
		Msg( "^3no server running.\n" );
		return;
	}

	Msg( "map: %s\n", sv.name );
	Msg( "num score ping    name            lastmsg address               port \n" );
	Msg( "--- ----- ------- --------------- ------- --------------------- ------\n" );

	for( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		int	j, l;
		char	*s;

		if( !cl->state ) continue;

		Msg( "%3i ", i );
		Msg( "%5i ", (int)cl->edict->v.frags );

		if( cl->state == cs_connected ) Msg( "Connect" );
		else if( cl->state == cs_zombie ) Msg( "Zombie " );
		else if( FBitSet( cl->flags, FCL_FAKECLIENT )) Msg( "Bot   " );
		else Msg( "%7i ", SV_CalcPing( cl ));

		Msg( "%s", cl->name );
		l = 24 - Q_strlen( cl->name );
		for( j = 0; j < l; j++ ) Msg( " " );
		Msg( "%g ", ( host.realtime - cl->netchan.last_received ));
		s = NET_BaseAdrToString( cl->netchan.remote_address );
		Msg( "%s", s );
		l = 22 - Q_strlen( s );
		for( j = 0; j < l; j++ ) Msg( " " );
		Msg( "%5i", cl->netchan.qport );
		Msg( "\n" );
	}
	Msg( "\n" );
}

/*
==================
SV_ConSay_f
==================
*/
void SV_ConSay_f( void )
{
	char		*p, text[MAX_SYSPATH];
	sv_client_t	*client;
	int		i;

	if( Cmd_Argc() < 2 ) return;

	if( !svs.clients || sv.background )
	{
		Msg( "^3no server running.\n" );
		return;
	}

	Q_snprintf( text, sizeof( text ), "%s: ", Cvar_VariableString( "hostname" ));
	p = Cmd_Args();

	if( *p == '"' )
	{
		p++;
		p[Q_strlen(p) - 1] = 0;
	}

	Q_strncat( text, p, MAX_SYSPATH );

	for( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
	{
		if( client->state != cs_spawned )
			continue;

		SV_ClientPrintf( client, PRINT_CHAT, "%s\n", text );
	}
	Log_Printf( "Server say: \"%s\"\n", p );
}

/*
==================
SV_Heartbeat_f
==================
*/
void SV_Heartbeat_f( void )
{
	svs.last_heartbeat = MAX_HEARTBEAT;
}

/*
===========
SV_ServerInfo_f

Examine or change the serverinfo string
===========
*/
void SV_ServerInfo_f( void )
{
	convar_t	*var;

	if( Cmd_Argc() == 1 )
	{
		Msg( "Server info settings:\n" );
		Info_Print( svs.serverinfo );
		Msg( "Total %i symbols\n", Q_strlen( svs.serverinfo ));
		return;
	}

	if( Cmd_Argc() != 3 )
	{
		Msg( "Usage: serverinfo [ <key> <value> ]\n");
		return;
	}

	if( Cmd_Argv(1)[0] == '*' )
	{
		Msg( "Star variables cannot be changed.\n" );
		return;
	}

	// if this is a cvar, change it too	
	var = Cvar_FindVar( Cmd_Argv( 1 ));
	if( var )
	{
		freestring( var->string ); // free the old value string	
		var->string = copystring( Cmd_Argv( 2 ));
		var->value = Q_atof( var->string );
	}

	Info_SetValueForStarKey( svs.serverinfo, Cmd_Argv( 1 ), Cmd_Argv( 2 ), MAX_SERVERINFO_STRING );
	SV_BroadcastCommand( "fullserverinfo \"%s\"\n", SV_Serverinfo( ));
}

/*
===========
SV_LocalInfo_f

Examine or change the localinfo string
===========
*/
void SV_LocalInfo_f( void )
{
	if( Cmd_Argc() == 1 )
	{
		Msg( "Local info settings:\n" );
		Info_Print( svs.localinfo );
		Msg( "Total %i symbols\n", Q_strlen( svs.localinfo ));
		return;
	}

	if( Cmd_Argc() != 3 )
	{
		Msg( "Usage: localinfo [ <key> <value> ]\n");
		return;
	}

	if( Cmd_Argv(1)[0] == '*' )
	{
		Msg( "Star variables cannot be changed.\n" );
		return;
	}

	Info_SetValueForStarKey( svs.localinfo, Cmd_Argv(1), Cmd_Argv(2), MAX_LOCALINFO_STRING );
}

/*
===========
SV_ClientInfo_f

Examine all a users info strings
===========
*/
void SV_ClientInfo_f( void )
{
	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: clientinfo <userid>\n" );
		return;
	}

	if( !SV_SetPlayer( )) return;
	Msg( "userinfo\n" );
	Msg( "--------\n" );
	Info_Print( svs.currentPlayer->userinfo );

}

/*
===============
SV_KillServer_f

Kick everyone off, possibly in preparation for a new game
===============
*/
void SV_KillServer_f( void )
{
	if( !svs.initialized )
		return;

	Q_strncpy( host.finalmsg, "Server was killed", MAX_STRING );
	SV_Shutdown( false );
	NET_Config ( false ); // close network sockets
}

/*
===============
SV_PlayersOnly_f

disable plhysics but players
===============
*/
void SV_PlayersOnly_f( void )
{
	if( !Cvar_VariableInteger( "sv_cheats" )) return;
	sv.hostflags = sv.hostflags ^ SVF_PLAYERSONLY;

	if( !FBitSet( sv.hostflags, SVF_PLAYERSONLY ))
		SV_BroadcastPrintf( NULL, PRINT_HIGH, "Resume server physic\n" );
	else SV_BroadcastPrintf( NULL, PRINT_HIGH, "Freeze server physic\n" );
}

/*
===============
SV_EdictUsage_f

===============
*/
void SV_EdictUsage_f( void )
{
	int	active;

	if( sv.state != ss_active )
	{
		Msg( "^3no server running.\n" );
		return;
	}

	active = pfnNumberOfEntities(); 
	Msg( "%5i edicts is used\n", active );
	Msg( "%5i edicts is free\n", svgame.globals->maxEntities - active );
	Msg( "%5i total\n", svgame.globals->maxEntities );
}

/*
===============
SV_EntityInfo_f

===============
*/
void SV_EntityInfo_f( void )
{
	edict_t	*ent;
	int	i;

	if( sv.state != ss_active )
	{
		Msg( "^3no server running.\n" );
		return;
	}

	for( i = 0; i < svgame.numEntities; i++ )
	{
		ent = EDICT_NUM( i );
		if( !SV_IsValidEdict( ent )) continue;

		Msg( "%5i origin: %.f %.f %.f", i, ent->v.origin[0], ent->v.origin[1], ent->v.origin[2] );

		if( ent->v.classname )
			Msg( ", class: %s", STRING( ent->v.classname ));

		if( ent->v.globalname )
			Msg( ", global: %s", STRING( ent->v.globalname ));

		if( ent->v.targetname )
			Msg( ", name: %s", STRING( ent->v.targetname ));

		if( ent->v.target )
			Msg( ", target: %s", STRING( ent->v.target ));

		if( ent->v.model )
			Msg( ", model: %s", STRING( ent->v.model ));

		Msg( "\n" );
	}
}
/*
==================
SV_InitHostCommands

commands that create server
is available always
==================
*/
void SV_InitHostCommands( void )
{
	Cmd_AddCommand( "map", SV_Map_f, "start new level" );
	Cmd_AddCommand( "changelevel", SV_ChangeLevel_f, "changing level" );

	if( host.type == HOST_NORMAL )
	{
		Cmd_AddCommand( "newgame", SV_NewGame_f, "begin new game" );
		Cmd_AddCommand( "hazardcourse", SV_HazardCourse_f, "starting a Hazard Course" );
		Cmd_AddCommand( "map_background", SV_MapBackground_f, "set background map" );
		Cmd_AddCommand( "load", SV_Load_f, "load a saved game file" );
		Cmd_AddCommand( "loadquick", SV_QuickLoad_f, "load a quick-saved game file" );
		Cmd_AddCommand( "killsave", SV_DeleteSave_f, "delete a saved game file and saveshot" );
	}
}

/*
==================
SV_InitOperatorCommands
==================
*/
void SV_InitOperatorCommands( void )
{
	Cmd_AddCommand( "heartbeat", SV_Heartbeat_f, "send a heartbeat to the master server" );
	Cmd_AddCommand( "kick", SV_Kick_f, "kick a player off the server by number or name" );
	Cmd_AddCommand( "kill", SV_Kill_f, "die instantly" );
	Cmd_AddCommand( "status", SV_Status_f, "print server status information" );
	Cmd_AddCommand( "localinfo", SV_LocalInfo_f, "examine or change the localinfo string" );
	Cmd_AddCommand( "serverinfo", SV_ServerInfo_f, "examine or change the serverinfo string" );
	Cmd_AddCommand( "clientinfo", SV_ClientInfo_f, "print user infostring (player num required)" );
	Cmd_AddCommand( "playersonly", SV_PlayersOnly_f, "freezes time, except for players" );
	Cmd_AddCommand( "restart", SV_Restart_f, "restarting current level" );
	Cmd_AddCommand( "reload", SV_Reload_f, "continue from latest save or restart level" );
	Cmd_AddCommand( "entpatch", SV_EntPatch_f, "write entity patch to allow external editing" );
	Cmd_AddCommand( "edict_usage", SV_EdictUsage_f, "show info about edicts usage" );
	Cmd_AddCommand( "entity_info", SV_EntityInfo_f, "show more info about edicts" );

	if( host.type == HOST_NORMAL )
	{
		Cmd_AddCommand( "save", SV_Save_f, "save the game to a file" );
		Cmd_AddCommand( "savequick", SV_QuickSave_f, "save the game to the quicksave" );
		Cmd_AddCommand( "autosave", SV_AutoSave_f, "save the game to 'autosave' file" );
	}
	else if( host.type == HOST_DEDICATED )
	{
		Cmd_AddCommand( "say", SV_ConSay_f, "send a chat message to everyone on the server" );
		Cmd_AddCommand( "killserver", SV_KillServer_f, "shutdown current server" );
	}
}

/*
==================
SV_KillOperatorCommands
==================
*/
void SV_KillOperatorCommands( void )
{
	Cvar_Reset( "public" );
	Cvar_Reset( "sv_lan" );

	Cmd_RemoveCommand( "heartbeat" );
	Cmd_RemoveCommand( "kick" );
	Cmd_RemoveCommand( "kill" );
	Cmd_RemoveCommand( "status" );
	Cmd_RemoveCommand( "localinfo" );
	Cmd_RemoveCommand( "serverinfo" );
	Cmd_RemoveCommand( "clientinfo" );
	Cmd_RemoveCommand( "playersonly" );
	Cmd_RemoveCommand( "restart" );
	Cmd_RemoveCommand( "reload" );
	Cmd_RemoveCommand( "entpatch" );
	Cmd_RemoveCommand( "edict_usage" );
	Cmd_RemoveCommand( "entity_info" );

	if( host.type == HOST_NORMAL )
	{
		Cmd_RemoveCommand( "save" );
		Cmd_RemoveCommand( "savequick" );
		Cmd_RemoveCommand( "killsave" );
		Cmd_RemoveCommand( "autosave" );
	}
	else if( host.type == HOST_DEDICATED )
	{
		Cmd_RemoveCommand( "say" );
		Cmd_RemoveCommand( "killserver" );
	}
}