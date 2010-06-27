//=======================================================================
//			Copyright XashXT Group 2010 ©
//			 s_mouth.c - animate mouth
//=======================================================================

#include "sound.h"
#include "const.h"

#define CAVGSAMPLES		10

void SND_InitMouth( int entnum, int entchannel )
{
	if(( entchannel == CHAN_VOICE || entchannel == CHAN_STREAM ) && entnum > 0 )
	{
		edict_t	*clientEntity;

		// init mouth movement vars
		clientEntity = si.GetClientEdict( entnum );

		if( clientEntity && !clientEntity->free )
		{
			mouth_t	*m = si.GetEntityMouth( clientEntity );

			if( m )
			{
				m->mouthopen = 0;
				m->sndavg = 0;
				m->sndcount = 0;
			}
		}
	}
}

void SND_CloseMouth( channel_t *ch )
{
	edict_t	*clientEntity = si.GetClientEdict( ch->entnum );

	if( clientEntity && !clientEntity->free )
	{
		mouth_t	*m = si.GetEntityMouth( clientEntity );

		if( m && ( ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_STREAM ))
		{
			// shut mouth
			m->mouthopen = 0;
		}
	}
}

void SND_MoveMouth8( channel_t *ch, wavdata_t *pSource, int count )
{
	int 	i, data;
	edict_t	*clientEntity;
	char	*pdata = NULL;
	mouth_t	*pMouth = NULL;
	int	savg;
	int	scount;

	clientEntity = si.GetClientEdict( ch->entnum );

	if( clientEntity && !clientEntity->free )
	{
		pMouth = si.GetEntityMouth( clientEntity );
          }

	if( !pMouth ) return;

	S_GetOutputData( pSource, &pdata, ch->pos, count );

	if( pdata == NULL )
		return;
	
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