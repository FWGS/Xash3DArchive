/*
sv_init.c - server initialize operations
Copyright (C) 2009 Uncle Mike

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
#include "net_encode.h"

int SV_UPDATE_BACKUP = SINGLEPLAYER_BACKUP;

server_t		sv;	// local server
server_static_t	svs;	// persistant server info
svgame_static_t	svgame;	// persistant game info

const char *check_exts[7] =
{
	".cfg",
	".lst",
	".exe",
	".vbs",
	".com",
	".bat",
	".dll",
};

/*
================
SV_IsGenericFileAllowed

some files is not allowed to be sended to client
================
*/
qboolean SV_IsGenericFileAllowed( const char *filename )
{
	const char	*ext = FS_FileExtension( filename );
	int		i;

	for( i = 0; i < ARRAYSIZE( check_exts ); i++ )
	{
		if( !Q_stricmp( ext, check_exts[i] ))
		{
			MsgDev( D_WARN, "Can't precache %s files:  %s\n", check_exts[i], filename );
			return false;
		}
	}
	return true;
}

/*
================
SV_ModelIndex

register unique model for a server and client
================
*/
int SV_ModelIndex( const char *filename )
{
	char	name[64];
	int	i;

	if( !filename || !filename[0] )
		return 0;

	Q_strncpy( name, filename, sizeof( name ));
	COM_FixSlashes( name );

	for( i = 1; i < MAX_MODELS && sv.model_precache[i][0]; i++ )
	{
		if( !Q_stricmp( sv.model_precache[i], name ))
			return i;
	}

	if( i == MAX_MODELS )
	{
		Host_Error( "SV_ModelIndex: MAX_MODELS limit exceeded\n" );
		return 0;
	}

	// register new model
	Q_strncpy( sv.model_precache[i], name, sizeof( sv.model_precache[i] ));

	if( sv.state != ss_loading )
	{	
		MsgDev( D_WARN, "late precache of %s\n", name );

		// send the update to everyone
		MSG_BeginServerCmd( &sv.reliable_datagram, svc_modelindex );
		MSG_WriteUBitLong( &sv.reliable_datagram, i, MAX_MODEL_BITS );
		MSG_WriteString( &sv.reliable_datagram, name );
	}

	return i;
}

/*
================
SV_SoundIndex

register unique sound for client
================
*/
int SV_SoundIndex( const char *filename )
{
	char	name[64];
	int	i;

	// don't precache sentence names!
	if( !filename || !filename[0] || filename[0] == '!' )
		return 0;

	Q_strncpy( name, filename, sizeof( name ));
	COM_FixSlashes( name );

	for( i = 1; i < MAX_SOUNDS && sv.sound_precache[i][0]; i++ )
	{
		if( !Q_stricmp( sv.sound_precache[i], name ))
			return i;
	}

	if( i == MAX_SOUNDS )
	{
		Host_Error( "SV_SoundIndex: MAX_SOUNDS limit exceeded\n" );
		return 0;
	}

	// register new sound
	Q_strncpy( sv.sound_precache[i], name, sizeof( sv.sound_precache[i] ));

	if( sv.state != ss_loading )
	{	
		MsgDev( D_WARN, "late precache of %s\n", name );

		// send the update to everyone
		MSG_BeginServerCmd( &sv.reliable_datagram, svc_soundindex );
		MSG_WriteUBitLong( &sv.reliable_datagram, i, MAX_SOUND_BITS );
		MSG_WriteString( &sv.reliable_datagram, name );
	}

	return i;
}

