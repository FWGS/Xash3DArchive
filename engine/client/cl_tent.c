//=======================================================================
//			Copyright XashXT Group 2009 ©
//		   cl_tent.c - temp entity effects management
//=======================================================================

#include "common.h"
#include "client.h"
#include "r_efx.h"
#include "event_flags.h"
#include "entity_types.h"
#include "studio.h"

/*
==============================================================

TEMPENTS MANAGEMENT

==============================================================
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
	}

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
==============================================================

LIGHT STYLE MANAGEMENT

==============================================================
*/
static int	lastofs;
		
/*
================
CL_ClearLightStyles
================
*/
void CL_ClearLightStyles( void )
{
	Mem_Set( cl.lightstyles, 0, sizeof( cl.lightstyles ));
	lastofs = -1;
}

/*
================
CL_RunLightStyles

light animations
'm' is normal light, 'a' is no light, 'z' is double bright
================
*/
void CL_RunLightStyles( void )
{
	int		i, ofs;
	lightstyle_t	*ls;		
	float		l;

	if( cls.state != ca_active ) return;

	ofs = (cl.time * 10);
	if( ofs == lastofs ) return;
	lastofs = ofs;

	for( i = 0, ls = cl.lightstyles; i < MAX_LIGHTSTYLES; i++, ls++ )
	{
		if( ls->length == 0 ) l = 0.0f;
		else if( ls->length == 1 ) l = ls->map[0];
		else l = ls->map[ofs%ls->length];

		VectorSet( ls->rgb, l, l, l );
	}
}

void CL_SetLightstyle( int style, const char *s )
{
	int	j, k;

	ASSERT( s );
	ASSERT( style >= 0 && style < MAX_LIGHTSTYLES );

	com.strncpy( cl.lightstyles[style].pattern, s, sizeof( cl.lightstyles[0].pattern ));

	j = com.strlen( s );
	cl.lightstyles[style].length = j;

	for( k = 0; k < j; k++ )
		cl.lightstyles[style].map[k] = (float)(s[k]-'a') / (float)('m'-'a');
}

/*
================
CL_AddLightStyles
================
*/
void CL_AddLightStyles( void )
{
	int		i;
	lightstyle_t	*ls;

	for( i = 0, ls = cl.lightstyles; i < MAX_LIGHTSTYLES; i++, ls++ )
		re->AddLightStyle( i, ls->rgb );
}

/*
==============================================================

DLIGHT MANAGEMENT

==============================================================
*/
dlight_t	cl_dlights[MAX_DLIGHTS];

/*
================
CL_ClearDlights
================
*/
void CL_ClearDlights( void )
{
	Mem_Set( cl_dlights, 0, sizeof( cl_dlights ));
}

/*
===============
CL_AllocDlight

===============
*/
dlight_t *CL_AllocDlight( int key )
{
	dlight_t	*dl;
	int	i;

	// first look for an exact key match
	if( key )
	{
		for( i = 0, dl = cl_dlights; i < MAX_DLIGHTS; i++, dl++ )
		{
			if( dl->key == key )
			{
				// reuse this light
				Mem_Set( dl, 0, sizeof( *dl ));
				dl->key = key;
				return dl;
			}
		}
	}

	// then look for anything else
	for( i = 0, dl = cl_dlights; i < MAX_DLIGHTS; i++, dl++ )
	{
		if( dl->die < cl.time )
		{
			Mem_Set( dl, 0, sizeof( *dl ));
			dl->key = key;
			return dl;
		}
	}

	// otherwise grab first dlight
	dl = &cl_dlights[0];
	Mem_Set( dl, 0, sizeof( *dl ));
	dl->key = key;

	return dl;
}

/*
===============
CL_AllocElight

===============
*/
dlight_t *CL_AllocElight( int key )
{
	dlight_t	*dl;

	dl = CL_AllocDlight( key );
	dl->elight = true;

	return dl;
}

/*
===============
CL_AddDlight

copy dlight to renderer
===============
*/
qboolean CL_AddDlight( dlight_t *dl )
{
	int	flags = 0;
	qboolean	add;

	if( dl->dark ) flags |= DLIGHT_DARK;
	if( dl->elight ) flags |= DLIGHT_ONLYENTS;

	add = re->AddDLight( dl->origin, dl->color, dl->radius, flags );

	return add;
}

