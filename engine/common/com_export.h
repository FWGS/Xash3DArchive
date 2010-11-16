//=======================================================================
//			Copyright XashXT Group 2009 ©
//		com_export.h - safe calls exports from other libraries
//=======================================================================
#ifndef COM_EXPORT_H
#define COM_EXPORT_H

#include "cm_local.h"

#ifdef __cplusplus
extern "C" {
#endif

// linked interfaces
extern stdlib_api_t	com;
extern vsound_exp_t	*se;
extern render_exp_t	*re;

#ifdef __cplusplus
}
#endif

_inline int CL_CreateDecalList( decallist_t *pList, qboolean changelevel )
{
	if( !re ) return 0;

	return re->CreateDecalList( pList, changelevel );
}

//
// vsound.dll exports
//
#define S_Shutdown			if( se ) se->Shutdown
#define S_StartStreaming		if( se ) se->StartStreaming
#define S_StopStreaming		if( se ) se->StopStreaming
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
#define S_ExtraUpdate		if( se ) se->ExtraUpdate
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

_inline int S_GetCurrentStaticSounds( soundlist_t *pout, int size, int entchannel )
{
	if( !se ) return 0;
	return se->GetCurrentStaticSounds( pout, size, entchannel );
}

#endif//COM_EXPORT_H