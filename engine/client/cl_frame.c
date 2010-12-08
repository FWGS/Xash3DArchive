//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        cl_frame.c - client world snapshot
//=======================================================================

#include "common.h"
#include "client.h"
#include "net_encode.h"
#include "entity_types.h"
#include "cl_tent.h"
#include "dlight.h"
#include "input.h"

qboolean CL_IsPlayerIndex( int idx )
{
	if( idx > 0 && idx <= cl.maxclients )
		return true;
	return false;
}

/*
=========================================================================

FRAME PARSING

=========================================================================
*/
void CL_UpdateEntityFields( cl_entity_t *ent )
{
	// FIXME: this very-very temporary stuffffffff
	// make me lerping
	VectorCopy( ent->curstate.origin, ent->origin );
	VectorCopy( ent->curstate.angles, ent->angles );

	ent->model = CM_ClipHandleToModel( ent->curstate.modelindex );
	ent->curstate.msg_time = cl.time;
}

qboolean CL_AddVisibleEntity( cl_entity_t *ent, int entityType )
{
	model_t	*mod;

	mod = CM_ClipHandleToModel( ent->curstate.modelindex );
	if( !mod ) return false;

	// if entity is beam add it here
	// because render doesn't know how to draw beams
	if( entityType == ET_BEAM )
	{
		CL_AddCustomBeam( ent );
		return true;
	}

	// check for adding this entity
	if( !clgame.dllFuncs.pfnAddEntity( entityType, ent, mod->name ))
		return false;

	if( !R_AddEntity( ent, entityType ))
		return false;

	// apply effects
	if( ent->curstate.effects & EF_BRIGHTFIELD )
		CL_EntityParticles( ent );

	// add in muzzleflash effect
	if( ent->curstate.effects & EF_MUZZLEFLASH )
	{
		vec3_t	pos;

		if( entityType == ET_VIEWENTITY )
			ent->curstate.effects &= ~EF_MUZZLEFLASH;

		VectorCopy( ent->attachment[0], pos );

		if(!VectorCompare( pos, ent->origin ))
                    {
			dlight_t	*dl = CL_AllocDlight( 0 );

			VectorCopy( ent->origin, dl->origin );
			dl->die = cl.time + 0.05f;
			dl->color.r = 255;
			dl->color.g = 180;
			dl->color.b = 64;
			dl->radius = 100;
                    }
	}

	// add light effect
	if( ent->curstate.effects & EF_LIGHT )
	{
		dlight_t	*dl = CL_AllocDlight( 0 );
		VectorCopy( ent->origin, dl->origin );
		dl->die = cl.time + 0.001f;	// die at next frame
		dl->color.r = 100;
		dl->color.g = 100;
		dl->color.b = 100;
		dl->radius = 200;
		CL_RocketFlare( ent->origin );
	}

	// add dimlight
	if( ent->curstate.effects & EF_DIMLIGHT )
	{
		if( entityType == ET_PLAYER )
		{
			CL_UpadteFlashlight( ent );
		}
		else
		{
			dlight_t	*dl = CL_AllocDlight( 0 );
			VectorCopy( ent->origin, dl->origin );
			dl->die = cl.time + 0.001f;	// die at next frame
			dl->color.r = 255;
			dl->color.g = 255;
			dl->color.b = 255;
			dl->radius = Com_RandomLong( 200, 230 );
		}
	}	

	if( ent->curstate.effects & EF_BRIGHTLIGHT )
	{			
		dlight_t	*dl = CL_AllocDlight( 0 );
		VectorSet( dl->origin, ent->origin[0], ent->origin[1], ent->origin[2] + 16 );
		dl->die = cl.time + 0.001f;	// die at next frame
		dl->color.r = 255;
		dl->color.g = 255;
		dl->color.b = 255;
		dl->radius = Com_RandomLong( 400, 430 );
	}
	return true;
}

