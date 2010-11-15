//=======================================================================
//			Copyright XashXT Group 2010 �
//		        cl_part.c - particles and tracers
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

PARTICLES MANAGEMENT

==============================================================
*/

#define NUMVERTEXNORMALS	162
#define SPARK_COLORCOUNT	9
#define TRACER_WIDTH	0.5f	// FIXME: tune this
#define SIMSHIFT		10

// particle velocities
static const float	cl_avertexnormals[NUMVERTEXNORMALS][3] =
{
#include "anorms.h"
};

// particle ramps
static int ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
static int ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };
static int ramp3[6] = { 0x6d, 0x6b, 6, 5, 4, 3 };

static int boxpnt[6][4] =
{
	{ 0, 4, 6, 2 }, // +X
	{ 0, 1, 5, 4 }, // +Y
	{ 0, 2, 3, 1 }, // +Z
	{ 7, 5, 1, 3 }, // -X
	{ 7, 3, 2, 6 }, // -Y
	{ 7, 6, 4, 5 }, // -Z
};

static rgb_t gTracerColors[] =
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

convar_t		*tracerred;
convar_t		*tracergreen;
convar_t		*tracerblue;
convar_t		*traceralpha;
convar_t		*tracerspeed;
convar_t		*tracerlength;
convar_t		*traceroffset;

particle_t	*cl_active_particles;
particle_t	*cl_free_particles;
particle_t	*cl_particles = NULL;	// particle pool
static vec3_t	cl_avelocities[NUMVERTEXNORMALS];
#define		COL_SUM( pal, clr )	(pal - clr) * (pal - clr)

/*
================
CL_LookupColor

find nearest color in particle palette
================
*/
short CL_LookupColor( byte r, byte g, byte b )
{
	int	i, fi, best_color = 0;
	int	f_min = 1000000;
	byte	*pal;
#if 0
	// apply gamma-correction
	r = clgame.ds.gammaTable[r];
	g = clgame.ds.gammaTable[g];
	b = clgame.ds.gammaTable[b];
#endif
	for( i = 0; i < 256; i++ )
	{
		pal = clgame.palette[i];

		fi = 30 * COL_SUM( pal[0], r ) + 59 * COL_SUM( pal[1], g ) + 11 * COL_SUM( pal[2], b );
		if( fi < f_min )
		{
			best_color = i,
			f_min = fi;
		}
	}

	// make sure what color is in-range
	return (word)(best_color & 255);
}

/*
================
CL_GetPackedColor

in hardware mode does nothing
================
*/
void CL_GetPackedColor( short *packed, short color )
{
	if( packed ) *packed = 0;
}

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

	tracerred = Cvar_Get( "tracerred", "0.8", 0, "tracer red component weight ( 0 - 1.0 )" );
	tracergreen = Cvar_Get( "tracergreen", "0.8", 0, "tracer green component weight ( 0 - 1.0 )" );
	tracerblue = Cvar_Get( "tracerblue", "0.4", 0, "tracer blue component weight ( 0 - 1.0 )" );
	traceralpha = Cvar_Get( "traceralpha", "0.5", 0, "tracer alpha amount ( 0 - 1.0 )" );
	tracerspeed = Cvar_Get( "tracerspeed", "6000", 0, "tracer speed" );
	tracerlength = Cvar_Get( "tracerlength", "0.8", 0, "tracer length factor" );
	traceroffset = Cvar_Get( "traceroffset", "30", 0, "tracer starting offset" );
}

/*
================
CL_ClearParticles

================
*/
void CL_ClearParticles( void )
{
	int	i;

	if( !cl_particles ) return;

	cl_free_particles = cl_particles;
	cl_active_particles = NULL;

	for( i = 0; i < GI->max_particles - 1; i++ )
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
	cl_particles = NULL;
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
	p->die = cl.time;
	p->ramp = 0;

	if( callback )
	{
		p->type = pt_clientcustom;
		p->callback = callback;
	}

	return p;
}

