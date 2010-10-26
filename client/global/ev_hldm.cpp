//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       ev_common.cpp - events common code
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "r_beams.h"
#include "r_efx.h"
#include "r_particle.h"
#include "r_tempents.h"
#include "pm_materials.h"
#include "pm_movevars.h"
#include "pm_shared.h"
#include "pm_defs.h"
#include "ev_hldm.h"
#include "event_args.h"
#include "hud.h"


Vector previousorigin;	// egon use this
float g_flApplyVel = 0.0f;	// for gauss-jumping
static int tracerCount[32];
void V_PunchAxis( int axis, float punch );
void VectorAngles( const float *forward, float *angles );

void EV_FireGlock1( struct event_args_s *args  );
void EV_FireGlock2( struct event_args_s *args  );
void EV_FireShotGunSingle( struct event_args_s *args  );
void EV_FireShotGunDouble( struct event_args_s *args  );
void EV_FireMP5( struct event_args_s *args  );
void EV_FireMP52( struct event_args_s *args  );
void EV_FirePython( struct event_args_s *args  );
void EV_FireGauss( struct event_args_s *args  );
void EV_SpinGauss( struct event_args_s *args  );
void EV_Crowbar( struct event_args_s *args  );
void EV_FireCrossbow( struct event_args_s *args  );
void EV_FireCrossbow2( struct event_args_s *args  );
void EV_FireRpg( struct event_args_s *args  );
void EV_EgonFire( struct event_args_s *args  );
void EV_EgonStop( struct event_args_s *args  );
void EV_HornetGunFire( struct event_args_s *args  );
void EV_TripmineFire( struct event_args_s *args  );
void EV_SnarkFire( struct event_args_s *args  );

#define VECTOR_CONE_1DEGREES	Vector( 0.00873, 0.00873, 0.00873 )
#define VECTOR_CONE_2DEGREES	Vector( 0.01745, 0.01745, 0.01745 )
#define VECTOR_CONE_3DEGREES	Vector( 0.02618, 0.02618, 0.02618 )
#define VECTOR_CONE_4DEGREES	Vector( 0.03490, 0.03490, 0.03490 )
#define VECTOR_CONE_5DEGREES	Vector( 0.04362, 0.04362, 0.04362 )
#define VECTOR_CONE_6DEGREES	Vector( 0.05234, 0.05234, 0.05234 )
#define VECTOR_CONE_7DEGREES	Vector( 0.06105, 0.06105, 0.06105 )
#define VECTOR_CONE_8DEGREES	Vector( 0.06976, 0.06976, 0.06976 )
#define VECTOR_CONE_9DEGREES	Vector( 0.07846, 0.07846, 0.07846 )
#define VECTOR_CONE_10DEGREES	Vector( 0.08716, 0.08716, 0.08716 )
#define VECTOR_CONE_15DEGREES	Vector( 0.13053, 0.13053, 0.13053 )
#define VECTOR_CONE_20DEGREES	Vector( 0.17365, 0.17365, 0.17365 )

