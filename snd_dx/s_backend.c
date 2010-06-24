//=======================================================================
//			Copyright XashXT Group 2009 ©
//		        s_direct.c - sound hardware output
//=======================================================================

#include <dsound.h>
#include "sound.h"

#define iDirectSoundCreate( a, b, c )	pDirectSoundCreate( a, b, c )

static HRESULT ( _stdcall *pDirectSoundCreate)(GUID* lpGUID, LPDIRECTSOUND* lplpDS, IUnknown* pUnkOuter );

static dllfunc_t dsound_funcs[] =
{
{ "DirectSoundCreate", (void **) &pDirectSoundCreate },
{ NULL, NULL }
};

dll_info_t dsound_dll = { "dsound.dll", dsound_funcs, NULL, NULL, NULL, false, 0, 0 };

// 64K is > 1 second at 16-bit, 22050 Hz
#define WAV_BUFFERS			64
#define WAV_MASK			0x3F
#define WAV_BUFFER_SIZE		0x0400
#define SECONDARY_BUFFER_SIZE		0x10000

typedef enum
{
	SIS_SUCCESS,
	SIS_FAILURE,
	SIS_NOTAVAIL
} sndinitstat;

static bool	dsound_init;
static bool	wav_init;
static bool	snd_firsttime = true, snd_isdirect, snd_iswave;
static bool	primary_format_set;
static HWND	snd_hwnd;
static int	sample16;
static int	snd_sent, snd_completed;


/* 
 Global variables. Must be visible to window-procedure function 
 so it can unlock and free the data block after it has been played. 
*/ 

HANDLE		hData;
HPSTR		lpData, lpData2;

HGLOBAL		hWaveHdr;
LPWAVEHDR		lpWaveHdr;
HWAVEOUT		hWaveOut; 
WAVEOUTCAPS	wavecaps;

DWORD		gSndBufSize;
MMTIME		mmstarttime;

LPDIRECTSOUND	pDS;
LPDIRECTSOUNDBUFFER	pDSBuf, pDSPBuf;
HINSTANCE		hInstDS;

bool SNDDMA_InitDirect( void );
bool SNDDMA_InitWav( void );

/*
===========
S_PrintDeviceName
===========
*/
void S_PrintDeviceName( void )
{
	if( snd_isdirect ) Msg( "Audio: DirectSound\n" );
	if( snd_iswave ) Msg( "Audio: WaveOutput\n" );
}

/*
===========
S_SetHWND

cl_hwnd pointer come from engine
===========
*/
void S_SetHWND( void *hInst )
{
	snd_hwnd = (HWND)hInst;
}

/*
==================
S_BlockSound
==================
*/
void S_BlockSound( void )
{
	// DirectSound takes care of blocking itself
	if( snd_iswave )
	{
		snd_blocked++;

		if( snd_blocked == 1 )
			waveOutReset( hWaveOut );
	}
}

/*
==================
S_UnblockSound
==================
*/
void S_UnblockSound( void )
{
	// DirectSound takes care of blocking itself
	if( snd_iswave )
	{
		snd_blocked--;
	}
}

/*
===========
S_Activate

Called when the main window gains or loses focus.
The window have been destroyed and recreated
between a deactivate and an activate.
===========
*/
void S_Activate( bool active )
{
	if( active )
	{
		
		S_AmbientOn();
		S_UnblockSound();
	}
	else
	{
		S_BlockSound();
		S_AmbientOff();
	}
}