/*
==================
CL_WeaponAnim

Set new weapon animation
==================
*/
void CL_WeaponAnim( int iAnim, int body )
{
	cl_entity_t	*view = &clgame.viewent;

	// anim is changed. update latchedvars
	if( iAnim != view->curstate.sequence )
	{
		int	i;
			
		// save current blends to right lerping from last sequence
		for( i = 0; i < 2; i++ )
			view->latched.prevseqblending[i] = view->curstate.blending[i];
		view->latched.prevsequence = view->curstate.sequence; // save old sequence

		// save animtime
		view->latched.prevanimtime = view->curstate.animtime;
		view->syncbase = -0.01f; // back up to get 0'th frame animations
	}

	view->model = CM_ClipHandleToModel( view->curstate.modelindex );
	view->curstate.entityType = ET_VIEWENTITY;
	view->curstate.animtime = cl_time();	// start immediately
	view->curstate.framerate = 1.0f;
	view->curstate.sequence = iAnim;
	view->latched.prevframe = 0.0f;
	view->curstate.scale = 1.0f;
	view->curstate.frame = 0.0f;
	view->curstate.body = body;
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
		if( ent->index > 0 && ent->index <= cl.maxclients )
			ent->latched.sequencetime = ent->curstate.animtime + 0.01f;
		else ent->latched.sequencetime = ent->curstate.animtime + 0.1f;
			
		// save current blends to right lerping from last sequence
		for( i = 0; i < 4; i++ )
			ent->latched.prevseqblending[i] = ent->curstate.blending[i];
		ent->latched.prevsequence = ent->curstate.sequence;	// save old sequence	
		ent->syncbase = -0.01f; // back up to get 0'th frame animations
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

void CL_DeltaEntity( sizebuf_t *msg, frame_t *frame, int newnum, entity_state_t *old, qboolean unchanged )
{
	cl_entity_t	*ent;
	entity_state_t	*state;
	qboolean		newent = (old) ? false : true;
	qboolean		noInterp = false;
	int		result = 1;

	ent = EDICT_NUM( newnum );
	state = &cls.packet_entities[cls.next_client_entities % cls.num_client_entities];

	if( newent ) old = &ent->baseline;
	if( unchanged )
	{
		*state = *old;
	}
	else
	{
		result = MSG_ReadDeltaEntity( msg, old, state, newnum, CL_IsPlayerIndex( newnum ), sv_time( ));
		state->messagenum = cl.parsecountmod;
	}

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
			CL_KillDeadBeams( ent ); // release dead beams
		}

		// entity was delta removed
		return;
	}

	cls.next_client_entities++;
	frame->num_entities++;

	if( !ent->index )
	{
		CL_InitEntity( ent );
		noInterp = true;
	}

	// set player state
	ent->player = CL_IsPlayerIndex( ent->index );

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
	if( Mod_GetType( state->modelindex ) == mod_studio )
		CL_UpdateStudioVars( ent, state );

	// set right current state
	ent->curstate = *state;

	if( ent->player )
	{
		clgame.dllFuncs.pfnProcessPlayerState( &cl.frame.playerstate[ent->index-1], state );

		// fill private structure for local client
		if(( ent->index - 1 ) == cl.playernum )
			cl.frame.local.playerstate = cl.frame.playerstate[ent->index-1];
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

		MSG_ReadDeltaEntity( msg, &from, &to, newnum, CL_IsPlayerIndex( newnum ), sv_time( ));
	}
}

/*
==================
CL_ParsePacketEntities

An svc_packetentities has just been parsed, deal with the
rest of the data stream.
==================
*/
void CL_ParsePacketEntities( sizebuf_t *msg, qboolean delta )
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

		Cvar_SetFloat( "scr_loading", 0.0f ); // reset progress bar	

		// getting a valid frame message ends the connection process
		VectorCopy( player->origin, cl.predicted_origin );
		VectorCopy( player->angles, cl.predicted_angles );
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
	for( ; !EDICT_NUM( clgame.numEntities - 1 )->index; clgame.numEntities-- );

	clent = CL_GetLocalPlayer();
	if( !clent ) return;

	// update client vars
	clgame.dllFuncs.pfnTxferLocalOverrides( &clent->curstate, &cl.frame.local.client );

	for( e = 1; e < clgame.numEntities; e++ )
	{
		ent = CL_GetEntityByIndex( e );
		if( !ent || !ent->index ) continue;

		// entity not visible for this client
		if( ent->curstate.effects & EF_NODRAW )
			continue;

		CL_UpdateEntityFields( ent );

		if( ent->player ) entityType = ET_PLAYER;
		else if( ent->curstate.entityType == ENTITY_BEAM )
			entityType = ET_BEAM;
		else entityType = ET_NORMAL;

		CL_AddVisibleEntity( ent, entityType );
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

	cl.num_custombeams = 0;

	clgame.dllFuncs.CAM_Think();

	CL_AddPacketEntities( &cl.frame );
	clgame.dllFuncs.pfnCreateEntities();

	CL_FireEvents();	// so tempents can be created immediately
	CL_AddTempEnts();

	// perfomance test
	CL_TestLights();
}

//
// sound engine implementation
//
qboolean CL_GetEntitySpatialization( int entnum, vec3_t origin, vec3_t velocity )
{
	cl_entity_t	*ent;
	qboolean		from_baseline = false;

	if( entnum < 0 || entnum > clgame.numEntities )
		return false;

	ent = EDICT_NUM( entnum );

	if( !ent->index || ent->curstate.number != entnum )
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
		if( origin && Mod_GetType( ent->baseline.modelindex ) == mod_brush )
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
		if( origin && Mod_GetType( ent->curstate.modelindex ) == mod_brush )
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
	clgame.dllFuncs.IN_Accumulate();
	S_ExtraUpdate();
}