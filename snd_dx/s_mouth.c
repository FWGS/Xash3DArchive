//=======================================================================
//			Copyright XashXT Group 2010 �
//			 s_mouth.c - animate mouth
//=======================================================================

#include "sound.h"
#include "const.h"

#define CAVGSAMPLES		10

void SND_InitMouth( int entnum, int entchannel )
{
	if(( entchannel == CHAN_VOICE || entchannel == CHAN_STREAM ) && entnum > 0 )
	{
		cl_entity_t	*clientEntity;

		// init mouth movement vars
		clientEntity = si.GetClientEdict( entnum );

		if( clientEntity )
		{
			clientEntity->mouth.mouthopen = 0;
			clientEntity->mouth.sndavg = 0;
			clientEntity->mouth.sndcount = 0;
		}
	}
}

void SND_CloseMouth( channel_t *ch )
{
	if( ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_STREAM )
	{
		cl_entity_t	*clientEntity;

		clientEntity = si.GetClientEdict( ch->entnum );

		if( clientEntity )
		{
			// shut mouth
			clientEntity->mouth.mouthopen = 0;
		}
	}
}

void SND_MoveMouth8( channel_t *ch, wavdata_t *pSource, int count )
{
	cl_entity_t	*clientEntity;
	char		*pdata = NULL;
	mouth_t		*pMouth = NULL;
	int		savg, data;
	int		scount;
	uint 		i;

	clientEntity = si.GetClientEdict( ch->entnum );
	if( !clientEntity ) return;

	pMouth = &clientEntity->mouth;

	S_GetOutputData( pSource, &pdata, ch->pos, count );

	if( pdata == NULL ) return;
	
	i = 0;
	scount = pMouth->sndcount;
	savg = 0;

	while( i < count && scount < CAVGSAMPLES )
	{
		data = pdata[i];
		savg += abs( data );	

		i += 80 + ((byte)data & 0x1F);
		scount++;
	}

	pMouth->sndavg += savg;
	pMouth->sndcount = (byte)scount;

	if( pMouth->sndcount >= CAVGSAMPLES ) 
	{
		pMouth->mouthopen = pMouth->sndavg / CAVGSAMPLES;
		pMouth->sndavg = 0;
		pMouth->sndcount = 0;
	}
}