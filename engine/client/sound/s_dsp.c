//=======================================================================
//			Copyright XashXT Group 2009 ©
//	   s_dsp.c - digital signal processing algorithms for audio FX
//=======================================================================

#include "common.h"
#include "sound.h"
#include "room_int.h"
#include "client.h"

convar_t			*dsp_room;	// room dsp preset - sounds more distant from player (1ch)
convar_t			*dsp_room_type;
convar_t			*dsp_stereo;	// set to 1 for true stereo processing.  2x perf hit.
int			ipset_room_prev;
int			ipset_room_typeprev;
int			idsp_room;
static DSP_FUNCTIONS	gDspFuncs;
static dll_info_t		room_dll = { "room.dll", NULL, NULL, NULL, NULL, false };

// engine callbacks
static dsp_enginefuncs_t gEngfuncs = 
{
	pfnRandomFloat,
	pfnRandomLong,
	Con_Printf,
	Con_DPrintf,
};

float DSP_GetGain( int idsp )
{
	if( !room_dll.link ) return 0.0f;
	return gDspFuncs.pfnDSP_GetGain( idsp );
}

void DSP_ClearState( void )
{
	if( !room_dll.link ) return;

	Cvar_SetFloat( "dsp_room", 0.0f );
	Cvar_SetFloat( "room_type", 0.0f );

	CheckNewDspPresets();
	gDspFuncs.pfnVidInit();
}

void DSP_Process( int idsp, portable_samplepair_t *pbfront, int sampleCount )
{
	ASSERT( pbfront );

	// return right away if fx processing is turned off
	if( !room_dll.link || dsp_off->integer ) return;

	gDspFuncs.pfnDSP_Process( idsp, pbfront, sampleCount );
}

// free all dsp processors 
void FreeDsps( void )
{
	if( !room_dll.link ) return;

	gDspFuncs.pfnDSP_Free( idsp_room );
	idsp_room = 0;
	
	gDspFuncs.pfnShutdown();

	Sys_FreeLibrary( &room_dll );
	Mem_Set( &gDspFuncs, 0, sizeof( gDspFuncs ));
}

// alloc dsp processors
qboolean AllocDsps( void )
{
	static ROOMAPI		InitDSP;
	static dsp_enginefuncs_t	gpEngfuncs;

	if( !Sys_LoadLibrary( NULL, &room_dll ))
	{
          	MsgDev( D_INFO, "DSP: disabled\n" );
		return false;
	}

	InitDSP = (ROOMAPI)Sys_GetProcAddress( &room_dll, "InitDSP" );

	if( !InitDSP )
	{
		Sys_FreeLibrary( &room_dll );
          	MsgDev( D_ERROR, "AllocDsps: failed to get address of InitDSP proc\n" );
		return false;
	}

	// make local copy of engfuncs to prevent overwrite it with user dll
	Mem_Copy( &gpEngfuncs, &gEngfuncs, sizeof( gpEngfuncs ));

	if( !InitDSP( &gDspFuncs, &gpEngfuncs, DSP_VERSION ))
	{
		Sys_FreeLibrary( &room_dll );
		MsgDev( D_ERROR, "AllocDsps: can't init DSP\n" );
		return false;
	}

	gDspFuncs.pfnInit();

	idsp_room = -1.0;

	// initialize DSP cvars
	dsp_room = Cvar_Get( "dsp_room", "0", 0, "room dsp preset - sounds more distant from player (1ch)" );
	dsp_room_type = Cvar_Get( "room_type", "0", 0, "duplicate for dsp_room cvar for backward compatibility" );
	dsp_stereo = Cvar_Get( "dsp_stereo", "0", 0, "set to 1 for true stereo processing.  2x perf hits" );

	// alloc dsp room channel (mono, stereo if dsp_stereo is 1)

	// dsp room is mono, 300ms fade time
	idsp_room = gDspFuncs.pfnDSP_Alloc( dsp_room->integer, 300, dsp_stereo->integer * 2 );

	// init prev values
	ipset_room_prev = dsp_room->integer;
	ipset_room_typeprev = dsp_room_type->integer;

	if( idsp_room < 0 )
	{
		MsgDev( D_WARN, "DSP processor failed to initialize! \n" );

		FreeDsps();
		return false;
	}
	return true;
}

void CheckNewDspPresets( void )
{
	int	iroomtype;
	int	iroom;

	if( !room_dll.link ) return;

	iroomtype = dsp_room_type->integer;

	if( s_listener.waterlevel > 2 )
		iroom = 15;
	else if( s_listener.inmenu && !cl.background )
		iroom = 0;
	else iroom = dsp_room->integer;

	// legacy code support for "room_type" Cvar
	if( iroomtype != ipset_room_typeprev )
	{
		// force dsp_room = room_type
		ipset_room_typeprev = iroomtype;
		Cvar_SetFloat( "dsp_room", iroomtype );
	}

	if( iroom != ipset_room_prev )
	{
		gDspFuncs.pfnDSP_SetPreset( idsp_room, iroom );
		ipset_room_prev = iroom;

		// force room_type = dsp_room
		Cvar_SetFloat( "room_type", iroom );
		ipset_room_typeprev = iroom;
	}
}