/*
================
SV_EventIndex

register network event for a server and client
================
*/
int SV_EventIndex( const char *filename )
{
	char	name[64];
	int	i;

	if( !filename || !filename[0] )
		return 0;

	Q_strncpy( name, filename, sizeof( name ));
	COM_FixSlashes( name );

	for( i = 1; i < MAX_EVENTS && sv.event_precache[i][0]; i++ )
	{
		if( !Q_stricmp( sv.event_precache[i], name ))
			return i;
	}

	if( i == MAX_EVENTS )
	{
		Host_Error( "SV_EventIndex: MAX_EVENTS limit exceeded\n" );
		return 0;
	}

	// register new event
	Q_strncpy( sv.event_precache[i], name, sizeof( sv.event_precache[i] ));

	if( sv.state != ss_loading )
	{
		// send the update to everyone
		MSG_BeginServerCmd( &sv.reliable_datagram, svc_eventindex );
		MSG_WriteUBitLong( &sv.reliable_datagram, i, MAX_EVENT_BITS );
		MSG_WriteString( &sv.reliable_datagram, name );
	}

	return i;
}

/*
================
SV_GenericIndex

register generic resourse for a server and client
================
*/
int SV_GenericIndex( const char *filename )
{
	char	name[64];
	int	i;

	if( !filename || !filename[0] )
		return 0;

	Q_strncpy( name, filename, sizeof( name ));
	COM_FixSlashes( name );

	for( i = 1; i < MAX_CUSTOM && sv.files_precache[i][0]; i++ )
	{
		if( !Q_stricmp( sv.files_precache[i], name ))
			return i;
	}

	if( i == MAX_CUSTOM )
	{
		Host_Error( "SV_GenericIndex: MAX_RESOURCES limit exceeded\n" );
		return 0;
	}

	if( sv.state != ss_loading )
	{
		// g-cont. can we downloading resources in-game ? need testing
		Host_Error( "SV_PrecacheGeneric: ( %s ). Precache can only be done in spawn functions.", name );
		return 0;
	}

	// register new generic resource
	Q_strncpy( sv.files_precache[i], name, sizeof( sv.files_precache[i] ));

	return i;
}

void SV_AddResource( resourcetype_t type, const char *name, int size, byte flags, int index )
{
	resource_t	*pResource = &sv.resources[sv.num_resources];

	if( sv.num_resources >= MAX_RESOURCES )
		Host_Error( "SV_AddResource: MAX_RESOURCES limit exceeded\n" );
	sv.num_resources++;

	Q_strncpy( pResource->szFileName, name, sizeof( pResource->szFileName ));
	pResource->nDownloadSize = size;
	pResource->ucFlags = flags;
	pResource->nIndex = index;
	pResource->type = type;
}

void SV_CreateGenericResources( void )
{
	char	*afile, *pfile;
	string	filename, token;

	Q_strncpy( filename, sv.model_precache[1], sizeof( filename ));
	FS_StripExtension( filename );
	FS_DefaultExtension( filename, ".res" );
	COM_FixSlashes( filename );

	afile = FS_LoadFile( filename, NULL, false );
	if( !afile ) return;

	pfile = afile;

	MsgDev( D_REPORT, "Precaching from %s\n", filename );
	MsgDev( D_REPORT, "----------------------------------\n" );

	while( ( pfile = COM_ParseFile( pfile, token )) != NULL )
	{
		if( Q_strlen( token ) <= 0 )
			break;

		if ( Q_strstr( token, ".." ) )
		{
			Con_Printf( "Can't precache resource with invalid relative path %s\n", token );
			continue;
		}

		if ( Q_strstr( token, ":" ) )
		{
			Con_Printf( "Can't precache resource with absolute path %s\n", token );
			continue;
		}

		if ( Q_strstr( token, "\\" ) )
		{
			Con_Printf( "Can't precache resource with invalid relative path %s\n", token );
			continue;
		}

		if( !SV_IsGenericFileAllowed( token ))
			continue;

		MsgDev( D_REPORT, "  %s\n", token );
		SV_GenericIndex( token );
	}

	MsgDev( D_REPORT, "----------------------------------\n" );
	Mem_Free( afile );
}

