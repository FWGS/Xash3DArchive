//=======================================================================
//			Copyright XashXT Group 2007 ©
//		common.h - definitions common between client and server
//=======================================================================
#ifndef COMMON_H
#define COMMON_H

#include <setjmp.h>
#include <stdio.h>
#include <time.h>
#include <windows.h>

#include "launch_api.h"
#include "engine_api.h"
#include "render_api.h"
#include "com_export.h"
#include "com_model.h"

// PERFORMANCE INFO
#define MIN_FPS         	0.1		// host minimum fps value for maxfps.
#define MAX_FPS         	1000.0		// upper limit for maxfps.

#define MAX_FRAMETIME	0.1
#define MIN_FRAMETIME	0.001

#define MAX_RENDERS		8		// max libraries to keep tracking
#define MAX_ENTNUMBER	99999		// for server and client parsing
#define MAX_HEARTBEAT	-99999		// connection time

#define CIN_MAIN		0
#define CIN_LOGO		1

// config strings are a general means of communication from
// the server to all connected clients.
// each config string can be at most CS_SIZE characters.
#define CS_SIZE			64	// size of one config string
#define CS_TIME			16	// size of time string

#define MAX_MSGLEN			32768	// max length of network message
					// FIXME: replace with NET_MAX_PAYLOAD

#ifdef _DEBUG
void DBG_AssertFunction( qboolean fExpr, const char* szExpr, const char* szFile, int szLine, const char* szMessage );
#define Assert( f )		DBG_AssertFunction( f, #f, __FILE__, __LINE__, NULL )
#else
#define Assert( f )
#endif

extern convar_t	*scr_width;
extern convar_t	*scr_height;
extern convar_t	*scr_download;
extern convar_t	*allow_download;
extern convar_t	*host_limitlocal;
extern convar_t	*host_maxfps;

/*
==============================================================

HOST INTERFACE

==============================================================
*/
#define MAX_SYSEVENTS	1024

typedef enum
{
	HOST_INIT = 0,	// initalize operations
	HOST_FRAME,	// host running
	HOST_SHUTDOWN,	// shutdown operations	
	HOST_ERROR,	// host stopped by error
	HOST_SLEEP,	// sleeped by different reason, e.g. minimize window
	HOST_NOFOCUS,	// same as HOST_FRAME, but disable mouse
	HOST_RESTART,	// during the changes video mode
	HOST_CRASHED	// an exception handler called
} host_state;

typedef enum
{
	RD_NONE = 0,
	RD_CLIENT,
	RD_PACKET
} rdtype_t;

// game print level
typedef enum
{
	PRINT_LOW,	// pickup messages
	PRINT_MEDIUM,	// death messages
	PRINT_HIGH,	// critical messages
	PRINT_CHAT,	// chat messages
} messagelevel_t;

typedef struct host_redirect_s
{
	rdtype_t		target;
	char		*buffer;
	int		buffersize;
	netadr_t		address;
	void		(*flush)( netadr_t adr, rdtype_t target, char *buffer );
} host_redirect_t;

typedef struct host_parm_s
{
	HINSTANCE		hInst;
	host_state	state;		// global host state
	uint		type;		// running at
	jmp_buf		abortframe;	// abort current frame
	dword		errorframe;	// to avoid each-frame host error
	byte		*mempool;		// static mempool for misc allocations
	string		finalmsg;		// server shutdown final message
	host_redirect_t	rd;		// remote console

	double		realtime;		// host.curtime
	double		frametime;	// time between engine frames
	double		realframetime;	// for some system events, e.g. console animations

	uint		framecount;	// global framecount

	int		events_head;
	int		events_tail;
	sys_event_t	events[MAX_SYSEVENTS];

	// list of unique decal indexes
	char		draw_decals[MAX_DECALS][CS_SIZE];

	HWND		hWnd;		// main window
	int		developer;	// show all developer's message
	qboolean		key_overstrike;	// key overstrike mode

	// for IN_MouseMove() easy access
	int		window_center_x;
	int		window_center_y;

	// renderers info
	char		*video_dlls[MAX_RENDERS];
	int		num_video_dlls;

	decallist_t	*decalList;	// used for keep decals, when renderer is restarted or changed
	int		numdecals;

	soundlist_t	*soundList;	// used for keep ambient sounds, when renderer or sound is restarted
	int		numsounds;
} host_parm_t;

