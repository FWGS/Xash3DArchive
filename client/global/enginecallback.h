//=======================================================================
//			Copyright XashXT Group 2008 �
//		     enginecallback.h - actual engine callbacks
//=======================================================================

#ifndef ENGINECALLBACKS_H
#define ENGINECALLBACKS_H

// built-in memory manager
#define MALLOC( x )		(*gEngfuncs.pfnMemAlloc)( x, __FILE__, __LINE__ )
#define CALLOC( x, y )	(*gEngfuncs.pfnMemAlloc)((x) * (y), __FILE__, __LINE__ )
#define FREE( x )		(*gEngfuncs.pfnMemFree)( x, __FILE__, __LINE__ )

// screen handlers
#define SPR_GetList		(*gEngfuncs.pfnSPR_GetList)
#define SPR_Frames		(*gEngfuncs.pfnSPR_Frames)
#define SPR_Width		(*gEngfuncs.pfnSPR_Width)
#define SPR_Height		(*gEngfuncs.pfnSPR_Height)
#define SPR_EnableScissor	(*gEngfuncs.pfnSPR_EnableScissor)
#define SPR_DisableScissor	(*gEngfuncs.pfnSPR_DisableScissor)
#define FillRGBA		(*gEngfuncs.pfnFillRGBA)
#define GetScreenInfo	(*gEngfuncs.pfnGetScreenInfo)

#define SPR_Load		(*gEngfuncs.pfnSPR_Load)
#define TEX_Load( x )	(*gEngfuncs.pTriAPI->LoadShader)( x, false )
#define TEX_LoadNoMip( x )	(*gEngfuncs.pTriAPI->LoadShader)( x, true )
#define SetCrosshair	(*gEngfuncs.pfnSetCrosshair)

#define SendWeaponAnim	(*gEngfuncs.pEventAPI->EV_WeaponAnimation)
#define GetModelType	(*gEngfuncs.pfnGetModelType)
#define GetModelFrames	(*gEngfuncs.pfnGetModFrames)
#define GetModelBounds	(*gEngfuncs.pfnGetModBounds)
#define CVAR_REGISTER	(*gEngfuncs.pfnRegisterVariable)
#define CVAR_SET_FLOAT	(*gEngfuncs.pfnCvarSetValue)
#define CVAR_GET_FLOAT	(*gEngfuncs.pfnGetCvarFloat)
#define CVAR_GET_STRING	(*gEngfuncs.pfnGetCvarString)
#define SERVER_COMMAND	(*gEngfuncs.pfnServerCmd)
#define CLIENT_COMMAND	(*gEngfuncs.pfnClientCmd)
#define GetPlayerInfo	(*gEngfuncs.pfnGetPlayerInfo)
#define TextMessageGet	(*gEngfuncs.pfnTextMessageGet)
#define Cmd_AddCommand	(*gEngfuncs.pfnAddCommand)
#define Cmd_RemoveCommand	(*gEngfuncs.pfnDelCommand)
#define CMD_ARGC		(*gEngfuncs.pfnCmdArgc)
#define CMD_ARGV		(*gEngfuncs.pfnCmdArgv)
#define Con_Printf		(*gEngfuncs.Con_Printf)
#define Con_DPrintf		(*gEngfuncs.Con_DPrintf)
#define IN_GAME		(*gEngfuncs.pfnIsInGame)

inline void SPR_Set( HSPRITE hPic, int r, int g, int b )
{
	gEngfuncs.pfnSPR_Set( hPic, r, g, b, 255 );
}

inline void SPR_Set( HSPRITE hPic, int r, int g, int b, int a )
{
	gEngfuncs.pfnSPR_Set( hPic, r, g, b, a );
}

inline void SPR_Draw( int frame, int x, int y, const wrect_t *prc )
{
	gEngfuncs.pfnSPR_Draw( frame, x, y, -1, -1, prc );
}

inline void SPR_Draw( int frame, int x, int y, int width, int height )
{
	gEngfuncs.pfnSPR_Draw( frame, x, y, width, height, NULL );
}

inline void SPR_DrawTransColor( int frame, int x, int y, const wrect_t *prc )
{
	gEngfuncs.pfnSPR_DrawTrans( frame, x, y, -1, -1, prc );
}

inline void SPR_DrawTransColor( int frame, int x, int y, int width, int height )
{
	gEngfuncs.pfnSPR_DrawTrans( frame, x, y, width, height, NULL );
}

inline void SPR_DrawHoles( int frame, int x, int y, const wrect_t *prc )
{
	gEngfuncs.pfnSPR_DrawHoles( frame, x, y, -1, -1, prc );
}

