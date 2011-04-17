//=======================================================================
//			Copyright XashXT Group 2009 �
//		       server.h - primary header for server
//=======================================================================

#ifndef SERVER_H
#define SERVER_H

#include "mathlib.h"
#include "edict.h"
#include "eiface.h"
#include "mod_local.h"
#include "pm_defs.h"
#include "pm_movevars.h"
#include "entity_state.h"
#include "protocol.h"
#include "netchan.h"
#include "custom.h"
#include "world.h"

//=============================================================================
#define MAX_MASTERS		8 			// max recipients for heartbeat packets

#define SV_UPDATE_MASK	(SV_UPDATE_BACKUP - 1)
extern int SV_UPDATE_BACKUP;

// hostflags
#define SVF_SKIPLOCALHOST	BIT( 0 )
#define SVF_PLAYERSONLY	BIT( 1 )
#define SVF_PORTALPASS	BIT( 2 )			// we are do portal pass

// mapvalid flags
#define MAP_IS_EXIST	BIT( 0 )
#define MAP_HAS_SPAWNPOINT	BIT( 1 )
#define MAP_HAS_LANDMARK	BIT( 2 )
#define MAP_INVALID_VERSION	BIT( 3 )

#define SV_IsValidEdict( e )	( e && !e->free )
#define EDICT_FROM_AREA( l )	STRUCT_FROM_LINK( l, edict_t, area )
#define NUM_FOR_EDICT(e)	((int)((edict_t *)(e) - svgame.edicts))
#define EDICT_NUM( num )	SV_EDICT_NUM( num, __FILE__, __LINE__ )
#define STRING( offset )	SV_GetString( offset )
#define ALLOC_STRING(str)	SV_AllocString( str )
#define MAKE_STRING(str)	(int)(str - svgame.globals->pStringBase)

#define MAX_PUSHED_ENTS	256

#define DVIS_PVS		0
#define DVIS_PHS		1

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

typedef struct server_s
{
	sv_state_t	state;		// precache commands are only valid during load

	qboolean		background;	// this is background map
	qboolean		loadgame;		// client begins should reuse existing entity
	int		viewentity;	// applied on client restore. this is temporare place
					// until client connected

	double		time;		// sv.time += host.frametime
	int		net_framenum;	// to avoid send edicts twice through portals

	int		hostflags;	// misc server flags: predicting etc

	string		name;		// map name
	string		startspot;	// player_start name on nextmap

	char		model_precache[MAX_MODELS][CS_SIZE];
	char		sound_precache[MAX_SOUNDS][CS_SIZE];
	char		files_precache[MAX_CUSTOM][CS_SIZE];
	char		event_precache[MAX_EVENTS][CS_SIZE];

	// run local lightstyles to let SV_LightPoint grab the actual information
	lightstyle_t	lightstyles[MAX_LIGHTSTYLES];

	sv_consistency_t	consistency_files[MAX_MODELS];
	int		num_consistency_files;

	sv_baselines_t	instanced;	// instanced baselines

	// unreliable data to send to clients.
	sizebuf_t		datagram;
	byte		datagram_buf[NET_MAX_PAYLOAD];

	// reliable data to send to clients.
	sizebuf_t		reliable_datagram;	// copied to all clients at end of frame
	byte		reliable_datagram_buf[NET_MAX_PAYLOAD];

	// the multicast buffer is used to send a message to a set of clients
	sizebuf_t		multicast;
	byte		multicast_buf[NET_MAX_PAYLOAD];

	sizebuf_t		signon;
	byte		signon_buf[NET_MAX_PAYLOAD];

	sizebuf_t		spectator_datagram;
	byte		spectator_buf[NET_MAX_PAYLOAD];

	model_t		*worldmodel;	// pointer to world
	uint		checksum;		// for catching cheater maps

	qboolean		write_bad_message;	// just for debug
	qboolean		paused;
} server_t;

typedef struct
{
	double		senttime;
	float		raw_ping;
	float		latency;

	clientdata_t	clientdata;
	weapon_data_t	weapondata[MAX_WEAPONS];
	int  		num_entities;
	int  		first_entity;		// into the circular sv_packet_entities[]
} client_frame_t;

