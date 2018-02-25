/*
sv_pmove.c - server-side player physic
Copyright (C) 2010 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"
#include "server.h"
#include "const.h"
#include "pm_local.h"
#include "event_flags.h"
#include "studio.h"

static qboolean has_update = false;

void SV_ClearPhysEnts( void )
{
	svgame.pmove->numtouch = 0;
	svgame.pmove->numvisent = 0;
	svgame.pmove->nummoveent = 0;
	svgame.pmove->numphysent = 0;
}

qboolean SV_PlayerIsFrozen( edict_t *pClient )
{
	if( FBitSet( host.features, ENGINE_QUAKE_COMPATIBLE ))
		return false;

	if( FBitSet( pClient->v.flags, FL_FROZEN ))
		return true;
	return false;
}

void SV_ClipPMoveToEntity( physent_t *pe, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, pmtrace_t *tr )
{
	Assert( tr != NULL );

	if( svgame.physFuncs.ClipPMoveToEntity != NULL )
	{
		// do custom sweep test
		svgame.physFuncs.ClipPMoveToEntity( pe, start, mins, maxs, end, tr );
	}
	else
	{
		// function is missed, so we didn't hit anything
		tr->allsolid = false;
	}
}

qboolean SV_CopyEdictToPhysEnt( physent_t *pe, edict_t *ed )
{
	model_t	*mod = SV_ModelHandle( ed->v.modelindex );

	if( !mod ) return false;
	pe->player = false;

	pe->info = NUM_FOR_EDICT( ed );
	VectorCopy( ed->v.origin, pe->origin );
	VectorCopy( ed->v.angles, pe->angles );

	if( ed->v.flags & FL_CLIENT )
	{
		// client
		SV_GetTrueOrigin( &svs.clients[pe->info - 1], (pe->info - 1), pe->origin );
		Q_strncpy( pe->name, "player", sizeof( pe->name ));
		pe->player = pe->info;
	}
	else if( ed->v.flags & FL_FAKECLIENT && ed->v.solid != MOVETYPE_PUSH )
	{
		// bot
		Q_strncpy( pe->name, "bot", sizeof( pe->name ));
		pe->player = pe->info;
	}
	else
	{
		// otherwise copy the classname
		Q_strncpy( pe->name, STRING( ed->v.classname ), sizeof( pe->name ));
	}

	pe->model = pe->studiomodel = NULL;

	switch( ed->v.solid )
	{
	case SOLID_NOT:
	case SOLID_BSP:
		pe->model = mod;
		VectorClear( pe->mins );
		VectorClear( pe->maxs );
		break;
	case SOLID_BBOX:
		if( mod && mod->type == mod_studio && mod->flags & STUDIO_TRACE_HITBOX )
			pe->studiomodel = mod;
		VectorCopy( ed->v.mins, pe->mins );
		VectorCopy( ed->v.maxs, pe->maxs );
		break;
	case SOLID_CUSTOM:
		pe->model = (mod->type == mod_brush) ? mod : NULL;
		pe->studiomodel = (mod->type == mod_studio) ? mod : NULL;
		VectorCopy( ed->v.mins, pe->mins );
		VectorCopy( ed->v.maxs, pe->maxs );
		break;
	default:
		pe->studiomodel = (mod->type == mod_studio) ? mod : NULL;
		VectorCopy( ed->v.mins, pe->mins );
		VectorCopy( ed->v.maxs, pe->maxs );
		break;
	}

	pe->solid = ed->v.solid;
	pe->rendermode = ed->v.rendermode;
	pe->skin = ed->v.skin;
	pe->frame = ed->v.frame;
	pe->sequence = ed->v.sequence;

	memcpy( &pe->controller[0], &ed->v.controller[0], 4 * sizeof( byte ));
	memcpy( &pe->blending[0], &ed->v.blending[0], 2 * sizeof( byte ));

	pe->movetype = ed->v.movetype;
	pe->takedamage = ed->v.takedamage;
	pe->team = ed->v.team;
	pe->classnumber = ed->v.playerclass;
	pe->blooddecal = 0;	// unused in GoldSrc

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

void SV_GetTrueOrigin( sv_client_t *cl, int edictnum, vec3_t origin )
{
	// don't allow unlag in singleplayer
	if( svs.maxclients <= 1 ) return;

	if( cl->state < cs_connected || edictnum < 0 || edictnum >= svs.maxclients )
		return;

	if( !FBitSet( cl->flags, FCL_LAG_COMPENSATION ) || !sv_unlag.value )
		return;

	if( !svgame.interp[edictnum].active || !svgame.interp[edictnum].moving )
		return;

	VectorCopy( svgame.interp[edictnum].newpos, origin ); 
}

void SV_GetTrueMinMax( sv_client_t *cl, int edictnum, vec3_t mins, vec3_t maxs )
{
	// don't allow unlag in singleplayer
	if( svs.maxclients <= 1 ) return;

	if( cl->state < cs_connected || edictnum < 0 || edictnum >= svs.maxclients )
		return;

	if( !FBitSet( cl->flags, FCL_LAG_COMPENSATION ) || !sv_unlag.value )
		return;

	if( !svgame.interp[edictnum].active || !svgame.interp[edictnum].moving )
		return;

	VectorCopy( svgame.interp[edictnum].mins, mins );
	VectorCopy( svgame.interp[edictnum].maxs, maxs );
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
	vec3_t	mins, maxs;
	physent_t	*pe;

	pl = EDICT_NUM( svgame.pmove->player_index + 1 );
	Assert( SV_IsValidEdict( pl ));

	// touch linked edicts
	for( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;
		check = EDICT_FROM_AREA( l );

		if( check->v.groupinfo != 0 )
		{
			if(( !svs.groupop && (check->v.groupinfo & pl->v.groupinfo ) == 0) ||
			( svs.groupop == 1 && ( check->v.groupinfo & pl->v.groupinfo ) != 0 ))
				continue;
		}

		if( check->v.owner == pl || check->v.solid == SOLID_TRIGGER )
			continue; // player or player's own missile

		if( svgame.pmove->numvisent < MAX_PHYSENTS )
		{
			pe = &svgame.pmove->visents[svgame.pmove->numvisent];
			if( SV_CopyEdictToPhysEnt( pe, check ))
				svgame.pmove->numvisent++;
		}

		if( check->v.solid == SOLID_NOT && ( check->v.skin == CONTENTS_NONE || check->v.modelindex == 0 ))
			continue;

		// ignore monsterclip brushes
		if( FBitSet( check->v.flags, FL_MONSTERCLIP ) && check->v.solid == SOLID_BSP )
			continue;

		if( check == pl ) continue;	// himself

		// nehahra collision flags
		if( check->v.movetype != MOVETYPE_PUSH )
		{
			if(( FBitSet( check->v.flags, FL_CLIENT|FL_FAKECLIENT ) && check->v.health <= 0.0f ) || check->v.deadflag == DEAD_DEAD )
				continue;	// dead body
		}

		if( VectorIsNull( check->v.size ))
			continue;

		VectorCopy( check->v.absmin, mins );
		VectorCopy( check->v.absmax, maxs );

		if( FBitSet( check->v.flags, FL_CLIENT ))
		{
			int	e = NUM_FOR_EDICT( check ) - 1;

			// trying to get interpolated values
			SV_GetTrueMinMax( &svs.clients[e], e, mins, maxs );
		}

		if( !BoundsIntersect( pmove_mins, pmove_maxs, mins, maxs ))
			continue;

		if( svgame.pmove->numphysent < MAX_PHYSENTS )
		{
			pe = &svgame.pmove->physents[svgame.pmove->numphysent];

			if( SV_CopyEdictToPhysEnt( pe, check ))
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
	model_t	*mod;
	physent_t	*pe;
	
	// get water edicts
	for( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;
		check = EDICT_FROM_AREA( l );

		if( check->v.solid != SOLID_NOT || check->v.skin != CONTENTS_LADDER )
			continue;

		mod = SV_ModelHandle( check->v.modelindex );

		// only brushes can have special contents
		if( !mod || mod->type != mod_brush )
			continue;

		if( !BoundsIntersect( pmove_mins, pmove_maxs, check->v.absmin, check->v.absmax ))
			continue;

		if( svgame.pmove->nummoveent == MAX_MOVEENTS )
			return;

		pe = &svgame.pmove->moveents[svgame.pmove->nummoveent];
		if( SV_CopyEdictToPhysEnt( pe, check ))
			svgame.pmove->nummoveent++;
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;
	
	if( pmove_maxs[node->axis] > node->dist )
		SV_AddLaddersToPmove( node->children[0], pmove_mins, pmove_maxs );
	if( pmove_mins[node->axis] < node->dist )
		SV_AddLaddersToPmove( node->children[1], pmove_mins, pmove_maxs );
}

static int pfnTestPlayerPosition( float *pos, pmtrace_t *ptrace )
{
	return PM_TestPlayerPosition( svgame.pmove, pos, ptrace, NULL );
}

static void pfnStuckTouch( int hitent, pmtrace_t *tr )
{
	int	i;

	for( i = 0; i < svgame.pmove->numtouch; i++ )
	{
		if( svgame.pmove->touchindex[i].ent == hitent )
			return;
	}

	if( svgame.pmove->numtouch >= MAX_PHYSENTS )
	{
		MsgDev( D_ERROR, "PM_StuckTouch: MAX_TOUCHENTS limit exceeded\n" );
		return;
	}

	VectorCopy( svgame.pmove->velocity, tr->deltavelocity );
	tr->ent = hitent;

	svgame.pmove->touchindex[svgame.pmove->numtouch++] = *tr;
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
	return PM_PlayerTraceExt( svgame.pmove, start, end, traceFlags, svgame.pmove->numphysent, svgame.pmove->physents, ignore_pe, NULL );
}

static pmtrace_t *pfnTraceLine( float *start, float *end, int flags, int usehull, int ignore_pe )
{
	static pmtrace_t	tr;
	int		old_usehull;

	old_usehull = svgame.pmove->usehull;
	svgame.pmove->usehull = usehull;	

	switch( flags )
	{
	case PM_TRACELINE_PHYSENTSONLY:
		tr = PM_PlayerTraceExt( svgame.pmove, start, end, 0, svgame.pmove->numphysent, svgame.pmove->physents, ignore_pe, NULL );
		break;
	case PM_TRACELINE_ANYVISIBLE:
		tr = PM_PlayerTraceExt( svgame.pmove, start, end, 0, svgame.pmove->numvisent, svgame.pmove->visents, ignore_pe, NULL );
		break;
	}

	svgame.pmove->usehull = old_usehull;

	return &tr;
}

static hull_t *pfnHullForBsp( physent_t *pe, float *offset )
{
	return PM_HullForBsp( pe, svgame.pmove, offset );
}

static float pfnTraceModel( physent_t *pe, float *start, float *end, trace_t *trace )
{
	int	old_usehull;
	vec3_t	start_l, end_l;
	vec3_t	offset, temp;
	qboolean	rotated;
	matrix4x4	matrix;
	hull_t	*hull;

	old_usehull = svgame.pmove->usehull;
	svgame.pmove->usehull = 2;

	hull = PM_HullForBsp( pe, svgame.pmove, offset );

	svgame.pmove->usehull = old_usehull;

	if( pe->solid == SOLID_BSP && !VectorIsNull( pe->angles ))
		rotated = true;
	else rotated = false;

 	if( rotated )
 	{
 		Matrix4x4_CreateFromEntity( matrix, pe->angles, offset, 1.0f );
 		Matrix4x4_VectorITransform( matrix, start, start_l );
 		Matrix4x4_VectorITransform( matrix, end, end_l );
 	}
 	else
 	{
 		VectorSubtract( start, offset, start_l );
 		VectorSubtract( end, offset, end_l );
 	}

	PM_RecursiveHullCheck( hull, hull->firstclipnode, 0, 1, start_l, end_l, (pmtrace_t *)trace );
	trace->ent = NULL;

	if( rotated )
	{
		VectorCopy( trace->plane.normal, temp );
		Matrix4x4_TransformPositivePlane( matrix, temp, trace->plane.dist, trace->plane.normal, &trace->plane.dist );
	}

	VectorLerp( start, trace->fraction, end, trace->endpos );

	return trace->fraction;
}

static const char *pfnTraceTexture( int ground, float *vstart, float *vend )
{
	physent_t *pe;

	if( ground < 0 || ground >= svgame.pmove->numphysent )
		return NULL; // bad ground

	pe = &svgame.pmove->physents[ground];
	return PM_TraceTexture( pe, vstart, vend );
}			

static void pfnPlaySound( int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch )
{
	edict_t	*ent;

	ent = EDICT_NUM( svgame.pmove->player_index + 1 );
	if( !SV_IsValidEdict( ent )) return;

	SV_StartSound( ent, channel, sample, volume, attenuation, fFlags|SND_FILTER_CLIENT, pitch );
}

static void pfnPlaybackEventFull( int flags, int clientindex, word eventindex, float delay, float *origin,
	float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	edict_t	*ent;

	ent = EDICT_NUM( clientindex + 1 );
	if( !SV_IsValidEdict( ent )) return;

	if( host.type == HOST_DEDICATED )
		flags |= FEV_NOTHOST; // no local clients for dedicated server

	SV_PlaybackEventFull( flags, ent, eventindex,
		delay, origin, angles,
		fparam1, fparam2,
		iparam1, iparam2,
		bparam1, bparam2 );
}

static pmtrace_t pfnPlayerTraceEx( float *start, float *end, int traceFlags, pfnIgnore pmFilter )
{
	return PM_PlayerTraceExt( svgame.pmove, start, end, traceFlags, svgame.pmove->numphysent, svgame.pmove->physents, -1, pmFilter );
}

static int pfnTestPlayerPositionEx( float *pos, pmtrace_t *ptrace, pfnIgnore pmFilter )
{
	return PM_TestPlayerPosition( svgame.pmove, pos, ptrace, pmFilter );
}

static pmtrace_t *pfnTraceLineEx( float *start, float *end, int flags, int usehull, pfnIgnore pmFilter )
{
	static pmtrace_t	tr;
	int		old_usehull;

	old_usehull = svgame.pmove->usehull;
	svgame.pmove->usehull = usehull;	

	switch( flags )
	{
	case PM_TRACELINE_PHYSENTSONLY:
		tr = PM_PlayerTraceExt( svgame.pmove, start, end, 0, svgame.pmove->numphysent, svgame.pmove->physents, -1, pmFilter );
		break;
	case PM_TRACELINE_ANYVISIBLE:
		tr = PM_PlayerTraceExt( svgame.pmove, start, end, 0, svgame.pmove->numvisent, svgame.pmove->visents, -1, pmFilter );
		break;
	}

	svgame.pmove->usehull = old_usehull;

	return &tr;
}

static struct msurface_s *pfnTraceSurface( int ground, float *vstart, float *vend )
{
	physent_t *pe;

	if( ground < 0 || ground >= svgame.pmove->numphysent )
		return NULL; // bad ground

	pe = &svgame.pmove->physents[ground];
	return PM_TraceSurface( pe, vstart, vend );
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

	// enumerate client hulls
	for( i = 0; i < MAX_MAP_HULLS; i++ )
	{
		if( svgame.dllFuncs.pfnGetHullBounds( i, host.player_mins[i], host.player_maxs[i] ))
			MsgDev( D_NOTE, "SV: hull%i, player_mins: %g %g %g, player_maxs: %g %g %g\n", i,
			host.player_mins[i][0], host.player_mins[i][1], host.player_mins[i][2],
			host.player_maxs[i][0], host.player_maxs[i][1], host.player_maxs[i][2] );
	}

	memcpy( svgame.pmove->player_mins, host.player_mins, sizeof( host.player_mins ));
	memcpy( svgame.pmove->player_maxs, host.player_maxs, sizeof( host.player_maxs ));

	// common utilities
	svgame.pmove->PM_Info_ValueForKey = Info_ValueForKey;
	svgame.pmove->PM_Particle = CL_Particle; // for local system only
	svgame.pmove->PM_TestPlayerPosition = pfnTestPlayerPosition;
	svgame.pmove->Con_NPrintf = Con_NPrintf;
	svgame.pmove->Con_DPrintf = Con_DPrintf;
	svgame.pmove->Con_Printf = Con_Printf;
	svgame.pmove->Sys_FloatTime = Sys_DoubleTime;
	svgame.pmove->PM_StuckTouch = pfnStuckTouch;
	svgame.pmove->PM_PointContents = pfnPointContents;
	svgame.pmove->PM_TruePointContents = pfnTruePointContents;
	svgame.pmove->PM_HullPointContents = pfnHullPointContents; 
	svgame.pmove->PM_PlayerTrace = pfnPlayerTrace;
	svgame.pmove->PM_TraceLine = pfnTraceLine;
	svgame.pmove->RandomLong = COM_RandomLong;
	svgame.pmove->RandomFloat = COM_RandomFloat;
	svgame.pmove->PM_GetModelType = pfnGetModelType;
	svgame.pmove->PM_GetModelBounds = pfnGetModelBounds;	
	svgame.pmove->PM_HullForBsp = pfnHullForBsp;
	svgame.pmove->PM_TraceModel = pfnTraceModel;
	svgame.pmove->COM_FileSize = COM_FileSize;
	svgame.pmove->COM_LoadFile = COM_LoadFile;
	svgame.pmove->COM_FreeFile = COM_FreeFile;
	svgame.pmove->memfgets = COM_MemFgets;
	svgame.pmove->PM_PlaySound = pfnPlaySound;
	svgame.pmove->PM_TraceTexture = pfnTraceTexture;
	svgame.pmove->PM_PlaybackEventFull = pfnPlaybackEventFull;
	svgame.pmove->PM_PlayerTraceEx = pfnPlayerTraceEx;
	svgame.pmove->PM_TestPlayerPositionEx = pfnTestPlayerPositionEx;
	svgame.pmove->PM_TraceLineEx = pfnTraceLineEx;
	svgame.pmove->PM_TraceSurface = pfnTraceSurface;

	// initalize pmove
	svgame.dllFuncs.pfnPM_Init( svgame.pmove );
}

static void PM_CheckMovingGround( edict_t *ent, float frametime )
{
	if( svgame.physFuncs.SV_UpdatePlayerBaseVelocity != NULL )
	{
		svgame.physFuncs.SV_UpdatePlayerBaseVelocity( ent );
	}
	else
	{
		SV_UpdateBaseVelocity( ent );
	}

	if( !FBitSet( ent->v.flags, FL_BASEVELOCITY ))
	{
		// apply momentum (add in half of the previous frame of velocity first)
		VectorMA( ent->v.velocity, 1.0f + (frametime * 0.5f), ent->v.basevelocity, ent->v.velocity );
		VectorClear( ent->v.basevelocity );
	}

	ClearBits( ent->v.flags, FL_BASEVELOCITY );
}

static void SV_SetupPMove( playermove_t *pmove, sv_client_t *cl, usercmd_t *ucmd, const char *physinfo )
{
	vec3_t	absmin, absmax;
	edict_t	*clent = cl->edict;
	int	i;

	svgame.globals->frametime = (ucmd->msec * 0.001f);

	pmove->player_index = NUM_FOR_EDICT( clent ) - 1;
	pmove->multiplayer = (svs.maxclients > 1) ? true : false;
	pmove->time = (float)(cl->timebase * 1000.0);
	VectorCopy( clent->v.origin, pmove->origin );
	VectorCopy( clent->v.v_angle, pmove->angles );
	VectorCopy( clent->v.v_angle, pmove->oldangles );
	VectorCopy( clent->v.velocity, pmove->velocity );
	VectorCopy( clent->v.basevelocity, pmove->basevelocity );
	VectorCopy( clent->v.view_ofs, pmove->view_ofs );
	VectorCopy( clent->v.movedir, pmove->movedir );
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
	pmove->spectator = 0; // spectator physic all execute on client
	pmove->movetype = clent->v.movetype;
	if( pmove->multiplayer ) pmove->onground = -1;
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
	pmove->runfuncs = true;
	
	Q_strncpy( pmove->physinfo, physinfo, MAX_INFO_STRING );

	// setup physents
	pmove->numvisent = 0;
	pmove->numphysent = 0;
	pmove->nummoveent = 0;

	for( i = 0; i < 3; i++ )
	{
		absmin[i] = clent->v.origin[i] - 256.0f;
		absmax[i] = clent->v.origin[i] + 256.0f;
	}

	SV_CopyEdictToPhysEnt( &svgame.pmove->physents[0], &svgame.edicts[0] );
	svgame.pmove->visents[0] = svgame.pmove->physents[0];
	svgame.pmove->numphysent = 1;	// always have world
	svgame.pmove->numvisent = 1;

	SV_AddLinksToPmove( sv_areanodes, absmin, absmax );
	SV_AddLaddersToPmove( sv_areanodes, absmin, absmax );
}

static void SV_FinishPMove( playermove_t *pmove, sv_client_t *cl )
{
	edict_t	*clent = cl->edict;

	clent->v.teleport_time = pmove->waterjumptime;
	VectorCopy( pmove->origin, clent->v.origin );
	VectorCopy( pmove->view_ofs, clent->v.view_ofs );
	VectorCopy( pmove->velocity, clent->v.velocity );
	VectorCopy( pmove->basevelocity, clent->v.basevelocity );
	VectorCopy( pmove->punchangle, clent->v.punchangle );
	VectorCopy( pmove->movedir, clent->v.movedir );
	clent->v.flTimeStepSound = pmove->flTimeStepSound;
	clent->v.flFallVelocity = pmove->flFallVelocity;
	clent->v.oldbuttons = pmove->oldbuttons;
	clent->v.waterlevel = pmove->waterlevel;
	clent->v.watertype = pmove->watertype;
	clent->v.maxspeed = pmove->clientmaxspeed;
	clent->v.flDuckTime = pmove->flDuckTime;
	clent->v.flSwimTime = pmove->flSwimTime;
	clent->v.iStepLeft = pmove->iStepLeft;
	clent->v.movetype = pmove->movetype;
	clent->v.friction = pmove->friction;
	clent->v.deadflag = pmove->deadflag;
	clent->v.effects = pmove->effects;
	clent->v.bInDuck = pmove->bInDuck;
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

	if( pmove->onground == -1 )
	{
		clent->v.flags &= ~FL_ONGROUND;
	}
	else if( pmove->onground >= 0 && pmove->onground < pmove->numphysent )
	{
		clent->v.flags |= FL_ONGROUND;
		clent->v.groundentity = EDICT_NUM( pmove->physents[pmove->onground].info );
	}

	// angles
	// show 1/3 the pitch angle and all the roll angle	
	if( !clent->v.fixangle )
	{
		VectorCopy( pmove->angles, clent->v.v_angle );
		clent->v.angles[PITCH] = -( clent->v.v_angle[PITCH] / 3.0f );
		clent->v.angles[ROLL] = clent->v.v_angle[ROLL];
		clent->v.angles[YAW] = clent->v.v_angle[YAW];
	}

	SV_SetMinMaxSize( clent, pmove->player_mins[pmove->usehull], pmove->player_maxs[pmove->usehull], false );

	// all next calls ignore footstep sounds
	pmove->runfuncs = false;
}

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

qboolean SV_UnlagCheckTeleport( vec3_t old_pos, vec3_t new_pos )
{
	int	i;

	for( i = 0; i < 3; i++ )
	{
		if( fabs( old_pos[i] - new_pos[i] ) > 64.0f )
			return true;
	}
	return false;
}

void SV_SetupMoveInterpolant( sv_client_t *cl )
{
	int		i, j, clientnum;
	float		finalpush, lerp_msec;
	float		latency, lerpFrac;
	client_frame_t	*frame, *frame2;
	entity_state_t	*state, *lerpstate;
	vec3_t		curpos, newpos;
	sv_client_t	*check;
	sv_interp_t	*lerp;

	memset( svgame.interp, 0, sizeof( svgame.interp ));
	has_update = false;

	// don't allow unlag in singleplayer
	if( svs.maxclients <= 1 || cl->state != cs_spawned )
		return;

	// unlag disabled by game request
	if( !svgame.dllFuncs.pfnAllowLagCompensation() || !sv_unlag.value )
		return;

	// unlag disabled for current client
	if( !FBitSet( cl->flags, FCL_LAG_COMPENSATION ))
		return;

	has_update = true;

	for( i = 0, check = svs.clients; i < svs.maxclients; i++, check++ )
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

	if( sv_maxunlag.value != 0.0f )
	{
		if (sv_maxunlag.value < 0.0f )
			Cvar_SetValue( "sv_maxunlag", 0.0f );

		if( latency >= sv_maxunlag.value )
			latency = sv_maxunlag.value;
	}

	lerp_msec = cl->lastcmd.lerp_msec * 0.001f;
	if( lerp_msec > 0.1f ) lerp_msec = 0.1f;

	if( lerp_msec < cl->cl_updaterate )
		lerp_msec = cl->cl_updaterate;

	finalpush = ( host.realtime - latency - lerp_msec ) + sv_unlagpush.value;
	if( finalpush > host.realtime ) finalpush = host.realtime; // pushed too much ?

	frame = NULL;

	for( frame2 = NULL, i = 0; i < SV_UPDATE_BACKUP; i++, frame2 = frame )
	{
		frame = &cl->frames[(cl->netchan.outgoing_sequence - (i + 1)) & SV_UPDATE_MASK];

		for( j = 0; j < frame->num_entities; j++ )
		{
			state = &svs.packet_entities[(frame->first_entity+j)%svs.num_client_entities];

			if( state->number <= 0 || state->number >= svs.maxclients )
				continue;

			lerp = &svgame.interp[state->number-1];
			if( lerp->nointerp ) continue;

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
		memset( svgame.interp, 0, sizeof( svgame.interp ));
		has_update = false;
		return;
	}

	if( !frame2 )
	{
		frame2 = frame;
		lerpFrac = 0;
	}
	else
	{
		if( frame2->senttime - frame->senttime == 0.0 )
		{
			lerpFrac = 0;
		}
		else
		{
			lerpFrac = (finalpush - frame->senttime) / (frame2->senttime - frame->senttime);
			lerpFrac = bound( 0.0f, lerpFrac, 1.0f );
		}
	}

	for( i = 0; i < frame->num_entities; i++ )
	{
		state = &svs.packet_entities[(frame->first_entity+i)%svs.num_client_entities];

		if( state->number <= 0 || state->number >= svs.maxclients )
			continue;

		clientnum = state->number - 1;
		check = &svs.clients[clientnum];

		if( check->state != cs_spawned || check == cl )
			continue;

		lerp = &svgame.interp[clientnum];

		if( !lerp->active || lerp->nointerp )
			continue;

		lerpstate = SV_FindEntInPack( state->number, frame2 );

		if( !lerpstate )
		{
			VectorCopy( state->origin, curpos );
		}
		else
		{
			VectorSubtract( lerpstate->origin, state->origin, newpos );
			VectorMA( state->origin, lerpFrac, newpos, curpos );
		}

		VectorCopy( curpos, lerp->curpos );
		VectorCopy( curpos, lerp->newpos );

		if( !VectorCompare( curpos, check->edict->v.origin ))
		{
			VectorCopy( curpos, check->edict->v.origin );
			SV_LinkEdict( check->edict, false );
			lerp->moving = true;
		}
	}
}

void SV_RestoreMoveInterpolant( sv_client_t *cl )
{
	sv_client_t	*check;
	sv_interp_t	*oldlerp;
	int		i;

	if( !has_update )
	{
		has_update = true;
		return;
	}

	// don't allow unlag in singleplayer
	if( svs.maxclients <= 1 || cl->state != cs_spawned )
		return;

	// unlag disabled by game request
	if( !svgame.dllFuncs.pfnAllowLagCompensation() || !sv_unlag.value )
		return;

	// unlag disabled for current client
	if( !FBitSet( cl->flags, FCL_LAG_COMPENSATION ))
		return;

	for( i = 0, check = svs.clients; i < svs.maxclients; i++, check++ )
	{
		if( check->state != cs_spawned || check == cl )
			continue;

		oldlerp = &svgame.interp[i];

		if( VectorCompare( oldlerp->oldpos, oldlerp->newpos ))
			continue; // they didn't actually move.

		if( !oldlerp->moving || !oldlerp->active )
			return;

		if( VectorCompare( oldlerp->curpos, check->edict->v.origin ))
		{
			VectorCopy( oldlerp->oldpos, check->edict->v.origin );
			SV_LinkEdict( check->edict, false );
		}
	}
}

/*
===========
SV_RunCmd
===========
*/
void SV_RunCmd( sv_client_t *cl, usercmd_t *ucmd, int random_seed )
{
	edict_t	*clent, *touch;
	double	frametime;
	int	i, oldmsec;
	pmtrace_t	*pmtrace;
	trace_t	trace;
	vec3_t	oldvel;
	usercmd_t cmd;
   
	clent = cl->edict;
	cmd = *ucmd;

	if( cl->ignorecmdtime > host.realtime )
	{
		cl->cmdtime += ((double)ucmd->msec / 1000.0 );
		return;
	}

	cl->ignorecmdtime = 0.0;

	// chop up very long commands
	if( cmd.msec > 50 )
	{
		oldmsec = ucmd->msec;
		cmd.msec = oldmsec / 2;
		SV_RunCmd( cl, &cmd, random_seed );
		cmd.msec = oldmsec / 2;
		cmd.impulse = 0;
		SV_RunCmd( cl, &cmd, random_seed );
		return;
	}

	if( !FBitSet( cl->flags, FCL_FAKECLIENT ))
	{
		SV_SetupMoveInterpolant( cl );
	}

	svgame.dllFuncs.pfnCmdStart( cl->edict, ucmd, random_seed );

	frametime = ((double)ucmd->msec / 1000.0 );
	cl->timebase += frametime;
	cl->cmdtime += frametime;

	PM_CheckMovingGround( clent, frametime );

	VectorCopy( clent->v.v_angle, svgame.pmove->oldangles ); // save oldangles
	if( !clent->v.fixangle ) VectorCopy( ucmd->viewangles, clent->v.v_angle );

	VectorClear( clent->v.clbasevelocity );

	// copy player buttons
	clent->v.button = ucmd->buttons;
	clent->v.light_level = ucmd->lightlevel;
	if( ucmd->impulse ) clent->v.impulse = ucmd->impulse;

	if( ucmd->impulse == 204 )
	{
		// force client.dll update
		SV_RefreshUserinfo();
	}

	svgame.globals->time = cl->timebase;
	svgame.dllFuncs.pfnPlayerPreThink( clent );
	SV_PlayerRunThink( clent, frametime, cl->timebase );

	// If conveyor, or think, set basevelocity, then send to client asap too.
	if( !VectorIsNull( clent->v.basevelocity ))
		VectorCopy( clent->v.basevelocity, clent->v.clbasevelocity );

	// setup playermove state
	SV_SetupPMove( svgame.pmove, cl, ucmd, cl->physinfo );

	// motor!
	svgame.dllFuncs.pfnPM_Move( svgame.pmove, true );

	// copy results back to client
	SV_FinishPMove( svgame.pmove, cl );

	if( clent->v.solid != SOLID_NOT && !FBitSet( sv.hostflags, SVF_PLAYERSONLY ))
	{
		if( svgame.physFuncs.PM_PlayerTouch != NULL )
		{
			// run custom impact function
			svgame.physFuncs.PM_PlayerTouch( svgame.pmove, clent );
		}
		else
		{
			// link into place and touch triggers
			SV_LinkEdict( clent, true );
			VectorCopy( clent->v.velocity, oldvel ); // save velocity

			// touch other objects
			for( i = 0; i < svgame.pmove->numtouch; i++ )
			{
				pmtrace = &svgame.pmove->touchindex[i];
				touch = EDICT_NUM( svgame.pmove->physents[pmtrace->ent].info );
				VectorCopy( pmtrace->deltavelocity, clent->v.velocity );
				PM_ConvertTrace( &trace, pmtrace, touch );
				SV_Impact( touch, clent, &trace );
			}

			// restore velocity
			VectorCopy( oldvel, clent->v.velocity );
		}
	}

	svgame.pmove->numtouch = 0;
	svgame.globals->time = cl->timebase;
	svgame.globals->frametime = frametime;

	// run post-think
	svgame.dllFuncs.pfnPlayerPostThink( clent );
	svgame.dllFuncs.pfnCmdEnd( clent );

	if( !FBitSet( cl->flags, FCL_FAKECLIENT ))
	{
		SV_RestoreMoveInterpolant( cl );
	}
}                                                         