extern host_parm_t	host;

//
// build.c
//
int com_buildnum( void );

//
// host.c
//
void Host_Init( const int argc, const char **argv );
void Host_Main( void );
void Host_Free( void );
void Host_SetServerState( int state );
int Host_ServerState( void );
int Host_CompareFileTime( long ft1, long ft2 );
qboolean Host_NewGame( const char *mapName, qboolean loadGame );
int Host_CreateDecalList( decallist_t *pList, qboolean changelevel );
void Host_EndGame( const char *message, ... );
void Host_AbortCurrentFrame( void );
void Host_WriteServerConfig( const char *name );
void Host_WriteOpenGLConfig( void );
void Host_WriteConfig( void );
qboolean Host_IsLocalGame( void );
void Host_ShutdownServer( void );
void Host_CheckChanges( void );
void Host_Print( const char *txt );
void Host_Error( const char *error, ... );
void Host_Credits( void );

/*
==============================================================

CLIENT / SERVER SYSTEMS

==============================================================
*/
void CL_Init( void );
void CL_Shutdown( void );
void Host_ClientFrame( void );
qboolean CL_Active( void );

void SV_Init( void );
void SV_Shutdown( qboolean reconnect );
void Host_ServerFrame( void );
qboolean SV_Active( void );

/*
==============================================================

	SHARED ENGFUNCS

==============================================================
*/
cvar_t *pfnCvar_RegisterVariable( const char *szName, const char *szValue, int flags );
char *pfnMemFgets( byte *pMemFile, int fileSize, int *filePos, char *pBuffer, int bufferSize );
byte* pfnLoadFile( const char *filename, int *pLength );
void pfnCVarSetString( const char *szName, const char *szValue );
void pfnCVarSetValue( const char *szName, float flValue );
float pfnCVarGetValue( const char *szName );
char* pfnCVarGetString( const char *szName );
cvar_t *pfnCVarGetPointer( const char *szVarName );
void pfnFreeFile( void *buffer );
int pfnFileExists( const char *filename );
void *pfnLoadLibrary( const char *name );
void *pfnGetProcAddress( void *hInstance, const char *name );
void pfnFreeLibrary( void *hInstance );
long pfnRandomLong( long lLow, long lHigh );
float pfnRandomFloat( float flLow, float flHigh );
void pfnVecToAngles( const float *rgflVectorIn, float *rgflVectorOut );
int pfnAddCommand( const char *cmd_name, xcommand_t func );
void pfnDelCommand( const char *cmd_name );
void *Cache_Check( byte *mempool, struct cache_user_s *c );
edict_t* pfnPEntityOfEntIndex( int iEntIndex );
void pfnGetGameDir( char *szGetGameDir );
char *pfnCmd_Args( void );
char *pfnCmd_Argv( int argc );
void Con_DPrintf( char *fmt, ... );
void Con_Printf( char *szFmt, ... );
int pfnCmd_Argc( void );
int pfnIsInGame( void );
float pfnTime( void );

/*
==============================================================

	MISC COMMON FUNCTIONS

==============================================================
*/
#define Z_Malloc( size )		Mem_Alloc( host.mempool, size )
#define Z_Realloc( ptr, size )	Mem_Realloc( host.mempool, ptr, size )
#define Z_Free( ptr )		if( ptr ) Mem_Free( ptr )

//
// keys.c
//
qboolean Key_IsDown( int keynum );
const char *Key_IsBind( int keynum );
void Key_Event( int key, qboolean down );
void Key_Init( void );
void Key_WriteBindings( file_t *f );
const char *Key_GetBinding( int keynum );
void Key_SetBinding( int keynum, const char *binding );
void Key_ClearStates( void );
const char *Key_KeynumToString( int keynum );
int Key_StringToKeynum( const char *str );
int Key_GetKey( const char *binding );
void Key_EnumCmds_f( void );
void Key_SetKeyDest( int key_dest );