/*
==================
SNDDMA_FreeSound
==================
*/
void SNDDMA_FreeSound( void )
{
	int	i;

	if( pDSBuf )
	{
		pDSBuf->lpVtbl->Stop( pDSBuf );
		pDSBuf->lpVtbl->Release( pDSBuf );
	}

	// only release primary buffer if it's not also the mixing buffer we just released
	if( pDSPBuf && ( pDSBuf != pDSPBuf ))
	{
		pDSPBuf->lpVtbl->Release( pDSPBuf );
	}

	if( pDS && snd_hwnd )
	{
		pDS->lpVtbl->SetCooperativeLevel( pDS, snd_hwnd, DSSCL_NORMAL );
		pDS->lpVtbl->Release( pDS );
	}

	if( hWaveOut )
	{
		waveOutReset( hWaveOut );

		if( lpWaveHdr )
		{
			for( i = 0; i < WAV_BUFFERS; i++ )
				waveOutUnprepareHeader( hWaveOut, lpWaveHdr+i, sizeof( WAVEHDR ));
		}

		waveOutClose( hWaveOut );

		if( hWaveHdr )
		{
			GlobalUnlock( hWaveHdr ); 
			GlobalFree( hWaveHdr );
		}

		if( hData )
		{
			GlobalUnlock( hData );
			GlobalFree( hData );
		}

	}

	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;
	hWaveOut = 0;
	hData = 0;
	hWaveHdr = 0;
	lpData = NULL;
	lpWaveHdr = NULL;
	dsound_init = false;
	wav_init = false;
}

