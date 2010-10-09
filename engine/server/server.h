//=======================================================================
//			Copyright XashXT Group 2009 �
//		       server.h - primary header for server
//=======================================================================

#ifndef SERVER_H
#define SERVER_H

#include "mathlib.h"
#include "edict.h"
#include "eiface.h"
#include "cm_local.h"
#include "pm_defs.h"
#include "pm_movevars.h"
#include "entity_state.h"
#include "netchan.h"
#include "world.h"

//=============================================================================
#define MAX_MASTERS		8 			// max recipients for heartbeat packets
#define RATE_MESSAGES	10

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

#define NUM_FOR_EDICT(e)	((int)((edict_t *)(e) - svgame.edicts))
#define EDICT_NUM( num )	SV_EDICT_NUM( num, __FILE__, __LINE__ )
#define STRING( offset )	SV_GetString( offset )
#define MAKE_STRING(str)	SV_AllocString( str )
#define MAX_MULTICAST	2500

#define DVIS_PVS		0
#define DVIS_PHS		1

// convert msecs to float time properly
#define sv_time()		( sv.time )
#define sv_frametime()	( sv.frametime )
#define SV_IsValidEdict( e )	( e && !e->free )

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

typedef struct server_s
{
	sv_state_t	state;		// precache commands are only valid during load

	bool		loadgame;		// client begins should reuse existing entity

	double		time;		// sv.time += sv.frametime
	float		frametime;
	int		framenum;
	int		net_framenum;	// to avoid send edicts twice through portals

	int		hostflags;	// misc server flags: predicting etc

	string		name;		// map name
	string		startspot;	// player_start name on nextmap

	char		configstrings[MAX_CONFIGSTRINGS][CS_SIZE];

	// unreliable data to send to clients.
	sizebuf_t		datagram;
	byte		datagram_buf[NET_MAX_PAYLOAD];

	// reliable data to send to clients.
	sizebuf_t		reliable_datagram;	// copied to all clients at end of frame
	byte		reliable_datagram_buf[NET_MAX_PAYLOAD];

	// the multicast buffer is used to send a message to a set of clients
	sizebuf_t		multicast;
	byte		multicast_buf[MAX_MSGLEN];

	sizebuf_t		signon;
	byte		signon_buf[MAX_MSGLEN];

	model_t		*worldmodel;	// pointer to world

	bool		write_bad_message;	// just for debug
	bool		paused;
} server_t;

typedef struct
{
	int  		num_entities;
	int  		first_entity;		// into the circular sv_packet_entities[]
	double		senttime;			// time the message was transmitted
	float		latency;
	clientdata_t	cd;			// clientdata
} client_frame_t;

typedef struct sv_client_s
{
	cl_state_t	state;

	char		userinfo[MAX_INFO_STRING];	// name, etc (received from client)
	char		physinfo[MAX_INFO_STRING];	// set on server (transmit to client)
	bool		send_message;
	bool		skip_message;

	double		next_messagetime;		// time when we should send next world state update  
	double		next_messageinterval;	// default time to wait for next message

	bool		sendmovevars;
	bool		sendinfo;

	bool		fakeclient;		// This client is a fake player controlled by the game DLL

	int		random_seed;		// fpr predictable random values
	int		lastframe;		// for delta compression
	usercmd_t		lastcmd;			// for filling in big drops

	int		modelindex;		// custom playermodel index
	int		packet_loss;
	int		ping;

	int		message_size[RATE_MESSAGES];	// used to rate drop packets
	int		rate;

	int		surpressCount;		// number of messages rate supressed

	float		addangle;			// add angles to client position

	edict_t		*edict;			// EDICT_NUM(clientnum+1)
	edict_t		*pViewEntity;		// svc_setview member
	char		name[32];			// extracted from userinfo, color string allowed
	int		messagelevel;		// for filtering printed messages

	// the datagram is written to by sound calls, prints, temp ents, etc.
	// it can be harmlessly overflowed.
	sizebuf_t		datagram;
	byte		datagram_buf[MAX_MSGLEN];

	client_frame_t	*frames;			// updates can be delta'd from here
	event_state_t	events;

	byte		*download;		// file being downloaded
	int		downloadsize;		// total bytes (can't use EOF because of paks)
	int		downloadcount;		// bytes sent

	double		lastmessage;		// time when packet was last received
	double		lastconnect;

	int		challenge;		// challenge of this user, randomly generated
	int		userid;			// identifying number on server

	netchan_t		netchan;
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
	bool		connected;
} challenge_t;

