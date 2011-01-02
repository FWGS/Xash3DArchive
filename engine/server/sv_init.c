//=======================================================================
//			Copyright XashXT Group 2009 ©
//		     sv_init.c - server initialize operations
//=======================================================================

#include "common.h"
#include "server.h"

int SV_UPDATE_BACKUP = SINGLEPLAYER_BACKUP;

server_static_t	svs;	// persistant server info
svgame_static_t	svgame;	// persistant game info
server_t		sv;	// local server

int SV_ModelIndex( const char *name )
{
	int	i;

	if( !name || !name[0] )
		return 0;

	for( i = 1; i < MAX_MODELS && sv.model_precache[i][0]; i++ )
	{
		if( !com.stricmp( sv.model_precache[i], name ))
			return i;
	}

	if( i == MAX_MODELS )
	{
		Host_Error( "SV_ModelIndex: MAX_MODELS limit exceeded\n" );
		return 0;
	}

	// register new model
	com.strncpy( sv.model_precache[i], name, sizeof( sv.model_precache[i] ));

	if( sv.state != ss_loading )
	{	
		// send the update to everyone
		BF_WriteByte( &sv.reliable_datagram, svc_modelindex );
		BF_WriteUBitLong( &sv.reliable_datagram, i, MAX_MODEL_BITS );
		BF_WriteString( &sv.reliable_datagram, name );
	}

	return i;
}

int SV_SoundIndex( const char *name )
{
	int	i;

	if( !name || !name[0] )
		return 0;

	for( i = 1; i < MAX_SOUNDS && sv.sound_precache[i][0]; i++ )
	{
		if( !com.stricmp( sv.sound_precache[i], name ))
			return i;
	}

	if( i == MAX_SOUNDS )
	{
		Host_Error( "SV_SoundIndex: MAX_SOUNDS limit exceeded\n" );
		return 0;
	}

	// register new sound
	com.strncpy( sv.sound_precache[i], name, sizeof( sv.sound_precache[i] ));

	if( sv.state != ss_loading )
	{	
		// send the update to everyone
		BF_WriteByte( &sv.reliable_datagram, svc_modelindex );
		BF_WriteUBitLong( &sv.reliable_datagram, i, MAX_SOUND_BITS );
		BF_WriteString( &sv.reliable_datagram, name );
	}

	return i;
}

int SV_EventIndex( const char *name )
{
	int	i;

	if( !name || !name[0] )
		return 0;

	for( i = 1; i < MAX_EVENTS && sv.event_precache[i][0]; i++ )
	{
		if( !com.stricmp( sv.event_precache[i], name ))
			return i;
	}

	if( i == MAX_EVENTS )
	{
		Host_Error( "SV_EventIndex: MAX_EVENTS limit exceeded\n" );
		return 0;
	}

	// register new event
	com.strncpy( sv.event_precache[i], name, sizeof( sv.event_precache[i] ));

	if( sv.state != ss_loading )
	{
		// send the update to everyone
		BF_WriteByte( &sv.reliable_datagram, svc_eventindex );
		BF_WriteUBitLong( &sv.reliable_datagram, i, MAX_EVENT_BITS );
		BF_WriteString( &sv.reliable_datagram, name );
	}

	return i;
}