static void CL_SparkTracerDraw( particle_t *p, float frametime )
{
	float	lifePerc = p->die - cl.time;
	float	grav = frametime * clgame.movevars.gravity * 0.05f;
	float	length, width;
	int	alpha = 255;
	vec3_t	delta;

	VectorScale( p->vel, p->ramp, delta );
	length = VectorLength( delta );
	width = ( length < TRACER_WIDTH ) ? length : TRACER_WIDTH;
	if( lifePerc < 0.5f ) alpha = (lifePerc * 2) * 255;

	CL_DrawTracer( p->org, delta, width, clgame.palette[p->color], alpha, 0.0f, 0.8f );

	p->vel[2] -= grav * 8; // use vox gravity
	VectorMA( p->org, frametime, p->vel, p->org );
}

static void CL_BulletTracerDraw( particle_t *p, float frametime )
{
	int	alpha = (int)(traceralpha->value * 255);
	float	length, width;
	vec3_t	delta;

	VectorScale( p->vel, p->ramp, delta );
	length = VectorLength( delta );
	width = ( length < TRACER_WIDTH ) ? length : TRACER_WIDTH;

	// bullet tracers used particle palette
	CL_DrawTracer( p->org, delta, width, clgame.palette[p->color], alpha, 0.0f, 0.8f );

	VectorMA( p->org, frametime, p->vel, p->org );
}

