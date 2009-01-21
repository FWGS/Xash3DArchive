//=======================================================================
//			Copyright XashXT Group 2008 �
//		     enginecallback.h - actual engine callbacks
//=======================================================================

#ifndef ENGINECALLBACKS_H
#define ENGINECALLBACKS_H

// built-in memory manager
#define MALLOC( x )		(*g_engfuncs.pfnMemAlloc)( x, __FILE__, __LINE__ )
#define CALLOC( x, y )	(*g_engfuncs.pfnMemAlloc)((x) * (y), __FILE__, __LINE__ )
#define MEMCPY( x, y, z )	(*g_engfuncs.pfnMemCopy)( x, y, z, __FILE__, __LINE__ )
#define FREE( x )		(*g_engfuncs.pfnMemFree)( x, __FILE__, __LINE__ )

// screen handlers
#define SPR_Load( x )	(*g_engfuncs.pfnLoadShader)( x, true )
#define TEX_Load( x )	(*g_engfuncs.pfnLoadShader)( x, false )
#define DrawImageExt	(*g_engfuncs.pfnDrawImageExt)
#define SetColor		(*g_engfuncs.pfnSetColor)
#define SetParms		(*g_engfuncs.pfnSetParms)
#define CVAR_REGISTER	(*g_engfuncs.pfnRegisterVariable)
#define CVAR_SET_FLOAT	(*g_engfuncs.pfnCvarSetValue)
#define CVAR_GET_FLOAT	(*g_engfuncs.pfnGetCvarFloat)
#define CVAR_GET_STRING	(*g_engfuncs.pfnGetCvarString)
#define SET_KEYDEST		(*g_engfuncs.pfnSetKeyDest)
#define SERVER_COMMAND	(*g_engfuncs.pfnServerCmd)
#define CLIENT_COMMAND	(*g_engfuncs.pfnClientCmd)
#define GET_PLAYER_INFO	(*g_engfuncs.pfnGetPlayerInfo)
#define GET_GAME_MESSAGE	(*g_engfuncs.pfnTextMessageGet)
#define CMD_ARGC		(*g_engfuncs.pfnCmdArgc)
#define CMD_ARGV		(*g_engfuncs.pfnCmdArgv)
#define ALERT		(*g_engfuncs.pfnAlertMessage)

inline void CL_PlaySound( const char *szSound, float flVolume, float pitch = PITCH_NORM )
{
	g_engfuncs.pfnPlaySoundByName( szSound, flVolume, pitch, NULL );
}

inline void CL_PlaySound( int iSound, float flVolume, float pitch = PITCH_NORM )
{
	g_engfuncs.pfnPlaySoundByIndex( iSound, flVolume, pitch, NULL );
}

inline void CL_PlaySound( const char *szSound, float flVolume, Vector &pos, float pitch = PITCH_NORM )
{
	g_engfuncs.pfnPlaySoundByName( szSound, flVolume, pitch, pos );
}

inline void CL_PlaySound( int iSound, float flVolume, Vector &pos, float pitch = PITCH_NORM )
{
	g_engfuncs.pfnPlaySoundByIndex( iSound, flVolume, pitch, pos );
}

#define AngleVectors	(*g_engfuncs.pfnAngleVectors)
#define DrawCenterPrint	(*g_engfuncs.pfnDrawCenterPrint)
#define CenterPrint		(*g_engfuncs.pfnCenterPrint)
#define DrawString		(*g_engfuncs.pfnDrawString)
#define GetParms		(*g_engfuncs.pfnGetParms)
#define GetViewAngles	(*g_engfuncs.pfnGetViewAngles)
#define GetEntityByIndex	(*g_engfuncs.pfnGetEntityByIndex)
#define GetLocalPlayer	(*g_engfuncs.pfnGetLocalPlayer)
#define IsSpectateOnly	(*g_engfuncs.pfnIsSpectateOnly)
#define GetClientTime	(*g_engfuncs.pfnGetClientTime)
#define GetMaxClients	(*g_engfuncs.pfnGetMaxClients)
#define GetViewModel	(*g_engfuncs.pfnGetViewModel)
#define MAKE_LEVELSHOT	(*g_engfuncs.pfnMakeLevelShot)
#define POINT_CONTENTS	(*g_engfuncs.pfnPointContents)
#define TRACE_LINE		(*g_engfuncs.pfnTraceLine)
#define RANDOM_LONG		(*g_engfuncs.pfnRandomLong)
#define RANDOM_FLOAT	(*g_engfuncs.pfnRandomFloat)
#define LOAD_FILE		(*g_engfuncs.pfnLoadFile)
#define FILE_EXISTS		(*g_engfuncs.pfnFileExists)
#define FREE_FILE		FREE
#define GET_GAME_DIR	(*g_engfuncs.pfnGetGameDir)
#define LOAD_LIBRARY	(*g_engfuncs.pfnLoadLibrary)
#define GET_PROC_ADDRESS	(*g_engfuncs.pfnGetProcAddress)
#define FREE_LIBRARY	(*g_engfuncs.pfnFreeLibrary)
#define HOST_ERROR		(*g_engfuncs.pfnHostError)

#endif//ENGINECALLBACKS_H