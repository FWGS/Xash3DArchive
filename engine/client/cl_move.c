//=======================================================================
//			Copyright XashXT Group 2008 ©
//		   cl_physics.c - client physic and prediction
//=======================================================================

#include "common.h"
#include "client.h"
#include "matrix_lib.h"
#include "const.h"
#include "pm_defs.h"

bool CL_IsPredicted( void )
{
	edict_t	*player = CL_GetLocalPlayer();

	if( !player )
		return false;

	if( !cl.validsequence )
		return false;

	if( cls.netchan.outgoing_sequence - cls.netchan.incoming_sequence >= CL_UPDATE_BACKUP - 1 )
		return false;

	if( player->pvClientData->current.ed_flags & ESF_NO_PREDICTION )
		return false;
	if( !cl_predict->integer )
		return false;
	return true;
}

/*
=================
CL_CreateCmd
=================
*/
usercmd_t CL_CreateCmd( void )
{
	usercmd_t		cmd;
	int		ms;

	// catch windowState for client.dll
	switch( host.state )
	{
	case HOST_INIT:
	case HOST_FRAME:
	case HOST_SHUTDOWN:
	case HOST_ERROR:
		clgame.globals->windowState = true;	// active
		break;
	case HOST_SLEEP:
	case HOST_NOFOCUS:
	case HOST_RESTART:
		clgame.globals->windowState = false;	// inactive
		break;
	}

	// send milliseconds of time to apply the move
	ms = ( cl.time - cl.oldtime ) * 1000;
	if( ms > 250 ) ms = 100;	// time was unreasonable

	Mem_Set( &cmd, 0, sizeof( cmd ));

	clgame.dllFuncs.pfnCreateMove( &cmd, ( cls.state == ca_active && !cl.refdef.paused ));

	// never let client.dll calc frametime for player
	// because is potential backdoor for cheating
	cmd.msec = ms;

	return cmd;
}

/*
===================
CL_WritePacket

Create and send the command packet to the server
Including both the reliable commands and the usercmds

During normal gameplay, a client packet will contain something like:

1 clc_move
<usercmd[-2]>
<usercmd[-1]>
<usercmd[-0]>
===================
*/
void CL_WritePacket( void )
{
	sizebuf_t		buf;
	byte		data[MAX_MSGLEN];
	usercmd_t		*cmd, *oldcmd;
	usercmd_t		nullcmd;
	int		key;

	// don't send anything if playing back a demo
	if( cls.demoplayback || cls.state == ca_cinematic )
		return;

	if( cls.state == ca_disconnected || cls.state == ca_connecting )
		return;

	if( cls.state == ca_connected )
	{
		// just update reliable
		if( cls.netchan.message.cursize )
			Netchan_Transmit( &cls.netchan, 0, NULL );
		return;
	}

	// send a userinfo update if needed
	if( userinfo->modified )
	{
		userinfo->modified = false;
		MSG_WriteByte( &cls.netchan.message, clc_userinfo );
		MSG_WriteString( &cls.netchan.message, Cvar_Userinfo( ));
	}

	MSG_Init( &buf, data, sizeof( data ));

	// begin a client move command
	MSG_WriteByte( &buf, clc_move );

	// save the position for a checksum byte
	key = buf.cursize;
	MSG_WriteByte( &buf, 0 );

	// send this and the previous cmds in the message, so
	// if the last packet was dropped, it can be recovered
	cmd = &cl.cmds[(cls.netchan.outgoing_sequence - 2) & CMD_MASK];
	Mem_Set( &nullcmd, 0, sizeof( nullcmd ));
	MSG_WriteDeltaUsercmd( &buf, &nullcmd, cmd );
	oldcmd = cmd;

	cmd = &cl.cmds[(cls.netchan.outgoing_sequence - 1) & CMD_MASK];
	MSG_WriteDeltaUsercmd( &buf, oldcmd, cmd );
	oldcmd = cmd;

	cmd = &cl.cmds[(cls.netchan.outgoing_sequence - 0) & CMD_MASK];
	MSG_WriteDeltaUsercmd( &buf, oldcmd, cmd );

	// calculate a checksum over the move commands
	buf.data[key] = CRC_Sequence( buf.data + key + 1, buf.cursize - key - 1, cls.netchan.outgoing_sequence );

	// request delta compression of entities
	if( cls.netchan.outgoing_sequence - cl.validsequence >= CL_UPDATE_BACKUP - 1 )
		cl.validsequence = 0;

	if( cl.validsequence && !cl_nodelta->integer && !cls.demowaiting )
	{
		cl.frames[cls.netchan.outgoing_sequence & CL_UPDATE_MASK].delta_sequence = cl.validsequence;
		MSG_WriteByte( &buf, clc_delta );
		MSG_WriteByte( &buf, cl.validsequence & 255 );
	}
	else cl.frames[cls.netchan.outgoing_sequence & CL_UPDATE_MASK].delta_sequence = -1;

	// deliver the message
	Netchan_Transmit( &cls.netchan, buf.cursize, buf.data );
}

