//=======================================================================
//			Copyright XashXT Group 2010 ©
//		      sv_pmove.c - server-side player physic
//=======================================================================

#include "common.h"
#include "server.h"
#include "const.h"
#include "pm_local.h"

qboolean SV_CopyEdictToPhysEnt( physent_t *pe, edict_t *ed, qboolean player_trace )
{
	model_t	*mod = CM_ClipHandleToModel( ed->v.modelindex );

	// NOTE: player never collide with sprites (even with solid sprites)
	if( !mod || mod->type == mod_bad || mod->type == mod_sprite )
		return false;

	// this is monsterclip brush, player ignore it
	if( player_trace && mod->type == mod_brush && ed->v.flags & FL_MONSTERCLIP )
		return false;

	pe->player = false;

	if( ed->v.flags & ( FL_CLIENT|FL_FAKECLIENT ))
	{
		// client or bot
		com.strncpy( pe->name, "player", sizeof( pe->name ));
		pe->player = NUM_FOR_EDICT( ed );
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

	pe->info = NUM_FOR_EDICT( ed );
	VectorCopy( ed->v.origin, pe->origin );
	VectorCopy( ed->v.angles, pe->angles );
	VectorCopy( ed->v.mins, pe->mins );
	VectorCopy( ed->v.maxs, pe->maxs );

	pe->solid = ed->v.solid;
	pe->rendermode = ed->v.rendermode;
	pe->skin = ed->v.skin;
	pe->frame = ed->v.frame;
	pe->sequence = ed->v.sequence;

	Mem_Copy( &pe->controller[0], &ed->v.controller[0], 4 * sizeof( byte ));
	Mem_Copy( &pe->blending[0], &ed->v.blending[0], 2 * sizeof( byte ));

	pe->movetype = ed->v.movetype;
	pe->takedamage = ed->v.takedamage;
	pe->team = ed->v.team;
	pe->classnumber = ed->v.playerclass;
	pe->blooddecal = 0;	// FIXME: what i'm do write here ???

	// for mods
	pe->iuser1 = ed->v.iuser1;
	pe->iuser2 = ed->v.iuser2;
	pe->iuser3 = ed->v.iuser3;
	pe->iuser4 = ed->v.iuser4;
	pe->fuser1 = ed->v.fuser1;
	pe->fuser2 = ed->v.fuser2;
	pe->fuser3 = ed->v.fuser3;
	pe->fuser4 = ed->v.fuser4;

	VectorCopy( ed->v.vuser1, pe->vuser1 );
	VectorCopy( ed->v.vuser2, pe->vuser2 );
	VectorCopy( ed->v.vuser3, pe->vuser3 );
	VectorCopy( ed->v.vuser4, pe->vuser4 );

	return true;
}

/*
====================
SV_AddLinksToPmove

collect solid entities
====================
*/
void SV_AddLinksToPmove( areanode_t *node, const vec3_t pmove_mins, const vec3_t pmove_maxs )
{
	link_t	*l, *next;
	edict_t	*check, *pl;
	physent_t	*pe;

	pl = EDICT_NUM( svgame.pmove->player_index + 1 );
	ASSERT( SV_IsValidEdict( pl ));

	// touch linked edicts
	for( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;
		check = EDICT_FROM_AREA( l );

		if( check == pl || check->v.owner == pl )
			continue; // player or player's own missile

		if( check->v.deadflag == DEAD_DEAD )
			continue;
		
		if( check->v.solid == SOLID_BSP || check->v.solid == SOLID_BBOX || check->v.solid == SOLID_SLIDEBOX )
		{
			if( !BoundsIntersect( pmove_mins, pmove_maxs, check->v.absmin, check->v.absmax ))
				continue;

			if( svgame.pmove->numphysent == MAX_PHYSENTS )
				return;

			pe = &svgame.pmove->physents[svgame.pmove->numphysent];

			if( SV_CopyEdictToPhysEnt( pe, check, true ))
				svgame.pmove->numphysent++;
		}
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;

	if( pmove_maxs[node->axis] > node->dist )
		SV_AddLinksToPmove( node->children[0], pmove_mins, pmove_maxs );
	if( pmove_mins[node->axis] < node->dist )
		SV_AddLinksToPmove( node->children[1], pmove_mins, pmove_maxs );
}

/*
====================
SV_AddLaddersToPmove
====================
*/
void SV_AddLaddersToPmove( areanode_t *node, const vec3_t pmove_mins, const vec3_t pmove_maxs )
{
	link_t	*l, *next;
	edict_t	*check;
	physent_t	*pe;
	
	// get water edicts
	for( l = node->water_edicts.next; l != &node->water_edicts; l = next )
	{
		next = l->next;
		check = EDICT_FROM_AREA( l );

		if( check->v.solid != SOLID_NOT ) // disabled ?
			continue;

		// only brushes can have special contents
		if( Mod_GetType( check->v.modelindex ) != mod_brush )
			continue;

		if( !BoundsIntersect( pmove_mins, pmove_maxs, check->v.absmin, check->v.absmax ))
			continue;

		if( svgame.pmove->nummoveent == MAX_MOVEENTS )
			return;

		pe = &svgame.pmove->moveents[svgame.pmove->nummoveent];

		if( SV_CopyEdictToPhysEnt( pe, check, true ))
			svgame.pmove->nummoveent++;
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;
	
	if( pmove_maxs[node->axis] > node->dist )
		SV_AddLaddersToPmove( node->children[0], pmove_mins, pmove_maxs );
	if( pmove_mins[node->axis] < node->dist )
		SV_AddLaddersToPmove( node->children[1], pmove_mins, pmove_maxs );
}

static void pfnParticle( float *origin, int color, float life, int zpos, int zvel )
{
	int	v;

	if( !origin )
	{
		MsgDev( D_ERROR, "SV_StartParticle: NULL origin. Ignored\n" );
		return;
	}

	BF_WriteByte( &sv.reliable_datagram, svc_particle );
	BF_WriteBitVec3Coord( &sv.reliable_datagram, origin );
	BF_WriteChar( &sv.reliable_datagram, 0 ); // no x-vel
	BF_WriteChar( &sv.reliable_datagram, 0 ); // no y-vel
	v = bound( -128, (zpos * zvel) * 16, 127 );
	BF_WriteChar( &sv.reliable_datagram, v ); // write z-vel

	// FIXME: send lifetime too

	BF_WriteByte( &sv.reliable_datagram, 1 );
	BF_WriteByte( &sv.reliable_datagram, color );
}

static int pfnTestPlayerPosition( float *pos, pmtrace_t *ptrace )
{
	pmtrace_t trace;

	trace = PM_PlayerTrace( svgame.pmove, pos, pos, PM_NORMAL, svgame.pmove->usehull, -1, NULL );
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

	truecont = cont = SV_TruePointContents( p );
	if( truecontents ) *truecontents = truecont;

	if( cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN )
		cont = CONTENTS_WATER;
	return cont;
}

static int pfnTruePointContents( float *p )
{
	return SV_TruePointContents( p );
}

static int pfnHullPointContents( struct hull_s *hull, int num, float *p )
{
	return PM_HullPointContents( hull, num, p );
}

static pmtrace_t pfnPlayerTrace( float *start, float *end, int traceFlags, int ignore_pe )
{
#if 1
	return PM_PlayerTrace( svgame.pmove, start, end, traceFlags, svgame.pmove->usehull, ignore_pe, NULL );
#else
	float		*mins;
	float		*maxs;
	trace_t		result;
	edict_t		*clent = EDICT_NUM( svgame.pmove->player_index + 1 );
	pmtrace_t		out;
	int		i;

	if( VectorIsNAN( start ) || VectorIsNAN( end ))
		Host_Error( "PlayerTrace: NAN errors detected ('%f %f %f', '%f %f %f'\n", start[0], start[1], start[2], end[0], end[1], end[2] );

	svgame.pmove->usehull = bound( 0, svgame.pmove->usehull, 3 );
	mins = svgame.pmove->player_mins[svgame.pmove->usehull];
	maxs = svgame.pmove->player_maxs[svgame.pmove->usehull];

	result = SV_Move( start, mins, maxs, end, MOVE_NORMAL, clent );

	VectorCopy( result.vecEndPos, out.endpos );
	VectorCopy( result.vecPlaneNormal, out.plane.normal );
	out.plane.dist = result.flPlaneDist;
	out.allsolid = result.fAllSolid;
	out.startsolid = result.fStartSolid;
	out.hitgroup = result.iHitgroup;
	out.inopen = result.fInOpen;
	out.inwater = result.fInWater;
	out.fraction = result.flFraction;
	out.ent = -1;

	for( i = 0; result.pHit != NULL && i < svgame.pmove->numphysent; i++ )
	{
		if( svgame.pmove->physents[i].info == result.pHit->serialnumber )
		{
			out.ent = i;
			break;
		}
	}

	return out;
#endif
}

static pmtrace_t *pfnTraceLine( float *start, float *end, int flags, int usehull, int ignore_pe )
{
	static pmtrace_t	tr;

	tr = PM_PlayerTrace( svgame.pmove, start, end, flags, usehull, ignore_pe, NULL );
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
	float *mins = svgame.pmove->player_mins[svgame.pmove->usehull];
	float *maxs = svgame.pmove->player_maxs[svgame.pmove->usehull];
	return PM_HullForBsp( pe, mins, maxs, offset );
}

static float pfnTraceModel( physent_t *pEnt, float *start, float *end, trace_t *trace )
{
	pmtrace_t	pmtrace;

	PM_TraceModel( pEnt, start, vec3_origin, vec3_origin, end, &pmtrace, PM_STUDIO_BOX );

	// copy pmtrace_t to trace_t
	if( trace )
	{
		// NOTE: if pmtrace.h is changed is must be changed too
		Mem_Copy( trace, &pmtrace, sizeof( *trace ));
		trace->hitgroup = pmtrace.hitgroup;
		trace->ent = NULL;
	}

	return pmtrace.fraction;
}

static const char *pfnTraceTexture( int ground, float *vstart, float *vend )
{
	physent_t *pe;

	if( ground < 0 || ground >= svgame.pmove->numphysent )
		return NULL; // bad ground

	pe = &svgame.pmove->physents[ground];
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
	edict_t	*ent;

	ent = EDICT_NUM( svgame.pmove->player_index + 1 );
	if( !SV_IsValidEdict( ent )) return;

	SV_StartSound( ent, channel, sample, volume, attenuation, fFlags, pitch );
}

static void pfnPlaybackEventFull( int flags, int clientindex, word eventindex, float delay, float *origin,
	float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	edict_t	*ent;

	ent = EDICT_NUM( clientindex + 1 );
	if( !SV_IsValidEdict( ent )) return;

	SV_PlaybackEventFull( flags, ent, eventindex,
		delay, origin, angles,
		fparam1, fparam2,
		iparam1, iparam2,
		bparam1, bparam2 );
}

static pmtrace_t pfnPlayerTraceEx( float *start, float *end, int traceFlags, pfnIgnore pmFilter )
{
	return PM_PlayerTrace( svgame.pmove, start, end, traceFlags, svgame.pmove->usehull, -1, pmFilter );
}

static int pfnTestPlayerPositionEx( float *pos, pmtrace_t *ptrace, pfnIgnore pmFilter )
{
	pmtrace_t trace;

	trace = PM_PlayerTrace( svgame.pmove, pos, pos, PM_STUDIO_BOX, svgame.pmove->usehull, -1, pmFilter );
	if( ptrace ) *ptrace = trace; 
	return trace.ent;
}

static pmtrace_t *pfnTraceLineEx( float *start, float *end, int flags, int usehull, pfnIgnore pmFilter )
{
	static pmtrace_t	tr;

	tr = PM_PlayerTrace( svgame.pmove, start, end, flags, usehull, -1, pmFilter );
	return &tr;
}

/*
===============
SV_InitClientMove

===============
*/
void SV_InitClientMove( void )
{
	int	i;

	Pmove_Init ();

	svgame.pmove->server = true;
	svgame.pmove->movevars = &svgame.movevars;
	svgame.pmove->runfuncs = false;

	Mod_SetupHulls( svgame.player_mins, svgame.player_maxs );
	
	// enumerate client hulls
	for( i = 0; i < 4; i++ )
		svgame.dllFuncs.pfnGetHullBounds( i, svgame.player_mins[i], svgame.player_maxs[i] );

	Mem_Copy( svgame.pmove->player_mins, svgame.player_mins, sizeof( svgame.player_mins ));
	Mem_Copy( svgame.pmove->player_maxs, svgame.player_maxs, sizeof( svgame.player_maxs ));

	// common utilities
	svgame.pmove->PM_Info_ValueForKey = Info_ValueForKey;
	svgame.pmove->PM_Particle = pfnParticle;
	svgame.pmove->PM_TestPlayerPosition = pfnTestPlayerPosition;
	svgame.pmove->Con_NPrintf = Con_NPrintf;
	svgame.pmove->Con_DPrintf = Con_DPrintf;
	svgame.pmove->Con_Printf = Con_Printf;
	svgame.pmove->Sys_FloatTime = Sys_FloatTime;
	svgame.pmove->PM_StuckTouch = pfnStuckTouch;
	svgame.pmove->PM_PointContents = pfnPointContents;
	svgame.pmove->PM_TruePointContents = pfnTruePointContents;
	svgame.pmove->PM_HullPointContents = pfnHullPointContents; 
	svgame.pmove->PM_PlayerTrace = pfnPlayerTrace;
	svgame.pmove->PM_TraceLine = pfnTraceLine;
	svgame.pmove->RandomLong = pfnRandomLong;
	svgame.pmove->RandomFloat = pfnRandomFloat;
	svgame.pmove->PM_GetModelType = pfnGetModelType;
	svgame.pmove->PM_GetModelBounds = pfnGetModelBounds;	
	svgame.pmove->PM_HullForBsp = pfnHullForBsp;
	svgame.pmove->PM_TraceModel = pfnTraceModel;
	svgame.pmove->COM_FileSize = pfnCOM_FileSize;
	svgame.pmove->COM_LoadFile = pfnCOM_LoadFile;
	svgame.pmove->COM_FreeFile = pfnCOM_FreeFile;
	svgame.pmove->memfgets = pfnMemFgets;
	svgame.pmove->PM_PlaySound = pfnPlaySound;
	svgame.pmove->PM_TraceTexture = pfnTraceTexture;
	svgame.pmove->PM_PlaybackEventFull = pfnPlaybackEventFull;
	svgame.pmove->PM_PlayerTraceEx = pfnPlayerTraceEx;
	svgame.pmove->PM_TestPlayerPositionEx = pfnTestPlayerPositionEx;
	svgame.pmove->PM_TraceLineEx = pfnTraceLineEx;

	// initalize pmove
	svgame.dllFuncs.pfnPM_Init( svgame.pmove );
}

static void PM_CheckMovingGround( edict_t *ent, float frametime )
{
	SV_UpdateBaseVelocity( ent );

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
	vec3_t	absmin, absmax;
	int	i;

	pmove->player_index = NUM_FOR_EDICT( clent ) - 1;
	pmove->multiplayer = (sv_maxclients->integer > 1) ? true : false;
	pmove->time = sv_time(); // probably never used
	VectorCopy( clent->v.origin, pmove->origin );
	VectorCopy( clent->v.v_angle, pmove->angles );
	VectorCopy( clent->v.v_angle, pmove->oldangles );
	VectorCopy( clent->v.velocity, pmove->velocity );
	VectorCopy( clent->v.basevelocity, pmove->basevelocity );
	VectorCopy( clent->v.view_ofs, pmove->view_ofs );
	VectorClear( pmove->movedir );
	pmove->flDuckTime = clent->v.flDuckTime;
	pmove->bInDuck = clent->v.bInDuck;
	pmove->usehull = (clent->v.flags & FL_DUCKING) ? 1 : 0; // reset hull
	pmove->flTimeStepSound = clent->v.flTimeStepSound;
	pmove->iStepLeft = clent->v.iStepLeft;
	pmove->flFallVelocity = clent->v.flFallVelocity;
	pmove->flSwimTime = clent->v.flSwimTime;
	VectorCopy( clent->v.punchangle, pmove->punchangle );
	pmove->flSwimTime = clent->v.flSwimTime;
	pmove->flNextPrimaryAttack = 0.0f; // not used by PM_ code
	pmove->effects = clent->v.effects;
	pmove->flags = clent->v.flags;
	pmove->gravity = clent->v.gravity;
	pmove->friction = clent->v.friction;
	pmove->oldbuttons = clent->v.oldbuttons;
	pmove->waterjumptime = clent->v.teleport_time;
	pmove->dead = (clent->v.health <= 0.0f ) ? true : false;
	pmove->deadflag = clent->v.deadflag;
	pmove->spectator = 0;	// FIXME: implement
	pmove->movetype = clent->v.movetype;
	pmove->onground = -1; // will be set later
	pmove->waterlevel = clent->v.waterlevel;
	pmove->watertype = clent->v.watertype;
	pmove->maxspeed = svgame.movevars.maxspeed;
	pmove->clientmaxspeed = clent->v.maxspeed;
	pmove->iuser1 = clent->v.iuser1;
	pmove->iuser2 = clent->v.iuser2;
	pmove->iuser3 = clent->v.iuser3;
	pmove->iuser4 = clent->v.iuser4;
	pmove->fuser1 = clent->v.fuser1;
	pmove->fuser2 = clent->v.fuser2;
	pmove->fuser3 = clent->v.fuser3;
	pmove->fuser4 = clent->v.fuser4;
	VectorCopy( clent->v.vuser1, pmove->vuser1 );
	VectorCopy( clent->v.vuser2, pmove->vuser2 );
	VectorCopy( clent->v.vuser3, pmove->vuser3 );
	VectorCopy( clent->v.vuser4, pmove->vuser4 );
	pmove->cmd = *ucmd;	// setup current cmds	

	com.strncpy( pmove->physinfo, physinfo, MAX_INFO_STRING );

	// setup physents
	pmove->numvisent = 0; // FIXME: add visents for debugging
	pmove->numphysent = 0;
	pmove->nummoveent = 0;

	for( i = 0; i < 3; i++ )
	{
		absmin[i] = clent->v.origin[i] - 256;
		absmax[i] = clent->v.origin[i] + 256;
	}

	SV_CopyEdictToPhysEnt( &svgame.pmove->physents[0], &svgame.edicts[0], true );
	svgame.pmove->numphysent = 1;	// always have world

	SV_AddLinksToPmove( sv_areanodes, absmin, absmax );
	SV_AddLaddersToPmove( sv_areanodes, absmin, absmax );

	// setup ground entity
	for( i = 0; clent->v.groundentity && i < svgame.pmove->numphysent; i++ )
	{
		if( NUM_FOR_EDICT( clent->v.groundentity ) == svgame.pmove->physents[i].info )
		{
			pmove->onground = i;
			break;
		}
	}
}

static void PM_FinishMove( playermove_t *pmove, edict_t *clent )
{
	clent->v.teleport_time = pmove->waterjumptime;
	VectorCopy( pmove->angles, clent->v.v_angle );
	VectorCopy( pmove->origin, clent->v.origin );
	VectorCopy( pmove->view_ofs, clent->v.view_ofs );
	VectorCopy( pmove->velocity, clent->v.velocity );
	VectorCopy( pmove->basevelocity, clent->v.basevelocity );
	VectorCopy( pmove->punchangle, clent->v.punchangle );
	clent->v.flFallVelocity = pmove->flFallVelocity;
	clent->v.oldbuttons = pmove->oldbuttons;
	clent->v.waterlevel = pmove->waterlevel;
	clent->v.watertype = pmove->watertype;
	clent->v.flTimeStepSound = pmove->flTimeStepSound;
	clent->v.flDuckTime = pmove->flDuckTime;
	clent->v.flSwimTime = pmove->flSwimTime;
	clent->v.iStepLeft = pmove->iStepLeft;
	clent->v.movetype = pmove->movetype;
	clent->v.friction = pmove->friction;
	clent->v.bInDuck = pmove->bInDuck;
	clent->v.gravity = pmove->gravity;
	clent->v.flags = pmove->flags;

	// copy back user variables
	clent->v.iuser1 = pmove->iuser1;
	clent->v.iuser2 = pmove->iuser2;
	clent->v.iuser3 = pmove->iuser3;
	clent->v.iuser4 = pmove->iuser4;
	clent->v.fuser1 = pmove->fuser1;
	clent->v.fuser2 = pmove->fuser2;
	clent->v.fuser3 = pmove->fuser3;
	clent->v.fuser4 = pmove->fuser4;
	VectorCopy( pmove->vuser1, clent->v.vuser1 );
	VectorCopy( pmove->vuser2, clent->v.vuser2 );
	VectorCopy( pmove->vuser3, clent->v.vuser3 );
	VectorCopy( pmove->vuser4, clent->v.vuser4 );

	if( pmove->onground >= 0 && pmove->onground < pmove->numphysent )
		clent->v.groundentity = EDICT_NUM( pmove->physents[pmove->onground].info );
	else clent->v.groundentity = NULL;
}

int nofind = 0;

entity_state_t *SV_FindEntInPack( int index, client_frame_t *frame )
{
	entity_state_t	*state;
	int		i;	

	for( i = 0; i < frame->num_entities; i++ )
	{
		state = &svs.packet_entities[(frame->first_entity+i)%svs.num_client_entities];

		if( state->number == index )
			return state;
	}
	return NULL;
}

int SV_UnlagCheckTeleport( vec3_t old_pos, vec3_t new_pos )
{
	int	i;

	for( i = 0; i < 3; i++ )
	{
		if( fabs( old_pos[i] - new_pos[i] ) > 50 )
			return 1;
	}
	return 0;
}

void SV_SetupMoveInterpolant( sv_client_t *cl )
{
	int		i, j, var_C_entindex;
	float		finalpush, lerp_msec, latency, temp, lerpFrac;
	client_frame_t	*frame, *var_24;
	entity_state_t	*state, *var_38_FoundEntity;
	sv_client_t	*check;
	sv_interp_t	*lerp;
	vec3_t		tempvec, tempvec2;

	Mem_Set( svgame.interp, 0, sizeof( svgame.interp ));
	nofind = 1;

	// don't allow unlag in singleplayer
	if( sv_maxclients->integer <= 1 ) return;

	// unlag disabled by game request
	if( !svgame.dllFuncs.pfnAllowLagCompensation() || !sv_unlag->integer )
		return;

	// unlag disabled for current client
	if( cl->state != cs_spawned || !cl->lag_compensation )
		return;

	nofind = 0;

	for( i = 0, check = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( check->state != cs_spawned || check == cl )
			continue;

		lerp = &svgame.interp[i];

		VectorCopy( check->edict->v.origin, lerp->oldpos );
		VectorCopy( check->edict->v.absmin, lerp->mins );
		VectorCopy( check->edict->v.absmax, lerp->maxs );
		lerp->active = true;
	}

	if( cl->latency > 1.5f )
		latency = 1.5f;
	else latency = cl->latency;

	temp = sv_maxunlag->value;

	if( temp > 0 && latency > temp )
		latency = temp;

	lerp_msec = cl->lastcmd.lerp_msec * 0.001f;
	if( lerp_msec > 0.1f ) lerp_msec = 0.1f;

	if( lerp_msec < cl->cl_updaterate )
		lerp_msec = cl->cl_updaterate;

	finalpush = sv_unlagpush->value + (( host.realtime - latency ) - lerp_msec );

	if( finalpush > host.realtime )
		finalpush = host.realtime;

	frame = NULL;

	for( var_24 = NULL, i = 0; i < SV_UPDATE_BACKUP; i++, var_24 = frame )
	{
		frame = &cl->frames[(cl->netchan.outgoing_sequence - 1) & SV_UPDATE_MASK];

		for( j = 0; j < frame->num_entities; j++ )
		{
			state = &svs.packet_entities[(frame->first_entity+j)%svs.num_client_entities];

			if( state->number <= 0 || state->number >= sv_maxclients->integer )
				continue;

			// you know, jge doesn't sound right given how the indexing normally works.

			lerp = &svgame.interp[state->number-1];

			if( lerp->nointerp )
				continue;

			if( state->health <= 0 || ( state->effects & EF_NOINTERP ))
				lerp->nointerp = true;

			if( !lerp->firstframe )
				lerp->firstframe = true;
			else if( SV_UnlagCheckTeleport( state->origin, lerp->finalpos ))
				lerp->nointerp = true;

			VectorCopy( state->origin, lerp->finalpos );
		}

		if( finalpush > frame->senttime )
			break;
	}


	if( i == SV_UPDATE_BACKUP || finalpush - frame->senttime > 1.0 )
	{
		Mem_Set( svgame.interp, 0, sizeof( svgame.interp ));
		nofind = 1;
		return;
	}

	if( var_24 == NULL )
	{
		var_24 = frame;
		lerpFrac = 0;
	}
	else
	{
		if( var_24->senttime - frame->senttime == 0.0 )
		{
			lerpFrac = 0;
		}
		else
		{
			lerpFrac = (finalpush - frame->senttime) / (var_24->senttime - frame->senttime);
			lerpFrac = bound( 0.0f, lerpFrac, 1.0f );
		}
	}

	for( i = 0; i < frame->num_entities; i++ )
	{
		state = &svs.packet_entities[(frame->first_entity+i)%svs.num_client_entities];

		if( state->number <= 0 || state->number >= sv_maxclients->integer )
			continue;

		var_C_entindex = state->number - 1;
		check = &svs.clients[var_C_entindex];

		if( check->state != cs_spawned || check == cl )
			continue;

		lerp = &svgame.interp[var_C_entindex];

		if( !lerp->active || lerp->nointerp )
			continue;

		var_38_FoundEntity = SV_FindEntInPack( state->number, var_24 );

		if( var_38_FoundEntity == NULL )
		{
			tempvec[0] = state->origin[0];
			tempvec[1] = state->origin[1];
			tempvec[2] = state->origin[2];
		}
		else
		{
			tempvec2[0] = var_38_FoundEntity->origin[0] - state->origin[0];
			tempvec2[1] = var_38_FoundEntity->origin[1] - state->origin[1];
			tempvec2[2] = var_38_FoundEntity->origin[2] - state->origin[2];

			VectorMA( state->origin, lerpFrac, tempvec2, tempvec );
		}

		VectorCopy( tempvec, lerp->curpos );
		VectorCopy( tempvec, lerp->newpos );

		if( !VectorCompare( tempvec, check->edict->v.origin ))
		{
			VectorCopy( tempvec, check->edict->v.origin );
			SV_LinkEdict( check->edict, false );
			lerp->moving = true;
		}
	}
}

void SV_PreRunCmd( sv_client_t *cl, usercmd_t *ucmd, int random_seed )
{
	svgame.pmove->runfuncs = true; // FIXME: check cl_lc ?
	svgame.dllFuncs.pfnCmdStart( cl->edict, ucmd, random_seed );
}

/*
===========
SV_RunCmd
===========
*/
void SV_RunCmd( sv_client_t *cl, usercmd_t *ucmd, int random_seed )
{
	edict_t	*clent;
	vec3_t	oldvel;
	usercmd_t cmd;
	int	oldmsec;
   
	clent = cl->edict;

	if( cl->next_movetime > host.realtime )
	{
		cl->last_movetime += ( ucmd->msec * 0.001f );
		return;
	}

	cl->next_movetime = 0.0;

	// chop up very long commands
	if( ucmd->msec > 50 )
	{
		cmd = *ucmd;
		oldmsec = ucmd->msec;
		cmd.msec = oldmsec / 2;

		SV_RunCmd( cl, &cmd, random_seed );

		cmd.msec = oldmsec / 2 + (oldmsec & 1);	// give them back thier msec.
		cmd.impulse = 0;
		SV_RunCmd( cl, &cmd, random_seed );
		return;
	}


	PM_CheckMovingGround( clent, ucmd->msec * 0.001f );

	VectorCopy( clent->v.v_angle, svgame.pmove->oldangles ); // save oldangles
	if( !clent->v.fixangle ) VectorCopy( ucmd->viewangles, clent->v.v_angle );

	// copy player buttons
	clent->v.button = ucmd->buttons;
	if( ucmd->impulse ) clent->v.impulse = ucmd->impulse;

	// angles
	// show 1/3 the pitch angle and all the roll angle	
	if( clent->v.deadflag < DEAD_DEAD )
	{
		if( !clent->v.fixangle )
		{
			clent->v.angles[PITCH] = -clent->v.v_angle[PITCH] / 3;
			clent->v.angles[YAW] = clent->v.v_angle[YAW];
		}
	}

	if( clent->v.flags & FL_DUCKING ) 
		SV_SetMinMaxSize( clent, svgame.pmove->player_mins[1], svgame.pmove->player_maxs[1] );
	else SV_SetMinMaxSize( clent, svgame.pmove->player_mins[0], svgame.pmove->player_maxs[0] );

	if(!( clent->v.flags & FL_SPECTATOR ))
	{
		svgame.dllFuncs.pfnPlayerPreThink( clent );
		SV_RunThink( clent ); // clients cannot be deleted from map

		// If conveyor, or think, set basevelocity, then send to client asap too.
		if( VectorLength( clent->v.basevelocity ) > 0.0f )
			VectorCopy( clent->v.basevelocity, clent->v.clbasevelocity );
	}

	if(( sv_maxclients->integer <= 1 ) && !CL_IsInGame( ) || ( clent->v.flags & FL_FROZEN ))
		ucmd->msec = 0; // pause

	// setup playermove state
	PM_SetupMove( svgame.pmove, clent, ucmd, cl->physinfo );

	// motor!
	svgame.dllFuncs.pfnPM_Move( svgame.pmove, true );

	// copy results back to client
	PM_FinishMove( svgame.pmove, clent );

	if(!( clent->v.flags & FL_SPECTATOR ))
	{
		int	i;
		edict_t	*touch;
			
		// link into place and touch triggers
		SV_LinkEdict( clent, true );
		VectorCopy( clent->v.velocity, oldvel ); // save velocity

		// touch other objects
		for( i = 0; i < svgame.pmove->numtouch; i++ )
		{
			if( i == MAX_PHYSENTS ) break;
			touch = EDICT_NUM( svgame.pmove->physents[svgame.pmove->touchindex[i].ent].info );
			if( touch == clent ) continue;

			VectorCopy( svgame.pmove->touchindex[i].deltavelocity, clent->v.velocity );
			svgame.dllFuncs.pfnTouch( touch, clent );
		}

		// restore velocity
		VectorCopy( oldvel, clent->v.velocity );
	}

	svgame.pmove->numtouch = 0;
}

/*
===========
SV_PostRunCmd

Done after running a player command.
===========
*/
void SV_PostRunCmd( sv_client_t *cl )
{
	edict_t	*clent;

	clent = cl->edict;
	if( !clent || clent->free ) return;

	svgame.pmove->runfuncs = false;	// all next calls ignore footstep sounds
		
	// run post-think
	if( clent->v.flags & FL_SPECTATOR )
		svgame.dllFuncs.pfnSpectatorThink( clent );
	else svgame.dllFuncs.pfnPlayerPostThink( clent );

	// restore frametime
	svgame.globals->frametime = host.frametime;
	svgame.dllFuncs.pfnCmdEnd( cl->edict );
}                                                                   