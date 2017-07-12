/*
server.h - primary header for server
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

#ifndef SERVER_H
#define SERVER_H

#include "mathlib.h"
#include "edict.h"
#include "eiface.h"
#include "physint.h"	// physics interface
#include "mod_local.h"
#include "pm_defs.h"
#include "pm_movevars.h"
#include "entity_state.h"
#include "protocol.h"
#include "netchan.h"
#include "custom.h"
#include "world.h"

//=============================================================================

#define SV_UPDATE_MASK	(SV_UPDATE_BACKUP - 1)
extern int SV_UPDATE_BACKUP;

// hostflags
#define SVF_SKIPLOCALHOST	BIT( 0 )
#define SVF_PLAYERSONLY	BIT( 1 )
#define SVF_MERGE_VISIBILITY	BIT( 2 )	// we are do portal pass

// mapvalid flags
#define MAP_IS_EXIST	BIT( 0 )
#define MAP_HAS_SPAWNPOINT	BIT( 1 )
#define MAP_HAS_LANDMARK	BIT( 2 )
#define MAP_INVALID_VERSION	BIT( 3 )

#define SV_IsValidEdict( e )	( e && !e->free )
#define NUM_FOR_EDICT(e)	((int)((edict_t *)(e) - svgame.edicts))
#define EDICT_NUM( num )	SV_EDICT_NUM( num, __FILE__, __LINE__ )
#define STRING( offset )	SV_GetString( offset )
#define ALLOC_STRING(str)	SV_AllocString( str )
#define MAKE_STRING(str)	SV_MakeString( str )

#define MAX_PUSHED_ENTS	256
#define MAX_VIEWENTS	128

#define FCL_RESEND_USERINFO	BIT( 0 )
#define FCL_RESEND_MOVEVARS	BIT( 1 )
#define FCL_SKIP_NET_MESSAGE	BIT( 2 )
#define FCL_SEND_NET_MESSAGE	BIT( 3 )
#define FCL_PREDICT_MOVEMENT	BIT( 4 )	// movement prediction is enabled
#define FCL_LOCAL_WEAPONS	BIT( 5 )	// weapon prediction is enabled
#define FCL_LAG_COMPENSATION	BIT( 6 )	// lag compensation is enabled
#define FCL_FAKECLIENT	BIT( 7 )	// this client is a fake player controlled by the game DLL
#define FCL_HLTV_PROXY	BIT( 8 )	// this is a proxy for a HLTV client (spectator)


typedef enum
{
	ss_dead,		// no map loaded
	ss_loading,	// spawning level edicts
	ss_active		// actively running
} sv_state_t;

typedef enum
{
	cs_free = 0,	// can be reused for a new connection
	cs_zombie,	// client has been disconnected, but don't reuse connection for a couple seconds
	cs_connected,	// has been assigned to a sv_client_t, but not in game yet
	cs_spawned	// client is fully in game
} cl_state_t;

// instanced baselines container
typedef struct
{
	int		count;
	string_t		classnames[64];
	entity_state_t	baselines[64];
} sv_baselines_t;

typedef struct
{
	const char	*name;
	FORCE_TYPE	force_state;
	vec3_t		mins, maxs;
} sv_consistency_t;

// like as entity_state_t in Quake
typedef struct
{
	char		model[64];	// name of static-entity model for right precache
	vec3_t		origin;
	vec3_t		angles;
	byte		sequence;
	byte		frame;
	short		colormap;
	byte		skin;		// can't set contents! only real skin!
	byte		rendermode;
	byte		renderamt;
	color24		rendercolor;
	byte		renderfx;
} sv_static_entity_t;

typedef struct server_s
{
	sv_state_t	state;		// precache commands are only valid during load

	qboolean		background;	// this is background map
	qboolean		loadgame;		// client begins should reuse existing entity
	qboolean		changelevel;	// set if changelevel in-action (smooth or classic)
	int		viewentity;	// applied on client restore. this is temporare place
					// until client connected

	double		time;		// sv.time += sv.frametime
	double		time_residual;	// unclamped
	float		frametime;	// 1.0 / sv_fps->value
	int		net_framenum;	// to avoid send edicts twice through portals

	int		hostflags;	// misc server flags: predicting etc

	string		name;		// map name
	string		startspot;	// player_start name on nextmap

	double		lastchecktime;
	int		lastcheck;	// number of last checked client

	char		model_precache[MAX_MODELS][CS_SIZE];
	char		sound_precache[MAX_SOUNDS][CS_SIZE];
	char		files_precache[MAX_CUSTOM][CS_SIZE];
	char		event_precache[MAX_EVENTS][CS_SIZE];

	sv_static_entity_t	static_entities[MAX_STATIC_ENTITIES];
	int		num_static_entities;

	// run local lightstyles to let SV_LightPoint grab the actual information
	lightstyle_t	lightstyles[MAX_LIGHTSTYLES];

	sv_consistency_t	consistency_files[MAX_MODELS];
	resource_t	resources[MAX_MODELS];
	int		num_consistency_resources;	// typically check model bounds on this
	int		num_resources;

	sv_baselines_t	instanced;	// instanced baselines

	// unreliable data to send to clients.
	sizebuf_t		datagram;
	byte		datagram_buf[MAX_DATAGRAM];

	// reliable data to send to clients.
	sizebuf_t		reliable_datagram;	// copied to all clients at end of frame
	byte		reliable_datagram_buf[MAX_DATAGRAM];

	// the multicast buffer is used to send a message to a set of clients
	sizebuf_t		multicast;
	byte		multicast_buf[MAX_MULTICAST];

	sizebuf_t		signon;
	byte		signon_buf[NET_MAX_PAYLOAD];	// need a get to maximum size

	sizebuf_t		spec_datagram;
	byte		spectator_buf[MAX_MULTICAST];

	model_t		*worldmodel;	// pointer to world
	uint		checksum;		// for catching cheater maps

	qboolean		simulating;
	qboolean		write_bad_message;	// just for debug
	qboolean		paused;
} server_t;

typedef struct
{
	double		senttime;
	float		ping_time;

	clientdata_t	clientdata;
	weapon_data_t	weapondata[MAX_LOCAL_WEAPONS];

	int  		num_entities;
	int  		first_entity;		// into the circular sv_packet_entities[]
} client_frame_t;

typedef struct sv_client_s
{
	cl_state_t	state;
	char		name[32];			// extracted from userinfo, color string allowed
	int		flags;			// client flags, some info

	char		userinfo[MAX_INFO_STRING];	// name, etc (received from client)
	char		physinfo[MAX_INFO_STRING];	// set on server (transmit to client)

	netchan_t		netchan;
	int		chokecount;         	// number of messages rate supressed
	int		delta_sequence;		// -1 = no compression.

	double		next_messagetime;		// time when we should send next world state update  
	double		next_checkpingtime;		// time to send all players pings to client
	double		next_sendinfotime;		// time to send info about all players
	double		cl_updaterate;		// client requested updaterate
	double		timebase;			// client timebase
	double		lastservertime;		// check if server time was not changed so no resaon to send update

	customization_t	customdata;		// player customization linked list
	resource_t	resourcesonhand;
	resource_t	resourcesneeded;		// <mapname.res> from client (server downloading)

	usercmd_t		lastcmd;			// for filling in big drops

	double		connecttime;
	double		cmdtime;
	double		ignorecmdtime;

	int		modelindex;		// custom playermodel index
	int		packet_loss;
	float		latency;

	int		ignored_ents;		// if visibility list is full we should know how many entities will be ignored
	int		listeners;		// 32 bits == MAX_CLIENTS (voice listeners)

	edict_t		*edict;			// EDICT_NUM(clientnum+1)
	edict_t		*pViewEntity;		// svc_setview member
	int		messagelevel;		// for filtering printed messages

	edict_t		*viewentity[MAX_VIEWENTS];	// list of portal cameras in player PVS
	int		num_viewents;		// num of portal cameras that can merge PVS

	// the datagram is written to by sound calls, prints, temp ents, etc.
	// it can be harmlessly overflowed.
	sizebuf_t		datagram;
	byte		datagram_buf[MAX_DATAGRAM];

	client_frame_t	*frames;			// updates can be delta'd from here
	event_state_t	events;

	double		lastmessage;		// time when packet was last received
	double		connection_started;

	int		challenge;		// challenge of this user, randomly generated
	int		userid;			// identifying number on server
	int		authentication_method;
	uint		WonID;			// WonID
} sv_client_t;

/*
=============================================================================
 a client can leave the server in one of four ways:
 dropping properly by quiting or disconnecting
 timing out if no valid messages are received for timeout.value seconds
 getting kicked off by the server operator
 a program error, like an overflowed reliable buffer
=============================================================================
*/

