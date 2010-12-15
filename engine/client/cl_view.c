//=======================================================================
//			Copyright XashXT Group 2009 ©
//		    cl_view.c - player rendering positioning
//=======================================================================

#include "common.h"
#include "client.h"
#include "const.h"
#include "entity_types.h"
#include "gl_local.h"

/*
====================
V_ClearScene

Specifies the model that will be used as the world
====================
*/
void V_ClearScene( void )
{
	R_ClearScene();
}

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

	VectorCopy( cl.frame.local.client.punchangle, cl.refdef.punchangle );
	clgame.viewent.curstate.modelindex = cl.frame.local.client.viewmodel;

	cl.refdef.movevars = &clgame.movevars;
	cl.refdef.onground = ( cl.frame.local.client.flags & FL_ONGROUND ) ? 1 : 0;
	cl.refdef.health = cl.frame.local.client.health;
	cl.refdef.playernum = cl.playernum;
	cl.refdef.max_entities = clgame.maxEntities;
	cl.refdef.maxclients = cl.maxclients;
	cl.refdef.time = cl_time();
	cl.refdef.frametime = cl.time - cl.oldtime;
	cl.refdef.demoplayback = cls.demoplayback;
	cl.refdef.smoothing = cl_smooth->integer;
	cl.refdef.waterlevel = cl.frame.local.client.waterlevel;		
	cl.refdef.onlyClientDraw = 0;	// reset clientdraw
	cl.refdef.viewsize = 120;	// FIXME if you can
	cl.refdef.hardware = true;	// always true
	cl.refdef.nextView = 0;

	// setup default viewport
	cl.refdef.viewport[0] = cl.refdef.viewport[1] = 0;
	cl.refdef.viewport[2] = scr_width->integer;
	cl.refdef.viewport[3] = scr_height->integer;

	// calc FOV
	cl.refdef.fov_x = cl.data.fov; // this is a final fov value
	cl.refdef.fov_y = V_CalcFov( &cl.refdef.fov_x, cl.refdef.viewport[2], cl.refdef.viewport[3] );

	// calculate the origin
	if( CL_IsPredicted( ) && !cl.refdef.demoplayback )
	{	
		// use predicted values
		float	backlerp = 1.0f - cl.lerpFrac;
		int	i;

		for( i = 0; i < 3; i++ )
		{
			cl.refdef.simorg[i] = cl.predicted_origin[i] - backlerp * cl.prediction_error[i];
			cl.refdef.viewheight[i] = cl.predicted_viewofs[i] - backlerp * cl.prediction_error[i];
                    }
		VectorCopy( cl.predicted_velocity, cl.refdef.simvel );
	}
	else
	{
		VectorCopy( clent->origin, cl.refdef.simorg );
		VectorCopy( cl.frame.local.client.view_ofs, cl.refdef.viewheight );
		VectorCopy( cl.frame.local.client.velocity, cl.refdef.simvel );
	}
}

/*
===============
V_ApplyRefDef

apply pre-calculated values
===============
*/
void V_AddViewModel( void )
{
	if( cl.refdef.nextView ) return; // add viewmodel only at firstperson pass
	if( !cl.frame.valid || cl.refdef.paused || cl.thirdperson ) return;

	CL_AddVisibleEntity( &clgame.viewent, ET_NORMAL );
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
		V_AddViewModel();
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
	if( !cl.video_prepped ) return; // still loading

	if( cl.frame.valid && ( cl.force_refdef || !cl.refdef.paused ))
	{
		cl.force_refdef = false;

		V_ClearScene ();
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
		if(( host.realtime - cls.disable_screen ) > 60.0f )
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
	}

	if( cls.scrshot_action == scrshot_inactive )
	{
		SCR_RSpeeds();
		SCR_NetSpeeds();
		SCR_DrawFPS();
		CL_DrawDemoRecording();
		R_ShowTextures();
		CL_DrawHUD( CL_CHANGELEVEL );
		UI_UpdateMenu( host.realtime );
		Con_DrawConsole();
		S_ExtraUpdate();
	}

	SCR_MakeScreenShot();
	R_EndFrame();
}