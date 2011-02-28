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
#include "ref_params.h"
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
#define QCHAR_WIDTH		16		// font width

#define CIN_MAIN		0
#define CIN_LOGO		1

// config strings are a general means of communication from
// the server to all connected clients.
// each config string can be at most CS_SIZE characters.
#define CS_SIZE		64	// size of one config string
#define CS_TIME		16	// size of time string

#define MAX_DECALS		512	// touching TE_DECAL messages, etc
#define MAX_MSGLEN		32768	// max length of network message
				// FIXME: replace with NET_MAX_PAYLOAD

#ifdef _DEBUG
void DBG_AssertFunction( qboolean fExpr, const char* szExpr, const char* szFile, int szLine, const char* szMessage );
#define Assert( f )		DBG_AssertFunction( f, #f, __FILE__, __LINE__, NULL )
#else
#define Assert( f )
#endif

extern convar_t	*scr_width;
extern convar_t	*scr_height;
extern convar_t	*scr_loading;
extern convar_t	*scr_download;
extern convar_t	*allow_download;
extern convar_t	*cl_allow_levelshots;
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

	byte		*imagepool;	// imagelib mempool
	byte		*soundpool;	// soundlib mempool

	// for IN_MouseMove() easy access
	int		window_center_x;
	int		window_center_y;

	decallist_t	*decalList;	// used for keep decals, when renderer is restarted or changed
	int		numdecals;

	soundlist_t	*soundList;	// used for keep ambient sounds, when renderer or sound is restarted
	int		numsounds;
} host_parm_t;

#ifdef __cplusplus
extern "C" {
#endif

extern host_parm_t	host;

#ifdef __cplusplus
}
#endif

/*
========================================================================

internal image format

typically expanded to rgba buffer
NOTE: number at end of pixelformat name it's a total bitscount e.g. PF_RGB_24 == PF_RGB_888
========================================================================
*/
typedef enum
{
	PF_UNKNOWN = 0,
	PF_INDEXED_24,	// inflated palette (768 bytes)
	PF_INDEXED_32,	// deflated palette (1024 bytes)
	PF_RGBA_32,	// normal rgba buffer
	PF_BGRA_32,	// big endian RGBA (MacOS)
	PF_RGB_24,	// uncompressed dds or another 24-bit image 
	PF_BGR_24,	// big-endian RGB (MacOS)
	PF_TOTALCOUNT,	// must be last
} pixformat_t;

typedef struct bpc_desc_s
{
	int	format;	// pixelformat
	char	name[16];	// used for debug
	uint	glFormat;	// RGBA format
	int	bpp;	// channels (e.g. rgb = 3, rgba = 4)
} bpc_desc_t;

// imagelib global settings
typedef enum
{
	IL_USE_LERPING	= BIT(0),	// lerping images during resample
	IL_KEEP_8BIT	= BIT(1),	// don't expand paletted images
	IL_ALLOW_OVERWRITE	= BIT(2),	// allow to overwrite stored images
} ilFlags_t;

// rgbdata output flags
typedef enum
{
	// rgbdata->flags
	IMAGE_CUBEMAP	= BIT(0),		// it's 6-sides cubemap buffer
	IMAGE_HAS_ALPHA	= BIT(1),		// image contain alpha-channel
	IMAGE_HAS_COLOR	= BIT(2),		// image contain RGB-channel
	IMAGE_COLORINDEX	= BIT(3),		// all colors in palette is gradients of last color (decals)
	IMAGE_HAS_LUMA	= BIT(4),		// image has luma pixels (q1-style maps)
	IMAGE_SKYBOX	= BIT(5),		// only used by FS_SaveImage - for write right suffixes
	IMAGE_QUAKESKY	= BIT(6),		// it's a quake sky double layered clouds (so keep it as 8 bit)
	IMAGE_STATIC	= BIT(7),		// never trying to free this image (static memory)

	// Image_Process manipulation flags
	IMAGE_FLIP_X	= BIT(16),	// flip the image by width
	IMAGE_FLIP_Y	= BIT(17),	// flip the image by height
	IMAGE_ROT_90	= BIT(18),	// flip from upper left corner to down right corner
	IMAGE_ROT180	= IMAGE_FLIP_X|IMAGE_FLIP_Y,
	IMAGE_ROT270	= IMAGE_FLIP_X|IMAGE_FLIP_Y|IMAGE_ROT_90,	
	IMAGE_ROUND	= BIT(19),	// round image to nearest Pow2
	IMAGE_RESAMPLE	= BIT(20),	// resample image to specified dims
	IMAGE_PALTO24	= BIT(21),	// turn 32-bit palette into 24-bit mode (only for indexed images)
	IMAGE_ROUNDFILLER	= BIT(22),	// round image to Pow2 and fill unused entries with single color	
	IMAGE_FORCE_RGBA	= BIT(23),	// force image to RGBA buffer
	IMAGE_MAKE_LUMA	= BIT(24),	// create luma texture from indexed
} imgFlags_t;

typedef struct rgbdata_s
{
	word	width;		// image width
	word	height;		// image height
	uint	type;		// compression type
	uint	flags;		// misc image flags
	byte	*palette;		// palette if present
	byte	*buffer;		// image buffer
	rgba_t	fogParams;	// some water textures in hl1 has info about fog color and alpha
	size_t	size;		// for bounds checking
} rgbdata_t;

//
// imagelib
//
void Image_Init( void );
void Image_Shutdown( void );
rgbdata_t *FS_LoadImage( const char *filename, const byte *buffer, size_t size );
qboolean FS_SaveImage( const char *filename, rgbdata_t *pix );
void FS_FreeImage( rgbdata_t *pack );
extern const bpc_desc_t PFDesc[];	// image get pixelformat
qboolean Image_Process( rgbdata_t **pix, int width, int height, uint flags );

