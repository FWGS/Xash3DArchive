//=======================================================================
//			Copyright XashXT Group 2010 ©
//		      cl_pmove.c - client-side player physic
//=======================================================================

#include "common.h"
#include "client.h"
#include "const.h"
#include "pm_local.h"

void CL_ClearPhysEnts( void )
{
	clgame.pmove->numtouch = 0;
	clgame.pmove->numvisent = 0;
	clgame.pmove->nummoveent = 0;
	clgame.pmove->numphysent = 0;
}

bool CL_CopyEntityToPhysEnt( physent_t *pe, cl_entity_t *ent )
{
	model_t	*mod = CM_ClipHandleToModel( ent->curstate.modelindex );

	// NOTE: player never collide with sprites (even with solid sprites)
	if( !mod || mod->type == mod_bad || mod->type == mod_sprite )
		return false;
		
	pe->player = ent->player;

	if( pe->player )
	{
		// client or bot
		com.strncpy( pe->name, "player", sizeof( pe->name ));
	}
	else
	{
		// otherwise copy the modelname
		com.strncpy( pe->name, mod->name, sizeof( pe->name ));
	}

	if( mod->type == mod_studio )
	{
		pe->studiomodel = mod;
		pe->model = NULL;
	}
	else
	{	
		pe->studiomodel = NULL;
		pe->model = mod;
	}

	pe->info = NUM_FOR_EDICT( ent );
	VectorCopy( ent->curstate.origin, pe->origin );
	VectorCopy( ent->curstate.angles, pe->angles );
	VectorCopy( ent->curstate.mins, pe->mins );
	VectorCopy( ent->curstate.maxs, pe->maxs );

	pe->solid = ent->curstate.solid;
	pe->rendermode = ent->curstate.rendermode;
	pe->skin = ent->curstate.skin;
	pe->frame = ent->curstate.frame;
	pe->sequence = ent->curstate.sequence;

	Mem_Copy( &pe->controller[0], &ent->curstate.controller[0], 4 * sizeof( byte ));
	Mem_Copy( &pe->blending[0], &ent->curstate.blending[0], 2 * sizeof( byte ));

	pe->movetype = ent->curstate.movetype;
	pe->takedamage = ( pe->player ) ? DAMAGE_AIM : DAMAGE_YES; // FIXME: this right
	pe->team = ent->curstate.team;
	pe->classnumber = ent->curstate.playerclass;
	pe->blooddecal = 0;	// FIXME: what i'm do write here ???

	// for mods
	pe->iuser1 = ent->curstate.iuser1;
	pe->iuser2 = ent->curstate.iuser2;
	pe->iuser3 = ent->curstate.iuser3;
	pe->iuser4 = ent->curstate.iuser4;
	pe->fuser1 = ent->curstate.fuser1;
	pe->fuser2 = ent->curstate.fuser2;
	pe->fuser3 = ent->curstate.fuser3;
	pe->fuser4 = ent->curstate.fuser4;

	VectorCopy( ent->curstate.vuser1, pe->vuser1 );
	VectorCopy( ent->curstate.vuser2, pe->vuser2 );
	VectorCopy( ent->curstate.vuser3, pe->vuser3 );
	VectorCopy( ent->curstate.vuser4, pe->vuser4 );

	return true;
}

