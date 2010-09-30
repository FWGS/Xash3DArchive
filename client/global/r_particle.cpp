//=======================================================================
//			Copyright XashXT Group 2010 ©
//	       	        r_partsystem.cpp - particle manager
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "triangle_api.h"
#include "r_efx.h"
#include "pm_movevars.h"
#include "ev_hldm.h"
#include "hud.h"
#include "r_particle.h"
#include "r_tempents.h"

// particle velocities
static const float r_avertexnormals[NUMVERTEXNORMALS][3] =
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

CParticleSystem	*g_pParticles = NULL;

CParticleSystem :: CParticleSystem( void )
{
	memset( m_pParticles, 0, sizeof( CBaseParticle ) * MAX_PARTICLES );

	m_pFreeParticles = m_pParticles;
	m_pActiveParticles = NULL;
}

CParticleSystem :: ~CParticleSystem( void )
{
}

void CParticleSystem :: Clear( void )
{
	m_pFreeParticles = m_pParticles;
	m_pActiveParticles = NULL;

	for( int i = 0; i < MAX_PARTICLES; i++ )
		m_pParticles[i].m_pNext = &m_pParticles[i+1];

	m_pParticles[MAX_PARTICLES-1].m_pNext = NULL;

	//  this is used for EF_BRIGHTFIELD
	for( i = 0; i < NUMVERTEXNORMALS; i++ )
	{
		m_vecAvelocities[i][0] = RANDOM_LONG( 0, 255 ) * 0.01f;
		m_vecAvelocities[i][1] = RANDOM_LONG( 0, 255 ) * 0.01f;
		m_vecAvelocities[i][2] = RANDOM_LONG( 0, 255 ) * 0.01f;

		// also build avertexnormals
		m_vecAvertexNormals[i] = Vector( r_avertexnormals[i] );
	}

	// build the local copy of particle palette
	for( i = 0; i < 256; i++ )
	{
		float entry[3];

		CL_GetPaletteColor( i, entry );

		m_uchPalette[i][0] = (byte)entry[0];
		m_uchPalette[i][1] = (byte)entry[1];
		m_uchPalette[i][2] = (byte)entry[2];
	}

	// NOTE: shader nomip automatically create default shader
	// with allow change rendermodes
	m_hDefaultParticle = TEX_LoadNoMip( "*particle" );
}

void CParticleSystem :: FreeParticle( CBaseParticle *pCur )
{
	if( pCur->GetType( ) == pt_clientcustom && pCur->pfnDeathFunc )
	{
		// call the deathfunc func before die
		pCur->pfnDeathFunc( pCur );
	}

	pCur->m_pNext = m_pFreeParticles;
	m_pFreeParticles = pCur;
}

CBaseParticle *CParticleSystem :: AllocParticle( HSPRITE m_hSpr )
{
	CBaseParticle	*pAlloc;

	// never alloc particles when we not in game
	if ( IN_GAME() == 0 )
		return NULL;

	if( !m_pFreeParticles )
	{
		Con_Printf( "Overflow %d particles\n", MAX_PARTICLES );
		return NULL;
	}

	pAlloc = m_pFreeParticles;
	m_pFreeParticles = pAlloc->m_pNext;
	pAlloc->m_pNext = m_pActiveParticles;
	m_pActiveParticles = pAlloc;

	// clear old particle
	pAlloc->SetType( pt_static );
	pAlloc->m_hSprite = m_hSpr;
	pAlloc->m_Velocity = g_vecZero;
	pAlloc->m_Pos = g_vecZero;
	pAlloc->m_Ramp = 0;

	return pAlloc;
}

