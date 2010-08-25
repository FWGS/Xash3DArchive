//=======================================================================
//			Copyright XashXT Group 2009 �
//		     sv_init.c - server initialize operations
//=======================================================================

#include "common.h"
#include "server.h"
#include "protocol.h"

int SV_UPDATE_BACKUP = SINGLEPLAYER_BACKUP;

server_static_t	svs;	// persistant server info
svgame_static_t	svgame;	// persistant game info
server_t		sv;	// local server

/*
================
SV_FindIndex

================
*/
int SV_FindIndex( const char *name, int start, int end, bool create )
{
	int	i;
	
	if( !name || !name[0] ) return 0;

	for( i = 1; i < end && sv.configstrings[start+i][0]; i++ )
		if(!com.strcmp( sv.configstrings[start+i], name ))
			return i;
	if( !create ) return 0;
	if( i == end ) 
	{
		MsgDev( D_WARN, "SV_FindIndex: %d out of range [%d - %d]\n", start, end );
		return 0;
	}

	// register new resource
	com.strncpy( sv.configstrings[start+i], name, sizeof( sv.configstrings[i] ));

	if( sv.state != ss_loading )
	{	
		// send the update to everyone
		BF_Clear( &sv.multicast );
		BF_WriteByte( &sv.multicast, svc_configstring );
		BF_WriteShort( &sv.multicast, start + i );
		BF_WriteString( &sv.multicast, name );
		SV_Send( MSG_ALL, vec3_origin, NULL );
	}
	return i;
}

int SV_ModelIndex( const char *name )
{
	return SV_FindIndex( name, CS_MODELS, MAX_MODELS, true );
}

int SV_SoundIndex( const char *name )
{
	return SV_FindIndex( name, CS_SOUNDS, MAX_SOUNDS, true );
}

int SV_DecalIndex( const char *name )
{
	return SV_FindIndex( name, CS_DECALS, MAX_DECALNAMES, true );
}

int SV_EventIndex( const char *name )
{
	return SV_FindIndex( name, CS_EVENTS, MAX_EVENTS, true );
}

int SV_GenericIndex( const char *name )
{
	return SV_FindIndex( name, CS_GENERICS, MAX_GENERICS, true );
}

/*
================
SV_CreateBaseline

Entity baselines are used to compress the update messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
void SV_CreateBaseline( void )
{
	edict_t	*pEdict;
	int	entnum;	

	for( entnum = 0; entnum < svgame.numEntities; entnum++ )
	{
		pEdict = EDICT_NUM( entnum );
		if( !SV_IsValidEdict( pEdict )) continue;
		SV_BaselineForEntity( pEdict );
	}
}

/*
=================
SV_CheckForSavegame
=================
*/
void SV_CheckForSavegame( const char *savename )
{
	sv.loadgame = false;

	if( savename )
	{
		sv.loadgame = true; // predicting state
		if( sv_noreload->value ) sv.loadgame = false;
		if( svgame.globals->deathmatch || svgame.globals->coop || svgame.globals->teamplay )
			sv.loadgame = false;
		if( !savename ) sv.loadgame = false;
		if( !FS_FileExists( va( "save/%s", savename )))
			sv.loadgame = false;
	}
}

/*
================
SV_ActivateServer

activate server on changed map, run physics
================
*/
void SV_ActivateServer( void )
{
	int	i;

	if( !svs.initialized )
	{
		// probably server.dll doesn't loading
//		Host_AbortCurrentFrame ();
		return;
	}

	// Activate the DLL server code
	svgame.dllFuncs.pfnServerActivate( svgame.edicts, svgame.numEntities, svgame.globals->maxClients );

	if( !sv.loadgame )
	{
		// run two frames to allow everything to settle
		for( i = 0; i < 2; i++ )
			SV_Physics();

		// create a baseline for more efficient communications
		SV_CreateBaseline();
	}

	// invoke to refresh all movevars
	Mem_Set( &svgame.oldmovevars, 0, sizeof( movevars_t ));
	svgame.globals->changelevel = false;	// changelevel ends here

	// setup hostflags
	sv.hostflags = 0;

	// tell what kind of server has been started.
	if( svgame.globals->maxClients > 1 )
	{
		MsgDev( D_INFO, "%i player server started\n", svgame.globals->maxClients );
	}
	else
	{
		MsgDev( D_INFO, "Game started\n" );
	}

	// set serverinfo variable
	Cvar_FullSet( "mapname", sv.name, CVAR_INIT );

	CM_EndRegistration (); // free unused models

	sv.state = ss_active;
	physinfo->modified = true;
	sv.paused = false;

	Host_SetServerState( sv.state );
}

