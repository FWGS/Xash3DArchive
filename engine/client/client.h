//=======================================================================
//			Copyright XashXT Group 2009 �
//		      client.h -- primary header for client
//=======================================================================

#ifndef CLIENT_H
#define CLIENT_H

#include "mathlib.h"
#include "cdll_int.h"
#include "menu_int.h"
#include "cl_entity.h"
#include "com_model.h"
#include "cm_local.h"
#include "pm_defs.h"
#include "pm_movevars.h"
#include "screenfade.h"
#include "protocol.h"
#include "netchan.h"
#include "world.h"

#define MAX_DEMOS		32
#define MAX_MOVIES		8
#define MAX_CDTRACKS	32
#define MAX_IMAGES		256	// SpriteTextures
#define MAX_EFRAGS		640

// screenshot types
#define VID_SCREENSHOT	0
#define VID_LEVELSHOT	1
#define VID_MINISHOT	2

#define EDICT_FROM_AREA( l )	STRUCT_FROM_LINK( l, cl_entity_t, area )
#define NUM_FOR_EDICT(e)	((int)((cl_entity_t *)(e) - clgame.entities))
#define EDICT_NUM( num )	CL_EDICT_NUM( num, __FILE__, __LINE__ )
#define cl_time()		( cl.time )
#define sv_time()		( cl.mtime[0] )

//=============================================================================
typedef struct frame_s
{
	// received from server
	double		receivedtime;	// time message was received, or -1
	double		latency;
	double		time;		// server timestamp

	local_state_t	local;		// local client state
	entity_state_t	playerstate[MAX_CLIENTS];
	int		num_entities;
	int		first_entity;	// into the circular cl_packet_entities[]

	int		delta_sequence;	// last valid sequence
	qboolean		valid;		// cleared if delta parsing was invalid
} frame_t;

#define CMD_BACKUP		MULTIPLAYER_BACKUP	// allow a lot of command backups for very fast systems
#define CMD_MASK		(CMD_BACKUP - 1)

#define CL_UPDATE_MASK	(CL_UPDATE_BACKUP - 1)
extern int CL_UPDATE_BACKUP;

// the client_t structure is wiped completely at every
// server map change
typedef struct
{
	int		timeoutcount;

	int		servercount;		// server identification for prespawns
	int		validsequence;		// this is the sequence number of the last good
						// world snapshot/update we got.  If this is 0, we can't
						// render a frame yet
	int		parsecount;		// server message counter
	int		parsecountmod;		// modulo with network window
	double		parsecounttime;		// timestamp of parse
									
	qboolean		video_prepped;		// false if on new level or new ref dll
	qboolean		audio_prepped;		// false if on new level or new snd dll
	qboolean		force_refdef;		// vid has changed, so we can't use a paused refdef

	int		delta_sequence;		// acknowledged sequence number

	double		mtime[2];			// the timestamp of the last two messages


	int		last_incoming_sequence;

	qboolean		force_send_usercmd;
	qboolean		thirdperson;

	uint		checksum;			// for catching cheater maps

	client_data_t	data;			// some clientdata holds

	frame_t		frame;			// received from server
	int		surpressCount;		// number of messages rate supressed
	frame_t		frames[MULTIPLAYER_BACKUP];	// alloced on svc_serverdata
	usercmd_t		cmds[MULTIPLAYER_BACKUP];	// each mesage will send several old cmds

	double		time;			// this is the time value that the client
						// is rendering at.  always <= cls.realtime
						// a lerp point for other data
	double		oldtime;			// previous cl.time, time-oldtime is used
						// to decay light values and smooth step ups

	float		lerpFrac;			// interpolation value
	ref_params_t	refdef;			// shared refdef

	char		serverinfo[MAX_INFO_STRING];
	player_info_t	players[MAX_CLIENTS];
	event_state_t	events;

	// predicting stuff
	uint		random_seed;			// for predictable random values
	vec3_t		predicted_origins[CMD_BACKUP];	// for debug comparing against server
	vec3_t		predicted_origin;			// generated by CL_PredictMovement
	vec3_t		predicted_viewofs;
	vec3_t		predicted_angles;
	vec3_t		predicted_velocity;
	vec3_t		prediction_error;

	// server state information
	int		playernum;
	int		maxclients;
	int		movemessages;
	int		num_custombeams;			// server beams count

	char		model_precache[MAX_MODELS][CS_SIZE];
	char		sound_precache[MAX_SOUNDS][CS_SIZE];
	char		event_precache[MAX_EVENTS][CS_SIZE];
	lightstyle_t	lightstyles[MAX_LIGHTSTYLES];

	int		sound_index[MAX_SOUNDS];
	int		decal_index[MAX_DECALS];

	model_t		*worldmodel;			// pointer to world
	efrag_t		*free_efrags;
} client_t;