typedef struct sv_client_s
{
	cl_state_t	state;

	char		userinfo[MAX_INFO_STRING];	// name, etc (received from client)
	char		physinfo[MAX_INFO_STRING];	// set on server (transmit to client)

	qboolean		send_message;
	qboolean		skip_message;

	qboolean		local_weapons;		// enable weapon predicting
	qboolean		lag_compensation;		// enable lag compensation
	qboolean		hltv_proxy;		// this is spectator proxy (hltv)		
	qboolean		spectator;		// this is spectator (not a real client)

	netchan_t		netchan;
	int		chokecount;         	// number of messages rate supressed
	int		delta_sequence;		// -1 = no compression.

	double		next_messagetime;		// time when we should send next world state update  
	double		cl_updaterate;		// default time to wait for next message
	double		next_checkpingtime;		// time to send all players pings to client
	double		timebase;			// client timebase

	customization_t	customization;		// player customization linked list
	resource_t	resource1;
	resource_t	resource2;		// <mapname.res> from client (server downloading)

	qboolean		sendmovevars;
	qboolean		sendinfo;

	qboolean		fakeclient;		// This client is a fake player controlled by the game DLL

	usercmd_t		lastcmd;			// for filling in big drops

	double		last_cmdtime;
	double		last_movetime;
	double		next_movetime;

	int		modelindex;		// custom playermodel index
	int		packet_loss;
	float		latency;
	float		ping;

	int		listeners;		// 32 bits == MAX_CLIENTS (voice listeners)

	edict_t		*edict;			// EDICT_NUM(clientnum+1)
	edict_t		*pViewEntity;		// svc_setview member
	char		name[32];			// extracted from userinfo, color string allowed
	int		messagelevel;		// for filtering printed messages

	// the datagram is written to by sound calls, prints, temp ents, etc.
	// it can be harmlessly overflowed.
	sizebuf_t		datagram;
	byte		datagram_buf[NET_MAX_PAYLOAD];

	client_frame_t	*frames;			// updates can be delta'd from here
	event_state_t	events;

	double		lastmessage;		// time when packet was last received
	double		lastconnect;

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
	int		challenge;
	double		time;
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
	qboolean		msg_started;		// to avoid include messages
	qboolean		msg_system;		// this is message with engine index
	edict_t		*msg_ent;			// user message member entity
	vec3_t		msg_org;			// user message member origin

	// catched user messages (nasty hack)
	int		gmsgHudText;		// -1 if not catched

	void		*hInstance;		// pointer to game.dll

	union
	{
		edict_t	*edicts;			// acess by edict number
		void	*vp;			// acess by offset in bytes
	};
	int		numEntities;		// actual entities count
	float		force_retouch;		// always start at new frame to relink all ents

	movevars_t	movevars;			// curstate
	movevars_t	oldmovevars;		// oldstate
	playermove_t	*pmove;			// pmove state
	sv_interp_t	interp[32];		// interpolate clients

#ifdef BUILD_PUSH_LIST
	int		numpushents;		// actual count. don't forget to reset it before
						// call SV_BuildPushList
	edict_t		*pushlist[MAX_PUSHED_ENTS];	// array of edicts that contacted with pusher
#endif
	sv_pushed_t	pushed[MAX_PUSHED_ENTS];	// no reason to keep array for all edicts
						// 256 it should be enough for any game situation

	vec3_t		player_mins[4];		// 4 hulls allowed
	vec3_t		player_maxs[4];		// 4 hulls allowed

	globalvars_t	*globals;			// server globals
	DLL_FUNCTIONS	dllFuncs;			// dll exported funcs
	NEW_DLL_FUNCTIONS	dllFuncs2;		// new dll exported funcs (can be NULL)
	byte		*mempool;			// server premamnent pool: edicts etc
	byte		*stringspool;		// for shared strings

	SAVERESTOREDATA	SaveData;			// shared struct, used for save data
} svgame_static_t;

typedef struct
{
	qboolean		initialized;		// sv_init has completed
	double		timestart;		// just for profiling

	int		groupmask;
	int		groupop;

	double		changelevel_next_time;	// don't execute multiple changelevels at once time
	int		spawncount;		// incremented each server start
						// used to check late spawns
	sv_client_t	*clients;			// [sv_maxclients->integer]
	sv_client_t	*currentPlayer;		// current client who network message sending on
	int		num_client_entities;	// sv_maxclients->integer*UPDATE_BACKUP*MAX_PACKET_ENTITIES
	int		next_client_entities;	// next client_entity to use
	entity_state_t	*packet_entities;		// [num_client_entities]
	entity_state_t	*baselines;		// [GI->max_edicts]

	double		last_heartbeat;
	challenge_t	challenges[MAX_CHALLENGES];	// to prevent invalid IPs from connecting
} server_static_t;