// play a strike sound based on the texture that was hit by the attack traceline.  VecSrc/VecEnd are the
// original traceline endpoints used by the attacker, iBulletType is the type of bullet that hit the texture.
// returns volume of strike instrument (crowbar) to play
float EV_HLDM_PlayTextureSound( int idx, pmtrace_t *ptr, float *vecSrc, float *vecEnd, int iBulletType )
{
	// hit the world, try to play sound based on texture material type
	char chTextureType = CHAR_TEX_CONCRETE;
	float fvol;
	float fvolbar;
	char *rgsz[4];
	int cnt;
	float fattn = ATTN_NORM;
	int entity = 0;
	char *pTextureName;
	char texname[64];
	char szbuffer[64];

	entity = gEngfuncs.pEventAPI->EV_IndexFromTrace( ptr );

	// check if playtexture sounds movevar is set
	if( gEngfuncs.GetMaxClients() != 1 && gpMovevars->footsteps == 0 )
		return 0.0f;

	chTextureType = 0;

	// Player
	if ( entity >= 1 && entity <= gEngfuncs.GetMaxClients() )
	{
		// hit body
		chTextureType = CHAR_TEX_FLESH;
	}
	else if ( entity == 0 )
	{
		// get texture from entity or world (world is ent(0))
		pTextureName = (char *)gEngfuncs.pEventAPI->EV_TraceTexture( ptr->ent, vecSrc, vecEnd );
		
		if( pTextureName )
		{
			strcpy( texname, pTextureName );
			pTextureName = texname;

			// strip leading '-0' or '+0~' or '{' or '!'
			if ( *pTextureName == '-' || *pTextureName == '+' )
			{
				pTextureName += 2;
			}

			if ( *pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ' )
			{
				pTextureName++;
			}
			
			// '}}'
			strcpy( szbuffer, pTextureName );
			szbuffer[CBTEXTURENAMEMAX - 1] = 0;
				
			// get texture type
			chTextureType = PM_FindTextureType( szbuffer );	
		}
	}
	
	switch ( chTextureType )
	{
	case CHAR_TEX_CONCRETE:
	default:	fvol = 0.9; fvolbar = 0.6;
		rgsz[0] = "player/pl_step1.wav";
		rgsz[1] = "player/pl_step2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_METAL: fvol = 0.9; fvolbar = 0.3;
		rgsz[0] = "player/pl_metal1.wav";
		rgsz[1] = "player/pl_metal2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_DIRT:	fvol = 0.9; fvolbar = 0.1;
		rgsz[0] = "player/pl_dirt1.wav";
		rgsz[1] = "player/pl_dirt2.wav";
		rgsz[2] = "player/pl_dirt3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_VENT:	fvol = 0.5; fvolbar = 0.3;
		rgsz[0] = "player/pl_duct1.wav";
		rgsz[1] = "player/pl_duct1.wav";
		cnt = 2;
		break;
	case CHAR_TEX_GRATE: fvol = 0.9; fvolbar = 0.5;
		rgsz[0] = "player/pl_grate1.wav";
		rgsz[1] = "player/pl_grate4.wav";
		cnt = 2;
		break;
	case CHAR_TEX_TILE:	fvol = 0.8; fvolbar = 0.2;
		rgsz[0] = "player/pl_tile1.wav";
		rgsz[1] = "player/pl_tile3.wav";
		rgsz[2] = "player/pl_tile2.wav";
		rgsz[3] = "player/pl_tile4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_SLOSH: fvol = 0.9; fvolbar = 0.0;
		rgsz[0] = "player/pl_slosh1.wav";
		rgsz[1] = "player/pl_slosh3.wav";
		rgsz[2] = "player/pl_slosh2.wav";
		rgsz[3] = "player/pl_slosh4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_WOOD: fvol = 0.9; fvolbar = 0.2;
		rgsz[0] = "debris/wood1.wav";
		rgsz[1] = "debris/wood2.wav";
		rgsz[2] = "debris/wood3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_GLASS:
	case CHAR_TEX_COMPUTER:
		fvol = 0.8; fvolbar = 0.2;
		rgsz[0] = "debris/glass1.wav";
		rgsz[1] = "debris/glass2.wav";
		rgsz[2] = "debris/glass3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_FLESH:
		if ( iBulletType == BULLET_PLAYER_CROWBAR )
			return 0.0f; // crowbar already makes this sound
		fvol = 1.0;	fvolbar = 0.2;
		rgsz[0] = "weapons/bullet_hit1.wav";
		rgsz[1] = "weapons/bullet_hit2.wav";
		fattn = 1.0;
		cnt = 2;
		break;
	}

	// play material hit sound
	gEngfuncs.pEventAPI->EV_PlaySound( 0, ptr->endpos, CHAN_STATIC, rgsz[RANDOM_LONG(0, cnt-1)], fvol, fattn, 0, 96 + RANDOM_LONG( 0, 0xf ));
	return fvolbar;
}

char *EV_HLDM_DamageDecal( physent_t *pe )
{
	static char decalname[32];
	int idx;

	if ( pe->classnumber == 1 )
	{
		idx = RANDOM_LONG( 0, 2 );
		sprintf( decalname, "{break%i", idx + 1 );
	}
	else if ( pe->rendermode != kRenderNormal )
	{
		sprintf( decalname, "{bproof1" );
	}
	else
	{
		idx = RANDOM_LONG( 0, 4 );
		sprintf( decalname, "{shot%i", idx + 1 );
	}
	return decalname;
}

void EV_HLDM_GunshotDecalTrace( pmtrace_t *pTrace, char *decalName )
{
	int	iRand;
	physent_t *pe;

	g_pParticles->BulletImpactParticles( pTrace->endpos );

	iRand = RANDOM_LONG( 0, 0x7FFF );
	if ( iRand < ( 0x7fff / 2 )) // not every bullet makes a sound.
	{
		switch( iRand % 5 )
		{
		case 0:
			gEngfuncs.pEventAPI->EV_PlaySound( NULL, pTrace->endpos, 0, "weapons/ric1.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 1:
			gEngfuncs.pEventAPI->EV_PlaySound( NULL, pTrace->endpos, 0, "weapons/ric2.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 2:	
			gEngfuncs.pEventAPI->EV_PlaySound( NULL, pTrace->endpos, 0, "weapons/ric3.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 3:
			gEngfuncs.pEventAPI->EV_PlaySound( NULL, pTrace->endpos, 0, "weapons/ric4.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 4:
			gEngfuncs.pEventAPI->EV_PlaySound( NULL, pTrace->endpos, 0, "weapons/ric5.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			break;
		}
	}

	pe = gEngfuncs.pEventAPI->EV_GetPhysent( pTrace->ent );

	struct model_s *mdl = pe->model;

	// Only decal brush models such as the world etc.
	if( decalName && decalName[0] && pe && ( pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP ))
	{
		if ( CVAR_GET_FLOAT( "r_decals" ) )
		{
			g_pTempEnts->PlaceDecal( pTrace->endpos, gEngfuncs.pEventAPI->EV_IndexFromTrace( pTrace ), decalName );
		}
	}
}

void EV_HLDM_DecalGunshot( pmtrace_t *pTrace, int iBulletType )
{
	physent_t *pe;

	pe = gEngfuncs.pEventAPI->EV_GetPhysent( pTrace->ent );

	if ( pe && pe->solid == SOLID_BSP )
	{
		switch( iBulletType )
		{
		case BULLET_PLAYER_9MM:
		case BULLET_MONSTER_9MM:
		case BULLET_PLAYER_MP5:
		case BULLET_MONSTER_MP5:
		case BULLET_PLAYER_BUCKSHOT:
		case BULLET_PLAYER_357:
		default:
			// smoke and decal
			EV_HLDM_GunshotDecalTrace( pTrace, EV_HLDM_DamageDecal( pe ));
			break;
		}
	}
}

int EV_HLDM_CheckTracer( int idx, Vector vecSrc, Vector end, Vector forward, Vector right, int iBulletType, int iTracerFreq, int *tracerCount )
{
	int tracer = 0;
	BOOL player = (idx >= 1 && idx <= gEngfuncs.GetMaxClients()) ? true : false;

	if ( iTracerFreq != 0 && ( ( *tracerCount )++ % iTracerFreq ) == 0 )
	{
		Vector vecTracerSrc;

		if ( player )
		{
			Vector offset( 0, 0, -4 );

			// adjust tracer position for player
			vecTracerSrc = vecSrc + offset + right * 2 + forward * 16;
		}
		else
		{
			vecTracerSrc = vecSrc;
		}
		
		if ( iTracerFreq != 1 )	// guns that always trace also always decal
			tracer = 1;

		switch( iBulletType )
		{
		case BULLET_PLAYER_MP5:
		case BULLET_MONSTER_MP5:
		case BULLET_MONSTER_9MM:
		case BULLET_MONSTER_12MM:
		default:
			EV_CreateTracer( vecTracerSrc, end );
			break;
		}
	}
	return tracer;
}

/*
================
FireBullets

Go to the trouble of combining multiple pellets into a single damage call.
================
*/
void EV_HLDM_FireBullets( int idx, Vector forward, Vector right, Vector up, int cShots, Vector vecSrc, Vector vecDirShooting, float flDistance, int iBulletType, int iTracerFreq, int *tracerCount, float flSpreadX, float flSpreadY )
{
	pmtrace_t tr;
	int iShot;
	int tracer;
	
	for ( iShot = 1; iShot <= cShots; iShot++ )	
	{
		Vector vecDir, vecEnd;
			
		float x, y, z;
		// we randomize for the Shotgun.
		if ( iBulletType == BULLET_PLAYER_BUCKSHOT )
		{
			do {
				x = RANDOM_FLOAT( -0.5f, 0.5f ) + RANDOM_FLOAT( -0.5f, 0.5f );
				y = RANDOM_FLOAT( -0.5f, 0.5f ) + RANDOM_FLOAT( -0.5f, 0.5f );
				z = x * x + y * y;
			} while ( z > 1 );

			vecDir = vecDirShooting + x * flSpreadX * right + y * flSpreadY * up;
			vecEnd = vecSrc + flDistance * vecDir;
		}
		else
		{
			// But other guns already have their spread randomized in the synched spread.
			vecDir = vecDirShooting + flSpreadX * right + flSpreadY * up;
			vecEnd = vecSrc + flDistance * vecDir;
		}

		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );
	
		// Store off the old count
		gEngfuncs.pEventAPI->EV_PushPMStates();
	
		// Now add in all of the players.
		gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );	

		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr );

		tracer = EV_HLDM_CheckTracer( idx, vecSrc, tr.endpos, forward, right, iBulletType, iTracerFreq, tracerCount );

		// do damage, paint decals
		if ( tr.fraction != 1.0f )
		{
			switch( iBulletType )
			{
			default:
			case BULLET_PLAYER_9MM:		
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			case BULLET_PLAYER_MP5:		
				if ( !tracer )
				{
					EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
					EV_HLDM_DecalGunshot( &tr, iBulletType );
				}
				break;
			case BULLET_PLAYER_BUCKSHOT:
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			case BULLET_PLAYER_357:
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			}
		}
		gEngfuncs.pEventAPI->EV_PopPMStates();
	}
}

//======================
//	    GLOCK START
//======================
void EV_FireGlock1( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	int empty;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	
	idx = args->entindex;
	origin = args->origin;
	angles = args->angles;
	velocity = args->velocity;

	empty = args->bparam1;
	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( empty ? GLOCK_SHOOT_EMPTY : GLOCK_SHOOT, 2 );

		V_PunchAxis( 0, -2.0 );
	}

	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4 );

	EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/pl_gun3.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong( 0, 3 ) );

	EV_GetGunPosition( args, vecSrc, origin );
	
	vecAiming = forward;

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_9MM, 0, 0, args->fparam1, args->fparam2 );
}

