//=======================================================================
//			Copyright XashXT Group 2008 ©
//	      cdll_int.h - client dll interface declarations
//=======================================================================
#ifndef CDLL_INT_H
#define CDLL_INT_H

typedef int HSPRITE; // handle to a graphic
typedef int (*pfnUserMsgHook)( const char *pszName, int iSize, void *pbuf );

#include "wrect.h"
#include "cvardef.h"

#define SCRINFO_SCREENFLASH	1
#define SCRINFO_STRETCHED	2

typedef struct
{
	int		iSize;
	int		iWidth;
	int		iHeight;
	int		iFlags;
	int		iCharHeight;
	byte		charWidths[256];
} SCREENINFO;

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

// hud_player_info_t formed from player_info_t
typedef struct hud_player_info_s
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

typedef struct cl_enginefuncs_s
{
	// sprite handlers
	HSPRITE	(*pfnSPR_Load)( const char *szPicName );
	int	(*pfnSPR_Frames)( HSPRITE hPic );
	int	(*pfnSPR_Height)( HSPRITE hPic, int frame );
	int	(*pfnSPR_Width)( HSPRITE hPic, int frame );
	void	(*pfnSPR_Set)( HSPRITE hPic, int r, int g, int b );
	void	(*pfnSPR_Draw)( int frame, int x, int y, const wrect_t *prc );
	void	(*pfnSPR_DrawHoles)( int frame, int x, int y, const wrect_t *prc );
	void	(*pfnSPR_DrawAdditive)( int frame, int x, int y, const wrect_t *prc );
	void	(*pfnSPR_EnableScissor)( int x, int y, int width, int height );
	void	(*pfnSPR_DisableScissor)( void );
	client_sprite_t *(*pfnSPR_GetList)( char *psz, int *piCount );

	// screen handlers
	void	(*pfnFillRGBA)( int x, int y, int width, int height, int r, int g, int b, int a );
	int	(*pfnGetScreenInfo)( SCREENINFO *pscrinfo );
	void	(*pfnSetCrosshair)( HSPRITE hspr, wrect_t rc, int r, int g, int b );

	// cvar handlers
	cvar_t*	(*pfnRegisterVariable)( const char *szName, const char *szValue, int flags );
	float	(*pfnGetCvarFloat)( const char *szName );
	char*	(*pfnGetCvarString)( const char *szName );

	// command handlers
	void	(*pfnAddCommand)( const char *cmd_name, void (*function)(void) );
	void	(*pfnHookUserMsg)( const char *szMsgName, pfnUserMsgHook pfn );
	void	(*pfnServerCmd)( const char *szCmdString );
	void	(*pfnClientCmd)( const char *szCmdString );

	void	(*pfnGetPlayerInfo)( int player_num, hud_player_info_t *pinfo );

	// sound handlers
	void	(*pfnPlaySoundByName)( const char *szSound, float volume );
	void	(*pfnPlaySoundByIndex)( int iSound, float volume );

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

	// added for user input processing
	int	(*GetWindowCenterX)( void );
	int	(*GetWindowCenterY)( void );
	void	(*pfnGetViewAngles)( float *rgflAngles );
	void	(*pfnSetViewAngles)( float *rgflAngles );
	int	(*GetMaxClients)( void );
	void	(*pfnCvarSetValue)( const char *szName, float flValue );

	int       (*pfnCmdArgc)( void );	
	const char* (*pfnCmdArgv)( int argc );
	void	(*Con_Printf)( char *fmt, ... );
	void	(*Con_DPrintf)( char *fmt, ... );
	void	(*Con_NPrintf)( int pos, char *fmt, ... );
	void	(*Con_NXPrintf)( struct con_nprint_s *info, char *fmt, ... );

	const char* (*pfnPhysInfo_ValueForKey)( const char *key );
	const char* (*pfnServerInfo_ValueForKey)( const char *key );
	float	(*pfnGetClientMaxspeed)( void );
	int	(*CheckParm)( char *parm, char **ppnext );

	void	(*Key_Event)( int key, int down );
	void	(*GetMousePosition)( int *mx, int *my );
	int	(*IsNoClipping)( void );

	// entity handlers
	struct cl_entity_s *(*pfnGetLocalPlayer)( void );
	struct cl_entity_s *(*pfnGetViewModel)( void );
	struct cl_entity_s *(*pfnGetEntityByIndex)( int idx );	// matched with entity serialnumber

	float	(*pfnGetClientTime)( void );
	void	(*pfnFadeClientVolume)( float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds );	// was V_CalcShake
	struct model_s* (*pfnGetModelPtr)( int modelIndex );							// was V_ApplyShake

	int	(*PM_PointContents)( const float *rgflPos, int *truecontents );
	struct cl_entity_s *(*pfnWaterEntity)( const float *rgflPos );
	struct pmtrace_s *(*PM_TraceLine)( float *start, float *end, int flags, int usehull, int ignore_pe );

	struct model_s *(*CL_LoadModel)( const char *modelname, int *index );
	int	(*CL_CreateVisibleEntity)( int type, struct cl_entity_s *ent, HSPRITE customShader );

	void	(*pfnHostError)( const char *szFmt, ... );							// was GetSpritePointer
	void	(*pfnPlaySoundByNameAtLocation)( char *szSound, float volume, float *origin );
	
	word	(*pfnPrecacheEvent)( int type, const char* psz );
	void	(*pfnPlaybackEvent)( int flags, const struct cl_entity_s *pInvoker, word eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
	void	(*pfnWeaponAnim)( int iAnim, int body );
	float	(*pfnRandomFloat)( float flLow, float flHigh );	
	long	(*pfnRandomLong)( long lLow, long lHigh );
	void	(*pfnHookEvent)( const char *name, void ( *pfnEvent )( struct event_args_s *args ));
	int	(*Con_IsVisible)( void );
	const char *(*pfnGetGameDirectory)( void );
	struct cvar_s *(*pfnGetCvarPointer)( const char *szName );
	const char *(*Key_LookupBinding)( const char *pBinding );
	const char *(*pfnGetLevelName)( void );
	struct movevars_s *(*pfnGetMovementVariables)( void );							// was pfnGetScreenFade
	void	(*MakeEnvShot)( const float *vieworg, const char *name, int skyshot );				// was pfnSetScreenFade

	// vgui handlers
	void*	(*VGui_GetPanel)( void );				// UNDONE: wait for version 0.75
	void	(*VGui_ViewportPaintBackground)( int extents[4] );

	// parse txt files
	byte*	(*COM_LoadFile)( const char *filename, int *pLength );
	char*	(*COM_ParseFile)( char *data, char *token );
	void	(*COM_FreeFile)( void *buffer );

	struct triapi_s	*pTriAPI;
	struct efxapi_s	*pEfxAPI;
	struct event_api_s	*pEventAPI;	

	int	(*pfnIsSpectateOnly)( void );
	int	(*pfnIsInGame)( void );				// return false for menu, console, etc		// was LoadMapSprite 
} cl_enginefuncs_t;

typedef struct
{
	int	(*pfnVidInit)( void );
	void	(*pfnInit)( void );
	int	(*pfnRedraw)( float flTime, int state );
	int	(*pfnUpdateClientData)( struct client_data_s *pcldata, float flTime );
	int	(*pfnGetHullBounds)( int hullnumber, float *mins, float *maxs );
	void	(*pfnTxferLocalOverrides)( struct entity_state_s *state, const struct clientdata_s *client );
	void	(*pfnProcessPlayerState)( struct entity_state_s *dst, const struct entity_state_s *src );
	void	(*pfnUpdateOnRemove)( struct cl_entity_s *pEdict );
	void	(*pfnReset)( void );
	void	(*pfnStartFrame)( void );
	void	(*pfnFrame)( double time );
	void 	(*pfnShutdown)( void );
	void	(*pfnDrawTriangles)( int fTrans );
	void	(*pfnCreateEntities)( void );
	int	(*pfnAddVisibleEntity)( struct cl_entity_s *pEnt, int entityType );
	void	(*pfnStudioEvent)( const struct mstudioevent_s *event, struct cl_entity_s *entity );
	void	(*pfnStudioFxTransform)( struct cl_entity_s *pEdict, float transform[4][4] );
	void	(*pfnCalcRefdef)( struct ref_params_s *parms );
	void	(*pfnPM_Move)( struct playermove_s *ppmove, int server );
	void	(*pfnPM_Init)( struct playermove_s *ppmove );
	char	(*pfnPM_FindTextureType)( char *name );
	void	(*pfnCmdStart)( const struct cl_entity_s *player, int runfuncs );
	void	(*pfnCmdEnd)( const struct cl_entity_s *player, const struct usercmd_s *cmd, unsigned int random_seed );
	void	(*pfnCreateMove)( struct usercmd_s *cmd, float frametime, int active );
	void	(*pfnMouseEvent)( int mx, int my );
	int	(*pfnKeyEvent)( int down, int keynum, const char *pszBind );
	void	(*VGui_ConsolePrint)( const char *text );
	void	(*pfnParticleEffect)( const float *org, const float *dir, int color, int count ); // SV_ParticleEffect
	void	(*pfnTempEntityMessage)( int iSize, void *pbuf );
	void	(*pfnDirectorMessage)( int iSize, void *pbuf );
} HUD_FUNCTIONS;

typedef int (*CLIENTAPI)( HUD_FUNCTIONS *pFunctionTable, cl_enginefuncs_t* engfuncs );

#endif//CDLL_INT_H