/*
================
CL_UpdateParticle

update particle color, position etc
================
*/
void CL_UpdateParticle( particle_t *p, float ft )
{
	float	time3 = 15.0 * ft;
	float	time2 = 10.0 * ft;
	float	time1 = 5.0 * ft;
	float	dvel = 4 * ft;
	float	grav = ft * clgame.movevars.gravity * 0.05f;
	float	size = 1.5f;
	int	i, iRamp, alpha = 255;
	vec3_t	right, up;
	rgb_t	color;

	switch( p->type )
	{
	case pt_static:
		break;
	case pt_clientcustom:
		if( p->callback )
		{
			p->callback( p, ft );
			if( p->callback == CL_BulletTracerDraw )
				return;	// already drawed
			else if( p->callback == CL_SparkTracerDraw )
				return;	// already drawed
		}
		break;
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
	case pt_blob2:
		p->ramp += time2;
		iRamp = (int)p->ramp >> SIMSHIFT;

		if( iRamp >= SPARK_COLORCOUNT )
		{
			p->ramp = 0.0f;
			iRamp = 0;
		}
		
		p->color = CL_LookupColor( gSparkRamp[iRamp][0], gSparkRamp[iRamp][1], gSparkRamp[iRamp][2] );

		for( i = 0; i < 2; i++ )		
			p->vel[i] -= p->vel[i] * 0.5f * ft;
		p->vel[2] -= grav * 5.0f;

		if( Com_RandomLong( 0, 3 ))
		{
			p->type = pt_blob;
			alpha = 0;
		}
		else
		{
			p->type = pt_blob2;
			alpha = 255;
		}
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

#if 0
	// HACKHACK a scale up to keep particles from disappearing
	size += (p->org[0] - cl.refdef.vieworg[0]) * cl.refdef.forward[0];
	size += (p->org[1] - cl.refdef.vieworg[1]) * cl.refdef.forward[1];
	size += (p->org[2] - cl.refdef.vieworg[2]) * cl.refdef.forward[2];

	if( size < 20.0f ) size = 1.0f;
	else size = 1.0f + size * 0.004f;
#endif
 	// scale the axes by radius
	VectorScale( cl.refdef.right, size, right );
	VectorScale( cl.refdef.up, size, up );

	p->color = bound( 0, p->color, 255 );
	VectorSet( color, clgame.palette[p->color][0], clgame.palette[p->color][1], clgame.palette[p->color][2] );

	re->RenderMode( kRenderTransTexture );
	re->Color4ub( color[0], color[1], color[2], alpha );

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

	if( p->type != pt_clientcustom )
	{
		// update position.
		VectorMA( p->org, ft, p->vel, p->org );
	}
}

void CL_DrawParticles( void )
{
	particle_t	*p, *kill;
	float		frametime;

	if( !cl_draw_particles->integer )
		return;

	frametime = cl.time - cl.oldtime;

	if( tracerred->modified || tracergreen->modified || tracerblue->modified )
	{
		gTracerColors[4][0] = (byte)(tracerred->value * 255);
		gTracerColors[4][1] = (byte)(tracergreen->value * 255);
		gTracerColors[4][2] = (byte)(tracerblue->value * 255);
		tracerred->modified = tracergreen->modified = tracerblue->modified = false;
	}

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
===============
CL_EntityParticles

set EF_BRIGHTFIELD effect
===============
*/
void CL_EntityParticles( cl_entity_t *ent )
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward;	
	particle_t	*p;
	int		i;

	for( i = 0; i < NUMVERTEXNORMALS; i++ )
	{
		p = CL_AllocParticle( NULL );
		if( !p ) return;

		angle = cl.time * cl_avelocities[i][0];
		com.sincos( angle, &sy, &cy );
		angle = cl.time * cl_avelocities[i][1];
		com.sincos( angle, &sp, &cp );
		angle = cl.time * cl_avelocities[i][2];
		com.sincos( angle, &sr, &cr );
	
		VectorSet( forward, cp * cy, cp * sy, -sp ); 

		p->die += 0.01f;
		p->color = 111;		// yellow
		p->type = pt_explode;

		p->org[0] = ent->origin[0] + cl_avertexnormals[i][0] * 64.0f + forward[0] * 16.0f;
		p->org[1] = ent->origin[1] + cl_avertexnormals[i][1] * 64.0f + forward[1] * 16.0f;		
		p->org[2] = ent->origin[2] + cl_avertexnormals[i][2] * 64.0f + forward[2] * 16.0f;
	}
}

/*
===============
CL_ParticleExplosion

===============
*/
void CL_ParticleExplosion( const vec3_t org )
{
	particle_t	*p;
	int		i, j;
	int		hSound;

	if( !org ) return;

	hSound = S_RegisterSound( "weapons/explode3.wav" );
	S_StartSound( org, 0, CHAN_AUTO, hSound, VOL_NORM, ATTN_NORM, PITCH_NORM, 0 );
	
	for( i = 0; i < 1024; i++ )
	{
		p = CL_AllocParticle( NULL );
		if( !p ) return;

		p->die += 5.0f;
		p->color = ramp1[0];
		p->ramp = Com_RandomLong( 0, 4 );

		if( i & 1 )
		{
			p->type = pt_explode;
			for( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + Com_RandomFloat( -16.0f, 16.0f );
				p->vel[j] = Com_RandomFloat( -256.0f, 256.0f );
			}
		}
		else
		{
			p->type = pt_explode2;
			for( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + Com_RandomFloat( -16.0f, 16.0f );
				p->vel[j] = Com_RandomFloat( -256.0f, 256.0f );
			}
		}
	}
}

/*
===============
CL_ParticleExplosion2

===============
*/
void CL_ParticleExplosion2( const vec3_t org, int colorStart, int colorLength )
{
	int		i, j;
	int		colorMod = 0;
	particle_t	*p;
	int		hSound;

	if( !org ) return;

	hSound = S_RegisterSound( "weapons/explode3.wav" );
	S_StartSound( org, 0, CHAN_AUTO, hSound, VOL_NORM, ATTN_NORM, PITCH_NORM, 0 );

	for( i = 0; i < 512; i++ )
	{
		p = CL_AllocParticle( NULL );
		if( !p ) return;

		p->die += 0.3f;
		p->color = colorStart + ( colorMod % colorLength );
		colorMod++;

		p->type = pt_blob;

		for( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j] + Com_RandomFloat( -16.0f, 16.0f );
			p->vel[j] = Com_RandomFloat( -256.0f, 256.0f );
		}
	}
}

/*
===============
CL_BlobExplosion

===============
*/
void CL_BlobExplosion( const vec3_t org )
{
	particle_t	*p;
	int		i, j;
	int		hSound;

	if( !org ) return;

	hSound = S_RegisterSound( "weapons/explode3.wav" );
	S_StartSound( org, 0, CHAN_AUTO, hSound, VOL_NORM, ATTN_NORM, PITCH_NORM, 0 );
	
	for( i = 0; i < 1024; i++ )
	{
		p = CL_AllocParticle( NULL );
		if( !p ) return;

		p->die += 1.0f + Com_RandomFloat( 0, 0.4f );

		if( i & 1 )
		{
			p->type = pt_explode;
			p->color = 66 + rand() % 6;

			for( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + Com_RandomFloat( -16.0f, 16.0f );
				p->vel[j] = Com_RandomFloat( -256.0f, 256.0f );
			}
		}
		else
		{
			p->type = pt_explode2;
			p->color = 150 + rand() % 6;

			for( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + Com_RandomFloat( -16.0f, 16.0f );
				p->vel[j] = Com_RandomFloat( -256.0f, 256.0f );
			}
		}
	}
}