inline void SPR_DrawHoles( int frame, int x, int y, int width, int height )
{
	gEngfuncs.pfnSPR_DrawHoles( frame, x, y, width, height, NULL );
}

inline void SPR_DrawAdditive( int frame, int x, int y, const wrect_t *prc )
{
	gEngfuncs.pfnSPR_DrawAdditive( frame, x, y, -1, -1, prc );
}

inline void SPR_DrawAdditive( int frame, int x, int y, int width, int height )
{
	gEngfuncs.pfnSPR_DrawAdditive( frame, x, y, width, height, NULL );
}

inline void CL_PlaySound( const char *szSound, float flVolume, float pitch = PITCH_NORM )
{
	gEngfuncs.pfnPlaySoundByName( szSound, flVolume, pitch, NULL );
}

inline void CL_PlaySound( int iSound, float flVolume, float pitch = PITCH_NORM )
{
	gEngfuncs.pfnPlaySoundByIndex( iSound, flVolume, pitch, NULL );
}

inline void CL_PlaySound( const char *szSound, float flVolume, Vector &pos, float pitch = PITCH_NORM )
{
	gEngfuncs.pfnPlaySoundByName( szSound, flVolume, pitch, pos );
}

inline void CL_PlaySound( int iSound, float flVolume, Vector &pos, float pitch = PITCH_NORM )
{
	gEngfuncs.pfnPlaySoundByIndex( iSound, flVolume, pitch, pos );
}

#define TextMessageDrawChar	(*gEngfuncs.pfnDrawCharacter)
#define TextMessageSetColor	(*gEngfuncs.pfnDrawSetTextColor)
#define DrawConsoleString	(*gEngfuncs.pfnDrawConsoleString)
#define GetConsoleStringSize	(*gEngfuncs.pfnDrawConsoleStringLen)
#define AngleVectors	(*gEngfuncs.pfnAngleVectors)
#define CenterPrint		(*gEngfuncs.pfnCenterPrint)
#define ConsolePrint	(*gEngfuncs.pfnConsolePrint)
#define GetViewAngles	(*gEngfuncs.pfnGetViewAngles)
#define SetViewAngles	(*gEngfuncs.pfnSetViewAngles)
#define GetLocalPlayer	(*gEngfuncs.pfnGetLocalPlayer)
#define GetClientMaxspeed	(*gEngfuncs.pfnGetClientMaxspeed)
#define IsSpectateOnly	(*gEngfuncs.pfnIsSpectateOnly)
#define GetClientTime	(*gEngfuncs.pfnGetClientTime)
#define GetLerpFrac		(*gEngfuncs.pfnGetLerpFrac)
#define GetViewModel	(*gEngfuncs.pfnGetViewModel)
#define GetModelPtr		(*gEngfuncs.pfnGetModelPtr)
#define CL_GetPaletteColor	(*gEngfuncs.pEfxAPI->R_GetPaletteColor)
#define CL_AddEntity	(*gEngfuncs.pEfxAPI->R_AddEntity)
#define POINT_CONTENTS( x )	(*gEngfuncs.PM_PointContents)( x, NULL )
#define TRACE_LINE		(*gEngfuncs.pfnTraceLine)
#define TRACE_HULL		(*gEngfuncs.pfnTraceHull)
#define RANDOM_LONG		(*gEngfuncs.pfnRandomLong)
#define RANDOM_FLOAT	(*gEngfuncs.pfnRandomFloat)
#define LOAD_FILE		(*gEngfuncs.pfnLoadFile)
#define FILE_EXISTS		(*gEngfuncs.pfnFileExists)
#define FREE_FILE		(*gEngfuncs.pfnFreeFile)
#define GET_GAME_DIR	(*gEngfuncs.pfnGetGameDir)
#define LOAD_LIBRARY	(*gEngfuncs.pfnLoadLibrary)
#define GET_PROC_ADDRESS	(*gEngfuncs.pfnGetProcAddress)
#define FREE_LIBRARY	(*gEngfuncs.pfnFreeLibrary)
#define HOST_ERROR		(*gEngfuncs.pfnHostError)
#define COM_ParseToken	(*gEngfuncs.pfnParseToken)
#define MAKE_ENVSHOT( x, y )	(*gEngfuncs.R_EnvShot)( x, y, 0 )
#define MAKE_SKYSHOT( x, y )	(*gEngfuncs.R_EnvShot)( x, y, 1 )

#endif//ENGINECALLBACKS_H