void CParticleSystem :: DrawParticle( HSPRITE hSprite, const Vector &pos, const byte color[4], float size )
{
	// draw the particle
	gEngfuncs.pTriAPI->Enable( TRI_SHADER );

	gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
	gEngfuncs.pTriAPI->Color4ub( color[0], color[1], color[2], color[3] );

	gEngfuncs.pTriAPI->Bind( hSprite, 0 );

	if ( hSprite == m_hDefaultParticle )
	{
		// HACKHACK a scale up to keep particles from disappearing
		size += (pos.x - m_vecOrigin.x) * m_vecForward.x;
		size += (pos.y - m_vecOrigin.y) * m_vecForward.y;
		size += (pos.z - m_vecOrigin.z) * m_vecForward.z;

		if( size < 20.0f ) size = 1.0f;
		else size = 1.0f + size * 0.004f;
	}

	Vector	right, up;

	// scale the axes by radius
	right = m_vecRight * size;
	up = m_vecUp * size;

	// Add the 4 corner vertices.
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	gEngfuncs.pTriAPI->TexCoord2f ( 0.0f, 1.0f );
	gEngfuncs.pTriAPI->Vertex3fv ( pos - right + up );

	gEngfuncs.pTriAPI->TexCoord2f ( 0.0f, 0.0f );
	gEngfuncs.pTriAPI->Vertex3fv ( pos + right + up );

	gEngfuncs.pTriAPI->TexCoord2f ( 1.0f, 0.0f );
	gEngfuncs.pTriAPI->Vertex3fv ( pos + right - up );

	gEngfuncs.pTriAPI->TexCoord2f ( 1.0f, 1.0f );
	gEngfuncs.pTriAPI->Vertex3fv ( pos - right - up );

	gEngfuncs.pTriAPI->End();
		
	gEngfuncs.pTriAPI->Disable( TRI_SHADER );
}

void CParticleSystem :: SimulateAndRender( CBaseParticle *pParticle )
{
	float	ft = GetTimeDelta();
	float	time3 = 15.0 * ft;
	float	time2 = 10.0 * ft;
	float	time1 = 5.0 * ft;
	float	dvel = 4 * ft;

	float grav = ft * gpMovevars->gravity * 0.05f;

	int	iRamp;

	switch( pParticle->GetType( ))
	{
	case pt_static:
		break;

	case pt_clientcustom:
		if( pParticle->pfnCallback )
			pParticle->pfnCallback( pParticle, ft );
		return;

	case pt_fire:
		pParticle->m_Ramp += (word)( time1 * ( 1 << SIMSHIFT ));
		iRamp = pParticle->m_Ramp >> SIMSHIFT;

		if( iRamp >= 6 )
		{
			pParticle->m_flLifetime = -1;
		}
		else
		{
			pParticle->SetColor( ramp3[iRamp] );
		}
		pParticle->m_Velocity[2] += grav;
		break;

	case pt_explode:
		pParticle->m_Ramp += (word)(time2 * ( 1 << SIMSHIFT));
		iRamp = pParticle->m_Ramp >> SIMSHIFT;
		if( iRamp >= 8 )
		{
			pParticle->m_flLifetime = -1;
		}
		else
		{
			pParticle->SetColor( ramp1[iRamp] );
		}
		pParticle->m_Velocity = pParticle->m_Velocity + pParticle->m_Velocity * dvel;
		pParticle->m_Velocity.z -= grav;
		break;

	case pt_explode2:
		pParticle->m_Ramp += (word)(time3 * ( 1 << SIMSHIFT ));
		iRamp = pParticle->m_Ramp >> SIMSHIFT;
		if( iRamp >= 8 )
		{
			pParticle->m_flLifetime = -1;
		}
		else
		{
			pParticle->SetColor( ramp2[iRamp] );
		}
		pParticle->m_Velocity = pParticle->m_Velocity - pParticle->m_Velocity * ft;
		pParticle->m_Velocity.z -= grav;
		break;

	case pt_grav:
		pParticle->m_Velocity.z -= grav * 20;
		break;

	case pt_slowgrav:
		pParticle->m_Velocity.z = grav;
		break;

	case pt_vox_grav:
		pParticle->m_Velocity.z -= grav * 8;
		break;
		
	case pt_vox_slowgrav:
		pParticle->m_Velocity.z -= grav * 4;
		break;
		
	case pt_blob:
	case pt_blob2:
		pParticle->m_Ramp += (word)( time2 * ( 1 << SIMSHIFT ));
		iRamp = pParticle->m_Ramp >> SIMSHIFT;

		if( iRamp >= SPARK_COLORCOUNT )
		{
			pParticle->m_Ramp = 0;
			iRamp = 0;
		}
		
		pParticle->SetColor( gSparkRamp[iRamp] );
		
		pParticle->m_Velocity[0] -= pParticle->m_Velocity[0] * 0.5f * ft;
		pParticle->m_Velocity[1] -= pParticle->m_Velocity[1] * 0.5f * ft;
		pParticle->m_Velocity[2] -= grav * 5;

		if ( RANDOM_LONG( 0, 3 ))
		{
			pParticle->SetType( pt_blob );
			pParticle->SetAlpha(0);
		}
		else
		{
			pParticle->SetType( pt_blob2 );
			pParticle->SetAlpha( 255.9f );
		}
		break;
	}

	HSPRITE hTexture = pParticle->m_hSprite;

	if( hTexture <= 0 )
		hTexture = m_hDefaultParticle;

	// render particle now
	DrawParticle( hTexture, pParticle->m_Pos, pParticle->m_Color, 1.5f );

	// update position.
	pParticle->m_Pos = pParticle->m_Pos + pParticle->m_Velocity * ft;
}
	
