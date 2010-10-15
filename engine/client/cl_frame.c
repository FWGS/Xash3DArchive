//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        cl_frame.c - client world snapshot
//=======================================================================

#include "common.h"
#include "client.h"
#include "net_encode.h"
#include "entity_types.h"
#include "input.h"

/*
=========================================================================

FRAME PARSING

=========================================================================
*/
void CL_UpdateEntityFields( cl_entity_t *ent )
{
	// set player state
	ent->player = ( ent->curstate.entityType == ET_PLAYER ) ? true : false;
	ent->onground = CL_GetEntityByIndex( ent->curstate.onground );

	// FIXME: this very-very temporary stuffffffff
	// make me lerping
	VectorCopy( ent->curstate.origin, ent->origin );
	VectorCopy( ent->curstate.angles, ent->angles );
}

/*
==================
CL_UpdateStudioVars

Update studio latched vars so interpolation work properly
==================
*/
void CL_UpdateStudioVars( cl_entity_t *ent, entity_state_t *newstate )
{
	int	i;

	if( newstate->effects & EF_NOINTERP )
	{
		ent->latched.sequencetime = 0.0f; // no lerping between sequences
		ent->latched.prevsequence = newstate->sequence; // keep an actual
		ent->latched.prevanimtime = newstate->animtime;

		VectorCopy( newstate->origin, ent->latched.prevorigin );
		VectorCopy( newstate->angles, ent->latched.prevangles );

		// copy controllers
		for( i = 0; i < 4; i++ )
			ent->latched.prevcontroller[i] = newstate->controller[i];

		// copy blends
		for( i = 0; i < 4; i++ )
			ent->latched.prevblending[i] = newstate->blending[i];

		newstate->effects &= ~EF_NOINTERP;
		return;
	}

	// sequence has changed, hold the previous sequence info
	if( newstate->sequence != ent->curstate.sequence )
	{
		if( ent->curstate.entityType == ET_PLAYER )
			ent->latched.sequencetime = ent->curstate.animtime + 0.01f;
		else ent->latched.sequencetime = ent->curstate.animtime + 0.1f;
			
		// save current blends to right lerping from last sequence
		for( i = 0; i < 4; i++ )
			ent->latched.prevseqblending[i] = ent->curstate.blending[i];
		ent->latched.prevsequence = ent->curstate.sequence;	// save old sequence
	}

	if( newstate->animtime != ent->curstate.animtime )
	{
		// client got new packet, shuffle animtimes
		ent->latched.prevanimtime = ent->curstate.animtime;
		VectorCopy( newstate->origin, ent->latched.prevorigin );
		VectorCopy( newstate->angles, ent->latched.prevangles );

		for( i = 0; i < 4; i++ )
			ent->latched.prevcontroller[i] = newstate->controller[i];
	}

	// copy controllers
	for( i = 0; i < 4; i++ )
	{
		if( ent->curstate.controller[i] != newstate->controller[i] )
			ent->latched.prevcontroller[i] = ent->curstate.controller[i];
	}

	// copy blends
	for( i = 0; i < 4; i++ )
		ent->latched.prevblending[i] = ent->curstate.blending[i];

	if( !VectorCompare( newstate->origin, ent->curstate.origin ))
		VectorCopy( ent->curstate.origin, ent->latched.prevorigin );

	if( !VectorCompare( newstate->angles, ent->curstate.angles ))
		VectorCopy( ent->curstate.angles, ent->latched.prevangles );
}

