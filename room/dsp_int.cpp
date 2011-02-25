//=======================================================================
//			Copyright XashXT Group 2010 ©
//		          dll_int.cpp - dll entry points 
//=======================================================================

#include "dsp.h"

#define DLLEXPORT __declspec( dllexport )

dsp_enginefuncs_t	g_engfuncs;

// main DLL entry point
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
	return TRUE;
}

static DSP_FUNCTIONS gFunctionTable = 
{
	DSP_ClearState,
	DSP_InitAll,
	DSP_FreeAll,
	DSP_Alloc,	// alloc
	DSP_SetPreset,	// set preset
	DSP_Process,	// process
	DSP_GetGain,
	DSP_Free,
};

extern "C" int DLLEXPORT InitDSP( DSP_FUNCTIONS *pFunctionTable, dsp_enginefuncs_t* pEngfuncsFromEngine, int iVersion );

//=======================================================================
//			GetApi
//=======================================================================
int InitDSP( DSP_FUNCTIONS *pFunctionTable, dsp_enginefuncs_t* pEngfuncsFromEngine, int iVersion )
{
	if( !pFunctionTable || !pEngfuncsFromEngine || iVersion != DSP_VERSION )
	{
		return FALSE;
	}

	// copy HUD_FUNCTIONS table to engine, copy engfuncs table from engine
	memcpy( pFunctionTable, &gFunctionTable, sizeof( DSP_FUNCTIONS ));
	memcpy( &g_engfuncs, pEngfuncsFromEngine, sizeof( dsp_enginefuncs_t ));

	return TRUE;
}