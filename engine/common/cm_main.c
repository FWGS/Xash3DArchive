//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_main.c - collision interface
//=======================================================================

#include "cm_local.h"

// cvars
cvar_t		*cm_novis;

bool CM_InitPhysics( void )
{
	cm_novis = Cvar_Get( "cm_novis", "0", 0, "force to ignore server visibility" );

	Mem_Set( cm.nullrow, 0xFF, MAX_MAP_LEAFS / 8 );
	return true;
}

void CM_Frame( float time )
{
	CM_RunLightStyles( time );
}

void CM_FreePhysics( void )
{
	CM_FreeModels();
}