//=======================================================================
//			Copyright XashXT Group 2010 ©
//		       gl_studio.c - studio model renderer
//=======================================================================

#include "common.h"
#include "client.h"
#include "matrix_lib.h"
#include "const.h"
#include "r_studioint.h"
#include "studio.h"
#include "pm_local.h"
#include "gl_local.h"

convar_t			*r_studio_lerping;
static r_studio_interface_t	*pStudioDraw;
static float		aliasXscale, aliasYscale;
static matrix3x4		g_rotationmatrix;

/*
====================
R_StudioInit

====================
*/
void R_StudioInit( void )
{
	float	pixelAspect;

	r_studio_lerping = Cvar_Get( "r_studio_lerping", "1", CVAR_ARCHIVE, "enables studio animation lerping" );

	// recalc software X and Y alias scale (this stuff is used only by HL software renderer but who knews...)
	pixelAspect = ((float)scr_height->integer / (float)scr_width->integer);
	if( scr_width->integer < 640 )
		pixelAspect *= (320.0f / 240.0f);
	else pixelAspect *= (640.0f / 480.0f);

	aliasXscale = (float)scr_width->integer / RI.refdef.fov_y;
	aliasYscale = aliasXscale * pixelAspect;
}

/*
===============
pfnGetCurrentEntity

===============
*/
static cl_entity_t *pfnGetCurrentEntity( void )
{
	return RI.currententity;
}

/*
===============
pfnPlayerInfo

===============
*/
static player_info_t *pfnPlayerInfo( int index )
{
	if( index < 0 || index > cl.maxclients )
		return NULL;
	return &cl.players[index];
}

/*
===============
pfnGetPlayerState

===============
*/
static entity_state_t *pfnGetPlayerState( int index )
{
	if( index < 0 || index > cl.maxclients )
		return NULL;
	return &cl.frame.playerstate[index];
}

/*
===============
pfnGetViewEntity

===============
*/
static cl_entity_t *pfnGetViewEntity( void )
{
	return &clgame.viewent;
}

/*
===============
pfnGetEngineTimes

===============
*/
static void pfnGetEngineTimes( int *framecount, double *current, double *old )
{
	if( framecount ) *framecount = tr.framecount;
	if( current ) *current = cl.time;
	if( old ) *old = cl.oldtime;
}

/*
===============
pfnGetViewInfo

===============
*/
static void pfnGetViewInfo( float *origin, float *upv, float *rightv, float *forwardv )
{
	if( origin ) VectorCopy( RI.vieworg, origin );
	if( forwardv ) VectorCopy( RI.vforward, forwardv );
	if( rightv ) VectorCopy( RI.vright, rightv );
	if( upv ) VectorCopy( RI.vup, upv );
}

/*
===============
pfnGetChromeSprite

===============
*/
static model_t *pfnGetChromeSprite( void )
{
	if( cls.hChromeSprite <= 0 || cls.hChromeSprite > ( MAX_IMAGES - 1 ))
		return NULL; // bad sprite
	return &clgame.sprites[cls.hChromeSprite];
}

/*
===============
pfnGetModelCounters

===============
*/
static void pfnGetModelCounters( int **s, int **a )
{
	static int studio_count, studio_drawn;

	// FIXME: implement
	*s = &studio_count;
	*a = &studio_drawn;
}

/*
===============
pfnGetAliasScale

===============
*/
static void pfnGetAliasScale( float *x, float *y )
{
	if( x ) *x = aliasXscale;
	if( y ) *y = aliasYscale;
}

/*
===============
pfnStudioGetBoneTransform

===============
*/
static float ****pfnStudioGetBoneTransform( void )
{
	// FIXME: implement
	return NULL;
}

/*
===============
pfnStudioGetLightTransform

===============
*/
static float ****pfnStudioGetLightTransform( void )
{
	// FIXME: implement
	return NULL;
}

/*
===============
pfnStudioGetAliasTransform

===============
*/
static float ***pfnStudioGetAliasTransform( void )
{
	// FIXME: implement
	return NULL;
}

/*
===============
pfnStudioGetRotationMatrix

===============
*/
static float ***pfnStudioGetRotationMatrix( void )
{
	return (float ***)g_rotationmatrix;
}