// MAX_CHALLENGES is made large to prevent a denial
// of service attack that could cycle all of them
// out before legitimate users connected
#define MAX_CHALLENGES	1024

typedef struct
{
	netadr_t		adr;
	double		time;
	int		challenge;
	qboolean		connected;
} challenge_t;

typedef struct
{
	char		name[32];	// in GoldSrc max name length is 12
	int		number;	// svc_ number
	int		size;	// if size == -1, size come from first byte after svcnum
} sv_user_message_t;

typedef struct
{
	edict_t		*ent;
	vec3_t		origin;
	vec3_t		angles;
	int		fixangle;
} sv_pushed_t;

typedef struct
{
	qboolean		active;
	qboolean		moving;
	qboolean		firstframe;
	qboolean		nointerp;

	vec3_t		mins;
	vec3_t		maxs;

	vec3_t		curpos;
	vec3_t		oldpos;
	vec3_t		newpos;
	vec3_t		finalpos;
} sv_interp_t;

typedef struct
{
	// user messages stuff
	const char	*msg_name;		// just for debug
	sv_user_message_t	msg[MAX_USER_MESSAGES];	// user messages array
	int		msg_size_index;		// write message size at this pos in bitbuf
	int		msg_realsize;		// left in bytes
	int		msg_index;		// for debug messages
	int		msg_dest;			// msg destination ( MSG_ONE, MSG_ALL etc )
	qboolean		msg_started;		// to avoid recursive included messages
	edict_t		*msg_ent;			// user message member entity
	vec3_t		msg_org;			// user message member origin

	// catched user messages (nasty hack)
	int		gmsgHudText;		// -1 if not catched (e.g. mod not registered this message)

	void		*hInstance;		// pointer to game.dll
	qboolean		config_executed;		// should to execute config.cfg once time to restore FCVAR_ARCHIVE that specified in hl.dll

	union
	{
		edict_t	*edicts;			// acess by edict number
		void	*vp;			// acess by offset in bytes
	};
	int		numEntities;		// actual entities count

	movevars_t	movevars;			// curstate
	movevars_t	oldmovevars;		// oldstate
	playermove_t	*pmove;			// pmove state
	sv_interp_t	interp[MAX_CLIENTS];	// interpolate clients

	sv_pushed_t	pushed[MAX_PUSHED_ENTS];	// no reason to keep array for all edicts
						// 256 it should be enough for any game situation

	globalvars_t	*globals;			// server globals
	DLL_FUNCTIONS	dllFuncs;			// dll exported funcs
	NEW_DLL_FUNCTIONS	dllFuncs2;		// new dll exported funcs (may be NULL)
	physics_interface_t	physFuncs;		// physics interface functions (Xash3D extension)
	byte		*mempool;			// server premamnent pool: edicts etc
	byte		*stringspool;		// for engine strings

	SAVERESTOREDATA	SaveData;			// shared struct, used for save data
} svgame_static_t;

