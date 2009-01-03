//=======================================================================
//			Copyright XashXT Group 2008 �
//		        cl_frame.c - client world snapshot
//=======================================================================

#include "common.h"
#include "client.h"

/*
=========================================================================

FRAME PARSING

=========================================================================
*/
void CL_UpdateEntityFields( edict_t *ent )
{
	int	i;

	// always keep an actual
	ent->serialnumber = ent->pvClientData->current.number;

	// copy state to progs
	ent->v.classname = cl.edict_classnames[ent->pvClientData->current.classname];
	ent->v.modelindex = ent->pvClientData->current.model.index;
	ent->v.ambient = ent->pvClientData->current.soundindex;
	ent->v.model = MAKE_STRING( cl.configstrings[CS_MODELS+ent->pvClientData->current.model.index] ); 
	ent->v.weaponmodel = MAKE_STRING( cl.configstrings[CS_MODELS+ent->pvClientData->current.pmodel.index] );
	ent->v.frame = ent->pvClientData->current.model.frame;
	ent->v.sequence = ent->pvClientData->current.model.sequence;
	ent->v.gaitsequence = ent->pvClientData->current.model.gaitsequence;
	ent->v.body = ent->pvClientData->current.model.body;
	ent->v.skin = ent->pvClientData->current.model.skin;
	VectorCopy( ent->pvClientData->current.rendercolor, ent->v.rendercolor );
	VectorCopy( ent->pvClientData->current.velocity, ent->v.velocity );
	VectorCopy( ent->pvClientData->current.origin, ent->v.origin );
	VectorCopy( ent->pvClientData->current.angles, ent->v.angles );
	VectorCopy( ent->pvClientData->prev.origin, ent->v.oldorigin );
	VectorCopy( ent->pvClientData->prev.angles, ent->v.oldangles );
	VectorCopy( ent->pvClientData->current.mins, ent->v.mins );
	VectorCopy( ent->pvClientData->current.maxs, ent->v.maxs );
	ent->v.framerate = ent->pvClientData->current.model.framerate;
	ent->v.colormap = ent->pvClientData->current.model.colormap; 
	ent->v.rendermode = ent->pvClientData->current.rendermode; 
	ent->v.renderamt = ent->pvClientData->current.renderamt; 
	ent->v.renderfx = ent->pvClientData->current.renderfx; 
	ent->v.scale = ent->pvClientData->current.model.scale; 
	ent->v.weapons = ent->pvClientData->current.weapons;
	ent->v.gravity = ent->pvClientData->current.gravity;
	ent->v.health = ent->pvClientData->current.health;
	ent->v.solid = ent->pvClientData->current.solidtype;
	ent->v.movetype = ent->pvClientData->current.movetype;
	if( ent->v.scale == 0.0f ) ent->v.scale = 1.0f;

	for( i = 0; i < MAXSTUDIOBLENDS; i++ )
		ent->v.blending[i] = ent->pvClientData->current.model.blending[i]; 
	for( i = 0; i < MAXSTUDIOCONTROLLERS; i++ )
		ent->v.controller[i] = ent->pvClientData->current.model.controller[i]; 
	
	if( ent->pvClientData->current.aiment )
		ent->v.aiment = EDICT_NUM( ent->pvClientData->current.aiment );
	else ent->v.aiment = NULL;

	ent->v.pContainingEntity = ent;
}