/*
==================================================================

the client_static_t structure is persistant through an arbitrary number
of server connections

==================================================================
*/
typedef enum
{
	ca_uninitialized = 0,
	ca_disconnected, 	// not talking to a server
	ca_connecting,	// sending request packets to the server
	ca_connected,	// netchan_t established, waiting for svc_serverdata
	ca_active,	// game views should be displayed
	ca_cinematic,	// playing a cinematic, not connected to a server
} connstate_t;

typedef enum
{
	key_console = 0,
	key_game,
	key_menu
} keydest_t;

typedef enum
{
	dl_none,
	dl_model,
	dl_sound,
	dl_generic,
} dltype_t;		// download type

typedef enum
{
	scrshot_inactive,
	scrshot_plaque,  	// levelshot
	scrshot_savegame,	// saveshot
	scrshot_demoshot,	// for demos preview
	scrshot_envshot,	// cubemap view
	scrshot_skyshot	// skybox view
} scrshot_t;

// client screen state
typedef enum
{
	CL_LOADING = 1,	// draw loading progress-bar
	CL_ACTIVE,	// draw normal hud
	CL_PAUSED,	// pause when active
	CL_CHANGELEVEL,	// draw 'loading' during changelevel
} scrstate_t;

typedef struct
{
	char		name[32];
	int		number;	// svc_ number
	int		size;	// if size == -1, size come from first byte after svcnum
	pfnUserMsgHook	func;	// user-defined function	
} cl_user_message_t;

typedef void (*pfnEventHook)( event_args_t *args );

typedef struct
{
	char		name[CS_SIZE];
	word		index;	// event index
	pfnEventHook	func;	// user-defined function
} user_event_t;

typedef struct
{
	int		hFontTexture;		// handle to texture
	wrect_t		fontRc[256];		// rectangles
	qboolean		valid;			// rectangles are valid
} cl_font_t;

typedef struct
{
	// temp handle
	const model_t	*pSprite;			// pointer to current SpriteTexture

	// scissor test
	int		scissor_x;
	int		scissor_y;
	int		scissor_width;
	int		scissor_height;
	qboolean		scissor_test;

	// holds text color
	rgba_t		textColor;
	rgba_t		spriteColor;

	// crosshair members
	const model_t	*pCrosshair;
	wrect_t		rcCrosshair;
	rgba_t		rgbaCrosshair;
	byte		gammaTable[256];
} client_draw_t;

typedef struct
{
	int		gl_texturenum;		// this is a real texnum

	// scissor test
	int		scissor_x;
	int		scissor_y;
	int		scissor_width;
	int		scissor_height;
	qboolean		scissor_test;

	// holds text color
	rgba_t		textColor;
} gameui_draw_t;

typedef struct
{
	// centerprint stuff
	int		lines;
	int		y, time;
	char		message[2048];
	int		totalWidth;
	int		totalHeight;
} center_print_t;

typedef struct
{
	float		time;
	float		duration;
	float		amplitude;
	float		frequency;
	float		next_shake;
	vec3_t		offset;
	float		angle;
	vec3_t		applied_offset;
	float		applied_angle;
} screen_shake_t;