void SV_CreateResourceList( void )
{
	int	ffirstsent = 0;
	int	i, nSize;
	char	*s;

	sv.num_resources = 0;

	for( i = 1; i < MAX_CUSTOM; i++ )
	{
		s = sv.files_precache[i];
		if( !*s ) break; // end of list
		nSize = ( svs.maxclients > 1 ) ? FS_FileSize( s, false ) : 0;
		if( nSize < 0 ) nSize = 0;
		SV_AddResource( t_generic, s, nSize, RES_FATALIFMISSING, i );
	}

	for( i = 1; i < MAX_SOUNDS; i++ )
	{
		s = sv.sound_precache[i];
		if( !*s ) break; // end of list

		if( s[0] == '!' )
		{
			if( !ffirstsent )
			{
				SV_AddResource( t_sound, "!", 0, RES_FATALIFMISSING, i );
				ffirstsent = 1;
			}
		}
		else
		{
			nSize = ( svs.maxclients > 1 ) ? FS_FileSize( va( "sound/%s", s ), false ) : 0;
			if( nSize < 0 ) nSize = 0;
			SV_AddResource( t_sound, s, nSize, 0, i );
		}
	}

	for( i = 1; i < MAX_MODELS; i++ )
	{
		s = sv.model_precache[i];
		if( !*s ) break; // end of list
		nSize = ( svs.maxclients > 1 && s[0] != '*' ) ? FS_FileSize( s, false ) : 0;
		if( nSize < 0 ) nSize = 0;
		SV_AddResource( t_model, s, nSize, sv.model_precache_flags[i], i );
	}

	// just send names
	for( i = 0; i < MAX_DECALS && host.draw_decals[i][0]; i++ )
	{
		SV_AddResource( t_decal, host.draw_decals[i], 0, 0, i );
	}

	for( i = 1; i < MAX_EVENTS; i++ )
	{
		s = sv.event_precache[i];
		if( !*s ) break; // end of list
		nSize = ( svs.maxclients > 1 ) ? FS_FileSize( s, false ) : 0;
		if( nSize < 0 ) nSize = 0;
		SV_AddResource( t_eventscript, s, nSize, RES_FATALIFMISSING, i );
	}
}

/*
================
SV_EntityScript

get entity script for current map
================
*/
char *SV_EntityScript( void )
{
	string	entfilename;
	size_t	ft1, ft2;
	char	*ents;

	if( !sv.worldmodel )
		return NULL;

	// check for entfile too
	Q_strncpy( entfilename, sv.worldmodel->name, sizeof( entfilename ));
	FS_StripExtension( entfilename );
	FS_DefaultExtension( entfilename, ".ent" );

	// make sure what entity patch is never than bsp
	ft1 = FS_FileTime( sv.worldmodel->name, false );
	ft2 = FS_FileTime( entfilename, true );

	if( ft2 != -1 )
	{
		if( ft1 > ft2 )
		{
			MsgDev( D_INFO, "^1Entity patch is older than bsp. Ignored.\n", entfilename );			
		}
		else if(( ents = FS_LoadFile( entfilename, NULL, true )) != NULL )
		{
			MsgDev( D_INFO, "^2Read entity patch:^7 %s\n", entfilename );
			return ents;
		}
	}

	// use internal entities
	return sv.worldmodel->entities;
}

