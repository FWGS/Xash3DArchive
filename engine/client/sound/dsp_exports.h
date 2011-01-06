void DSP_InitAll( void );	// initalize
void DSP_FreeAll( void );	// shutdown
void DSP_ClearState( void );	// same as VidInit
int DSP_Alloc( int ipset, float xfade, int cchan );			// alloc
void DSP_SetPreset( int idsp, int ipsetnew );				// set preset
void DSP_Process( int idsp, portable_samplepair_t *pbfront, int sampleCount );	// process
void DSP_Free( int idsp );						// free