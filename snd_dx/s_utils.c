//=======================================================================
//			Copyright XashXT Group 2009 ©
//		       s_utils.c - common sound functions
//=======================================================================

#include "sound.h"

// hardcoded macros to test for zero crossing
#define ZERO_X_8( b )	(( b )< 2 && ( b ) > -2 )
#define ZERO_X_16( b )	(( b )< 512 && ( b ) > -512 )

//-----------------------------------------------------------------------------
// Purpose: Search backward for a zero crossing starting at sample
// Input  : sample - starting point
// Output : position of zero crossing
//-----------------------------------------------------------------------------
int S_ZeroCrossingBefore( wavdata_t *pWaveData, int sample )
{
	if( pWaveData == NULL )
		return sample;

	if( pWaveData->type == WF_PCMDATA )
	{
		if( pWaveData->width == 1 )
		{
								// FIXME: this is right ?
			char	*pData = pWaveData->buffer + sample * pWaveData->width;
			bool	zero = false;

			if( pWaveData->channels == 1 )
			{
				while( sample > 0 && !zero )
				{
					if( ZERO_X_8( *pData ))
					{
						zero = true;
					}
					else
					{
						sample--;
						pData--;
					}
				}
			}
			else
			{
				while( sample > 0 && !zero )
				{
					if( ZERO_X_8( *pData ) && ZERO_X_8( pData[1] ))
					{
						zero = true;
					}
					else
					{
						sample--;
						pData--;
					}
				}
			}
		}
		else
		{							// FIXME: this is right ?
			short	*pData = (short *)(pWaveData->buffer + sample * pWaveData->width);
			bool	zero = false;

			if( pWaveData->channels == 1 )
			{
				while( sample > 0 && !zero )
				{
					if( ZERO_X_16(*pData ))
					{
						zero = true;
					}
					else
					{
						pData--;
						sample--;
					}
				}
			}
			else
			{
				while( sample > 0 && !zero )
				{
					if( ZERO_X_16( *pData ) && ZERO_X_16( pData[1] ))
					{
						zero = true;
					}
					else
					{
						sample--;
						pData--;
					}
				}
			}
		}
	}
	return sample;
}

//-----------------------------------------------------------------------------
// Purpose: Search forward for a zero crossing
// Input  : sample - starting point
// Output : position of found zero crossing
//-----------------------------------------------------------------------------
int S_ZeroCrossingAfter( wavdata_t *pWaveData, int sample )
{
	if( pWaveData == NULL )
		return sample;

	if( pWaveData->type == WF_PCMDATA )
	{
		if( pWaveData->width == 1 )	// 8-bit
		{
								// FIXME: this is right ?
			char	*pData = pWaveData->buffer + sample * pWaveData->width;
			bool	zero = false;

			if( pWaveData->channels == 1 )
			{
				while( sample < pWaveData->samples && !zero )
				{
					if( ZERO_X_8( *pData ))
					{
						zero = true;
					}
					else
					{
						sample++;
						pData++;
					}
				}
			}
			else
			{
				while( sample < pWaveData->samples && !zero )
				{
					if( ZERO_X_8( *pData ) && ZERO_X_8( pData[1] ))
					{
						zero = true;
					}
					else
					{
						sample++;
						pData++;
					}
				}
			}
		}
		else
		{							// FIXME: this is right ?
			short	*pData = (short *)(pWaveData->buffer + sample * pWaveData->width);
			bool	zero = false;

			if( pWaveData->channels == 1 )
			{
				while( sample > 0 && !zero )
				{
					if( ZERO_X_16( *pData ))
					{
						zero = true;
					}
					else
					{
						pData++;
						sample++;
					}
				}
			}
			else
			{
				while( sample > 0 && !zero )
				{
					if( ZERO_X_16( *pData ) && ZERO_X_16( pData[1] ))
					{
						zero = true;
					}
					else
					{
						sample++;
						pData++;
					}
				}
			}
		}
	}
	return sample;
}

//-----------------------------------------------------------------------------
// Purpose: wrap the position wrt looping
// Input  : samplePosition - absolute position
// Output : int - looped position
//-----------------------------------------------------------------------------
int S_ConvertLoopedPosition( wavdata_t *pSource, int samplePosition )
{
	// if the wave is looping and we're past the end of the sample
	// convert to a position within the loop
	// At the end of the loop, we return a short buffer, and subsequent call
	// will loop back and get the rest of the buffer
	if( pSource->loopStart >= 0 && samplePosition >= pSource->samples )
	{
		// size of loop
		int	loopSize = pSource->samples - pSource->loopStart;

		// subtract off starting bit of the wave
		samplePosition -= pSource->loopStart;
		
		if( loopSize )
		{
			// "real" position in memory (mod off extra loops)
			samplePosition = pSource->loopStart + ( samplePosition % loopSize );
		}
		// ERROR? if no loopSize
	}
	return samplePosition;
}

int S_GetOutputData( wavdata_t *pSource, void **pData, int samplePosition, int sampleCount )
{
	int	totalSampleCount;

	// handle position looping
	samplePosition = S_ConvertLoopedPosition( pSource, samplePosition );

	// how many samples are available (linearly not counting looping)
	totalSampleCount = pSource->samples - samplePosition;

	// may be asking for a sample out of range, clip at zero
	if( totalSampleCount < 0 ) totalSampleCount = 0;

	// clip max output samples to max available
	if( sampleCount > totalSampleCount )
		sampleCount = totalSampleCount;

	// byte offset in sample database
	samplePosition *= pSource->width;	// FIXME: this is right ?

	// if we are returning some samples, store the pointer
	if( sampleCount )
	{
		*pData = pSource->buffer + samplePosition;
	}
	return sampleCount;
}