/*
===============
pfnStudioSetupModel

===============
*/
static void pfnStudioSetupModel( int bodypart, void **ppbodypart, void **ppsubmodel )
{
	// FIXME: implement
}

/*
===============
pfnStudioCheckBBox

===============
*/
static int pfnStudioCheckBBox( void )
{
	// FIXME: implement
	return false;
}

/*
===============
pfnStudioDynamicLight

===============
*/
static void pfnStudioDynamicLight( struct cl_entity_s *ent, struct alight_s *plight )
{
	// FIXME: implement
}

/*
===============
pfnStudioEntityLight

===============
*/
static void pfnStudioEntityLight( struct alight_s *plight )
{
	// FIXME: implement
}

/*
===============
pfnStudioSetupLighting

===============
*/
static void pfnStudioSetupLighting( struct alight_s *plighting )
{
	// FIXME: implement
}

/*
===============
pfnStudioDrawPoints

===============
*/
static void pfnStudioDrawPoints( void )
{
	// FIXME: implement
}

/*
===============
pfnStudioDrawHulls

===============
*/
static void pfnStudioDrawHulls( void )
{
	// FIXME: implement
}

/*
===============
pfnStudioDrawAbsBBox

===============
*/
static void pfnStudioDrawAbsBBox( void )
{
	// FIXME: implement
}

/*
===============
pfnStudioDrawBones

===============
*/
static void pfnStudioDrawBones( void )
{
	// FIXME: implement
}

/*
===============
pfnStudioSetupSkin

===============
*/
static void pfnStudioSetupSkin( void *ptexturehdr, int index )
{
	// FIXME: implement
}

/*
===============
pfnStudioSetRemapColors

===============
*/
static void pfnStudioSetRemapColors( int top, int bottom )
{
	// FIXME: implement
}

/*
===============
pfnSetupPlayerModel

===============
*/
static model_t *pfnSetupPlayerModel( int index )
{
	player_info_t	*info;
	string		modelpath;
	int		modelIndex;

	if( index < 0 || index > cl.maxclients )
		return NULL; // bad client ?

	info = &cl.players[index];

	com.snprintf( modelpath, sizeof( modelpath ), "models/player/%s/%s.mdl", info->model, info->model );
	modelIndex = CL_FindModelIndex( modelpath );	

	return CM_ClipHandleToModel( modelIndex );
}

/*
===============
pfnStudioClientEvents

===============
*/
static void pfnStudioClientEvents( void )
{
	// FIXME: implement
}

/*
===============
pfnGetForceFaceFlags

===============
*/
static int pfnGetForceFaceFlags( void )
{
	// FIXME: implement
	return 0;
}

/*
===============
pfnSetForceFaceFlags

===============
*/
static void pfnSetForceFaceFlags( int flags )
{
	// FIXME: implement
}

/*
===============
pfnStudioSetHeader

===============
*/
static void pfnStudioSetHeader( void *header )
{
	// FIXME: implement
}

/*
===============
pfnSetRenderModel

===============
*/
static void pfnSetRenderModel( model_t *model )
{
	// FIXME: implement
}

/*
===============
pfnSetupRenderer

===============
*/
static void pfnSetupRenderer( int rendermode )
{
	// FIXME: implement
}

/*
===============
pfnRestoreRenderer

===============
*/
static void pfnRestoreRenderer( void )
{
	// FIXME: implement
}

/*
===============
pfnSetChromeOrigin

===============
*/
static void pfnSetChromeOrigin( void )
{
	// FIXME: implement
}

/*
===============
pfnIsHardware

Xash3D is always works in hadrware mode
===============
*/
static int pfnIsHardware( void )
{
	return true;
}
	
/*
===============
pfnStudioDrawShadow

===============
*/
static void pfnStudioDrawShadow( void )
{
	// in GoldSrc shadow call is dsiabled with 'return' at start of the function
	// some mods used a hack with calling DrawShadow ahead of 'return'
	// this code is for HL compatibility.
	return;

	// FIXME: implement
	MsgDev( D_INFO, "GL_StudioDrawShadow()\n" );	// just a debug
}

