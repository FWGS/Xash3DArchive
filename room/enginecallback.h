//=======================================================================
//			Copyright XashXT Group 2011 ©
//		     enginecallback.h - actual engine callbacks
//=======================================================================

#ifndef ENGINECALLBACKS_H
#define ENGINECALLBACKS_H

// screen handlers
#define RANDOM_LONG		(*g_engfuncs.pfnRandomLong)
#define RANDOM_FLOAT	(*g_engfuncs.pfnRandomFloat)
#define CVAR_GET_POINTER	(*g_engfuncs.pfnGetCvarPointer)

#define CVAR_REGISTER	(*g_engfuncs.pfnRegisterVariable)
#define CVAR_SET_FLOAT	(*g_engfuncs.pfnCvarSetValue)
#define CVAR_GET_FLOAT	(*g_engfuncs.pfnGetCvarFloat)
#define CVAR_GET_STRING	(*g_engfuncs.pfnGetCvarString)
#define CVAR_SET_STRING	(*g_engfuncs.pfnCvarSetString)

#define Con_Printf		(*g_engfuncs.Con_Printf)
#define Con_DPrintf		(*g_engfuncs.Con_DPrintf)

#endif//ENGINECALLBACKS_H