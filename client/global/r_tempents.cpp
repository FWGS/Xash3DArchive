//=======================================================================
//			Copyright XashXT Group 2010 ©
//		     r_tempents.cpp - tempentity management
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "studio_event.h"
#include "triangle_api.h"
#include "effects_api.h"
#include "entity_types.h"
#include "pm_movevars.h"
#include "r_particle.h"
#include "r_tempents.h"
#include "pm_defs.h"
#include "ev_hldm.h"
#include "r_beams.h"
#include "hud.h"

extern TEMPENTITY *m_pEndFlare;
extern TEMPENTITY *m_pLaserSpot;

CTempEnts *g_pTempEnts;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTempEnts::CTempEnts( void )
{
	memset( m_TempEnts, 0, sizeof( TEMPENTITY ) * MAX_TEMP_ENTITIES );

	m_pFreeTempEnts = m_TempEnts;
	m_pActiveTempEnts = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTempEnts::~CTempEnts( void )
{
	// free pvEngineData pointers
	Clear();
}

void CTempEnts::TE_Prepare( TEMPENTITY *pTemp, int modelIndex )
{
	int	frameCount;

	// Use these to set per-frame and termination conditions / actions
	pTemp->flags = FTENT_NONE;		
	pTemp->die = gpGlobals->time + 0.75f;

	frameCount = GetModelFrames( modelIndex );

	pTemp->entity.curstate.modelindex = modelIndex;
	pTemp->entity.curstate.rendermode = kRenderNormal;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.curstate.rendercolor.r = 255;
	pTemp->entity.curstate.rendercolor.g = 255;
	pTemp->entity.curstate.rendercolor.b = 255;
	pTemp->frameMax = max( 0, frameCount - 1 );
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.body = 0;
	pTemp->entity.curstate.skin = 0;
	pTemp->fadeSpeed = 0.5f;
	pTemp->hitSound = 0;
	pTemp->clientIndex = 0;
	pTemp->bounceFactor = 1;
	pTemp->entity.curstate.scale = 1.0f;
}

int CTempEnts::TE_Active( TEMPENTITY *pTemp )
{
	bool	active = true;
	float	life;

	if( !pTemp ) return false;
	life = pTemp->die - gpGlobals->time;
	
	if( life < 0.0f )
	{
		if( pTemp->flags & FTENT_FADEOUT )
		{
			int	alpha;

			if( pTemp->entity.curstate.rendermode == kRenderNormal )
				pTemp->entity.curstate.rendermode = kRenderTransTexture;
			
			alpha = pTemp->entity.baseline.renderamt * ( 1.0f + life * pTemp->fadeSpeed );
			
			if( alpha <= 0 )
			{
				active = false;
				alpha = 0;
			}
			pTemp->entity.curstate.renderamt = alpha;
		}
		else 
		{
			active = false;
		}
	}

	// never die tempents only die when their die is cleared
	if( pTemp->flags & FTENT_NEVERDIE )
	{
		active = (pTemp->die != 0.0f);
	}
	return active;
}

int CTempEnts::TE_Update( TEMPENTITY *pTemp )
{
	// before first frame when movevars not initialized
	if( !gpMovevars )
	{
		Con_Printf( "ERROR: TempEntUpdate: no movevars!!!\n" );
		return true;
	}

	float	gravity, gravitySlow, fastFreq;
	float	frametime = gpGlobals->frametime;

	fastFreq = gpGlobals->time * 5.5;
	gravity = -frametime * gpMovevars->gravity;
	gravitySlow = gravity * 0.5;

	// in order to have tents collide with players, we have to run the player prediction code so
	// that the client has the player list. We run this code once when we detect any COLLIDEALL 
	// tent, then set this BOOL to true so the code doesn't get run again if there's more than
	// one COLLIDEALL ent for this update. (often are).
	g_engfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );

	// Store off the old count
	g_engfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	g_engfuncs.pEventAPI->EV_SetSolidPlayers ( -1 );	

	// save oldorigin
	pTemp->entity.prevstate.origin = pTemp->entity.origin;

	if( pTemp->flags & FTENT_SPARKSHOWER )
	{
		// adjust speed if it's time
		// scale is next think time
		if( gpGlobals->time > pTemp->entity.baseline.scale )
		{
			// show Sparks
			DoSparks( pTemp->entity.origin );

			// reduce life
			pTemp->entity.baseline.framerate -= 0.1f;

			if( pTemp->entity.baseline.framerate <= 0.0f )
			{
				pTemp->die = gpGlobals->time;
			}
			else
			{
				// so it will die no matter what
				pTemp->die = gpGlobals->time + 0.5f;

				// next think
				pTemp->entity.baseline.scale = gpGlobals->time + 0.1f;
			}
		}
	}
	else if( pTemp->flags & FTENT_PLYRATTACHMENT )
	{
		cl_entity_t *pClient = GetEntityByIndex( pTemp->clientIndex );

		if( pClient )
		{
			pTemp->entity.origin = pClient->origin + pTemp->tentOffset;
		}
	}
	else if( pTemp->flags & FTENT_SINEWAVE )
	{
		pTemp->x += pTemp->entity.baseline.origin.x * frametime;
		pTemp->y += pTemp->entity.baseline.origin.y * frametime;

		pTemp->entity.origin.x = pTemp->x + sin( pTemp->entity.baseline.origin.z + gpGlobals->time ) * ( 10 * pTemp->entity.curstate.scale );
		pTemp->entity.origin.y = pTemp->y + sin( pTemp->entity.baseline.origin.z + fastFreq + 0.7f ) * ( 8 * pTemp->entity.curstate.scale);
		pTemp->entity.origin.z = pTemp->entity.origin.z + pTemp->entity.baseline.origin[2] * frametime;
	}
	else if( pTemp->flags & FTENT_SPIRAL )
	{
		float s, c;
		s = sin( pTemp->entity.baseline.origin.z + fastFreq );
		c = cos( pTemp->entity.baseline.origin.z + fastFreq );

		pTemp->entity.origin.x = pTemp->entity.origin.x + pTemp->entity.baseline.origin.x * frametime + 8 * sin( gpGlobals->time * 20 );
		pTemp->entity.origin.y = pTemp->entity.origin.y + pTemp->entity.baseline.origin.y * frametime + 4 * sin( gpGlobals->time * 30 );
		pTemp->entity.origin.z = pTemp->entity.origin.z + pTemp->entity.baseline.origin.z * frametime;
	}
	else
	{
		// just add linear velocity
		pTemp->entity.origin = pTemp->entity.origin + pTemp->entity.baseline.origin * frametime;
	}
	
	if( pTemp->flags & FTENT_SPRANIMATE )
	{
		pTemp->entity.curstate.frame += frametime * pTemp->entity.curstate.framerate;

		if( pTemp->entity.curstate.frame >= pTemp->frameMax )
		{
			pTemp->entity.curstate.frame = pTemp->entity.curstate.frame - (int)(pTemp->entity.curstate.frame);

			if(!( pTemp->flags & FTENT_SPRANIMATELOOP ))
			{
				// this animating sprite isn't set to loop, so destroy it.
				pTemp->die = 0.0f;

				// restore state info
				g_engfuncs.pEventAPI->EV_PopPMStates();
				return false;
			}
		}
	}
	else if( pTemp->flags & FTENT_SPRCYCLE )
	{
		pTemp->entity.curstate.frame += frametime * 10;
		if( pTemp->entity.curstate.frame >= pTemp->frameMax )
		{
			pTemp->entity.curstate.frame = pTemp->entity.curstate.frame - (int)(pTemp->entity.curstate.frame);
		}
	}

	if( pTemp->flags & FTENT_SCALE )
	{
		// this only used for Egon effect
		pTemp->entity.curstate.scale += frametime * 10;
	}

	if( pTemp->flags & FTENT_ROTATE )
	{
		// just add angular velocity
		pTemp->entity.angles += pTemp->entity.baseline.angles * frametime;
		pTemp->entity.latched.prevangles = pTemp->entity.angles; 
	}

	if( pTemp->flags & ( FTENT_COLLIDEALL|FTENT_COLLIDEWORLD ))
	{
		Vector	traceNormal;
		float	traceFraction = 1.0f;

		traceNormal.Init();

		if( pTemp->flags & FTENT_COLLIDEALL )
		{
			pmtrace_t pmtrace;
			physent_t *pe;

			g_engfuncs.pEventAPI->EV_SetTraceHull( 2 );
			g_engfuncs.pEventAPI->EV_PlayerTrace( pTemp->entity.prevstate.origin, pTemp->entity.origin, PM_STUDIO_BOX, -1, &pmtrace );

			if ( pmtrace.fraction != 1 )
			{
				pe = g_engfuncs.pEventAPI->EV_GetPhysent( pmtrace.ent );

				if( !pmtrace.ent || ( pe->info != pTemp->clientIndex ))
				{
					traceFraction = pmtrace.fraction;
					traceNormal = pmtrace.plane.normal;

					if ( pTemp->hitcallback )
						(*pTemp->hitcallback)( pTemp, &pmtrace );
				}
			}
		}
		else if( pTemp->flags & FTENT_COLLIDEWORLD )
		{
			pmtrace_t pmtrace;
					
			g_engfuncs.pEventAPI->EV_SetTraceHull( 2 );
			g_engfuncs.pEventAPI->EV_PlayerTrace( pTemp->entity.prevstate.origin, pTemp->entity.origin, PM_STUDIO_BOX|PM_WORLD_ONLY, -1, &pmtrace );

			if( pmtrace.fraction != 1.0f )
			{
				traceFraction = pmtrace.fraction;
				traceNormal = pmtrace.plane.normal;

				if ( pTemp->flags & FTENT_SPARKSHOWER )
				{
					// chop spark speeds a bit more
					pTemp->entity.baseline.origin *= 0.6f;

					if( pTemp->entity.baseline.origin.Length() < 10.0f )
						pTemp->entity.baseline.framerate = 0.0f;
				}

				if( pTemp->hitcallback )
					(*pTemp->hitcallback)( pTemp, &pmtrace );
			}
		}
		
		if( traceFraction != 1.0f )	// Decent collision now, and damping works
		{
			float	proj, damp;

			// Place at contact point
			pTemp->entity.origin = pTemp->entity.prevstate.origin + (traceFraction * frametime) * pTemp->entity.baseline.origin;
			
			// Damp velocity
			damp = pTemp->bounceFactor;
			if( pTemp->flags & ( FTENT_GRAVITY|FTENT_SLOWGRAVITY ))
			{
				damp *= 0.5f;
				if( traceNormal[2] > 0.9f ) // Hit floor?
				{
					if( pTemp->entity.baseline.origin[2] <= 0 && pTemp->entity.baseline.origin[2] >= gravity * 3 )
					{
						pTemp->flags &= ~(FTENT_SLOWGRAVITY|FTENT_ROTATE|FTENT_GRAVITY);
						pTemp->flags &= ~(FTENT_COLLIDEWORLD|FTENT_SMOKETRAIL);
						pTemp->entity.angles.x = pTemp->entity.angles.z = 0;
						damp = 0;	// stop
					}
				}
			}

			if( pTemp->hitSound )
			{
				PlaySound( pTemp, damp );
			}

			if( pTemp->flags & FTENT_COLLIDEKILL )
			{
				// die on impact
				pTemp->flags &= ~FTENT_FADEOUT;	
				pTemp->die = gpGlobals->time;			
			}
			else
			{
				// reflect velocity
				if( damp != 0 )
				{
					proj = DotProduct( pTemp->entity.baseline.origin, traceNormal );
					pTemp->entity.baseline.origin += (-proj * 2) * traceNormal;

					// reflect rotation (fake)
					pTemp->entity.angles.y = -pTemp->entity.angles.y;
				}
				
				if( damp != 1.0f )
				{
					pTemp->entity.baseline.origin *= damp;
					pTemp->entity.angles *= 0.9f;
				}
			}
		}
	}

	// FIXME: this code is right ???
	if(( pTemp->flags & FTENT_FLICKER ) && m_iTempEntFrame == pTemp->entity.curstate.effects )
	{
		dlight_t *dl = g_engfuncs.pEfxAPI->CL_AllocDLight( 0 );

		dl->origin = pTemp->entity.origin;
		dl->radius = 60;
		dl->color[0] = 255;
		dl->color[1]= 120;
		dl->color[2] = 0;
		dl->die = gpGlobals->time + 0.01f;
	}

	if( pTemp->flags & FTENT_SMOKETRAIL )
	{
		g_pParticles->RocketTrail( pTemp->entity.prevstate.origin, pTemp->entity.origin, 1 );
	}

	if( pTemp->flags & FTENT_GRAVITY )
		pTemp->entity.baseline.origin.z += gravity;
	else if( pTemp->flags & FTENT_SLOWGRAVITY )
		pTemp->entity.baseline.origin.z += gravitySlow;

	if( pTemp->flags & FTENT_CLIENTCUSTOM )
	{
		if( pTemp->callback )
			(*pTemp->callback)( pTemp );
	}

	if( pTemp->flags & FTENT_WINDBLOWN )
	{
		Vector	vecWind = gHUD.m_vecWindVelocity;

		for( int i = 0 ; i < 2 ; i++ )
		{
			if( pTemp->entity.baseline.origin[i] < vecWind[i] )
			{
				pTemp->entity.baseline.origin[i] += ( frametime * TENT_WIND_ACCEL );

				// clamp
				if( pTemp->entity.baseline.origin[i] > vecWind[i] )
					pTemp->entity.baseline.origin[i] = vecWind[i];
			}
			else if( pTemp->entity.baseline.origin[i] > vecWind[i] )
			{
				pTemp->entity.baseline.origin[i] -= ( frametime * TENT_WIND_ACCEL );

				// clamp
				if( pTemp->entity.baseline.origin[i] < vecWind[i] )
					pTemp->entity.baseline.origin[i] = vecWind[i];
			}
		}
	}

	// restore state info
	g_engfuncs.pEventAPI->EV_PopPMStates();

	return true;
}

