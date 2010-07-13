//=======================================================================
//			Copyright XashXT Group 2009 ©
//		com_export.h - safe calls exports from other libraries
//=======================================================================
#ifndef COM_EXPORT_H
#define COM_EXPORT_H

// linked interfaces
extern stdlib_api_t	com;
extern physic_exp_t	*pe;
extern vsound_exp_t	*se;
extern render_exp_t	*re;

//
// physic.dll exports
//
#define CM_RegisterModel		if( pe ) pe->RegisterModel
#define CM_GetAttachment		if( pe ) pe->Mod_GetAttachment
#define CM_GetBonePosition		if( pe ) pe->Mod_GetBonePos
#define CM_GetAmbientLevels		if( pe ) pe->AmbientLevels
#define CM_SetLightStyle		if( pe ) pe->AddLightstyle
#define CM_EndRegistration		if( pe ) pe->EndRegistration
#define CM_Frame			if( pe ) pe->Frame

_inline void CM_BeginRegistration( const char *name, bool clientload, uint *checksum )
{
	if( pe ) pe->BeginRegistration( name, clientload, checksum );
	else if( checksum ) *checksum = 0xBAD;
}

_inline void Mod_GetBounds( model_t handle, vec3_t mins, vec3_t maxs )
{
	if( pe ) pe->Mod_GetBounds( handle, mins, maxs );
	else
	{
		if( mins ) mins[0] = mins[1] = mins[2] = 0.0f;
		if( maxs ) maxs[0] = maxs[1] = maxs[2] = 0.0f;
	}
}

_inline void Mod_GetFrames( model_t handle, int *numFrames )
{
	if( pe ) pe->Mod_GetFrames( handle, numFrames );
	else if( numFrames ) *numFrames = 0;
}

_inline int CM_NumBmodels( void )
{
	if( !pe ) return 1;	// world
	return pe->NumBmodels();
}

_inline int CM_GetModelType( model_t handle )
{
	if( !pe ) return mod_bad;
	return pe->Mod_GetType( handle );
}

_inline int CM_PointLeafnum( const vec3_t origin )
{
	if( !pe )	return -1;
	return pe->PointLeafnum( origin );
}

_inline int CM_LightEntity( edict_t *pEnt )
{
	if( !pe ) return 255;
	return pe->LightPoint( pEnt );
}

_inline byte *CM_LeafPVS( int cluster )
{
	if( !pe )	return NULL;
	return pe->LeafPVS( cluster );
}

_inline byte *CM_LeafPHS( int cluster )
{
	if( !pe )	return NULL;
	return pe->LeafPHS( cluster );
}

_inline byte *CM_FatPVS( const vec3_t org, bool merge )
{
	if( !pe )	return NULL;
	return pe->FatPVS( org, merge );
}

_inline byte *CM_FatPHS( const vec3_t org, bool merge )
{
	if( !pe )	return NULL;
	return pe->FatPHS( org, merge );
}

_inline int CM_BoxLeafnums( vec3_t mins, vec3_t maxs, short *list, int listsize, int *topNode )
{
	if( !pe )
	{
		if( topNode ) *topNode = -1;
		return 0;
	}
	return pe->BoxLeafnums( mins, maxs, list, listsize, topNode );
}

_inline bool CM_HeadnodeVisible( int nodenum, byte *visbits )
{
	if( !pe )	return true;
	return pe->HeadnodeVisible( nodenum, visbits );
}

_inline int CM_PointContents( const vec3_t p )
{
	if( pe ) return pe->PointContents( p );
	return CONTENTS_NONE;
}

_inline int CM_HullPointContents( chull_t *hull, int num, const vec3_t p )
{
	if( pe ) return pe->HullPointContents( hull, num, p );
	return CONTENTS_NONE;
}

_inline trace_t CM_ClipMove( edict_t *ent, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int flags )
{
	trace_t	trace;

	if( !pe )
	{
		Mem_Set( &trace, 0, sizeof( trace_t ));
		trace.vecEndPos[0] = end[0];
		trace.vecEndPos[1] = end[1];
		trace.vecEndPos[2] = end[2];
		trace.flFraction = 1.0f;
		trace.fAllSolid = true;
		trace.iHitgroup = -1;
	}
	else trace = pe->Trace( ent, start, mins, maxs, end, flags );

	return trace;
}

_inline const char *CM_TraceTexture( edict_t *pTextureEntity, const vec3_t v1, const vec3_t v2 )
{
	if( !pe ) return NULL;
	return pe->TraceTexture( pTextureEntity, v1, v2 );
}

_inline chull_t *CM_HullForBsp( edict_t *ent, const vec3_t mins, const vec3_t maxs, float *offset )
{
	if( !pe ) return NULL;
	return pe->HullForBsp( ent, mins, maxs, offset );
}

_inline void *Mod_Extradata( model_t modelIndex )
{
	if( !pe )	return NULL;
	return pe->Mod_Extradata( modelIndex );
}

_inline bool CM_BoxVisible( const vec3_t mins, const vec3_t maxs )
{
	if( !pe || !re ) return true;
	return pe->BoxVisible( mins, maxs, re->GetCurrentVis());
}

_inline int CL_CreateDecalList( decallist_t *pList, bool changelevel )
{
	if( !re ) return 0;

	return re->CreateDecalList( pList, changelevel );
}

//
// vsound.dll exports
//
#define S_Shutdown			if( se ) se->Shutdown
#define S_StartStreaming		if( se ) se->StartStreaming
#define S_StartSound		if( se ) se->StartSound
#define S_AmbientSound		if( se ) se->StaticSound
#define S_StartLocalSound		if( se ) se->StartLocalSound
#define S_StartBackgroundTrack	if( se ) se->StartBackgroundTrack
#define S_StopBackgroundTrack		if( se ) se->StopBackgroundTrack
#define S_RawSamples 		if( se ) se->StreamRawSamples
#define S_StopAllSounds		if( se ) se->StopAllSounds
#define S_StopSound			if( se ) se->StopSound
#define S_AddLoopingSound		if( se ) se->AddLoopingSound
#define S_Activate			if( se ) se->Activate
#define S_BeginFrame		if( se ) se->BeginFrame
#define S_RenderFrame		if( se ) se->RenderFrame
#define S_BeginRegistration		if( se ) se->BeginRegistration
#define S_EndRegistration		if( se ) se->EndRegistration

_inline void S_FadeClientVolume( float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds )
{
	if( se ) se->FadeClientVolume( fadePercent, fadeOutSeconds, holdTime, fadeInSeconds );
}

_inline sound_t S_RegisterSound( const char *name )
{
	if( !se ) return 0;
	return se->RegisterSound( name );
}

#endif//COM_EXPORT_H