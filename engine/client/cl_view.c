/*
cl_view.c - player rendering positioning
Copyright (C) 2009 Uncle Mike

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
#include "const.h"
#include "entity_types.h"
#include "gl_local.h"
#include "vgui_draw.h"

/*
===============
V_SetupRefDef

update refdef values each frame
===============
*/
void V_SetupRefDef( void )
{
	cl_entity_t	*clent;

	clent = CL_GetLocalPlayer ();

	clgame.entities->curstate.scale = clgame.movevars.waveHeight;
	VectorCopy( cl.frame.local.client.punchangle, cl.refdef.punchangle );
	clgame.viewent.curstate.modelindex = cl.frame.local.client.viewmodel;
	clgame.viewent.model = Mod_Handle( clgame.viewent.curstate.modelindex );
	clgame.viewent.curstate.entityType = ET_NORMAL;
	clgame.viewent.index = cl.playernum + 1;

	cl.refdef.movevars = &clgame.movevars;
	cl.refdef.onground = ( cl.frame.local.client.flags & FL_ONGROUND ) ? 1 : 0;
	cl.refdef.health = cl.frame.local.client.health;
	cl.refdef.playernum = cl.playernum;
	cl.refdef.max_entities = clgame.maxEntities;
	cl.refdef.maxclients = cl.maxclients;
	cl.refdef.time = cl.time;
	cl.refdef.frametime = cl.time - cl.oldtime;
	cl.refdef.demoplayback = cls.demoplayback;
	cl.refdef.smoothing = cl_smooth->integer;
	cl.refdef.waterlevel = cl.frame.local.client.waterlevel;		
	cl.refdef.onlyClientDraw = 0;	// reset clientdraw
	cl.refdef.viewsize = scr_viewsize->integer;
	cl.refdef.hardware = true;	// always true
	cl.refdef.spectator = cl.spectator;
	cl.refdef.nextView = 0;

	// setup default viewport
	cl.refdef.viewport[0] = cl.refdef.viewport[1] = 0;
	cl.refdef.viewport[2] = scr_width->integer;
	cl.refdef.viewport[3] = scr_height->integer;

	// calc FOV
	cl.refdef.fov_x = cl.data.fov; // this is a final fov value
	cl.refdef.fov_y = V_CalcFov( &cl.refdef.fov_x, cl.refdef.viewport[2], cl.refdef.viewport[3] );

	if( CL_IsPredicted( ) && !cl.refdef.demoplayback )
	{	
		VectorCopy( cl.predicted_origin, cl.refdef.simorg );
		VectorCopy( cl.predicted_velocity, cl.refdef.simvel );
		VectorCopy( cl.predicted_viewofs, cl.refdef.viewheight );
	}
	else
	{
		VectorCopy( cl.frame.local.client.origin, cl.refdef.simorg );
		VectorCopy( cl.frame.local.client.view_ofs, cl.refdef.viewheight );
		VectorCopy( cl.frame.local.client.velocity, cl.refdef.simvel );
	}
}

/*
===============
V_CalcRefDef

sets cl.refdef view values
===============
*/
void V_CalcRefDef( void )
{
	do
	{
		clgame.dllFuncs.pfnCalcRefdef( &cl.refdef );
		R_RenderFrame( &cl.refdef, true );
		cl.refdef.onlyClientDraw = false;
	} while( cl.refdef.nextView );
}

//============================================================================

/*
==================
V_RenderView

==================
*/
void V_RenderView( void )
{
	if( !cl.video_prepped || ( UI_IsVisible() && !cl.background ))
		return; // still loading

	if( cl.frame.valid && ( cl.force_refdef || !cl.refdef.paused ))
	{
		cl.force_refdef = false;

		R_ClearScene ();
		CL_AddEntities ();
		V_SetupRefDef ();
	}

	V_CalcRefDef ();
}

/*
==================
V_PreRender

==================
*/
qboolean V_PreRender( void )
{
	// too early
	if( !glw_state.initialized )
		return false;

	if( host.state == HOST_NOFOCUS )
		return false;

	if( host.state == HOST_SLEEP )
		return false;

	// if the screen is disabled (loading plaque is up)
	if( cls.disable_screen )
	{
		if(( host.realtime - cls.disable_screen ) > cl_timeout->value )
		{
			MsgDev( D_NOTE, "V_PreRender: loading plaque timed out.\n" );
			cls.disable_screen = 0.0f;
		}
		return false;
	}
	
	R_BeginFrame( !cl.refdef.paused );

	return true;
}

/*
==================
V_PostRender

==================
*/
void V_PostRender( void )
{
	R_Set2DMode( true );

	if( cls.state == ca_active )
	{
		CL_DrawHUD( CL_ACTIVE );
		VGui_Paint();
	}

	if( cls.scrshot_action == scrshot_inactive || cls.scrshot_action == scrshot_normal )
	{
		SCR_RSpeeds();
		SCR_NetSpeeds();
		SCR_DrawFPS();
		CL_DrawDemoRecording();
		R_ShowTextures();
		CL_DrawHUD( CL_CHANGELEVEL );
		Con_DrawConsole();
		UI_UpdateMenu( host.realtime );
		Con_DrawDebug(); // must be last
		S_ExtraUpdate();
	}

	SCR_MakeScreenShot();
	R_EndFrame();
}