void CParticleSystem :: Update( void )
{
	CBaseParticle	*pCur, *pKill;

	m_fOldTime = m_flTime;
	m_flTime = GetClientTime();

	if( !cl_particles->integer ) return;

	cl_entity_t *m_pPlayer = GetLocalPlayer();

	if( !m_pPlayer ) return;

	Vector	view_ofs;

	// calc vectors
	AngleVectors( gHUD.m_vecAngles, m_vecForward, m_vecRight, m_vecUp );

	// calc vieworg
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight( view_ofs );
	m_vecOrigin[0] = gHUD.m_vecOrigin[0] + view_ofs.x;
	m_vecOrigin[1] = gHUD.m_vecOrigin[1] + view_ofs.y;
	m_vecOrigin[2] = gHUD.m_vecOrigin[2] + view_ofs.z;

	while( 1 ) 
	{
		// free time-expired particles
		pKill = m_pActiveParticles;
		if ( pKill && pKill->GetLifetime() < GetClientTime() )
		{
			m_pActiveParticles = pKill->m_pNext;
			FreeParticle( pKill );
			continue;
		}
		break;
	}

	for( pCur = m_pActiveParticles; pCur; pCur = pCur->m_pNext )
	{
		while( 1 )
		{
			pKill = pCur->m_pNext;
			if ( pKill && pKill->GetLifetime() < GetClientTime() )
			{
				pCur->m_pNext = pKill->m_pNext;
				FreeParticle( pKill );
				continue;
			}
			break;
		}

		SimulateAndRender( pCur );
	}
}

/*
===============
CL_EntityParticles

EF_BRIGHTFIELD effect
===============
*/
void CParticleSystem :: EntityParticles( cl_entity_t *ent )
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	float		dist = 64, beamlength = 16;
	Vector		m_vecForward, m_vecPos;	
	CBaseParticle	*p;

	for( int i = 0; i < NUMVERTEXNORMALS; i++ )
	{
		p = AllocParticle ();
		if( !p ) return;

		angle = GetClientTime() * m_vecAvelocities[i].x;
		SinCos( angle, &sy, &cy );
		angle = GetClientTime() * m_vecAvelocities[i].y;
		SinCos( angle, &sp, &cp );
		angle = GetClientTime() * m_vecAvelocities[i].z;
		SinCos( angle, &sr, &cr );
	
		m_vecForward.Init( cp * cy, cp * sy, -sp ); 

		p->SetLifetime( 0.01f );
		p->SetColor( 111 );		// yellow
		p->SetType( pt_explode );

		p->m_Pos = ent->origin + m_vecAvertexNormals[i] * dist + m_vecForward * beamlength;
	}
}

/*
===============
ParticleEffect

PARTICLE_EFFECT on server
===============
*/
void CParticleSystem :: ParticleEffect( const Vector org, const Vector dir, int color, int count )
{
	CBaseParticle	*p;

	if( count == 1024 )
	{
		// Quake hack: count == 255 it's a RocketExplode
		ParticleExplosion( org );
		return;
	}
	
	for( int i = 0; i < count; i++ )
	{
		p = AllocParticle();
		if( !p ) return;

		p->SetLifetime( RANDOM_FLOAT( 0.1, 0.5 ));
		p->SetColor(( color & ~7 ) + RANDOM_LONG( 0, 8 ));
		p->SetType( pt_slowgrav );

		for( int j = 0; j < 3; j++ )
		{
			p->m_Pos[j] = org[j] + RANDOM_FLOAT( -16, 16 );
			p->m_Velocity[j] = dir[j] * 15;
		}
	}
}