/*
===============
CL_AddDLights

===============
*/
void CL_AddDLights( void )
{
	dlight_t	*dl;
	float	time;
	int	i;
	
	time = cl.time - cl.oldtime;

	for( i = 0, dl = cl_dlights; i < MAX_DLIGHTS; i++, dl++ )
	{
		if( dl->die < cl_time() || !dl->radius )
			continue;
		
		dl->radius -= time * dl->decay;
		if( dl->radius < 0 ) dl->radius = 0;

		CL_AddDlight( dl );
	}
}

/*
================
CL_TestLights

if cl_testlights is set, create 32 lights models
================
*/
void CL_TestLights( void )
{
	int	i, j;
	float	f, r;
	dlight_t	dl;

	if( !cl_testlights->integer ) return;

	Mem_Set( &dl, 0, sizeof( dlight_t ));
	
	for( i = 0; i < bound( 1, cl_testlights->integer, MAX_DLIGHTS ); i++ )
	{
		r = 64 * ((i % 4) - 1.5f );
		f = 64 * ( i / 4) + 128;

		for( j = 0; j < 3; j++ )
			dl.origin[j] = cl.refdef.vieworg[j] + cl.refdef.forward[j] * f + cl.refdef.right[j] * r;

		dl.color.r = ((((i % 6) + 1) & 1)>>0) * 255;
		dl.color.g = ((((i % 6) + 1) & 2)>>1) * 255;
		dl.color.b = ((((i % 6) + 1) & 4)>>2) * 255;
		dl.radius = 200;

		if( !CL_AddDlight( &dl ))
			break; 
	}
}

/*
==============================================================

DECAL MANAGEMENT

==============================================================
*/
/*
===============
CL_DecalShoot

normal temporary decal
===============
*/
void CL_DecalShoot( HSPRITE hDecal, int entityIndex, int modelIndex, float *pos, int flags )
{
	rgba_t	color;

	Vector4Set( color, 255, 255, 255, 255 ); // don't use custom colors
	if( re ) re->DecalShoot( hDecal, entityIndex, modelIndex, pos, NULL, flags, color, 0.0f, 0.0f );
}

/*
===============
CL_PlayerDecal

spray custom colored decal (clan logo etc)
===============
*/
void CL_PlayerDecal( HSPRITE hDecal, int entityIndex, float *pos, byte *color )
{
	cl_entity_t	*pEnt;
	int		modelIndex = 0;

	pEnt = CL_GetEntityByIndex( entityIndex );
	if( pEnt ) modelIndex = pEnt->curstate.modelindex;

	if( re ) re->DecalShoot( hDecal, entityIndex, modelIndex, pos, NULL, FDECAL_CUSTOM, color, 0.0f, 0.0f );
}

/*
===============
CL_LightForPoint

get lighting color for specified point
===============
*/
void CL_LightForPoint( const vec3_t point, vec3_t ambientLight )
{
	if( re ) re->LightForPoint( point, ambientLight );
	else VectorClear( ambientLight );
}

/*
===============
CL_ResetEvent

===============
*/
void CL_ResetEvent( event_info_t *ei )
{
	Mem_Set( ei, 0, sizeof( *ei ));
}

word CL_EventIndex( const char *name )
{
	int	i;
	
	if( !name || !name[0] )
		return 0;

	for( i = 1; i < MAX_EVENTS && cl.event_precache[i][0]; i++ )
		if( !com.stricmp( cl.event_precache[i], name ))
			return i;
	return 0;
}

qboolean CL_FireEvent( event_info_t *ei )
{
	user_event_t	*ev;
	const char	*name;
	int		i, idx;

	if( !ei || !ei->index )
		return false;

	// get the func pointer
	for( i = 0; i < MAX_EVENTS; i++ )
	{
		ev = clgame.events[i];		
		if( !ev )
		{
			idx = bound( 1, ei->index, MAX_EVENTS );
			Msg( "CL_FireEvent: %s not precached\n", cl.event_precache[idx] );
			break;
		}

		if( ev->index == ei->index )
		{
			if( ev->func )
			{
				ev->func( &ei->args );
				return true;
			}

			name = cl.event_precache[ei->index];
			MsgDev( D_ERROR, "CL_FireEvent: %s not hooked\n", name );
			break;			
		}
	}
	return false;
}