typedef struct
{
	qboolean		initialized;		// sv_init has completed
	double		timestart;		// just for profiling

	int		maxclients;		// server max clients

	int		groupmask;
	int		groupop;

	char		serverinfo[MAX_SERVERINFO_STRING];
	char		localinfo[MAX_LOCALINFO_STRING];

	double		changelevel_next_time;	// don't execute multiple changelevels at once time
	int		spawncount;		// incremented each server start
						// used to check late spawns
	sv_client_t	*clients;			// [svs.maxclients]
	sv_client_t	*currentPlayer;		// current client who network message sending on
	int		currentPlayerNum;		// for easy acess to some global arrays
	int		num_client_entities;	// svs.maxclients*UPDATE_BACKUP*MAX_PACKET_ENTITIES
	int		next_client_entities;	// next client_entity to use
	entity_state_t	*packet_entities;		// [num_client_entities]
	entity_state_t	*baselines;		// [GI->max_edicts]

	double		last_heartbeat;
	challenge_t	challenges[MAX_CHALLENGES];	// to prevent invalid IPs from connecting
} server_static_t;

//=============================================================================

extern	server_static_t	svs;			// persistant server info
extern	server_t		sv;			// local server
extern	svgame_static_t	svgame;			// persistant game info
extern	areanode_t	sv_areanodes[];		// AABB dynamic tree