/*
===============
CL_ParticleExplosion

===============
*/
void CParticleSystem :: ParticleExplosion( const Vector org )
{
	CBaseParticle	*p;
	
	for( int i = 0; i < 1024; i++ )
	{
		p = AllocParticle();
		if( !p ) return;

		p->SetLifetime( 5.0 );
		p->SetColor( ramp1[0] );
		p->m_Ramp = RANDOM_LONG( 0, 4 );

		if( i & 1 )
		{
			p->SetType( pt_explode );
			for( int j = 0; j < 3; j++ )
			{
				p->m_Pos[j] = org[j] + RANDOM_FLOAT( -16, 16 );
				p->m_Velocity[j] = RANDOM_FLOAT( -256, 256 );
			}
		}
		else
		{
			p->SetType( pt_explode2 );
			for( int j = 0; j < 3; j++ )
			{
				p->m_Pos[j] = org[j] + RANDOM_FLOAT( -16, 16 );
				p->m_Velocity[j] = RANDOM_FLOAT( -256, 256 );
			}
		}
	}
}

/*
===============
CL_ParticleExplosion2

===============
*/
void CParticleSystem :: ParticleExplosion2( const Vector org, int colorStart, int colorLength )
{
	int colorMod = 0;
	CBaseParticle *p;

	for( int i = 0; i < 512; i++ )
	{
		p = AllocParticle();
		if( !p ) return;

		p->SetLifetime ( 0.3f );
		p->SetColor( colorStart + ( colorMod % colorLength ));
		colorMod++;

		p->SetType( pt_blob );
		for ( int j = 0; j < 3; j++ )
		{
			p->m_Pos[j] = org[j] + RANDOM_FLOAT( -16, 16 );
			p->m_Velocity[j] = RANDOM_FLOAT( -256, 256 );
		}
	}
}

/*
===============
CL_BlobExplosion

===============
*/
void CParticleSystem :: BlobExplosion( const Vector org )
{
	CBaseParticle	*p;
	
	for( int i = 0; i < 1024; i++ )
	{
		p = AllocParticle();
		if( !p ) return;

		p->SetLifetime( 1.0f + RANDOM_FLOAT( 0, 0.4f ));

		if( i & 1 )
		{
			p->SetType( pt_blob );
			p->SetColor( 66 + rand() % 6 );

			for( int j = 0; j < 3; j++ )
			{
				p->m_Pos[j] = org[j] + RANDOM_FLOAT( -16, 16 );
				p->m_Velocity[j] = RANDOM_FLOAT( -256, 256 );
			}
		}
		else
		{
			p->SetType( pt_blob2 );
			p->SetColor( 150 + rand() % 6 );

			for( int j = 0; j < 3; j++ )
			{
				p->m_Pos[j] = org[j] + RANDOM_FLOAT( -16, 16 );
				p->m_Velocity[j] = RANDOM_FLOAT( -256, 256 );
			}
		}
	}
}

/*
===============
CL_LavaSplash

===============
*/
void CParticleSystem :: LavaSplash( const Vector org )
{
	CBaseParticle	*p;
	float		vel;
	Vector		dir;

	for ( int i = -16; i < 16; i++ )
	{
		for ( int j = -16; j <16; j++ )
		{
			for ( int k = 0; k < 1; k++ )
			{
				p = AllocParticle();
				if( !p ) return;
		
				p->SetLifetime( 2.0f + RANDOM_FLOAT( 0.0f, 0.65f ));
				p->SetColor( 224 + RANDOM_LONG( 0, 8 ));
				p->SetType( pt_slowgrav );
				
				dir[0] = j * 8 + RANDOM_LONG( 0, 8 );
				dir[1] = i * 8 + RANDOM_LONG( 0, 8 );
				dir[2] = 256;
	
				p->m_Pos[0] = org[0] + dir[0];
				p->m_Pos[1] = org[1] + dir[1];
				p->m_Pos[2] = org[2] + RANDOM_LONG( 0, 64 );
	
				dir = dir.Normalize();
				vel = 50 + RANDOM_LONG( 0, 64 );
				p->m_Velocity = dir * vel;
			}
		}
	}
}

