//=======================================================================
//			Copyright XashXT Group 2008 ©
//	      clgame_api.h - entity interface between engine and clgame
//=======================================================================
#ifndef CLGAME_API_H
#define CLGAME_API_H

#include "trace_def.h"
#include "pm_shared.h"

typedef int		HSPRITE;					// handle to a graphic
typedef struct tempent_s	TEMPENTITY;
typedef struct dlight_s	dlight_t;
typedef struct usercmd_s	usercmd_t;
typedef struct skyportal_s	skyportal_t;
typedef struct ref_params_s	ref_params_t;
typedef struct mstudioevent_s	mstudioevent_t;
typedef void (*ENTCALLBACK)( TEMPENTITY *ent );
typedef void (*HITCALLBACK)( TEMPENTITY *ent, TraceResult *ptr );
typedef int (*pfnUserMsgHook)( const char *pszName, int iSize, void *pbuf );	// user message handle

#include "wrect.h"

#define SCRINFO_VIRTUALSPACE	1

typedef struct
{
	int		iFlags;
	int		iRealWidth;
	int		iRealHeight;
	int		iWidth;
	int		iHeight;
	int		iCharHeight;
	byte		charWidths[256];
} SCREENINFO;

typedef struct
{
	char		*name;
	short		ping;
	byte		thisplayer;	// TRUE if this is the calling player

	byte		spectator;
	byte		packetloss;
	const char	*model;
	short		topcolor;
	short		bottomcolor;
} hud_player_info_t;

typedef struct client_textmessage_s
{
	int		effect;
	byte		r1, g1, b1, a1;	// 2 colors for effects
	byte		r2, g2, b2, a2;
	float		x;
	float		y;
	float		fadein;
	float		fadeout;
	float		holdtime;
	float		fxtime;
	const char	*pName;
	const char	*pMessage;
} client_textmessage_t;

typedef struct client_data_s
{
	// fields that cannot be modified  (ie. have no effect if changed)
	vec3_t		origin;

	// fields that can be changed by the cldll
	vec3_t		viewangles;
	int		iWeaponBits;
	float		fov;		// field of view
} client_data_t;

typedef struct client_sprite_s
{
	char		szName[64];
	char		szSprite[64];
	HSPRITE		hSprite;
	int		iRes;
	wrect_t		rc;
} client_sprite_t;

typedef struct cl_globalvars_s
{	
	float		time;		// time from server
	float		frametime;
	string_t		mapname;

	ref_params_t	*pViewParms;	// just for easy acess on client

	int		serverflags;	// shared serverflags
	int		maxClients;
	int		windowState;	// 0 - inactive (minimize, notfocus), 1 - active
	int		maxEntities;
	int		numEntities;	// actual ents count

	const char	*pStringBase;	// actual only when sys_sharedstrings is 1

	void		*pSaveData;	// (SAVERESTOREDATA *) pointer

	// Xash3D specific
	float		viewheight[PM_MAXHULLS]; // values from gameinfo.txt
	vec3_t		hullmins[PM_MAXHULLS];
	vec3_t		hullmaxs[PM_MAXHULLS];
} cl_globalvars_t;