typedef struct
{
	int	(*pfnInitialize)( cl_enginefunc_t *pEnginefuncs, int iVersion );
	int	(*pfnVidInit)( void );
	void	(*pfnInit)( void );
	void	(*pfnShutdown)( void );
	int	(*pfnRedraw)( float flTime, int intermission );
	int	(*pfnUpdateClientData)( client_data_t *cdata, float flTime );
	void	(*pfnReset)( void );
	void	(*pfnPlayerMove)( struct playermove_s *ppmove, int server );
	void	(*pfnPlayerMoveInit)( struct playermove_s *ppmove );
	char	(*pfnPlayerMoveTexture)( char *name );
	int	(*pfnConnectionlessPacket)( const netadr_t *net_from, const char *args, char *buffer, int *size );
	int	(*pfnGetHullBounds)( int hullnumber, float *mins, float *maxs );
	void	(*pfnFrame)( double time );
	void	(*pfnVoiceStatus)( int entindex, qboolean bTalking );
	void	(*pfnDirectorMessage)( int iSize, void *pbuf );
	void	(*pfnPostRunCmd)( local_state_t *from, local_state_t *to, usercmd_t *cmd, int runfuncs, double time, uint random_seed );
	int	(*pfnKey_Event)( int eventcode, int keynum, const char *pszCurrentBinding );
	void	(*pfnDemo_ReadBuffer)( int size, byte *buffer );
	int	(*pfnAddEntity)( int type, cl_entity_t *ent, const char *modelname );
	void	(*pfnCreateEntities)( void );
	void	(*pfnStudioEvent)( const struct mstudioevent_s *event, const cl_entity_t *entity );
	void	(*pfnTxferLocalOverrides)( entity_state_t *state, const clientdata_t *client );
	void	(*pfnProcessPlayerState)( entity_state_t *dst, const entity_state_t *src );
	void	(*pfnTxferPredictionData)( entity_state_t *ps, const entity_state_t *pps, clientdata_t *pcd, const clientdata_t *ppcd, weapon_data_t *wd, const weapon_data_t *pwd );
	void	(*pfnTempEntUpdate)( double frametime, double client_time, double cl_gravity, struct tempent_s **ppTempEntFree, struct tempent_s **ppTempEntActive, int ( *Callback_AddVisibleEntity )( cl_entity_t *pEntity ), void ( *Callback_TempEntPlaySound )( struct tempent_s *pTemp, float damp ));
	int	(*pfnGetStudioModelInterface)( int version, struct r_studio_interface_s **ppinterface, struct engine_studio_api_s *pstudio );
	void	(*pfnDrawNormalTriangles)( void );
	void	(*pfnDrawTransparentTriangles)( void );
	cl_entity_t *(*pfnGetUserEntity)( int index );
	void	*(*KB_Find)( const char *name );
	void	(*CAM_Think)( void );		// camera stuff
	int	(*CL_IsThirdPerson)( void );
	void	(*CL_CreateMove)( float frametime, usercmd_t *cmd, int active );
	void	(*IN_ActivateMouse)( void );
	void	(*IN_DeactivateMouse)( void );
	void	(*IN_MouseEvent)( int mstate );
	void	(*IN_Accumulate)( void );
	void	(*IN_ClearStates)( void );
	void	(*pfnCalcRefdef)( ref_params_t *pparams );
} HUD_FUNCTIONS;

typedef struct
{
	void		*hInstance;		// pointer to client.dll
	HUD_FUNCTIONS	dllFuncs;			// dll exported funcs
	byte		*mempool;			// client edicts pool
	string		mapname;			// map name
	string		maptitle;			// display map title
	string		itemspath;		// path to items description for auto-complete func

	cl_entity_t	*entities;		// dynamically allocated entity array

	int		numEntities;		// actual ents count
	int		maxEntities;

	// movement values from server
	movevars_t	movevars;
	movevars_t	oldmovevars;
	playermove_t	*pmove;			// pmove state

	int		trace_hull;		// used by PM_SetTraceHull
	int		oldcount;			// used by PM_Push\Pop state

	vec3_t		player_mins[4];		// 4 hulls allowed
	vec3_t		player_maxs[4];		// 4 hulls allowed

	cl_user_message_t	msg[MAX_USER_MESSAGES];	// keep static to avoid fragment memory
	user_event_t	*events[MAX_EVENTS];

	string		cdtracks[MAX_CDTRACKS];	// 32 cd-tracks read from cdaudio.txt

	model_t		sprites[MAX_IMAGES];	// client spritetextures
	int		load_sequence;		// for unloading unneeded sprites

	client_draw_t	ds;			// draw2d stuff (hud, weaponmenu etc)
	screenfade_t	fade;			// screen fade
	screen_shake_t	shake;			// screen shake
	center_print_t	centerPrint;		// centerprint variables
	SCREENINFO	scrInfo;			// actual screen info
	rgb_t		palette[256];		// Quake1 palette used for particle colors

	client_textmessage_t *titles;			// title messages, not network messages
	int		numTitles;

	cl_entity_t	viewent;			// viewmodel
} clgame_static_t;

typedef struct
{
	void		*hInstance;		// pointer to client.dll
	UI_FUNCTIONS	dllFuncs;			// dll exported funcs
	byte		*mempool;			// client edicts pool

	cl_entity_t	playermodel;		// uiPlayerSetup drawing model

	gameui_draw_t	ds;			// draw2d stuff (menu images)
	GAMEINFO		gameInfo;			// current gameInfo
	GAMEINFO		*modsInfo[MAX_MODS];	// simplified gameInfo for MainUI

	ui_globalvars_t	*globals;

	qboolean		drawLogo;			// set to TRUE if logo.avi missed or corrupted
	long		logo_xres;
	long		logo_yres;
} menu_static_t;

