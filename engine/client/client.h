//=======================================================================
//			Copyright XashXT Group 2009 �
//		      client.h -- primary header for client
//=======================================================================

#ifndef CLIENT_H
#define CLIENT_H

#include "mathlib.h"
#include "clgame_api.h"
#include "com_world.h"

#define MAX_DEMOS		32
#define MAX_EDIT_LINE	256
#define COMMAND_HISTORY	32
#define MAX_GAME_TITLES	1024
#define ColorIndex(c)	(((c) - '0') & 7)

#define NUM_FOR_EDICT(e) ((int)((edict_t *)(e) - clgame.edicts))
#define EDICT_NUM( num ) CL_EDICT_NUM( num, __FILE__, __LINE__ )
#define STRING( offset ) CL_GetString( offset )
#define MAKE_STRING(str) CL_AllocString( str )

typedef struct player_info_s
{
	char	name[CS_SIZE];
	char	userinfo[MAX_INFO_STRING];
	char	model[CS_SIZE];
} player_info_t;

//=============================================================================
typedef struct frame_s
{
	bool		valid;			// cleared if delta parsing was invalid
	int		serverframe;
	int		servertime;
	int		deltaframe;
	byte		areabits[MAX_MAP_AREA_BYTES];	// portalarea visibility bits
	int		num_entities;
	int		parse_entities;		// non-masked index into cl_parse_entities array
} frame_t;

// console stuff
typedef struct field_s
{
	int	cursor;
	int	scroll;
	int	widthInChars;
	char	buffer[MAX_EDIT_LINE];
	int	maxchars; // menu stuff
} field_t;

#define	CMD_BACKUP		64	// allow a lot of command backups for very fast systems
#define	CMD_MASK			(CMD_BACKUP - 1)

// the cl_parse_entities must be large enough to hold UPDATE_BACKUP frames of
// entities, so that when a delta compressed message arives from the server
// it can be un-deltad from the original 
#define MAX_PARSE_ENTITIES		2048

//
// the client_t structure is wiped completely at every
// server map change
//
typedef struct
{
	int		timeoutcount;

	bool		video_prepped;		// false if on new level or new ref dll
	bool		audio_prepped;		// false if on new level or new snd dll
	bool		force_refdef;		// vid has changed, so we can't use a paused refdef
	int		parse_entities;		// index (not anded off) into cl_parse_entities[]

	usercmd_t		cmds[CMD_BACKUP];		// each mesage will send several old cmds

	frame_t		frame;			// received from server
	frame_t		*oldframe;		// previous frame to lerping from
	int		surpressCount;		// number of messages rate supressed
	frame_t		frames[UPDATE_BACKUP];

	int		time;		// this is the time value that the client
					// is rendering at.  always <= cls.realtime
	int		render_flags;	// clearing at end of frame
	float		lerpFrac;		// interpolation value
	ref_params_t	refdef;		// shared refdef

	cinematics_t	*cin;

	player_info_t	players[MAX_CLIENTS];
	event_state_t	events;

	// predicting stuff
	vec3_t		predicted_origins[CMD_BACKUP];// for debug comparing against server

	float		predicted_step;		// for stair up smoothing
	uint		predicted_step_time;

	vec3_t		predicted_origin;		// generated by CL_PredictMovement
	vec3_t		predicted_viewofs;
	vec3_t		predicted_angles;
	vec3_t		predicted_velocity;
	vec3_t		prediction_error;

	//
	// server state information
	//
	int		playernum;
	int		servercount;			// server identification for prespawns
	int		serverframetime;			// server frametime
	char		configstrings[MAX_CONFIGSTRINGS][CS_SIZE];
	char		physinfo[MAX_INFO_STRING];		// physics info string

	entity_state_t	entity_curstates[MAX_PARSE_ENTITIES];

	// locally derived information from server state
	model_t		models[MAX_MODELS];
	string_t		edict_classnames[MAX_CLASSNAMES];
	sound_t		sound_precache[MAX_SOUNDS];
	shader_t		decal_shaders[MAX_DECALS];
} client_t;