typedef struct
{
	char		name[32];
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
	// user messages stuff
	const char	*msg_name;		// just for debug
	sv_user_message_t	msg[MAX_USER_MESSAGES];	// user messages array
	int		msg_size_index;		// write message size at this pos in bitbuf
	int		msg_realsize;		// left in bytes
	int		msg_index;		// for debug messages
	int		msg_dest;			// msg destination ( MSG_ONE, MSG_ALL etc )
	bool		msg_started;		// to avoid include messages
	bool		msg_system;		// this is message with engine index
	edict_t		*msg_ent;			// user message member entity
	vec3_t		msg_org;			// user message member origin

	// catched user messages (nasty hack)
	int		gmsgHudText;		// -1 if not catched

	void		*hInstance;		// pointer to server.dll

	union
	{
		edict_t	*edicts;			// acess by edict number
		void	*vp;			// acess by offset in bytes
	};
	int		numEntities;		// actual entities count

	movevars_t	movevars;			// curstate
	movevars_t	oldmovevars;		// oldstate
	playermove_t	*pmove;			// pmove state

	sv_pushed_t	pushed[256];		// no reason to keep array for all edicts
						// 256 it should be enough for any game situation

	vec3_t		player_mins[4];		// 4 hulls allowed
	vec3_t		player_maxs[4];		// 4 hulls allowed

	globalvars_t	*globals;			// server globals
	DLL_FUNCTIONS	dllFuncs;			// dll exported funcs
	NEW_DLL_FUNCTIONS	dllFuncs2;		// new dll exported funcs (can be NULL)
	byte		*mempool;			// server premamnent pool: edicts etc
	byte		*stringspool;		// for shared strings

	int		hStringTable;		// stringtable handle
	SAVERESTOREDATA	SaveData;			// shared struct, used for save data
} svgame_static_t;

typedef struct
{
	bool		initialized;		// sv_init has completed
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
	entity_state_t	*client_entities;		// [num_client_entities]
	entity_state_t	*baselines;		// [GI->max_edicts]

	double		last_heartbeat;
	challenge_t	challenges[MAX_CHALLENGES];	// to prevent invalid IPs from connecting
} server_static_t;

//=============================================================================

extern	netadr_t		master_adr[MAX_MASTERS];	// address of the master server
extern	server_static_t	svs;			// persistant server info
extern	server_t		sv;			// local server
extern	svgame_static_t	svgame;			// persistant game info

extern	cvar_t		*sv_pausable;		// allows pause in multiplayer
extern	cvar_t		*sv_newunit;
extern	cvar_t		*sv_airaccelerate;
extern	cvar_t		*sv_accelerate;
extern	cvar_t		*sv_friction;
extern	cvar_t		*sv_edgefriction;
extern	cvar_t		*sv_maxvelocity;
extern	cvar_t		*sv_gravity;
extern	cvar_t		*sv_stopspeed;
extern	cvar_t		*sv_check_errors;
extern	cvar_t		*sv_reconnect_limit;
extern	cvar_t		*rcon_password;
extern	cvar_t		*hostname;
extern	cvar_t		*sv_stepheight;
extern	cvar_t		*sv_rollangle;
extern	cvar_t		*sv_rollspeed;
extern	cvar_t		*sv_maxspeed;
extern	cvar_t		*sv_maxclients;
extern	cvar_t		*sv_skyname;
extern	cvar_t		*serverinfo;
extern	cvar_t		*physinfo;
extern	sv_client_t	*sv_client;

//===========================================================
//
// sv_main.c
//
void SV_FinalMessage( char *message, bool reconnect );
void SV_DropClient( sv_client_t *drop );

int SV_ModelIndex( const char *name );
int SV_SoundIndex( const char *name );
int SV_DecalIndex( const char *name );
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
void SV_LevelInit( const char *pMapName, char const *pOldLevel, char const *pLandmarkName, bool loadGame );
bool SV_SpawnServer( const char *server, const char *startspot );
int SV_FindIndex( const char *name, int start, int end, bool create );

//
// sv_phys.c
//
void SV_Physics( void );
void SV_CheckVelocity( edict_t *ent );
bool SV_CheckWater( edict_t *ent );
bool SV_RunThink( edict_t *ent );
void SV_FreeOldEntities( void );
bool SV_TestEntityPosition( edict_t *ent );	// for EntityInSolid checks
bool SV_TestPlayerPosition( edict_t *ent );	// for PlayerInSolid checks