void EV_FireGlock2( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	
	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t vecSpread;
	vec3_t up, right, forward;

	idx = args->entindex;
	origin = args->origin;
	angles = args->angles;
	velocity = args->velocity;

	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( GLOCK_SHOOT, 2 );

		V_PunchAxis( 0, -2.0 );
	}

	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4 );

	EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/pl_gun3.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong( 0, 3 ) );

	EV_GetGunPosition( args, vecSrc, origin );
	
	vecAiming = forward;

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_9MM, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );
	
}
//======================
//	   GLOCK END
//======================

//======================
//	  SHOTGUN START
//======================
void EV_FireShotGunDouble( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	int j;
	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t vecSpread;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	origin = args->origin;
	angles = args->angles;
	velocity = args->velocity;

	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shotgunshell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( SHOTGUN_FIRE2, 2 );
		V_PunchAxis( 0, -10.0 );
	}

	for ( j = 0; j < 2; j++ )
	{
		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 32, -12, 6 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHOTSHELL ); 
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/dbarrel1.wav", gEngfuncs.pfnRandomFloat(0.98, 1.0), ATTN_NORM, 0, 85 + gEngfuncs.pfnRandomLong( 0, 0x1f ) );

	EV_GetGunPosition( args, vecSrc, origin );
	vecAiming = forward;

	if ( gEngfuncs.GetMaxClients() > 1 )
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 8, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &tracerCount[idx-1], 0.17365, 0.04362 );
	}
	else
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 12, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &tracerCount[idx-1], 0.08716, 0.08716 );
	}
}

void EV_FireShotGunSingle( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	
	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t vecSpread;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	origin = args->origin;
	angles = args->angles;
	velocity = args->velocity;

	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shotgunshell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( SHOTGUN_FIRE, 2 );

		V_PunchAxis( 0, -5.0 );
	}

	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 32, -12, 6 );

	EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHOTSHELL ); 

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/sbarrel1.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong( 0, 0x1f ) );

	EV_GetGunPosition( args, vecSrc, origin );
	vecAiming = forward;

	if ( gEngfuncs.GetMaxClients() > 1 )
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 4, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &tracerCount[idx-1], 0.08716, 0.04362 );
	}
	else
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 6, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &tracerCount[idx-1], 0.08716, 0.08716 );
	}
}
//======================
//	   SHOTGUN END
//======================

//======================
//	    MP5 START
//======================
void EV_FireMP5( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	origin = args->origin;
	angles = args->angles;
	velocity = args->velocity;

	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shell.mdl");// brass shell
	
	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( MP5_FIRE1 + gEngfuncs.pfnRandomLong(0,2), 2 );

		V_PunchAxis( 0, gEngfuncs.pfnRandomFloat( -2, 2 ) );
	}

	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4 );

	EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 

	switch( gEngfuncs.pfnRandomLong( 0, 1 ) )
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/hks1.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/hks2.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
		break;
	}

	EV_GetGunPosition( args, vecSrc, origin );
	vecAiming = forward;

	if ( gEngfuncs.GetMaxClients() > 1 )
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_MP5, 2, &tracerCount[idx-1], args->fparam1, args->fparam2 );
	}
	else
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_MP5, 2, &tracerCount[idx-1], args->fparam1, args->fparam2 );
	}
}

// We only predict the animation and sound
// The grenade is still launched from the server.
void EV_FireMP52( event_args_t *args )
{
	int idx;
	vec3_t origin;
	
	idx = args->entindex;
	origin = args->origin;

	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( MP5_LAUNCH, 2 );
		V_PunchAxis( 0, -10 );
	}
	
	switch( gEngfuncs.pfnRandomLong( 0, 1 ) )
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/glauncher.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/glauncher2.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
		break;
	}
}
//======================
//		 MP5 END
//======================

//======================
//	   PHYTON START 
//	     ( .357 )
//======================
void EV_FirePython( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	origin = args->origin;
	angles = args->angles;
	velocity = args->velocity;

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Python uses different body in multiplayer versus single player
		int multiplayer = gEngfuncs.GetMaxClients() == 1 ? 0 : 1;

		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( PYTHON_FIRE1, multiplayer ? 1 : 0 );

		V_PunchAxis( 0, -10.0 );
	}

	switch( gEngfuncs.pfnRandomLong( 0, 1 ) )
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/357_shot1.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), ATTN_NORM, 0, PITCH_NORM );
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/357_shot2.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), ATTN_NORM, 0, PITCH_NORM );
		break;
	}

	EV_GetGunPosition( args, vecSrc, origin );
	
	vecAiming = forward;

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_357, 0, 0, args->fparam1, args->fparam2 );
}
//======================
//	    PHYTON END 
//	     ( .357 )
//======================