extern convar_t		sv_lan;
extern convar_t		sv_lan_rate;
extern convar_t		sv_unlag;
extern convar_t		sv_maxunlag;
extern convar_t		sv_unlagpush;
extern convar_t		sv_unlagsamples;
extern convar_t		rcon_password;
extern convar_t		sv_instancedbaseline;
extern convar_t		sv_minupdaterate;
extern convar_t		sv_maxupdaterate;
extern convar_t		sv_newunit;
extern convar_t		sv_clienttrace;
extern convar_t		sv_failuretime;
extern convar_t		sv_send_resources;
extern convar_t		sv_send_logos;
extern convar_t		sv_allow_upload;
extern convar_t		sv_allow_download;
extern convar_t		sv_airaccelerate;
extern convar_t		sv_accelerate;
extern convar_t		sv_friction;
extern convar_t		sv_edgefriction;
extern convar_t		sv_gravity;
extern convar_t		sv_stopspeed;
extern convar_t		sv_maxspeed;
extern convar_t		sv_stepsize;
extern convar_t		sv_maxvelocity;
extern convar_t		sv_rollangle;
extern convar_t		sv_rollspeed;
extern convar_t		sv_skyname;
extern convar_t		sv_skyspeed;
extern convar_t		sv_skyangle;
extern convar_t		sv_consistency;
extern convar_t		deathmatch;
extern convar_t		skill;
extern convar_t		coop;

extern	convar_t		*sv_pausable;		// allows pause in multiplayer
extern	convar_t		*sv_check_errors;
extern	convar_t		*sv_reconnect_limit;
extern	convar_t		*sv_lighting_modulate;
extern	convar_t		*hostname;
extern	convar_t		*sv_maxclients;
extern	convar_t		*sv_novis;
extern	convar_t		*sv_allow_studio_attachment_angles;
extern	convar_t		*sv_allow_rotate_pushables;
extern	convar_t		*sv_sendvelocity;
extern	convar_t		*sv_validate_changelevel;
extern	convar_t		*public_server;

//===========================================================
//
// sv_main.c
//
void SV_FinalMessage( char *message, qboolean reconnect );
void SV_DropClient( sv_client_t *drop );
void SV_UpdateMovevars( qboolean initialize );
int SV_ModelIndex( const char *name );
int SV_SoundIndex( const char *name );
int SV_EventIndex( const char *name );
int SV_GenericIndex( const char *name );
int SV_CalcPacketLoss( sv_client_t *cl );
void SV_ExecuteUserCommand (char *s);
void SV_InitOperatorCommands( void );
void SV_KillOperatorCommands( void );
void SV_UserinfoChanged( sv_client_t *cl, const char *userinfo );
void SV_RemoteCommand( netadr_t from, sizebuf_t *msg );
void SV_PrepWorldFrame( void );
void SV_ProcessFile( sv_client_t *cl, char *filename );
void SV_SendResourceList( sv_client_t *cl );
void SV_AddToMaster( netadr_t from, sizebuf_t *msg );
qboolean SV_IsSimulating( void );
void Master_Add( void );
void Master_Heartbeat( void );
void Master_Packet( void );

//
// sv_init.c
//
void SV_InitGame( void );
void SV_ActivateServer( void );
void SV_DeactivateServer( void );
void SV_LevelInit( const char *pMapName, char const *pOldLevel, char const *pLandmarkName, qboolean loadGame );
qboolean SV_SpawnServer( const char *server, const char *startspot );

