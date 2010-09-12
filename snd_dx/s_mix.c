//=======================================================================
//			Copyright XashXT Group 2009 ©
//		      s_mix.c - portable code to mix sounds
//=======================================================================

#include "sound.h"
#include "byteorder.h"

#define PAINTBUFFER_SIZE	512
portable_samplepair_t	paintbuffer[PAINTBUFFER_SIZE];
int			snd_scaletable[32][256];
int 			*snd_p, snd_linear_count, snd_vol;
short			*snd_out;

void S_WriteLinearBlastStereo16( void )
{
	int	i, val;

	for( i = 0; i < snd_linear_count; i += 2 )
	{
		val = snd_p[i]>>8;
		if( val > 0x7fff ) snd_out[i] = 0x7fff;
		else if( val < (short)0x8000 )
			snd_out[i] = (short)0x8000;
		else snd_out[i] = val;

		val = snd_p[i+1]>>8;
		if( val > 0x7fff ) snd_out[i+1] = 0x7fff;
		else if( val < (short)0x8000 )
			snd_out[i+1] = (short)0x8000;
		else snd_out[i+1] = val;
	}
}

void S_TransferStereo16( dword *pbuf, int endtime )
{
	int	lpos, lpaintedtime;
	
	snd_p = (int *)paintbuffer;
	lpaintedtime = paintedtime;

	while( lpaintedtime < endtime )
	{
		// handle recirculating buffer issues
		lpos = lpaintedtime & ((dma.samples >> 1) - 1);

		snd_out = (short *) pbuf + (lpos << 1);

		snd_linear_count = (dma.samples>>1) - lpos;
		if( lpaintedtime + snd_linear_count > endtime )
			snd_linear_count = endtime - lpaintedtime;

		snd_linear_count <<= 1;

		// write a linear blast of samples
		S_WriteLinearBlastStereo16();

		snd_p += snd_linear_count;
		lpaintedtime += (snd_linear_count >> 1);
	}
}

/*
===================
S_TransferPaintBuffer

===================
*/
void S_TransferPaintBuffer( int endtime )
{
	int 	out_idx, count;
	int 	step, val;
	int 	*p, out_mask;
	dword	*pbuf;

	pbuf = (dword *)dma.buffer;

	if( dma.samplebits == 16 && dma.channels == 2 )
	{	
		// optimized case
		S_TransferStereo16( pbuf, endtime );
	}
	else
	{	
		// general case
		p = (int *)paintbuffer;
		count = (endtime - paintedtime) * dma.channels;
		out_mask = dma.samples - 1; 
		out_idx = paintedtime * dma.channels & out_mask;
		step = 3 - dma.channels;

		if( dma.samplebits == 16 )
		{
			short	*out = (short *)pbuf;

			while( count-- )
			{
				val = *p >> 8;
				p += step;
				if( val > 0x7fff ) val = 0x7fff;
				else if( val < (short)0x8000 )
					val = (short)0x8000;
				out[out_idx] = val;
				out_idx = (out_idx + 1) & out_mask;
			}
		}
		else if( dma.samplebits == 8 )
		{
			byte	*out = (byte *)pbuf;

			while( count-- )
			{
				val = *p >> 8;
				p += step;
				if( val > 0x7fff ) val = 0x7fff;
				else if( val < (short)0x8000 )
					val = (short)0x8000;
				out[out_idx] = (val>>8) + 128;
				out_idx = (out_idx + 1) & out_mask;
			}
		}
	}
}

/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/
void S_PaintMonoFrom8( portable_samplepair_t *pbuf, int *volume, byte *pData, int outCount )
{
	int 	i, data;
	int	*lscale, *rscale;
		
	lscale = snd_scaletable[volume[0] >> 3];
	rscale = snd_scaletable[volume[1] >> 3];

	for( i = 0; i < outCount; i++ )
	{
		data = pData[i];
		pbuf[i].left += lscale[data];
		pbuf[i].right += rscale[data];
	}
}