/*
===============
R_StudioDrawModel

===============
*/
static int R_StudioDrawModel( int flags )
{
	return 0;
}

/*
===============
R_StudioDrawPlayer

===============
*/
static int R_StudioDrawPlayer( int flags, entity_state_t *pplayer )
{
	return 0;
}

/*
=================
R_DrawStudioModel
=================
*/
void R_DrawStudioModel( cl_entity_t *e )
{
	int	flags, result;

	ASSERT( pStudioDraw != NULL );

	if( e == &clgame.viewent )
		flags = STUDIO_EVENTS;	
	else flags = STUDIO_RENDER|STUDIO_EVENTS;

	// select the properly method
	if( e->player )
		result = pStudioDraw->StudioDrawPlayer( flags, &cl.frame.playerstate[e->index-1] );
	else result = pStudioDraw->StudioDrawModel( flags );

	if( result ) r_stats.c_studio_models++;
}

void Mod_UnloadStudioModel( model_t *mod )
{
	studiohdr_t	*pstudio;
	mstudiotexture_t	*ptexture;
	int		i;

	ASSERT( mod != NULL );

	if( mod->type != mod_studio )
		return; // not a studio

	pstudio = mod->cache.data;
	if( !pstudio ) return; // already freed

	ptexture = (mstudiotexture_t *)(((byte *)pstudio) + pstudio->textureindex);

	// release all textures
	for( i = 0; i < pstudio->numtextures; i++ )
	{
//		GL_FreeTexture( ptexture[i].index );
	}

	Mem_FreePool( &mod->mempool );
	Mem_Set( mod, 0, sizeof( *mod ));
}
		
static engine_studio_api_t gStudioAPI =
{
	Mod_Calloc,
	Mod_CacheCheck,
	Mod_LoadCacheFile,
	Mod_ForName,
	Mod_Extradata,
	CM_ClipHandleToModel,
	pfnGetCurrentEntity,
	pfnPlayerInfo,
	pfnGetPlayerState,
	pfnGetViewEntity,
	pfnGetEngineTimes,
	pfnCVarGetPointer,
	pfnGetViewInfo,
	pfnGetChromeSprite,
	pfnGetModelCounters,
	pfnGetAliasScale,
	pfnStudioGetBoneTransform,
	pfnStudioGetLightTransform,
	pfnStudioGetAliasTransform,
	pfnStudioGetRotationMatrix,
	pfnStudioSetupModel,
	pfnStudioCheckBBox,
	pfnStudioDynamicLight,
	pfnStudioEntityLight,
	pfnStudioSetupLighting,
	pfnStudioDrawPoints,
	pfnStudioDrawHulls,
	pfnStudioDrawAbsBBox,
	pfnStudioDrawBones,
	pfnStudioSetupSkin,
	pfnStudioSetRemapColors,
	pfnSetupPlayerModel,
	pfnStudioClientEvents,
	pfnGetForceFaceFlags,
	pfnSetForceFaceFlags,
	pfnStudioSetHeader,
	pfnSetRenderModel,
	pfnSetupRenderer,
	pfnRestoreRenderer,
	pfnSetChromeOrigin,
	pfnIsHardware,
	pfnStudioDrawShadow,
	GL_SetRenderMode,
};

static r_studio_interface_t gStudioDraw =
{
	STUDIO_INTERFACE_VERSION,
	R_StudioDrawModel,
	R_StudioDrawPlayer,
};

/*
===============
CL_InitStudioAPI

Initialize client studio
===============
*/
qboolean CL_InitStudioAPI( void )
{
	pStudioDraw = &gStudioDraw;

	// Xash will be used internal StudioModelRenderer
	if( !clgame.dllFuncs.pfnGetStudioModelInterface )
		return true;

	if( clgame.dllFuncs.pfnGetStudioModelInterface( STUDIO_INTERFACE_VERSION, &pStudioDraw, &gStudioAPI ))
		return true;

	// NOTE: we always return true even if game interface was not correct
	// because we need Draw our StudioModels
	// just restore pointer to builtin function
	pStudioDraw = &gStudioDraw;

	return true;
}