/*
========================================================================

internal sound format

typically expanded to wav buffer
========================================================================
*/
typedef enum
{
	WF_UNKNOWN = 0,
	WF_PCMDATA,
	WF_MPGDATA,
	WF_TOTALCOUNT,	// must be last
} sndformat_t;

// imagelib global settings
typedef enum
{
	SL_USE_LERPING	= BIT(0),		// lerping sounds during resample
	SL_KEEP_8BIT	= BIT(1),		// don't expand 8bit sounds automatically up to 16 bit
	SL_ALLOW_OVERWRITE	= BIT(2),		// allow to overwrite stored sounds
} slFlags_t;

// wavdata output flags
typedef enum
{
	// wavdata->flags
	SOUND_LOOPED	= BIT( 0 ),	// this is looped sound (contain cue markers)
	SOUND_STREAM	= BIT( 1 ),	// this is a streaminfo, not a real sound

	// Sound_Process manipulation flags
	SOUND_RESAMPLE	= BIT(12),	// resample sound to specified rate
	SOUND_CONVERT16BIT	= BIT(13),	// change sound resolution from 8 bit to 16
} sndFlags_t;

typedef struct
{
	word	rate;		// num samples per second (e.g. 11025 - 11 khz)
	byte	width;		// resolution - bum bits divided by 8 (8 bit is 1, 16 bit is 2)
	byte	channels;		// num channels (1 - mono, 2 - stereo)
	int	loopStart;	// offset at this point sound will be looping while playing more than only once
	int	samples;		// total samplecount in wav
	uint	type;		// compression type
	uint	flags;		// misc sound flags
	byte	*buffer;		// sound buffer
	size_t	size;		// for bounds checking
} wavdata_t;

//
// soundlib
//
void Sound_Init( void );
void Sound_Shutdown( void );
wavdata_t *FS_LoadSound( const char *filename, const byte *buffer, size_t size );
void FS_FreeSound( wavdata_t *pack );
stream_t *FS_OpenStream( const char *filename );
wavdata_t *FS_StreamInfo( stream_t *stream );
long FS_ReadStream( stream_t *stream, int bytes, void *buffer );
void FS_FreeStream( stream_t *stream );
qboolean Sound_Process( wavdata_t **wav, int rate, int width, uint flags );

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
void Host_EndGame( const char *message, ... );
void Host_AbortCurrentFrame( void );
void Host_WriteServerConfig( const char *name );
void Host_WriteOpenGLConfig( void );
void Host_WriteVideoConfig( void );
void Host_WriteConfig( void );
qboolean Host_IsLocalGame( void );
void Host_ShutdownServer( void );
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
int pfnFileExists( const char *filename, int gamedironly );
void *pfnLoadLibrary( const char *name );
void *pfnGetProcAddress( void *hInstance, const char *name );
void pfnFreeLibrary( void *hInstance );
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
// crclib.c
//
void CRC32_Init( dword *pulCRC );
byte CRC32_BlockSequence( byte *base, int length, int sequence );
void CRC32_ProcessBuffer( dword *pulCRC, const void *pBuffer, int nBuffer );
void CRC32_ProcessByte( dword *pulCRC, byte ch );
void CRC32_Final( dword *pulCRC );
qboolean CRC32_File( dword *crcvalue, const char *filename );
qboolean CRC32_MapFile( dword *crcvalue, const char *filename );
void MD5Init( MD5Context_t *ctx );
void MD5Update( MD5Context_t *ctx, const byte *buf, uint len );
void MD5Final( byte digest[16], MD5Context_t *ctx );
uint Com_HashKey( const char *string, uint hashSize );

//
// hpak.c
//
void HPAK_Init( void );
qboolean HPAK_GetDataPointer( const char *filename, struct resource_s *pRes, byte **buffer, int *size );
qboolean HPAK_ResourceForHash( const char *filename, char *hash, struct resource_s *pRes );
void HPAK_AddLump( qboolean queue, const char *filename, struct resource_s *pRes, byte *data, file_t *f );
void HPAK_CheckIntegrity( const char *filename );
void HPAK_CheckSize( const char *filename );
void HPAK_FlushHostQueue( void );

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
void AVI_OpenVideo( movie_state_t *Avi, const char *filename, qboolean load_audio, qboolean ignore_hwgamma, int quiet );
void AVI_CloseVideo( movie_state_t *Avi );
qboolean AVI_IsActive( movie_state_t *Avi );
movie_state_t *AVI_GetState( int num );

// shared calls
qboolean CL_IsInGame( void );
qboolean CL_IsInMenu( void );
qboolean CL_IsInConsole( void );
qboolean CL_IsThirdPerson( void );
float CL_GetServerTime( void );
float CL_GetLerpFrac( void );
void CL_CharEvent( int key );
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
int R_CreateDecalList( decallist_t *pList, qboolean changelevel );
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
void SCR_Init( void );
void SCR_UpdateScreen( void );
void SCR_BeginLoadingPlaque( void );
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
long Com_RandomLong( long lMin, long lMax );
float Com_RandomFloat( float fMin, float fMax );

typedef struct autocomplete_list_s
{
	const char *name;
	qboolean (*func)( const char *s, char *name, int length );
} autocomplete_list_t;

extern autocomplete_list_t cmd_list[];

// soundlib shared exports
qboolean S_Init( void );
void S_Shutdown( void );
void S_Activate( qboolean active, void *hInst );
void S_StopSound( int entnum, int channel, const char *soundname );
int S_GetCurrentStaticSounds( soundlist_t *pout, int size );
void S_StopAllSounds( void );

#endif//COMMON_H