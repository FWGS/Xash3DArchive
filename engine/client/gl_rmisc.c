//=======================================================================
//			Copyright XashXT Group 2010 ©
//		      gl_rmisc.c - renderer misceallaneous
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"
#include "cm_local.h"

void R_NewMap( void )
{
	GL_BuildLightmaps ();

	R_SetupSky( cl.refdef.movevars->skyName );
}