/*
=================
CL_SendCmd

Called every frame to builds and sends a command packet to the server.
=================
*/
void CL_SendCmd( void )
{
	if( host.type == HOST_DEDICATED ) return;

	// we create commands even if a demo is playing,
	cl.refdef.cmd = &cl.cmds[cls.netchan.outgoing_sequence & CMD_MASK];
	*cl.refdef.cmd = CL_CreateCmd();

	// clc_move, userinfo etc
	CL_WritePacket();
}

/*
============================================================

PLAYER MOVEMENT CODE

============================================================
*/
// builtins
/*
=================
PM_ClientPrintf

only for himself, not to all
=================
*/
static void PM_ClientPrintf( int index, char *fmt, ... )
{
	va_list		argptr;
	char		string[MAX_SYSPATH];

	if( index != cl.playernum )
		return;
	
	va_start( argptr, fmt );
	com.vsprintf( string, fmt, argptr );
	va_end( argptr );

	Con_Print( string );	
}

/*
===============
PM_PlayerTrace

===============
*/
static TraceResult PM_PlayerTrace( const vec3_t start, const vec3_t end, int trace_type )
{
	float		*mins;
	float		*maxs;
	trace_t		result;
	TraceResult	out;

	if( VectorIsNAN( start ) || VectorIsNAN( end ))
		Host_Error( "PlayerTrace: NAN errors detected ('%f %f %f', '%f %f %f'\n", start[0], start[1], start[2], end[0], end[1], end[2] );

	clgame.pmove->usehull = bound( 0, clgame.pmove->usehull, 3 );
	mins = clgame.pmove->player_mins[clgame.pmove->usehull];
	maxs = clgame.pmove->player_maxs[clgame.pmove->usehull];

	result = CL_Move( start, mins, maxs, end, trace_type|FMOVE_SIMPLEBOX, clgame.pmove->player );
	Mem_Copy( &out, &result, sizeof( TraceResult ));

	return out;
}

/*
===============
PM_TraceTexture

===============
*/
static const char *PM_TraceTexture( edict_t *pTextureEntity, const float *v1, const float *v2 )
{
	trace_t	result;

	if( VectorIsNAN( v1 ) || VectorIsNAN( v2 ))
		Host_Error( "TraceTexture: NAN errors detected ('%f %f %f', '%f %f %f'\n", v1[0], v1[1], v1[2], v2[0], v2[1], v2[2] );

	if( !CL_IsValidEdict( pTextureEntity )) return NULL; 

	result = CM_ClipMove( pTextureEntity, v1, vec3_origin, vec3_origin, v2, 0 );
	return CM_TraceTexture( v1, result );
}

/*
===============
PM_TraceModel

===============
*/
static TraceResult PM_TraceModel( edict_t *pEnt, const vec3_t start, const vec3_t end )
{
	if( VectorIsNAN( start ) || VectorIsNAN( end ))
		Host_Error( "TraceModel: NAN errors detected ('%f %f %f', '%f %f %f'\n", start[0], start[1], start[2], end[0], end[1], end[2] );

	return CM_ClipMove( pEnt, start, vec3_origin, vec3_origin, end, FMOVE_SIMPLEBOX );
}

