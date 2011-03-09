//=======================================================================
//			Copyright XashXT Group 2007 �
//			 s_load.c - sound managment
//=======================================================================

#include "common.h"
#include "sound.h"

// during registration it is possible to have more sounds
// than could actually be referenced during gameplay,
// because we don't want to free anything until we are
// sure we won't need it.
#define MAX_SFX		8192
#define MAX_SFX_HASH	(MAX_SFX/4)

static int	s_numSfx = 0;
static sfx_t	s_knownSfx[MAX_SFX];
static sfx_t	*s_sfxHashList[MAX_SFX_HASH];
static string	s_sentenceImmediateName;	// keep dummy sentence name
qboolean		s_registering = false;
int		s_registration_sequence = 0;

/*
=================
S_SoundList_f
=================
*/
void S_SoundList_f( void )
{
	sfx_t		*sfx;
	wavdata_t		*sc;
	int		i, totalSfx = 0;
	int		totalSize = 0;

	for( i = 0, sfx = s_knownSfx; i < s_numSfx; i++, sfx++ )
	{
		if( !sfx->touchFrame )
			continue;

		sc = sfx->cache;
		if( sc )
		{
			totalSize += sc->size;

			if( sc->loopStart >= 0 ) Msg( "L" );
			else Msg( " " );
			Msg( " (%2db) %s : sound/%s\n", sc->width * 8, Q_memprint( sc->size ), sfx->name );
			totalSfx++;
		}
	}

	Msg( "-------------------------------------------\n" );
	Msg( "%i total sounds\n", totalSfx );
	Msg( "%s total memory\n", Q_memprint( totalSize ));
	Msg( "\n" );
}

// return true if char 'c' is one of 1st 2 characters in pch
qboolean S_TestSoundChar( const char *pch, char c )
{
	int	i;
	char	*pcht = (char *)pch;

	// check first 2 characters
	for( i = 0; i < 2; i++ )
	{
		if( *pcht == c )
			return true;
		pcht++;
	}
	return false;
}

// return pointer to first valid character in file name
char *S_SkipSoundChar( const char *pch )
{
	char *pcht = (char *)pch;

	// check first character
	if( *pcht == '!' )
		pcht++;
	return pcht;
}

/*
=================
S_CreateDefaultSound
=================
*/
static wavdata_t *S_CreateDefaultSound( void )
{
	wavdata_t	*sc;

	sc = Mem_Alloc( sndpool, sizeof( wavdata_t ));

	sc->width = 2;
	sc->channels = 1;
	sc->loopStart = -1;
	sc->rate = SOUND_DMA_SPEED;
	sc->samples = SOUND_DMA_SPEED;
	sc->size = sc->samples * sc->width * sc->channels;
	sc->buffer = Mem_Alloc( sndpool, sc->size );

	return sc;
}

/*
=================
S_LoadSound
=================
*/
wavdata_t *S_LoadSound( sfx_t *sfx )
{
	wavdata_t	*sc;

	if( !sfx ) return NULL;
	if( sfx->cache ) return sfx->cache; // see if still in memory

	// load it from disk
	if( sfx->name[0] == '*' )
		sc = FS_LoadSound( sfx->name + 1, NULL, 0 );
	else sc = FS_LoadSound( sfx->name, NULL, 0 );

	if( !sc ) sc = S_CreateDefaultSound();
	sfx->cache = sc;

	return sfx->cache;
}

