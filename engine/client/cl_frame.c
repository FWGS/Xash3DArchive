//=======================================================================
//			Copyright XashXT Group 2008 �
//		        cl_frame.c - client world snapshot
//=======================================================================

#include "common.h"
#include "client.h"
#include "protocol.h"
#include "net_encode.h"

/*
=========================================================================

FRAME PARSING

=========================================================================
*/
void CL_UpdateEntityFields( edict_t *ent )
{
	// these fields user can overwrite if need
	ent->v.model = MAKE_STRING( cl.configstrings[CS_MODELS+ent->pvClientData->current.modelindex] );

	clgame.dllFuncs.pfnUpdateEntityVars( ent, &ent->pvClientData->current, &ent->pvClientData->prev );

	if( ent->pvClientData->current.ed_flags & ESF_LINKEDICT )
	{
		CL_LinkEdict( ent, false );
		// to avoids multiple relinks when wait for next packet
		ent->pvClientData->current.ed_flags &= ~ESF_LINKEDICT;
	}

	// always keep an actual (users can't replace this)
	ent->serialnumber = ent->pvClientData->current.number;
	ent->v.classname = cl.edict_classnames[ent->pvClientData->current.classname];
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
	bool		newent = (old) ? false : true;

	ent = EDICT_NUM( newnum );
	state = &cl.entity_curstates[cl.parse_entities & (MAX_PARSE_ENTITIES-1)];

	if( newent ) old = &clgame.baselines[newnum];

	if( unchanged ) *state = *old;
	else MSG_ReadDeltaEntity( msg, old, state, newnum, cl.frame.servertime );

	if( state->number == MAX_EDICTS )
	{
		if( newent ) Host_Error( "Cl_DeltaEntity: tried to release new entity\n" );
		if( !ent->free ) CL_FreeEdict( ent );
		return; // entity was delta removed
	}

	cl.parse_entities++;
	frame->num_entities++;

	if( ent->free ) CL_InitEdict( ent );

	// some data changes will force no lerping
	if( state->ed_flags & ESF_NODELTA ) ent->pvClientData->serverframe = -99;
	if( newent ) state->ed_flags |= ESF_LINKEDICT; // need to relink

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
	edict_t	*clent;
          
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
		edict_t	*player;

		// client entered the game
		cls.state = ca_active;
		cl.force_refdef = true;
		cls.changelevel = false;	// changelevel is done

		player = CL_GetLocalPlayer();
		SCR_MakeLevelShot();	// make levelshot if needs

		Cvar_SetValue( "scr_loading", 0.0f ); // reset progress bar	
		// getting a valid frame message ends the connection process
		VectorCopy( player->pvClientData->current.origin, cl.predicted_origin );
		VectorCopy( player->v.v_angle, cl.predicted_angles );
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
	edict_t		*ent, *clent;
	clientdata_t	*cd, *ocd, dummy;
	int		e, ed_type;

	// now recalc actual entcount
	for( ; EDICT_NUM( clgame.globals->numEntities - 1 )->free; clgame.globals->numEntities-- );

	clent = CL_GetLocalPlayer();
	if( !clent ) return;

	cd = &cl.frame.cd;

	if( cl.oldframe )
	{
		ocd = &cl.oldframe->cd;
	}
	else
	{
		Mem_Set( &dummy, 0, sizeof( dummy ));
		ocd = &dummy;
	}

	// update client vars
	clgame.dllFuncs.pfnUpdateClientVars( clent, cd, ocd );

	for( e = 1; e < clgame.globals->numEntities; e++ )
	{
		ent = CL_GetEdictByIndex( e );
		if( !CL_IsValidEdict( ent )) continue;

		ed_type = ent->pvClientData->current.ed_type;
		CL_UpdateEntityFields( ent );

		if( clgame.dllFuncs.pfnAddVisibleEntity( ent, ed_type ))
		{
			if( ed_type == ED_PORTAL && !VectorCompare( ent->v.origin, ent->v.oldorigin ))
				cl.render_flags |= RDF_PORTALINVIEW;
		}
		// NOTE: skyportal entity never added to rendering
		if( ed_type == ED_SKYPORTAL ) cl.render_flags |= RDF_SKYPORTALINVIEW;
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
	edict_t	*ent;

	ent = CL_GetEdictByIndex( entnum );
	if( !CL_IsValidEdict( ent ))
		return; // leave uncahnged

	// setup origin and velocity
	if( origin ) VectorCopy( ent->v.origin, origin );
	if( velocity ) VectorCopy( ent->v.velocity, velocity );

	// if a brush model, offset the origin
	if( origin && CM_GetModelType( ent->v.modelindex ) == mod_brush )
	{
		vec3_t	mins, maxs, midPoint;
	
		Mod_GetBounds( ent->v.modelindex, mins, maxs );
		VectorAverage( mins, maxs, midPoint );
		VectorAdd( origin, midPoint, origin );
	}
}