/*
===============
ParticleEffect

PARTICLE_EFFECT on server
===============
*/
void CL_RunParticleEffect( const vec3_t org, const vec3_t dir, int color, int count )
{
	particle_t	*p;
	int		i, j;

	if( count == 1024 )
	{
		// Quake hack: count == 255 it's a RocketExplode
		CL_ParticleExplosion( org );
		return;
	}
	
	for( i = 0; i < count; i++ )
	{
		p = CL_AllocParticle( NULL );
		if( !p ) return;

		p->die += Com_RandomFloat( 0.1f, 0.5f );
		p->color = ( color & ~7 ) + Com_RandomLong( 0, 8 );
		p->type = pt_slowgrav;

		for( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j] + Com_RandomFloat( -16, 16 );
			p->vel[j] = dir[j] * 15;
		}
	}
}

/*
===============
CL_Blood

particle spray
===============
*/
void CL_Blood( const vec3_t org, const vec3_t dir, int pcolor, int speed )
{
	particle_t	*p;
	int		i, j;

	for( i = 0; i < speed * 20; i++ )
	{
		p = CL_AllocParticle( NULL );
		if( !p ) return;

		p->die += Com_RandomFloat( 0.1f, 0.5f );
		p->type = pt_slowgrav;
		p->color = pcolor;

		for( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j] + Com_RandomFloat( -16.0f, 16.0f );
			p->vel[j] = dir[j] * speed;
		}
	}
}

/*
===============
CL_BloodStream

particle spray 2
===============
*/
void CL_BloodStream( const vec3_t org, const vec3_t dir, int pcolor, int speed )
{
	particle_t	*p;
	int		i, j;

	for( i = 0; i < speed * 20; i++ )
	{
		p = CL_AllocParticle( NULL );
		if( !p ) return;

		p->die += Com_RandomFloat( 0.2f, 0.8f );
		p->type = pt_slowgrav;
		p->color = pcolor;

		for( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j];
			p->vel[j] = dir[j] * speed;
		}
	}
}

/*
===============
CL_LavaSplash

===============
*/
void CL_LavaSplash( const vec3_t org )
{
	particle_t	*p;
	float		vel;
	vec3_t		dir;
	int		i, j, k;

	for( i = -16; i < 16; i++ )
	{
		for( j = -16; j <16; j++ )
		{
			for( k = 0; k < 1; k++ )
			{
				p = CL_AllocParticle( NULL );
				if( !p ) return;

				p->die += 2.0f + Com_RandomFloat( 0.0f, 0.65f );
				p->color = 224 + Com_RandomLong( 0, 8 );
				p->type = pt_slowgrav;
				
				dir[0] = j * 8 + Com_RandomLong( 0, 8 );
				dir[1] = i * 8 + Com_RandomLong( 0, 8 );
				dir[2] = 256;

				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + Com_RandomLong( 0, 64 );

				VectorNormalize( dir );
				vel = 50 + Com_RandomLong( 0, 64 );
				VectorScale( dir, vel, p->vel );
			}
		}
	}
}

/*
===============
CL_ParticleBurst

===============
*/
void CL_ParticleBurst( const vec3_t org, int size, int color, float life )
{
	particle_t	*p;
	float		vel;
	vec3_t		dir;
	int		i, j, k;

	for( i = -size; i < size; i++ )
	{
		for( j = -size; j < size; j++ )
		{
			for( k = 0; k < 1; k++ )
			{
				p = CL_AllocParticle( NULL );
				if( !p ) return;

				p->die += life + Com_RandomFloat( 0.0f, 0.1f );
				p->color = color;
				p->type = pt_slowgrav;
				
				dir[0] = j * 8 + Com_RandomLong( 0, 8 );
				dir[1] = i * 8 + Com_RandomLong( 0, 8 );
				dir[2] = 256;

				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + Com_RandomLong( 0, 64 );

				VectorNormalize( dir );
				vel = 50 + Com_RandomLong( 0, 64 );
				VectorScale( dir, vel, p->vel );
			}
		}
	}
}