/*
===============
CL_TeleportSplash

===============
*/
void CParticleSystem :: TeleportSplash( const Vector org )
{
	CBaseParticle	*p;
	Vector		dir;

	for( int i = -16; i < 16; i+=4 )
	{
		for( int j = -16; j < 16; j += 4 )
		{
			for( int k = -24; k < 32; k += 4 )
			{
				p = AllocParticle();
				if( !p ) return;
		
				p->SetLifetime( RANDOM_FLOAT( 0.2f, 0.36f ));
				p->SetColor( RANDOM_LONG( 7, 14 ));
				p->SetType( pt_slowgrav );
				
				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;
	
				p->m_Pos[0] = org[0] + i + RANDOM_FLOAT( -4, 4 );
				p->m_Pos[1] = org[1] + j + RANDOM_FLOAT( -4, 4 );
				p->m_Pos[2] = org[2] + k + RANDOM_FLOAT( -4, 4 );
	
				dir = dir.Normalize();
				p->m_Velocity = dir * RANDOM_LONG( 50, 114 );
			}
		}
	}
}

/*
===============
CL_RocketTrail

===============
*/
void CParticleSystem :: RocketTrail( const Vector org, const Vector end, int type )
{
	vec3_t		vec, start;
	float		len;
	CBaseParticle	*p;
	int		j, dec;
	static int	tracercount;

	start = org;
	vec = end - start;
	len = vec.Length();
	vec = vec.Normalize();

	if( type < 128 )
	{
		dec = 3;
	}
	else
	{
		dec = 1;
		type -= 128;
	}

	while( len > 0 )
	{
		len -= dec;

		p = AllocParticle();
		if( !p ) return;
		
		p->SetLifetime( 2.0f );

		switch( type )
		{
		case 0:	// rocket trail
			p->m_Ramp = RANDOM_LONG( 0, 4 );
			p->SetColor( ramp3[p->m_Ramp] );
			p->SetType( pt_fire );
			for( j = 0; j < 3; j++ )
				p->m_Pos[j] = start[j] + ((rand()%6)-3);
			break;
		case 1:	// smoke smoke
			p->m_Ramp = RANDOM_LONG( 2, 6 );
			p->SetColor( ramp3[p->m_Ramp] );
			p->SetType( pt_fire );
			for( j = 0; j < 3; j++ )
				p->m_Pos[j] = start[j] + ((rand() % 6) - 3);
			break;
		case 2:	// blood
			p->SetType( pt_grav );
			p->SetColor( RANDOM_LONG( 67, 71 ));
			for( j = 0; j < 3; j++ )
				p->m_Pos[j] = start[j] + ((rand() % 6) - 3);
			break;
		case 3:
		case 5:	// tracer
			p->SetLifetime( 0.5f );
			p->SetType( pt_static );

			if( type == 3 )
				p->SetColor( 52 + (( tracercount & 4 )<<1 ));
			else p->SetColor( 230 + (( tracercount & 4 )<<1 ));

			tracercount++;
			p->m_Pos = start;

			if( tracercount & 1 )
			{
				p->m_Velocity[0] = 30 *  vec[1];
				p->m_Velocity[1] = 30 * -vec[0];
			}
			else
			{
				p->m_Velocity[0] = 30 * -vec[1];
				p->m_Velocity[1] = 30 *  vec[0];
			}
			break;
		case 4:	// slight blood
			p->SetType( pt_grav );
			p->SetColor( RANDOM_LONG( 67, 71 ));
			for( j = 0; j < 3; j++ )
				p->m_Pos[j] = start[j] + RANDOM_FLOAT( -3, 3 );
			len -= 3;
			break;
		case 6:	// voor trail
			p->SetColor( RANDOM_LONG( 152, 156 ));
			p->SetType( pt_static );
			p->SetLifetime( 0.3f );
			for( j = 0; j < 3; j++ )
				p->m_Pos[j] = start[j] + RANDOM_FLOAT( -16, 16 );
			break;
		}
		start += vec;
	}
}

/*
==============================================================================

	CUSTOM USER PARTICLES

==============================================================================
*/
static void pfnSparkTracerDraw( CBaseParticle *pPart, float frametime )
{
	float	lifePerc = ( pPart->m_flLifetime - GetClientTime() );
	float	scale = pPart->m_flLength;
	Vector	delta = pPart->m_Velocity;

	float	flLength = ( pPart->m_Velocity * scale ).Length();
	float	flWidth = ( flLength < pPart->m_flWidth ) ? flLength : pPart->m_flWidth;

	g_pParticles->DrawTracer(pPart->m_hSprite, pPart->m_Pos, delta * scale, flWidth, pPart->m_Color, 0.0f, 0.8f);

	float grav = frametime * gpMovevars->gravity * 0.05f;
	pPart->m_Velocity.z -= grav * 8; // use vox gravity
	pPart->m_Pos = pPart->m_Pos + pPart->m_Velocity * frametime;
	if( lifePerc < 0.5 ) pPart->SetAlpha( lifePerc * 2 );	// fade alpha
}

