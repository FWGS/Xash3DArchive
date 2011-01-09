//=======================================================================
//			Copyright XashXT Group 2011 ©
//		 room_int.h - interface between engine and DSP
//=======================================================================
#ifndef ROOM_INT_H
#define ROOM_INT_H

#define DSP_VERSION		1

typedef struct dsp_enginefuncs_s
{
	// random generator
	float	(*pfnRandomFloat)( float flLow, float flHigh );	
	long	(*pfnRandomLong)( long lLow, long lHigh );
	
	// debug messages
	void	(*Con_Printf)( char *fmt, ... );
	void	(*Con_DPrintf)( char *fmt, ... );
} dsp_enginefuncs_t;

typedef struct
{
	void	(*pfnVidInit)( void );
	void	(*pfnInit)( void );
	void	(*pfnShutdown)( void );
	int	(*pfnDSP_Alloc)( int ipset, float xfade, int cchan );		// alloc
	void	(*pfnDSP_SetPreset)( int idsp, int ipsetnew );			// set preset
	void	(*pfnDSP_Process)( int idsp, void *pbfront, int sampleCount );	// process
	float	(*pfnDSP_GetGain)( int idsp );
	void	(*pfnDSP_Free)( int idsp );
} DSP_FUNCTIONS;

typedef int (*ROOMAPI)( DSP_FUNCTIONS *pFunctionTable, dsp_enginefuncs_t* engfuncs, int iVersion );

#endif//ROOM_INT_H