/*
===============
CL_TeleportSplash

===============
*/
void CL_TeleportSplash( const vec3_t org )
{
	particle_t	*p;
	vec3_t		dir;
	int		i, j, k;

	for( i = -16; i < 16; i += 4 )
	{
		for( j = -16; j < 16; j += 4 )
		{
			for( k = -24; k < 32; k += 4 )
			{
				p = CL_AllocParticle( NULL );
				if( !p ) return;
		
				p->die += Com_RandomFloat( 0.2f, 0.36f );
				p->color = Com_RandomLong( 7, 14 );
				p->type = pt_slowgrav;
				
				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;
	
				p->org[0] = org[0] + i + Com_RandomFloat( -4.0f, 4.0f );
				p->org[1] = org[1] + j + Com_RandomFloat( -4.0f, 4.0f );
				p->org[2] = org[2] + k + Com_RandomFloat( -4.0f, 4.0f );
	
				VectorNormalize( dir );
				VectorScale( dir, Com_RandomLong( 50, 114 ), p->vel );
			}
		}
	}
}

/*
===============
CL_RocketTrail

===============
*/
void CL_RocketTrail( vec3_t start, vec3_t end, int type )
{
	vec3_t		vec;
	float		len;
	particle_t	*p;
	int		j, dec;
	static int	tracercount;

	VectorSubtract( end, start, vec );
	len = VectorNormalizeLength( vec );

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

		p = CL_AllocParticle( NULL );
		if( !p ) return;
		
		p->die += 2.0f;

		switch( type )
		{
		case 0:	// rocket trail
			p->ramp = Com_RandomLong( 0, 4 );
			p->color = ramp3[(int)p->ramp];
			p->type = pt_fire;
			for( j = 0; j < 3; j++ )
				p->org[j] = start[j] + ((rand() % 6 ) - 3 );
			break;
		case 1:	// smoke smoke
			p->ramp = Com_RandomLong( 2, 6 );
			p->color = ramp3[(int)p->ramp];
			p->type = pt_fire;
			for( j = 0; j < 3; j++ )
				p->org[j] = start[j] + ((rand() % 6 ) - 3 );
			break;
		case 2:	// blood
			p->type = pt_grav;
			p->color = Com_RandomLong( 67, 71 );
			for( j = 0; j < 3; j++ )
				p->org[j] = start[j] + ((rand() % 6 ) - 3 );
			break;
		case 3:
		case 5:	// tracer
			p->die += 0.5f;
			p->type = pt_static;

			if( type == 3 ) p->color = 52 + (( tracercount & 4 )<<1 );
			else p->color = 230 + (( tracercount & 4 )<<1 );

			tracercount++;
			VectorCopy( start, p->org );

			if( tracercount & 1 )
			{
				p->vel[0] = 30 *  vec[1];
				p->vel[1] = 30 * -vec[0];
			}
			else
			{
				p->vel[0] = 30 * -vec[1];
				p->vel[1] = 30 *  vec[0];
			}
			break;
		case 4:	// slight blood
			p->type = pt_grav;
			p->color = Com_RandomLong( 67, 71 );
			for( j = 0; j < 3; j++ )
				p->org[j] = start[j] + Com_RandomFloat( -3.0f, 3.0f );
			len -= 3;
			break;
		case 6:	// voor trail
			p->color = Com_RandomLong( 152, 156 );
			p->type = pt_static;
			p->die += 0.3f;
			for( j = 0; j < 3; j++ )
				p->org[j] = start[j] + Com_RandomFloat( -16.0f, 16.0f );
			break;
		}
		VectorAdd( start, vec, start );
	}
}

/*
================
CL_DrawLine

================
*/
static void CL_DrawLine( const vec3_t start, const vec3_t end, int pcolor, float life, float gap )
{
	particle_t	*p;
	float		len, curdist;
	vec3_t		diff;
	int		i;

	// Determine distance;
	VectorSubtract( end, start, diff );
	len = VectorNormalizeLength( diff );
	curdist = 0;

	while( curdist <= len )
	{
		p = CL_AllocParticle( NULL );
		if( !p ) return;

		for( i = 0; i < 3; i++ )
			p->org[i] = start[i] + curdist * diff[i];

		p->color = pcolor;
		p->type = pt_static;
		p->die += life;
		curdist += gap;
	}
}