/*
====================
CL_AddLinksToPmove

collect solid entities
====================
*/
void CL_AddLinksToPmove( areanode_t *node, const vec3_t pmove_mins, const vec3_t pmove_maxs )
{
	link_t		*l, *next;
	cl_entity_t	*check;
	physent_t		*pe;

	// touch linked edicts
	for( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;
		check = EDICT_FROM_AREA( l );

		// don't add the world and clients here
		if( !check || check == &clgame.entities[0] || check->player )
			continue;

		if(check->curstate.solid == SOLID_BSP || check->curstate.solid == SOLID_BBOX || check->curstate.solid == SOLID_SLIDEBOX)
		{
			if( !BoundsIntersect( pmove_mins, pmove_maxs, check->absmin, check->absmax ))
				continue;

			if( clgame.pmove->numphysent == MAX_PHYSENTS )
				return;

			pe = &clgame.pmove->physents[clgame.pmove->numphysent];

			if( CL_CopyEntityToPhysEnt( pe, check ))
				clgame.pmove->numphysent++;
		}
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;

	if( pmove_maxs[node->axis] > node->dist )
		CL_AddLinksToPmove( node->children[0], pmove_mins, pmove_maxs );
	if( pmove_mins[node->axis] < node->dist )
		CL_AddLinksToPmove( node->children[1], pmove_mins, pmove_maxs );
}

/*
===============
CL_SetSolid

Builds all the pmove physents for the current frame
Note that CL_SetUpPlayerPrediction() must be called first!
pmove must be setup with world and solid entity hulls before calling
(via CL_PredictMove)
===============
*/
void CL_SetSolidPlayers( int playernum )
{
	int		j;
	extern	vec3_t	player_mins;
	extern	vec3_t	player_maxs;
	cl_entity_t	*ent;
	physent_t		*pe;

	if( !cl_solid_players->integer )
		return;

	// FIXME: create predicted_players array
	for( j = 0; j < cl.maxclients; j++ )
	{
		// the player object never gets added
		if( j == playernum ) continue;

		ent = CL_GetEntityByIndex( j + 1 );		

		if( !ent || !ent->player )
			continue; // not present this frame

		pe = &clgame.pmove->physents[clgame.pmove->numphysent];
		if( CL_CopyEntityToPhysEnt( pe, ent ))
			clgame.pmove->numphysent++;
	}
}

static void pfnParticle( float *origin, int color, float life, int zpos, int zvel )
{
	vec3_t	dir;

	if( !origin )
	{
		MsgDev( D_ERROR, "CL_StartParticle: NULL origin. Ignored\n" );
		return;
	}

	// FIXME: send lifetime too

	VectorSet( dir, 0.0f, 0.0f, ( zpos * zvel ));
	clgame.dllFuncs.pfnParticleEffect( origin, dir, color, 1 );
}

static int pfnTestPlayerPosition( float *pos, pmtrace_t *ptrace )
{
	pmtrace_t trace;

	trace = PM_PlayerTrace( clgame.pmove, pos, pos, PM_NORMAL, clgame.pmove->usehull, -1, NULL );
	if( ptrace ) *ptrace = trace; 
	return trace.ent;
}

static double Sys_FloatTime( void )
{
	return Sys_DoubleTime();
}

static void pfnStuckTouch( int hitent, pmtrace_t *ptraceresult )
{
	// empty for now
	// FIXME: write some code
}

static int pfnPointContents( float *p, int *truecontents )
{
	int	cont, truecont;

	truecont = cont = CL_TruePointContents( p );
	if( truecontents ) *truecontents = truecont;

	if( cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN )
		cont = CONTENTS_WATER;
	return cont;
}

static int pfnTruePointContents( float *p )
{
	return CL_TruePointContents( p );
}

static int pfnHullPointContents( struct hull_s *hull, int num, float *p )
{
	return PM_HullPointContents( hull, num, p );
}

static pmtrace_t pfnPlayerTrace( float *start, float *end, int traceFlags, int ignore_pe )
{
	return PM_PlayerTrace( clgame.pmove, start, end, traceFlags, clgame.pmove->usehull, ignore_pe, NULL );
}

static pmtrace_t *pfnTraceLine( float *start, float *end, int flags, int usehull, int ignore_pe )
{
	static pmtrace_t	tr;

	tr = PM_PlayerTrace( clgame.pmove, start, end, flags, usehull, ignore_pe, NULL );
	return &tr;
}

static int pfnGetModelType( model_t *mod )
{
	if( !mod ) return mod_bad;
	return mod->type;
}

static void pfnGetModelBounds( model_t *mod, float *mins, float *maxs )
{
	if( mod )
	{
		if( mins ) VectorCopy( mod->mins, mins );
		if( maxs ) VectorCopy( mod->maxs, maxs );
	}
	else
	{
		MsgDev( D_ERROR, "Mod_GetBounds: NULL model\n" );
		if( mins ) VectorClear( mins );
		if( maxs ) VectorClear( maxs );
	}
}

static hull_t *pfnHullForBsp( physent_t *pe, float *offset )
{
	float *mins = clgame.pmove->player_mins[clgame.pmove->usehull];
	float *maxs = clgame.pmove->player_maxs[clgame.pmove->usehull];
	return PM_HullForBsp( pe, mins, maxs, offset );
}

static float pfnTraceModel( physent_t *pEnt, float *start, float *end, pmtrace_t *trace )
{
	if( PM_TraceModel( pEnt, start, vec3_origin, vec3_origin, end, trace, PM_STUDIO_BOX ))
		return trace->fraction;
	return 1.0f;
}

static const char *pfnTraceTexture( int ground, float *vstart, float *vend )
{
	physent_t *pe;

	if( ground < 0 || ground >= clgame.pmove->numphysent )
		return NULL; // bad ground

	pe = &clgame.pmove->physents[ground];
	return PM_TraceTexture( pe, vstart, vend );
}			

static int pfnCOM_FileSize( const char *filename )
{
	return FS_FileSize( filename );
}

static byte *pfnCOM_LoadFile( const char *path, int usehunk, int *pLength )
{
	return FS_LoadFile( path, pLength );
}

static void pfnCOM_FreeFile( void *buffer )
{
	Mem_Free( buffer );
}

static void pfnPlaySound( int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch )
{
	sound_t	snd = S_RegisterSound( sample );

	S_StartSound( NULL, clgame.pmove->player_index + 1, channel, snd, volume, attenuation, pitch, fFlags );
}

static void pfnPlaybackEventFull( int flags, int clientindex, word eventindex, float delay, float *origin,
	float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	cl_entity_t	*ent;

	ent = CL_GetEntityByIndex( clientindex + 1 );
	if( ent == NULL ) return;

	CL_PlaybackEvent( flags, ent, eventindex,
		delay, origin, angles,
		fparam1, fparam2,
		iparam1, iparam2,
		bparam1, bparam2 );
}

static pmtrace_t pfnPlayerTraceEx( float *start, float *end, int traceFlags, pfnIgnore pmFilter )
{
	return PM_PlayerTrace( clgame.pmove, start, end, traceFlags, clgame.pmove->usehull, -1, pmFilter );
}

static int pfnTestPlayerPositionEx( float *pos, pmtrace_t *ptrace, pfnIgnore pmFilter )
{
	pmtrace_t trace;

	trace = PM_PlayerTrace( clgame.pmove, pos, pos, PM_STUDIO_BOX, clgame.pmove->usehull, -1, pmFilter );
	if( ptrace ) *ptrace = trace; 
	return trace.ent;
}

static pmtrace_t *pfnTraceLineEx( float *start, float *end, int flags, int usehull, pfnIgnore pmFilter )
{
	static pmtrace_t	tr;

	tr = PM_PlayerTrace( clgame.pmove, start, end, flags, usehull, -1, pmFilter );
	return &tr;
}

/*
===============
CL_InitClientMove

===============
*/
void CL_InitClientMove( void )
{
	int	i;

	Pmove_Init ();

	clgame.pmove->server = false;	// running at client
	clgame.pmove->movevars = &clgame.movevars;
	clgame.pmove->runfuncs = false;

	CM_SetupHulls( clgame.player_mins, clgame.player_maxs );

	// enumerate client hulls
	for( i = 0; i < 4; i++ )
		clgame.dllFuncs.pfnGetHullBounds( i, clgame.player_mins[i], clgame.player_maxs[i] );

	Mem_Copy( clgame.pmove->player_mins, clgame.player_mins, sizeof( clgame.player_mins ));
	Mem_Copy( clgame.pmove->player_maxs, clgame.player_maxs, sizeof( clgame.player_maxs ));

	// common utilities
	clgame.pmove->PM_Info_ValueForKey = Info_ValueForKey;
	clgame.pmove->PM_Particle = pfnParticle;
	clgame.pmove->PM_TestPlayerPosition = pfnTestPlayerPosition;
	clgame.pmove->Con_NPrintf = Con_NPrintf;
	clgame.pmove->Con_DPrintf = Con_DPrintf;
	clgame.pmove->Con_Printf = Con_Printf;
	clgame.pmove->Sys_FloatTime = Sys_FloatTime;
	clgame.pmove->PM_StuckTouch = pfnStuckTouch;
	clgame.pmove->PM_PointContents = pfnPointContents;
	clgame.pmove->PM_TruePointContents = pfnTruePointContents;
	clgame.pmove->PM_HullPointContents = pfnHullPointContents; 
	clgame.pmove->PM_PlayerTrace = pfnPlayerTrace;
	clgame.pmove->PM_TraceLine = pfnTraceLine;
	clgame.pmove->RandomLong = pfnRandomLong;
	clgame.pmove->RandomFloat = pfnRandomFloat;
	clgame.pmove->PM_GetModelType = pfnGetModelType;
	clgame.pmove->PM_GetModelBounds = pfnGetModelBounds;	
	clgame.pmove->PM_HullForBsp = pfnHullForBsp;
	clgame.pmove->PM_TraceModel = pfnTraceModel;
	clgame.pmove->COM_FileSize = pfnCOM_FileSize;
	clgame.pmove->COM_LoadFile = pfnCOM_LoadFile;
	clgame.pmove->COM_FreeFile = pfnCOM_FreeFile;
	clgame.pmove->memfgets = pfnMemFgets;
	clgame.pmove->PM_PlaySound = pfnPlaySound;
	clgame.pmove->PM_TraceTexture = pfnTraceTexture;
	clgame.pmove->PM_PlaybackEventFull = pfnPlaybackEventFull;
	clgame.pmove->PM_PlayerTraceEx = pfnPlayerTraceEx;
	clgame.pmove->PM_TestPlayerPositionEx = pfnTestPlayerPositionEx;
	clgame.pmove->PM_TraceLineEx = pfnTraceLineEx;

	// initalize pmove
	clgame.dllFuncs.pfnPM_Init( clgame.pmove );
}

static void PM_CheckMovingGround( clientdata_t *cd, entity_state_t *state, float frametime )
{
	if(!( cd->flags & FL_BASEVELOCITY ))
	{
		// apply momentum (add in half of the previous frame of velocity first)
		VectorMA( cd->velocity, 1.0f + (frametime * 0.5f), state->basevelocity, cd->velocity );
		VectorClear( state->basevelocity );
	}
	cd->flags &= ~FL_BASEVELOCITY;
}

void CL_SetSolidEntities( void )
{
	physent_t		*pe;
	vec3_t		absmin, absmax;
	cl_entity_t	*touch[MAX_EDICTS];
	int		i, count;

	// world not initialized
	if( !cl.frame.valid ) return;

	// setup physents
	clgame.pmove->numvisent = 0; // FIXME: add visents for debugging
	clgame.pmove->numphysent = 0;
	clgame.pmove->nummoveent = 0;

	for( i = 0; i < 3; i++ )
	{
		absmin[i] = cl.frame.clientdata.origin[i] - 1024;
		absmax[i] = cl.frame.clientdata.origin[i] + 1024;
	}

	CL_CopyEntityToPhysEnt( &clgame.pmove->physents[0], &clgame.entities[0] );
	clgame.pmove->numphysent = 1;	// always have world

	CL_AddLinksToPmove( cl_areanodes, absmin, absmax );
	count = CL_AreaEdicts( absmin, absmax, touch, MAX_EDICTS, AREA_CUSTOM );

	// build list of ladders around player
	for( i = 0; i < count; i++ )
	{
		if( clgame.pmove->nummoveent >= MAX_MOVEENTS )
		{
			MsgDev( D_ERROR, "PM_PlayerMove: too many ladders in PVS\n" );
			break;
		}

		if( touch[i] == CL_GetLocalPlayer( )) continue;

		pe = &clgame.pmove->moveents[clgame.pmove->nummoveent];
		if( CL_CopyEntityToPhysEnt( pe, touch[i] ))
			clgame.pmove->nummoveent++;
	}
}

static void PM_SetupMove( playermove_t *pmove, clientdata_t *cd, entity_state_t *state, usercmd_t *ucmd )
{
	pmove->player_index = cl.playernum;
	pmove->multiplayer = (cl.maxclients > 1) ? true : false;
	pmove->time = cl_time(); // probably never used
	VectorCopy( cd->origin, pmove->origin );
	VectorCopy( cl.refdef.cl_viewangles, pmove->angles );
	VectorCopy( cl.refdef.cl_viewangles, pmove->oldangles );
	VectorCopy( cd->velocity, pmove->velocity );
	VectorCopy( state->basevelocity, pmove->basevelocity );
	VectorCopy( cd->view_ofs, pmove->view_ofs );
	VectorClear( pmove->movedir );
	pmove->flDuckTime = cd->flDuckTime;
	pmove->bInDuck = cd->bInDuck;
	pmove->usehull = (cd->flags & FL_DUCKING) ? 1 : 0; // reset hull
	pmove->flTimeStepSound = cd->flTimeStepSound;
	pmove->iStepLeft = state->iStepLeft;
	pmove->flFallVelocity = state->flFallVelocity;
	pmove->flSwimTime = cd->flSwimTime;
	VectorCopy( cd->punchangle, pmove->punchangle );
	pmove->flSwimTime = cd->flSwimTime;
	pmove->flNextPrimaryAttack = 0.0f; // not used by PM_ code
	pmove->effects = state->effects;
	pmove->flags = cd->flags;
	pmove->gravity = state->gravity;
	pmove->friction = state->friction;
	pmove->oldbuttons = state->oldbuttons;
	pmove->waterjumptime = cd->waterjumptime;
	pmove->dead = (cd->health <= 0.0f ) ? true : false;
	pmove->deadflag = cd->deadflag;
	pmove->spectator = 0;	// FIXME: implement
	pmove->movetype = state->movetype;
	pmove->onground = -1; // will be set by PM_ code
	pmove->waterlevel = cd->waterlevel;
	pmove->watertype = cd->watertype;
	pmove->maxspeed = clgame.movevars.maxspeed;
	pmove->clientmaxspeed = cd->maxspeed;
	pmove->iuser1 = cd->iuser1;
	pmove->iuser2 = cd->iuser2;
	pmove->iuser3 = cd->iuser3;
	pmove->iuser4 = cd->iuser4;
	pmove->fuser1 = cd->fuser1;
	pmove->fuser2 = cd->fuser2;
	pmove->fuser3 = cd->fuser3;
	pmove->fuser4 = cd->fuser4;
	VectorCopy( cd->vuser1, pmove->vuser1 );
	VectorCopy( cd->vuser2, pmove->vuser2 );
	VectorCopy( cd->vuser3, pmove->vuser3 );
	VectorCopy( cd->vuser4, pmove->vuser4 );
	pmove->cmd = *ucmd;	// setup current cmds	

	com.strncpy( pmove->physinfo, cd->physinfo, MAX_INFO_STRING );
}

static void PM_FinishMove( playermove_t *pmove, clientdata_t *cd, entity_state_t *state )
{
	cd->waterjumptime = pmove->waterjumptime;
	state->onground = pmove->onground;
}