/*
==================
SNDDMA_InitDirect

Direct-Sound support
==================
*/
sndinitstat SNDDMA_InitDirect( void )
{
	DSBUFFERDESC	dsbuf;
	DSBCAPS		dsbcaps;
	DWORD		dwSize, dwWrite;
	DSCAPS		dscaps;
	WAVEFORMATEX	format, pformat; 
	HRESULT		hresult;
	int		reps;

	Mem_Set( &dma, 0, sizeof( dma ));

	dma.channels = 2;
	dma.samplebits = 16;
	dma.speed = 22050;

	Mem_Set( &format, 0, sizeof( format ));

	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = dma.channels;
	format.wBitsPerSample = dma.samplebits;
	format.nSamplesPerSec = dma.speed;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign; 

	if( !dsound_dll.link )
	{
		if( !Sys_LoadLibrary( NULL, &dsound_dll ))
		{
			MsgDev( D_ERROR, "SNDDMA_InitDirect: couldn't load dsound.dll\n" );
			return SIS_FAILURE;
		}
	}

	while(( hresult = iDirectSoundCreate( NULL, &pDS, NULL )) != DS_OK )
	{
		if( hresult != DSERR_ALLOCATED )
		{
			MsgDev( D_ERROR, "SNDDMA_InitDirect: DirectSound create failed\n" );
			return SIS_FAILURE;
		}

		MsgDev( D_ERROR, "SNDDMA_InitDirect: hardware already in use\n" );
		return SIS_NOTAVAIL;
	}

	dscaps.dwSize = sizeof( dscaps );

	if( DS_OK != pDS->lpVtbl->GetCaps( pDS, &dscaps ))
	{
		MsgDev( D_ERROR, "SNDDMA_InitDirect: GetCaps failed\n");
	}

	if( dscaps.dwFlags & DSCAPS_EMULDRIVER )
	{
		MsgDev( D_ERROR, "SNDDMA_InitDirect: no DSound driver found\n" );
		SNDDMA_FreeSound();
		return SIS_FAILURE;
	}

	if( pDS->lpVtbl->SetCooperativeLevel( pDS, snd_hwnd, DSSCL_EXCLUSIVE ) != DS_OK )
	{
		MsgDev( D_ERROR, "SNDDMA_InitDirect: set coop level failed\n" );
		SNDDMA_FreeSound();
		return SIS_FAILURE;
	}

	// get access to the primary buffer, if possible, so we can set the
	// sound hardware format
	Mem_Set( &dsbuf, 0, sizeof( dsbuf ));
	dsbuf.dwSize = sizeof( DSBUFFERDESC );
	dsbuf.dwFlags = DSBCAPS_PRIMARYBUFFER;
	dsbuf.dwBufferBytes = 0;
	dsbuf.lpwfxFormat = NULL;

	Mem_Set( &dsbcaps, 0, sizeof( dsbcaps ));
	dsbcaps.dwSize = sizeof( dsbcaps );
	primary_format_set = false;

	if( pDS->lpVtbl->CreateSoundBuffer( pDS, &dsbuf, &pDSPBuf, NULL ) == DS_OK )
	{
		pformat = format;

		if( pDSPBuf->lpVtbl->SetFormat( pDSPBuf, &pformat ) != DS_OK )
		{
			if( snd_firsttime )
				MsgDev( D_INFO, "Set primary sound buffer format: no\n" );
		}
		else
		{
			if( snd_firsttime )
				MsgDev( D_INFO, "Set primary sound buffer format: yes\n" );
			primary_format_set = true;
		}
	}

	if( !primary_format_set )
	{
		// create the secondary buffer we'll actually work with
		Mem_Set( &dsbuf, 0, sizeof( dsbuf ));
		dsbuf.dwSize = sizeof( DSBUFFERDESC );
		dsbuf.dwFlags = (DSBCAPS_CTRLFREQUENCY|DSBCAPS_LOCSOFTWARE);
		dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
		dsbuf.lpwfxFormat = &format;

		Mem_Set( &dsbcaps, 0, sizeof( dsbcaps ));
		dsbcaps.dwSize = sizeof(dsbcaps);

		if( pDS->lpVtbl->CreateSoundBuffer( pDS, &dsbuf, &pDSBuf, NULL ) != DS_OK )
		{
			MsgDev( D_ERROR, "SNDDMA_InitDirect: DS->CreateSoundBuffer failed\n" );
			SNDDMA_FreeSound();
			return SIS_FAILURE;
		}

		dma.channels = format.nChannels;
		dma.samplebits = format.wBitsPerSample;
		dma.speed = format.nSamplesPerSec;

		if( pDSBuf->lpVtbl->GetCaps( pDSBuf, &dsbcaps ) != DS_OK )
		{
			MsgDev( D_ERROR, "SNDDMA_InitDirect: DS->GetCaps failed\n" );
			SNDDMA_FreeSound();
			return SIS_FAILURE;
		}

		if( snd_firsttime )
			MsgDev( D_INFO, "Using secondary sound buffer\n" );
	}
	else
	{
		if( pDS->lpVtbl->SetCooperativeLevel( pDS, snd_hwnd, DSSCL_WRITEPRIMARY ) != DS_OK )
		{
			MsgDev( D_ERROR, "SNDDMA_InitDirect: DS->SetCooperativeLevel failed\n" );
			SNDDMA_FreeSound();
			return SIS_FAILURE;
		}

		if( pDSPBuf->lpVtbl->GetCaps (pDSPBuf, &dsbcaps ) != DS_OK )
		{
			MsgDev( D_ERROR, "SNDDMA_InitDirect: DS->GetCaps failed\n" );
			SNDDMA_FreeSound();
			return SIS_FAILURE;
		}

		pDSBuf = pDSPBuf;
		MsgDev( D_INFO, "Using primary sound buffer\n" );
	}

	// make sure mixer is active
	pDSBuf->lpVtbl->Play( pDSBuf, 0, 0, DSBPLAY_LOOPING );

	if( snd_firsttime )
		MsgDev( D_INFO, "   %d channel(s)\n""   %d bits/sample\n""   %d bytes/sec\n", dma.channels, dma.samplebits, dma.speed );
	
	gSndBufSize = dsbcaps.dwBufferBytes;

	// initialize the buffer
	reps = 0;

	while(( hresult = pDSBuf->lpVtbl->Lock( pDSBuf, 0, gSndBufSize, &lpData, &dwSize, NULL, NULL, 0 )) != DS_OK )
	{
		if( hresult != DSERR_BUFFERLOST )
		{
			MsgDev( D_ERROR, "SNDDMA_InitDirect: DS->Lock failed\n");
			SNDDMA_FreeSound();
			return SIS_FAILURE;
		}

		if( ++reps > 10000 )
		{
			MsgDev( D_ERROR, "SNDDMA_InitDirect: couldn't restore buffer\n" );
			SNDDMA_FreeSound();
			return SIS_FAILURE;
		}

	}

	Mem_Set( lpData, 0, dwSize );
	pDSBuf->lpVtbl->Unlock( pDSBuf, lpData, dwSize, NULL, 0 );

	// we don't want anyone to access the buffer directly w/o locking it first.
	lpData = NULL; 

	pDSBuf->lpVtbl->Stop( pDSBuf );
	pDSBuf->lpVtbl->GetCurrentPosition( pDSBuf, &mmstarttime.u.sample, &dwWrite );
	pDSBuf->lpVtbl->Play( pDSBuf, 0, 0, DSBPLAY_LOOPING );

	dma.samples = gSndBufSize / (dma.samplebits / 8);
	dma.samplepos = 0;
	dma.submission_chunk = 1;
	dma.buffer = (byte *)lpData;
	sample16 = (dma.samplebits / 8) - 1;

	dsound_init = true;

	return SIS_SUCCESS;
}