/*
================
CL_DrawRectangle

================
*/
void CL_DrawRectangle( const vec3_t tl, const vec3_t bl, const vec3_t tr, const vec3_t br, int pcolor, float life )
{
	CL_DrawLine( tl, bl, pcolor, life, 2.0f );
	CL_DrawLine( bl, br, pcolor, life, 2.0f );
	CL_DrawLine( br, tr, pcolor, life, 2.0f );
	CL_DrawLine( tr, tl, pcolor, life, 2.0f );
}

void CL_ParticleLine( const vec3_t start, const vec3_t end, byte r, byte g, byte b, float life )
{
	int	pcolor;

	pcolor = CL_LookupColor( r, g, b );
	CL_DrawLine( start, end, pcolor, life, 2.0f );
}

/*
================
CL_ParticleBox

================
*/
void CL_ParticleBox( const vec3_t mins, const vec3_t maxs, byte r, byte g, byte b, float life )
{
	vec3_t	tmp, p[8];
	int	i, col;

	col = CL_LookupColor( r, g, b );

	for( i = 0; i < 8; i++ )
	{
		tmp[0] = (i & 1) ? mins[0] : maxs[0];
		tmp[1] = (i & 2) ? mins[1] : maxs[1];
		tmp[2] = (i & 4) ? mins[2] : maxs[2];
		VectorCopy( tmp, p[i] );
	}

	for( i = 0; i < 6; i++ )
	{
		CL_DrawRectangle( p[boxpnt[i][1]], p[boxpnt[i][0]], p[boxpnt[i][2]], p[boxpnt[i][3]], col, life );
	}

}

/*
================
CL_ShowLine

================
*/
void CL_ShowLine( const vec3_t start, const vec3_t end )
{
	int	pcolor;

	pcolor = CL_LookupColor( 192, 0, 0 );
	CL_DrawLine( start, end, pcolor, 30.0f, 5.0f );
}

/*
===============
CL_BulletImpactParticles

===============
*/
void CL_BulletImpactParticles( const vec3_t org )
{
	particle_t	*p;
	vec3_t		pos, dir;
	int		i, j;

	// do sparks
	// randomize position
	pos[0] = org[0] + Com_RandomFloat( -2.0f, 2.0f );
	pos[1] = org[1] + Com_RandomFloat( -2.0f, 2.0f );
	pos[2] = org[2] + Com_RandomFloat( -2.0f, 2.0f );

	// create a 8 random spakle tracers
	for( i = 0; i < 8; i++ )
	{
		dir[0] = Com_RandomFloat( -1.0f, 1.0f );
		dir[1] = Com_RandomFloat( -1.0f, 1.0f );
		dir[2] = Com_RandomFloat( -1.0f, 1.0f );

		CL_SparkleTracer( pos, dir );
	}

	for( i = 0; i < 25; i++ )
	{
		p = CL_AllocParticle( NULL );
		if( !p ) return;
            
		p->die += 2.0f;
		p->color = 0; // black

		if( i & 1 )
		{
			p->type = pt_grav;
			for( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + Com_RandomFloat( -2.0f, 3.0f );
				p->vel[j] = Com_RandomFloat( -70.0f, 70.0f );
			}
		}
	}
}

/*
===============
CL_FlickerParticles

===============
*/
void CL_FlickerParticles( const vec3_t org )
{
	particle_t	*p;
	int		i, j;

	for( i = 0; i < 16; i++ )
	{
		p = CL_AllocParticle( NULL );
		if( !p ) return;

		p->die += Com_RandomFloat( 0.5f, 2.0f );
		p->type = pt_blob;

		for( j = 0; j < 3; j++ )
			p->org[j] = org[j] + Com_RandomFloat( -32.0f, 32.0f );
		p->vel[2] = Com_RandomFloat( 64.0f, 100.0f );
	}
}

/*
==============================================================

TRACERS MANAGEMENT (particle extension)

==============================================================
*/
/*
================
CL_CullTracer

check tracer bbox
================
*/
static qboolean CL_CullTracer( const vec3_t start, const vec3_t end )
{
	vec3_t	mins, maxs;
	int	i;

	if( !re ) return true;	// culled

	// compute the bounding box
	for( i = 0; i < 3; i++ )
	{
		if( start[i] < end[i] )
		{
			mins[i] = start[i];
			maxs[i] = end[i];
		}
		else
		{
			mins[i] = end[i];
			maxs[i] = start[i];
		}
		
		// don't let it be zero sized
		if( mins[i] == maxs[i] )
		{
			maxs[i] += 1;
		}
	}

	// check bbox
	return re->CullBox( mins, maxs );
}

