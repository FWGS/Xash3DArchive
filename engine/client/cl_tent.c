//=======================================================================
//			Copyright XashXT Group 2009 ©
//		   cl_tent.c - temp entity effects management
//=======================================================================

#include "common.h"
#include "client.h"
#include "r_efx.h"
#include "event_flags.h"
#include "entity_types.h"
#include "triangleapi.h"
#include "studio.h"

/*
==============================================================

PARTICLES MANAGEMENT

==============================================================
*/
#define NUMVERTEXNORMALS	162
#define SPARK_COLORCOUNT	9

// particle velocities
static const float	cl_avertexnormals[NUMVERTEXNORMALS][3] =
{
#include "anorms.h"
};

// particle ramps
static int ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
static int ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };
static int ramp3[6] = { 0x6d, 0x6b, 6, 5, 4, 3 };

static int gTracerColors[][3] =
{
{ 255, 255, 255 },		// White
{ 255, 0, 0 },		// Red
{ 0, 255, 0 },		// Green
{ 0, 0, 255 },		// Blue
{ 0, 0, 0 },		// Tracer default, filled in from cvars, etc.
{ 255, 167, 17 },		// Yellow-orange sparks
{ 255, 130, 90 },		// Yellowish streaks (garg)
{ 55, 60, 144 },		// Blue egon streak
{ 255, 130, 90 },		// More Yellowish streaks (garg)
{ 255, 140, 90 },		// More Yellowish streaks (garg)
{ 200, 130, 90 },		// More red streaks (garg)
{ 255, 120, 70 },		// Darker red streaks (garg)
};

static int gSparkRamp[SPARK_COLORCOUNT][3] =
{
{ 255, 255, 255 },
{ 255, 247, 199 },
{ 255, 243, 147 },
{ 255, 243, 27 },
{ 239, 203, 31 },
{ 223, 171, 39 },
{ 207, 143, 43 },
{ 127, 59, 43 },
{ 35, 19, 7 }
};

particle_t	*cl_active_particles;
particle_t	*cl_free_particles;
particle_t	*cl_particles = NULL;	// particle pool
static vec3_t	cl_avelocities[NUMVERTEXNORMALS];

/*
================
CL_InitParticles

================
*/
void CL_InitParticles( void )
{
	int	i;

	cl_particles = Mem_Alloc( cls.mempool, sizeof( particle_t ) * GI->max_particles );
	CL_ClearParticles ();

	// this is used for EF_BRIGHTFIELD
	for( i = 0; i < NUMVERTEXNORMALS; i++ )
	{
		cl_avelocities[i][0] = Com_RandomLong( 0, 255 ) * 0.01f;
		cl_avelocities[i][1] = Com_RandomLong( 0, 255 ) * 0.01f;
		cl_avelocities[i][2] = Com_RandomLong( 0, 255 ) * 0.01f;
	}
}

/*
================
CL_ClearParticles

================
*/
void CL_ClearParticles( void )
{
	int	i;

	cl_free_particles = cl_particles;
	cl_active_particles = NULL;

	for( i = 0; i < GI->max_particles; i++ )
		cl_particles[i].next = &cl_particles[i+1];

	cl_particles[GI->max_particles-1].next = NULL;
}

/*
================
CL_FreeParticles

================
*/
void CL_FreeParticles( void )
{
	if( cl_particles ) Mem_Free( cl_particles );
}

/*
================
CL_FreeParticle

move particle to freelist
================
*/
void CL_FreeParticle( particle_t *p )
{
	if( p->type == pt_clientcustom && p->deathfunc )
	{
		// call right the deathfunc func before die
		p->deathfunc( p );
	}

	p->next = cl_free_particles;
	cl_free_particles = p;
}