#define SPARK_ELECTRIC_MINSPEED	64.0f
#define SPARK_ELECTRIC_MAXSPEED	100.0f

/*
===============
CL_SparkleTracer

===============
*/
void CParticleSystem :: SparkleTracer( const Vector& pos, const Vector& dir )
{
	CBaseParticle *p;

	p = AllocParticle( m_hDefaultParticle );
	if( !p ) return;

	p->SetType( pt_clientcustom );
	p->SetLifetime( RANDOM_FLOAT( 0.5f, 1.0f ));
	p->SetColor( gTracerColors[5] );	// Yellow-Orange
	p->m_Pos = pos;

	p->m_flWidth = RANDOM_FLOAT( 0.35f, 0.5f );
	p->m_flLength = RANDOM_FLOAT( 0.05f, 0.08f );
	p->m_Velocity = dir * RANDOM_FLOAT( SPARK_ELECTRIC_MINSPEED, SPARK_ELECTRIC_MAXSPEED );
	p->pfnCallback = pfnSparkTracerDraw;
}

/*
===============
CL_StreakTracer

===============
*/
void CParticleSystem :: StreakTracer( const Vector& pos, const Vector& velocity, int color )
{
	CBaseParticle *p;

	p = AllocParticle( m_hDefaultParticle );
	if( !p ) return;

	p->SetType( pt_clientcustom );
	p->SetLifetime( RANDOM_FLOAT( 0.5f, 1.0f ));
	p->SetColor( gTracerColors[color] );	// Yellow-Orange
	p->m_Pos = pos;

	p->m_flWidth = RANDOM_FLOAT( 0.45f, 0.65f );
	p->m_flLength = RANDOM_FLOAT( 0.05f, 0.08f );
	p->m_Velocity = velocity;
	p->pfnCallback = pfnSparkTracerDraw;
}

static void pfnBulletTracerDraw( CBaseParticle *pPart, float frametime )
{
	Vector	delta = pPart->m_Velocity * pPart->m_flLength;

	g_pParticles->DrawTracer(pPart->m_hSprite, pPart->m_Pos, delta, pPart->m_flWidth, pPart->m_Color, 0.0f, 0.8f);

	pPart->m_Pos = pPart->m_Pos + pPart->m_Velocity * frametime;
}

/*
===============
CL_BulletTracer

===============
*/
void CParticleSystem :: BulletTracer( const Vector& start, const Vector& end )
{
	CBaseParticle *p;

	p = AllocParticle( m_hDefaultParticle );
	if( !p ) return;

	float vel = ((end - start).Length()) * 16;
	Vector dir = ( end - start ).Normalize();

	p->SetType( pt_clientcustom );
	p->SetLifetime( 0.5f );		// const
	p->SetColor( gTracerColors[0] );	// White tracer
	p->m_Pos = start;

	p->m_flWidth = RANDOM_FLOAT( 0.8f, 1.0f );
	p->m_flLength = RANDOM_FLOAT( 0.05f, 0.06f );
	p->m_Velocity = dir * vel;
	p->pfnCallback = pfnBulletTracerDraw;
}

/*
===============
BulletImpactParticles

===============
*/
void CParticleSystem :: BulletImpactParticles( const Vector &pos )
{
	CBaseParticle	*p;

	// uncomment this to enable texture based particle
	// HSPRITE	sprtex = SPR_Load("sprites/debris/debris_concrete01.spr");

	g_pTempEnts->SparkShower( pos ); // Sparks

	for( int i = 0; i < 25; i++ )
	{
		p = AllocParticle();
		if( !p ) return;
            
		p->SetLifetime( 2.0 );
		p->SetColor( 0, 0, 0 ); // 0,0,0 = black
		p->SetAlpha( 255 );
        
		//p->SetTexture ( sprtex ) ; // Uncomment to enable texture based particle
        
		if( i & 1 )
		{
			p->SetType( pt_grav );
			for( int j = 0; j < 3; j++ )
			{
				p->m_Pos[j] = pos[j] + RANDOM_FLOAT( -2, 3 ); // distance between particles
				p->m_Velocity[j] = RANDOM_FLOAT( -70, 70 ); // speed, velocity of particles
			}
		}
	}
}