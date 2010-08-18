//=======================================================================
//			Copyright XashXT Group 2010 ©
//		      cl_pmove.c - client-side player physic
//=======================================================================

#include "common.h"
#include "client.h"
#include "const.h"
#include "protocol.h"
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

	// bad model ?
	if( !mod || mod->type == mod_bad )
		return false;
		
	pe->player = false;

	if( ent->index > 0 && ent->index < clgame.globals->maxClients )
	{
		// client or bot
		com.strncpy( pe->name, "player", sizeof( pe->name ));
		pe->player = true;
	}
	else if( ent == clgame.entities )
	{
		// it's a world
		com.strncpy( pe->name, "world", sizeof( pe->name ));
	}
	else
	{
		// otherwise copy the modelname
		com.strncpy( pe->name, mod->name, sizeof( pe->name ));
	}

	if( ent->curstate.movetype == MOVETYPE_PUSHSTEP || mod->type == mod_studio && mod->extradata )
	{
		pe->studiomodel = mod;
		pe->model = NULL;
	}
	else if( mod->type == mod_brush || mod->type == mod_world )
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
	pe->skin = ent->curstate.scale;
	pe->rendermode = ent->curstate.rendermode;
	pe->framerate = ent->curstate.framerate;
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

	trace = PM_PlayerTrace( clgame.pmove, pos, pos, PM_STUDIO_BOX, clgame.pmove->usehull, -1, NULL );
	if( ptrace ) *ptrace = trace; 
	return trace.ent;
}

static void pfnCon_NPrintf( int idx, char *fmt, ... )
{
	va_list	argptr;
	char	string[MAX_SYSPATH];

	if( idx != cl.playernum )
		return;

	va_start( argptr, fmt );
	com.vsprintf( string, fmt, argptr );
	va_end( argptr );

	MsgDev( D_INFO, "^6%s\n", string );	
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

	clgame.pmove->server = false;	// running at client
	clgame.pmove->movevars = &clgame.movevars;
	clgame.pmove->runfuncs = false;

	// enumerate client hulls
	for( i = 0; i < 4; i++ )
		clgame.dllFuncs.pfnGetHullBounds( i, clgame.player_mins[i], clgame.player_maxs[i] );

	Mem_Copy( clgame.pmove->player_mins, clgame.player_mins, sizeof( clgame.player_mins ));
	Mem_Copy( clgame.pmove->player_maxs, clgame.player_maxs, sizeof( clgame.player_maxs ));

	// common utilities
	clgame.pmove->PM_Info_ValueForKey = Info_ValueForKey;
	clgame.pmove->PM_Particle = pfnParticle;
	clgame.pmove->PM_TestPlayerPosition = pfnTestPlayerPosition;
	clgame.pmove->ConNPrintf = pfnCon_NPrintf;
	clgame.pmove->ConDPrintf = pfnCon_DPrintf;
	clgame.pmove->ConPrintf = pfnCon_Printf;
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