int SV_GenericIndex( const char *name )
{
	int	i;

	if( !name || !name[0] )
		return 0;

	for( i = 1; i < MAX_CUSTOM && sv.files_precache[i][0]; i++ )
	{
		if( !com.stricmp( sv.files_precache[i], name ))
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
	com.strncpy( sv.files_precache[i], name, sizeof( sv.files_precache[i] ));

	return i;
}

/*
================
SV_EntityScript

get entity script for current map
FIXME: merge with SV_GetEntityScript
================
*/
script_t *SV_EntityScript( void )
{
	string	entfilename;
	script_t	*ents;

	if( !sv.worldmodel )
		return NULL;

	// check for entfile too
	com.strncpy( entfilename, sv.worldmodel->name, sizeof( entfilename ));
	FS_StripExtension( entfilename );
	FS_DefaultExtension( entfilename, ".ent" );

	if(( ents = Com_OpenScript( entfilename, NULL, 0 )))
	{
		MsgDev( D_INFO, "^2Read entity patch:^7 %s\n", entfilename );
		return ents;
	}

	// create script from internal entities
	return Com_OpenScript( entfilename, sv.worldmodel->entities, com.strlen( sv.worldmodel->entities ));
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
	int	e;	

	for( e = 0; e < svgame.numEntities; e++ )
	{
		pEdict = EDICT_NUM( e );
		if( !SV_IsValidEdict( pEdict )) continue;
		SV_BaselineForEntity( pEdict );
	}

	// create the instanced baselines
	svgame.dllFuncs.pfnCreateInstancedBaselines();
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

	// custom muzzleflashes
	pfnPrecacheModel( "sprites/muzzleflash.spr" );
	pfnPrecacheModel( "sprites/muzzleflash1.spr" );
	pfnPrecacheModel( "sprites/muzzleflash2.spr" );
	pfnPrecacheModel( "sprites/muzzleflash3.spr" );

	// rocket flare
	pfnPrecacheModel( "sprites/animglow01.spr" );

	// ricochet sprite
	pfnPrecacheModel( "sprites/richo1.spr" );

	// Activate the DLL server code
	svgame.dllFuncs.pfnServerActivate( svgame.edicts, svgame.numEntities, svgame.globals->maxClients );

	// create a baseline for more efficient communications
	SV_CreateBaseline();

	// Send serverinfo to all connected clients
	for( i = 0; i < sv_maxclients->integer; i++ )
	{
		if( svs.clients[i].state >= cs_connected )
		{
			Netchan_Clear( &svs.clients[i].netchan );
			svs.clients[i].delta_sequence = -1;
		}
	}

	// FIXME: test this for correct
	sv.frametime = (sv.loadgame) ? host.frametime : 0.0f;
	numFrames = (sv.loadgame) ? 1 : 2;
			
	// run some frames to allow everything to settle
	for( i = 0; i < numFrames; i++ )
		SV_Physics();

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

	Mod_FreeUnused ();

	sv.state = ss_active;
	physinfo->modified = true;
	sv.paused = false;

	Host_SetServerState( sv.state );
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

	if( !svs.initialized ) return;

	if( sv.state == ss_dead ) return;
	sv.state = ss_dead;

	SV_FreeEdicts ();

	Mem_EmptyPool( svgame.stringspool );

	svgame.dllFuncs.pfnServerDeactivate();

	for( i = 0; i < svgame.globals->maxClients; i++ )
	{
		// release client frames
		SV_ClearFrames( &svs.clients[i].frames );
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
	}
	else
	{
		svgame.dllFuncs.pfnResetGlobalState();
		SV_SpawnEntities( pMapName, SV_EntityScript( ));
	}

	if( sv_newunit->integer )
	{
		Cvar_SetFloat( "sv_newunit", 0 );
		SV_ClearSaveDir();
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
qboolean SV_SpawnServer( const char *mapname, const char *startspot )
{
	int	i, current_skill;
	qboolean	loadgame, paused;

	Cmd_ExecuteString( "latch\n" );

	if( sv.state == ss_dead )
		SV_InitGame(); // the game is just starting

	if( !svs.initialized )
		return false;

	svgame.globals->changelevel = false;	// will be restored later if needed
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

	// save state
	loadgame = sv.loadgame;
	paused = sv.paused;

	sv.state = ss_dead;
	Host_SetServerState( sv.state );
	Mem_Set( &sv, 0, sizeof( sv ));	// wipe the entire per-level structure

	// restore state
	sv.paused = paused;
	sv.loadgame = loadgame;
	sv.time = 1.0f;			// server spawn time it's always 1.0 second
	svgame.globals->time = sv_time();
	
	// initialize buffers
	BF_Init( &sv.datagram, "Datagram", sv.datagram_buf, sizeof( sv.datagram_buf ));
	BF_Init( &sv.reliable_datagram, "Datagram R", sv.reliable_datagram_buf, sizeof( sv.reliable_datagram_buf ));
	BF_Init( &sv.multicast, "Multicast", sv.multicast_buf, sizeof( sv.multicast_buf ));
	BF_Init( &sv.signon, "Signon", sv.signon_buf, sizeof( sv.signon_buf ));

	// leave slots at start for clients only
	for( i = 0; i < sv_maxclients->integer; i++ )
	{
		// needs to reconnect
		if( svs.clients[i].state > cs_connected )
			svs.clients[i].state = cs_connected;
	}

	// make cvars consistant
	if( Cvar_VariableInteger( "coop" )) Cvar_SetFloat( "deathmatch", 0 );
	current_skill = (int)(Cvar_VariableValue( "skill" ) + 0.5f);
	current_skill = bound( 0, current_skill, 3 );

	Cvar_SetFloat( "skill", (float)current_skill );

	// make sure what server name doesn't contain path and extension
	FS_FileBase( mapname, sv.name );

	if( startspot )
		com.strncpy( sv.startspot, startspot, sizeof( sv.startspot ));
	else sv.startspot[0] = '\0';

	com.snprintf( sv.model_precache[1], sizeof( sv.model_precache[0] ), "maps/%s.bsp", sv.name );
	Mod_LoadWorld( sv.model_precache[1], &sv.checksum );
	sv.worldmodel = CM_ClipHandleToModel( 1 ); // get world pointer

	for( i = 1; i < sv.worldmodel->numsubmodels; i++ )
	{
		com.sprintf( sv.model_precache[i+1], "*%i", i );
		Mod_RegisterModel( sv.model_precache[i+1], i+1 );
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
			if( !SV_LoadProgs( GI->game_dll ))
			{
				MsgDev( D_ERROR, "SV_InitGame: can't initialize %s\n", GI->game_dll );
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
	svs.num_client_entities = sv_maxclients->integer * SV_UPDATE_BACKUP * 64;
	svs.packet_entities = Z_Malloc( sizeof( entity_state_t ) * svs.num_client_entities );
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
		svs.clients[i].edict = ent;
	}

	svgame.numEntities = svgame.globals->maxClients + 1; // clients + world
	svs.initialized = true;
}

qboolean SV_Active( void )
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
	SV_LoadProgs( GI->game_dll );
}

qboolean SV_NewGame( const char *mapName, qboolean loadGame )
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