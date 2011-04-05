//=======================================================================
//			Copyright XashXT Group 2008 ©
//		      cl_pred.c - client movement prediction
//=======================================================================

#include "common.h"
#include "client.h"
#include "const.h"
#include "pm_local.h"
#include "net_encode.h"

#define MAX_FORWARD			6

qboolean CL_IsPredicted( void )
{
	cl_entity_t	*player = CL_GetLocalPlayer();

	if( !player )
		return false;

	if( !cl.frame.valid )
		return false;

	if( cls.netchan.outgoing_sequence - cls.netchan.incoming_sequence >= CL_UPDATE_BACKUP - 1 )
		return false;

	if( !cl_predict->integer )
		return false;
	return true;
}

/*
===========
CL_PostRunCmd

Done after running a player command.
===========
*/
void CL_PostRunCmd( usercmd_t *ucmd, int random_seed )
{
	local_state_t	*from, *to;

	// TODO: write real predicting code

	from = &cl.predict[cl.predictcount & CL_UPDATE_MASK];
	to = &cl.predict[(cl.predictcount + 1) & CL_UPDATE_MASK];

	*to = *from;

	clgame.dllFuncs.pfnPostRunCmd( from, to, ucmd, clgame.pmove->runfuncs, cl.time, random_seed );
	cl.predictcount++;
}

/*
===================
CL_CheckPredictionError
===================
*/
void CL_CheckPredictionError( void )
{
	int	frame;
	vec3_t	delta;
	float	flen;

	if( !CL_IsPredicted( )) return;

	// calculate the last usercmd_t we sent that the server has processed
	frame = cls.netchan.incoming_acknowledged;
	frame &= CL_UPDATE_MASK;

	// compare what the server returned with what we had predicted it to be
	VectorSubtract( cl.frame.local.client.origin, cl.predicted_origins[frame], delta );

	// save the prediction error for interpolation
	flen = fabs( delta[0] ) + fabs( delta[1] ) + fabs( delta[2] );

	if( flen > 80 )
	{	
		// a teleport or something
		VectorClear( cl.prediction_error );
	}
	else
	{
		if( cl_showmiss->integer && flen > 0.1f ) Msg( "prediction miss: %g\n", flen );
		VectorCopy( cl.frame.local.client.origin, cl.predicted_origins[frame] );

		// save for error itnerpolation
		VectorCopy( delta, cl.prediction_error );
	}
}

/*
===============
CL_SetIdealPitch
===============
*/
void CL_SetIdealPitch( cl_entity_t *ent )
{
	float	angleval, sinval, cosval;
	pmtrace_t	tr;
	vec3_t	top, bottom;
	float	z[MAX_FORWARD];
	int	i, j;
	int	step, dir, steps;

	if( !( cl.frame.local.client.flags & FL_ONGROUND ))
		return;
		
	angleval = ent->angles[YAW] * M_PI * 2 / 360;
	SinCos( angleval, &sinval, &cosval );

	for( i = 0; i < MAX_FORWARD; i++ )
	{
		top[0] = ent->origin[0] + cosval * (i + 3) * 12;
		top[1] = ent->origin[1] + sinval * (i + 3) * 12;
		top[2] = ent->origin[2] + cl.frame.local.client.view_ofs[2];
		
		bottom[0] = top[0];
		bottom[1] = top[1];
		bottom[2] = top[2] - 160;

		// skip any monsters (only world and brush models)
		tr = PM_PlayerTrace( clgame.pmove, top, bottom, PM_STUDIO_IGNORE, 2, -1, NULL );
		if( tr.allsolid ) return; // looking at a wall, leave ideal the way is was

		if( tr.fraction == 1.0f )
			return;	// near a dropoff
		
		z[i] = top[2] + tr.fraction * (bottom[2] - top[2]);
	}
	
	dir = 0;
	steps = 0;
	for( j = 1; j < i; j++ )
	{
		step = z[j] - z[j-1];
		if( step > -ON_EPSILON && step < ON_EPSILON )
			continue;

		if( dir && ( step-dir > ON_EPSILON || step-dir < -ON_EPSILON ))
			return; // mixed changes

		steps++;	
		dir = step;
	}
	
	if( !dir )
	{
		cl.refdef.idealpitch = 0;
		return;
	}
	
	if( steps < 2 ) return;
	cl.refdef.idealpitch = -dir * cl_idealpitchscale->value;
}

/*
=================
CL_PredictMovement

Sets cl.predicted_origin and cl.predicted_angles
=================
*/
void CL_PredictMovement( void )
{
	int		frame = 1;
	int		ack, outgoing_command;
	int		current_command;
	int		current_command_mod;
	cl_entity_t	*player, *viewent;
	clientdata_t	*cd;

	if( cls.state != ca_active ) return;
	if( cl.refdef.paused || cls.key_dest == key_menu ) return;

	player = CL_GetLocalPlayer ();
	viewent = CL_GetEntityByIndex( cl.refdef.viewentity );
	cd = &cl.frame.local.client;

	CL_SetIdealPitch( player );

	if( cls.demoplayback && viewent )
	{
		// restore viewangles from angles
		cl.refdef.cl_viewangles[PITCH] = -viewent->angles[PITCH] * 3;
		cl.refdef.cl_viewangles[YAW] = viewent->angles[YAW];
		cl.refdef.cl_viewangles[ROLL] = 0; // roll will be computed in view.cpp
	}

	// unpredicted pure angled values converted into axis
	AngleVectors( cl.refdef.cl_viewangles, cl.refdef.forward, cl.refdef.right, cl.refdef.up );

	if( !CL_IsPredicted( ))
	{	
		// run commands even if client predicting is disabled - client expected it
		clgame.pmove->runfuncs = true;

		VectorCopy( cl.refdef.cl_viewangles, cl.predicted_angles );
		VectorCopy( cd->view_ofs, cl.predicted_viewofs );

		CL_PostRunCmd( cl.refdef.cmd, cls.lastoutgoingcommand );
		return;
	}

	ack = cls.netchan.incoming_acknowledged;
	outgoing_command = cls.netchan.outgoing_sequence;

	ASSERT( cl.refdef.cmd != NULL );

	// setup initial pmove state
	CL_SetupPMove( clgame.pmove, cd, &player->curstate, cl.refdef.cmd );
	clgame.pmove->runfuncs = false;

	while( 1 )
	{
		// we've run too far forward
		if( frame >= CL_UPDATE_BACKUP - 1 )
			break;

		// Incoming_acknowledged is the last usercmd the server acknowledged having acted upon
		current_command = ack + frame;
		current_command_mod = current_command & CL_UPDATE_MASK;

		// we've caught up to the current command.
		if( current_command > outgoing_command )
			break;

		clgame.pmove->cmd = cl.cmds[frame];

		// motor!
		clgame.dllFuncs.pfnPlayerMove( clgame.pmove, false );	// run frames

		// save for debug checking
		VectorCopy( clgame.pmove->origin, cl.predicted_origins[frame-1] );
		clgame.pmove->runfuncs = true;

		frame++;
	}

	CL_PostRunCmd( cl.refdef.cmd, frame );
		
	// copy results out for rendering
	VectorCopy( clgame.pmove->view_ofs, cl.predicted_viewofs );
	VectorCopy( clgame.pmove->origin, cl.predicted_origin );
	VectorCopy( clgame.pmove->angles, cl.predicted_angles );
	VectorCopy( clgame.pmove->velocity, cl.predicted_velocity );
}