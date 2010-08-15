//=======================================================================
//			Copyright XashXT Group 2010 ©
//		      cl_pmove.c - client-side player physic
//=======================================================================

#include "common.h"
#include "client.h"
#include "const.h"
#include "pm_defs.h"

void CL_ClearPhysEnts( void )
{
	clgame.pmove->numtouch = 0;
	clgame.pmove->numvisent = 0;
	clgame.pmove->nummoveent = 0;
	clgame.pmove->numphysent = 0;
}

/*
===============
CL_InitClientMove

===============
*/
void CL_InitClientMove( void )
{
	int	i;

	clgame.pmove->server = false;	// running at client
	clgame.pmove->movevars = &clgame.movevars;
	clgame.pmove->runfuncs = false;

	// enumerate client hulls
	for( i = 0; i < 4; i++ )
		clgame.dllFuncs.pfnGetHullBounds( i, clgame.pmove->player_mins[i], clgame.pmove->player_maxs[i] );

	// common utilities
#if 0
	clgame.pmove->PM_Info_ValueForKey = Info_ValueForKey;
	clgame.pmove->PM_TestPlayerPosition = PM_TestPlayerPosition;
	clgame.pmove->ClientPrintf = PM_ClientPrintf;
	clgame.pmove->AlertMessage = pfnAlertMessage;
	clgame.pmove->PM_GetString = CL_GetString;
	clgame.pmove->PM_PointContents = PM_PointContents;
	clgame.pmove->PM_HullPointContents = CM_HullPointContents;
	clgame.pmove->PM_HullForBsp = PM_HullForBsp;
	clgame.pmove->PM_PlayerTrace = PM_PlayerTrace;
	clgame.pmove->PM_TraceTexture = PM_TraceTexture;
	clgame.pmove->PM_GetEntityByIndex = PM_GetEntityByIndex;
	clgame.pmove->AngleVectors = AngleVectors;
	clgame.pmove->RandomLong = pfnRandomLong;
	clgame.pmove->RandomFloat = pfnRandomFloat;
	clgame.pmove->PM_GetModelType = CM_GetModelType;
	clgame.pmove->PM_GetModelBounds = Mod_GetBounds;
	clgame.pmove->PM_ModExtradata = Mod_Extradata;
	clgame.pmove->PM_TraceModel = PM_TraceModel;
	clgame.pmove->COM_LoadFile = pfnLoadFile;
	clgame.pmove->COM_ParseToken = pfnParseToken;
	clgame.pmove->COM_FreeFile = pfnFreeFile;
	clgame.pmove->memfgets = pfnMemFgets;
	clgame.pmove->PM_PlaySound = PM_PlaySound;
#endif
	// initalize pmove
//	clgame.dllFuncs.pfnPM_Init( clgame.pmove );
}