//=============================================================================

extern	netadr_t		master_adr[MAX_MASTERS];	// address of the master server
extern	server_static_t	svs;			// persistant server info
extern	server_t		sv;			// local server
extern	svgame_static_t	svgame;			// persistant game info

extern	convar_t		*sv_pausable;		// allows pause in multiplayer
extern	convar_t		*sv_newunit;
extern	convar_t		*sv_airaccelerate;
extern	convar_t		*sv_accelerate;
extern	convar_t		*sv_friction;
extern	convar_t		*sv_edgefriction;
extern	convar_t		*sv_maxvelocity;
extern	convar_t		*sv_gravity;
extern	convar_t		*sv_stopspeed;
extern	convar_t		*sv_check_errors;
extern	convar_t		*sv_reconnect_limit;
extern	convar_t		*sv_lighting_modulate;
extern	convar_t		*sv_fix_pushents;
extern	convar_t		*rcon_password;
extern	convar_t		*hostname;
extern	convar_t		*sv_stepsize;
extern	convar_t		*sv_rollangle;
extern	convar_t		*sv_rollspeed;
extern	convar_t		*sv_maxspeed;
extern	convar_t		*sv_maxclients;
extern	convar_t		*sv_skyname;
extern	convar_t		*serverinfo;
extern	convar_t		*sv_failuretime;
extern	convar_t		*sv_unlag;
extern	convar_t		*sv_novis;
extern	convar_t		*sv_maxunlag;
extern	convar_t		*sv_unlagpush;
extern	convar_t		*sv_unlagsamples;
extern	convar_t		*sv_allow_upload;
extern	convar_t		*sv_allow_download;
extern	convar_t		*sv_allow_studio_scaling;
extern	convar_t		*sv_allow_studio_attachment_angles;
extern	convar_t		*sv_allow_rotate_pushables;
extern	convar_t		*sv_clienttrace;
extern	convar_t		*sv_send_resources;
extern	convar_t		*sv_send_logos;
extern	convar_t		*sv_sendvelocity;
extern	convar_t		*mp_consistency;
extern	convar_t		*physinfo;

//===========================================================
//
// sv_main.c
//
void SV_FinalMessage( char *message, qboolean reconnect );
void SV_DropClient( sv_client_t *drop );

int SV_ModelIndex( const char *name );
int SV_SoundIndex( const char *name );
int SV_EventIndex( const char *name );
int SV_GenericIndex( const char *name );
int SV_CalcPacketLoss( sv_client_t *cl );
void SV_ExecuteUserCommand (char *s);
void SV_InitOperatorCommands( void );
void SV_KillOperatorCommands( void );
void SV_UserinfoChanged( sv_client_t *cl, const char *userinfo );
void SV_PrepWorldFrame( void );
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
void SV_CheckVelocity( edict_t *ent );
qboolean SV_CheckWater( edict_t *ent );
qboolean SV_RunThink( edict_t *ent );
void SV_FreeOldEntities( void );
qboolean SV_TestEntityPosition( edict_t *ent, edict_t *blocker );	// for EntityInSolid checks
qboolean SV_TestPlayerPosition( edict_t *ent );	// for PlayerInSolid checks
void SV_Impact( edict_t *e1, trace_t *trace );
qboolean SV_CanPushed( edict_t *ent );
void SV_CheckAllEnts( void );

//
// sv_move.c
//
qboolean SV_MoveStep( edict_t *ent, vec3_t move, qboolean relink );
qboolean SV_MoveTest( edict_t *ent, vec3_t move, qboolean relink );
void SV_MoveToOrigin( edict_t *ed, const vec3_t goal, float dist, int iMode );
qboolean SV_CheckBottom( edict_t *ent, int iMode );
float SV_VecToYaw( const vec3_t src );

//
// sv_send.c
//
void SV_SendClientMessages( void );
void SV_ClientPrintf( sv_client_t *cl, int level, char *fmt, ... );
void SV_BroadcastPrintf( int level, char *fmt, ... );
void SV_BroadcastCommand( char *fmt, ... );