/*
================
SV_DeactivateServer

deactivate server, free edicts stringtables etc
================
*/
void SV_DeactivateServer( void )
{
	int	i;

	if( !svs.initialized ) return;

	SV_FreeEdicts ();

	if( sv.state == ss_dead ) return;
	sv.state = ss_dead;

	if( svgame.globals->pStringBase )
		Mem_EmptyPool( svgame.stringspool );
	else StringTable_Clear( svgame.hStringTable );

	svgame.dllFuncs.pfnServerDeactivate();

	// set client fields on player ents
	for( i = 0; i < svgame.globals->maxClients; i++ )
	{
		// free client frames
		if( svs.clients[i].frames )
			Mem_Free( svs.clients[i].frames );
		svs.clients[i].frames = NULL;
	}

	svgame.globals->maxEntities = GI->max_edicts;
	svgame.globals->maxClients = sv_maxclients->integer;
	svgame.numEntities = svgame.globals->maxClients + 1; // clients + world
	svgame.globals->mapname = 0;
}

/*
================
SV_LevelInit

Spawn all entities
================
*/
void SV_LevelInit( const char *pMapName, char const *pOldLevel, char const *pLandmarkName, bool loadGame )
{
	if( !svs.initialized )
		return;

	if( loadGame )
	{
		if( !SV_LoadGameState( pMapName, 1 ))
		{
			SV_SpawnEntities( pMapName, CM_GetEntityScript( ));
		}

		if( pOldLevel )
		{
			SV_LoadAdjacentEnts( pOldLevel, pLandmarkName );
		}

		if( sv_newunit->integer )
		{
			Cvar_SetValue( "sv_newunit", 0 );
			SV_ClearSaveDir();
		}
	}
	else
	{
		svgame.dllFuncs.pfnResetGlobalState();

		SV_SpawnEntities( pMapName, CM_GetEntityScript( ));
	}

	// call before sending baselines into the client
	svgame.dllFuncs.pfnCreateInstancedBaselines();

	SV_FreeOldEntities ();
}

/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.