//======================
//	   GAUSS START 
//======================
#define GAUSS_PRIMARY_CHARGE_VOLUME	256	// how loud gauss is while charging
#define GAUSS_PRIMARY_FIRE_VOLUME	450	// how loud gauss is when discharged
#define SND_CHANGE_PITCH		(1<<7)	// duplicated in protocol.h change sound pitch

void EV_SpinGauss( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	int iSoundState = 0;

	int pitch;

	idx = args->entindex;
	origin = args->origin;
	angles = args->angles;
	velocity = args->velocity;

	pitch = args->iparam1;

	iSoundState = args->bparam1 ? SND_CHANGE_PITCH : 0;

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "ambience/pulsemachine.wav", 1.0, ATTN_NORM, iSoundState, pitch );

	if( EV_IsLocal( idx ))
	{
		cl_entity_t *view = GetViewModel();

		// change framerate up from 1.0 to 2.5x
		view->curstate.framerate = (float)pitch / PITCH_NORM;
	}
}

/*
==============================
EV_StopPreviousGauss

==============================
*/
void EV_StopPreviousGauss( int idx )
{
	// Make sure we don't have a gauss spin event in the queue for this guy
	gEngfuncs.pEventAPI->EV_KillEvents( idx, "events/gaussspin.sc" );
	gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_WEAPON, "ambience/pulsemachine.wav" );
}

void EV_FireGauss( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	float flDamage = args->fparam1;
	int primaryfire = args->bparam1;

	int m_fPrimaryFire = args->bparam1;
	int m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;
	Vector vecSrc, vecDest;
  
	cl_entity_t *pentIgnore;
	pmtrace_t tr, beam_tr;
	float flMaxFrac = 1.0;
	int nTotal = 0;                             
	int fHasPunched = 0;
	int fFirstBeam = 1;
	int nMaxHits = 10;
	physent_t *pEntity;
	int m_iBeam, m_iGlow, m_iBalls;
	Vector forward;

	idx = args->entindex;
	origin = args->origin;
	angles = args->angles;
	velocity = args->velocity;

	if ( args->bparam2 )
	{
		EV_StopPreviousGauss( idx );
		return;
	}

//	gEngfuncs.Con_Printf( "firing gauss with %f\n", flDamage );
	EV_GetGunPosition( args, vecSrc, origin );

	m_iBeam = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/smoke.spr" );
	m_iBalls = m_iGlow = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/hotglow.spr" );
	
	AngleVectors( angles, forward, NULL, NULL );
          
	vecDest = vecSrc + forward * 8192;
	
	if ( EV_IsLocal( idx ) )
	{
		V_PunchAxis( 0, -2.0 );
		gEngfuncs.pEventAPI->EV_WeaponAnimation( GAUSS_FIRE2, 2 );

		if ( m_fPrimaryFire == false )
			 g_flApplyVel = flDamage;

	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/gauss2.wav", 0.5 + flDamage * (1.0 / 400.0), ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 0x1f ) );

	while (flDamage > 10 && nMaxHits > 0 )
	{
		nMaxHits--;

		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );
		
		// Store off the old count
		gEngfuncs.pEventAPI->EV_PushPMStates();
	
		// Now add in all of the players.
		gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );	

		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc, vecDest, PM_STUDIO_BOX, -1, &tr );

		gEngfuncs.pEventAPI->EV_PopPMStates();
                  
		if ( tr.allsolid )
			break;

		if (fFirstBeam)
		{
			if ( EV_IsLocal( idx ) )
			{
				// Add muzzle flash to current weapon model
				EV_MuzzleFlash();
			}
			fFirstBeam = 0;

			g_pViewRenderBeams->CreateBeamEntPoint(
				idx | 0x1000,
				tr.endpos, m_iBeam,
				0.1,
				m_fPrimaryFire ? 1.0 : 2.5,
				0.0,
				m_fPrimaryFire ? 128.0 : flDamage,
				0,
				0,
				0,
				m_fPrimaryFire ? 255 : 255,	// g-cont. ???
				m_fPrimaryFire ? 128 : 255,
				m_fPrimaryFire ? 0 : 255
			);
		}
		else
		{
			g_pViewRenderBeams->CreateBeamPoints(
				vecSrc,
				tr.endpos,
				m_iBeam,
				0.1,
				m_fPrimaryFire ? 1.0 : 2.5,
				0.0f,
				m_fPrimaryFire ? 128.0 : flDamage,
				0,
				0,
				0,
				m_fPrimaryFire ? 255 : 255,	// g-cont. ???
				m_fPrimaryFire ? 128 : 255,
				m_fPrimaryFire ? 0 : 255
			);
		}

		pEntity = gEngfuncs.pEventAPI->EV_GetPhysent( tr.ent );
		if ( pEntity == NULL )
			break;
                    
		if ( pEntity->solid == SOLID_BSP )
		{
			float n;

			pentIgnore = NULL;

			n = -DotProduct( tr.plane.normal, forward );

			if ( n < 0.5f ) // 60 degrees	
			{
				// gEngfuncs.Con_Printf( "reflect %f\n", n );
				// reflect
				Vector r;
			
				r = forward + ( tr.plane.normal * (2.0f * n ));

				flMaxFrac = flMaxFrac - tr.fraction;
				
				forward = r;

				vecSrc = tr.endpos + forward * 8.0f;
				vecDest = vecSrc + forward * 8192.0f;

				g_pTempEnts->TempSprite( tr.endpos, g_vecZero, 0.2, m_iGlow, kRenderGlow, kRenderFxNoDissipation, flDamage * n / 255.0, flDamage * n * 0.5 * 0.1, FTENT_FADEOUT );
                                        
				Vector fwd;
				fwd = tr.endpos + tr.plane.normal;

				g_pTempEnts->Sprite_Trail( TE_SPRITETRAIL, tr.endpos, fwd, m_iBalls, 3, 0.1, RANDOM_FLOAT( 10, 20 ) / 100.0, 100,
					255, 100 );
                                        
				// lose energy
				if ( n == 0 )
				{
					n = 0.1f;
				}
				
				flDamage = flDamage * (1 - n);

			}
			else
			{
				// tunnel
				EV_HLDM_DecalGunshot( &tr, BULLET_MONSTER_12MM );

				g_pTempEnts->TempSprite( tr.endpos, g_vecZero, 1.0, m_iGlow, kRenderGlow, kRenderFxNoDissipation, flDamage / 255.0, 6.0, FTENT_FADEOUT );

				// limit it to one hole punch
				if (fHasPunched)
				{
					break;
				}
				fHasPunched = 1;
				
				// try punching through wall if secondary attack (primary is incapable of breaking through)
				if ( !m_fPrimaryFire )
				{
					Vector start;
					
					start = tr.endpos + forward * 8.0f;

					// Store off the old count
					gEngfuncs.pEventAPI->EV_PushPMStates();
						
					// Now add in all of the players.
					gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );

					gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
					gEngfuncs.pEventAPI->EV_PlayerTrace( start, vecDest, PM_STUDIO_BOX, -1, &beam_tr );					

					if ( !beam_tr.allsolid )
					{
						Vector delta;
						float n;

						// trace backwards to find exit point

						gEngfuncs.pEventAPI->EV_PlayerTrace( beam_tr.endpos, tr.endpos, PM_STUDIO_BOX, -1, &beam_tr );

						delta = beam_tr.endpos - tr.endpos;
						
						n = delta.Length();

						if ( n < flDamage )
						{
							if (n == 0)
								n = 1;
							flDamage -= n;

							// absorption balls
							{
								Vector fwd = tr.endpos - forward;
								g_pTempEnts->Sprite_Trail( TE_SPRITETRAIL, tr.endpos, fwd, m_iBalls, 3, 0.1, RANDOM_FLOAT( 10, 20 ) / 100.0, 100,
									255, 100 );
							}

							EV_HLDM_DecalGunshot( &beam_tr, BULLET_MONSTER_12MM );
							
							g_pTempEnts->TempSprite( beam_tr.endpos, g_vecZero, 0.1, m_iGlow, kRenderGlow, kRenderFxNoDissipation, flDamage / 255.0, 6.0, FTENT_FADEOUT );
			
							// balls
							{
								Vector fwd = beam_tr.endpos - forward;
								g_pTempEnts->Sprite_Trail( TE_SPRITETRAIL, beam_tr.endpos, fwd, m_iBalls, (int)(flDamage * 0.3), 0.1, RANDOM_FLOAT( 10, 20 ) / 100.0, 200, 255, 40 );
							}
							
							vecSrc = beam_tr.endpos + forward;
						}
					}
					else
					{
						flDamage = 0;
					}
					gEngfuncs.pEventAPI->EV_PopPMStates();
				}
				else
				{
					if ( m_fPrimaryFire )
					{
						// slug doesn't punch through ever with primary 
						// fire, so leave a little glowy bit and make some balls
						g_pTempEnts->TempSprite( tr.endpos, g_vecZero, 0.2, m_iGlow, kRenderGlow, kRenderFxNoDissipation, 200.0 / 255.0, 0.3, FTENT_FADEOUT );
						
						{
							Vector fwd;
							fwd = tr.endpos + tr.plane.normal;
							g_pTempEnts->Sprite_Trail( TE_SPRITETRAIL, tr.endpos, fwd, m_iBalls, 8, 0.6, RANDOM_FLOAT( 10, 20 ) / 100.0, 100,
								255, 200 );
						}
					}

					flDamage = 0;
				}
			}
		}
		else
		{
			vecSrc = tr.endpos + forward;
		}
	}
}
//======================
//	   GAUSS END 
//======================

