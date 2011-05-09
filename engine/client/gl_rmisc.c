/*
gl_rmisc.c - renderer misceallaneous
Copyright (C) 2010 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"
#include "client.h"
#include "gl_local.h"
#include "mod_local.h"

void R_NewMap( void )
{
	int	i;

	R_ClearDecals(); // clear all level decals

	GL_BuildLightmaps ();
	R_SetupSky( cl.refdef.movevars->skyName );

	// clear out efrags in case the level hasn't been reloaded
	for( i = 0; i < cl.worldmodel->numleafs; i++ )
		cl.worldmodel->leafs[i].efrags = NULL;

	tr.skytexturenum = -1;
	r_viewleaf = r_oldviewleaf = NULL;

	// clearing texture chains
	for( i = 0; i < cl.worldmodel->numtextures; i++ )
	{
		if( !cl.worldmodel->textures[i] )
			continue;

		if( world.version == Q1BSP_VERSION && !Q_strncmp( cl.worldmodel->textures[i]->name, "sky", 3 ))
			tr.skytexturenum = i;

 		cl.worldmodel->textures[i]->texturechain = NULL;
	}
}