extern client_t	cl;
extern render_exp_t	*re;
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
	scrshot_demoshot	// for demos preview
} e_scrshot;

typedef struct
{
	byte		open;		// 0 = mouth closed, 255 = mouth agape
	byte		sndcount;		// counter for running average
	int		sndavg;		// running average
} mouth_t;

// cl_private_edict_t
struct cl_priv_s
{
	link_t		area;		// linked to a division node or leaf
	bool		linked;

	int		serverframe;	// if not current, this ent isn't in the frame

	entity_state_t	current;
	entity_state_t	prev;		// will always be valid, but might just be a copy of current
	studioframe_t	frame;		// holds the studio values for right lerping

	// studiomodels attachments
	vec3_t		origin[MAXSTUDIOATTACHMENTS];
	vec3_t		angles[MAXSTUDIOATTACHMENTS];

	mouth_t		mouth;		// shared mouth info
};

typedef enum { key_console = 0, key_game, key_menu } keydest_t;

typedef struct
{
	char		name[CS_SIZE];
	int		number;	// svc_ number
	int		size;	// if size == -1, size come from first byte after svcnum
	pfnUserMsgHook	func;	// user-defined function	
} user_message_t;

typedef struct
{
	char		name[CS_SIZE];
	word		index;	// event index
	pfnEventHook	func;	// user-defined function
} user_event_t;

typedef struct
{
	// temp handle
	HSPRITE		hSprite;

	// scissor test
	int		scissor_x;
	int		scissor_y;
	int		scissor_width;
	int		scissor_height;
	bool		scissor_test;

	// centerprint stuff
	int		centerPrintY;
	int		centerPrintTime;
	int		centerPrintCharWidth;
	char		centerPrint[2048];
	int		centerPrintLines;

	HSPRITE		hHudFont;

	// crosshair members
	HSPRITE		hCrosshair;
	wrect_t		rcCrosshair;
	rgba_t		rgbaCrosshair;
} draw_stuff_t;

typedef struct
{
	void		*hInstance;		// pointer to client.dll
	HUD_FUNCTIONS	dllFuncs;			// dll exported funcs
	byte		*mempool;			// client edicts pool
	byte		*private;			// client.dll private pool
	string		maptitle;			// display map title

	union
	{
		edict_t	*edicts;			// acess by edict number
		void	*vp;			// acess by offset in bytes
	};

	// movement values from server
	movevars_t	movevars;
	movevars_t	oldmovevars;
	playermove_t	*pmove;			// pmove state

	cl_globalvars_t	*globals;
	user_message_t	*msg[MAX_USER_MESSAGES];
	user_event_t	*events[MAX_EVENTS];	// keep static to avoid fragment memory
	entity_state_t	*baselines;

	draw_stuff_t	ds;			// draw2d stuff (hud, weaponmenu etc)
	SCREENINFO	scrInfo;			// actual screen info

	client_textmessage_t titles[MAX_GAME_TITLES];
	int		numTitles;

	edict_t		viewent;			// viewmodel or playermodel in UI_PlayerSetup
	edict_t		playermodel;		// uiPlayerSetup latched vars
	
	int		numMessages;		// actual count of user messages
	int		hStringTable;		// stringtable handle

} clgame_static_t;

