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

void CL_PreRunCmd( cl_entity_t *clent, usercmd_t *ucmd )
{
	clgame.pmove->runfuncs = (( clent->index - 1 ) == cl.playernum ) ? true : false;
}

/*
===========
CL_PostRunCmd

Done after running a player command.
===========
*/
void CL_PostRunCmd( cl_entity_t *clent, usercmd_t *ucmd )
{
	local_state_t	*from, *to;

	if( !clent ) return;

	from = &cl.frames[cl.delta_sequence & CL_UPDATE_MASK].local;
	to = &cl.frame.local;

	clgame.dllFuncs.pfnPostRunCmd( from, to, ucmd, clgame.pmove->runfuncs, cl.time, cl.random_seed );
}

/*
===================
CL_CheckPredictionError
===================
*/
void CL_CheckPredictionError( void )
{
	int		frame;
	vec3_t		delta;
	cl_entity_t	*player;
	float		flen;

	if( !CL_IsPredicted( )) return;

	player = CL_GetLocalPlayer();
	if( !player ) return;

	// calculate the last usercmd_t we sent that the server has processed
	frame = cls.netchan.incoming_acknowledged;
	frame &= CL_UPDATE_MASK;

	// compare what the server returned with what we had predicted it to be
	VectorSubtract( player->curstate.origin, cl.predicted_origins[frame], delta );

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
		VectorCopy( player->curstate.origin, cl.predicted_origins[frame] );

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
	int		frame = 0;
	int		ack, current;
	cl_entity_t	*player, *viewent;
	clientdata_t	*cd;
	usercmd_t		*cmd;

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

	if( 1 )//// disabled for now!!!!!!!!!!!!!!!!!!!!!!!!! ///////!CL_IsPredicted( ))
	{	
		cmd = cl.refdef.cmd; // use current command

		// run commands even if client predicting is disabled - client expected it
		CL_PreRunCmd( player, cmd );

		VectorCopy( cl.refdef.cl_viewangles, cl.predicted_angles );
		VectorCopy( cd->view_ofs, cl.predicted_viewofs );

		CL_PostRunCmd( player, cmd );
		return;
	}

	ack = cls.netchan.incoming_acknowledged;
	current = cls.netchan.outgoing_sequence;

	// if we are too far out of date, just freeze
	if( current - ack >= CMD_BACKUP )
	{
		if( cl_showmiss->value )
			MsgDev( D_ERROR, "CL_Predict: exceeded CMD_BACKUP\n" );
		return;	
	}
#if 0
	// setup initial pmove state
// FIXME!!!!...
//	VectorCopy( player->v.movedir, clgame.pmove->movedir );
	VectorCopy( cd->origin, clgame.pmove->origin );
	VectorCopy( cd->velocity, clgame.pmove->velocity );
	VectorCopy( player->curstate.basevelocity, clgame.pmove->basevelocity );
	clgame.pmove->flWaterJumpTime = cd->waterjumptime;
	clgame.pmove->onground = (edict_t *)CL_GetEntityByIndex( player->curstate.onground );
	clgame.pmove->usehull = (player->curstate.flags & FL_DUCKING) ? 1 : 0; // reset hull
	
	// run frames
	while( ++ack < current )
	{
		frame = ack & CL_UPDATE_MASK;
		cmd = &cl.cmds[frame];

		CL_PreRunCmd( player, cmd );
		CL_RunCmd( player, cmd );
		CL_PostRunCmd( player, cmd );

		// save for debug checking
		VectorCopy( clgame.pmove->origin, cl.predicted_origins[frame] );
	}

	// copy results out for rendering
	VectorCopy( player->v.view_ofs, cl.predicted_viewofs );
	VectorCopy( clgame.pmove->origin, cl.predicted_origin );
	VectorCopy( clgame.pmove->angles, cl.predicted_angles );
	VectorCopy( clgame.pmove->velocity, cl.predicted_velocity );
#endif
}
