//=======================================================================
//			Copyright XashXT Group 2010 ©
//			   cl_tent.h -- efx api set
//=======================================================================

#ifndef CL_TENT_H
#define CL_TENT_H

// EfxAPI
struct particle_s *CL_AllocParticle( void (*callback)( struct particle_s*, float ));
void CL_ParticleExplosion( const vec3_t org );
void CL_ParticleExplosion2( const vec3_t org, int colorStart, int colorLength );
void CL_BlobExplosion( const vec3_t org );
void CL_EntityParticles( cl_entity_t *ent );
void CL_RunParticleEffect( const vec3_t org, const vec3_t dir, int color, int count );
void CL_ParticleBurst( const vec3_t org, int size, int color, float life );
void CL_LavaSplash( const vec3_t org );
void CL_TeleportSplash( const vec3_t org );
void CL_RocketTrail( vec3_t start, vec3_t end, int type );
short CL_LookupColor( byte r, byte g, byte b );
void CL_GetPackedColor( short *packed, short color );
void CL_SparkleTracer( const vec3_t pos, const vec3_t dir );
void CL_TracerEffect( const vec3_t start, const vec3_t end );
void CL_UserTracerParticle( float *org, float *vel, float life, int colorIndex, float length, byte deathcontext,
	void (*deathfunc)( struct particle_s *p ));
struct particle_s *CL_TracerParticles( float *org, float *vel, float life );
void CL_BulletImpactParticles( const vec3_t pos );
void CL_SparkShower( const vec3_t org );

// TriAPI
void TriVertex3fv( const float *v );
int TriWorldToScreen( float *world, float *screen );

#endif//CL_TENT_H