typedef struct
{
	connstate_t	state;
	qboolean		initialized;
	qboolean		changelevel;		// during changelevel

	// screen rendering information
	float		disable_screen;		// showing loading plaque between levels
						// or changing rendering dlls
						// if time gets > 30 seconds ahead, break it
	int		disable_servercount;	// when we receive a frame and cl.servercount
						// > cls.disable_servercount, clear disable_screen

	qboolean		draw_changelevel;		// draw changelevel image 'Loading...'

	keydest_t		key_dest;

	byte		*mempool;			// client premamnent pool: edicts etc
	
	int		framecount;
	int		quakePort;		// a 16 bit value that allows quake servers
						// to work around address translating routers
	// connection information
	string		servername;		// name of server from original connect
	int		connect_time;		// for connection retransmits

	netchan_t		netchan;
	int		serverProtocol;		// in case we are doing some kind of version hack
	int		challenge;		// from the server to use for connecting

	float		packet_loss;
	double		packet_loss_recalc_time;

	float		nextcmdtime;		// when can we send the next command packet?                
	int		lastoutgoingcommand;	// sequence number of last outgoing command

	// internal images
	int		fillImage;		// used for emulate FillRGBA to avoid wrong draw-sort
	int		particleImage;		// built-in particle and sparks image
	int		pauseIcon;		// draw 'paused' when game in-pause
	int		loadingBar;		// 'loading' progress bar
	int		glowShell;		// for renderFxGlowShell
	HSPRITE		hChromeSprite;		// this is a really HudSprite handle, not texnum!
	cl_font_t		creditsFont;		// shared creditsfont

	int		num_client_entities;	// cl.maxclients * CL_UPDATE_BACKUP * MAX_PACKET_ENTITIES
	int		next_client_entities;	// next client_entity to use
	entity_state_t	*packet_entities;		// [num_client_entities]
	
	file_t		*download;		// file transfer from server
	string		downloadname;
	string		downloadtempname;
	int		downloadnumber;
	dltype_t		downloadtype;

	scrshot_t		scrshot_request;		// request for screen shot
	scrshot_t		scrshot_action;		// in-action
	const float	*envshot_vieworg;		// envshot position
	string		shotname;

	// demo loop control
	int		demonum;			// -1 = don't play demos
	string		demos[MAX_DEMOS];		// when not playing

	// movie playlist
	int		movienum;
	string		movies[MAX_MOVIES];

	// demo recording info must be here, so it isn't clearing on level change
	qboolean		demorecording;
	qboolean		demoplayback;
	qboolean		demowaiting;		// don't record until a non-delta message is received
	qboolean		timedemo;
	string		demoname;			// for demo looping

	file_t		*demofile;
} client_static_t;

#ifdef __cplusplus
extern "C" {
#endif

extern client_t		cl;
extern client_static_t	cls;
extern clgame_static_t	clgame;
extern menu_static_t	menu;

#ifdef __cplusplus
}
#endif

//
// cvars
//
extern convar_t	*cl_predict;
extern convar_t	*cl_smooth;
extern convar_t	*cl_showfps;
extern convar_t	*cl_envshot_size;
extern convar_t	*cl_nodelta;
extern convar_t	*cl_crosshair;
extern convar_t	*cl_showmiss;
extern convar_t	*cl_testlights;
extern convar_t	*cl_solid_players;
extern convar_t	*cl_idealpitchscale;
extern convar_t	*cl_allow_levelshots;
extern convar_t	*cl_lightstyle_lerping;
extern convar_t	*cl_draw_particles;
extern convar_t	*cl_levelshot_name;
extern convar_t	*cl_draw_beams;
extern convar_t	*scr_centertime;
extern convar_t	*scr_download;
extern convar_t	*scr_loading;
extern convar_t	*scr_dark;	// start from dark
extern convar_t	*userinfo;

//=============================================================================

qboolean CL_CheckOrDownloadFile( const char *filename );
void CL_SetLightstyle( int style, const char* s );
void CL_RunLightStyles( void );

void CL_AddEntities( void );
void CL_DecayLights( void );

//=================================================

void CL_PrepVideo( void );
void CL_PrepSound( void );

