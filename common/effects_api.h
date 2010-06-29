//=======================================================================
//			Copyright XashXT Group 2009 ©
//		       effects_api.h - client temp entities
//=======================================================================
#ifndef EFFECTS_API_H
#define EFFECTS_API_H

struct particle_s
{
	byte		color;	// colorIndex for R_GetPaletteColor
	vec3_t		org;
	vec3_t		vel;
	float		die;
	float		ramp;
	ptype_t		type;
};

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
};

// decal flags
#define FDECAL_PERMANENT	0x01	// This decal should not be removed in favor of any new decals
#define FDECAL_REFERENCE	0x02	// This is a decal that's been moved from another level
#define FDECAL_CUSTOM	0x04	// This is a custom clan logo and should not be saved/restored
#define FDECAL_HFLIP	0x08	// Flip horizontal (U/S) axis
#define FDECAL_VFLIP	0x10	// Flip vertical (V/T) axis
#define FDECAL_CLIPTEST	0x20	// Decal needs to be clip-tested
#define FDECAL_NOCLIP	0x40	// Decal is not clipped by containing polygon
#define FDECAL_USESAXIS	0x80	// Uses the s axis field to determine orientation
#define FDECAL_DYNAMIC	0x100	// Indicates the decal is dynamic
#define FDECAL_SECONDPASS	0x200	// Decals that have to be drawn after everything else
#define FDECAL_WATER	0x400	// Decal should only be applied to water
#define FDECAL_DONTSAVE	0x800	// Decal was loaded from adjacent level, don't save it for this level

typedef struct efxapi_s
{
	particle_t*	(*R_AllocParticle)( void ); 
	void		(*R_BlobExplosion)( const float *org );
	void		(*R_EntityParticles)( edict_t *ent );
	void		(*R_LavaSplash)( const float *org );
	void		(*R_ParticleExplosion)( const float *org );
	void		(*R_ParticleExplosion2)( const float *org, int colorStart, int colorLength );
	void		(*R_RocketTrail)( const float *start, const float *end, int type );
	void		(*R_RunParticleEffect)( const float *org, const float *dir, int color, int count );
	void		(*R_TeleportSplash)( const float *org );
	void		(*R_GetPaletteColor)( int colorIndex, float *outColor );
	int		(*CL_DecalIndex)( int id );
	int		(*CL_DecalIndexFromName)( const char *szDecalName );
	void		(*R_DecalShoot)( HSPRITE hDecal, edict_t *pEnt, int modelIndex, float *pos, int flags );
	dlight_t*		(*CL_AllocDLight)( int key );
	dlight_t*		(*CL_AllocELight)( int key );
	void		(*R_LightForPoint)( const float *rgflOrigin, float *lightValue );
	int		(*CL_IsBoxVisible)( const float *mins, const float *maxs );
	int		(*R_CullBox)( const float *mins, const float *maxs );
	int		(*R_AddEntity)( edict_t *pEnt, int ed_type, HSPRITE customShader );
	int		(*R_AddTempEntity)( TEMPENTITY *pTemp, HSPRITE customShader );
	void		(*R_EnvShot)( const float *vieworg, const char *name, int skyshot );
} efxapi_t;

#endif//EFFECTS_API_H