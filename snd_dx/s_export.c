//=======================================================================
//			Copyright XashXT Group 2007 �
//			s_export.c - sound library main
//=======================================================================

#include "sound.h"

vsound_imp_t	si;
stdlib_api_t	com;

// main DLL entry point
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
	return TRUE;
}

vsound_exp_t EXPORT *CreateAPI( stdlib_api_t *input, vsound_imp_t *engfuncs )
{
	static vsound_exp_t		snd;

	com = *input;

	// Sys_LoadLibrary can create fake instance, to check
	// api version and api size, but second argument will be 0
	// and always make exception, run simply check to avoid it
	if( engfuncs ) si = *engfuncs;

	// generic functions
	snd.api_size = sizeof( vsound_exp_t );
	snd.com_size = sizeof( stdlib_api_t );
	
	snd.Init = S_Init;
	snd.Shutdown = S_Shutdown;

	// sound manager
	snd.BeginRegistration = S_BeginRegistration;
	snd.RegisterSound = S_RegisterSound;
	snd.EndRegistration = S_EndRegistration;

	snd.FadeClientVolume = S_FadeClientVolume;
	snd.StartSound = S_StartSound;
	snd.StaticSound = S_StaticSound;
	snd.StreamRawSamples = S_StreamRawSamples;
	snd.StartLocalSound = S_StartLocalSound;
	snd.StartBackgroundTrack = S_StartBackgroundTrack;
	snd.StopBackgroundTrack = S_StopBackgroundTrack;
	snd.GetCurrentStaticSounds = S_GetCurrentStaticSounds;
	snd.PauseBackgroundTrack = S_StreamSetPause;

	snd.StartStreaming = S_StartStreaming;
	snd.StopStreaming = S_StopStreaming;

	snd.ExtraUpdate = S_ExtraUpdate;
	snd.RenderFrame = S_RenderFrame;
	snd.StopSound = S_StopSound;
	snd.StopAllSounds = S_StopAllSounds;

	snd.Activate = S_Activate;

	return &snd;
}