================
*/
bool SV_SpawnServer( const char *mapname, const char *startspot )
{
	uint	i, checksum;
	int	current_skill;
	bool	loadgame, paused;

	Cmd_ExecuteString( "latch\n" );

	if( sv.state == ss_dead )
		SV_InitGame(); // the game is just starting

	if( !svs.initialized )
		return false;

	svgame.globals->changelevel = false;	// will be restored later if needed
	svs.timestart = Sys_DoubleTime();
	svs.spawncount++; // any partially connected client will be restarted
	svs.realtime = 0;

	if( startspot )
	{
		MsgDev( D_INFO, "Spawn Server: %s [%s]\n", mapname, startspot );
	}
	else
	{
		MsgDev( D_INFO, "Spawn Server: %s\n", mapname );
	}

	// save state
	loadgame = sv.loadgame;
	paused = sv.paused;

	sv.state = ss_dead;
	Host_SetServerState( sv.state );
	Mem_Set( &sv, 0, sizeof( sv ));	// wipe the entire per-level structure

	// restore state
	sv.paused = paused;
	sv.loadgame = loadgame;
	sv.time = 1000;			// server spawn time it's always 1.0 second
	svgame.globals->time = sv_time();
	
	// initialize buffers
	BF_Init( &sv.multicast, "Multicast", sv.multicast_buf, sizeof( sv.multicast_buf ));
	BF_Init( &sv.signon, "Signon", sv.signon_buf, sizeof( sv.signon_buf ));

	// leave slots at start for clients only
	for( i = 0; i < sv_maxclients->integer; i++ )
	{
		// needs to reconnect
		if( svs.clients[i].state > cs_connected )
			svs.clients[i].state = cs_connected;
		svs.clients[i].lastframe = -1;
	}

	// make cvars consistant
	if( Cvar_VariableInteger( "coop" )) Cvar_SetValue( "deathmatch", 0 );
	current_skill = (int)(Cvar_VariableValue( "skill" ) + 0.5f);
	current_skill = bound( 0, current_skill, 3 );

	Cvar_SetValue( "skill", (float)current_skill );

	// make sure what server name doesn't contain path and extension
	FS_FileBase( mapname, sv.name );
	com.strncpy( sv.configstrings[CS_NAME], sv.name, CS_SIZE );

	if( startspot )
		com.strncpy( sv.startspot, startspot, sizeof( sv.startspot ));
	else sv.startspot[0] = '\0';

	com.sprintf( sv.configstrings[CS_MODELS+1], "maps/%s.bsp", sv.name );
	CM_BeginRegistration( sv.configstrings[CS_MODELS+1], false, &checksum );
	com.sprintf( sv.configstrings[CS_MAPCHECKSUM], "%i", checksum );
	sv.worldmodel = CM_ClipHandleToModel( 1 ); // get world pointer

	for( i = 1; i < CM_NumBmodels(); i++ )
	{
		com.sprintf( sv.configstrings[CS_MODELS+1+i], "*%i", i );
		CM_RegisterModel( sv.configstrings[CS_MODELS+1+i], i+1 );
	}

	// precache and static commands can be issued during map initialization
	sv.state = ss_loading;

	Host_SetServerState( sv.state );

	// clear physics interaction links
	SV_ClearWorld();

	// tell dlls about new level started
	svgame.dllFuncs.pfnParmsNewLevel();

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
	string	idmaster;
	edict_t	*ent;
	int	i;
	
	if( svs.initialized )
	{
		// cause any connected clients to reconnect
		com.strncpy( host.finalmsg, "Server restarted\n", MAX_STRING );
		SV_Shutdown( true );
	}
	else
	{
		// init game after host error
		if( !svgame.hInstance )
		{
			if( !SV_LoadProgs( "server.dll" ))
			{
				MsgDev( D_ERROR, "SV_InitGame: can't initialize server.dll\n" );
				return; // can't loading
			}
		}

		// make sure the client is down
		CL_Drop();
	}

	if( Cvar_VariableValue( "coop" ) && Cvar_VariableValue ( "deathmatch" ) && Cvar_VariableValue( "teamplay" ))
	{
		MsgDev( D_WARN, "Deathmatch, Teamplay and Coop set, defaulting to Deathmatch\n");
		Cvar_FullSet( "coop", "0",  CVAR_LATCH );
		Cvar_FullSet( "teamplay", "0", CVAR_LATCH );
	}

	// dedicated servers are can't be single player and are usually DM
	// so unless they explicity set coop, force it to deathmatch
	if( host.type == HOST_DEDICATED )
	{
		if(!Cvar_VariableValue( "coop" ) && !Cvar_VariableValue( "teamplay" ))
			Cvar_FullSet( "deathmatch", "1",  CVAR_LATCH );
	}

	// init clients
	if( Cvar_VariableValue( "deathmatch" ) || Cvar_VariableValue( "teamplay" ))
	{
		if( sv_maxclients->integer <= 1 )
			Cvar_FullSet( "sv_maxclients", "8", CVAR_LATCH );
		else if( sv_maxclients->integer > MAX_CLIENTS )
			Cvar_FullSet( "sv_maxclients", "32", CVAR_LATCH );
	}
	else if( Cvar_VariableValue( "coop" ))
	{
		if( sv_maxclients->integer <= 1 || sv_maxclients->integer > 4 )
			Cvar_FullSet( "sv_maxclients", "4", CVAR_LATCH );
	}
	else	
	{
		// non-deathmatch, non-coop is one player
		Cvar_FullSet( "sv_maxclients", "1", CVAR_LATCH );
	}

	svgame.globals->maxClients = sv_maxclients->integer;

	SV_UPDATE_BACKUP = ( svgame.globals->maxClients == 1 ) ? SINGLEPLAYER_BACKUP : MULTIPLAYER_BACKUP;

	svs.spawncount = Com_RandomLong( 0, 65535 );
	svs.clients = Z_Malloc( sizeof( sv_client_t ) * sv_maxclients->integer );
	svs.num_client_entities = sv_maxclients->integer * SV_UPDATE_BACKUP * 64; // g-cont: what a mem waster ???
	svs.client_entities = Z_Malloc( sizeof( entity_state_t ) * svs.num_client_entities );
	svs.baselines = Z_Malloc( sizeof( entity_state_t ) * GI->max_edicts );

	// client frames will be allocated in SV_DirectConnect

	// init network stuff
	NET_Config(( sv_maxclients->integer > 1 ));

	// copy gamemode into svgame.globals
	svgame.globals->deathmatch = Cvar_VariableInteger( "deathmatch" );
	svgame.globals->teamplay = Cvar_VariableInteger( "teamplay" );
	svgame.globals->coop = Cvar_VariableInteger( "coop" );

	// heartbeats will always be sent to the id master
	svs.last_heartbeat = MAX_HEARTBEAT; // send immediately
	com.sprintf( idmaster, "192.246.40.37:%i", PORT_MASTER );
	NET_StringToAdr( idmaster, &master_adr[0] );

	// set client fields on player ents
	for( i = 0; i < svgame.globals->maxClients; i++ )
	{
		// setup all the clients
		ent = EDICT_NUM( i + 1 );
		SV_InitEdict( ent );

		// make crosslinks
		svs.clients[i].edict = ent;
		Mem_Set( &svs.clients[i].lastcmd, 0, sizeof( svs.clients[i].lastcmd ));
		Mem_Set( &svs.clients[i].physinfo, 0, sizeof( svs.clients[i].physinfo ));
	}

	svgame.numEntities = svgame.globals->maxClients + 1; // clients + world
	svs.initialized = true;
}

bool SV_Active( void )
{
	return svs.initialized;
}

void SV_ForceError( void )
{
	// this is only for singleplayer testing
	if( sv_maxclients->integer != 1 ) return;
	sv.write_bad_message = true;
}

void SV_InitGameProgs( void )
{
	if( svgame.hInstance ) return; // not needs

	// just try to initialize
	SV_LoadProgs( "server.dll" );
}

bool SV_NewGame( const char *mapName, bool loadGame )
{
	if( !loadGame )
	{
		if( !SV_MapIsValid( mapName, GI->sp_entity, NULL ))
		return false;
	}

	S_StopAllSounds ();
	SV_DeactivateServer ();

	sv.loadgame = loadGame;

	if( !SV_SpawnServer( mapName, NULL ))
		return false;

	SV_LevelInit( mapName, NULL, NULL, loadGame );
	sv.loadgame = loadGame;

	SV_ActivateServer();

	if( sv.state != ss_active )
		return false;

	return true;
}