typedef struct
{
	connstate_t	state;
	bool		initialized;

	keydest_t		key_dest;

	byte		*mempool;			// client premamnent pool: edicts etc
	
	int		framecount;
	float		frametime;		// seconds since last frame
	int		realtime;

	int		quakePort;		// a 16 bit value that allows quake servers
						// to work around address translating routers

	// connection information
	string		servername;		// name of server from original connect
	int		connect_time;		// for connection retransmits

	netchan_t		netchan;
	int		serverProtocol;		// in case we are doing some kind of version hack

	int		challenge;		// from the server to use for connecting
	shader_t		consoleFont;		// current console font
	shader_t		clientFont;		// current client font
	shader_t		consoleBack;		// console background
	shader_t		fillShader;		// used for emulate FillRGBA to avoid wrong draw-sort
	shader_t		particle;			// used for drawing quake1 particles (SV_ParticleEffect)
	shader_t		netIcon;			// netIcon displayed bad network connection
	
	file_t		*download;		// file transfer from server
	string		downloadname;
	string		downloadtempname;
	int		downloadnumber;
	dltype_t		downloadtype;

	e_scrshot		scrshot_request;		// request for screen shot
	e_scrshot		scrshot_action;		// in-action
	string		shotname;

	// demo loop control
	int		demonum;			// -1 = don't play demos
	string		demos[MAX_DEMOS];		// when not playing

	// demo recording info must be here, so it isn't clearing on level change
	bool		demorecording;
	bool		demoplayback;
	bool		demowaiting;		// don't record until a non-delta message is received
	bool		drawplaque;		// draw plaque when level is loading
	string		demoname;			// for demo looping

	file_t		*demofile;
	int		pingtime;			// servers timebase
} client_static_t;

extern client_static_t	cls;
extern clgame_static_t	clgame;

/*
==============================================================

SCREEN CONSTS

==============================================================
*/
extern rgba_t g_color_table[8];

// basic console charwidths
#define TINYCHAR_WIDTH	(SMALLCHAR_WIDTH)
#define TINYCHAR_HEIGHT	(SMALLCHAR_HEIGHT/2)
#define SMALLCHAR_WIDTH	8
#define SMALLCHAR_HEIGHT	16
#define BIGCHAR_WIDTH	16
#define BIGCHAR_HEIGHT	24
#define GIANTCHAR_WIDTH	32
#define GIANTCHAR_HEIGHT	48

#define SCREEN_WIDTH	640
#define SCREEN_HEIGHT	480

//
// cvars
//
extern cvar_t	*cl_predict;
extern cvar_t	*cl_showfps;
extern cvar_t	*cl_shownet;
extern cvar_t	*cl_envshot_size;
extern cvar_t	*cl_font;
extern cvar_t	*cl_nodelta;
extern cvar_t	*cl_crosshair;
extern cvar_t	*cl_showmiss;
extern cvar_t	*cl_particles;
extern cvar_t	*cl_particlelod;
extern cvar_t	*cl_testentities;
extern cvar_t	*cl_testlights;
extern cvar_t	*cl_testflashlight;
extern cvar_t	*cl_levelshot_name;
extern cvar_t	*scr_centertime;
extern cvar_t	*scr_download;
extern cvar_t	*scr_loading;
extern cvar_t	*userinfo;
extern cvar_t	*con_font;

//=============================================================================

bool CL_CheckOrDownloadFile( const char *filename );

//=================================================
void CL_TeleportSplash( vec3_t org );
int CL_ParseEntityBits( sizebuf_t *msg, uint *bits );
void CL_ParseFrame( sizebuf_t *msg );

void CL_ParseTempEnts( sizebuf_t *msg );
void CL_ParseConfigString( sizebuf_t *msg );
void CL_SetLightstyle (int i);
void CL_RunParticles (void);
void CL_RunDLights (void);
void CL_RunLightStyles (void);

void CL_AddEntities (void);
void CL_AddDLights (void);
void CL_AddLightStyles (void);

//=================================================

void CL_PrepVideo( void );
void CL_PrepSound( void );

//
// cl_cmds.c
//
void CL_Quit_f (void);
void CL_ScreenShot_f( void );
void CL_EnvShot_f( void );
void CL_SkyShot_f( void );
void CL_SaveShot_f( void );
void CL_DemoShot_f( void );
void CL_LevelShot_f( void );
void CL_SetSky_f( void );
void CL_SetFont_f( void );
void SCR_Viewpos_f( void );

