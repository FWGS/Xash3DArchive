//=======================================================================
//			Copyright XashXT Group 2010 ©
//		      r_tempents.h - tempentities management
//=======================================================================

#ifndef R_TEMPENTS_H
#define R_TEMPENTS_H

#include "r_efx.h"

//-----------------------------------------------------------------------------
// Purpose: implementation for temp entities
//-----------------------------------------------------------------------------

typedef struct tent_s TENT;

struct tent_s
{
	int		flags;
	float		die;
	float		frameMax;		// this is also animtime for studiomodels
	float		x, y, z;		// probably z isn't used
	float		fadeSpeed;
	float		bounceFactor;
	int		hitSound;
	void		(*hitcallback)( struct tent_s *ent, struct pmtrace_s *ptr );
	void		(*callback)( struct tent_s *ent, float frametime, float currenttime );
	TENT	*next;
	int		priority;		// 0 - low, 1 - high
	short		clientIndex;	// if attached, this is the index of the client to stick to
					// if COLLIDEALL, this is the index of the client to ignore
					// TENTS with FTENT_PLYRATTACHMENT MUST set the clientindex! 
	vec3_t		tentOffset;	// if attached, client origin + tentOffset = tent origin.
	cl_entity_t	entity;

	// baseline.origin		- velocity
	// baseline.renderamt	- starting fadeout intensity
	// baseline.angles		- angle velocity
};

#define MAX_TEMP_ENTITIES		500
#define TENT_WIND_ACCEL		50
#define MAX_MUZZLEFLASH		4
#define SHARD_VOLUME		12.0	// on shard ever n^3 units

class CTempEnts
{
public:
		CTempEnts( void );
	virtual	~CTempEnts( void );

	void	Update( void );
	void	Clear( void );

	void	TE_Prepare( TENT *pTemp, int modelIndex );
	int	TE_Active( TENT *pTemp );
	int	TE_Update( TENT *pTemp, float frametime );	// return false for instantly die

	void	BloodSprite( const Vector &org, int colorIndex, int modelIndex, int modelIndex2, float size );
	void	RicochetSprite( const Vector &pos, int modelIndex, float scale );
	void	MuzzleFlash( cl_entity_t *pEnt, int iAttachment, int type );
	TENT *TempModel( const Vector &pos, const Vector &dir, const Vector &ang, float life, int modelIndex, int soundtype );
	void	BreakModel( const Vector &pos, const Vector &size, const Vector &dir, float random, float life, int count, int modelIndex, char flags );
	void	Bubbles( const Vector &mins, const Vector &maxs, float height, int modelIndex, int count, float speed );
	void	BubbleTrail( const Vector &start, const Vector &end, float height, int modelIndex, int count, float speed );
	void	Sprite_Explode( TENT *pTemp, float scale, int flags );
	void	FizzEffect( cl_entity_t *pent, int modelIndex, int density );
	TENT *DefaultSprite( const Vector &pos, int spriteIndex, float framerate );
	void	Sprite_Smoke( TENT *pTemp, float scale );
	TENT *TempSprite( const Vector &pos, const Vector &dir, float scale, int modelIndex, int rendermode, int renderfx, float a, float life, int flags );
	void	AttachTentToPlayer( int client, int modelIndex, float zoffset, float life );
	void	KillAttachedTents( int client );
	void	Sprite_Spray( const Vector &pos, const Vector &dir, int modelIndex, int count, int speed, int iRand, int renderMode = kRenderTransAlpha );
	void	Sprite_Trail( int type, const Vector &vecStart, const Vector &vecEnd, int modelIndex, int nCount, float flLife, float flSize, float flAmplitude, int nRenderamt, float flSpeed );
	void	RocketFlare( const Vector& pos );
	void	PlaySound( TENT *pTemp, float damp );
	void	TracerEffect( const Vector &start, const Vector &end );
	void	WeaponFlash( cl_entity_t *pEnt, int iAttachment );
	void	PlaceDecal( Vector pos, int entityIndex, int decalIndex );
	void	PlaceDecal( Vector pos, int entityIndex, const char *decalname );
	void	AllocDLight( Vector pos, byte r, byte g, byte b, float radius, float time, float decay = 0.0f );
	void	AllocDLight( Vector pos, float radius, float time, float decay = 0.0f );
	void	RocketTrail( Vector start, Vector end, int type );
	void	Large_Funnel( Vector pos, int spriteIndex, int flags );
	void	SparkShower( const Vector& pos );
	void	SparkEffect( const Vector& pos, int count, int velocityMin, int velocityMax );
	void	StreakSplash( const Vector &pos, const Vector &dir, int color, int count, int speed, int velMin, int velMax );
// Data
private:
	float		m_flTime;		// the current client time
	float		m_fOldTime;	// the time at which the HUD was last redrawn
	
	int		m_iTempEntFrame;	// used for keyed dlights only

	// Global temp entity pool
	TENT	m_TempEnts[MAX_TEMP_ENTITIES];

	// Free and active temp entity lists
	TENT	*m_pFreeTempEnts;
	TENT	*m_pActiveTempEnts;

	// muzzle flash sprites
	int		m_iMuzzleFlash[MAX_MUZZLEFLASH];

	void		TempEntFree( TENT *pTemp, TENT *pPrev );
	bool		FreeLowPriorityTempEnt( void );
public:
	TENT	*TempEntAllocNoModel( const Vector& org );
	TENT	*TempEntAlloc( const Vector& org, int modelindex );
	TENT	*TempEntAllocHigh( const Vector& org, int modelIndex );
	TENT	*TempEntAllocCustom( const Vector& org, int modelIndex, int high, void ( *callback )( struct tent_s *ent, float frametime, float currenttime ));

	// misc utility shaders
	HSPRITE		hSprGlowShell;	// glowshell shader
};

extern CTempEnts *g_pTempEnts;

#endif//R_TEMPENTS_H