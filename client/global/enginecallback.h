//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     enginecallback.h - actual engine callbacks
//=======================================================================

#ifndef ENGINECALLBACKS_H
#define ENGINECALLBACKS_H

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
#define CVAR_REGISTER	(*gEngfuncs.pfnRegisterVariable)
#define CVAR_SET_FLOAT	(*gEngfuncs.Cvar_SetValue)
#define CVAR_GET_FLOAT	(*gEngfuncs.pfnGetCvarFloat)
#define CVAR_GET_STRING	(*gEngfuncs.pfnGetCvarString)
#define SERVER_COMMAND	(*gEngfuncs.pfnServerCmd)
#define CLIENT_COMMAND	(*gEngfuncs.pfnClientCmd)
#define GetPlayerInfo	(*gEngfuncs.pfnGetPlayerInfo)
#define TextMessageGet	(*gEngfuncs.pfnTextMessageGet)
#define Cmd_AddCommand	(*gEngfuncs.pfnAddCommand)
#define CMD_ARGC		(*gEngfuncs.Cmd_Argc)
#define CMD_ARGV		(*gEngfuncs.Cmd_Argv)

inline void SPR_Set( HSPRITE hPic, int r, int g, int b )
{
	gEngfuncs.pfnSPR_Set( hPic, r, g, b );
}

inline void SPR_Draw( int frame, int x, int y, const wrect_t *prc )
{
	gEngfuncs.pfnSPR_Draw( frame, x, y, prc );
}

inline void SPR_DrawHoles( int frame, int x, int y, const wrect_t *prc )
{
	gEngfuncs.pfnSPR_DrawHoles( frame, x, y, prc );
}

inline void SPR_DrawAdditive( int frame, int x, int y, const wrect_t *prc )
{
	gEngfuncs.pfnSPR_DrawAdditive( frame, x, y, prc );
}

// sound functions
inline void PlaySound( char *szSound, float vol ) { gEngfuncs.pfnPlaySoundByName( szSound, vol ); }
inline void PlaySound( int iSound, float vol ) { gEngfuncs.pfnPlaySoundByIndex( iSound, vol ); }

#define TextMessageDrawChar	(*gEngfuncs.pfnDrawCharacter)
#define TextMessageSetColor	(*gEngfuncs.pfnDrawSetTextColor)
#define DrawConsoleString	(*gEngfuncs.pfnDrawConsoleString)
#define GetConsoleStringSize	(*gEngfuncs.pfnDrawConsoleStringLen)
#define AngleVectors	(*gEngfuncs.pfnAngleVectors)
#define CenterPrint		(*gEngfuncs.pfnCenterPrint)
#define ConsolePrint	(*gEngfuncs.pfnConsolePrint)
#define GetViewAngles	(*gEngfuncs.GetViewAngles)
#define SetViewAngles	(*gEngfuncs.SetViewAngles)
#define GetLocalPlayer	(*gEngfuncs.GetLocalPlayer)
#define GetClientMaxspeed	(*gEngfuncs.GetClientMaxspeed)
#define IsSpectateOnly	(*gEngfuncs.IsSpectateOnly)
#define GetClientTime	(*gEngfuncs.GetClientTime)
#define GetViewModel	(*gEngfuncs.GetViewModel)
#define GetModelPtr		(*gEngfuncs.pfnGetModelPtr)
#define CL_GetPaletteColor	(*gEngfuncs.pEfxAPI->R_GetPaletteColor)
#define POINT_CONTENTS( x )	(*gEngfuncs.PM_PointContents)( x, NULL )
#define TRACE_LINE		(*gEngfuncs.pfnTraceLine)
#define TRACE_HULL		(*gEngfuncs.pfnTraceHull)
#define RANDOM_LONG		(*gEngfuncs.pfnRandomLong)
#define RANDOM_FLOAT	(*gEngfuncs.pfnRandomFloat)
#define HOST_ERROR		(*gEngfuncs.pfnHostError)
#define MAKE_ENVSHOT( x, y )	(*gEngfuncs.R_EnvShot)( x, y, 0 )
#define MAKE_SKYSHOT( x, y )	(*gEngfuncs.R_EnvShot)( x, y, 1 )

#endif//ENGINECALLBACKS_H