/*
================
SV_CreateBaseline

Entity baselines are used to compress the update messages
to the clients -- only the fields that differ from the
baseline will be transmitted

INTERNAL RESOURCE
================
*/
void SV_CreateBaseline( void )
{
	entity_state_t	nullstate, *base;
	int		playermodel;
	qboolean		player;
	int		entnum;

	playermodel = SV_ModelIndex( "models/player.mdl" );
	memset( &nullstate, 0, sizeof( nullstate ));

	for( entnum = 0; entnum < svgame.numEntities; entnum++ )
	{
		edict_t	*pEdict = EDICT_NUM( entnum );

		if( !SV_IsValidEdict( pEdict ))
			continue;

		if( entnum != 0 && entnum <= svs.maxclients )
		{
			player = true;
		}
		else
		{
			if( !pEdict->v.modelindex )
				continue; // invisible
			player = false;
		}

		// take current state as baseline
		base = &svs.baselines[entnum];

		base->number = entnum;

		// set entity type
		if( FBitSet( pEdict->v.flags, FL_CUSTOMENTITY ))
			base->entityType = ENTITY_BEAM;
		else base->entityType = ENTITY_NORMAL;

		svgame.dllFuncs.pfnCreateBaseline( player, entnum, base, pEdict, playermodel, host.player_mins[0], host.player_maxs[0] );
		sv.last_valid_baseline = entnum;
	}

	// create the instanced baselines
	svgame.dllFuncs.pfnCreateInstancedBaselines();

	// now put the baseline into the signon message.
	MSG_BeginServerCmd( &sv.signon, svc_spawnbaseline );

	for( entnum = 0; entnum < svgame.numEntities; entnum++ )
	{
		edict_t	*pEdict = EDICT_NUM( entnum );

		if( !SV_IsValidEdict( pEdict ))
			continue;

		if( entnum != 0 && entnum <= svs.maxclients )
		{
			player = true;
		}
		else
		{
			if( !pEdict->v.modelindex )
				continue; // invisible
			player = false;
		}

		// take current state as baseline
		base = &svs.baselines[entnum];

		MSG_WriteDeltaEntity( &nullstate, base, &sv.signon, true, player, 1.0f, 0 );
	}

	MSG_WriteUBitLong( &sv.signon, LAST_EDICT, MAX_ENTITY_BITS ); // end of baselines
	MSG_WriteUBitLong( &sv.signon, sv.instanced.count, 6 );

	for( entnum = 0; entnum < sv.instanced.count; entnum++ )
	{
		base = &sv.instanced.baselines[entnum];
		MSG_WriteDeltaEntity( &nullstate, base, &sv.signon, true, false, 1.0f, 0 );
	}
}

/*
================
SV_FreeOldEntities

remove immediate entities
================
*/
void SV_FreeOldEntities( void )
{
	edict_t	*ent;
	int	i;

	// at end of frame kill all entities which supposed to it 
	for( i = svgame.globals->maxClients + 1; i < svgame.numEntities; i++ )
	{
		ent = EDICT_NUM( i );
		if( ent->free ) continue;

		if( ent->v.flags & FL_KILLME )
			SV_FreeEdict( ent );
	}

	// decrement svgame.numEntities if the highest number entities died
	for( ; EDICT_NUM( svgame.numEntities - 1 )->free; svgame.numEntities-- );
}

/*
================
SV_ActivateServer

activate server on changed map, run physics
================
*/
void SV_ActivateServer( void )
{
	int	i, numFrames;

	if( !svs.initialized )
		return;

	// Activate the DLL server code
	svgame.dllFuncs.pfnServerActivate( svgame.edicts, svgame.numEntities, svgame.globals->maxClients );

	if( sv.loadgame || svgame.globals->changelevel )
	{
		sv.frametime = bound( 0.001, sv_changetime.value, 0.1 );
		numFrames = 1;
	}
	else if( svs.maxclients <= 1 )
	{
		sv.frametime = bound( 0.1, sv_spawntime.value, 0.8 );
		numFrames = 2;
	}
	else
	{
		sv.frametime = 0.1f;
		numFrames = 8;
	}

	// run some frames to allow everything to settle
	for( i = 0; i < numFrames; i++ )
		SV_Physics();

	// parse user-specified resources
	SV_CreateGenericResources();

	// create a baseline for more efficient communications
	SV_CreateBaseline();

	// collect all info from precached resources
	SV_CreateResourceList();

	// check and count all files that marked by user as unmodified (typically is a player models etc)
	sv.num_consistency = SV_TransferConsistencyInfo();

	// send serverinfo to all connected clients
	for( i = 0; i < svs.maxclients; i++ )
	{
		if( svs.clients[i].state >= cs_connected )
		{
			Netchan_Clear( &svs.clients[i].netchan );
			svs.clients[i].delta_sequence = -1;
		}
	}

	// invoke to refresh all movevars
	memset( &svgame.oldmovevars, 0, sizeof( movevars_t ));
	svgame.globals->changelevel = false; // changelevel ends here

	// setup hostflags
	sv.hostflags = 0;

	HPAK_FlushHostQueue();

	// tell what kind of server has been started.
	if( svgame.globals->maxClients > 1 )
	{
		MsgDev( D_INFO, "%i player server started\n", svgame.globals->maxClients );
	}
	else
	{
		MsgDev( D_INFO, "Game started\n" );
	}

	Log_Printf( "Started map \"%s\" (CRC \"%i\")\n", sv.name, sv.worldmapCRC );

	// dedicated server purge unused resources here
	if( host.type == HOST_DEDICATED )
		Mod_FreeUnused ();

	sv.state = ss_active;
	host.movevars_changed = true;
	sv.changelevel = false;
	sv.paused = false;

	Host_SetServerState( sv.state );

	MsgDev( D_INFO, "level loaded at %.2f sec\n", Sys_DoubleTime() - svs.timestart );

	if( svs.maxclients > 1 )
	{
		char *mapchangecfgfile = Cvar_VariableString( "mapchangecfgfile" );
		if( *mapchangecfgfile ) Cbuf_AddText( va( "exec %s\n", mapchangecfgfile ));

		if( public_server->value )
		{
			MsgDev( D_INFO, "Adding your server to master server list\n" );
			Master_Add( );
		}
	}
}

