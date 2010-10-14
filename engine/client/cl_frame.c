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

bool CL_ParseDelta( sizebuf_t *msg, entity_state_t *from, entity_state_t *to, int newnum )
{
	cl_entity_t	*ent;
	int		alive;
	int		noInterp = false;

	ent = EDICT_NUM( newnum );
	alive = MSG_ReadDeltaEntity( msg, from, to, newnum, sv_time( ));

	if( !alive )
	{
		// entity was delta removed

		if( to->number == -1 )
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
		return false;
	}

	if( ent->index <= 0 )
	{
		// spawn entity
		CL_InitEntity( ent );
		noInterp = true;
	}

	if( to->effects & EF_NOINTERP || noInterp )
		ent->prevstate = *to;	// duplicate the current state so lerping doesn't hurt anything
	else ent->prevstate = ent->curstate;	// shuffle the last state to previous

	// NOTE: always check modelindex for new state not current
	if( CM_GetModelType( to->modelindex ) == mod_studio )
		CL_UpdateStudioVars( ent, to );

	// set right current state
	ent->curstate = *to;

	CL_LinkEdict( ent ); // relink entity

	return true;
}

/*
=======================
CL_ClearPacketEntities
=======================
*/
void CL_ClearPacketEntities( frame_t *frame )
{
	packet_entities_t	*packet;

	ASSERT( frame != NULL );

	packet = &frame->entities;

	if( packet )
	{
		if( packet->entities != NULL )
			Mem_Free( packet->entities );

		packet->num_entities = 0;
		packet->max_entities = 0;
		packet->entities = NULL;
	}
}

/*
=======================
CL_AllocPacketEntities
=======================
*/
void CL_AllocPacketEntities( frame_t *frame, int count )
{
	packet_entities_t	*packet;

	ASSERT( frame != NULL );

	packet = &frame->entities;
	if( count < 1 ) count = 1;

	// check if new frame has more entities than previous
	if( packet->max_entities < count )
	{
		CL_ClearPacketEntities( frame );
		packet->entities = Mem_Alloc( clgame.mempool, sizeof( entity_state_t ) * count );
		packet->max_entities = count;
	}
	packet->num_entities = count;
}

/*
================
CL_ClearFrames

free client frames memory
================
*/
void CL_ClearFrames( void )
{
	frame_t	*frame;
	int	i;

	for( i = 0, frame = cl.frames; i < CL_UPDATE_BACKUP; i++, frame++ )
	{
		CL_ClearPacketEntities( frame );
	}
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
	frame_t		*frame;
	int		oldpacket, newpacket;
	packet_entities_t	*from, *to, dummy;
	int		oldindex, newindex;
	int		count, delta_sequence = -1;
	int		newnum, oldnum;
	bool		full = false;

	// first, allocate packet for new frame
	count = BF_ReadWord( msg );

	newpacket = cl.parsecountmod;
	frame = &cl.frames[newpacket];
	frame->valid = true;

	if( delta )
	{
		oldpacket = BF_ReadByte( msg );

		if(( cl.delta_sequence & CL_UPDATE_MASK ) != ( oldpacket & CL_UPDATE_MASK ))
			MsgDev( D_WARN, "CL_ParsePacketEntities: mismatch delta_sequence %i != %i\n", cl.delta_sequence, oldpacket );
	}
	else oldpacket = -1;

	if( oldpacket != -1 )
	{
		int	subtracted = ((( cls.netchan.incoming_sequence & 0xFF ) - oldpacket ) & 0xFF );

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

		from = &cl.frames[oldpacket & CL_UPDATE_MASK].entities;

		// alloc room for save entities from prevframe too
		if( from->num_entities > count )
			count = from->num_entities;
	}
	else
	{	
		// this is a full update that we can start delta compressing from now
		dummy.entities = NULL;
		dummy.num_entities = 0;
		cls.demowaiting = false;	// we can start recording now
		cl.force_send_usercmd = true;
		from = &dummy;
		full = true;
	}

	CL_AllocPacketEntities( frame, count );
	to = &frame->entities;

	// mark current delta state
	cl.validsequence = cls.netchan.incoming_sequence;

	oldindex = 0;
	newindex = 0;
	to->num_entities = 0;

	while( 1 )
	{
		newnum = BF_ReadWord( msg );

		if( !newnum )
		{
			while( oldindex < from->num_entities )
			{	
				// copy all the rest of the entities from the old packet
				to->entities[newindex] = from->entities[oldindex];
				newindex++;
				oldindex++;
			}
			break;
		}

		if( BF_CheckOverflow( msg ))
			Host_Error( "CL_ParsePacketEntities: read overflow\n" );

		while( newnum >= clgame.numEntities )
			clgame.numEntities++;

		oldnum = oldindex >= from->num_entities ? MAX_ENTNUMBER : from->entities[oldindex].number;

		while( newnum > oldnum )
		{
			if( full )
			{
				MsgDev( D_WARN, "CL_ParsePacketEntities: oldcopy on full update\n" );
				CL_FlushEntityPacket( msg );
				return;
			}

			to->entities[newindex] = from->entities[oldindex];
			newindex++;
			oldindex++;
			oldnum = oldindex >= from->num_entities ? MAX_ENTNUMBER : from->entities[oldindex].number;
		}

		if( newnum < oldnum )
		{
			cl_entity_t	*ent;

			// new from baseline
			ent = EDICT_NUM( newnum );

			if( !CL_ParseDelta( msg, &ent->baseline, &to->entities[newindex], newnum ))
			{
				if( full )
				{
					MsgDev( D_WARN, "CL_ParsePacketEntities: remove on full update\n" );
					CL_FlushEntityPacket( msg );
					return;
				}
				continue;
			}

			newindex++;
			continue;
		}

		if( newnum == oldnum )
		{	
			// delta from previous
			if( full )
			{
				cl.validsequence = 0;
				MsgDev( D_WARN, "CL_ParsePacketEntities: delta on full update\n" );
			}

			if( !CL_ParseDelta( msg, &from->entities[oldindex], &to->entities[newindex], newnum ))
			{
				// entity was delta-removed
				oldindex++;
				continue;
			}

			newindex++;
			oldindex++;
		}

	}

	to->num_entities = newindex;	// done

	cl.frame = *frame;
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