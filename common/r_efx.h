//=======================================================================
//			Copyright XashXT Group 2009 ©
//		       r_efx.h - decals & dlight management
//=======================================================================
#ifndef R_EFX_H
#define R_EFX_H

#include "particledef.h"
#include "beamdef.h"
#include "dlight.h"
#include "cl_entity.h"

typedef struct efx_api_s
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
} efx_api_t;

#endif//R_EFX_H