//
// cl_cmds.c
//
void CL_Quit_f( void );
void CL_ScreenShot_f( void );
void CL_PlayCDTrack_f( void );
void CL_EnvShot_f( void );
void CL_SkyShot_f( void );
void CL_SaveShot_f( void );
void CL_DemoShot_f( void );
void CL_LevelShot_f( void );
void CL_SetSky_f( void );
void SCR_Viewpos_f( void );
void SCR_TimeRefresh_f( void );

//
// cl_main
//
void CL_Init( void );
void CL_SendCommand( void );
void CL_Disconnect_f( void );
void CL_GetChallengePacket( void );
void CL_PingServers_f( void );
void CL_RequestNextDownload( void );
void CL_ClearState( void );

//
// cl_demo.c
//
void CL_DrawDemoRecording( void );
void CL_WriteDemoMessage( sizebuf_t *msg, int head_size );
void CL_ReadDemoMessage( void );
void CL_StopPlayback( void );
void CL_StopRecord( void );
void CL_PlayDemo_f( void );
void CL_StartDemos_f( void );
void CL_Demos_f( void );
void CL_DeleteDemo_f( void );
void CL_Record_f( void );
void CL_Stop_f( void );

//
// cl_game.c
//
void CL_UnloadProgs( void );
qboolean CL_LoadProgs( const char *name );
void CL_ParseUserMessage( sizebuf_t *msg, int svc_num );
void CL_LinkUserMessage( char *pszName, const int svc_num, int iSize );
void CL_ParseTextMessage( sizebuf_t *msg );
void CL_DrawHUD( int state );
void CL_InitEdicts( void );
void CL_FreeEdicts( void );
void CL_ClearWorld( void );
void CL_InitEntity( cl_entity_t *pEdict );
void CL_FreeEntity( cl_entity_t *pEdict );
void CL_CenterPrint( const char *text, float y );
void CL_SetEventIndex( const char *szEvName, int ev_index );
void CL_TextMessageParse( byte *pMemFile, int fileSize );
int pfnDecalIndexFromName( const char *szDecalName );
int CL_FindModelIndex( const char *m );
HSPRITE pfnSPR_Load( const char *szPicName );

_inline cl_entity_t *CL_EDICT_NUM( int n, const char *file, const int line )
{
	if(( n >= 0 ) && ( n < clgame.maxEntities ))
		return clgame.entities + n;
	Host_Error( "CL_EDICT_NUM: bad number %i (called at %s:%i)\n", n, file, line );
	return NULL;	
}

//
// cl_parse.c
//
extern const char *svc_strings[256];
void CL_ParseServerMessage( sizebuf_t *msg );
void CL_ParseTempEntity( sizebuf_t *msg );
qboolean CL_DispatchUserMessage( const char *pszName, int iSize, void *pbuf );
void CL_Download_f( void );

//
// cl_scrn.c
//
void SCR_VidInit( void );
void SCR_EndLoadingPlaque( void );
void SCR_RegisterShaders( void );
void SCR_MakeScreenShot( void );
void SCR_MakeLevelShot( void );
void SCR_NetSpeeds( void );
void SCR_RSpeeds( void );
void SCR_DrawFPS( void );

//
// cl_view.c
//

void V_Init (void);
void V_Shutdown( void );
void V_ClearScene( void );
qboolean V_PreRender( void );
void V_PostRender( void );
void V_RenderView( void );

//
// cl_pmove.c
//
void CL_SetSolidEntities( void );
void CL_SetSolidPlayers( int playernum );
void CL_InitClientMove( void );
void CL_PredictMovement( void );
void CL_CheckPredictionError( void );
qboolean CL_IsPredicted( void );
int CL_TruePointContents( const vec3_t p );
int CL_PointContents( const vec3_t p );

//
// cl_studio.c
//
qboolean CL_InitStudioAPI( void );

//
// cl_frame.c
//
void CL_ParsePacketEntities( sizebuf_t *msg, qboolean delta );
qboolean CL_AddVisibleEntity( cl_entity_t *ent, int entityType );
void CL_UpdateStudioVars( cl_entity_t *ent, entity_state_t *newstate );
qboolean CL_GetEntitySpatialization( int ent, vec3_t origin, vec3_t velocity );
qboolean CL_IsPlayerIndex( int idx );