//
// sv_client.c
//
char *SV_StatusString( void );
void SV_RefreshUserinfo( void );
void SV_GetChallenge( netadr_t from );
void SV_DirectConnect( netadr_t from );
void SV_TogglePause( const char *msg );
void SV_PutClientInServer( edict_t *ent );
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
void SV_PreRunCmd( sv_client_t *cl, usercmd_t *ucmd, int random_seed );
void SV_RunCmd( sv_client_t *cl, usercmd_t *ucmd, int random_seed );
void SV_PostRunCmd( sv_client_t *cl );
qboolean SV_IsPlayerIndex( int idx );
void SV_InitClientMove( void );
void SV_UpdateServerInfo( void );

//
// sv_cmds.c
//
void SV_Status_f( void );
void SV_Newgame_f( void );

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
void SV_CopyTraceToGlobal( trace_t *trace );
void SV_SetMinMaxSize( edict_t *e, const float *min, const float *max );
void SV_CreateDecal( const float *origin, int decalIndex, int entityIndex, int modelIndex, int flags );
void SV_PlaybackEventFull( int flags, const edict_t *pInvoker, word eventindex, float delay, float *origin,
	float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
void SV_PlaybackEvent( sizebuf_t *msg, event_info_t *info );
void SV_BaselineForEntity( edict_t *pEdict );
void SV_WriteEntityPatch( const char *filename );
char *SV_ReadEntityScript( const char *filename, int *flags );
float SV_AngleMod( float ideal, float current, float speed );
void SV_SpawnEntities( const char *mapname, char *entities );
edict_t* SV_AllocPrivateData( edict_t *ent, string_t className );
string_t SV_AllocString( const char *szValue );
sv_client_t *SV_ClientFromEdict( const edict_t *pEdict, qboolean spawned_only );
const char *SV_GetString( string_t iString );
void SV_SetClientMaxspeed( sv_client_t *cl, float fNewMaxspeed );
int SV_MapIsValid( const char *filename, const char *spawn_entity, const char *landmark_name );
void SV_StartSound( edict_t *ent, int chan, const char *sample, float vol, float attn, int flags, int pitch );
int pfnCheckVisibility( const edict_t *ent, byte *pset );
edict_t* pfnPEntityOfEntIndex( int iEntIndex );
int pfnIndexOfEdict( const edict_t *pEdict );
void SV_UpdateBaseVelocity( edict_t *ent );
byte *pfnSetFatPVS( const float *org );
byte *pfnSetFatPAS( const float *org );
int pfnPrecacheModel( const char *s );
int pfnDecalIndex( const char *m );
int pfnNumberOfEntities( void );

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
const char *SV_GetLatestSave( void );
int SV_LoadGameState( char const *level, qboolean createPlayers );
void SV_LoadAdjacentEnts( const char *pOldLevel, const char *pLandmarkName );
void SV_InitSaveRestore( void );

//
// sv_studio.c
//
void SV_InitStudioHull( void );
qboolean SV_InitStudioAPI( void );
qboolean SV_StudioExtractBbox( model_t *mod, int sequence, float *mins, float *maxs );
void SV_StudioGetAttachment( edict_t *e, int iAttachment, float *org, float *ang );
trace_t SV_TraceHitbox( edict_t *ent, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end );
void SV_GetBonePosition( edict_t *e, int iBone, float *org, float *ang );

//
// sv_world.c
//

extern areanode_t	sv_areanodes[];

void SV_ClearWorld( void );
void SV_UnlinkEdict( edict_t *ent );
qboolean SV_HeadnodeVisible( mnode_t *node, byte *visbits );
trace_t SV_TraceHull( edict_t *ent, int hullNum, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end );
trace_t SV_Move( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int type, edict_t *e );
trace_t SV_MoveHull( const vec3_t start, int hullNumber, const vec3_t end, int type, edict_t *e );
const char *SV_TraceTexture( edict_t *ent, const vec3_t start, const vec3_t end );
trace_t SV_MoveToss( edict_t *tossent, edict_t *ignore );
void SV_LinkEdict( edict_t *ent, qboolean touch_triggers );
void SV_TouchLinks( edict_t *ent, areanode_t *node );
int SV_TruePointContents( const vec3_t p );
int SV_PointContents( const vec3_t p );
void SV_RunLightStyles( void );
void SV_SetLightStyle( int style, const char* s );
int SV_LightForEntity( edict_t *pEdict );

#endif//SERVER_H