//======================
//	   CROWBAR START
//======================
int g_iSwing;

//Only predict the miss sounds, hit sounds are still played 
//server side, so players don't get the wrong idea.
void EV_Crowbar( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	idx = args->entindex;
	origin = args->origin;
	
	//Play Swing sound
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, PITCH_NORM); 

	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( CROWBAR_ATTACK1MISS, 1 );

		switch( (g_iSwing++) % 3 )
		{
			case 0:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( CROWBAR_ATTACK1MISS, 1 ); break;
			case 1:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( CROWBAR_ATTACK2MISS, 1 ); break;
			case 2:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( CROWBAR_ATTACK3MISS, 1 ); break;
		}
	}
}
//======================
//	   CROWBAR END 
//======================

//======================
//	  CROSSBOW START
//======================
//=====================
// EV_BoltCallback
// This function is used to correct the origin and angles 
// of the bolt, so it looks like it's stuck on the wall.
//=====================
void EV_BoltCallback ( struct tempent_s *ent, float frametime, float currenttime )
{
	ent->entity.origin = ent->entity.baseline.vuser1;
	ent->entity.angles = ent->entity.baseline.vuser2;
}

void EV_FireCrossbow2( event_args_t *args )
{
	vec3_t vecSrc, vecEnd;
	vec3_t up, right, forward;
	pmtrace_t tr;

	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	idx = args->entindex;
	origin = args->origin;
	angles = args->angles;

	velocity = args->velocity;
	
	AngleVectors( angles, forward, right, up );

	EV_GetGunPosition( args, vecSrc, origin );

	vecEnd = vecSrc + forward * 8192.0f;

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/xbow_fire1.wav", 1, ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0,0xF) );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_ITEM, "weapons/xbow_reload1.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0,0xF) );

	if ( EV_IsLocal( idx ) )
	{
		if ( args->iparam1 )
			gEngfuncs.pEventAPI->EV_WeaponAnimation( CROSSBOW_FIRE1, 1 );
		else if ( args->iparam2 )
			gEngfuncs.pEventAPI->EV_WeaponAnimation( CROSSBOW_FIRE3, 1 );
	}

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );	
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr );
	
	//We hit something
	if ( tr.fraction < 1.0 )
	{
		physent_t *pe = gEngfuncs.pEventAPI->EV_GetPhysent( tr.ent ); 

		//Not the world, let's assume we hit something organic ( dog, cat, uncle joe, etc ).
		if ( pe->solid != SOLID_BSP )
		{
			switch( gEngfuncs.pfnRandomLong(0,1) )
			{
			case 0:
				gEngfuncs.pEventAPI->EV_PlaySound( idx, tr.endpos, CHAN_BODY, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
			case 1:
				gEngfuncs.pEventAPI->EV_PlaySound( idx, tr.endpos, CHAN_BODY, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
			}
		}
		//Stick to world but don't stick to glass, it might break and leave the bolt floating. It can still stick to other non-transparent breakables though.
		else if ( pe->rendermode == kRenderNormal ) 
		{
			gEngfuncs.pEventAPI->EV_PlaySound( 0, tr.endpos, CHAN_BODY, "weapons/xbow_hit1.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, PITCH_NORM );
		
			//Not underwater, do some sparks...
			if ( gEngfuncs.PM_PointContents( tr.endpos, NULL ) != CONTENTS_WATER)
				 g_pTempEnts->SparkShower( tr.endpos );

			vec3_t vBoltAngles;
			int iModelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( "models/crossbow_bolt.mdl" );

			VectorAngles( forward, vBoltAngles );

			TEMPENTITY *bolt = g_pTempEnts->TempModel( tr.endpos - forward * 10, Vector( 0, 0, 0), vBoltAngles , 5, iModelIndex, TE_BOUNCE_NULL );
			
			if ( bolt )
			{
				bolt->flags |= ( FTENT_CLIENTCUSTOM ); //So it calls the callback function.
				bolt->entity.baseline.vuser1 = tr.endpos - forward * 10; // Pull out a little bit
				bolt->entity.baseline.vuser2 = vBoltAngles; //Look forward!
				bolt->callback = EV_BoltCallback; //So we can set the angles and origin back. (Stick the bolt to the wall)
			}
		}
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();
}

// TODO: Fully predict the fliying bolt.
void EV_FireCrossbow( event_args_t *args )
{
	int idx;
	vec3_t origin;

	idx = args->entindex;
	origin = args->origin;
	
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/xbow_fire1.wav", 1, ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0,0xF) );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_ITEM, "weapons/xbow_reload1.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0,0xF) );

	//Only play the weapon anims if I shot it. 
	if ( EV_IsLocal( idx ) )
	{
		if ( args->iparam1 )
			gEngfuncs.pEventAPI->EV_WeaponAnimation( CROSSBOW_FIRE1, 1 );
		else if ( args->iparam2 )
			gEngfuncs.pEventAPI->EV_WeaponAnimation( CROSSBOW_FIRE3, 1 );

		V_PunchAxis( 0, -2.0 );
	}
}
//======================
//	   CROSSBOW END 
//======================

//======================
//	    RPG START 
//======================
void EV_FireRpg( event_args_t *args )
{
	int idx;
	vec3_t origin;

	idx = args->entindex;
	origin = args->origin;
	
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/rocketfire1.wav", 0.9, ATTN_NORM, 0, PITCH_NORM );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_ITEM, "weapons/glauncher.wav", 0.7, ATTN_NORM, 0, PITCH_NORM );

	//Only play the weapon anims if I shot it. 
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( RPG_FIRE2, 1 );
	
		V_PunchAxis( 0, -5.0 );
	}
}
//======================
//	     RPG END 
//======================