void S_PaintStereoFrom8( portable_samplepair_t *pbuf, int *volume, byte *pData, int outCount )
{
	int	*lscale, *rscale;
	uint	left, right;
	word	*data;
	int	i;

	lscale = snd_scaletable[volume[0] >> 3];
	rscale = snd_scaletable[volume[1] >> 3];
	data = (word *)pData;

	for( i = 0; i < outCount; i++, data++ )
	{
		left = (byte)((*data & 0x00FF));
		right = (byte)((*data & 0xFF00) >> 8);
		pbuf[i].left += lscale[left];
		pbuf[i].right += rscale[right];
	}
}

void S_PaintMonoFrom16( portable_samplepair_t *pbuf, int *volume, short *pData, int outCount )
{
	int	i, data;
	int	left, right;
	int	lscale, rscale;

	lscale = volume[0] * snd_vol;
	rscale = volume[1] * snd_vol;

	for( i = 0; i < outCount; i++ )
	{
		data = pData[i];
		left = ( data * lscale ) >> 8;
		right = (data * rscale ) >> 8;
		pbuf[i].left += left;
		pbuf[i].right += right;
	}
}

void S_PaintStereoFrom16( portable_samplepair_t *pbuf, int *volume, short *pData, int outCount )
{
	uint	*data;
	int	lscale, rscale;
	int	left, right;
	int	i;

	lscale = volume[0] * snd_vol;
	rscale = volume[1] * snd_vol;
	data = (uint *)pData;
		
	for( i = 0; i < outCount; i++, data++ )
	{
		left = (signed short)((*data & 0x0000FFFF));
		right = (signed short)((*data & 0xFFFF0000) >> 16);

		left =  (left * lscale ) >> 8;
		right = (right * rscale) >> 8;

		pbuf[i].left += left;
		pbuf[i].right += right;
	}
}

void S_Mix8Mono( portable_samplepair_t *pbuf, int *volume, byte *pData, int inputOffset, uint rateScaleFix, int outCount )
{
	int	i, sampleIndex = 0;
	uint	sampleFrac = inputOffset;
	int	*lscale, *rscale;

	// Not using pitch shift?
	if( rateScaleFix == FIX( 1 ))
	{
		S_PaintMonoFrom8( pbuf, volume, pData, outCount );
		return;
	}

	lscale = snd_scaletable[volume[0] >> 3];
	rscale = snd_scaletable[volume[1] >> 3];

	for( i = 0; i < outCount; i++ )
	{
		pbuf[i].left += lscale[pData[sampleIndex]];
		pbuf[i].right += rscale[pData[sampleIndex]];
		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART( sampleFrac );
		sampleFrac = FIX_FRACPART( sampleFrac );
	}
}

void S_Mix8Stereo( portable_samplepair_t *pbuf, int *volume, byte *pData, int inputOffset, uint rateScaleFix, int outCount )
{
	int	i, sampleIndex = 0;
	uint	sampleFrac = inputOffset;
	int	*lscale, *rscale;

	// Not using pitch shift?
	if( rateScaleFix == FIX( 1 ))
	{
		S_PaintStereoFrom8( pbuf, volume, pData, outCount );
		return;
	}

	lscale = snd_scaletable[volume[0] >> 3];
	rscale = snd_scaletable[volume[1] >> 3];

	for( i = 0; i < outCount; i++ )
	{
		pbuf[i].left += lscale[pData[sampleIndex+0]];
		pbuf[i].right += rscale[pData[sampleIndex+1]];
		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART( sampleFrac )<<1;
		sampleFrac = FIX_FRACPART( sampleFrac );
	}
}

void S_Mix16Mono( portable_samplepair_t *pbuf, int *volume, short *pData, int inputOffset, uint rateScaleFix, int outCount )
{
	int	i, sampleIndex = 0;
	uint	sampleFrac = inputOffset;
	int	lscale, rscale;

	// Not using pitch shift?
	if( rateScaleFix == FIX( 1 ))
	{
		S_PaintMonoFrom16( pbuf, volume, pData, outCount );
		return;
	}

	lscale = volume[0] * snd_vol;
	rscale = volume[1] * snd_vol;

	for( i = 0; i < outCount; i++ )
	{
		pbuf[i].left += (lscale * (int)( pData[sampleIndex] ))>>8;
		pbuf[i].right += (rscale * (int)( pData[sampleIndex] ))>>8;
		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART( sampleFrac );
		sampleFrac = FIX_FRACPART( sampleFrac );
	}
}