/*
==================
CL_DeltaEntity

Parses deltas from the given base and adds the resulting entity
to the current frame
==================
*/
void CL_DeltaEntity( sizebuf_t *msg, frame_t *frame, int newnum, entity_state_t *old, bool unchanged )
{
	edict_t		*ent;
	entity_state_t	*state;

	ent = EDICT_NUM( newnum );
	state = &cl_parse_entities[cl.parse_entities & (MAX_PARSE_ENTITIES-1)];

	if( unchanged ) *state = *old;
	else MSG_ReadDeltaEntity( msg, old, state, newnum );

	if( state->number == -1 ) return; // entity was delta removed

	cl.parse_entities++;
	frame->num_entities++;

	// some data changes will force no lerping
	if( state->model.index != ent->pvClientData->current.model.index || state->pmodel.index != ent->pvClientData->current.pmodel.index || state->model.body != ent->pvClientData->current.model.body
		|| state->model.sequence != ent->pvClientData->current.model.sequence || abs(state->origin[0] - ent->pvClientData->current.origin[0]) > 512
		|| abs(state->origin[1] - ent->pvClientData->current.origin[1]) > 512 || abs(state->origin[2] - ent->pvClientData->current.origin[2]) > 512 )
	{
		ent->pvClientData->serverframe = -99;
	}

	if( ent->pvClientData->serverframe != cl.frame.serverframe - 1 )
	{	
		// duplicate the current state so lerping doesn't hurt anything
		ent->pvClientData->prev = *state;
	}
	else
	{	// shuffle the last state to previous
		ent->pvClientData->prev = ent->pvClientData->current;
	}

	ent->pvClientData->serverframe = cl.frame.serverframe;
	ent->pvClientData->current = *state;

	// update prvm fields
	CL_UpdateEntityFields( ent );
}