//
// cl_tent.c
//
int CL_AddEntity( int entityType, cl_entity_t *pEnt );
void CL_WeaponAnim( int iAnim, int body );
void CL_ClearEffects( void );
void CL_TestLights( void );
void CL_DecalShoot( HSPRITE hDecal, int entityIndex, int modelIndex, float *pos, int flags );
void CL_PlayerDecal( HSPRITE hDecal, int entityIndex, float *pos, byte *color );
void CL_QueueEvent( int flags, int index, float delay, event_args_t *args );
void CL_PlaybackEvent( int flags, const edict_t *pInvoker, word eventindex, float delay, float *origin,
	float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
word CL_EventIndex( const char *name );
void CL_ResetEvent( event_info_t *ei );
void CL_FireEvents( void );
void CL_InitParticles( void );
void CL_ClearParticles( void );
void CL_FreeParticles( void );
void CL_DrawParticles( void );
void CL_InitTempEnts( void );
void CL_ClearTempEnts( void );
void CL_FreeTempEnts( void );
void CL_AddTempEnts( void );
void CL_InitViewBeams( void );
void CL_ClearViewBeams( void );
void CL_FreeViewBeams( void );
void CL_DrawBeams( int fTrans );
void CL_AddCustomBeam( cl_entity_t *pEnvBeam );
void CL_KillDeadBeams( cl_entity_t *pDeadEntity );
void CL_ParseViewBeam( sizebuf_t *msg, int beamType );
void CL_RegisterMuzzleFlashes( void );

//
// console.c
//
qboolean Con_Visible( void );
void Con_Init( void );
void Con_VidInit( void );
void Con_ToggleConsole_f( void );
void Con_ClearNotify( void );
void Con_RunConsole( void );
void Con_DrawConsole( void );
void Con_DrawStringLen( const char *pText, int *length, int *height );
int Con_DrawString( int x, int y, const char *string, rgba_t setColor );
void Con_DefaultColor( int r, int g, int b );
void Con_CharEvent( int key );
void Key_Console( int key );
void Con_Close( void );

//
// sound.c
//
void S_StreamRawSamples( int samples, int rate, int width, int channels, const byte *data );
void S_StartBackgroundTrack( const char *intro, const char *loop );
void S_StopBackgroundTrack( void );
void S_StreamSetPause( int pause );
void S_StartStreaming( void );
void S_StopStreaming( void );
void S_BeginRegistration( void );
sound_t S_RegisterSound( const char *sample );
void S_EndRegistration( void );
void S_StartSound( const vec3_t pos, int ent, int chan, sound_t sfx, float vol, float attn, int pitch, int flags );
void S_AmbientSound( const vec3_t pos, int ent, int chan, sound_t handle, float fvol, float attn, int pitch, int flags );
void S_FadeClientVolume( float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds );
void S_StartLocalSound( const char *name );
void S_RenderFrame( struct ref_params_s *fd );
void S_ExtraUpdate( void );

//
// cl_menu.c
//
void UI_UnloadProgs( void );
qboolean UI_LoadProgs( const char *name );
void UI_UpdateMenu( float realtime );
void UI_KeyEvent( int key, qboolean down );
void UI_MouseMove( int x, int y );
void UI_SetActiveMenu( qboolean fActive );
void UI_AddServerToList( netadr_t adr, const char *info );
void UI_GetCursorPos( int *pos_x, int *pos_y );
void UI_SetCursorPos( int pos_x, int pos_y );
void UI_ShowCursor( qboolean show );
qboolean UI_CreditsActive( void );
void UI_CharEvent( int key );
qboolean UI_MouseInRect( void );
qboolean UI_IsVisible( void );
void pfnPIC_Set( HIMAGE hPic, int r, int g, int b, int a );
void pfnPIC_Draw( int x, int y, int width, int height, const wrect_t *prc );
void pfnPIC_DrawTrans( int x, int y, int width, int height, const wrect_t *prc );
void pfnPIC_DrawHoles( int x, int y, int width, int height, const wrect_t *prc );
void pfnPIC_DrawAdditive( int x, int y, int width, int height, const wrect_t *prc );

//
// cl_video.c
//
void SCR_InitCinematic( void );
void SCR_FreeCinematic( void );
qboolean SCR_PlayCinematic( const char *name );
qboolean SCR_DrawCinematic( void );
void SCR_RunCinematic( void );
void SCR_StopCinematic( void );
void CL_PlayVideo_f( void );

//
// vgui_int.cpp
//
#ifdef __cplusplus
extern "C" {
#endif

void VGui_Startup( void );
void VGui_Shutdown( void );
void *VGui_GetPanel( void );
void VGui_Paint( void );
void VGui_ViewportPaintBackground( int extents[4] );

#ifdef __cplusplus
}
#endif

#endif//CLIENT_H