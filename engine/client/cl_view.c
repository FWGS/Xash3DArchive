//=======================================================================
//			Copyright XashXT Group 2009 ©
//		    cl_view.c - player rendering positioning
//=======================================================================

#include "common.h"
#include "client.h"
#include "const.h"
#include "entity_types.h"

/*
====================
V_ClearScene

Specifies the model that will be used as the world
====================
*/
void V_ClearScene( void )
{
	if( re ) re->ClearScene();
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

	VectorCopy( cl.frame.clientdata.punchangle, cl.refdef.punchangle );
	clgame.viewent.curstate.modelindex = cl.frame.clientdata.viewmodel;

	cl.refdef.movevars = &clgame.movevars;
	cl.refdef.onground = ( cl.frame.clientdata.flags & FL_ONGROUND ) ? 1 : 0;
	cl.refdef.health = cl.frame.clientdata.health;
	cl.refdef.lerpfrac = cl.lerpFrac;
	cl.refdef.num_entities = clgame.numEntities;
	cl.refdef.max_entities = clgame.maxEntities;
	cl.refdef.maxclients = cl.maxclients;
	cl.refdef.time = cl_time();
	cl.refdef.frametime = cl.time - cl.oldtime;
	cl.refdef.demoplayback = cls.demoplayback;
	cl.refdef.smoothing = cl_smooth->integer;
	cl.refdef.waterlevel = cl.frame.clientdata.waterlevel;		
	cl.refdef.flags = cl.render_flags;
	cl.refdef.viewsize = 120; // FIXME if you can
	cl.refdef.nextView = 0;

	// setup default viewport
	cl.refdef.viewport[0] = cl.refdef.viewport[1] = 0;
	cl.refdef.viewport[2] = scr_width->integer;
	cl.refdef.viewport[3] = scr_height->integer;

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
		VectorCopy( cl.frame.clientdata.view_ofs, cl.refdef.viewheight );
		VectorCopy( cl.frame.clientdata.velocity, cl.refdef.simvel );
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
	if( !cl.frame.valid || cl.refdef.paused ) return;

	clgame.dllFuncs.pfnAddVisibleEntity( &clgame.viewent, ET_VIEWENTITY );
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
		re->RenderFrame( &cl.refdef );
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
bool V_PreRender( void )
{
	bool		clearScene = true;
	static bool	oldState;

	// too early
	if( !re ) return false;

	if( host.state == HOST_NOFOCUS )
		return false;
	if( host.state == HOST_SLEEP )
		return false;

	if( cl.refdef.paused || cls.changelevel )
		clearScene = false;

	re->BeginFrame( clearScene );

	if( !oldState && cls.changelevel )
	{
		// fire once
		CL_DrawHUD( CL_CHANGELEVEL );
		re->EndFrame();
	}
	oldState = cls.changelevel;

	if( cls.changelevel )
		return false;
	return true;
}

/*
==================
V_PostRender

==================
*/
void V_PostRender( void )
{
	if( cls.scrshot_action == scrshot_inactive )
	{
		SCR_RSpeeds();
		SCR_NetSpeeds();
		SCR_DrawFPS();
		UI_UpdateMenu( host.realtime );
		Con_DrawConsole();
		S_ExtraUpdate();
	}

	SCR_MakeScreenShot();
	re->EndFrame();
}