/*
==================
SNDDM_InitWav

Crappy windows multimedia base
==================
*/
bool SNDDMA_InitWav( void )
{
	WAVEFORMATEX	format; 
	HRESULT		hr;
	int		i;	

	snd_sent = 0;
	snd_completed = 0;

	Mem_Set( &dma, 0, sizeof( dma ));

	dma.channels = 2;
	dma.samplebits = 16;
	dma.speed = 22050;

	Mem_Set( &format, 0, sizeof( format ));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = dma.channels;
	format.wBitsPerSample = dma.samplebits;
	format.nSamplesPerSec = dma.speed;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign; 
	
	// open a waveform device for output using window callback 
	while(( hr = waveOutOpen((LPHWAVEOUT)&hWaveOut, WAVE_MAPPER, &format, 0, 0L, CALLBACK_NULL )) != MMSYSERR_NOERROR )
	{
		if( hr != MMSYSERR_ALLOCATED )
		{
			MsgDev( D_ERROR, "SNDDMA_InitWav: waveOutOpen failed\n" );
			return false;
		}

		MsgDev( D_ERROR, "SNDDMA_InitWav: hardware already in use\n" );
		return SIS_NOTAVAIL;
	} 

	// Allocate and lock memory for the waveform data. The memory 
	// for waveform data must be globally allocated with 
	// GMEM_MOVEABLE and GMEM_SHARE flags. 

	gSndBufSize = WAV_BUFFERS * WAV_BUFFER_SIZE;
	hData = GlobalAlloc( GMEM_MOVEABLE|GMEM_SHARE, gSndBufSize ); 

	if( !hData ) 
	{ 
		MsgDev( D_ERROR, "SNDDMA_InitWav: Out of memory.\n" );
		SNDDMA_FreeSound();
		return false; 
	}

	lpData = GlobalLock( hData );
	if( !lpData )
	{ 
		MsgDev( D_ERROR, "SNDDMA_InitWav: failed to lock.\n" );
		SNDDMA_FreeSound();
		return false; 
	} 

	Mem_Set( lpData, 0, gSndBufSize );

	// Allocate and lock memory for the header. This memory must 
	// also be globally allocated with GMEM_MOVEABLE and 
	// GMEM_SHARE flags. 
	hWaveHdr = GlobalAlloc( GMEM_MOVEABLE|GMEM_SHARE, (DWORD)sizeof( WAVEHDR ) * WAV_BUFFERS ); 

	if( hWaveHdr == NULL )
	{ 
		MsgDev( D_ERROR, "SNDDMA_InitWav: failed to alloc header.\n" );
		SNDDMA_FreeSound();
		return false; 
	} 

	lpWaveHdr = (LPWAVEHDR) GlobalLock( hWaveHdr ); 

	if( lpWaveHdr == NULL )
	{ 
		MsgDev( D_ERROR, "SNDDMA_InitWav: failed to lock header.\n");
		SNDDMA_FreeSound();
		return false; 
	}

	Mem_Set( lpWaveHdr, 0, sizeof( WAVEHDR ) * WAV_BUFFERS );

	// After allocation, set up and prepare headers. 
	for( i = 0; i < WAV_BUFFERS; i++ )
	{
		lpWaveHdr[i].dwBufferLength = WAV_BUFFER_SIZE; 
		lpWaveHdr[i].lpData = lpData + i * WAV_BUFFER_SIZE;

		if( waveOutPrepareHeader( hWaveOut, lpWaveHdr + i, sizeof( WAVEHDR )) != MMSYSERR_NOERROR )
		{
			MsgDev( D_ERROR, "SNDDMA_InitWav: failed to prepare wave headers\n" );
			SNDDMA_FreeSound();
			return false;
		}
	}

	dma.samples = gSndBufSize / (dma.samplebits / 8);
	dma.samplepos = 0;
	dma.submission_chunk = 1;
	dma.buffer = (byte *)lpData;
	sample16 = (dma.samplebits / 8) - 1;

	wav_init = true;

	return true;
}