/*
================
CL_TracerComputeVerts

compute four vertexes in screen space
================
*/
qboolean CL_TracerComputeVerts( const vec3_t start, const vec3_t delta, float width, vec3_t *pVerts )
{
	vec3_t	end, screenStart, screenEnd;
	vec3_t	tmp, tmp2, normal;

	VectorAdd( start, delta, end );

	// clip the tracer
	if( CL_CullTracer( start, end ))
		return false;

	// transform point into the screen space
	TriWorldToScreen( (float *)start, screenStart );
	TriWorldToScreen( (float *)end, screenEnd );

	VectorSubtract( screenStart, screenEnd, tmp );	
	tmp[2] = 0; // we don't need Z, we're in screen space

	VectorNormalize( tmp );

	// build point along noraml line (normal is -y, x)
	VectorScale( cl.refdef.up, tmp[0], normal );
	VectorScale( cl.refdef.right, -tmp[1], tmp2 );
	VectorSubtract( normal, tmp2, normal );

	// compute four vertexes
	VectorScale( normal, width, tmp );
	VectorSubtract( start, tmp, pVerts[0] ); 
	VectorAdd( start, tmp, pVerts[1] ); 
	VectorAdd( pVerts[0], delta, pVerts[2] ); 
	VectorAdd( pVerts[1], delta, pVerts[3] ); 

	return true;
}


/*
================
CL_DrawTracer

draws a single tracer
================
*/
void CL_DrawTracer( vec3_t start, vec3_t delta, float width, rgb_t color, int alpha, float startV, float endV )
{
	// Clip the tracer
	vec3_t	verts[4];

	if ( !CL_TracerComputeVerts( start, delta, width, verts ))
		return;

	// NOTE: Gotta get the winding right so it's not backface culled
	// (we need to turn of backface culling for these bad boys)

	re->RenderMode( kRenderTransTexture );

	re->Color4ub( color[0], color[1], color[2], alpha );

	re->Bind( cls.particleShader, 0 );
	re->Begin( TRI_QUADS );

	re->TexCoord2f( 0.0f, endV );
	TriVertex3fv( verts[2] );

	re->TexCoord2f( 1.0f, endV );
	TriVertex3fv( verts[3] );

	re->TexCoord2f( 1.0f, startV );
	TriVertex3fv( verts[1] );

	re->TexCoord2f( 0.0f, startV );
	TriVertex3fv( verts[0] );

	re->End();
}

/*
===============
CL_SparkleTracer

===============
*/
void CL_SparkleTracer( const vec3_t pos, const vec3_t dir )
{
	particle_t	*p;
	byte		*color;

	p = CL_AllocParticle( CL_SparkTracerDraw );
	if( !p ) return;

	color = gTracerColors[5];	// Yellow-Orange
	VectorCopy( pos, p->org );
	p->die += Com_RandomFloat( 0.45f, 0.7f );
	p->color = CL_LookupColor( color[0], color[1], color[2] );
	p->ramp = Com_RandomFloat( 0.07f, 0.08f );	// ramp used as tracer length
	VectorScale( dir, Com_RandomFloat( SPARK_ELECTRIC_MINSPEED, SPARK_ELECTRIC_MAXSPEED ), p->vel );
}

/*
===============
CL_StreakTracer

===============
*/
void CL_StreakTracer( const vec3_t pos, const vec3_t velocity, int colorIndex )
{
	particle_t	*p;
	byte		*color;

	p = CL_AllocParticle( CL_SparkTracerDraw );
	if( !p ) return;

	color = gTracerColors[colorIndex];
	p->die += Com_RandomFloat( 0.5f, 1.0f );
	VectorCopy( velocity, p->vel );
	VectorCopy( pos, p->org );
	p->color = CL_LookupColor( color[0], color[1], color[2] );
	p->ramp = Com_RandomFloat( 0.05f, 0.08f );
}