/*
================
SV_DeactivateServer

deactivate server, free edicts, strings etc
================
*/
void SV_DeactivateServer( void )
{
	int	i;

	if( !svs.initialized || sv.state == ss_dead )
		return;

	sv.state = ss_dead;

	svgame.dllFuncs.pfnServerDeactivate();

	SV_FreeEdicts ();

	SV_ClearPhysEnts ();

	Mem_EmptyPool( svgame.stringspool );

	for( i = 0; i < svs.maxclients; i++ )
	{
		// release client frames
		if( svs.clients[i].frames )
			Mem_Free( svs.clients[i].frames );
		svs.clients[i].frames = NULL;
	}

	svgame.globals->maxEntities = GI->max_edicts;
	svgame.globals->maxClients = svs.maxclients;
	svgame.numEntities = svgame.globals->maxClients + 1; // clients + world
	svgame.globals->startspot = 0;
	svgame.globals->mapname = 0;
}

/*
================
SV_LevelInit

Spawn all entities
================
*/
void SV_LevelInit( const char *pMapName, char const *pOldLevel, char const *pLandmarkName, qboolean loadGame )
{
	if( !svs.initialized )
		return;

	if( loadGame )
	{
		if( !SV_LoadGameState( pMapName, 1 ))
		{
			SV_SpawnEntities( pMapName, SV_EntityScript( ));
		}

		if( pOldLevel )
		{
			SV_LoadAdjacentEnts( pOldLevel, pLandmarkName );
		}

		if( sv_newunit.value )
		{
			SV_ClearSaveDir();
		}
	}
	else
	{
		svgame.dllFuncs.pfnResetGlobalState();
		SV_SpawnEntities( pMapName, SV_EntityScript( ));
		svgame.globals->frametime = 0.0f;

		if( sv_newunit.value )
		{
			SV_ClearSaveDir();
		}
	}

	// always clearing newunit variable
	Cvar_SetValue( "sv_newunit", 0 );

	// relese all intermediate entities
	SV_FreeOldEntities ();
}