void S_Mix16Stereo( portable_samplepair_t *pbuf, int *volume, short *pData, int inputOffset, uint rateScaleFix, int outCount )
{
	int	i, sampleIndex = 0;
	uint	sampleFrac = inputOffset;
	int	lscale, rscale;

	// Not using pitch shift?
	if( rateScaleFix == FIX( 1 ))
	{
		S_PaintStereoFrom16( pbuf, volume, pData, outCount );
		return;
	}
	
	lscale = volume[0] * snd_vol;
	rscale = volume[1] * snd_vol;

	for( i = 0; i < outCount; i++ )
	{
		pbuf[i].left += (lscale * (int)( pData[sampleIndex+0] ))>>8;
		pbuf[i].right += (rscale * (int)( pData[sampleIndex+1] ))>>8;

		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}

int S_MixDataToDevice( channel_t *pChannel, int sampleCount, int outputRate, int outputOffset )
{
	// save this to compute total output
	int	startingOffset = outputOffset;
	float	inputRate = ( pChannel->pitch * pChannel->sfx->cache->rate );
	float	rate = inputRate / outputRate;
	int	pvol[2];
		
	// shouldn't be playing this if finished, but return if we are
	if( pChannel->pMixer.finished )
		return 0;

	// If we are terminating this wave prematurely, then make sure we detect the limit
	if( pChannel->pMixer.forcedEndSample )
	{
		// how many total input samples will we need?
		int	samplesRequired = (int)(sampleCount * rate);

		// will this hit the end?
		if( pChannel->pMixer.sample + samplesRequired >= pChannel->pMixer.forcedEndSample )
		{
			// yes, mark finished and truncate the sample request
			pChannel->pMixer.finished = true;
			sampleCount = (int)(( pChannel->pMixer.forcedEndSample - pChannel->pMixer.sample ) / rate );
		}
	}

	pvol[0] = pChannel->leftvol;
	pvol[1] = pChannel->rightvol;

	while( sampleCount > 0 )
	{
		double	sampleFraction;
		portable_samplepair_t *pbuf;
		bool	advanceSample = true;
		int	availableSamples, outputSampleCount;
		wavdata_t	*pSource = pChannel->sfx->cache;
		bool	use_loop = pChannel->use_loop;
		char	*pData = NULL;

		// compute number of input samples required
		double	end = pChannel->pMixer.sample + rate * sampleCount;
		int	inputSampleCount = (int)(ceil( end ) - floor( pChannel->pMixer.sample ));

		availableSamples = S_GetOutputData( pSource, &pData, pChannel->pMixer.sample, inputSampleCount, use_loop );

		// none available, bail out
		if( !availableSamples ) break;

		sampleFraction = pChannel->pMixer.sample - floor( pChannel->pMixer.sample );

		if( availableSamples < inputSampleCount )
		{
			// how many samples are there given the number of input samples and the rate.
			outputSampleCount = (int)ceil(( availableSamples - sampleFraction ) / rate );
		}
		else
		{
			outputSampleCount = sampleCount;
		}

		// Verify that we won't get a buffer overrun.
		ASSERT( floor( sampleFraction + rate * ( outputSampleCount - 1 )) <= availableSamples );

		// mix this data to all active paintbuffers
		pbuf = &paintbuffer[outputOffset];

		if( pSource->channels == 1 )
		{
			if( pSource->width == 1 )
				S_Mix8Mono( pbuf, pvol, (char *)pData, FIX_FLOAT(sampleFraction), FIX_FLOAT(rate), outputSampleCount );
			else S_Mix16Mono( pbuf, pvol, (short *)pData, FIX_FLOAT(sampleFraction), FIX_FLOAT(rate), outputSampleCount );
		}
		else
		{
			if( pSource->width == 1 )
				S_Mix8Stereo( pbuf, pvol, (char *)pData, FIX_FLOAT(sampleFraction), FIX_FLOAT(rate), outputSampleCount );
			else S_Mix16Stereo( pbuf, pvol, (short *)pData, FIX_FLOAT(sampleFraction), FIX_FLOAT(rate), outputSampleCount );
		}

		if( advanceSample )
		{
			pChannel->pMixer.sample += outputSampleCount * rate;
		}

		outputOffset += outputSampleCount;
		sampleCount -= outputSampleCount;
	}

	// Did we run out of samples? if so, mark finished
	if( sampleCount > 0 )
	{
		pChannel->pMixer.finished = true;
	}

	// total number of samples mixed !!! at the output clock rate !!!
	return outputOffset - startingOffset;
}

bool S_ShouldContinueMixing( channel_t *ch )
{
	if( ch->isSentence )
	{
		if( ch->currentWord )
			return true;
		return false;
	}

	return !ch->pMixer.finished;
}

void S_MixAllChannels( int endtime, int end )
{
	channel_t *ch;
	wavdata_t	*sc;
	int	i, count = end - paintedtime;

	// paint in the channels.
	for( i = 0, ch = channels; i < total_channels; i++, ch++ )
	{
		if( !ch->sfx ) continue;
		if( !ch->leftvol && !ch->rightvol && !ch->isSentence )
		{
			// sentences must be playing even if not hearing
			continue;
		}

		sc = S_LoadSound( ch->sfx );
		if( !sc ) continue;

		if( s_listener.inmenu && !ch->localsound )
		{
			// play only local sounds, keep pause for other
			continue;
		}
		else if( !s_listener.inmenu && !s_listener.active && !ch->staticsound )
		{
			// play only ambient sounds, keep pause for other
			continue;
		}
		else if( s_listener.paused )
		{
			// play only ambient sounds, keep pause for other
			continue;
		}

		// get playback pitch
		if( ch->isSentence )
			ch->pitch = VOX_ModifyPitch( ch, ch->basePitch * 0.01f );
		else ch->pitch = ch->basePitch * 0.01f;

		// check volume
		ch->leftvol = bound( 0, ch->leftvol, 255 );
		ch->rightvol = bound( 0, ch->rightvol, 255 );

		if( si.GetClientEdict( ch->entnum ) && ( ch->entchannel == CHAN_VOICE ))
		{
			SND_MoveMouth8( ch, sc, count );
		}

		if( ch->isSentence )
			VOX_MixDataToDevice( ch, count, dma.speed, 0 );
		else S_MixDataToDevice( ch, count, dma.speed, 0 );

		if( !S_ShouldContinueMixing( ch ))
		{
			S_FreeChannel( ch );
		}
	}
}

void S_PaintChannels( int endtime )
{
	int	end;

	snd_vol = S_GetMasterVolume () * 256;

	while( paintedtime < endtime )
	{
		// if paintbuffer is smaller than DMA buffer
		end = endtime;
		if( endtime - paintedtime > PAINTBUFFER_SIZE )
			end = paintedtime + PAINTBUFFER_SIZE;

		// clear the paint buffer
		if( s_rawend < paintedtime )
		{
			Mem_Set( paintbuffer, 0, (end - paintedtime) * sizeof( portable_samplepair_t ));
		}
		else
		{	
			int	i, stop;

			// copy from the streaming sound source
			stop = (end < s_rawend) ? end : s_rawend;

			for( i = paintedtime; i < stop; i++ )
				paintbuffer[i - paintedtime] = s_rawsamples[i & (MAX_RAW_SAMPLES - 1)];
			
			for( ; i < end; i++ )
				paintbuffer[i-paintedtime].left = paintbuffer[i-paintedtime].right = 0;
		}

		S_MixAllChannels( endtime, end );

//		SX_RoomFX( endtime, true, true );

		// transfer out according to DMA format
		S_TransferPaintBuffer( end );
		paintedtime = end;
	}
}

void S_InitScaletable( void )
{
	int	i, j;
	int	scale;

	for( i = 0; i < 32; i++ )
	{
		scale = i * 8 * 256 * S_GetMasterVolume();
		for( j = 0; j < 256; j++ ) snd_scaletable[i][j] = ((signed char)j) * scale;
	}
	s_volume->modified = false;
}
