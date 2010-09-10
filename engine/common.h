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
#include "vsound_api.h"
#include "com_export.h"
#include "com_model.h"
#include "net_msg.h"

// PERFORMANCE INFO
#define MIN_FPS         	0.1		// host minimum fps value for maxfps.
#define MAX_FPS         	1000.0		// upper limit for maxfps.

#define MAX_FRAMETIME	0.1
#define MIN_FRAMETIME	0.001

#define MAX_RENDERS		8		// max libraries to keep tracking
#define MAX_ENTNUMBER	99999		// for server and client parsing
#define MAX_HEARTBEAT	-99999		// connection time

#ifdef _DEBUG
void DBG_AssertFunction( bool fExpr, const char* szExpr, const char* szFile, int szLine, const char* szMessage );
#define Assert( f )		DBG_AssertFunction( f, #f, __FILE__, __LINE__, NULL )
#else
#define Assert( f )
#endif

extern cvar_t	*scr_width;
extern cvar_t	*scr_height;
extern cvar_t	*allow_download;
extern cvar_t	*host_maxfps;

/*
==============================================================

HOST INTERFACE

==============================================================
*/
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
	host_state	state;		// global host state
	uint		type;		// running at
	jmp_buf		abortframe;	// abort current frame
	dword		errorframe;	// to avoid each-frame host error
	byte		*mempool;		// static mempool for misc allocations
	string		finalmsg;		// server shutdown final message
	host_redirect_t	rd;		// remote console

	int		inputmsec;	// time for buttons
	int		frametime;	// time between engine frames
	uint		framecount;	// global framecount

	int		events_head;
	int		events_tail;
	sys_event_t	events[MAX_EVENTS];

	HWND		hWnd;		// main window
	int		developer;	// show all developer's message
	bool		key_overstrike;	// key overstrike mode

	int		window_center_x;
	int		window_center_y;

	// renderers info
	char		*video_dlls[MAX_RENDERS];
	char		*audio_dlls[MAX_RENDERS];
	int		num_video_dlls;
	int		num_audio_dlls;

	decallist_t	*decalList;	// used for keep decals, when renderer is restarted or changed
	int		numdecals;
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
bool Host_NewGame( const char *mapName, bool loadGame );
void Host_EndGame( const char *message, ... );
void Host_AbortCurrentFrame( void );
void Host_WriteServerConfig( const char *name );
void Host_WriteOpenGLConfig( void );
void Host_WriteConfig( void );
bool Host_IsLocalGame( void );
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
void CL_Frame( int time );
bool CL_Active( void );

void SV_Init( void );
void SV_Shutdown( bool reconnect );
void SV_Frame( int time );
bool SV_Active( void );

/*
==============================================================

	SHARED ENGFUNCS

==============================================================
*/
cvar_t *pfnCVarRegister( const char *szName, const char *szValue, int flags, const char *szDesc );
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
void pfnAddCommand( const char *cmd_name, xcommand_t func, const char *cmd_desc );
void pfnDelCommand( const char *cmd_name );
void *Cache_Check( byte *mempool, struct cache_user_s *c );
void pfnGetGameDir( char *szGetGameDir );
const char *pfnCmd_Args( void );
const char *pfnCmd_Argv( int argc );
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
#define Z_Malloc(size)		Mem_Alloc( host.mempool, size )
#define Z_Realloc( ptr, size )	Mem_Realloc( host.mempool, ptr, size )
#define Z_Free( ptr )		if( ptr ) Mem_Free( ptr )

//
// keys.c
//
bool Key_IsDown( int keynum );
const char *Key_IsBind( int keynum );
void Key_Event( int key, bool down, int time );
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
// cinematic.c
//
void CIN_Init( void );
void CIN_ReadChunk( cinematics_t *cin );
byte *CIN_ReadNextFrame( cinematics_t *cin, bool silent );

// shared calls
bool CL_IsInGame( void );
bool CL_IsInMenu( void );
float CL_GetServerTime( void );
void CL_CharEvent( int key );
void Tri_DrawTriangles( int fTrans );
int CL_PointContents( const vec3_t point );
char *COM_ParseFile( char *data, char *token );
void CL_StudioFxTransform( struct cl_entity_s *ent, float transform[4][4] );
bool CL_GetEntitySpatialization( int entnum, vec3_t origin, vec3_t velocity );
void CL_StudioEvent( struct mstudioevent_s *event, struct cl_entity_s *ent );
bool CL_GetComment( const char *demoname, char *comment );
struct pmtrace_s *PM_TraceLine( float *start, float *end, int flags, int usehull, int ignore_pe );
struct cl_entity_s *CL_GetEntityByIndex( int index );
struct cl_entity_s *CL_GetLocalPlayer( void );
struct player_info_s *CL_GetPlayerInfo( int playerIndex );
int CL_GetMaxClients( void );
bool CL_IsPlaybackDemo( void );
bool SV_GetComment( const char *savename, char *comment );
bool SV_NewGame( const char *mapName, bool loadGame );
void SV_SysError( const char *error_string );
void SV_InitGameProgs( void );
void SV_ForceError( void );
void CL_WriteMessageHistory( void );
void CL_MouseEvent( int mx, int my );
void CL_SendCmd( void );
void CL_Disconnect( void );
bool CL_NextDemo( void );
void CL_Drop( void );
void CL_ForceVid( void );
void CL_ForceSnd( void );
void SCR_Init( void );
void SCR_UpdateScreen( void );
void SCR_Shutdown( void );
void Con_Print( const char *txt );
void Con_NPrintf( int idx, char *fmt, ... );
void Con_NXPrintf( struct con_nprint_s *info, char *fmt, ... );
char *Info_ValueForKey( const char *s, const char *key );
void Info_RemovePrefixedKeys( char *start, char prefix );
bool Info_RemoveKey( char *s, const char *key );
bool Info_SetValueForKey( char *s, const char *key, const char *value );
bool Info_Validate( const char *s );
void Info_Print( const char *s );
char *Cvar_Userinfo( void );
char *Cvar_Serverinfo( void );
void Cmd_WriteVariables( file_t *f );
bool Cmd_CheckMapsList( bool fRefresh );
void Cmd_ForwardToServer( void );
void Cmd_AutoComplete( char *complete_string );

typedef struct autocomplete_list_s
{
	const char *name;
	bool (*func)( const char *s, char *name, int length );
} autocomplete_list_t;

extern autocomplete_list_t cmd_list[];

#endif//COMMON_H