//======================
//	    EGON START 
//======================
int g_fireAnims1[] = { EGON_FIRE1, EGON_FIRE2, EGON_FIRE3, EGON_FIRE4 };
int g_fireAnims2[] = { EGON_ALTFIRECYCLE };

enum EGON_FIRESTATE { FIRE_OFF = 0, FIRE_CHARGE };
enum EGON_FIREMODE { FIRE_NARROW = 0, FIRE_WIDE };

#define EGON_PULSE_INTERVAL		0.1
#define EGON_DISCHARGE_INTERVAL	0.1
#define EGON_PRIMARY_VOLUME		450
#define EGON_BEAM_SPRITE		"sprites/xbeam1.spr"
#define EGON_FLARE_SPRITE		"sprites/XSpark1.spr"
#define EGON_SOUND_OFF		"weapons/egon_off1.wav"
#define EGON_SOUND_RUN		"weapons/egon_run3.wav"
#define EGON_SOUND_STARTUP		"weapons/egon_windup2.wav"

Beam_t *m_pBeam = NULL;
Beam_t *m_pNoise = NULL;

TEMPENTITY *m_pEndFlare = NULL;

void EV_EgonFire( event_args_t *args )
{
	int idx, iFireState, iFireMode;
	vec3_t origin;

	idx = args->entindex;
	origin = args->origin;
	iFireState = args->iparam1;
	iFireMode = args->iparam2;
	int iStartup = args->bparam1;
	int m_iFlare = gEngfuncs.pEventAPI->EV_FindModelIndex( EGON_FLARE_SPRITE );

	if ( iStartup )
	{
		if ( iFireMode == FIRE_WIDE )
			gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, EGON_SOUND_STARTUP, 0.98f, ATTN_NORM, 0, 125 );
		else gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, EGON_SOUND_STARTUP, 0.9f, ATTN_NORM, 0, 100 );
	}
	else
	{
		if ( iFireMode == FIRE_WIDE )
			gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_STATIC, EGON_SOUND_RUN, 0.98f, ATTN_NORM, 0, 125 );
		else gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_STATIC, EGON_SOUND_RUN, 0.9f, ATTN_NORM, 0, 100 );
	}

	// Only play the weapon anims if I shot it.
	if ( EV_IsLocal( idx ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation ( g_fireAnims1[ gEngfuncs.pfnRandomLong( 0, 3 ) ], 1 );

	if ( iStartup == 1 && EV_IsLocal( idx ) && !m_pBeam && !m_pNoise && cl_lw->value ) //Adrian: Added the cl_lw check for those lital people that hate weapon prediction.
	{
		Vector vecSrc, vecEnd, origin, angles, forward;
		pmtrace_t tr;

		cl_entity_t *pl = GetEntityByIndex( idx );

		if ( pl )
		{
			angles = gHUD.m_vecAngles;

			AngleVectors( angles, forward, NULL, NULL );

			EV_GetGunPosition( args, vecSrc, pl->origin );
		
			vecEnd = vecSrc + forward * 2048;
				
			gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );	
				
			// Store off the old count
			gEngfuncs.pEventAPI->EV_PushPMStates();

			// Now add in all of the players.
			gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );	

			gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
			gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr );

			gEngfuncs.pEventAPI->EV_PopPMStates();

			int iBeamModelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( EGON_BEAM_SPRITE );

			float r = 0.5f;	// g-cont. skip check for IEngineStudio.isHardware()
			float g = 0.5f;
			float b = 125.0f;
	
			if ( iFireMode == FIRE_WIDE )
			{
				m_pBeam = g_pViewRenderBeams->CreateBeamEntPoint(
					idx | 0x1000,	// end entity & attachment
					tr.endpos,	// start pos (sic!)
					iBeamModelIndex,	// beamSprite
					99999,		// life
					4.0,		// width
					2.0,		// amplitude (noise)
					100,		// brightness
					55,		// scrollSpeed
					0,		// startframe
					0,		// framerate
					r, g, b );	// color

				if ( m_pBeam ) m_pBeam->SetFlags( FBEAM_SINENOISE );

				m_pNoise = g_pViewRenderBeams->CreateBeamEntPoint(
					idx | 0x1000,	// end entity & attachment
					tr.endpos,	// start pos (sic!)
					iBeamModelIndex,	// beamSprite
					99999,		// life
					5.5,		// width
					0.08,		// amplitude (noise)
					100,		// brightness
					2.5,		// scrollSpeed
					0,		// startframe
					0,		// framerate
					50, 50, 255 );	// color
			}
			else
			{
				m_pBeam = g_pViewRenderBeams->CreateBeamEntPoint(
					idx | 0x1000,	// end entity & attachment
					tr.endpos,	// start pos (sic!)
					iBeamModelIndex,	// beamSprite
					99999,		// life
					1.5,		// width
					0.5,		// amplitude (noise)
					100,		// brightness
					5.5,		// scrollSpeed
					0,		// startframe
					0,		// framerate
					r, g, b );	// color

				if ( m_pBeam ) m_pBeam->SetFlags( FBEAM_SINENOISE );

				m_pNoise = g_pViewRenderBeams->CreateBeamEntPoint(
					idx | 0x1000,	// end entity & attachment
					tr.endpos,	// start pos (sic!)
					iBeamModelIndex,	// beamSprite
					99999,		// life
					5.5,		// width
					0.2,		// amplitude (noise)
					100,		// brightness
					2.5,		// scrollSpeed
					0,		// startframe
					0,		// framerate
					80, 120, 255 );	// color
			}

                     	m_pEndFlare = g_pTempEnts->TempSprite( tr.endpos, g_vecZero, 1.0, m_iFlare, kRenderGlow, kRenderFxNoDissipation, 1.0, 9999, FTENT_SPRCYCLE );
		}
	}
}