/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.
================
*/
qboolean SV_SpawnServer( const char *mapname, const char *startspot )
{
	int	i, current_skill;
	qboolean	loadgame, paused;
	qboolean	background, changelevel;

	// save state
	loadgame = sv.loadgame;
	background = sv.background;
	changelevel = sv.changelevel;
	paused = sv.paused;

	if( sv.state == ss_dead )
		SV_InitGame(); // the game is just starting

	NET_Config(( svs.maxclients > 1 )); // init network stuff
	ClearBits( sv_maxclients->flags, FCVAR_CHANGED );

	if( !svs.initialized )
		return false;

	Log_Open();
	Log_Printf( "Loading map \"%s\"\n", mapname );
	Log_PrintServerVars();

	svgame.globals->changelevel = false; // will be restored later if needed
	svs.timestart = Sys_DoubleTime();
	svs.spawncount++; // any partially connected client will be restarted

	if( startspot )
	{
		MsgDev( D_INFO, "Spawn Server: %s [%s]\n", mapname, startspot );
	}
	else
	{
		MsgDev( D_INFO, "Spawn Server: %s\n", mapname );
	}

	sv.state = ss_dead;
	Host_SetServerState( sv.state );
	memset( &sv, 0, sizeof( sv ));	// wipe the entire per-level structure

	// restore state
	sv.paused = paused;
	sv.loadgame = loadgame;
	sv.background = background;
	sv.changelevel = changelevel;
	sv.time = 1.0f;			// server spawn time it's always 1.0 second
	svgame.globals->time = sv.time;
	
	// initialize buffers
	MSG_Init( &sv.signon, "Signon", sv.signon_buf, sizeof( sv.signon_buf ));
	MSG_Init( &sv.multicast, "Multicast", sv.multicast_buf, sizeof( sv.multicast_buf ));
	MSG_Init( &sv.datagram, "Datagram", sv.datagram_buf, sizeof( sv.datagram_buf ));
	MSG_Init( &sv.reliable_datagram, "Reliable Datagram", sv.reliable_datagram_buf, sizeof( sv.reliable_datagram_buf ));
	MSG_Init( &sv.spec_datagram, "Spectator Datagram", sv.spectator_buf, sizeof( sv.spectator_buf ));

	// leave slots at start for clients only
	for( i = 0; i < svs.maxclients; i++ )
	{
		// needs to reconnect
		if( svs.clients[i].state > cs_connected )
			svs.clients[i].state = cs_connected;
	}

	// make cvars consistant
	if( coop.value ) Cvar_SetValue( "deathmatch", 0 );
	current_skill = Q_rint( skill.value );
	current_skill = bound( 0, current_skill, 3 );

	Cvar_SetValue( "skill", (float)current_skill );

	if( sv.background )
	{
		// tell the game parts about background state
		Cvar_FullSet( "sv_background", "1", FCVAR_READ_ONLY );
		Cvar_FullSet( "cl_background", "1", FCVAR_READ_ONLY );
	}
	else
	{
		Cvar_FullSet( "sv_background", "0", FCVAR_READ_ONLY );
		Cvar_FullSet( "cl_background", "0", FCVAR_READ_ONLY );
	}

	// make sure what server name doesn't contain path and extension
	FS_FileBase( mapname, sv.name );

	if( startspot )
		Q_strncpy( sv.startspot, startspot, sizeof( sv.startspot ));
	else sv.startspot[0] = '\0';

	Q_snprintf( sv.model_precache[1], sizeof( sv.model_precache[0] ), "maps/%s.bsp", sv.name );
	SetBits( sv.model_precache_flags[1], RES_FATALIFMISSING );
	Mod_LoadWorld( sv.model_precache[1], NS_SERVER );
	sv.worldmodel = Mod_Handle( 1 ); // get world pointer

	CRC32_MapFile( &sv.worldmapCRC, sv.model_precache[1], svs.maxclients > 1 );

	for( i = 1; i < sv.worldmodel->numsubmodels; i++ )
	{
		SetBits( sv.model_precache_flags[i+1], RES_FATALIFMISSING );
		Q_sprintf( sv.model_precache[i+1], "*%i", i );
		Mod_RegisterModel( sv.model_precache[i+1], i+1 );
	}

	// precache and static commands can be issued during map initialization
	sv.state = ss_loading;

	Host_SetServerState( sv.state );

	// clear physics interaction links
	SV_ClearWorld();

	return true;
}