//
// cl_main
//
void CL_Init( void );
void CL_SendCommand( void );
void CL_Disconnect_f( void );
void CL_GetChallengePacket( void );
void CL_PingServers_f( void );
void CL_Snd_Restart_f( void );
void CL_RequestNextDownload( void );
void CL_ClearState( void );

//
// cl_input.c
//
void CL_MouseEvent( int mx, int my );
void CL_SendCmd( void );

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
// cl_progs.c
//
void CL_InitClientProgs( void );
void CL_FreeClientProgs( void );
void CL_DrawHUD( int state );
edict_t *CL_GetEdict( int entnum );
void CL_FadeAlpha( int starttime, int endtime, rgba_t color );
void CL_InitEdicts( void );
void CL_FreeEdicts( void );
void CL_InitWorld( void );

//
// cl_game.c
//
void CL_UnloadProgs( void );
bool CL_LoadProgs( const char *name );
void CL_ParseUserMessage( sizebuf_t *msg, int svc_num );
void CL_LinkUserMessage( char *pszName, const int svc_num );
void CL_SortUserMessages( void );
edict_t *CL_AllocEdict( void );
void CL_InitEdict( edict_t *pEdict );
void CL_FreeEdict( edict_t *pEdict );
string_t CL_AllocString( const char *szValue );
const char *CL_GetString( string_t iString );
void CL_CenterPrint( const char *text, int y, int charWidth );
bool CL_IsValidEdict( const edict_t *e );
const char *CL_ClassName( const edict_t *e );
void CL_SetEventIndex( const char *szEvName, int ev_index );

_inline edict_t *CL_EDICT_NUM( int n, const char *file, const int line )
{
	if((n >= 0) && (n < clgame.globals->maxEntities))
		return clgame.edicts + n;
	Host_Error( "CL_EDICT_NUM: bad number %i (called at %s:%i)\n", n, file, line );
	return NULL;	
}

//
// cl_parse.c
//
int CL_CalcNet( void );
void CL_ParseServerMessage( sizebuf_t *msg );
void CL_RunBackgroundTrack( void );
void CL_Download_f( void );

//
// cl_scrn.c
//
void SCR_RegisterShaders( void );
void SCR_AdjustSize( float *x, float *y, float *w, float *h );
void SCR_DrawPic( float x, float y, float width, float height, shader_t shader );
void SCR_FillRect( float x, float y, float width, float height, const rgba_t color );
void SCR_DrawSmallChar( int x, int y, int ch );
void SCR_DrawChar( int x, int y, float w, float h, int ch );
void SCR_DrawSmallStringExt( int x, int y, const char *string, rgba_t setColor, bool forceColor );
void SCR_DrawStringExt( int x, int y, float w, float h, const char *string, rgba_t setColor, bool forceColor );
void SCR_DrawBigString( int x, int y, const char *s, byte alpha );
void SCR_DrawBigStringColor( int x, int y, const char *s, rgba_t color );
void SCR_MakeScreenShot( void );
void SCR_MakeLevelShot( void );
void SCR_RSpeeds( void );
void SCR_DrawFPS( void );
void SCR_DrawNet( void );

//
// cl_view.c
//

void V_Init (void);
void V_Shutdown( void );
void V_ClearScene( void );
bool V_PreRender( void );
void V_PostRender( void );
void V_RenderView( void );
float V_CalcFov( float fov_x, float width, float height );

//
// cl_move.c
//
void CL_InitClientMove( void );
void CL_PredictMove( void );
void CL_CheckPredictionError( void );

//
// cl_phys.c
//
void CL_CheckVelocity( edict_t *ent );
bool CL_CheckWater( edict_t *ent );
void CL_UpdateBaseVelocity( edict_t *ent );

//
// cl_frame.c
//
void CL_GetEntitySoundSpatialization( int ent, vec3_t origin, vec3_t velocity );
void CL_AddLoopingSounds( void );