//
// sv_move.c
//
bool SV_WalkMove( edict_t *ent, const vec3_t move, int iMode );
void SV_MoveToOrigin( edict_t *ed, const vec3_t goal, float dist, int iMode );
bool SV_CheckBottom( edict_t *ent, float flStepSize, int iMode );
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
void SV_FullClientUpdate( sv_client_t *cl, sizebuf_t *msg );
void SV_FullUpdateMovevars( sv_client_t *cl, sizebuf_t *msg );
bool SV_ClientConnect( edict_t *ent, char *userinfo );
void SV_ClientThink( sv_client_t *cl, usercmd_t *cmd );
void SV_ExecuteClientMessage( sv_client_t *cl, sizebuf_t *msg );
void SV_ConnectionlessPacket( netadr_t from, sizebuf_t *msg );
edict_t *SV_FakeConnect( const char *netname );
void SV_PreRunCmd( sv_client_t *cl, usercmd_t *ucmd );
void SV_RunCmd( sv_client_t *cl, usercmd_t *ucmd );
void SV_PostRunCmd( sv_client_t *cl );
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

//
// sv_game.c
//
bool SV_LoadProgs( const char *name );
void SV_UnloadProgs( void );
void SV_FreeEdicts( void );
edict_t *SV_AllocEdict( void );
void SV_FreeEdict( edict_t *pEdict );
void SV_InitEdict( edict_t *pEdict );
const char *SV_ClassName( const edict_t *e );
void SV_ConfigString( int index, const char *val );
void SV_SetModel( edict_t *ent, const char *name );
void SV_CopyTraceToGlobal( trace_t *trace );
void SV_SetMinMaxSize( edict_t *e, const float *min, const float *max );
void SV_CreateDecal( const float *origin, int decalIndex, int entityIndex, int modelIndex, int flags );
void SV_PlaybackEventFull( int flags, const edict_t *pInvoker, word eventindex, float delay, float *origin,
	float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
void SV_PlaybackEvent( sizebuf_t *msg, event_info_t *info );
void SV_BaselineForEntity( edict_t *pEdict );
void SV_WriteEntityPatch( const char *filename );
script_t *SV_GetEntityScript( const char *filename );
float SV_AngleMod( float ideal, float current, float speed );
void SV_SpawnEntities( const char *mapname, script_t *entities );
edict_t* SV_AllocPrivateData( edict_t *ent, string_t className );
string_t SV_AllocString( const char *szValue );
sv_client_t *SV_ClientFromEdict( const edict_t *pEdict, bool spawned_only );
const char *SV_GetString( string_t iString );
void SV_SetClientMaxspeed( sv_client_t *cl, float fNewMaxspeed );
int SV_MapIsValid( const char *filename, const char *spawn_entity, const char *landmark_name );
void SV_StartSound( edict_t *ent, int chan, const char *sample, float vol, float attn, int flags, int pitch );
edict_t* pfnPEntityOfEntIndex( int iEntIndex );
int pfnIndexOfEdict( const edict_t *pEdict );
void SV_UpdateBaseVelocity( edict_t *ent );
script_t *CM_GetEntityScript( void );

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
bool SV_LoadGame( const char *pName );
void SV_ChangeLevel( bool loadfromsavedgame, const char *mapname, const char *start );
const char *SV_GetLatestSave( void );
int SV_LoadGameState( char const *level, bool createPlayers );
void SV_LoadAdjacentEnts( const char *pOldLevel, const char *pLandmarkName );

//
// sv_studio.c
//
void SV_InitStudioHull( void );
bool SV_StudioExtractBbox( model_t *mod, int sequence, float *mins, float *maxs );
void SV_StudioGetAttachment( edict_t *e, int iAttachment, float *org, float *ang );
trace_t SV_TraceHitbox( edict_t *ent, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end );
void SV_GetBonePosition( edict_t *e, int iBone, float *org, float *ang );

//
// sv_pmove.c
//
bool SV_CopyEdictToPhysEnt( physent_t *pe, edict_t *ed, bool player_trace );

//
// sv_world.c
//

extern areanode_t	sv_areanodes[];

void SV_ClearWorld( void );
void SV_UnlinkEdict( edict_t *ent );
bool SV_HeadnodeVisible( mnode_t *node, byte *visbits );
int SV_HullPointContents( hull_t *hull, int num, const vec3_t p );
trace_t SV_TraceHull( edict_t *ent, int hullNum, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end );
trace_t SV_Move( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int type, edict_t *e );
trace_t SV_MoveHull( const vec3_t start, int hullNumber, const vec3_t end, int type, edict_t *e );
trace_t SV_MoveToss( edict_t *tossent, edict_t *ignore );
void SV_LinkEdict( edict_t *ent, bool touch_triggers );
void SV_TouchLinks( edict_t *ent, areanode_t *node );
int SV_TruePointContents( const vec3_t p );
int SV_PointContents( const vec3_t p );

#endif//SERVER_H