void EV_EgonStop( event_args_t *args )
{
	int idx;
	vec3_t origin;

	idx = args->entindex;
	origin = args->origin;

	gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_STATIC, EGON_SOUND_RUN );
	
	if ( args->iparam1 )
		 gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, EGON_SOUND_OFF, 0.98, ATTN_NORM, 0, 100 );

	if ( EV_IsLocal( idx ) ) 
	{
		int iFireMode = args->iparam2;
	
		if ( m_pBeam )
		{
			m_pBeam->die = 0.0f;
			m_pBeam = NULL;
		}

		if ( m_pNoise )
		{
			m_pNoise->die = 0.0f;
			m_pNoise = NULL;
		}

		if ( m_pEndFlare )
		{
			if ( iFireMode == FIRE_WIDE )
			{
				// m_pSprite->Expand( 10, 500 );
				m_pEndFlare->flags = FTENT_SCALE|FTENT_FADEOUT;
				m_pEndFlare->fadeSpeed = 3.0f;
			}
			else
			{
				// UTIL_Remove( m_pSprite );
				m_pEndFlare->die = 0.0f;
			}
			m_pEndFlare = NULL;
		}

		// g-cont. to prevent cyclyc playing fire anims
		gEngfuncs.pEventAPI->EV_WeaponAnimation( EGON_IDLE1, 1 );
	}
}

void EV_UpdateBeams ( void )
{
	if ( !m_pBeam && !m_pNoise ) return;
	
	Vector forward, vecSrc, vecEnd, origin, angles;
	pmtrace_t tr;
	float timedist;

	cl_entity_t *pthisplayer = GetLocalPlayer();
	int idx = pthisplayer->index;
	
	// Get our exact viewangles from engine
	GetViewAngles( angles );

	origin = previousorigin;	// HUD_GetLastOrg

	AngleVectors( angles, forward, NULL, NULL );

	vecSrc = origin;

	vecEnd = vecSrc + forward * 2048;
	
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );	

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );	

	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr );

	gEngfuncs.pEventAPI->EV_PopPMStates();

	// HACKHACK: extract firemode from beamWidth
	int iFireMode = ( m_pBeam->width == 4.0 ) ? FIRE_WIDE : FIRE_NARROW;

	// calc timedelta for beam effects
	switch( iFireMode )
	{
	case FIRE_NARROW:
		if ( m_pBeam->m_flDmgTime < GetClientTime() )
		{
			m_pBeam->m_flDmgTime = GetClientTime() + EGON_PULSE_INTERVAL;
		}
		timedist = ( m_pBeam->m_flDmgTime - GetClientTime() ) / EGON_DISCHARGE_INTERVAL;
		break;
	case FIRE_WIDE:
		if ( m_pBeam->m_flDmgTime < GetClientTime() )
		{
			m_pBeam->m_flDmgTime = GetClientTime() + EGON_PULSE_INTERVAL;
		}
		timedist = ( m_pBeam->m_flDmgTime - GetClientTime() ) / EGON_DISCHARGE_INTERVAL;
		break;
	}

	// clamp and inverse
	timedist = bound( 0.0f, timedist, 1.0f );
	timedist = 1.0f - timedist;

	m_pBeam->SetEndPos( tr.endpos );
	m_pBeam->SetBrightness( 255 - ( timedist * 180 ));
	m_pBeam->SetWidth( 40 - ( timedist * 20 ));
	m_pBeam->die = GetClientTime() + 0.1f; // We keep it alive just a little bit forward in the future, just in case.

	if ( iFireMode == FIRE_WIDE )
		m_pBeam->SetColor( 30 + (25 * timedist), 30  + (30 * timedist), 64 + 80 * fabs( sin( GetClientTime() * 10 )));
	else
		m_pBeam->SetColor( 60 + (25 * timedist), 120 + (30 * timedist), 64 + 80 * fabs( sin( GetClientTime() * 10 )));

	m_pNoise->SetEndPos( tr.endpos );
	m_pNoise->die = GetClientTime() + 0.1f; // We keep it alive just a little bit forward in the future, just in case.

	if( m_pEndFlare )
	{
		m_pEndFlare->entity.origin = tr.endpos;
		m_pEndFlare->die = GetClientTime() + 0.1f;
	}
}
//======================
//	    EGON END 
//======================

//======================
//	   HORNET START
//======================
void EV_HornetGunFire( event_args_t *args )
{
	int idx, iFireMode;
	vec3_t origin, angles, vecSrc, forward, right, up;

	idx = args->entindex;
	origin = args->origin;
	angles = args->angles;
	iFireMode = args->iparam1;

	//Only play the weapon anims if I shot it.
	if ( EV_IsLocal( idx ) )
	{
		V_PunchAxis( 0, gEngfuncs.pfnRandomLong ( 0, 2 ) );
		gEngfuncs.pEventAPI->EV_WeaponAnimation ( HGUN_SHOOT, 1 );
	}

	switch ( gEngfuncs.pfnRandomLong ( 0 , 2 ) )
	{
		case 0:	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "agrunt/ag_fire1.wav", 1, ATTN_NORM, 0, 100 );	break;
		case 1:	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "agrunt/ag_fire2.wav", 1, ATTN_NORM, 0, 100 );	break;
		case 2:	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "agrunt/ag_fire3.wav", 1, ATTN_NORM, 0, 100 );	break;
	}
}
//======================
//	   HORNET END
//======================