//
// avikit.c
//
typedef struct movie_state_s	movie_state_t;
long AVI_GetVideoFrameNumber( movie_state_t *Avi, float time );
byte *AVI_GetVideoFrame( movie_state_t *Avi, long frame );
qboolean AVI_GetVideoInfo( movie_state_t *Avi, long *xres, long *yres, float *duration );
qboolean AVI_GetAudioInfo( movie_state_t *Avi, wavdata_t *snd_info );
fs_offset_t AVI_GetAudioChunk( movie_state_t *Avi, char *audiodata, long offset, long length );
void AVI_OpenVideo( movie_state_t *Avi, const char *filename, qboolean load_audio, qboolean ignore_hwgamma, qboolean quiet );
void AVI_CloseVideo( movie_state_t *Avi );
qboolean AVI_IsActive( movie_state_t *Avi );
movie_state_t *AVI_GetState( int num );

// shared calls
qboolean CL_IsInGame( void );
qboolean CL_IsInMenu( void );
qboolean CL_IsThirdPerson( void );
float CL_GetServerTime( void );
float CL_GetLerpFrac( void );
void CL_CharEvent( int key );
void Tri_DrawTriangles( int fTrans );
int CL_PointContents( const vec3_t point );
char *COM_ParseFile( char *data, char *token );
byte *COM_LoadFile( const char *filename, int usehunk, int *pLength );
qboolean CL_GetEntitySpatialization( int entnum, vec3_t origin, vec3_t velocity );
void CL_StudioEvent( struct mstudioevent_s *event, struct cl_entity_s *ent );
qboolean CL_GetComment( const char *demoname, char *comment );
void COM_AddAppDirectoryToSearchPath( const char *pszBaseDir, const char *appName );
int COM_ExpandFilename( const char *fileName, char *nameOutBuffer, int nameOutBufferSize );
struct pmtrace_s *PM_TraceLine( float *start, float *end, int flags, int usehull, int ignore_pe );
void SV_StartSound( edict_t *ent, int chan, const char *sample, float vol, float attn, int flags, int pitch );
struct cl_entity_s *CL_GetEntityByIndex( int index );
struct cl_entity_s *CL_GetLocalPlayer( void );
struct player_info_s *CL_GetPlayerInfo( int playerIndex );
void CL_ExtraUpdate( void );
int CL_GetMaxClients( void );
qboolean CL_IsPlaybackDemo( void );
qboolean CL_LoadProgs( const char *name );
qboolean SV_GetComment( const char *savename, char *comment );
qboolean SV_NewGame( const char *mapName, qboolean loadGame );
void SV_SysError( const char *error_string );
void SV_InitGameProgs( void );
void SV_ForceError( void );
void CL_WriteMessageHistory( void );
void CL_MouseEvent( int mx, int my );
void CL_SendCmd( void );
void CL_Disconnect( void );
qboolean CL_NextDemo( void );
void CL_Drop( void );
void CL_ForceVid( void );
void CL_ForceSnd( void );
void SCR_Init( void );
void SCR_UpdateScreen( void );
void SCR_CheckStartupVids( void );
long SCR_GetAudioChunk( char *rawdata, long length );
wavdata_t *SCR_GetMovieInfo( void );
void SCR_Shutdown( void );
void Con_Print( const char *txt );
void Con_NPrintf( int idx, char *fmt, ... );
void Con_NXPrintf( struct con_nprint_s *info, char *fmt, ... );
char *Info_ValueForKey( const char *s, const char *key );
void Info_RemovePrefixedKeys( char *start, char prefix );
qboolean Info_RemoveKey( char *s, const char *key );
qboolean Info_SetValueForKey( char *s, const char *key, const char *value );
qboolean Info_Validate( const char *s );
void Info_Print( const char *s );
char *Cvar_Userinfo( void );
char *Cvar_Serverinfo( void );
void Cmd_WriteVariables( file_t *f );
qboolean Cmd_CheckMapsList( qboolean fRefresh );
void Cmd_ForwardToServer( void );
void Cmd_AutoComplete( char *complete_string );

typedef struct autocomplete_list_s
{
	const char *name;
	qboolean (*func)( const char *s, char *name, int length );
} autocomplete_list_t;

extern autocomplete_list_t cmd_list[];

// soundlib shared exports
qboolean S_Init( void *hInst );
void S_Shutdown( void );
void S_Activate( qboolean active, void *hInst );
void S_StopSound( int entnum, int channel, const char *soundname );
int S_GetCurrentStaticSounds( soundlist_t *pout, int size, int entchannel );
void S_StopAllSounds( void );

#endif//COMMON_H