/*
==================
SNDDMA_Init

Try to find a sound device to mix for.
Returns false if nothing is found.
==================
*/

int SNDDMA_Init( void )
{
	sndinitstat	stat;

	// assume DirectSound won't initialize
	stat = SIS_FAILURE;
	dsound_init = wav_init = 0;

	// Init DirectSound
	if( snd_firsttime || snd_isdirect )
	{
		stat = SNDDMA_InitDirect();

		if( stat == SIS_SUCCESS )
		{
			if( snd_firsttime )
				MsgDev( D_INFO, "DirectSound initialized\n" );
			snd_isdirect = true;
		}
		else snd_isdirect = false;
	}

	// if DirectSound didn't succeed in initializing, try to initialize
	// waveOut sound, unless DirectSound failed because the hardware is
	// already allocated (in which case the user has already chosen not
	// to have sound )

	if( !dsound_init && ( stat != SIS_NOTAVAIL ))
	{
		if( snd_firsttime || snd_iswave )
		{
			snd_iswave = SNDDMA_InitWav();

			if( snd_iswave )
			{
				if( snd_firsttime )
					MsgDev( D_INFO, "Wave sound initialized\n" );
			}
		}
	}

	if( !dsound_init && !wav_init )
	{
		if( snd_firsttime )
			MsgDev( D_NOTE, "No sound device initialized\n" );
		snd_firsttime = false;
		return 0;
	}

	snd_firsttime = false;
	return 1;
}

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
int SNDDMA_GetDMAPos( void )
{
	MMTIME	mmtime;
	DWORD	dwWrite;
	int	s;

	if( dsound_init ) 
	{
		mmtime.wType = TIME_SAMPLES;
		pDSBuf->lpVtbl->GetCurrentPosition( pDSBuf, &mmtime.u.sample, &dwWrite );
		s = mmtime.u.sample - mmstarttime.u.sample;
	}
	else if( wav_init )
	{
		s = snd_sent * WAV_BUFFER_SIZE;
	}


	s >>= sample16;
	s &= ( dma.samples - 1 );

	return s;
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit( void )
{
	LPWAVEHDR	h;
	int	wResult;

	if( !wav_init )
		return;

	// find which sound blocks have completed
	while( 1) 
	{
		if( snd_completed == snd_sent )
		{
			MsgDev( D_INFO, "Sound overrun\n" );
			break;
		}

		if(!( lpWaveHdr[snd_completed & WAV_MASK].dwFlags & WHDR_DONE ))
			break;
		snd_completed++;	// this buffer has been played
	}

	// submit two new sound blocks
	while((( snd_sent - snd_completed ) >> sample16 ) < 4 )
	{
		h = lpWaveHdr + ( snd_sent & WAV_MASK );

		snd_sent++;

		// Now the data block can be sent to the output device. The 
		// waveOutWrite function returns immediately and waveform 
		// data is sent to the output device in the background. 
		wResult = waveOutWrite( hWaveOut, h, sizeof( WAVEHDR )); 

		if( wResult != MMSYSERR_NOERROR )
		{ 
			MsgDev( D_ERROR, "SNDDMA_Submit: failed to write block to device\n" );
			SNDDMA_FreeSound();
			return; 
		} 
	}
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown( void )
{
	SNDDMA_FreeSound();
}