/*
cl_part.c - particles and tracers
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
#include "client.h"
#include "gl_local.h"
#include "r_efx.h"
#include "event_flags.h"
#include "entity_types.h"
#include "triangleapi.h"
#include "cl_tent.h"
#include "studio.h"

// particle velocities
static const float cl_avertexnormals[NUMVERTEXNORMALS][3] =
{
#include "anorms.h"
};

/*
==============================================================

PARTICLES MANAGEMENT

==============================================================
*/
// particle ramps
static int ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
static int ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };
static int ramp3[6] = { 0x6d, 0x6b, 6, 5, 4, 3 };
static float gTracerSize[11] = { 1.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
static int gSparkRamp[9] = { 0xfe, 0xfd, 0xfc, 0x6f, 0x6e, 0x6d, 0x6c, 0x67, 0x60 };

static color24 gTracerColors[] =
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

static int boxpnt[6][4] =
{
{ 0, 4, 6, 2 }, // +X
{ 0, 1, 5, 4 }, // +Y
{ 0, 2, 3, 1 }, // +Z
{ 7, 5, 1, 3 }, // -X
{ 7, 3, 2, 6 }, // -Y
{ 7, 6, 4, 5 }, // -Z
};

convar_t		*tracerred;
convar_t		*tracergreen;
convar_t		*tracerblue;
convar_t		*traceralpha;
convar_t		*tracerspeed;
convar_t		*tracerlength;
convar_t		*traceroffset;

particle_t	*cl_active_particles;
particle_t	*cl_active_tracers;
particle_t	*cl_free_particles;
particle_t	*cl_particles = NULL;	// particle pool
static vec3_t	cl_avelocities[NUMVERTEXNORMALS];

/*
================
CL_LookupColor

find nearest color in particle palette
================
*/
short CL_LookupColor( byte r, byte g, byte b )
{
	int	i, best;
	float	diff, bestdiff;
	float	rf, gf, bf;

	bestdiff = 999999;
	best = 65535;

	for( i = 0; i < 256; i++ )
	{
		rf = r - clgame.palette[i].r;
		gf = g - clgame.palette[i].g;
		bf = b - clgame.palette[i].b;

		// convert color to monochrome
		diff = rf * (rf * 0.2) + gf * (gf * 0.5) + bf * (bf * 0.3);

		if ( diff < bestdiff )
		{
			bestdiff = diff;
			best = i;
		}
	}

	return best;
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
		cl_avelocities[i][0] = COM_RandomLong( 0, 255 ) * 0.01f;
		cl_avelocities[i][1] = COM_RandomLong( 0, 255 ) * 0.01f;
		cl_avelocities[i][2] = COM_RandomLong( 0, 255 ) * 0.01f;
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
	cl_active_tracers = NULL;

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
	if( cl_particles )
		Mem_Free( cl_particles );
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
	if( p->deathfunc )
	{
		// call right the deathfunc before die
		p->deathfunc( p );
		p->deathfunc = NULL;
	}

	p->next = cl_free_particles;
	cl_free_particles = p;
}

/*
================
R_AllocParticle

can return NULL if particles is out
================
*/
particle_t *R_AllocParticle( void (*callback)( particle_t*, float ))
{
	particle_t	*p;

	if( !cl_draw_particles->value )
		return NULL;

	// never alloc particles when we not in game
	if( tr.frametime == 0.0 ) return NULL;

	if( !cl_free_particles )
	{
		MsgDev( D_ERROR, "Overflow %d particles\n", GI->max_particles );
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
	p->packedColor = 0;
	p->die = cl.time;
	p->color = 0;
	p->ramp = 0;

	if( callback )
	{
		p->type = pt_clientcustom;
		p->callback = callback;
	}

	return p;
}

/*
================
R_AllocTracer

can return NULL if particles is out
================
*/
particle_t *R_AllocTracer( const vec3_t org, const vec3_t vel, float life )
{
	particle_t	*p;

	if( !cl_draw_tracers->value )
		return NULL;

	// never alloc particles when we not in game
	if( tr.frametime == 0.0 ) return NULL;

	if( !cl_free_particles )
	{
		MsgDev( D_ERROR, "Overflow %d tracers\n", GI->max_particles );
		return NULL;
	}

	p = cl_free_particles;
	cl_free_particles = p->next;
	p->next = cl_active_tracers;
	cl_active_tracers = p;

	// clear old particle
	p->type = pt_static;
	VectorCopy( org, p->org );
	VectorCopy( vel, p->vel );
	p->die = cl.time + life;
	p->ramp = tracerlength->value;
	p->color = 4; // select custom color
	p->packedColor = 255; // alpha

	return p;
}

/*
==============
R_FreeDeadParticles

Free particles that time has expired
==============
*/
void R_FreeDeadParticles( particle_t **ppparticles )
{
	particle_t	*p, *kill;

	// kill all the ones hanging direcly off the base pointer
	while( 1 ) 
	{
		kill = *ppparticles;
		if( kill && kill->die < cl.time )
		{
			if( kill->deathfunc )
				kill->deathfunc( kill );
			kill->deathfunc = NULL;
			*ppparticles = kill->next;
			kill->next = cl_free_particles;
			cl_free_particles = kill;
			continue;
		}
		break;
	}

	// kill off all the others
	for( p = *ppparticles; p; p = p->next )
	{
		while( 1 )
		{
			kill = p->next;
			if( kill && kill->die < cl.time )
			{
				if( kill->deathfunc )
					kill->deathfunc( kill );
				kill->deathfunc = NULL;
				p->next = kill->next;
				kill->next = cl_free_particles;
				cl_free_particles = kill;
				continue;
			}
			break;
		}
	}
}

static void CL_BulletTracerDraw( particle_t *p, float frametime )
{
	vec3_t	lineDir, viewDir, cross;
	vec3_t	vecEnd, vecStart, vecDir;
	float	sDistance, eDistance, totalDist;
	float	dDistance, dTotal, fOffset;
	int	alpha = (int)(traceralpha->value * 255);
	float	width = 3.0f, life, frac, length;
	vec3_t	tmp;

	// calculate distance
	VectorCopy( p->vel, vecDir );
	totalDist = VectorNormalizeLength( vecDir );

	length = p->ramp; // ramp used as length

	// calculate fraction
	life = ( totalDist + length ) / ( max( 1.0f, tracerspeed->value ));
	frac = life - ( p->die - cl.time ) + frametime;

	// calculate our distance along our path
	sDistance = tracerspeed->value * frac;
	eDistance = sDistance - length;
	
	// clip to start
	sDistance = max( 0.0f, sDistance );
	eDistance = max( 0.0f, eDistance );

	if(( sDistance == 0.0f ) && ( eDistance == 0.0f ))
		return;

	// clip it
	if( totalDist != 0.0f )
	{
		sDistance = min( sDistance, totalDist );
		eDistance = min( eDistance, totalDist );
	}

	// get our delta to calculate the tc offset
	dDistance	= fabs( sDistance - eDistance );
	dTotal = ( length != 0.0f ) ? length : 0.01f;
	fOffset = ( dDistance / dTotal );

	// find our points along our path
	VectorMA( p->org, sDistance, vecDir, vecEnd );
	VectorMA( p->org, eDistance, vecDir, vecStart );

	// setup our info for drawing the line
	VectorSubtract( vecEnd, vecStart, lineDir );
	VectorSubtract( vecEnd, RI.vieworg, viewDir );
	
	CrossProduct( lineDir, viewDir, cross );
	VectorNormalize( cross );

	GL_SetRenderMode( kRenderTransTexture );

	GL_Bind( GL_TEXTURE0, cls.particleImage );
	pglBegin( GL_QUADS );

	pglColor4ub( clgame.palette[p->color].r, clgame.palette[p->color].g, clgame.palette[p->color].b, alpha );

	VectorMA( vecStart, -width, cross, tmp );	
	pglTexCoord2f( 1.0f, 0.0f );
	pglVertex3fv( tmp );

	VectorMA( vecStart, width, cross, tmp );
	pglTexCoord2f( 0.0f, 0.0f );
	pglVertex3fv( tmp );

	VectorMA( vecEnd, width, cross, tmp );
	pglTexCoord2f( 0.0f, fOffset );
	pglVertex3fv( tmp );

	VectorMA( vecEnd, -width, cross, tmp );
	pglTexCoord2f( 1.0f, fOffset );
	pglVertex3fv( tmp );

	pglEnd();
}

/*
================
CL_DrawParticles

update particle color, position, free expired and draw it
================
*/
void CL_DrawParticles( double frametime )
{
	particle_t	*p;
	float		time3 = 15.0f * frametime;
	float		time2 = 10.0f * frametime;
	float		time1 = 5.0f * frametime;
	float		dvel = 4.0f * frametime;
	float		grav = frametime * clgame.movevars.gravity * 0.05f;
	float		size;
	vec3_t		right, up;
	color24		*pColor;

	if( !cl_draw_particles->value )
		return;

	R_FreeDeadParticles( &cl_active_particles );

	if( !cl_active_particles )
		return;	// nothing to draw?

	GL_SetRenderMode( kRenderTransTexture );
	GL_Bind( GL_TEXTURE0, cls.particleImage );
	pglBegin( GL_QUADS );

	for( p = cl_active_particles; p; p = p->next )
	{
		if( p->type != pt_blob )
		{
			size = 1.5f; // get initial size of particle

			// HACKHACK a scale up to keep particles from disappearing
			size += (p->org[0] - RI.vieworg[0]) * RI.vforward[0];
			size += (p->org[1] - RI.vieworg[1]) * RI.vforward[1];
			size += (p->org[2] - RI.vieworg[2]) * RI.vforward[2];

			if( size < 20.0f ) size = 1.0f;
			else size = 1.0f + size * 0.004f;

			// scale the axes by radius
			VectorScale( RI.vright, size, right );
			VectorScale( RI.vup, size, up );

			p->color = bound( 0, p->color, 255 );
			pColor = &clgame.palette[p->color];
			// FIXME: should we pass color through lightgamma table?

			pglColor4ub( pColor->r, pColor->g, pColor->b, 255 );

			pglTexCoord2f( 0.0f, 1.0f );
			pglVertex3f( p->org[0] - right[0] + up[0], p->org[1] - right[1] + up[1], p->org[2] - right[2] + up[2] );
			pglTexCoord2f( 0.0f, 0.0f );
			pglVertex3f( p->org[0] + right[0] + up[0], p->org[1] + right[1] + up[1], p->org[2] + right[2] + up[2] );
			pglTexCoord2f( 1.0f, 0.0f );
			pglVertex3f( p->org[0] + right[0] - up[0], p->org[1] + right[1] - up[1], p->org[2] + right[2] - up[2] );
			pglTexCoord2f( 1.0f, 1.0f );
			pglVertex3f( p->org[0] - right[0] - up[0], p->org[1] - right[1] - up[1], p->org[2] - right[2] - up[2] );
			r_stats.c_particle_count++;
		}

		if( p->type != pt_clientcustom )
		{
			// update position.
			VectorMA( p->org, frametime, p->vel, p->org );
		}

		switch( p->type )
		{
		case pt_static:
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
			VectorMA( p->vel, dvel, p->vel, p->vel );
			p->vel[2] -= grav;
			break;
		case pt_explode2:
			p->ramp += time3;
			if( p->ramp >= 8 ) p->die = -1;
			else p->color = ramp2[(int)p->ramp];
			VectorMA( p->vel, -frametime, p->vel, p->vel );
			p->vel[2] -= grav;
			break;
		case pt_blob:
		case pt_blob2:
			p->ramp += time2;
			if( p->ramp >= 9 )
				p->ramp = 0;
			p->color = gSparkRamp[(int)p->ramp];
			VectorMA( p->vel, -frametime * 0.5f, p->vel, p->vel );
			p->type = COM_RandomLong( 0, 3 ) ? pt_blob : pt_blob2;
			p->vel[2] -= grav * 5.0f;
			break;
		case pt_grav:
			p->vel[2] -= grav * 20;
			break;
		case pt_slowgrav:
			p->vel[2] -= grav;
			break;
		case pt_vox_grav:
			p->vel[2] -= grav * 8;
			break;
		case pt_vox_slowgrav:
			p->vel[2] -= grav * 4;
			break;
		case pt_clientcustom:
			if( p->callback )
				p->callback( p, frametime );
			break;
		}
	}

	pglEnd();
}

/*
================
CL_CullTracer

check tracer bbox
================
*/
static qboolean CL_CullTracer( particle_t *p, const vec3_t start, const vec3_t end )
{
	vec3_t	mins, maxs;
	int	i;

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
			maxs[i] += gTracerSize[p->type] * 2.0f;
		}
	}

	// check bbox
	return R_CullBox( mins, maxs, RI.clipFlags );
}