void CTempEnts :: Update( void )
{
	TEMPENTITY *current, *pnext, *pprev;

	if ( !m_pActiveTempEnts )		
	{
		return;
	}

	// !!!BUGBUG -- This needs to be time based
	// g-cont. it's used only for flickering dlights, what difference ?
	m_iTempEntFrame = ( m_iTempEntFrame + 1 ) & 31;

	current = m_pActiveTempEnts;

	// !!! Don't simulate while paused....  This is sort of a hack, revisit.
	if( IN_GAME() == 0 )
	{
		while( current )
		{
			CL_AddEntity( &current->entity, ET_TEMPENTITY, -1 );
			current = current->next;
		}
	}
	else
	{
		pprev = NULL;

		while( current )
		{
			bool	fTempEntFreed = false;

			pnext = current->next;

			// Kill it
			if( !TE_Active( current ) || !TE_Update( current ))
			{
				TempEntFree( current, pprev );
				fTempEntFreed = true;
			}
			else
			{
				// renderer rejected entity for some reasons...
				if( !CL_AddEntity( &current->entity, ET_TEMPENTITY, -1 ))
				{
					if(!( current->flags & FTENT_PERSIST )) 
					{
						// If we can't draw it this frame, just dump it.
						current->die = gpGlobals->time;
						// don't fade out, just die
						current->flags &= ~FTENT_FADEOUT;

						TempEntFree( current, pprev );
						fTempEntFreed = true;
					}
				}
			}

			if( !fTempEntFreed )
				pprev = current;
			current = pnext;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Clear existing temp entities
//-----------------------------------------------------------------------------
void CTempEnts::Clear( void )
{
	m_iTempEntFrame = 0;

	// update muzzleflash indexes
	m_iMuzzleFlash[0] = g_engfuncs.pEventAPI->EV_FindModelIndex( "sprites/muzzleflash1.spr" );
	m_iMuzzleFlash[1] = g_engfuncs.pEventAPI->EV_FindModelIndex( "sprites/muzzleflash2.spr" );
	m_iMuzzleFlash[2] = g_engfuncs.pEventAPI->EV_FindModelIndex( "sprites/muzzleflash3.spr" );
	m_iMuzzleFlash[3] = g_engfuncs.pEventAPI->EV_FindModelIndex( "sprites/muzzleflash.spr" );

	hSprGlowShell = TEX_Load( "renderfx/glowshell" );

	for ( int i = 0; i < MAX_TEMP_ENTITIES-1; i++ )
	{
		m_TempEnts[i].next = &m_TempEnts[i+1];
	}

	m_TempEnts[MAX_TEMP_ENTITIES-1].next = NULL;
	m_pFreeTempEnts = m_TempEnts;
	m_pActiveTempEnts = NULL;

	// clearing user pointers
	m_pLaserSpot = NULL;
	m_pEndFlare = NULL;
}

void CTempEnts::TempEntFree( TEMPENTITY *pTemp, TEMPENTITY *pPrev )
{
	// Remove from the active list.
	if( pPrev )	
	{
		pPrev->next = pTemp->next;
	}
	else
	{
		m_pActiveTempEnts = pTemp->next;
	}

	memset( pTemp, 0, sizeof( TEMPENTITY ));

	// Add to the free list.
	pTemp->next = m_pFreeTempEnts;
	m_pFreeTempEnts = pTemp;
}

// free the first low priority tempent it finds.
bool CTempEnts::FreeLowPriorityTempEnt( void )
{
	TEMPENTITY	*pActive = m_pActiveTempEnts;
	TEMPENTITY	*pPrev = NULL;

	while( pActive )
	{
		if( pActive->priority == TENTPRIORITY_LOW )
		{
			TempEntFree( pActive, pPrev );
			return true;
		}

		pPrev = pActive;
		pActive = pActive->next;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Allocate temp entity ( normal/low priority )
// Input  : *org - 
//			*model - 
// Output : C_LocalTempEntity
//-----------------------------------------------------------------------------
TEMPENTITY *CTempEnts::TempEntAlloc( const Vector& org, int modelIndex )
{
	TEMPENTITY *pTemp;

	if ( !m_pFreeTempEnts )
	{
		Con_Printf( "Overflow %d temporary ents!\n", MAX_TEMP_ENTITIES );
		return NULL;
	}

	pTemp = m_pFreeTempEnts;
	m_pFreeTempEnts = pTemp->next;

	TE_Prepare( pTemp, modelIndex );

	pTemp->priority = TENTPRIORITY_LOW;
	if( org ) pTemp->entity.origin = org;

	pTemp->next = m_pActiveTempEnts;
	m_pActiveTempEnts = pTemp;

	return pTemp;
}

//-----------------------------------------------------------------------------
// Purpose: Allocate a temp entity, if there are no slots, kick out a low priority
//  one if possible
// Input  : *org - 
//			*model - 
// Output : C_LocalTempEntity
//-----------------------------------------------------------------------------
TEMPENTITY *CTempEnts::TempEntAllocHigh( const Vector& org, int modelIndex )
{
	TEMPENTITY	*pTemp;

	if ( !m_pFreeTempEnts )
	{
		// no temporary ents free, so find the first active low-priority temp ent 
		// and overwrite it.
		FreeLowPriorityTempEnt();
	}

	if ( !m_pFreeTempEnts )
	{
		// didn't find anything? The tent list is either full of high-priority tents
		// or all tents in the list are still due to live for > 10 seconds. 
		Con_Printf( "Couldn't alloc a high priority TENT!\n" );
		return NULL;
	}

	// Move out of the free list and into the active list.
	pTemp = m_pFreeTempEnts;
	m_pFreeTempEnts = pTemp->next;

	pTemp->next = m_pActiveTempEnts;
	m_pActiveTempEnts = pTemp;

	TE_Prepare( pTemp, modelIndex );

	pTemp->priority = TENTPRIORITY_HIGH;
	if( org ) pTemp->entity.origin = org;

	return pTemp;
}


TEMPENTITY *CTempEnts::TempEntAllocNoModel( const Vector& org )
{
	return TempEntAlloc( org, 0 );
}

TEMPENTITY *CTempEnts::TempEntAllocCustom( const Vector& org, int modelIndex, int high, ENTCALLBACK pfnCallback )
{
	TEMPENTITY	*pTemp;

	if( high )
	{
		pTemp = TempEntAllocHigh( org, modelIndex );
	}
	else
	{
		pTemp = TempEntAlloc( org, modelIndex );
	}

	if( pTemp && pfnCallback )
	{
		pTemp->callback = pfnCallback;
		pTemp->flags |= FTENT_CLIENTCUSTOM;
	}

	return pTemp;
}

/*
==============================================================

	BASE TEMPENTS CODE

==============================================================
*/
//-----------------------------------------------------------------------------
// Purpose: Create a fizz effect
// Input  : *pent - 
//			modelIndex - 
//			density - 
//-----------------------------------------------------------------------------
void CTempEnts::FizzEffect( cl_entity_t *pent, int modelIndex, int density )
{
	TEMPENTITY	*pTemp;
	int		i, width, depth, count, frameCount;
	float		angle, maxHeight, speed, xspeed, yspeed;
	Vector		origin;
	Vector		mins, maxs;

	if( !pent || GetModelType( modelIndex ) == mod_bad )
		return;

	count = density + 1;
	density = count * 3 + 6;

	GetModelBounds( pent->curstate.modelindex, mins, maxs );

	maxHeight = maxs[2] - mins[2];
	width = maxs[0] - mins[0];
	depth = maxs[1] - mins[1];
	speed = ( pent->curstate.rendercolor.r<<8 | pent->curstate.rendercolor.g );
	if( pent->curstate.rendercolor.b ) speed = -speed;
	if( speed == 0.0f ) speed = 100.0f;	// apply default value

	if( pent->angles[YAW] != 0.0f )
	{
		angle = pent->angles[YAW] * M_PI / 180;
		yspeed = sin( angle );
		xspeed = cos( angle );

		xspeed *= speed;
		yspeed *= speed;
	}
	else xspeed = yspeed = 0.0f;	// zonly

	frameCount = GetModelFrames( modelIndex );

	for ( i = 0; i < count; i++ )
	{
		origin[0] = mins[0] + RANDOM_LONG( 0, width - 1 );
		origin[1] = mins[1] + RANDOM_LONG( 0, depth - 1 );
		origin[2] = mins[2];
		pTemp = TempEntAlloc( origin, modelIndex );

		if ( !pTemp ) return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];

		float zspeed = RANDOM_LONG( 80, 140 );
		pTemp->entity.baseline.origin = Vector( xspeed, yspeed, zspeed );
		pTemp->die = gpGlobals->time + ( maxHeight / zspeed ) - 0.1f;
		pTemp->entity.curstate.frame = RANDOM_LONG( 0, frameCount - 1 );
		// Set sprite scale
		pTemp->entity.curstate.scale = 1.0f / RANDOM_FLOAT( 2, 5 );
		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 255;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create bubbles
// Input  : *mins - 
//			*maxs - 
//			height - 
//			modelIndex - 
//			count - 
//			speed - 
//-----------------------------------------------------------------------------
void CTempEnts::Bubbles( const Vector &mins, const Vector &maxs, float height, int modelIndex, int count, float speed )
{
	TEMPENTITY	*pTemp;
	int		i, frameCount;
	float		sine, cosine;
	Vector		origin;

	if( GetModelType( modelIndex ) == mod_bad )
		return;

	frameCount = GetModelFrames( modelIndex );

	for ( i = 0; i < count; i++ )
	{
		origin[0] = RANDOM_LONG( mins[0], maxs[0] );
		origin[1] = RANDOM_LONG( mins[1], maxs[1] );
		origin[2] = RANDOM_LONG( mins[2], maxs[2] );
		pTemp = TempEntAlloc( origin, modelIndex );
		if ( !pTemp ) return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];
		float angle = RANDOM_LONG( -M_PI, M_PI );
		sine = sin( angle );
		cosine = cos( angle );
		
		float zspeed = RANDOM_LONG( 80, 140 );
		pTemp->entity.baseline.origin = Vector( speed * cosine, speed * sine, zspeed );
		pTemp->die = gpGlobals->time + ((height - (origin[2] - mins[2])) / zspeed) - 0.1f;
		pTemp->entity.curstate.frame = RANDOM_LONG( 0, frameCount - 1 );
		
		// Set sprite scale
		pTemp->entity.curstate.scale = 1.0 / RANDOM_FLOAT( 4, 16 );
		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 192;	// g-cont. why difference with FizzEffect ???		
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create bubble trail
// Input  : *start - 
//			*end - 
//			height - 
//			modelIndex - 
//			count - 
//			speed - 
//-----------------------------------------------------------------------------
void CTempEnts::BubbleTrail( const Vector &start, const Vector &end, float flWaterZ, int modelIndex, int count, float speed )
{
	TEMPENTITY	*pTemp;
	int		i, frameCount;
	float		dist, angle;
	Vector		origin;

	if( GetModelType( modelIndex ) == mod_bad )
		return;

	frameCount = GetModelFrames( modelIndex );

	for ( i = 0; i < count; i++ )
	{
		dist = RANDOM_FLOAT( 0, 1.0 );	// g-cont. hmm may be use GetLerpFrac instead ?

		origin = LerpPoint( start, end, dist );
		pTemp = TempEntAlloc( origin, modelIndex );
		if ( !pTemp ) return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];
		angle = RANDOM_LONG( -M_PI, M_PI );

		float zspeed = RANDOM_LONG( 80, 140 );
		pTemp->entity.baseline.origin = Vector( speed * cos( angle ), speed * sin( angle ), zspeed );
		pTemp->die = gpGlobals->time + (( flWaterZ - origin[2]) / zspeed ) - 0.1f;
		pTemp->entity.curstate.frame = RANDOM_LONG( 0, frameCount - 1 );
		// Set sprite scale
		pTemp->entity.curstate.scale = 1.0 / RANDOM_FLOAT( 4, 8 );
		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 192;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Attaches entity to player
// Input  : client - 
//			modelIndex - 
//			zoffset - 
//			life - 
//-----------------------------------------------------------------------------
void CTempEnts::AttachTentToPlayer( int client, int modelIndex, float zoffset, float life )
{
	TEMPENTITY	*pTemp;
	Vector		position;
	int		frameCount;

	if ( client <= 0 || client > gpGlobals->maxClients )
	{
		Con_Printf( "Bad client in AttachTentToPlayer()!\n" );
		return;
	}

	cl_entity_t *pClient = GetEntityByIndex( client );
	if ( !pClient )
	{
		Con_Printf( "Couldn't get ClientEntity for %i\n", client );
		return;
	}

	if( GetModelType( modelIndex ) == mod_bad )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return;
	}

	position = pClient->origin;
	position[2] += zoffset;

	pTemp = TempEntAllocHigh( position, modelIndex );
	if ( !pTemp )
	{
		Con_Printf( "No temp ent.\n" );
		return;
	}

	pTemp->entity.curstate.rendermode = kRenderNormal;
	pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 192;
	pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
	
	pTemp->clientIndex = client;
	pTemp->tentOffset[0] = 0;
	pTemp->tentOffset[1] = 0;
	pTemp->tentOffset[2] = zoffset;
	pTemp->die = gpGlobals->time + life;
	pTemp->flags |= FTENT_PLYRATTACHMENT|FTENT_PERSIST;

	// is the model a sprite?
	if ( GetModelType( pTemp->entity.curstate.modelindex ) == mod_sprite )
	{
		frameCount = GetModelFrames( pTemp->entity.curstate.modelindex );
		pTemp->frameMax = frameCount - 1;
		pTemp->flags |= FTENT_SPRANIMATE|FTENT_SPRANIMATELOOP;
		pTemp->entity.curstate.framerate = 10;
	}
	else
	{
		// no animation support for attached clientside studio models.
		pTemp->frameMax = 0;
	}

	pTemp->entity.curstate.frame = 0;
}
#define FOR_EACH_LL( listName, iteratorName ) \
	for( int iteratorName=listName.Head(); iteratorName != listName.InvalidIndex(); iteratorName = listName.Next( iteratorName ) )

//-----------------------------------------------------------------------------
// Purpose: Detach entity from player
//-----------------------------------------------------------------------------
void CTempEnts::KillAttachedTents( int client )
{
	if ( client <= 0 || client > gpGlobals->maxClients )
	{
		Con_Printf( "Bad client in KillAttachedTents()!\n" );
		return;
	}

	for( int i = 0; i < MAX_TEMP_ENTITIES; i++ )
	{
		TEMPENTITY *pTemp = &m_TempEnts[i];

		if ( pTemp->flags & FTENT_PLYRATTACHMENT )
		{
			// this TENT is player attached.
			// if it is attached to this client, set it to die instantly.
			if ( pTemp->clientIndex == client )
			{
				pTemp->die = gpGlobals->time; // good enough, it will die on next tent update. 
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create ricochet sprite
// Input  : *pos - 
//			*pmodel - 
//			duration - 
//			scale - 
//-----------------------------------------------------------------------------
void CTempEnts::RicochetSprite( const Vector &pos, int modelIndex, float scale )
{
	TEMPENTITY	*pTemp;

	pTemp = TempEntAlloc( pos, modelIndex );
	if (!pTemp)
		return;

	pTemp->entity.curstate.rendermode = kRenderGlow;
	pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 200;
	pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
	pTemp->entity.curstate.scale = scale;
	pTemp->flags = FTENT_FADEOUT;
	pTemp->fadeSpeed = 8;
	pTemp->die = gpGlobals->time;

	pTemp->entity.curstate.frame = 0;
	pTemp->entity.angles[ROLL] = 45 * RANDOM_LONG( 0, 7 );
}

void CTempEnts::PlaySound( TEMPENTITY *pTemp, float damp )
{
	float fvol;
	char soundname[32];
	bool isshellcasing = false;
	int zvel;

	switch ( pTemp->hitSound )
	{
	default:
		return;	// null sound
	case BOUNCE_GLASS:
		{
			sprintf( soundname, "debris/glass%i.wav", RANDOM_LONG( 1, 4 ));
		}
		break;
	case BOUNCE_METAL:
		{
			sprintf( soundname, "debris/metal%i.wav", RANDOM_LONG( 1, 6 ));
		}
		break;
	case BOUNCE_FLESH:
		{
			sprintf( soundname, "debris/flesh%i.wav", RANDOM_LONG( 1, 7 ));
		}
		break;
	case BOUNCE_WOOD:
		{
			sprintf( soundname, "debris/wood%i.wav", RANDOM_LONG( 1, 4 ));
		}
		break;
	case BOUNCE_SHRAP:
		{
			sprintf( soundname, "weapons/ric%i.wav", RANDOM_LONG( 1, 5 ));
		}
		break;
	case BOUNCE_SHOTSHELL:
		{
			sprintf( soundname, "weapons/sshell%i.wav", RANDOM_LONG( 1, 3 ));
			isshellcasing = true; // shell casings have different playback parameters
		}
		break;
	case BOUNCE_SHELL:
		{
			sprintf( soundname, "player/pl_shell%i.wav", RANDOM_LONG( 1, 3 ));
			isshellcasing = true; // shell casings have different playback parameters
		}
		break;
	case BOUNCE_CONCRETE:
		{
			sprintf( soundname, "debris/concrete%i.wav", RANDOM_LONG( 1, 3 ));
		}
		break;
	}

	zvel = abs( pTemp->entity.baseline.origin[2] );
		
	// only play one out of every n

	if ( isshellcasing )
	{	
		// play first bounce, then 1 out of 3		
		if ( zvel < 200 && RANDOM_LONG( 0, 3 ))
			return;
	}
	else
	{
		if ( RANDOM_LONG( 0, 5 )) 
			return;
	}

	fvol = 1.0f;

	if ( damp > 0.0 )
	{
		int pitch;
		
		if ( isshellcasing )
		{
			fvol *= min ( 1.0f, ((float)zvel) / 350.0 ); 
		}
		else
		{
			fvol *= min ( 1.0f, ((float)zvel) / 450.0 ); 
		}
		
		if ( !RANDOM_LONG( 0, 3 ) && !isshellcasing )
		{
			pitch = RANDOM_LONG( 95, 105 );
		}
		else
		{
			pitch = 100; // FIXME
		}
		CL_PlaySound( soundname, fvol, pTemp->entity.origin, pitch );
	}
}

void CTempEnts::RocketFlare( const Vector& pos )
{
	TEMPENTITY	*pTemp;
	int		modelIndex;
	int		nframeCount;

	modelIndex = g_engfuncs.pEventAPI->EV_FindModelIndex( "sprites/animglow01.spr" );
	if( !modelIndex ) return;

	nframeCount = GetModelFrames( modelIndex );

	pTemp = TempEntAlloc( pos, modelIndex );
	if ( !pTemp ) return;

	pTemp->frameMax = nframeCount - 1;
	pTemp->entity.curstate.rendermode = kRenderGlow;
	pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
	pTemp->entity.curstate.renderamt = 200;
	pTemp->entity.curstate.framerate = 1.0;
	pTemp->entity.curstate.frame = RANDOM_LONG( 0, nframeCount - 1 );
	pTemp->entity.curstate.scale = 1.0;
	pTemp->die = gpGlobals->time + 0.01f;	// when 100 fps die at next frame
	pTemp->flags |= FTENT_SPRANIMATE;
}

void CTempEnts::MuzzleFlash( cl_entity_t *pEnt, int iAttachment, int type )
{
	Vector pos;
	TEMPENTITY *pTemp;
	int index, modelIndex, frameCount;
	float scale;

	index = bound( 0, type % 10, MAX_MUZZLEFLASH - 1 );
	scale = (type / 10) * 0.1f;
	if( scale == 0.0f ) scale = 0.5f;

	modelIndex = m_iMuzzleFlash[index];
	if( !modelIndex ) return;

	frameCount = GetModelFrames( modelIndex );
	pos = pEnt->origin;

	if( iAttachment > 0 )
		pos += pEnt->attachment_origin[iAttachment - 1];

	if( pos == pEnt->origin )
	{
		Con_Printf( "Invalid muzzleflash entity!\n" );
		return;
	}

	// must set position for right culling on render
	pTemp = TempEntAlloc( pos, modelIndex );
	if( !pTemp ) return;
	
	pTemp->entity.curstate.rendermode = kRenderTransAdd;
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.renderfx = 0;
	pTemp->die = gpGlobals->time + 0.05; // die at next frame
	pTemp->entity.curstate.frame = RANDOM_LONG( 0, frameCount - 1 );
	pTemp->frameMax = frameCount - 1;
	pTemp->clientIndex = pEnt->index;

	if( index == 0 )
	{
		// Rifle flash
		pTemp->entity.curstate.scale = scale * RANDOM_FLOAT( 0.5f, 0.6f );
		pTemp->entity.angles[2] = 90 * RANDOM_LONG( 0, 3 );
	}
	else
	{
		pTemp->entity.curstate.scale = scale;
		pTemp->entity.angles[2] = RANDOM_LONG( 0, 359 );
	}

	// render now (guranteed that muzzleflash will be draw)
	CL_AddEntity( &pTemp->entity, ET_TEMPENTITY, -1 );
}

void CTempEnts::BloodSprite( const Vector &org, int colorIndex, int modelIndex, int modelIndex2, float size )
{
	TEMPENTITY	*pTemp;

	if( GetModelType( modelIndex ) == mod_bad )
		return;

	// Large, single blood sprite is a high-priority tent
	if( pTemp = TempEntAllocHigh( org, modelIndex ))
	{
		int	frameCount;
		Vector	color;

		frameCount = GetModelFrames( modelIndex );
		pTemp->entity.curstate.rendermode = kRenderTransTexture;
		pTemp->entity.curstate.renderfx = kRenderFxClampMinScale;
		pTemp->entity.curstate.scale = RANDOM_FLOAT(( size / 25.0f), ( size / 35.0f ));
		pTemp->flags = FTENT_SPRANIMATE;
 
		CL_GetPaletteColor( colorIndex, color );
		pTemp->entity.curstate.rendercolor.r = color[0];
		pTemp->entity.curstate.rendercolor.g = color[1];
		pTemp->entity.curstate.rendercolor.b = color[2];
		pTemp->entity.curstate.framerate = frameCount * 4; // Finish in 0.250 seconds
		// play the whole thing once
		pTemp->die = gpGlobals->time + (frameCount / pTemp->entity.curstate.framerate);

		pTemp->entity.angles[2] = RANDOM_LONG( 0, 360 );
		pTemp->bounceFactor = 0;
	}
}

void CTempEnts::BreakModel( const Vector &pos, const Vector &size, const Vector &dir, float random, float life, int count, int modelIndex, char flags )
{
	int i, frameCount;
	TEMPENTITY *pTemp;
	char type;

	if ( !modelIndex ) return;

	type = flags & BREAK_TYPEMASK;

	if ( GetModelType( modelIndex ) == mod_bad )
		return;

	frameCount = GetModelFrames( modelIndex );
		
	if ( count == 0 )
	{
		// assume surface (not volume)
		count = (size[0] * size[1] + size[1] * size[2] + size[2] * size[0]) / (3 * SHARD_VOLUME * SHARD_VOLUME);
	}

	// limit to 100 pieces
	if ( count > 100 ) count = 100;

	for ( i = 0; i < count; i++ ) 
	{
		vec3_t	vecSpot;

		// fill up the box with stuff
		vecSpot[0] = pos[0] + RANDOM_FLOAT( -0.5f, 0.5f ) * size[0];
		vecSpot[1] = pos[1] + RANDOM_FLOAT( -0.5f, 0.5f ) * size[1];
		vecSpot[2] = pos[2] + RANDOM_FLOAT( -0.5f, 0.5f ) * size[2];

		pTemp = TempEntAlloc( vecSpot, modelIndex );
		if( !pTemp ) return;

		// keep track of break_type, so we know how to play sound on collision
		pTemp->hitSound = type;
		
		if( GetModelType( modelIndex ) == mod_sprite )
			pTemp->entity.curstate.frame = RANDOM_LONG( 0, frameCount - 1 );
		else if( GetModelType( modelIndex ) == mod_studio )
			pTemp->entity.curstate.body = RANDOM_LONG( 0, frameCount - 1 );

		pTemp->flags |= FTENT_COLLIDEWORLD | FTENT_FADEOUT | FTENT_SLOWGRAVITY;

		if ( RANDOM_LONG( 0, 255 ) < 200 ) 
		{
			pTemp->flags |= FTENT_ROTATE;
			pTemp->entity.baseline.angles[0] = RANDOM_FLOAT( -256, 255 );
			pTemp->entity.baseline.angles[1] = RANDOM_FLOAT( -256, 255 );
			pTemp->entity.baseline.angles[2] = RANDOM_FLOAT( -256, 255 );
		}

		if (( RANDOM_LONG( 0, 255 ) < 100 ) && ( flags & BREAK_SMOKE ))
			pTemp->flags |= FTENT_SMOKETRAIL;

		if (( type == BREAK_GLASS ) || ( flags & BREAK_TRANS ))
		{
			pTemp->entity.curstate.rendermode = kRenderTransTexture;
			pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 128;
		}
		else
		{
			pTemp->entity.curstate.rendermode = kRenderNormal;
			pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 255; // set this for fadeout
		}

		pTemp->entity.baseline.origin[0] = dir[0] + RANDOM_FLOAT( -random, random );
		pTemp->entity.baseline.origin[1] = dir[1] + RANDOM_FLOAT( -random, random );
		pTemp->entity.baseline.origin[2] = dir[2] + RANDOM_FLOAT( 0, random );

		pTemp->die = gpGlobals->time + life + RANDOM_FLOAT( 0, 1 ); // Add an extra 0-1 secs of life
	}
}

void CTempEnts::TempModel( const Vector &pos, const Vector &dir, const Vector &ang, float life, int modelIndex, int soundtype )
{
	// alloc a new tempent
	TEMPENTITY *pTemp = TempEntAlloc( pos, modelIndex );
	if( !pTemp ) return;

	// keep track of shell type
	switch( soundtype )
	{
	case TE_BOUNCE_SHELL: pTemp->hitSound = BOUNCE_SHELL; break;
	case TE_BOUNCE_SHOTSHELL: pTemp->hitSound = BOUNCE_SHOTSHELL; break;
	}

	pTemp->entity.origin  = pos;
	pTemp->entity.baseline.origin = dir;
	pTemp->entity.angles = ang;

	pTemp->entity.curstate.body = 0;
	pTemp->flags = (FTENT_COLLIDEWORLD|FTENT_FADEOUT|FTENT_GRAVITY|FTENT_ROTATE);
	pTemp->entity.baseline.angles[0] = RANDOM_FLOAT( -255, 255 );
	pTemp->entity.baseline.angles[1] = RANDOM_FLOAT( -255, 255 );
	pTemp->entity.baseline.angles[2] = RANDOM_FLOAT( -255, 255 );
	pTemp->entity.curstate.rendermode = kRenderNormal;
	pTemp->entity.baseline.renderamt = 255;
	pTemp->die = gpGlobals->time + life;
}

TEMPENTITY *CTempEnts::DefaultSprite( const Vector &pos, int spriteIndex, float framerate )
{
	TEMPENTITY	*pTemp;
	int		frameCount;

	if( !spriteIndex || GetModelType( spriteIndex ) != mod_sprite )
	{
		Con_Printf( "No Sprite %d!\n", spriteIndex );
		return NULL;
	}

	frameCount = GetModelFrames( spriteIndex );

	pTemp = TempEntAlloc( pos, spriteIndex );
	if( !pTemp ) return NULL;

	pTemp->frameMax = frameCount - 1;
	pTemp->entity.curstate.scale = 1.0f;
	pTemp->flags |= FTENT_SPRANIMATE;
	if( framerate == 0 ) framerate = 10;

	pTemp->entity.curstate.framerate = framerate;
	pTemp->die = gpGlobals->time + (float)frameCount / framerate;
	pTemp->entity.curstate.frame = 0;

	return pTemp;
}

/*
===============
CL_TempSprite
===============
*/
TEMPENTITY *CTempEnts::TempSprite( const Vector &pos, const Vector &dir, float scale, int modelIndex, int rendermode, int renderfx, float a, float life, int flags )
{
	TEMPENTITY	*pTemp;
	int		frameCount;

	if( !modelIndex ) 
		return NULL;

	if( GetModelType( modelIndex ) == mod_bad )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return NULL;
	}

	frameCount = GetModelFrames( modelIndex );

	pTemp = TempEntAlloc( pos, modelIndex );
	if( !pTemp ) return NULL;

	pTemp->frameMax = frameCount - 1;
	pTemp->entity.curstate.framerate = 10;
	pTemp->entity.curstate.rendermode = rendermode;
	pTemp->entity.curstate.renderfx = renderfx;
	pTemp->entity.curstate.scale = scale;
	pTemp->entity.baseline.renderamt = a * 255;
	pTemp->entity.curstate.renderamt = a * 255;
	pTemp->flags |= flags;

	pTemp->entity.origin = pos;
	pTemp->entity.baseline.origin = dir;

	if( life ) pTemp->die = gpGlobals->time + life;
	else pTemp->die = gpGlobals->time + (frameCount * 0.1f) + 1.0f;
	pTemp->entity.curstate.frame = 0;

	return pTemp;
}

void CTempEnts::Sprite_Explode( TEMPENTITY *pTemp, float scale, int flags )
{
	if( !pTemp ) return;

	// NOTE: Xash3D doesn't needs this stuff, because sprites
	// have right rendermodes already at loading point
	// but i'm leave it for backward compatibility
	if( flags & TE_EXPLFLAG_NOADDITIVE )
	{
		// solid sprite
		pTemp->entity.curstate.rendermode = kRenderNormal;
		pTemp->entity.curstate.renderamt = 255; 
	}
	else if( flags & TE_EXPLFLAG_DRAWALPHA )
	{
		// alpha sprite
		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.curstate.renderamt = 180;
	}
	else
	{
		// additive sprite
		pTemp->entity.curstate.rendermode = kRenderTransAdd;
		pTemp->entity.curstate.renderamt = 120;
	}

	if( flags & TE_EXPLFLAG_ROTATE )
	{
		pTemp->entity.angles[2] = RANDOM_LONG( 0, 360 );
	}

	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.baseline.origin[2] = 8;
	pTemp->entity.origin[2] += 10;
	pTemp->entity.curstate.scale = scale;
}

void CTempEnts::Sprite_Smoke( TEMPENTITY *pTemp, float scale )
{
	int	iColor;

	if( !pTemp ) return;

	iColor = RANDOM_LONG( 20, 35 );
	pTemp->entity.curstate.rendermode = kRenderTransAlpha;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.baseline.origin[2] = 30;
	pTemp->entity.curstate.rendercolor.r = iColor;
	pTemp->entity.curstate.rendercolor.g = iColor;
	pTemp->entity.curstate.rendercolor.b = iColor;
	pTemp->entity.origin[2] += 20;
	pTemp->entity.curstate.scale = scale;
	pTemp->flags |= FTENT_WINDBLOWN;
}

void CTempEnts::Sprite_Spray( const Vector &pos, const Vector &dir, int modelIndex, int count, int speed, int iRand, int renderMode )
{
	TEMPENTITY	*pTemp;
	float		noise;
	float		znoise;
	int		i, frameCount;

	noise = (float)iRand / 100;

	// more vertical displacement
	znoise = noise * 1.5f;
	
	if( znoise > 1 ) znoise = 1;

	if( GetModelType( modelIndex ) == mod_bad )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return;
	}

	frameCount = GetModelFrames( modelIndex );

	for( i = 0; i < count; i++ )
	{
		vec3_t	velocity;
		float	scale;

		pTemp = TempEntAlloc( pos, modelIndex );
		if( !pTemp ) return;

		pTemp->entity.curstate.rendermode = renderMode;
		pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
		pTemp->entity.curstate.scale = 0.5f;
		pTemp->flags |= FTENT_FADEOUT|FTENT_SLOWGRAVITY;
		pTemp->fadeSpeed = 2.0f;

		// make the spittle fly the direction indicated, but mix in some noise.
		velocity[0] = dir[0] + RANDOM_FLOAT( -noise, noise );
		velocity[1] = dir[1] + RANDOM_FLOAT( -noise, noise );
		velocity[2] = dir[2] + RANDOM_FLOAT( 0, znoise );
		scale = RANDOM_FLOAT(( speed * 0.8f ), ( speed * 1.2f ));
		pTemp->entity.baseline.origin = velocity * scale;
		pTemp->die = gpGlobals->time + 0.35f;
		pTemp->entity.curstate.frame = RANDOM_LONG( 0, frameCount - 1 );
	}
}

void CTempEnts::Sprite_Trail( int type, const Vector &vecStart, const Vector &vecEnd, int modelIndex, int nCount, float flLife, float flSize, float flAmplitude, int nRenderamt, float flSpeed )
{
	TEMPENTITY	*pTemp;
	vec3_t		vecDelta, vecDir;
	int		i, flFrameCount;

	if( GetModelType( modelIndex ) == mod_bad )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return;
	}	

	flFrameCount = GetModelFrames( modelIndex );

	vecDelta = vecEnd - vecStart;
	vecDir = vecDelta.Normalize();

	flAmplitude /= 256.0;

	for ( i = 0; i < nCount; i++ )
	{
		Vector vecPos, vecVel;

		// Be careful of divide by 0 when using 'count' here...
		if ( i == 0 )
		{
			vecPos = vecStart;
		}
		else
		{
			vecPos = vecStart + vecDelta * ( i / ( nCount - 1.0f ));
		}

		pTemp = TempEntAlloc( vecPos, modelIndex );
		if( !pTemp ) return;

		pTemp->flags |= FTENT_COLLIDEWORLD | FTENT_SPRCYCLE | FTENT_FADEOUT | FTENT_SLOWGRAVITY;

		vecVel = vecDir * flSpeed;
		vecVel[0] += RANDOM_FLOAT( -127, 128 ) * flAmplitude;
		vecVel[1] += RANDOM_FLOAT( -127, 128 ) * flAmplitude;
		vecVel[2] += RANDOM_FLOAT( -127, 128 ) * flAmplitude;
		pTemp->entity.baseline.origin = vecVel;
		pTemp->entity.origin = vecPos;

		pTemp->entity.curstate.scale = flSize;
		pTemp->entity.curstate.rendermode = kRenderGlow;
		pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
		pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = nRenderamt;

		pTemp->entity.curstate.frame = RANDOM_LONG( 0, flFrameCount - 1 );
		pTemp->frameMax	= flFrameCount - 1;
		pTemp->die = gpGlobals->time + flLife + RANDOM_FLOAT( 0, 4 );
	}
}

void CTempEnts::Large_Funnel( Vector pos, int spriteIndex, int flags )
{
	TEMPENTITY	*pTemp = NULL;
	CBaseParticle	*pPart = NULL;
	float		vel;
	Vector		dir;
	Vector		dest;
	Vector		m_vecPos;
	float		flDist;

	for ( int i = -256 ; i <= 256 ; i += 32 )
	{
		for ( int j = -256 ; j <= 256 ; j += 32 )
		{
			if( pTemp )
			{
				pPart = g_pParticles->AllocParticle ();
				pTemp = NULL;
			}
			else
			{
				pTemp = TempEntAlloc( pos, spriteIndex );
				pPart = NULL;
			}

			if( pTemp || pPart )
			{
				if ( flags & 1 )
				{
					m_vecPos = pos;

					dest[0] = pos[0] + i;
					dest[1] = pos[1] + j;
					dest[2] = pos[2] + RANDOM_FLOAT( 100, 800 );

					// send particle heading to dest at a random speed
					dir = dest - m_vecPos;

					vel = dest[2] / 8;// velocity based on how far particle has to travel away from org
				}
				else
				{
					m_vecPos[0] = pos[0] + i;
					m_vecPos[1] = pos[1] + j;
					m_vecPos[2] = pos[2] + RANDOM_FLOAT( 100, 800 );

					// send particle heading to org at a random speed
					dir = pos - m_vecPos;

					vel = m_vecPos[2] / 8;// velocity based on how far particle starts from org
				}

				flDist = dir.Length();	// save the distance
				dir = dir.Normalize();

				if ( vel < 64 )
				{
					vel = 64;
				}
				
				vel += RANDOM_FLOAT( 64, 128  );
				float life = (flDist / vel);

				if( pTemp )
				{
					pTemp->entity.origin = m_vecPos;
					pTemp->entity.baseline.origin = dir * vel;
					pTemp->entity.curstate.rendermode = kRenderTransAdd;
					pTemp->flags |= FTENT_FADEOUT;
					pTemp->fadeSpeed = 2.0f;
					pTemp->die = gpGlobals->time + RANDOM_FLOAT( life * 0.5, life );
					pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 255;
				}
				
				if( pPart )
				{
					pPart->m_Pos = m_vecPos;
					pPart->SetColor( 0.0f, 1.0f, 0.0f );
					pPart->SetType( pt_static );
					pPart->SetAlpha( 1.0f );

					pPart->m_Velocity = dir * vel;
					// die right when you get there
					pPart->SetLifetime( RANDOM_FLOAT( life * 0.5, life ));
				}
			}
		}
	}
}

void CTempEnts::DoSparks( const Vector& pos )
{
	Vector m_vecPos, m_vecDir;

	// randomize position
	m_vecPos.x = pos.x + RANDOM_FLOAT( -2, 2 );
	m_vecPos.y = pos.y + RANDOM_FLOAT( -2, 2 );
	m_vecPos.z = pos.z + RANDOM_FLOAT( -2, 2 );

	int modelIndex = g_engfuncs.pEventAPI->EV_FindModelIndex( "sprites/richo1.spr" );
	RicochetSprite( m_vecPos, modelIndex, RANDOM_FLOAT( 0.4, 0.6f ));

	// create a 8 random spakle tracers
	for ( int i = 0; i < 8; i++ )
	{
		m_vecDir.x = RANDOM_FLOAT( -1.0f, 1.0f );
		m_vecDir.y = RANDOM_FLOAT( -1.0f, 1.0f );
		m_vecDir.z = RANDOM_FLOAT( -1.0f, 1.0f );

		g_pParticles->SparkleTracer( m_vecPos, m_vecDir );
	}
}

void CTempEnts::TracerEffect( const Vector &start, const Vector &end )
{
	g_pParticles->BulletTracer( start, end );
}

void CTempEnts::StreakSplash( const Vector &pos, const Vector &dir, int color, int count, int speed, int velMin, int velMax )
{
	for ( int i = 0; i < count; i++ )
	{
		Vector	vel;

		vel.x = (dir.x * speed) + RANDOM_FLOAT( velMin, velMax );
		vel.y = (dir.y * speed) + RANDOM_FLOAT( velMin, velMax );
		vel.z = (dir.z * speed) + RANDOM_FLOAT( velMin, velMax );

		g_pParticles->StreakTracer( pos, vel, color );
	}
}

void CTempEnts::WeaponFlash( cl_entity_t *pEnt, int iAttachment )
{
	Vector	pos = pEnt->origin;

	if( iAttachment > 0 )
		pos += pEnt->attachment_origin[iAttachment - 1];
	if( pos == pEnt->origin ) return; // missing attachment

	AllocDLight( pos, 255, 180, 64, 100, 0.05f );
}

void CTempEnts::PlaceDecal( Vector pos, int entityIndex, int decalIndex )
{
	HSPRITE hDecal;
	cl_entity_t *pEnt;
	int modelIndex = 0;

	pEnt = GetEntityByIndex( entityIndex );
	if( pEnt ) modelIndex = pEnt->curstate.modelindex;
	hDecal = g_engfuncs.pEfxAPI->CL_DecalIndex( decalIndex );
	g_engfuncs.pEfxAPI->R_DecalShoot( hDecal, entityIndex, modelIndex, pos, 0 );
}

void CTempEnts::PlaceDecal( Vector pos, int entityIndex, const char *decalname )
{
	HSPRITE hDecal;
	cl_entity_t *pEnt;
	int modelIndex = 0;

	pEnt = GetEntityByIndex( entityIndex );
	if( pEnt ) modelIndex = pEnt->curstate.modelindex;
	hDecal = g_engfuncs.pEfxAPI->CL_DecalIndexFromName( decalname );
	g_engfuncs.pEfxAPI->R_DecalShoot( hDecal, entityIndex, modelIndex, pos, 0 );
}

void CTempEnts::AllocDLight( Vector pos, byte r, byte g, byte b, float radius, float time, float decay )
{
	if( radius <= 0 ) return;

	dlight_t	*dl;

	dl = g_engfuncs.pEfxAPI->CL_AllocDLight( 0 );

	dl->origin = pos;	
	dl->die = gpGlobals->time + time;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->radius = radius;
	dl->decay = decay;
}

void CTempEnts::AllocDLight( Vector pos, float radius, float time, float decay )
{
	AllocDLight( pos, 255, 255, 255, radius, time, decay );
}

void CTempEnts::RocketTrail( Vector start, Vector end, int type )
{
	g_pParticles->RocketTrail( start, end, type );
}