typedef struct cl_enginefuncs_s
{
	// sprite handlers
	HSPRITE	(*pfnSPR_Load)( const char *szPicName );
	int	(*pfnSPR_Frames)( HSPRITE hPic );
	int	(*pfnSPR_Height)( HSPRITE hPic, int frame );
	int	(*pfnSPR_Width)( HSPRITE hPic, int frame );
	void	(*pfnSPR_Set)( HSPRITE hPic, int r, int g, int b, int a );
	void	(*pfnSPR_Draw)( int frame, int x, int y, int width, int height, const wrect_t *prc );
	void	(*pfnSPR_DrawHoles)( int frame, int x, int y, int width, int height, const wrect_t *prc );
	void	(*pfnSPR_DrawTrans)( int frame, int x, int y, int width, int height, const wrect_t *prc );	// kRenderTransColor
	void	(*pfnSPR_DrawAdditive)( int frame, int x, int y, int width, int height, const wrect_t *prc );
	void	(*pfnSPR_EnableScissor)( int x, int y, int width, int height );
	void	(*pfnSPR_DisableScissor)( void );
	client_sprite_t *(*pfnSPR_GetList)( char *psz, int *piCount );

	// screen handlers
	void	(*pfnFillRGBA)( int x, int y, int width, int height, int r, int g, int b, int a );
	int	(*pfnGetScreenInfo)( SCREENINFO *pscrinfo );
	void	(*pfnSetCrosshair)( HSPRITE hspr, wrect_t rc, int r, int g, int b );

	// cvar handlers
	cvar_t*	(*pfnRegisterVariable)( const char *szName, const char *szValue, int flags, const char *szDesc );
	float	(*pfnGetCvarFloat)( const char *szName );
	char*	(*pfnGetCvarString)( const char *szName );

	// command handlers
	void	(*pfnAddCommand)( const char *cmd_name, void (*function)(void), const char *cmd_desc );
	void	(*pfnHookUserMsg)( const char *szMsgName, pfnUserMsgHook pfn );
	void	(*pfnServerCmd)( const char *szCmdString );
	void	(*pfnClientCmd)( const char *szCmdString );

	void	(*pfnGetPlayerInfo)( int player_num, hud_player_info_t *pinfo );

	// sound handlers (NULL origin == play at current client origin)
	void	(*pfnPlaySoundByName)( const char *szSound, float volume, int pitch, const float *org );
	void	(*pfnPlaySoundByIndex)( int iSound, float volume, int pitch, const float *org );

	// vector helpers
	void	(*pfnAngleVectors)( const float *rgflVector, float *forward, float *right, float *up );

	// text message system
	client_textmessage_t *(*pfnTextMessageGet)( const char *pName );
	int	(*pfnDrawCharacter)( int x, int y, int number, int r, int g, int b );
	int	(*pfnDrawConsoleString)( int x, int y, char *string );
	void	(*pfnDrawSetTextColor)( float r, float g, float b );
	void	(*pfnDrawConsoleStringLen)(  const char *string, int *length, int *height );

	void	(*pfnConsolePrint)( const char *string );
	void	(*pfnCenterPrint)( const char *string );

	// engine memory manager
	void*	(*pfnMemAlloc)( size_t cb, const char *filename, const int fileline );// was GetWindowCenterX
	void	(*pfnMemFree)( void *mem, const char *filename, const int fileline );	// was GetWindowCenterY

	// added for user input processing
	void	(*pfnGetViewAngles)( float *rgflAngles );
	void	(*pfnSetViewAngles)( float *rgflAngles );
	void	(*pfnCvarSetString)( const char *szName, const char *szValue );	// was GetMaxClients (see gpGlobals->maxClients)
	void	(*pfnCvarSetValue)( const char *szName, float flValue );

	int       (*pfnCmdArgc)( void );	
	const char* (*pfnCmdArgv)( int argc );
	const char *(*pfnCmd_Args)( void );					// was Con_Printf
	float	(*pfnGetLerpFrac)( void );					// was Con_DPrintf
	void	(*pfnDelCommand)( const char *cmd_name );			// was Con_NPrintf
	void	(*pfnAlertMessage)( ALERT_TYPE, char *szFmt, ... );		// was Con_NXPrintf

	const char* (*pfnPhysInfo_ValueForKey)( const char *key );
	const char* (*pfnServerInfo_ValueForKey)( const char *key );
	float	(*pfnGetClientMaxspeed)( void );
	void*	(*pfnGetModelPtr)( struct cl_entity_s *pEdict );				// was CheckParm
	string_t	(*pfnAllocString)( const char *szValue );			// was GetMousePosition
	const char *(*pfnGetString)( string_t iString );				// was IsNoClipping

	// entity handlers
	struct cl_entity_s *(*pfnGetLocalPlayer)( void );
	struct cl_entity_s *(*pfnGetViewModel)( void );
	struct cl_entity_s *(*pfnGetEntityByIndex)( int idx );	// matched with entity serialnumber

	float	(*pfnGetClientTime)( void );		// can use gpGlobals->time instead
	void	(*pfnFadeClientVolume)( float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds );	// was V_CalcShake

	int	(*pfnPointContents)( const float *rgflPos, int *truecontents );
	struct cl_entity_s *(*pfnWaterEntity)( const float *rgflPos );
	void	(*pfnTraceLine)( const float *v1, const float *v2, int fNoMonsters, struct cl_entity_s *pentToSkip, TraceResult *ptr );
	void	(*pfnTraceHull)( const float *v1, const float *v2, int fNoMonsters, int hullNumber, struct cl_entity_s *pentToSkip, TraceResult *ptr );
	void	(*pfnTraceModel)( const float *v1, const float *v2, struct cl_entity_s *pent, TraceResult *ptr );	// was GetSpritePointer
	const char *(*pfnTraceTexture)( struct cl_entity_s *pTextureEntity, const float *v1, const float *v2 ); // was pfnPlaySoundByNameAtLocation

	word	(*pfnPrecacheEvent)( int type, const char* psz );
	void	(*pfnPlaybackEvent)( int flags, const struct cl_entity_s *pInvoker, word eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
	void	(*pfnWeaponAnim)( int iAnim, int body, float framerate );
	float	(*pfnRandomFloat)( float flLow, float flHigh );	
	long	(*pfnRandomLong)( long lLow, long lHigh );
	void	(*pfnHookEvent)( const char *name, void ( *pfnEvent )( struct event_args_s *args ));
	int	(*Con_IsVisible)( void );

	// dlls managemenet
	void*	(*pfnLoadLibrary)( const char *name );			// was pfnGetGameDirectory
	void*	(*pfnGetProcAddress)( void *hInstance, const char *name );	// was pfnGetCvarPointer
	void	(*pfnFreeLibrary)( void *hInstance );			// was Key_LookupBinding
	void	(*pfnHostError)( const char *szFmt, ... );		// was pfnGetLevelName (see gpGlobals->mapname)
	int	(*pfnFileExists)( const char *filename );		// was pfnGetScreenFade
	void	(*pfnGetGameDir)( char *szGetGameDir );			// was pfnSetScreenFade

	// vgui handlers
	void*	(*VGui_GetPanel)( void );				// UNDONE: wait for version 0.75
	void	(*VGui_ViewportPaintBackground)( int extents[4] );

	// parse txt files
	byte*	(*pfnLoadFile)( const char *filename, int *pLength );	// was COM_LoadFile, like it
	char 	*(*pfnParseToken)( const char **data_p );		// was COM_ParseFile, like it
	void	(*pfnFreeFile)( void *buffer );			// was COM_FreeFile, like it

	struct triapi_s	*pTriAPI;
	struct efxapi_s	*pEfxAPI;
	struct event_api_s	*pEventAPI;	

	int	(*pfnIsSpectateOnly)( void );
	int	(*pfnIsInGame)( void );	// was LoadMapSprite, return false for menu, console, etc

} cl_enginefuncs_t;

typedef struct
{
	int	(*pfnVidInit)( void );
	void	(*pfnInit)( void );
	int	(*pfnRedraw)( float flTime, int state );
	int	(*pfnUpdateClientData)( struct client_data_s *pcldata, float flTime );
	void	(*pfnTxferLocalOverrides)( struct entity_state_s *state, const struct clientdata_s *client );
	void	(*pfnUpdateOnRemove)( struct cl_entity_s *pEdict );
	void	(*pfnReset)( void );
	void	(*pfnStartFrame)( void );
	void	(*pfnFrame)( double time );
	void 	(*pfnShutdown)( void );
	void	(*pfnDrawTriangles)( int fTrans );
	void	(*pfnCreateEntities)( void );
	int	(*pfnAddVisibleEntity)( struct cl_entity_s *pEnt, int ed_type );
	void	(*pfnStudioEvent)( const mstudioevent_t *event, struct cl_entity_s *entity );
	void	(*pfnStudioFxTransform)( struct cl_entity_s *pEdict, float transform[4][4] );
	void	(*pfnCalcRefdef)( ref_params_t *parms );
	void	(*pfnPM_Move)( playermove_t *ppmove, int server );
	void	(*pfnPM_Init)( playermove_t *ppmove );
	char	(*pfnPM_FindTextureType)( const char *name );
	void	(*pfnCmdStart)( const struct cl_entity_s *player, int runfuncs );
	void	(*pfnCmdEnd)( const struct cl_entity_s *player, const usercmd_t *cmd, unsigned int random_seed );
	void	(*pfnCreateMove)( usercmd_t *cmd, int msec, int active );
	void	(*pfnMouseEvent)( int mx, int my );
	int	(*pfnKeyEvent)( int down, int keynum, const char *pszBind );
	void	(*VGui_ConsolePrint)( const char *text );
	void	(*pfnParticleEffect)( const float *org, const float *dir, int color, int count ); // SV_ParticleEffect
} HUD_FUNCTIONS;

typedef int (*CLIENTAPI)( HUD_FUNCTIONS *pFunctionTable, cl_enginefuncs_t* engfuncs, cl_globalvars_t *pGlobals );

#endif//CLGAME_API_H