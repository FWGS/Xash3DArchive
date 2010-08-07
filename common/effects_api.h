//=======================================================================
//			Copyright XashXT Group 2009 ©
//		       effects_api.h - client temp entities
//=======================================================================
#ifndef EFFECTS_API_H
#define EFFECTS_API_H

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

typedef struct efxapi_s
{
	void		(*R_GetPaletteColor)( int colorIndex, float *outColor );
	int		(*CL_DecalIndex)( int id );
	int		(*CL_DecalIndexFromName)( const char *szDecalName );
	void		(*R_DecalShoot)( HSPRITE hDecal, int entityIndex, int modelIndex, float *pos, int flags );
	void		(*R_PlayerDecal)( HSPRITE hDecal, int entityIndex, float *pos, byte *color );
	dlight_t*		(*CL_AllocDLight)( int key );
	dlight_t*		(*CL_AllocELight)( int key );
	void		(*R_LightForPoint)( const float *rgflOrigin, float *lightValue );
	int		(*CL_IsBoxVisible)( const float *mins, const float *maxs );
	int		(*R_CullBox)( const float *mins, const float *maxs );
	int		(*R_AddEntity)( struct cl_entity_s *pEnt, int ed_type, HSPRITE customShader );
	void		(*R_EnvShot)( const float *vieworg, const char *name, int skyshot );
} efxapi_t;

#endif//EFFECTS_API_H