void CL_DeltaEntity( sizebuf_t *msg, frame_t *frame, int newnum, entity_state_t *old, bool unchanged )
{
	cl_entity_t	*ent;
	entity_state_t	*state;
	bool		newent = (old) ? false : true;
	bool		noInterp = false;
	int		result = 1;

	ent = EDICT_NUM( newnum );
	state = &cls.packet_entities[cls.next_client_entities % cls.num_client_entities];

	if( newent ) old = &ent->baseline;

	if( unchanged ) *state = *old;
	else result = MSG_ReadDeltaEntity( msg, old, state, newnum, sv_time( ));

	if( !result )
	{
		if( newent ) Host_Error( "Cl_DeltaEntity: tried to release new entity\n" );
			
		if( state->number == -1 )
		{
//			Msg( "Entity %s was removed from server\n", ent->curstate.classname );
			CL_FreeEntity( ent );
		}
		else
		{
//			Msg( "Entity %s was removed from delta-message\n", ent->curstate.classname );
			ent->curstate.effects |= EF_NODRAW; // don't rendering
			clgame.dllFuncs.pfnUpdateOnRemove( ent );
		}

		// entity was delta removed
		return;
	}

	cls.next_client_entities++;
	frame->num_entities++;

	if( ent->index <= 0 )
	{
		CL_InitEntity( ent );
		noInterp = true;
	}

	if( state->effects & EF_NOINTERP || noInterp )
	{	
		// duplicate the current state so lerping doesn't hurt anything
		ent->prevstate = *state;
	}
	else
	{	
		// shuffle the last state to previous
		ent->prevstate = ent->curstate;
	}

	// NOTE: always check modelindex for new state not current
	if( CM_GetModelType( state->modelindex ) == mod_studio )
		CL_UpdateStudioVars( ent, state );

	// set right current state
	ent->curstate = *state;

	CL_LinkEdict( ent ); // relink entity
}

/*
=================
CL_FlushEntityPacket
=================
*/
void CL_FlushEntityPacket( sizebuf_t *msg )
{
	int		newnum;
	entity_state_t	from, to;

	MsgDev( D_INFO, "FlushEntityPacket()\n" );
	Mem_Set( &from, 0, sizeof( from ));

	cl.frames[cl.parsecountmod].valid = false;
	cl.validsequence = 0; // can't render a frame

	// read it all, but ignore it
	while( 1 )
	{
		newnum = BF_ReadWord( msg );
		if( !newnum ) break; // done

		if( BF_CheckOverflow( msg ))
			Host_Error( "CL_FlushEntityPacket: read overflow\n" );

		while( newnum >= clgame.numEntities )
			clgame.numEntities++;

		MSG_ReadDeltaEntity( msg, &from, &to, newnum, sv_time( ));
	}
}