/*
===============
PM_GetEntityByIndex

safe version of CL_EDICT_NUM
===============
*/
static edict_t *PM_GetEntityByIndex( int index )
{
	if( index < 0 || index > clgame.globals->numEntities )
	{
		if( index == VIEWENT_INDEX ) return clgame.pmove->player->v.aiment; // current weapon
		if( index == NULLENT_INDEX ) return NULL;
		MsgDev( D_ERROR, "PM_GetEntityByIndex: invalid entindex %i\n", index );
		return NULL;
	}

	if( EDICT_NUM( index )->free )
		return NULL;
	return EDICT_NUM( index );
}

static void PM_PlaySound( int chan, const char *sample, float vol, float attn, int pitch )
{
	if( !clgame.pmove->runfuncs ) return; // ignored
	S_StartSound( vec3_origin, cl.playernum + 1, chan, S_RegisterSound( sample ), vol, attn, pitch, 0 );
}

static edict_t *PM_TestPlayerPosition( const vec3_t origin, TraceResult *trace )
{
	return CL_TestPlayerPosition( origin, clgame.pmove->player, trace );
}

static int PM_PointContents( const vec3_t p, int *truecontents )
{
	int cont = CL_TruePointContents( p );

	if( truecontents ) *truecontents = cont;
	if( cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN )
		cont = CONTENTS_WATER;
	return cont;
}

static chull_t *PM_HullForBsp( edict_t *ent, float *offset )
{
	return CM_HullForBsp( ent, clgame.pmove->player->v.mins, clgame.pmove->player->v.maxs, offset );
}

static void PM_CheckMovingGround( edict_t *ent, float frametime )
{
	if(!( ent->v.flags & FL_BASEVELOCITY ))
	{
		// apply momentum (add in half of the previous frame of velocity first)
		VectorMA( ent->v.velocity, 1.0f + (frametime * 0.5f), ent->v.basevelocity, ent->v.velocity );
		VectorClear( ent->v.basevelocity );
	}
	ent->v.flags &= ~FL_BASEVELOCITY;
}

static void PM_SetupMove( playermove_t *pmove, edict_t *clent, usercmd_t *ucmd, const char *physinfo )
{
	edict_t	*hit, *touch[MAX_EDICTS];
	vec3_t	absmin, absmax;
	int	i, count;

	// setup playermove globals
	pmove->multiplayer = (clgame.globals->maxClients > 1) ? true : false;
	pmove->serverflags = clgame.globals->serverflags;	// shared serverflags
	pmove->maxspeed = clgame.movevars.maxspeed;
	pmove->realtime = clgame.globals->time;
	com.strncpy( pmove->physinfo, physinfo, MAX_INFO_STRING );
	pmove->clientmaxspeed = clent->v.maxspeed;
	pmove->bInDuck = (ucmd->buttons & IN_DUCK) ? 1 : 0; // reset hull
	pmove->usehull = (clent->v.flags & FL_DUCKING) ? 1 : 0; // reset hull
	pmove->cmd = *ucmd;				// setup current cmds
	pmove->player = clent;			// ptr to client state

	pmove->numladders = 0;
	VectorCopy( clent->v.absmin, absmin );
	VectorCopy( clent->v.absmax, absmax );

	for( i = 0; i < 3; i++ )
	{
		absmin[i] = clent->v.origin[i] - 256;
		absmax[i] = clent->v.origin[i] + 256;
	}

	count = CL_AreaEdicts( absmin, absmax, touch, MAX_EDICTS, AREA_CUSTOM );

	// build list of ladders around player
	for( i = 0; i < count; i++ )
	{
		if( pmove->numladders >= MAX_LADDERS )
		{
			MsgDev( D_ERROR, "PM_PlayerMove: too many ladders in PVS\n" );
			break;
		}

		hit = touch[i];

		if( hit == clent ) continue;
		if( hit->v.solid != SOLID_NOT || hit->v.skin != CONTENTS_LADDER )
			continue; // not ladder

		// store ladder
		pmove->ladders[pmove->numladders++] = hit;
	}
}

