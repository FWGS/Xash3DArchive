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
	int	i;

	tr.framecount = tr.visframecount = 1;

	GL_BuildLightmaps ();
	R_SetupSky( cl.refdef.movevars->skyName );

	// clear out efrags in case the level hasn't been reloaded
	// FIXME: is this one short?
	for( i = 0; i < cl.worldmodel->numleafs; i++ )
		cl.worldmodel->leafs[i].efrags = NULL;

	tr.skytexturenum = -1;
	r_viewleaf = r_oldviewleaf = NULL;

	// clearing texture chains
	for( i = 0; i < cl.worldmodel->numtextures; i++ )
	{
		if( !cl.worldmodel->textures[i] )
			continue;

		if( ws.version == Q1BSP_VERSION && !com.strncmp( cl.worldmodel->textures[i]->name, "sky", 3 ))
			tr.skytexturenum = i;

 		cl.worldmodel->textures[i]->texturechain = NULL;
	}
}