/*
==================
CL_ParsePacketEntities

An svc_packetentities has just been parsed, deal with the
rest of the data stream.
==================
*/
void CL_ParsePacketEntities( sizebuf_t *msg, bool delta )
{
	frame_t		*newframe, *oldframe;
	int		oldindex, newnum, oldnum;
	entity_state_t	*oldent;
	int		count;

	// first, allocate packet for new frame
	count = BF_ReadWord( msg );

	newframe = &cl.frames[cl.parsecountmod];

	// allocate parse entities
	newframe->first_entity = cls.next_client_entities;
	newframe->num_entities = 0;

	if( delta )
	{
		int	subtracted, delta_sequence;

		delta_sequence = BF_ReadByte( msg );
		subtracted = ((( cls.netchan.incoming_sequence & 0xFF ) - delta_sequence ) & 0xFF );

		if( subtracted == 0 )
		{
			Host_Error( "CL_DeltaPacketEntities: update too old, connection dropped.\n" );
			return;
		}

		if( subtracted >= CL_UPDATE_MASK )
		{	
			// we can't use this, it is too old
			CL_FlushEntityPacket( msg );
			return;
		}

		oldframe = &cl.frames[delta_sequence & CL_UPDATE_MASK];

		if( !oldframe->valid )
		{	
			// should never happen
			MsgDev( D_INFO, "delta from invalid frame (not supposed to happen!)\n" );
		}

		if(( oldframe->delta_sequence & 0xFF ) != (( delta_sequence - 1 ) & 0xFF ))
		{	
			// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			MsgDev( D_INFO, "CL_ParsePacketEntities: delta frame too old\n" );
		}
		else if( cls.next_client_entities - oldframe->first_entity > cls.num_client_entities - 128 )
		{
			MsgDev( D_INFO, "CL_ParsePacketEntities: delta parse_entities too old\n" );
		}
		else newframe->valid = true;	// valid delta parse

		if(( cl.delta_sequence & CL_UPDATE_MASK ) != ( delta_sequence & CL_UPDATE_MASK ))
			MsgDev( D_WARN, "CL_ParsePacketEntities: mismatch delta_sequence %i != %i\n", cl.delta_sequence, delta );

		// keep sequence an actual
		newframe->delta_sequence = delta_sequence;
	}
	else
	{
		// this is a full update that we can start delta compressing from now
		newframe->delta_sequence = ( cls.netchan.incoming_sequence - 1 ) & 0xFF;
		newframe->valid = true;
		oldframe = NULL;

		cl.force_send_usercmd = true;	// send reply
		cls.demowaiting = false;	// we can start recording now
	}

	// mark current delta state
	cl.validsequence = cls.netchan.incoming_sequence;

	oldent = NULL;
	oldindex = 0;

	if( !oldframe )
	{
		oldnum = MAX_ENTNUMBER;
	}
	else
	{
		if( oldindex >= oldframe->num_entities )
		{
			oldnum = MAX_ENTNUMBER;
		}
		else
		{
			oldent = &cls.packet_entities[(oldframe->first_entity+oldindex) % cls.num_client_entities];
			oldnum = oldent->number;
		}
	}

	while( 1 )
	{
		newnum = BF_ReadWord( msg );
		if( !newnum ) break; // end of packet entities

		if( BF_CheckOverflow( msg ))
			Host_Error( "CL_ParsePacketEntities: read overflow\n" );

		while( newnum >= clgame.numEntities )
			clgame.numEntities++;

		while( oldnum < newnum )
		{	
			// one or more entities from the old packet are unchanged
			CL_DeltaEntity( msg, newframe, oldnum, oldent, true );
			
			oldindex++;

			if( oldindex >= oldframe->num_entities )
			{
				oldnum = MAX_ENTNUMBER;
			}
			else
			{
				oldent = &cls.packet_entities[(oldframe->first_entity+oldindex) % cls.num_client_entities];
				oldnum = oldent->number;
			}
		}

		if( oldnum == newnum )
		{	
			// delta from previous state
			CL_DeltaEntity( msg, newframe, newnum, oldent, false );
			oldindex++;

			if( oldindex >= oldframe->num_entities )
			{
				oldnum = MAX_ENTNUMBER;
			}
			else
			{
				oldent = &cls.packet_entities[(oldframe->first_entity+oldindex) % cls.num_client_entities];
				oldnum = oldent->number;
			}
			continue;
		}

		if( oldnum > newnum )
		{	
			// delta from baseline ?
			CL_DeltaEntity( msg, newframe, newnum, NULL, false );
			continue;
		}
	}

	// any remaining entities in the old frame are copied over
	while( oldnum != MAX_ENTNUMBER )
	{	
		// one or more entities from the old packet are unchanged
		CL_DeltaEntity( msg, newframe, oldnum, oldent, true );
		oldindex++;

		if( oldindex >= oldframe->num_entities )
		{
			oldnum = MAX_ENTNUMBER;
		}
		else
		{
			oldent = &cls.packet_entities[(oldframe->first_entity+oldindex) % cls.num_client_entities];
			oldnum = oldent->number;
		}
	}

	cl.frame = *newframe;
	if( !cl.frame.valid ) return;
		
	if( cls.state != ca_active )
	{
		cl_entity_t	*player;

		// client entered the game
		cls.state = ca_active;
		cl.force_refdef = true;
		cls.changelevel = false;	// changelevel is done

		player = CL_GetLocalPlayer();
		SCR_MakeLevelShot();	// make levelshot if needs

		Cvar_SetValue( "scr_loading", 0.0f ); // reset progress bar	

		// getting a valid frame message ends the connection process
		VectorCopy( player->origin, cl.predicted_origin );
		VectorCopy( player->angles, cl.predicted_angles );
	}

	CL_CheckPredictionError();
}
		
