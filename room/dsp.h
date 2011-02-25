//=======================================================================
//			Copyright XashXT Group 2011 ©
//			 dsp.h - main header for DSP
//=======================================================================
#ifndef DSP_H
#define DSP_H

#include <windows.h>
#include "mathlib.h"
#include "const.h"
#include "room_int.h"

#define bound(min, num, max)		((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))

// disable some warnings
#pragma warning(disable : 4244)	// MIPS
#pragma warning(disable : 4018)	// signed/unsigned mismatch
#pragma warning(disable : 4305)	// truncation from const double to float

#define SOUND_DMA_SPEED	44100	// hardware playback rate

typedef struct
{
	int		left;
	int		right;
} portable_samplepair_t;

extern dsp_enginefuncs_t	g_engfuncs;

extern void DSP_ClearState( void );
extern void DSP_InitAll( void );
extern void DSP_FreeAll( void );
extern int DSP_Alloc( int ipset, float xfade, int cchan );			// alloc
extern void DSP_SetPreset( int idsp, int ipsetnew );				// set preset
extern void DSP_Process( int idsp, void *pbfront, int sampleCount );	// process
extern float DSP_GetGain( int idsp );
extern void DSP_Free( int idsp );

#endif//DSP_H