//
// sv_phys.c
//
void SV_Physics( void );
qboolean SV_InitPhysicsAPI( void );
void SV_CheckVelocity( edict_t *ent );
qboolean SV_CheckWater( edict_t *ent );
qboolean SV_RunThink( edict_t *ent );
qboolean SV_PlayerRunThink( edict_t *ent, float frametime, double time );
qboolean SV_TestEntityPosition( edict_t *ent, edict_t *blocker );	// for EntityInSolid checks
void SV_Impact( edict_t *e1, edict_t *e2, trace_t *trace );
qboolean SV_CanPushed( edict_t *ent );
void SV_FreeOldEntities( void );
void SV_CheckAllEnts( void );

//
// sv_move.c
//
qboolean SV_MoveStep( edict_t *ent, vec3_t move, qboolean relink );
qboolean SV_MoveTest( edict_t *ent, vec3_t move, qboolean relink );
void SV_MoveToOrigin( edict_t *ed, const vec3_t goal, float dist, int iMode );
qboolean SV_CheckBottom( edict_t *ent, int iMode );
float SV_VecToYaw( const vec3_t src );
void SV_WaterMove( edict_t *ent );

//
// sv_send.c
//
void SV_SendClientMessages( void );
void SV_ClientPrintf( sv_client_t *cl, int level, char *fmt, ... );
void SV_BroadcastPrintf( sv_client_t *ignore, int level, char *fmt, ... );
void SV_BroadcastCommand( const char *fmt, ... );

//
// sv_client.c
//
char *SV_StatusString( void );
void SV_RefreshUserinfo( void );
void SV_GetChallenge( netadr_t from );
void SV_DirectConnect( netadr_t from );
void SV_TogglePause( const char *msg );
qboolean SV_ShouldUpdatePing( sv_client_t *cl );
const char *SV_GetClientIDString( sv_client_t *cl );
void SV_FullClientUpdate( sv_client_t *cl, sizebuf_t *msg );
void SV_FullUpdateMovevars( sv_client_t *cl, sizebuf_t *msg );
void SV_GetPlayerStats( sv_client_t *cl, int *ping, int *packet_loss );
qboolean SV_ClientConnect( edict_t *ent, char *userinfo );
void SV_ClientThink( sv_client_t *cl, usercmd_t *cmd );
void SV_ExecuteClientMessage( sv_client_t *cl, sizebuf_t *msg );
void SV_ConnectionlessPacket( netadr_t from, sizebuf_t *msg );
edict_t *SV_FakeConnect( const char *netname );
void SV_ExecuteClientCommand( sv_client_t *cl, char *s );
void SV_RunCmd( sv_client_t *cl, usercmd_t *ucmd, int random_seed );
qboolean SV_IsPlayerIndex( int idx );
int SV_CalcPing( sv_client_t *cl );
void SV_InitClientMove( void );
void SV_UpdateServerInfo( void );
void SV_EndRedirect( void );

//
// sv_cmds.c
//
void SV_Status_f( void );
void SV_Newgame_f( void );
void SV_InitHostCommands( void );

//
// sv_custom.c
//
void SV_SendResources( sizebuf_t *msg );
int SV_TransferConsistencyInfo( void );

//
// sv_frame.c
//
void SV_WriteFrameToClient( sv_client_t *client, sizebuf_t *msg );
void SV_BuildClientFrame( sv_client_t *client );
void SV_InactivateClients( void );
void SV_SendMessagesToAll( void );
void SV_SkipUpdates( void );

