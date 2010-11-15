//=======================================================================
//			Copyright XashXT Group 2010 ©
//		        cl_studio.c - client studio utilities
//=======================================================================

#include "common.h"
#include "client.h"
#include "byteorder.h"
#include "matrix_lib.h"
#include "const.h"
#include "r_studioint.h"
#include "studio.h"
#include "pm_local.h"

static r_studio_interface_t	*pStudioDraw;

/*
===============
pfnGetCurrentEntity

===============
*/
static cl_entity_t *pfnGetCurrentEntity( void )
{
	// FIXME: implement
	// this is will be needs is we called StudioDrawModel or StudioDrawPlayer
	return NULL;
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
	if( framecount ) *framecount = host.framecount;	// this is will not working properly with mirros etc
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
	if( origin ) VectorCopy( cl.refdef.vieworg, origin );
	if( upv ) VectorCopy( cl.refdef.up, upv );
	if( rightv ) VectorCopy( cl.refdef.right, rightv );
	if( forwardv ) VectorCopy( cl.refdef.forward, forwardv );
}

/*
===============
pfnGetChromeSprite

===============
*/
static model_t *pfnGetChromeSprite( void )
{
	if( cls.hChromeSprite <= 0 || cls.hChromeSprite > MAX_IMAGES )
		return NULL; // bad sprite
	return &clgame.ds.images[cls.hChromeSprite];
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

Software scales not used in Xash3D
===============
*/
static void pfnGetAliasScale( float *x, float *y )
{
	if( x ) *x = 0.0f;
	if( y ) *y = 0.0f;
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
	// FIXME: implement
	return NULL;
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
pfnSetRenderMode

===============
*/
static void pfnSetRenderMode( int mode )
{
	// FIXME: implement
}
		
static engine_studio_api_t gStudioAPI =
{
	Mod_Calloc,
	Mod_CacheCheck,
	Mod_LoadCacheFile,
	CM_ModForName,
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
	pfnSetRenderMode,

};

/*
===============
CL_InitStudioAPI

Initialize client studio
===============
*/
qboolean CL_InitStudioAPI( void )
{
	pStudioDraw = NULL;	// clear previous API

	return clgame.dllFuncs.pfnGetStudioModelInterface( STUDIO_INTERFACE_VERSION, &pStudioDraw, &gStudioAPI );
}