void CL_FireEvents( void )
{
	int		i;
	event_state_t	*es;
	event_info_t	*ei;
	qboolean		success;

	es = &cl.events;

	for( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		ei = &es->ei[i];

		if( ei->index == 0 )
			continue;

		if( cls.state == ca_disconnected )
		{
			CL_ResetEvent( ei );
			continue;
		}

		// delayed event!
		if( ei->fire_time && ( ei->fire_time > cl_time() ))
			continue;

		success = CL_FireEvent( ei );

		// zero out the remaining fields
		CL_ResetEvent( ei );
	}
}

event_info_t *CL_FindEmptyEvent( void )
{
	int		i;
	event_state_t	*es;
	event_info_t	*ei;

	es = &cl.events;

	// look for first slot where index is != 0
	for( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		ei = &es->ei[i];
		if( ei->index != 0 )
			continue;
		return ei;
	}

	// no slots available
	return NULL;
}

event_info_t *CL_FindUnreliableEvent( void )
{
	int		i;
	event_state_t	*es;
	event_info_t	*ei;

	es = &cl.events;
	for ( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		ei = &es->ei[i];
		if( ei->index != 0 )
		{
			// it's reliable, so skip it
			if( ei->flags & FEV_RELIABLE )
				continue;
		}
		return ei;
	}

	// this should never happen
	return NULL;
}

void CL_QueueEvent( int flags, int index, float delay, event_args_t *args )
{
	qboolean		unreliable = (flags & FEV_RELIABLE) ? false : true;
	event_info_t	*ei;

	// find a normal slot
	ei = CL_FindEmptyEvent();
	if( !ei && unreliable )
	{
		return;
	}

	// okay, so find any old unreliable slot
	if( !ei )
	{
		ei = CL_FindUnreliableEvent();
		if( !ei ) return;
	}

	ei->index	= index;
	ei->fire_time = delay ? (cl_time() + delay) : 0.0f;
	ei->flags	= flags;
	
	// copy in args event data
	Mem_Copy( &ei->args, args, sizeof( ei->args ));
}

/*
=============
CL_PlaybackEvent

=============
*/
void CL_PlaybackEvent( int flags, const edict_t *pInvoker, word eventindex, float delay, float *origin,
	float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	event_args_t	args;
	int		invokerIndex = 0;

	// first check event for out of bounds
	if( eventindex < 1 || eventindex > MAX_EVENTS )
	{
		MsgDev( D_ERROR, "CL_PlaybackEvent: invalid eventindex %i\n", eventindex );
		return;
	}
	// check event for precached
	if( !CL_EventIndex( cl.event_precache[eventindex] ))
	{
		MsgDev( D_ERROR, "CL_PlaybackEvent: event %i was not precached\n", eventindex );
		return;		
	}

	flags |= FEV_CLIENT; // it's a client event
	flags &= ~(FEV_NOTHOST|FEV_HOSTONLY|FEV_GLOBAL);

	if( delay < 0.0f )
		delay = 0.0f; // fixup negative delays

	if( pInvoker ) invokerIndex = NUM_FOR_EDICT( pInvoker );

	args.flags = 0;
	args.entindex = invokerIndex;
	VectorCopy( origin, args.origin );
	VectorCopy( angles, args.angles );

	args.fparam1 = fparam1;
	args.fparam2 = fparam2;
	args.iparam1 = iparam1;
	args.iparam2 = iparam2;
	args.bparam1 = bparam1;
	args.bparam2 = bparam2;

	if( flags & FEV_RELIABLE )
	{
		args.ducking = 0;
		VectorClear( args.velocity );
	}
	else if( invokerIndex )
	{
		// get up some info from invoker
		if( VectorIsNull( args.origin )) 
			VectorCopy(((cl_entity_t *)&pInvoker)->curstate.origin, args.origin );
		if( VectorIsNull( args.angles )) 
			VectorCopy(((cl_entity_t *)&pInvoker)->curstate.angles, args.angles );
		VectorCopy(((cl_entity_t *)&pInvoker)->curstate.velocity, args.velocity );
		args.ducking = ((cl_entity_t *)&pInvoker)->curstate.usehull;
	}

	CL_QueueEvent( flags, eventindex, delay, &args );
}

/*
==============
CL_ClearEffects
==============
*/
void CL_ClearEffects( void )
{
	CL_ClearDlights ();
	CL_ClearLightStyles ();
}