// =======================================================================
// Load a sound
// =======================================================================
/*
==================
S_FindName

==================
*/
sfx_t *S_FindName( const char *name, int *pfInCache )
{
	int	i;
	sfx_t	*sfx;
	uint	hash;

	if( !name || !name[0] || !dma.initialized )
		return NULL;

	if( Q_strlen( name ) >= MAX_STRING )
	{
		MsgDev( D_ERROR, "S_FindSound: sound name too long: %s", name );
		return NULL;
	}

	// see if already loaded
	hash = Com_HashKey( name, MAX_SFX_HASH );
	for( sfx = s_sfxHashList[hash]; sfx; sfx = sfx->hashNext )
	{
		if( !Q_strcmp( sfx->name, name ))
		{
			if( pfInCache )
			{
				// indicate whether or not sound is currently in the cache.
				*pfInCache = ( sfx->cache != NULL ) ? true : false;
			}
			// prolonge registration
			sfx->touchFrame = s_registration_sequence;
			return sfx;
		}
	}

	// find a free sfx slot spot
	for( i = 0, sfx = s_knownSfx; i < s_numSfx; i++, sfx++)
	{
		if( !sfx->name[0] ) break; // free spot
	}
	if( i == s_numSfx )
	{
		if( s_numSfx == MAX_SFX )
		{
			MsgDev( D_ERROR, "S_FindName: MAX_SFX limit exceeded\n" );
			return NULL;
		}
		s_numSfx++;
	}
	
	sfx = &s_knownSfx[i];
	Mem_Set( sfx, 0, sizeof( *sfx ));
	if( pfInCache ) *pfInCache = false;
	Q_strncpy( sfx->name, name, MAX_STRING );
	sfx->touchFrame = s_registration_sequence;
	sfx->hashValue = Com_HashKey( sfx->name, MAX_SFX_HASH );

	// link it in
	sfx->hashNext = s_sfxHashList[sfx->hashValue];
	s_sfxHashList[sfx->hashValue] = sfx;
		
	return sfx;
}

/*
==================
S_FreeSound
==================
*/
void S_FreeSound( sfx_t *sfx )
{
	sfx_t	*hashSfx;
	sfx_t	**prev;

	if( !sfx || !sfx->name[0] ) return;

	// de-link it from the hash tree
	prev = &s_sfxHashList[sfx->hashValue];
	while( 1 )
	{
		hashSfx = *prev;
		if( !hashSfx )
			break;

		if( hashSfx == sfx )
		{
			*prev = hashSfx->hashNext;
			break;
		}
		prev = &hashSfx->hashNext;
	}

	if( sfx->cache ) FS_FreeSound( sfx->cache );
	Mem_Set( sfx, 0, sizeof( *sfx ));
}

/*
=====================
S_BeginRegistration

=====================
*/
void S_BeginRegistration( void )
{
	s_registration_sequence++;
	s_registering = true;
}

/*
=====================
S_EndRegistration

=====================
*/
void S_EndRegistration( void )
{
	sfx_t	*sfx;
	int	i;

	if( !dma.initialized ) return;
	
	// free any sounds not from this registration sequence
	for( i = 0, sfx = s_knownSfx; i < s_numSfx; i++, sfx++ )
	{
		if( !sfx->name[0] ) continue;
		if( sfx->touchFrame != s_registration_sequence )
			S_FreeSound( sfx ); // don't need this sound
	}

	// load everything in
	for( i = 0, sfx = s_knownSfx; i < s_numSfx; i++, sfx++ )
	{
		if( !sfx->name[0] ) continue;
		S_LoadSound( sfx );
	}
	s_registering = false;
}

/*
==================
S_RegisterSound

==================
*/
sound_t S_RegisterSound( const char *name )
{
	sfx_t	*sfx;

	if( !dma.initialized ) return 0;

	if( S_TestSoundChar( name, '!' ))
	{
		Q_strncpy( s_sentenceImmediateName, name, sizeof( s_sentenceImmediateName ));
		return SENTENCE_INDEX;
	}

	sfx = S_FindName( name, NULL );
	if( !sfx ) return -1;

	sfx->touchFrame = s_registration_sequence;
	if( !s_registering ) S_LoadSound( sfx );

	return sfx - s_knownSfx;
}

sfx_t *S_GetSfxByHandle( sound_t handle )
{
	if( !dma.initialized )
		return NULL;

	if( handle == SENTENCE_INDEX )
	{
		// create new sfx
		return S_FindName( s_sentenceImmediateName, NULL );
	}

	if( handle < 0 || handle >= s_numSfx )
	{
		MsgDev( D_ERROR, "S_GetSfxByHandle: handle %i out of range (%i)\n", handle, s_numSfx );
		return NULL;
	}
	return &s_knownSfx[handle];
}

/*
=================
S_FreeSounds
=================
*/
void S_FreeSounds( void )
{
	sfx_t	*sfx;
	int	i;

	if( !dma.initialized )
		return;

	// stop all sounds
	S_StopAllSounds();

	// free all sounds
	for( i = 0, sfx = s_knownSfx; i < s_numSfx; i++, sfx++ )
		S_FreeSound( sfx );

	Mem_Set( s_knownSfx, 0, sizeof( s_knownSfx ));
	Mem_Set( s_sfxHashList, 0, sizeof( s_sfxHashList ));

	s_numSfx = 0;
}