//
// cl_effects.c
//
void CL_AddParticles( void );
void CL_AddDecals( void );
void CL_ClearEffects( void );
void CL_TestLights( void );
void CL_TestEntities( void );
void CL_FindExplosionPlane( const vec3_t origin, float radius, vec3_t result );
void pfnLightForPoint( const vec3_t point, vec3_t ambientLight );
bool pfnAddParticle( cparticle_t *src, HSPRITE shader, int flags );
void pfnAddDecal( float *org, float *dir, float *rgba, float rot, float rad, HSPRITE hSpr, int flags );
void pfnAddDLight( const float *org, const float *rgb, float radius, float time, int flags, int key );
void CL_ParticleEffect( const vec3_t org, const vec3_t dir, int color, int count ); // q1 legacy
void CL_SpawnStaticDecal( vec3_t origin, int decalIndex, int entityIndex, int modelIndex );
void CL_QueueEvent( int flags, int index, float delay, event_args_t *args );
word CL_PrecacheEvent( const char *name );
void CL_ResetEvent( event_info_t *ei );
void CL_FireEvents( void );
	
//
// cl_pred.c
//
void CL_PredictMovement (void);

//
// cl_con.c
//
bool Con_Active( void );
void Con_CheckResize( void );
void Con_Init( void );
void Con_Clear_f( void );
void Con_ToggleConsole_f( void );
void Con_DrawNotify( void );
void Con_ClearNotify( void );
void Con_RunConsole( void );
void Con_DrawConsole( void );
void Con_PageUp( void );
void Con_PageDown( void );
void Con_Top( void );
void Con_Bottom( void );
void Con_Close( void );

extern bool chat_team;
extern bool anykeydown;
extern int g_console_field_width;
extern field_t historyEditLines[COMMAND_HISTORY];
extern field_t g_consoleField;
extern field_t chatField;

//
// cl_menu.c
//
typedef enum { UI_CLOSEMENU, UI_MAINMENU } uiActiveMenu_t;

void UI_UpdateMenu( int realtime );
void UI_KeyEvent( int key, bool down );
void UI_MouseMove( int x, int y );
void UI_SetActiveMenu( uiActiveMenu_t activeMenu );
void UI_AddServerToList( netadr_t adr, const char *info );
void UI_GetCursorPos( POINT *pos );
void UI_SetCursorPos( int pos_x, int pos_y );
void UI_ShowCursor( bool show );
bool UI_CreditsActive( void );
bool UI_MouseInRect( void );
bool UI_IsVisible( void );
void UI_Precache( void );
void UI_Init( void );
void UI_Shutdown( void );

//
// cl_keys.c
//
void Field_Clear( field_t *edit );
void Field_CharEvent( field_t *edit, int ch );
void Field_KeyDownEvent( field_t *edit, int key );
void Field_Draw( field_t *edit, int x, int y, int width, bool showCursor );
void Field_BigDraw( field_t *edit, int x, int y, int width, bool showCursor );

//
// cl_video.c
//
void SCR_InitCinematic( void );
bool SCR_PlayCinematic( const char *name );
bool SCR_DrawCinematic( void );
void SCR_RunCinematic( void );
void SCR_StopCinematic( void );
void CL_PlayVideo_f( void );

//
// cl_world.c
//
void CL_ClearWorld( void );
void CL_UnlinkEdict( edict_t *ent );
void CL_ClassifyEdict( edict_t *ent );
void CL_LinkEdict( edict_t *ent, bool touch_triggers );
int CL_AreaEdicts( const vec3_t mins, const vec3_t maxs, edict_t **list, int maxcount, int areatype );
trace_t CL_ClipMoveToEntity( edict_t *e, const vec3_t p0, vec3_t b0, vec3_t b1, const vec3_t p1, uint mask, int flags );
trace_t CL_Move( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int type, edict_t *e );
edict_t *CL_TestPlayerPosition( const vec3_t origin, edict_t *pass, TraceResult *tr );
trace_t CL_MoveToss( edict_t *tossent, edict_t *ignore );
int CL_BaseContents( const vec3_t p, edict_t *e );
int CL_PointContents( const vec3_t p );

#endif//CLIENT_H