//======================
//	   TRIPMINE START
//======================
//We only check if it's possible to put a trip mine
//and if it is, then we play the animation. Server still places it.
void EV_TripmineFire( event_args_t *args )
{
	int idx;
	vec3_t vecSrc, angles, view_ofs, forward;
	pmtrace_t tr;

	idx = args->entindex;
	vecSrc = args->origin;
	angles = args->angles;

	AngleVectors ( angles, forward, NULL, NULL );
		
	if ( !EV_IsLocal ( idx ) )
		return;

	// Grab predicted result for local player
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight( view_ofs );

	vecSrc = vecSrc + view_ofs;

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );	
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc, vecSrc + forward * 128, PM_NORMAL, -1, &tr );

	// Hit something solid
	if ( tr.fraction < 1.0 )
		 gEngfuncs.pEventAPI->EV_WeaponAnimation ( TRIPMINE_DRAW, 0 );
	
	gEngfuncs.pEventAPI->EV_PopPMStates();
}
//======================
//	   TRIPMINE END
//======================

//======================
//	   SQUEAK START
//======================
void EV_SnarkFire( event_args_t *args )
{
	int idx;
	vec3_t vecSrc, angles, view_ofs, forward;
	pmtrace_t tr;

	idx = args->entindex;
	vecSrc = args->origin;
	angles = args->angles;

	AngleVectors ( angles, forward, NULL, NULL );
		
	if ( !EV_IsLocal ( idx ) )
		return;
	
	if ( args->ducking )
		vecSrc = vecSrc - ( VEC_HULL_MIN - VEC_DUCK_HULL_MIN );
	
	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );	
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc + forward * 20, vecSrc + forward * 64, PM_NORMAL, -1, &tr );

	//Find space to drop the thing.
	if ( tr.allsolid == 0 && tr.startsolid == 0 && tr.fraction > 0.25 )
		 gEngfuncs.pEventAPI->EV_WeaponAnimation ( SQUEAK_THROW, 0 );
	
	gEngfuncs.pEventAPI->EV_PopPMStates();
}
//======================
//	   SQUEAK END
//======================


//======================
//	   LASERSPOT START
//======================
TEMPENTITY *m_pLaserSpot = NULL;

void EV_UpdateLaserSpot( void )
{
	cl_entity_t	*m_pPlayer = GetLocalPlayer();
	cl_entity_t	*m_pWeapon = GetViewModel();

	if( !m_pPlayer ) return;

	if( m_pPlayer->curstate.effects & EF_LASERSPOT && !m_pLaserSpot && cl_lw->value )
	{
		// create laserspot
		int m_iSpotModel = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/laserdot.spr" );
	
		m_pLaserSpot = g_pTempEnts->TempSprite( g_vecZero, g_vecZero, 1.0, m_iSpotModel, kRenderGlow, kRenderFxNoDissipation, 1.0, 9999, FTENT_SPRCYCLE );
		if( !m_pLaserSpot ) return;

		m_pLaserSpot->entity.baseline.rendercolor.r = 200;
		m_pLaserSpot->entity.baseline.rendercolor.g = 12;
		m_pLaserSpot->entity.baseline.rendercolor.b = 12;
//		gEngfuncs.Con_Printf( "CLaserSpot::Create()\n" );
	}
	else if( !( m_pPlayer->curstate.effects & EF_LASERSPOT ) && m_pLaserSpot )
	{
		// destroy laserspot
//		gEngfuncs.Con_Printf( "CLaserSpot::Killed()\n" );
		m_pLaserSpot->die = 0.0f;
		m_pLaserSpot = NULL;
		return;
	}
	else if( !m_pLaserSpot )
	{
		// inactive
		return;		
	}

	ASSERT( m_pLaserSpot != NULL );

	Vector forward, vecSrc, vecEnd, origin, angles;
	pmtrace_t tr;

#if 1
	GetViewAngles( angles );	// viewmodel doesn't have attachment
	origin = m_pWeapon->origin;
#else
	// TEST: give viewmodel first attachment
	origin = m_pWeapon->origin + pEnt->attachment_origin[0];
	angles = pEnt->attachment_angles[0];	// already include viewmodel angles
#endif
	AngleVectors( angles, forward, NULL, NULL );

	vecSrc = origin;
	vecEnd = vecSrc + forward * 8192;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );	
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers ( m_pPlayer->index - 1 );	

	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr );
	gEngfuncs.pEventAPI->EV_PopPMStates();

	// update laserspot endpos
	m_pLaserSpot->entity.origin = tr.endpos;
	m_pLaserSpot->die = GetClientTime() + 0.1f;
}
//======================
//	   LASERSPOT END
//======================


void EV_TrainPitchAdjust( event_args_t *args )
{
	int idx;
	vec3_t origin;

	unsigned short us_params;
	int noise;
	float m_flVolume;
	int pitch;
	int stop;
	
	char sz[ 256 ];

	idx = args->entindex;
	
	origin = args->origin;

	us_params = (unsigned short)args->iparam1;
	stop	  = args->bparam1;

	m_flVolume	= (float)(us_params & 0x003f)/40.0;
	noise		= (int)(((us_params) >> 12 ) & 0x0007);
	pitch		= (int)( 10.0 * (float)( ( us_params >> 6 ) & 0x003f ) );

	switch ( noise )
	{
	case 1: strcpy( sz, "plats/ttrain1.wav"); break;
	case 2: strcpy( sz, "plats/ttrain2.wav"); break;
	case 3: strcpy( sz, "plats/ttrain3.wav"); break; 
	case 4: strcpy( sz, "plats/ttrain4.wav"); break;
	case 5: strcpy( sz, "plats/ttrain6.wav"); break;
	case 6: strcpy( sz, "plats/ttrain7.wav"); break;
	default:
		// no sound
		strcpy( sz, "" );
		return;
	}

	if ( stop )
	{
		gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_STATIC, sz );
	}
	else
	{
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_STATIC, sz, m_flVolume, ATTN_NORM, SND_CHANGE_PITCH, pitch );
	}
}