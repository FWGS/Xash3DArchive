//=======================================================================
//			Copyright XashXT Group 2007 �
//			cm_main.c - collision interface
//=======================================================================

#include "cm_local.h"

physic_imp_t	pi;
stdlib_api_t	com;

// cvars
cvar_t		*cm_novis;

// main DLL entry point
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
	return TRUE;
}

bool CM_InitPhysics( void )
{
	cm_novis = Cvar_Get( "cm_novis", "0", 0, "force to ignore server visibility" );

	Mem_Set( cm.nullrow, 0xFF, MAX_MAP_LEAFS / 8 );
	return true;
}

void CM_PhysFrame( float time )
{
	CM_RunLightStyles( time );
}

void CM_FreePhysics( void )
{
	CM_FreeModels();
}

physic_exp_t DLLEXPORT *CreateAPI ( stdlib_api_t *input, physic_imp_t *engfuncs )
{
	static physic_exp_t		Phys;

	com = *input;

	// Sys_LoadLibrary can create fake instance, to check
	// api version and api size, but second argument will be 0
	// and always make exception, run simply check for avoid it
	if(engfuncs) pi = *engfuncs;

	// generic functions
	Phys.api_size = sizeof( physic_exp_t );
	Phys.com_size = sizeof( stdlib_api_t );
	
	Phys.Init = CM_InitPhysics;
	Phys.Shutdown = CM_FreePhysics;

	Phys.DrawCollision = CM_DrawCollision;
	Phys.Frame = CM_PhysFrame;

	Phys.BeginRegistration = CM_BeginRegistration;
	Phys.RegisterModel = CM_RegisterModel;
	Phys.EndRegistration = CM_EndRegistration;

	Phys.BoxLeafnums = CM_BoxLeafnums;
	Phys.BoxVisible = CM_BoxVisible;
	Phys.HeadnodeVisible = CM_HeadnodeVisible;
	Phys.AmbientLevels = CM_AmbientLevels;
	Phys.PointLeafnum = CM_PointLeafnum;
	Phys.LeafPVS = CM_LeafPVS;
	Phys.LeafPHS = CM_LeafPHS;
	Phys.FatPVS = CM_FatPVS;
	Phys.FatPHS = CM_FatPHS;

	Phys.NumBmodels = CM_NumInlineModels;
	Phys.GetEntityScript = CM_EntityScript;

	Phys.Mod_GetType = CM_ModelType;
	Phys.Mod_Extradata = CM_Extradata;
	Phys.Mod_GetFrames = CM_ModelFrames;
	Phys.Mod_GetBounds = CM_ModelBounds;
	Phys.Mod_GetAttachment = CM_StudioGetAttachment;
	Phys.Mod_GetBonePos = CM_GetBonePosition;

	Phys.AddLightstyle = CM_AddLightstyle;
	Phys.LightPoint = CM_LightPoint;

	Phys.PointContents = CM_PointContents;
	Phys.HullPointContents = CM_HullPointContents;
	Phys.Trace = CM_ClipMoveToEntity;
	Phys.TraceTexture = CM_TraceTexture;
	Phys.HullForBsp = CM_HullForBsp;

	return &Phys;
}