/*
================
CL_ParseFrame
================
*/
void CL_ParseFrame( sizebuf_t *msg )
{
	Mem_Set( &cl.frame, 0, sizeof( cl.frame ));

	cl.mtime[1] = cl.mtime[0];
	cl.mtime[0] = BF_ReadFloat( msg );
	cl.surpressCount = BF_ReadByte( msg );

	if( !cl.frame.valid ) return;

	if( cls.state != ca_active )
	{
		cl_entity_t	*player;

		// client entered the game
		cls.state = ca_active;
		cl.force_refdef = true;
		cls.changelevel = false;	// changelevel is done

		player = CL_GetLocalPlayer();
		SCR_MakeLevelShot();	// make levelshot if needs

		Cvar_SetValue( "scr_loading", 0.0f ); // reset progress bar	
		// getting a valid frame message ends the connection process
		VectorCopy( player->origin, cl.predicted_origin );
		VectorCopy( player->angles, cl.predicted_angles );

		// request new HUD values
		BF_WriteByte( &cls.netchan.message, clc_stringcmd );
		BF_WriteString( &cls.netchan.message, "fullupdate" );
	}

	CL_CheckPredictionError();
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
	cl_entity_t	*ent, *clent;
	int		e, entityType;

	// now recalc actual entcount
	for( ; EDICT_NUM( clgame.numEntities - 1 )->index == -1; clgame.numEntities-- );

	clent = CL_GetLocalPlayer();
	if( !clent ) return;

	// update client vars
	clgame.dllFuncs.pfnTxferLocalOverrides( &clent->curstate, &cl.frame.clientdata );

	// if viewmodel has changed update sequence here
	if( clgame.viewent.curstate.modelindex != cl.frame.clientdata.viewmodel )
	{
		cl_entity_t *view = &clgame.viewent;
		CL_WeaponAnim( view->curstate.sequence, view->curstate.body );
	}

	// setup player viewmodel (only for local player!)
	clgame.viewent.curstate.modelindex = cl.frame.clientdata.viewmodel;

	for( e = 1; e < clgame.numEntities; e++ )
	{
		ent = CL_GetEntityByIndex( e );
		if( !ent ) continue;

		// entity not visible for this client
		if( ent->curstate.effects & EF_NODRAW )
			continue;

		entityType = ent->curstate.entityType;
		CL_UpdateEntityFields( ent );

		if( clgame.dllFuncs.pfnAddVisibleEntity( ent, entityType ))
		{
			if( entityType == ET_PORTAL && !VectorCompare( ent->curstate.origin, ent->curstate.vuser1 ))
				cl.render_flags |= RDF_PORTALINVIEW;
		}
		// NOTE: skyportal entity never added to rendering
		if( entityType == ET_SKYPORTAL ) cl.render_flags |= RDF_SKYPORTALINVIEW;
	}
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

	cl.render_flags = 0;

	clgame.dllFuncs.pfnStartFrame();	// new frame has begin
	CL_AddPacketEntities( &cl.frame );
	clgame.dllFuncs.pfnCreateEntities();

	CL_FireEvents();	// so tempents can be created immediately
	CL_AddDLights();
	CL_AddLightStyles();

	// perfomance test
	CL_TestLights();
}

//
// sound engine implementation
//
bool CL_GetEntitySpatialization( int entnum, vec3_t origin, vec3_t velocity )
{
	cl_entity_t	*ent;
	bool		from_baseline = false;

	if( entnum < 0 || entnum > clgame.numEntities )
		return false;

	ent = EDICT_NUM( entnum );

	if( ent->index == -1 || ent->curstate.number != entnum )
	{
		// this entity isn't visible but maybe it have baseline ?
		if( ent->baseline.number != entnum )
			return false;
		from_baseline = true;
	}

	if( from_baseline )
	{
		// setup origin and velocity
		if( origin ) VectorCopy( ent->baseline.origin, origin );
		if( velocity ) VectorCopy( ent->baseline.velocity, velocity );

		// if a brush model, offset the origin
		if( origin && CM_GetModelType( ent->baseline.modelindex ) == mod_brush )
		{
			vec3_t	mins, maxs, midPoint;
	
			Mod_GetBounds( ent->baseline.modelindex, mins, maxs );
			VectorAverage( mins, maxs, midPoint );
			VectorAdd( origin, midPoint, origin );
		}
	}
	else
	{

		// setup origin and velocity
		if( origin ) VectorCopy( ent->origin, origin );
		if( velocity ) VectorCopy( ent->curstate.velocity, velocity );

		// if a brush model, offset the origin
		if( origin && CM_GetModelType( ent->curstate.modelindex ) == mod_brush )
		{
			vec3_t	mins, maxs, midPoint;
	
			Mod_GetBounds( ent->curstate.modelindex, mins, maxs );
			VectorAverage( mins, maxs, midPoint );
			VectorAdd( origin, midPoint, origin );
		}
	}
	return true;
}

void CL_ExtraUpdate( void )
{
	S_ExtraUpdate();
}