/*
================
CL_AllocParticle

can return NULL if particles is out
================
*/
particle_t *CL_AllocParticle( void (*callback)( particle_t*, float ))
{
	particle_t	*p;

	// never alloc particles when we not in game
	if( !CL_IsInGame( )) return NULL;

	if( !cl_free_particles )
	{
		MsgDev( D_INFO, "Overflow %d particles\n", GI->max_particles );
		return NULL;
	}

	p = cl_free_particles;
	cl_free_particles = p->next;
	p->next = cl_active_particles;
	cl_active_particles = p;

	// clear old particle
	p->type = pt_static;
	VectorClear( p->vel );
	VectorClear( p->org );
	p->ramp = 0;

	if( callback )
	{
		p->type = pt_clientcustom;
		p->callback = callback;
	}

	return p;
}
void CL_UpdateParticle( particle_t *p, float ft )
{
	float	time3 = 15.0 * ft;
	float	time2 = 10.0 * ft;
	float	time1 = 5.0 * ft;
	float	dvel = 4 * ft;
	float	grav = ft * clgame.movevars.gravity * 0.05f;
	float	size = 1.5f;
	vec3_t	right, up;
	rgb_t	color;
	int	i;

	switch( p->type )
	{
	case pt_static:
		break;
	case pt_clientcustom:
		if( p->callback )
			p->callback( p, ft );
		return;
	case pt_fire:
		p->ramp += time1;
		if( p->ramp >= 6 ) p->die = -1;
		else p->color = ramp3[(int)p->ramp];
		p->vel[2] += grav;
		break;
	case pt_explode:
		p->ramp += time2;
		if( p->ramp >= 8 ) p->die = -1;
		else p->color = ramp1[(int)p->ramp];
		for( i = 0; i < 3; i++ )
			p->vel[i] += p->vel[i] * dvel;
		p->vel[2] -= grav;
		break;
	case pt_explode2:
		p->ramp += time3;
		if( p->ramp >= 8 ) p->die = -1;
		else p->color = ramp2[(int)p->ramp];
		for( i = 0; i < 3; i++ )
			p->vel[i] -= p->vel[i] * ft;
		p->vel[2] -= grav;
		break;
	case pt_blob:
		for( i = 0; i < 3; i++ )
			p->vel[i] += p->vel[i] * dvel;
		p->vel[2] -= grav;
		break;
	case pt_blob2:
		for( i = 0; i < 2; i++ )
			p->vel[i] -= p->vel[i] * dvel;
		p->vel[2] -= grav;
		break;
	case pt_grav:
		p->vel[2] -= grav * 20;
		break;
	case pt_slowgrav:
		p->vel[2] = grav;
		break;
	case pt_vox_grav:
		p->vel[2] -= grav * 8;
		break;
	case pt_vox_slowgrav:
		p->vel[2] -= grav * 4;
		break;
	}

	// HACKHACK a scale up to keep particles from disappearing
	size += (p->org[0] - cl.refdef.vieworg[0]) * cl.refdef.forward[0];
	size += (p->org[1] - cl.refdef.vieworg[1]) * cl.refdef.forward[1];
	size += (p->org[2] - cl.refdef.vieworg[2]) * cl.refdef.forward[2];

	if( size < 20.0f ) size = 1.0f;
	else size = 1.0f + size * 0.004f;

 	// scale the axes by radius
	VectorScale( cl.refdef.right, size, right );
	VectorScale( cl.refdef.up, size, up );

	p->color = bound( 0, p->color, 255 );
	VectorSet( color, clgame.palette[p->color][0], clgame.palette[p->color][1], clgame.palette[p->color][2] );

	re->Enable( TRI_SHADER );
	re->RenderMode( kRenderTransTexture );
	re->Color4ub( color[0], color[1], color[2], 0xFF );

	re->Bind( cls.particleShader, 0 );

	// add the 4 corner vertices.
	re->Begin( TRI_QUADS );

	re->TexCoord2f( 0.0f, 1.0f );
	re->Vertex3f( p->org[0] - right[0] + up[0], p->org[1] - right[1] + up[1], p->org[2] - right[2] + up[2] );
	re->TexCoord2f( 0.0f, 0.0f );
	re->Vertex3f( p->org[0] + right[0] + up[0], p->org[1] + right[1] + up[1], p->org[2] + right[2] + up[2] );
	re->TexCoord2f( 1.0f, 0.0f );
	re->Vertex3f( p->org[0] + right[0] - up[0], p->org[1] + right[1] - up[1], p->org[2] + right[2] - up[2] );
	re->TexCoord2f( 1.0f, 1.0f );
	re->Vertex3f( p->org[0] - right[0] - up[0], p->org[1] - right[1] - up[1], p->org[2] - right[2] - up[2] );

	re->End();
	re->Disable( TRI_SHADER );

	// update position.
	VectorMA( p->org, ft, p->vel, p->org );
}

void CL_DrawParticles( void )
{
	particle_t	*p, *kill;
	float		frametime;

	frametime = cl.time - cl.oldtime;

	if( !cl_draw_particles->integer )
		return;

	while( 1 ) 
	{
		// free time-expired particles
		kill = cl_active_particles;
		if( kill && kill->die < cl.time )
		{
			cl_active_particles = kill->next;
			CL_FreeParticle( kill );
			continue;
		}
		break;
	}

	for( p = cl_active_particles; p; p = p->next )
	{
		while( 1 )
		{
			kill = p->next;
			if( kill && kill->die < cl.time )
			{
				p->next = kill->next;
				CL_FreeParticle( kill );
				continue;
			}
			break;
		}

		CL_UpdateParticle( p, frametime );
	}
}

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
	CL_ClearParticles ();
	CL_ClearLightStyles ();
}