/*
==================
CL_ParsePacketEntities

An svc_packetentities has just been parsed, deal with the
rest of the data stream.
==================
*/
void CL_ParsePacketEntities( sizebuf_t *msg, frame_t *oldframe, frame_t *newframe )
{
	int		newnum;
	entity_state_t	*oldstate;
	int		oldindex, oldnum;

	newframe->parse_entities = cl.parse_entities;
	newframe->num_entities = 0;

	// delta from the entities present in oldframe
	oldindex = 0;
	oldstate = NULL;
	if( !oldframe ) oldnum = MAX_ENTNUMBER;
	else
	{
		if( oldindex >= oldframe->num_entities )
		{
			oldnum = MAX_ENTNUMBER;
		}
		else
		{
			oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}

	while( 1 )
	{
		// read the entity index number
		newnum = MSG_ReadShort( msg );
		if( !newnum ) break; // end of packet entities

		if( msg->readcount > msg->cursize )
			Host_Error("CL_ParsePacketEntities: end of message[%d > %d]\n", msg->readcount, msg->cursize );

		while( oldnum < newnum )
		{	
			// one or more entities from the old packet are unchanged
			CL_DeltaEntity( msg, newframe, oldnum, oldstate, true );
			
			oldindex++;

			if( oldindex >= oldframe->num_entities )
			{
				oldnum = MAX_ENTNUMBER;
			}
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
		}
		if( oldnum == newnum )
		{	
			// delta from previous state
			CL_DeltaEntity( msg, newframe, newnum, oldstate, false );
			oldindex++;

			if( oldindex >= oldframe->num_entities )
			{
				oldnum = MAX_ENTNUMBER;
			}
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
			continue;
		}

		if( oldnum > newnum )
		{	
			// delta from baseline
			edict_t *ent = EDICT_NUM( newnum );
			CL_DeltaEntity( msg, newframe, newnum, &ent->pvClientData->baseline, false );
			continue;
		}

	}

	// any remaining entities in the old frame are copied over
	while( oldnum != MAX_ENTNUMBER )
	{	
		// one or more entities from the old packet are unchanged
		CL_DeltaEntity( msg, newframe, oldnum, oldstate, true );
		oldindex++;

		if( oldindex >= oldframe->num_entities )
		{
			oldnum = MAX_ENTNUMBER;
		}
		else
		{
			oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}
}

/*
================
CL_ParseFrame
================
*/
void CL_ParseFrame( sizebuf_t *msg )
{
	int     		cmd, len, idx;
	edict_t		*clent;
	frame_t		*old;
          
	memset( &cl.frame, 0, sizeof(cl.frame));
	cl.frame.serverframe = MSG_ReadLong( msg );
	cl.frame.deltaframe = MSG_ReadLong( msg );
	cl.frame.servertime = cl.frame.serverframe * Host_FrameTime();
	cl.surpressCount = MSG_ReadByte( msg );

	// If the frame is delta compressed from data that we
	// no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed
	// message 
	if( cl.frame.deltaframe <= 0 )
	{
		cl.frame.valid = true;	// uncompressed frame
		cls.demowaiting = false;	// we can start recording now
		old = NULL;
	}
	else
	{
		old = &cl.frames[cl.frame.deltaframe & UPDATE_MASK];
		if( !old->valid )
		{	
			// should never happen
			MsgDev( D_INFO, "delta from invalid frame (not supposed to happen!).\n" );
		}
		if( old->serverframe != cl.frame.deltaframe )
		{	
			// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			MsgDev( D_INFO, "Delta frame too old.\n" );
		}
		else if( cl.parse_entities - old->parse_entities > MAX_PARSE_ENTITIES - 128 )
		{
			MsgDev( D_INFO, "delta parse_entities too old.\n" );
		}
		else cl.frame.valid = true;	// valid delta parse
	}

	// clamp time 
	if( cl.time > cl.frame.servertime ) cl.time = cl.frame.servertime;
	else if( cl.time < cl.frame.servertime - Host_FrameTime())
		cl.time = cl.frame.servertime - Host_FrameTime();

	// read areabits
	len = MSG_ReadByte( msg );
	MSG_ReadData( msg, &cl.frame.areabits, len );

	// read clientindex
	cmd = MSG_ReadByte( msg );
	if( cmd != svc_playerinfo ) Host_Error( "CL_ParseFrame: not clientindex\n" );
	idx = MSG_ReadByte( msg );
	clent = EDICT_NUM( idx ); // get client
	if(( idx - 1 ) != cl.playernum )
		Host_Error("CL_ParseFrame: invalid playernum (%d should be %d)\n", idx-1, cl.playernum );

	// read packet entities
	cmd = MSG_ReadByte( msg );
	if( cmd != svc_packetentities ) Host_Error("CL_ParseFrame: not packetentities[%d]\n", cmd );
	CL_ParsePacketEntities( msg, old, &cl.frame );

	// now we can reading delta player state
	if( old ) cl.frame.ps = MSG_ParseDeltaPlayer( &old->ps, &clent->pvClientData->current );
	else cl.frame.ps = MSG_ParseDeltaPlayer( NULL, &clent->pvClientData->current );

	// FIXME
	if( cls.state == ca_cinematic || cls.demoplayback )
		cl.frame.ps.pm_type = PM_FREEZE; // demo or movie playback

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.serverframe & UPDATE_MASK] = cl.frame;

	if( cl.frame.valid )
	{
		if( cls.state != ca_active )
		{
			cls.state = ca_active;
			cl.force_refdef = true;
			// getting a valid frame message ends the connection process
			VectorCopy( cl.frame.ps.origin, cl.predicted_origin );
			VectorCopy( cl.frame.ps.viewangles, cl.predicted_angles );
		}
		CL_CheckPredictionError();
	}
}

/*
==========================================================================

INTERPOLATE BETWEEN FRAMES TO GET RENDERING PARMS

==========================================================================
*/
/*
===============
CL_AddPacketEntities

===============
*/
void CL_AddPacketEntities( frame_t *frame )
{
	entity_state_t	*s1;
	edict_t		*ent;
	int		pnum;

	for( pnum = 0; pnum < frame->num_entities; pnum++ )
	{
		s1 = &cl_parse_entities[(frame->parse_entities + pnum)&(MAX_PARSE_ENTITIES-1)];
		ent = EDICT_NUM( s1->number );

		re->AddRefEntity( ent, s1->ed_type, cl.refdef.lerpfrac );
	}
}

/*
==============
CL_AddViewWeapon
==============
*/
void CL_AddViewWeapon( entity_state_t *ps )
{
	// allow the gun to be completely removed
	if( !cl_gun->value ) return;

	// don't draw gun if in wide angle view
	if( ps->fov > 135 ) return;
	if( !ps->viewmodel ) return;

	cl.viewent.v.scale = 1.0f;
	cl.viewent.serialnumber = -1;
	cl.viewent.v.framerate = 1.0f;
	cl.viewent.v.effects |= EF_MINLIGHT;
	cl.viewent.v.modelindex = ps->viewmodel;
	VectorCopy( cl.refdef.vieworg, cl.viewent.v.origin );
	VectorCopy( cl.refdef.viewangles, cl.viewent.v.angles );
	VectorCopy( cl.refdef.vieworg, cl.viewent.v.oldorigin );
	VectorCopy( cl.refdef.viewangles, cl.viewent.v.oldangles );
	re->AddRefEntity( &cl.viewent, ED_VIEWMODEL, cl.refdef.lerpfrac );
}


/*
===============
CL_CalcViewValues

Sets cl.refdef view values
===============
*/
void CL_CalcViewValues( void )
{
	int		i;
	float		lerp, backlerp;
	frame_t		*oldframe;
	entity_state_t	*ps, *ops;
	edict_t		*clent;

	// clamp time
	if( cl.time > cl.frame.servertime )
	{
		if( cl_showclamp->value )
			Msg ("high clamp %i\n", cl.time - cl.frame.servertime);
		cl.time = cl.frame.servertime;
		cl.refdef.lerpfrac = 1.0f;
	}
	else if (cl.time < cl.frame.servertime - Host_FrameTime())
	{
		if (cl_showclamp->value)
			Msg( "low clamp %i\n", cl.frame.servertime - Host_FrameTime() - cl.time);
		cl.time = cl.frame.servertime - Host_FrameTime();
		cl.refdef.lerpfrac = 0.0f;
	}
	else cl.refdef.lerpfrac = 1.0 - (cl.frame.servertime - cl.time) * 0.01f;

	// find the previous frame to interpolate from
	ps = &cl.frame.ps;
	i = (cl.frame.serverframe - 1) & UPDATE_MASK;
	oldframe = &cl.frames[i];
	if( oldframe->serverframe != cl.frame.serverframe-1 || !oldframe->valid )
		oldframe = &cl.frame; // previous frame was dropped or invalid
	ops = &oldframe->ps;

	// see if the player entity was teleported this frame
	if( ps->pm_flags & PMF_TIME_TELEPORT )
		ops = ps;	// don't interpolate
	lerp = cl.refdef.lerpfrac;

	// calculate the origin
	if((cl_predict->value) && !(cl.frame.ps.pm_flags & PMF_NO_PREDICTION) && !cls.demoplayback )
	{	
		// use predicted values
		int	delta;

		backlerp = 1.0 - lerp;
		for( i = 0; i < 3; i++ )
		{
			cl.refdef.vieworg[i] = cl.predicted_origin[i] + ops->viewoffset[i] 
				+ cl.refdef.lerpfrac * (ps->viewoffset[i] - ops->viewoffset[i]) - backlerp * cl.prediction_error[i];
		}

		// smooth out stair climbing
		delta = cls.realtime - cl.predicted_step_time;
		if( delta < Host_FrameTime()) cl.refdef.vieworg[2] -= cl.predicted_step * (Host_FrameTime() - delta) * 0.01f;
	}
	else
	{
		// just use interpolated values
		for( i = 0; i < 3; i++ )
			cl.refdef.vieworg[i] = LerpView( ops->origin[i], ps->origin[i], ops->viewoffset[i], ps->viewoffset[i], lerp );
	}

	// if not running a demo or on a locked frame, add the local angle movement
	if( cls.demoplayback )
	{
		for (i = 0; i < 3; i++)
			cl.refdef.viewangles[i] = LerpAngle( ops->viewangles[i], ps->viewangles[i], lerp );
	}
	else
	{	
		// in-game use predicted values
		for (i = 0; i < 3; i++) cl.refdef.viewangles[i] = cl.predicted_angles[i];
	}

	for( i = 0; i < 3; i++ )
		cl.refdef.viewangles[i] += LerpAngle( ops->punch_angles[i], ps->punch_angles[i], lerp );

	AngleVectors( cl.refdef.viewangles, cl.refdef.forward, cl.refdef.right, cl.refdef.up );

	// interpolate field of view
	cl.refdef.fov_x = ops->fov + lerp * ( ps->fov - ops->fov );
	clent = EDICT_NUM( cl.playernum + 1 );

	// add the weapon
	CL_AddViewWeapon( ps );

	cl.refdef.iWeaponBits = ps->weapons;
	cls.dllFuncs.pfnUpdateClientData( &cl.refdef, (cl.time * 0.001f));
}

/*
===============
CL_AddEntities

Emits all entities, particles, and lights to the refresh
===============
*/
void CL_AddEntities( void )
{
	if( cls.state != ca_active )
		return;

	CL_CalcViewValues();
	CL_AddPacketEntities( &cl.frame );
	CL_AddParticles();
	CL_AddDLights();
	CL_AddLightStyles();
	CL_AddDecals();
}

//
// sound engine implementation
//

void CL_GetEntitySoundSpatialization( int entnum, vec3_t origin, vec3_t velocity )
{
	edict_t		*ent;
	cmodel_t		*cmodel;
	vec3_t		midPoint;

	if( entnum < 0 || entnum >= host.max_edicts )
	{
		MsgDev( D_ERROR, "CL_GetEntitySoundSpatialization: invalid entnum %d\n", entnum );
		VectorCopy( vec3_origin, origin );
		VectorCopy( vec3_origin, velocity );
		return;
	}

	ent = EDICT_NUM( entnum );

	// calculate origin
	origin[0] = ent->pvClientData->prev.origin[0] + (ent->pvClientData->current.origin[0] - ent->pvClientData->prev.origin[0]) * cl.refdef.lerpfrac;
	origin[1] = ent->pvClientData->prev.origin[1] + (ent->pvClientData->current.origin[1] - ent->pvClientData->prev.origin[1]) * cl.refdef.lerpfrac;
	origin[2] = ent->pvClientData->prev.origin[2] + (ent->pvClientData->current.origin[2] - ent->pvClientData->prev.origin[2]) * cl.refdef.lerpfrac;

	// calculate velocity
	VectorSubtract( ent->pvClientData->current.origin, ent->pvClientData->prev.origin, velocity);
	VectorScale(velocity, 10, velocity);

	// if a brush model, offset the origin
	if( VectorIsNull( origin ))
	{
		cmodel = cl.models[ent->pvClientData->current.model.index];
		if( !cmodel ) return;
		VectorAverage( cmodel->mins, cmodel->maxs, midPoint );
		VectorAdd( origin, midPoint, origin );
	}
}

/*
=================
S_AddLoopingSounds

Entities with a sound field will generate looping sounds that are
automatically started and stopped as the entities are sent to the
client
=================
*/
void CL_AddLoopingSounds( void )
{
	entity_state_t	*ent;
	int		num, i;

	if( cls.state != ca_active ) return;
	if( cl_paused->integer ) return;
	if( !cl.audio_prepped ) return;

	for( i = 0; i < cl.frame.num_entities; i++ )
	{
		num = (cl.frame.parse_entities + i)&(MAX_PARSE_ENTITIES-1);
		ent = &cl_parse_entities[num];

		switch( ent->ed_type )
		{
		case ED_MOVER:
		case ED_AMBIENT:
		case ED_NORMAL: break;
		default: continue;
		}

		if( !ent->soundindex ) continue;
		S_AddLoopingSound( ent->number, cl.sound_precache[ent->soundindex], 1.0f, ATTN_IDLE );
	}
}