//
// sv_game.c
//
qboolean SV_LoadProgs( const char *name );
void SV_UnloadProgs( void );
void SV_FreeEdicts( void );
edict_t *SV_AllocEdict( void );
void SV_FreeEdict( edict_t *pEdict );
void SV_InitEdict( edict_t *pEdict );
const char *SV_ClassName( const edict_t *e );
void SV_SetModel( edict_t *ent, const char *name );
void SV_FreePrivateData( edict_t *pEdict );
void SV_CopyTraceToGlobal( trace_t *trace );
void SV_SetMinMaxSize( edict_t *e, const float *min, const float *max );
edict_t* SV_FindEntityByString( edict_t *pStartEdict, const char *pszField, const char *pszValue );
void SV_PlaybackEventFull( int flags, const edict_t *pInvoker, word eventindex, float delay, float *origin,
	float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
void SV_PlaybackReliableEvent( sizebuf_t *msg, word eventindex, float delay, event_args_t *args );
qboolean SV_BoxInPVS( const vec3_t org, const vec3_t absmin, const vec3_t absmax );
void SV_BaselineForEntity( edict_t *pEdict );
void SV_WriteEntityPatch( const char *filename );
char *SV_ReadEntityScript( const char *filename, int *flags );
float SV_AngleMod( float ideal, float current, float speed );
void SV_SpawnEntities( const char *mapname, char *entities );
edict_t* SV_AllocPrivateData( edict_t *ent, string_t className );
edict_t* SV_CreateNamedEntity( edict_t *ent, string_t className );
string_t SV_AllocString( const char *szValue );
string_t SV_MakeString( const char *szValue );
const char *SV_GetString( string_t iString );
sv_client_t *SV_ClientFromEdict( const edict_t *pEdict, qboolean spawned_only );
void SV_SetClientMaxspeed( sv_client_t *cl, float fNewMaxspeed );
int SV_MapIsValid( const char *filename, const char *spawn_entity, const char *landmark_name );
void SV_StartSound( edict_t *ent, int chan, const char *sample, float vol, float attn, int flags, int pitch );
void SV_CreateStaticEntity( struct sizebuf_s *msg, sv_static_entity_t *ent );
edict_t* pfnPEntityOfEntIndex( int iEntIndex );
int pfnIndexOfEdict( const edict_t *pEdict );
void pfnWriteBytes( const byte *bytes, int count );
void SV_UpdateBaseVelocity( edict_t *ent );
byte *pfnSetFatPVS( const float *org );
byte *pfnSetFatPAS( const float *org );
int pfnPrecacheModel( const char *s );
void pfnRemoveEntity( edict_t* e );
int pfnNumberOfEntities( void );
void SV_RestartStaticEnts( void );
char *SV_Localinfo( void );

_inline edict_t *SV_EDICT_NUM( int n, const char * file, const int line )
{
	if((n >= 0) && (n < svgame.globals->maxEntities))
		return svgame.edicts + n;
	Host_Error( "SV_EDICT_NUM: bad number %i (called at %s:%i)\n", n, file, line );
	return NULL;	
}

//
// sv_save.c
//
void SV_ClearSaveDir( void );
void SV_SaveGame( const char *pName );
qboolean SV_LoadGame( const char *pName );
void SV_ChangeLevel( qboolean loadfromsavedgame, const char *mapname, const char *start );
int SV_LoadGameState( char const *level, qboolean createPlayers );
void SV_LoadAdjacentEnts( const char *pOldLevel, const char *pLandmarkName );
const char *SV_GetLatestSave( void );
void SV_InitSaveRestore( void );

//
// sv_pmove.c
//
void SV_GetTrueOrigin( sv_client_t *cl, int edictnum, vec3_t origin );
void SV_GetTrueMinMax( sv_client_t *cl, int edictnum, vec3_t mins, vec3_t maxs );
qboolean SV_PlayerIsFrozen( edict_t *pClient );

//
// sv_world.c
//
void SV_ClearWorld( void );
void SV_UnlinkEdict( edict_t *ent );
void SV_ClipMoveToEntity( edict_t *ent, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, trace_t *trace );
void SV_CustomClipMoveToEntity( edict_t *ent, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, trace_t *trace );
trace_t SV_TraceHull( edict_t *ent, int hullNum, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end );
trace_t SV_Move( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int type, edict_t *e );
trace_t SV_MoveNoEnts( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int type, edict_t *e );
const char *SV_TraceTexture( edict_t *ent, const vec3_t start, const vec3_t end );
msurface_t *SV_TraceSurface( edict_t *ent, const vec3_t start, const vec3_t end );
trace_t SV_MoveToss( edict_t *tossent, edict_t *ignore );
void SV_LinkEdict( edict_t *ent, qboolean touch_triggers );
void SV_TouchLinks( edict_t *ent, areanode_t *node );
int SV_TruePointContents( const vec3_t p );
int SV_PointContents( const vec3_t p );
void SV_RunLightStyles( void );
void SV_SetLightStyle( int style, const char* s, float f );
const char *SV_GetLightStyle( int style );
int SV_LightForEntity( edict_t *pEdict );
void SV_ClearPhysEnts( void );

#endif//SERVER_H