static void PM_FinishMove( playermove_t *pmove, edict_t *clent )
{
	clent->v.teleport_time = pmove->flWaterJumpTime;
	clent->v.groundentity = pmove->onground;
	clent->v.bInDuck = pmove->bInDuck;
}

/*
===============
CL_InitClientMove

===============
*/
void CL_InitClientMove( void )
{
	int	i;

	clgame.pmove->movevars = &clgame.movevars;

	// init hulls
	VectorCopy( GI->client_mins[0], clgame.pmove->player_mins[2] ); // copy point hull
	VectorCopy( GI->client_maxs[0], clgame.pmove->player_maxs[2] );
	VectorCopy( GI->client_mins[1], clgame.pmove->player_mins[0] ); // copy human hull
	VectorCopy( GI->client_maxs[1], clgame.pmove->player_maxs[0] );
	VectorCopy( GI->client_mins[2], clgame.pmove->player_mins[3] ); // copy large hull
	VectorCopy( GI->client_maxs[2], clgame.pmove->player_maxs[3] );
	VectorCopy( GI->client_mins[3], clgame.pmove->player_mins[1] ); // copy head hull
	VectorCopy( GI->client_maxs[3], clgame.pmove->player_maxs[1] );

	for( i = 0; i < PM_MAXHULLS; i++ )
	{
		clgame.pmove->player_view[i] = clgame.globals->viewheight[i] = GI->viewheight[i];
		VectorCopy( clgame.pmove->player_mins[i], clgame.globals->hullmins[i] );
		VectorCopy( clgame.pmove->player_maxs[i], clgame.globals->hullmaxs[i] );
	}

	// common utilities
	clgame.pmove->PM_Info_ValueForKey = Info_ValueForKey;
	clgame.pmove->PM_TestPlayerPosition = PM_TestPlayerPosition;
	clgame.pmove->ClientPrintf = PM_ClientPrintf;
	clgame.pmove->AlertMessage = pfnAlertMessage;
	clgame.pmove->PM_GetString = CL_GetString;
	clgame.pmove->PM_PointContents = PM_PointContents;
	clgame.pmove->PM_HullPointContents = CM_HullPointContents;
	clgame.pmove->PM_HullForBsp = PM_HullForBsp;
	clgame.pmove->PM_PlayerTrace = PM_PlayerTrace;
	clgame.pmove->PM_TraceTexture = PM_TraceTexture;
	clgame.pmove->PM_GetEntityByIndex = PM_GetEntityByIndex;
	clgame.pmove->AngleVectors = AngleVectors;
	clgame.pmove->RandomLong = pfnRandomLong;
	clgame.pmove->RandomFloat = pfnRandomFloat;
	clgame.pmove->PM_GetModelType = CM_GetModelType;
	clgame.pmove->PM_GetModelBounds = Mod_GetBounds;
	clgame.pmove->PM_ModExtradata = Mod_Extradata;
	clgame.pmove->PM_TraceModel = PM_TraceModel;
	clgame.pmove->COM_LoadFile = pfnLoadFile;
	clgame.pmove->COM_ParseToken = pfnParseToken;
	clgame.pmove->COM_FreeFile = pfnFreeFile;
	clgame.pmove->memfgets = pfnMemFgets;
	clgame.pmove->PM_PlaySound = PM_PlaySound;

	// initalize pmove
	clgame.dllFuncs.pfnPM_Init( clgame.pmove );
}

void CL_PreRunCmd( edict_t *clent, usercmd_t *ucmd )
{
	clgame.pmove->runfuncs = false;
	clgame.dllFuncs.pfnCmdStart( clent, clgame.pmove->runfuncs );
}

