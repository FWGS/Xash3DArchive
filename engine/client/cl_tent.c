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
#include "cl_tent.h"
#include "studio.h"

/*
==============================================================

TEMPENTS MANAGEMENT

==============================================================
*/
/*
==============
CL_ParseTempEntity

handle temp-entity messages
==============
*/
void CL_ParseTempEntity( sizebuf_t *msg )
{
	sizebuf_t	buf;
	byte	pbuf[256];
	int	iSize = BF_ReadByte( msg );
	vec3_t	pos;

	// parse user message into buffer
	BF_ReadBytes( msg, pbuf, iSize );

	// init a safe tempbuffer
	BF_Init( &buf, "TempEntity", pbuf, iSize );

	switch( BF_ReadByte( &buf ))
	{	
	case TE_SPARKS:
		pos[0] = BF_ReadCoord( &buf );
		pos[1] = BF_ReadCoord( &buf );
		pos[2] = BF_ReadCoord( &buf );
		CL_SparkShower( pos );
		break;
	default:
		clgame.dllFuncs.pfnTempEntityMessage( iSize, pbuf );
		break;
	}

	// throw warning
	if( BF_CheckOverflow( &buf )) MsgDev( D_WARN, "ParseTempEntity: bad message\n" );
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