/*
================
CL_DrawTracers

update tracer color, position, free expired and draw it
================
*/
void CL_DrawTracers( double frametime )
{
	float		scale, atten, gravity;
	vec3_t		screenLast, screen;
	vec3_t		start, end, delta;
	particle_t	*p;

	if( !cl_draw_tracers->value )
		return;

	// update tracer color if this is changed
	if( FBitSet( tracerred->flags|tracergreen->flags|tracerblue->flags|traceralpha->flags, FCVAR_CHANGED ))
	{
		gTracerColors[4].r = (byte)(tracerred->value * traceralpha->value * 255);
		gTracerColors[4].g = (byte)(tracergreen->value * traceralpha->value * 255);
		gTracerColors[4].b = (byte)(tracerblue->value * traceralpha->value * 255);
		ClearBits( tracerred->flags, FCVAR_CHANGED );
		ClearBits( tracergreen->flags, FCVAR_CHANGED );
		ClearBits( tracerblue->flags, FCVAR_CHANGED );
		ClearBits( traceralpha->flags, FCVAR_CHANGED );
	}

	R_FreeDeadParticles( &cl_active_tracers );

	if( !cl_active_tracers )
		return;	// nothing to draw?

	gravity = frametime * clgame.movevars.gravity;
	scale = 1.0 - (frametime * 0.9);
	if( scale < 0.0f ) scale = 0.0f;

	GL_Bind( GL_TEXTURE0, cls.particleImage );	// FIXME: load a sprites/dot.spr instead
	GL_SetRenderMode( kRenderTransAdd );

	for( p = cl_active_tracers; p; p = p->next )
	{
		atten = (p->die - cl.time);
		if( atten > 0.1f ) atten = 0.1f;

		VectorScale( p->vel, ( p->ramp * atten ), delta );
		VectorAdd( p->org, delta, end );
		VectorCopy( p->org, start );

		if( !CL_CullTracer( p, start, end ))
		{
			vec3_t	verts[4], tmp2;
			vec3_t	tmp, normal;
			color24	*pColor;

			// Transform point into screen space
			TriWorldToScreen( start, screen );
			TriWorldToScreen( end, screenLast );

			// build world-space normal to screen-space direction vector
			VectorSubtract( screen, screenLast, tmp );

			// we don't need Z, we're in screen space
			tmp[2] = 0;
			VectorNormalize( tmp );

			// build point along noraml line (normal is -y, x)
			VectorScale( RI.vup, tmp[0], normal );
			VectorScale( RI.vright, -tmp[1], tmp2 );
			VectorSubtract( normal, tmp2, normal );

			// compute four vertexes
			VectorScale( normal, gTracerSize[p->type] * 2.0f, tmp );
			VectorSubtract( start, tmp, verts[0] ); 
			VectorAdd( start, tmp, verts[1] ); 
			VectorAdd( verts[0], delta, verts[2] ); 
			VectorAdd( verts[1], delta, verts[3] ); 

			pColor = &gTracerColors[p->color];
			pglColor4ub( pColor->r, pColor->g, pColor->b, p->packedColor );

			pglBegin( GL_QUADS );
				pglTexCoord2f( 0.0f, 0.8f );
				pglVertex3fv( verts[2] );
				pglTexCoord2f( 1.0f, 0.8f );
				pglVertex3fv( verts[3] );
				pglTexCoord2f( 1.0f, 0.0f );
				pglVertex3fv( verts[1] );
				pglTexCoord2f( 0.0f, 0.0f );
				pglVertex3fv( verts[0] );
			pglEnd();
		}

		// evaluate position
		VectorMA( p->org, frametime, p->vel, p->org );

		if( p->type == pt_grav )
		{
			p->vel[0] *= scale;
			p->vel[1] *= scale;
			p->vel[2] -= gravity;

			p->packedColor = 255 * (p->die - cl.time) * 2;
			if( p->packedColor > 255 ) p->packedColor = 255;
		}
		else if( p->type == pt_slowgrav )
		{
			p->vel[2] = gravity * 0.05;
		}
	}
}