/*
===========
CL_RunCmd
===========
*/
void CL_RunCmd( edict_t *clent, usercmd_t *ucmd )
{
	if( !CL_IsValidEdict( clent )) return;

	PM_CheckMovingGround( clent, ucmd->msec * 0.001f );

//	VectorCopy( ucmd->viewangles, clgame.pmove->oldangles ); // save oldangles
//	if( !clent->v.fixangle ) VectorCopy( ucmd->viewangles, clent->v.viewangles );

	// copy player buttons
	clent->v.button = ucmd->buttons;
	if( ucmd->impulse ) clent->v.impulse = ucmd->impulse;

	// setup playermove state
	PM_SetupMove( clgame.pmove, clent, ucmd, cl.physinfo );

	// motor!
	clgame.dllFuncs.pfnPM_Move( clgame.pmove, false );

	// copy results back to client
	PM_FinishMove( clgame.pmove, clent );
		
	if(!( clent->v.flags & FL_SPECTATOR ))
	{
		// link into place
		CL_LinkEdict( clent, false );
	}
}

/*
===========
CL_PostRunCmd

Done after running a player command.
===========
*/
void CL_PostRunCmd( edict_t *clent, usercmd_t *ucmd )
{
	if( !clent ) return;
	clgame.pmove->runfuncs = false; // all next calls ignore footstep sounds
	clgame.dllFuncs.pfnCmdEnd( clent, ucmd, ucmd->random_seed );
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
	edict_t		*player;
	float		flen;

	if( !CL_IsPredicted( )) return;

	player = CL_GetLocalPlayer();
	if( !player ) return;

	// calculate the last usercmd_t we sent that the server has processed
	frame = cls.netchan.incoming_acknowledged;
	frame &= CMD_MASK;

	// compare what the server returned with what we had predicted it to be
	VectorSubtract( player->pvClientData->current.origin, cl.predicted_origins[frame], delta );

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
		VectorCopy( player->pvClientData->current.origin, cl.predicted_origins[frame] );

		// save for error itnerpolation
		VectorCopy( delta, cl.prediction_error );
	}
}

/*
=================
CL_PredictMovement

Sets cl.predicted_origin and cl.predicted_angles
=================
*/
void CL_PredictMovement( void )
{
	int		frame;
	int		ack, current;
	edict_t		*player, *viewent;
	usercmd_t		*cmd;

	if( cls.state != ca_active ) return;
	if( cl.refdef.paused || cls.key_dest == key_menu ) return;

	player = CL_GetLocalPlayer ();
	viewent = CL_GetEdictByIndex( cl.refdef.viewentity );

	if( cls.demoplayback && CL_IsValidEdict( viewent ))
	{
		// use interpolated server values
		VectorCopy( viewent->v.viewangles, cl.refdef.cl_viewangles );
	}

	// unpredicted pure angled values converted into axis
	AngleVectors( cl.refdef.cl_viewangles, cl.refdef.forward, cl.refdef.right, cl.refdef.up );

	if( !CL_IsPredicted( ))
	{	
		cmd = cl.refdef.cmd; // use current command

		// run commands even if client predicting is disabled - client expected it
		CL_PreRunCmd( player, cmd );

		VectorCopy( cl.refdef.cl_viewangles, cl.predicted_angles );
		VectorCopy( player->v.view_ofs, cl.predicted_viewofs );

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

	// setup initial pmove state
	VectorCopy( player->v.movedir, clgame.pmove->movedir );
	VectorCopy( player->pvClientData->current.origin, clgame.pmove->origin );
	VectorCopy( player->pvClientData->current.velocity, clgame.pmove->velocity );
	VectorCopy( player->pvClientData->current.basevelocity, clgame.pmove->basevelocity );
	VectorCopy( player->pvClientData->current.viewoffset, player->v.view_ofs );
	clgame.pmove->flWaterJumpTime = player->v.teleport_time;
	clgame.pmove->onground = player->v.groundentity;
	clgame.pmove->usehull = (player->pvClientData->current.flags & FL_DUCKING) ? 1 : 0; // reset hull
	
	// run frames
	while( ++ack < current )
	{
		frame = ack & CMD_MASK;
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
}
