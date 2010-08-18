//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        cl_frame.c - client world snapshot
//=======================================================================

#include "common.h"
#include "client.h"
#include "protocol.h"
#include "net_encode.h"
#include "entity_types.h"

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

/*
==================
CL_DeltaEntity

Parses deltas from the given base and adds the resulting entity
to the current frame
==================
*/
void CL_DeltaEntity( sizebuf_t *msg, frame_t *frame, int newnum, entity_state_t *old, bool unchanged )
{
	cl_entity_t	*ent;
	entity_state_t	*state;
	bool		newent = (old) ? false : true;

	ent = EDICT_NUM( newnum );
	state = &cl.entity_curstates[cl.parse_entities & (MAX_PARSE_ENTITIES-1)];

	if( newent ) old = &ent->baseline;

	if( unchanged ) *state = *old;
	else MSG_ReadDeltaEntity( msg, old, state, newnum, cl.frame.servertime );

	if( state->number == -1 )
	{
		if( newent ) Host_Error( "Cl_DeltaEntity: tried to release new entity\n" );
		CL_FreeEntity( ent );
		return; // entity was delta removed
	}

	cl.parse_entities++;
	frame->num_entities++;

	if( ent->index <= 0 )
	{
		CL_InitEntity( ent );
		state->effects |= EF_NOINTERP;
	}

	// some data changes will force no lerping
	if( state->effects & EF_NOINTERP )
		ent->serverframe = -99;

	if( ent->serverframe != cl.frame.serverframe - 1 )
	{	
		// duplicate the current state so lerping doesn't hurt anything
		ent->prevstate = *state;
	}
	else
	{	
		// shuffle the last state to previous
		ent->prevstate = ent->curstate;
	}

	ent->serverframe = cl.frame.serverframe;

	// NOTE: always check modelindex for new state not current
	if( CM_GetModelType( state->modelindex ) == mod_studio )
		CL_UpdateStudioVars( ent, state );

	if( !VectorCompare( state->origin, ent->curstate.origin ))
		CL_LinkEdict( ent ); // relink entity

	// set right current state
	ent->curstate = *state;
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
			oldstate = &cl.entity_curstates[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}

	while( 1 )
	{
		// read the entity index number
		newnum = BF_ReadShort( msg );
		if( !newnum ) break; // end of packet entities

		if( BF_CheckOverflow( msg ))
			Host_Error( "CL_ParsePacketEntities: read overflow\n" );

		while( newnum >= clgame.globals->numEntities )
			clgame.globals->numEntities++;

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
				oldstate = &cl.entity_curstates[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
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
				oldstate = &cl.entity_curstates[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
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
		CL_DeltaEntity( msg, newframe, oldnum, oldstate, true );
		oldindex++;

		if( oldindex >= oldframe->num_entities )
		{
			oldnum = MAX_ENTNUMBER;
		}
		else
		{
			oldstate = &cl.entity_curstates[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}
}

/*
===================
CL_ParseClientData
===================
*/
void CL_ParseClientData( frame_t *from, frame_t *to, sizebuf_t *msg )
{
	clientdata_t	*cd, *ocd;
	clientdata_t	dummy;
	
	cd = &to->cd;

	// clear to old value before delta parsing
	if( !from )
	{
		ocd = &dummy;
		Mem_Set( &dummy, 0, sizeof( dummy ));
	}
	else ocd = &from->cd;

	MSG_ReadClientData( msg, ocd, cd, to->servertime );
}
		
/*
================
CL_ParseFrame
================
*/
void CL_ParseFrame( sizebuf_t *msg )
{
	int	cmd;
	cl_entity_t	*clent;
          
	Mem_Set( &cl.frame, 0, sizeof( cl.frame ));

	cl.frame.serverframe = BF_ReadLong( msg );
	cl.frame.servertime = BF_ReadLong( msg );
	cl.frame.deltaframe = BF_ReadLong( msg );
	cl.serverframetime = BF_ReadByte( msg );
	cl.surpressCount = BF_ReadByte( msg );

	// If the frame is delta compressed from data that we
	// no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed
	// message 
	if( cl.frame.deltaframe <= 0 )
	{
		cl.frame.valid = true;	// uncompressed frame
		cls.demowaiting = false;	// we can start recording now
		cl.oldframe = NULL;
	}
	else
	{
		cl.oldframe = &cl.frames[cl.frame.deltaframe & CL_UPDATE_MASK];
		if( !cl.oldframe->valid )
		{	
			// should never happen
			MsgDev( D_INFO, "delta from invalid frame (not supposed to happen!)\n" );
		}
		if( cl.oldframe->serverframe != cl.frame.deltaframe )
		{	
			// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			MsgDev( D_INFO, "delta frame too old\n" );
		}
		else if( cl.parse_entities - cl.oldframe->parse_entities > MAX_PARSE_ENTITIES - 128 )
		{
			MsgDev( D_INFO, "delta parse_entities too old\n" );
		}
		else cl.frame.valid = true;	// valid delta parse
	}

	// clamp time 
	if( cl.time > cl.frame.servertime )
		cl.time = cl.frame.servertime;
	else if( cl.time < cl.frame.servertime - cl.serverframetime )
		cl.time = cl.frame.servertime - cl.serverframetime;

	// read clientdata
	cmd = BF_ReadByte( msg );
	if( cmd != svc_clientdata ) Host_Error( "CL_ParseFrame: not cliendata[%d]\n", cmd );
	CL_ParseClientData( cl.oldframe, &cl.frame, msg );

	clent = CL_GetLocalPlayer(); // get client

	// read packet entities
	cmd = BF_ReadByte( msg );
	if( cmd != svc_packetentities ) Host_Error( "CL_ParseFrame: not packetentities[%d]\n", cmd );
	CL_ParsePacketEntities( msg, cl.oldframe, &cl.frame );

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.serverframe & CL_UPDATE_MASK] = cl.frame;

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
	for( ; EDICT_NUM( clgame.globals->numEntities - 1 )->index == -1; clgame.globals->numEntities-- );

	clent = CL_GetLocalPlayer();
	if( !clent ) return;

	// update client vars
	clgame.dllFuncs.pfnTxferLocalOverrides( &clent->curstate, &cl.frame.cd );

	// if viewmodel has changed update sequence here
	if( clgame.viewent.curstate.modelindex != cl.frame.cd.viewmodel )
	{
		cl_entity_t *view = &clgame.viewent;
		CL_WeaponAnim( view->curstate.sequence, view->curstate.body );
	}

	// setup player viewmodel (only for local player!)
	clgame.viewent.curstate.modelindex = cl.frame.cd.viewmodel;

	for( e = 1; e < clgame.globals->numEntities; e++ )
	{
		ent = CL_GetEntityByIndex( e );
		if( !ent ) continue;

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

	if( cl.time > cl.frame.servertime )
	{
		cl.time = cl.frame.servertime;
		cl.lerpFrac = 1.0f;
	}
	else if( cl.time < cl.frame.servertime - cl.serverframetime )
	{
		cl.time = cl.frame.servertime - cl.serverframetime;
		cl.lerpFrac = 0;
	}
	else cl.lerpFrac = 1.0f - ( cl.frame.servertime - cl.time ) * (float)( cl.serverframetime * 0.0001f );

	if( cl.refdef.paused )
		cl.lerpFrac = 1.0f;

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
void CL_GetEntitySpatialization( int entnum, vec3_t origin, vec3_t velocity )
{
	cl_entity_t	*ent;

	ent = CL_GetEntityByIndex( entnum );
	if( !ent ) return; // leave uncahnged

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