/*
===============
CL_TracerEffect

===============
*/
void CL_TracerEffect( const vec3_t start, const vec3_t end )
{
	particle_t	*p;
	vec3_t		dir;
	byte		*color;
	float		length;

	p = CL_AllocParticle( CL_BulletTracerDraw );
	if( !p ) return;

	color = gTracerColors[4];

	VectorSubtract( end, start, dir );
	length = VectorNormalizeLength( dir );
	VectorScale( dir, tracerspeed->value, p->vel );
	VectorMA( start, traceroffset->value, dir, p->org ); // make some offset
	p->color = CL_LookupColor( color[0], color[1], color[2] );
	p->die += ( length / tracerspeed->value );
	p->ramp = tracerlength->value; // ramp used as length
}

/*
===============
CL_TracerEffect

===============
*/
void CL_UserTracerParticle( float *org, float *vel, float life, int colorIndex, float length, byte deathcontext,
	void (*deathfunc)( particle_t *p ))
{
	particle_t	*p;
	byte		*color;

	p = CL_AllocParticle( CL_BulletTracerDraw );
	if( !p ) return;

	color = gTracerColors[colorIndex];
	p->color = CL_LookupColor( color[0], color[1], color[2] );
	p->ramp = length; // ramp used as length
	VectorCopy( org, p->org );
	VectorCopy( vel, p->vel );
	p->context = deathcontext;
	p->deathfunc = deathfunc;
	p->die += life;
}

/*
===============
CL_TracerParticles

allow more customization
===============
*/
particle_t *CL_TracerParticles( float *org, float *vel, float life )
{
	particle_t	*p;

	p = CL_AllocParticle( CL_BulletTracerDraw );
	if( !p ) return NULL;
	VectorCopy( org, p->org );
	VectorCopy( vel, p->vel );
	p->die += life;

	return p;
}

/*
===============
CL_SparkShower

Creates 8 random tracers
===============
*/
void CL_SparkShower( const vec3_t org )
{
	vec3_t	pos, dir;
	model_t	*pmodel;
	int	i;

	// randomize position
	pos[0] = org[0] + Com_RandomFloat( -2.0f, 2.0f );
	pos[1] = org[1] + Com_RandomFloat( -2.0f, 2.0f );
	pos[2] = org[2] + Com_RandomFloat( -2.0f, 2.0f );

	pmodel = CM_ClipHandleToModel( CL_FindModelIndex( "sprites/richo1.spr" ));
	CL_RicochetSprite( pos, pmodel, 0.0f, Com_RandomFloat( 0.4, 0.6f ));

	// create a 8 random spakle tracers
	for( i = 0; i < 8; i++ )
	{
		dir[0] = Com_RandomFloat( -1.0f, 1.0f );
		dir[1] = Com_RandomFloat( -1.0f, 1.0f );
		dir[2] = Com_RandomFloat( -1.0f, 1.0f );

		CL_SparkleTracer( pos, dir );
	}
}

/*
===============
CL_Implosion

===============
*/
void CL_Implosion( const vec3_t end, float radius, int count, float life )
{
	particle_t	*p;
	vec3_t		dir, dest;
	vec3_t		m_vecPos;
	float		flDist, vel;
	int		i, j, colorIndex;
	int		step;

	colorIndex = CL_LookupColor( gTracerColors[5][0], gTracerColors[5][1], gTracerColors[5][2] );
	step = count / 4;

	for( i = -radius; i <= radius; i += step )
	{
		for( j = -radius; j <= radius; j += step )
		{
			p = CL_AllocParticle( CL_SparkTracerDraw );
			if( !p ) return;

			VectorCopy( end, m_vecPos );

			dest[0] = end[0] + i;
			dest[1] = end[1] + j;
			dest[2] = end[2] + Com_RandomFloat( 100, 800 );

			// send particle heading to dest at a random speed
			VectorSubtract( dest, m_vecPos, dir );

			vel = dest[2] / 8;// velocity based on how far particle has to travel away from org

			flDist = VectorNormalizeLength( dir );	// save the distance
			if( vel < 64 ) vel = 64;
				
			vel += Com_RandomFloat( 64, 128  );
			life += ( flDist / vel );

			VectorCopy( m_vecPos, p->org );
			p->color = colorIndex;
			p->ramp = 1.0f; // length based on velocity

			VectorScale( dir, vel, p->vel );
			// die right when you get there
			p->die += life;
		}
	}
}