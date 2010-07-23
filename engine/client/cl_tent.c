//=======================================================================
//			Copyright XashXT Group 2009 ©
//		   cl_tent.c - temp entity effects management
//=======================================================================

#include "common.h"
#include "client.h"
#include "tmpent_def.h"

/*
==============================================================

TEMPENTS MANAGEMENT

==============================================================
*/
int CL_AddEntity( edict_t *pEnt, int ed_type, shader_t customShader )
{
	if( !re || !pEnt )
		return false;

	// let the render reject entity without model
	return re->AddRefEntity( pEnt, ed_type, customShader );
}

int CL_AddTempEntity( TEMPENTITY *pTemp, shader_t customShader )
{
	if( !re || !pTemp )
		return false;

	if( !pTemp->pvEngineData )
	{
		if( pTemp->modelindex && !( pTemp->flags & FTENT_NOMODEL ))
		{
			// check model			
			modtype_t	type = CM_GetModelType( pTemp->modelindex );

			if( type == mod_studio || type == mod_sprite )
			{
				// alloc engine data to holds lerping values for studiomdls and sprites
				pTemp->pvEngineData = Mem_Alloc( cls.mempool, sizeof( lerpframe_t ));
			}
			else
			{
				pTemp->flags |= FTENT_NOMODEL;
				pTemp->modelindex = 0;
			}
		}
	}		

	// let the render reject entity without model
	return re->AddTmpEntity( pTemp, ED_TEMPENTITY, customShader );
}

/*
==============================================================

LIGHT STYLE MANAGEMENT

==============================================================
*/

typedef struct
{
	int	length;
	float	value[3];
	float	map[MAX_STRING];
} clightstyle_t;

clightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
static int	lastofs;
		
/*
================
CL_ClearLightStyles
================
*/
void CL_ClearLightStyles( void )
{
	Mem_Set( cl_lightstyle, 0, sizeof( cl_lightstyle ));
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
	float		l, oldval, curval;
	clightstyle_t	*ls;		

	if( cls.state != ca_active ) return;

	if( cl_lightstyle_lerping->integer )
	{
		ofs = cl.frame.servertime / 100;
	}
	else
	{
		ofs = cl.time / 100;
		if( ofs == lastofs ) return;
		lastofs = ofs;
	}

	for( i = 0, ls = cl_lightstyle; i < MAX_LIGHTSTYLES; i++, ls++ )
	{
		if( ls->length == 0 ) l = 0.0f;
		else if( ls->length == 1 ) l = ls->map[0];
		else if( cl_lightstyle_lerping->integer )
		{
			int	curnum = (ofs % ls->length);
			int	oldnum = curnum - 1;

			// sequence is ends ?
			if( oldnum < 0 )
				oldnum += ls->length;

			oldval = ls->map[oldnum];
			curval = ls->map[curnum];

			// don't lerping fast sequences
			if( fabs( curval - oldval ) >= 1.0f ) l = curval;
			else l = oldval + cl.lerpFrac * (curval - oldval);
		}
		else l = ls->map[ofs%ls->length];

		VectorSet( ls->value, l, l, l );
	}
}

void CL_SetLightstyle( int i )
{
	char	*s;
	int	j, k;

	s = cl.configstrings[i+CS_LIGHTSTYLES];
	j = com.strlen( s );

	if( j >= MAX_STRING )
		Host_Error("CL_SetLightStyle: lightstyle %s is too long\n", s );

	cl_lightstyle[i].length = j;

	for( k = 0; k < j; k++ )
		cl_lightstyle[i].map[k] = (float)(s[k]-'a') / (float)('m'-'a');
}

/*
================
CL_AddLightStyles
================
*/
void CL_AddLightStyles( void )
{
	int		i;
	clightstyle_t	*ls;

	for( i = 0, ls = cl_lightstyle; i < MAX_LIGHTSTYLES; i++, ls++ )
		re->AddLightStyle( i, ls->value );
}

/*
==============================================================

DLIGHT MANAGEMENT

==============================================================
*/
struct dlight_s
{
	vec3_t		origin;
	float		radius;
	byte		color[3];
	float		die;	// stop lighting after this time
	float		decay;	// drop this each second
	float		minlight;	// don't add when contributing less
	int		key;
	bool		dark;	// subtracts light instead of adding
	bool		elight;	// true when calls with CL_AllocElight
};

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
		if( dl->die < cl_time( ))
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
bool CL_AddDlight( dlight_t *dl )
{
	int	flags = 0;
	bool	add;

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
	
	time = cls.frametime;

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

		dl.color[0] = ((((i % 6) + 1) & 1)>>0) * 255;
		dl.color[1] = ((((i % 6) + 1) & 2)>>1) * 255;
		dl.color[2] = ((((i % 6) + 1) & 4)>>2) * 255;
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
	edict_t	*pEnt;
	int	modelIndex = 0;

	pEnt = CL_GetEdictByIndex( entityIndex );
	if( CL_IsValidEdict( pEnt )) modelIndex = pEnt->v.modelindex;

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

word CL_PrecacheEvent( const char *name )
{
	int	i;
	
	if( !name || !name[0] ) return 0;

	for( i = 1; i < MAX_EVENTS && cl.configstrings[CS_EVENTS+i][0]; i++ )
		if( !com.strcmp( cl.configstrings[CS_EVENTS+i], name ))
			return i;
	return 0;
}

bool CL_FireEvent( event_info_t *ei )
{
	user_event_t	*ev;
	const char	*name;
	int		i;

	if( !ei || !ei->index )
		return false;

	// get the func pointer
	for( i = 0; i < MAX_EVENTS; i++ )
	{
		ev = clgame.events[i];		
		if( !ev ) break;

		if( ev->index == ei->index )
		{
			if( ev->func )
			{
				ev->func( &ei->args );
				return true;
			}

			name = cl.configstrings[CS_EVENTS+ei->index];
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
	bool		success;

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
		if( ei->fire_time && ( ei->fire_time > clgame.globals->time ))
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
	bool		unreliable = (flags & FEV_RELIABLE) ? false : true;
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
	ei->fire_time = delay ? (clgame.globals->time + delay) : 0.0f;
	ei->flags	= flags;
	
	// copy in args event data
	Mem_Copy( &ei->args, args, sizeof( ei->args ));
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