/*
==============
SV_InitGame

A brand new game has been started
==============
*/
void SV_InitGame( void )
{
	edict_t	*ent;
	int	i, load = sv.loadgame;
	
	if( svs.initialized )
	{
		// cause any connected clients to reconnect
		Q_strncpy( host.finalmsg, "Server restarted", MAX_STRING );
		SV_Shutdown( true );
	}
	else
	{
		// init game after host error
		if( !svgame.hInstance )
		{
			if( !SV_LoadProgs( GI->game_dll ))
			{
				MsgDev( D_ERROR, "SV_InitGame: can't initialize %s\n", GI->game_dll );
				return; // can't loading
			}
		}

		// make sure the client is down
		CL_Drop();
	}

	svs.maxclients = sv_maxclients->value;	// copy the actual value from cvar

	// dedicated servers are can't be single player and are usually DM
	if( host.type == HOST_DEDICATED )
		svs.maxclients = bound( 4, svs.maxclients, MAX_CLIENTS );
	else svs.maxclients = bound( 1, svs.maxclients, MAX_CLIENTS );

	if( svs.maxclients == 1 )
		Cvar_SetValue( "deathmatch", 0.0f );
	else Cvar_SetValue( "deathmatch", 1.0f );

	// make cvars consistant
	if( coop.value ) Cvar_SetValue( "deathmatch", 0.0f );

	// feedback for cvar
	Cvar_FullSet( "maxplayers", va( "%d", svs.maxclients ), FCVAR_LATCH );
	SV_UPDATE_BACKUP = ( svs.maxclients == 1 ) ? SINGLEPLAYER_BACKUP : MULTIPLAYER_BACKUP;
	svgame.globals->maxClients = svs.maxclients;

	svs.clients = Z_Malloc( sizeof( sv_client_t ) * svs.maxclients );
	svs.num_client_entities = svs.maxclients * SV_UPDATE_BACKUP * NUM_PACKET_ENTITIES;
	svs.packet_entities = Z_Malloc( sizeof( entity_state_t ) * svs.num_client_entities );
	svs.baselines = Z_Malloc( sizeof( entity_state_t ) * GI->max_edicts );
	if( !load ) MsgDev( D_INFO, "%s alloced by server packet entities\n", Q_memprint( sizeof( entity_state_t ) * svs.num_client_entities ));

	// client frames will be allocated in SV_DirectConnect

	// init network stuff
	NET_Config(( svs.maxclients > 1 ));

	// copy gamemode into svgame.globals
	svgame.globals->deathmatch = deathmatch.value;
	svgame.globals->coop = coop.value;

	// heartbeats will always be sent to the id master
	svs.last_heartbeat = MAX_HEARTBEAT; // send immediately

	// set client fields on player ents
	for( i = 0; i < svgame.globals->maxClients; i++ )
	{
		// setup all the clients
		ent = EDICT_NUM( i + 1 );
		svs.clients[i].edict = ent;
		SV_InitEdict( ent );
	}

	// get actual movevars
	SV_UpdateMovevars( true );

	svgame.numEntities = svgame.globals->maxClients + 1; // clients + world
	svs.initialized = true;
}

qboolean SV_Active( void )
{
	return svs.initialized;
}

int SV_GetMaxClients( void )
{
	return svs.maxclients;
}

void SV_ForceError( void )
{
	// this is only for singleplayer testing
	if( svs.maxclients != 1 ) return;
	sv.write_bad_message = true;
}

void SV_InitGameProgs( void )
{
	if( svgame.hInstance ) return; // already loaded

	// just try to initialize
	SV_LoadProgs( GI->game_dll );
}

void SV_FreeGameProgs( void )
{
	if( svs.initialized ) return;	// server is active

	// unload progs (free cvars and commands)
	SV_UnloadProgs();
}

qboolean SV_NewGame( const char *mapName, qboolean loadGame )
{
	if( !loadGame )
	{
		if( !SV_MapIsValid( mapName, GI->sp_entity, NULL ))
			return false;

		SV_ClearSaveDir ();
		SV_Shutdown( true );
	}
	else
	{
		S_StopAllSounds (true);
		SV_DeactivateServer ();
	}

	sv.loadgame = loadGame;
	sv.background = false;
	sv.changelevel = false;

	SCR_BeginLoadingPlaque( false );

	if( !SV_SpawnServer( mapName, NULL ))
		return false;

	SV_LevelInit( mapName, NULL, NULL, loadGame );
	sv.loadgame = loadGame;

	SV_ActivateServer();

	if( sv.state != ss_active )
		return false;

	return true;
}