void CL_DrawParticlesExternal( const float *vieworg, const float *forward, const float *right, const float *up, uint clipFlags )
{
	if( vieworg ) VectorCopy( vieworg, RI.vieworg );
	if( forward ) VectorCopy( forward, RI.vforward );
	if( right ) VectorCopy( right, RI.vright );
	if( up ) VectorCopy( up, RI.vup );

	RI.clipFlags = clipFlags;

	CL_DrawParticles ( tr.frametime );
	CL_DrawTracers( tr.frametime );
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
		p = R_AllocParticle( NULL );
		if( !p ) return;

		angle = cl.time * cl_avelocities[i][0];
		SinCos( angle, &sy, &cy );
		angle = cl.time * cl_avelocities[i][1];
		SinCos( angle, &sp, &cp );
		angle = cl.time * cl_avelocities[i][2];
		SinCos( angle, &sr, &cr );
	
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
		p = R_AllocParticle( NULL );
		if( !p ) return;

		p->die += 5.0f;
		p->color = ramp1[0];
		p->ramp = rand() & 3;

		if( i & 1 )
		{
			p->type = pt_explode;
			for( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
		else
		{
			p->type = pt_explode2;
			for( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
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
		p = R_AllocParticle( NULL );
		if( !p ) return;

		p->die += 0.3f;
		p->color = colorStart + ( colorMod % colorLength );
		colorMod++;

		p->type = pt_blob;

		for( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j] + ((rand() % 32) - 16);
			p->vel[j] = (rand() % 512) - 256;
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
		p = R_AllocParticle( NULL );
		if( !p ) return;

		p->die += 1.0f + (rand() & 8) * 0.05f;

		if( i & 1 )
		{
			p->type = pt_explode;
			p->color = 66 + rand() % 6;

			for( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
		else
		{
			p->type = pt_explode2;
			p->color = 150 + rand() % 6;

			for( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
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
void R_RunParticleEffect( const vec3_t org, const vec3_t dir, int color, int count )
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
		p = R_AllocParticle( NULL );
		if( !p ) return;

		p->die += 0.1f * (rand() % 5);
		p->color = (color & ~7) + (rand() & 7);
		p->type = pt_slowgrav;

		for( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j] + ((rand() & 15) - 8);
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
		p = R_AllocParticle( NULL );
		if( !p ) return;

		p->die += COM_RandomFloat( 0.1f, 0.5f );
		p->type = pt_slowgrav;
		p->color = pcolor;

		for( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j] + COM_RandomFloat( -8.0f, 8.0f );
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
	float		arc;

	for( arc = 0.05f, i = 0; i < 100; i++, arc -= 0.005f )
	{
		p = R_AllocParticle( NULL );

		if( !p ) return;

		p->die += 2.0f;
		p->type = pt_vox_grav;
		p->color = pcolor + COM_RandomLong( 0, 9 );

		VectorCopy( org, p->org );
		VectorCopy( dir, p->vel );

		p->vel[2] -= arc;
		arc -= 0.005f;

		VectorScale( p->vel, speed, p->vel );
	}

	for( arc = 0.075f, i = 0; i < ( speed / 5 ); i++, arc -= 0.005f )
	{
		float	num, spd;

		p = R_AllocParticle( NULL );
		if( !p ) return;

		p->die += 3.0f;
		p->color = pcolor + COM_RandomLong( 0, 9 );
		p->type = pt_vox_slowgrav;

		VectorCopy( org, p->org );
		VectorCopy( dir, p->vel );

		p->vel[2] -= arc;

		num = COM_RandomFloat( 0.0f, 1.0f );
		spd = speed * num;
		num *= 1.7f;

		VectorScale( p->vel, num, p->vel );
		VectorScale( p->vel, spd, p->vel );

		for( j = 0; j < 2; j++ )
		{
			p = R_AllocParticle( NULL );
			if( !p ) return;

			p->die += 3.0f;
			p->color = pcolor + COM_RandomLong( 0, 9 );
			p->type = pt_vox_slowgrav;

			p->org[0] = org[0] + COM_RandomFloat( -1.0f, 1.0f );
			p->org[1] = org[1] + COM_RandomFloat( -1.0f, 1.0f );
			p->org[2] = org[2] + COM_RandomFloat( -1.0f, 1.0f );

			VectorCopy( dir, p->vel );
			p->vel[2] -= arc;

			VectorScale( p->vel, num, p->vel );
			VectorScale( p->vel, spd, p->vel );
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
				p = R_AllocParticle( NULL );
				if( !p ) return;

				p->die += 2.0f + (rand() & 31) * 0.02f;
				p->color = 224 + (rand() & 7);
				p->type = pt_slowgrav;
				
				dir[0] = j * 8.0f + (rand() & 7);
				dir[1] = i * 8.0f + (rand() & 7);
				dir[2] = 256;

				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + (rand() & 63);

				VectorNormalize( dir );
				vel = 50 + (rand() & 63);
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
				p = R_AllocParticle( NULL );
				if( !p ) return;

				p->die += life + (rand() & 31) * 0.02f;
				p->color = color;
				p->type = pt_slowgrav;
				
				dir[0] = j * 8.0f + (rand() & 7);
				dir[1] = i * 8.0f + (rand() & 7);
				dir[2] = 256;

				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + (rand() & 63);

				VectorNormalize( dir );
				vel = 50 + (rand() & 63);
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
				p = R_AllocParticle( NULL );
				if( !p ) return;
		
				p->die += 0.2f + (rand() & 7) * 0.02f;
				p->color = 7 + (rand() & 7);
				p->type = pt_slowgrav;
				
				dir[0] = j * 8.0f;
				dir[1] = i * 8.0f;
				dir[2] = k * 8.0f;
	
				p->org[0] = org[0] + i + (rand() & 3);
				p->org[1] = org[1] + j + (rand() & 3);
				p->org[2] = org[2] + k + (rand() & 3);
	
				VectorNormalize( dir );
				VectorScale( dir, (50 + (rand() & 63)), p->vel );
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

		p = R_AllocParticle( NULL );
		if( !p ) return;
		
		p->die += 2.0f;

		switch( type )
		{
		case 0:	// rocket trail
			p->ramp = (rand() & 3);
			p->color = ramp3[(int)p->ramp];
			p->type = pt_fire;
			for( j = 0; j < 3; j++ )
				p->org[j] = start[j] + ((rand() % 6 ) - 3 );
			break;
		case 1:	// smoke smoke
			p->ramp = (rand() & 3) + 2;
			p->color = ramp3[(int)p->ramp];
			p->type = pt_fire;
			for( j = 0; j < 3; j++ )
				p->org[j] = start[j] + ((rand() % 6 ) - 3 );
			break;
		case 2:	// blood
			p->type = pt_grav;
			p->color = 67 + (rand() & 3);
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
			p->color = 67 + (rand() & 3);
			for( j = 0; j < 3; j++ )
				p->org[j] = start[j] + ((rand() % 6) - 3);
			len -= 3;
			break;
		case 6:	// voor trail
			p->color = 9 * 16 + 8 + (rand() & 3);
			p->type = pt_static;
			p->die += 0.3f;
			for( j = 0; j < 3; j++ )
				p->org[j] = start[j] + ((rand() & 15) - 8);
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
		p = R_AllocParticle( NULL );
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
R_BulletImpactParticles

===============
*/
void R_BulletImpactParticles( const vec3_t pos )
{
	int		i, quantity;
	int		color;
	float		dist;
	vec3_t		dir;
	particle_t	*p;
	
	VectorSubtract( pos, RI.vieworg, dir );
	dist = VectorLength( dir );
	if( dist > 1000.0f ) dist = 1000.0f;

	quantity = (1000.0f - dist) / 100.0f;
	if( quantity == 0 ) quantity = 1;

	color = 3 - ((30 * quantity) / 100 );
	R_SparkStreaks( pos, 2, -200, 200 );

	for( i = 0; i < quantity * 4; i++ )
	{
		p = R_AllocParticle( NULL );
		if( !p ) return;

		VectorCopy( pos, p->org);

		p->vel[0] = COM_RandomFloat( -1.0f, 1.0f );
		p->vel[1] = COM_RandomFloat( -1.0f, 1.0f );
		p->vel[2] = COM_RandomFloat( -1.0f, 1.0f );
		VectorScale( p->vel, COM_RandomFloat( 50.0f, 100.0f ), p->vel );

		p->die = cl.time + 0.5;
		p->color = 3 - color;
		p->packedColor = 0;
		p->type = pt_grav;
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
		p = R_AllocParticle( NULL );
		if( !p ) return;

		p->die += COM_RandomFloat( 0.5f, 2.0f );
		p->type = pt_blob;

		for( j = 0; j < 3; j++ )
			p->org[j] = org[j] + COM_RandomFloat( -32.0f, 32.0f );
		p->vel[2] = COM_RandomFloat( 64.0f, 100.0f );
	}
}

/*
===============
R_StreakSplash

create a splash of streaks
===============
*/
void R_StreakSplash( const vec3_t pos, const vec3_t dir, int color, int count, float speed, int velocityMin, int velocityMax )
{
	vec3_t		vel, vel2;
	particle_t	*p;	
	int		i;

	VectorScale( dir, speed, vel );

	for( i = 0; i < count; i++ )
	{
		vel2[0] = vel[0] + COM_RandomFloat( velocityMin, velocityMax );
		vel2[1] = vel[1] + COM_RandomFloat( velocityMin, velocityMax );
		vel2[2] = vel[2] + COM_RandomFloat( velocityMin, velocityMax );

		p = R_AllocTracer( pos, vel, COM_RandomFloat( 0.1f, 0.5f ));
		if( !p ) return;

		p->packedColor = 255;
		p->type = pt_grav;
		p->color = color;
		p->ramp = 1;
	}
}

/*
===============
CL_DebugParticle

just for debug purposes
===============
*/
void CL_DebugParticle( const vec3_t pos, byte r, byte g, byte b )
{
	particle_t	*p;

	p = R_AllocParticle( NULL );
	if( !p ) return;

	VectorCopy( pos, p->org );
	p->die += 10.0f;
	p->color = CL_LookupColor( r, g, b );
}

/*
===============
R_TracerEffect

===============
*/
void R_TracerEffect( const vec3_t start, const vec3_t end )
{
	vec3_t	tmp, vel, dir;
	float	len, speed;

	speed = Q_max( tracerspeed->value, 3.0f );

	VectorSubtract( end, start, dir );
	len = VectorLength( dir );
	if( len == 0.0f ) return;

	VectorScale( dir, 1.0f / len, dir ); // normalize
	VectorScale( dir, COM_RandomLong( -10.0f, 9.0f ) + traceroffset->value, tmp );
	VectorScale( dir, tracerspeed->value, vel );
	VectorAdd( tmp, start, tmp );

	R_AllocTracer( tmp, vel, len / speed );
}

/*
===============
R_UserTracerParticle

===============
*/
void R_UserTracerParticle( float *org, float *vel, float life, int colorIndex, float length, byte deathcontext, void (*deathfunc)( particle_t *p ))
{
	particle_t	*p;

	if( colorIndex < 0 )
	{
		MsgDev( D_ERROR, "UserTracer with color < 0\n" );
		return;
	}

	if( colorIndex > ARRAYSIZE( gTracerColors ))
	{
		MsgDev( D_ERROR, "UserTracer with color > %d\n", ARRAYSIZE( gTracerColors ));
		return;
	}

	if(( p = R_AllocTracer( org, vel, life )) != NULL )
	{
		p->context = deathcontext;
		p->deathfunc = deathfunc;
	}
}

/*
===============
R_TracerParticles

allow more customization
===============
*/
particle_t *R_TracerParticles( float *org, float *vel, float life )
{
	return R_AllocTracer( org, vel, life );
}

/*
===============
R_SparkStreaks

create a streak tracers
===============
*/
void R_SparkStreaks( const vec3_t pos, int count, int velocityMin, int velocityMax )
{
	particle_t	*p;
	vec3_t		vel;
	int		i;
	
	for( i = 0; i<count; i++ )
	{
		vel[0] = COM_RandomFloat( velocityMin, velocityMax );
		vel[1] = COM_RandomFloat( velocityMin, velocityMax );
		vel[2] = COM_RandomFloat( velocityMin, velocityMax );

		p = R_AllocTracer( pos, vel, COM_RandomFloat( 0.1f, 0.5f ));
		if( !p ) return;

		p->color = 5;
		p->packedColor = 255;
		p->type = pt_grav;
		p->ramp = 0.5;
	}
}

/*
===============
R_Implosion

make implosion tracers
===============
*/
void R_Implosion( const vec3_t end, float radius, int count, float life )
{
	float	dist = ( radius / 100.0f );
	vec3_t	start, temp, vel;
	float	factor;
	int	i;

	if( life <= 0.0f ) life = 0.1f; // to avoid divide by zero
	factor = -1.0 / life;

	for ( i = 0; i < count; i++ )
	{
		temp[0] = dist * COM_RandomFloat( -100.0f, 100.0f );
		temp[1] = dist * COM_RandomFloat( -100.0f, 100.0f );
		temp[2] = dist * COM_RandomFloat( 0.0f, 100.0f );
		VectorScale( temp, factor, vel );
		VectorAdd( temp, end, start );

		R_AllocTracer( start, vel, life );
	}
}

/*
===============
CL_ReadPointFile_f

===============
*/
void CL_ReadPointFile_f( void )
{
	char		*afile, *pfile;
	vec3_t		org;
	int		count;
	particle_t	*p;
	char		filename[64];
	string		token;
	
	Q_snprintf( filename, sizeof( filename ), "maps/%s.pts", clgame.mapname );
	afile = FS_LoadFile( filename, NULL, false );

	if( !afile )
	{
		MsgDev( D_ERROR, "couldn't open %s\n", filename );
		return;
	}
	
	Msg( "Reading %s...\n", filename );

	count = 0;
	pfile = afile;

	while( 1 )
	{
		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;
		org[0] = Q_atof( token );

		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;
		org[1] = Q_atof( token );

		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;
		org[2] = Q_atof( token );

		count++;
		
		if( !cl_free_particles )
		{
			MsgDev( D_ERROR, "CL_ReadPointFile: not enough free particles!\n" );
			break;
		}

		// NOTE: can't use CL_AllocateParticles because running from the console
		p = cl_free_particles;
		cl_free_particles = p->next;
		p->next = cl_active_particles;
		cl_active_particles = p;

		p->ramp = 0;		
		p->die = 99999;
		p->color = (-count) & 15;
		p->type = pt_static;
		VectorClear( p->vel );
		VectorCopy( org, p->org );
	}

	Mem_Free( afile );

	if( count ) Msg( "%i points read\n", count );
	else Msg( "map %s has no leaks!\n", clgame.mapname );
}