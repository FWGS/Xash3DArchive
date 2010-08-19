//=======================================================================
//			Copyright XashXT Group 2010 ©
//		      r_tempents.h - tempentities management
//=======================================================================

#ifndef R_TEMPENTS_H
#define R_TEMPENTS_H

#include "effects_api.h"

//-----------------------------------------------------------------------------
// Purpose: implementation for temp entities
//-----------------------------------------------------------------------------

// TEMPENTITY priority
#define TENTPRIORITY_LOW		0
#define TENTPRIORITY_HIGH		1

// TEMPENTITY flags
#define FTENT_NONE			0
#define FTENT_SINEWAVE		(1<<0)
#define FTENT_GRAVITY		(1<<1)
#define FTENT_ROTATE		(1<<2)
#define FTENT_SLOWGRAVITY		(1<<3)
#define FTENT_SMOKETRAIL		(1<<4)
#define FTENT_COLLIDEWORLD		(1<<5)
#define FTENT_FLICKER		(1<<6)
#define FTENT_FADEOUT		(1<<7)
#define FTENT_SPRANIMATE		(1<<8)
#define FTENT_HITSOUND		(1<<9)
#define FTENT_SPIRAL		(1<<10)
#define FTENT_SPRCYCLE		(1<<11)
#define FTENT_COLLIDEALL		(1<<12)	// will collide with world and slideboxes
#define FTENT_PERSIST		(1<<13)	// tent is not removed when unable to draw 
#define FTENT_COLLIDEKILL		(1<<14)	// tent is removed upon collision with anything
#define FTENT_PLYRATTACHMENT		(1<<15)	// tent is attached to a player (owner)
#define FTENT_SPRANIMATELOOP		(1<<16)	// animating sprite doesn't die when last frame is displayed
#define FTENT_SCALE			(1<<17)	// an experiment
#define FTENT_SPARKSHOWER		(1<<18)
#define FTENT_NOMODEL		(1<<19)	// sets by engine, never draw (it just triggers other things)
#define FTENT_CLIENTCUSTOM		(1<<20)	// Must specify callback. Callback function is responsible
					// for killing tempent and updating fields
					// ( unless other flags specify how to do things )
#define FTENT_WINDBLOWN		(1<<21)	// This is set when the temp entity is blown by the wind
#define FTENT_NEVERDIE		(1<<22)	// Don't die as long as die != 0
#define FTENT_BEOCCLUDED		(1<<23)	// Don't draw if my specified normal's facing away from the view
#define FTENT_ATTACHMENT		(1<<24)	// This is a dynamically relinked attachment (update every frame)

typedef struct tempent_s TEMPENTITY;

struct tempent_s
{
	int		flags;
	float		die;
	float		frameMax;		// this is also animtime for studiomodels
	float		x, y, z;		// probably z isn't used
	float		fadeSpeed;
	float		bounceFactor;
	int		hitSound;
	void		(*hitcallback)( struct tempent_s *ent, struct pmtrace_s *ptr );
	void		(*callback)( struct tempent_s *ent, float frametime, float currenttime );
	TEMPENTITY	*next;
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

	void	TE_Prepare( TEMPENTITY *pTemp, int modelIndex );
	int	TE_Active( TEMPENTITY *pTemp );
	int	TE_Update( TEMPENTITY *pTemp );	// return false for instantly die

	void	BloodSprite( const Vector &org, int colorIndex, int modelIndex, int modelIndex2, float size );
	void	RicochetSprite( const Vector &pos, int modelIndex, float scale );
	void	MuzzleFlash( cl_entity_t *pEnt, int iAttachment, int type );
	TEMPENTITY *TempModel( const Vector &pos, const Vector &dir, const Vector &ang, float life, int modelIndex, int soundtype );
	void	BreakModel( const Vector &pos, const Vector &size, const Vector &dir, float random, float life, int count, int modelIndex, char flags );
	void	Bubbles( const Vector &mins, const Vector &maxs, float height, int modelIndex, int count, float speed );
	void	BubbleTrail( const Vector &start, const Vector &end, float height, int modelIndex, int count, float speed );
	void	Sprite_Explode( TEMPENTITY *pTemp, float scale, int flags );
	void	FizzEffect( cl_entity_t *pent, int modelIndex, int density );
	TEMPENTITY *DefaultSprite( const Vector &pos, int spriteIndex, float framerate );
	void	Sprite_Smoke( TEMPENTITY *pTemp, float scale );
	TEMPENTITY *TempSprite( const Vector &pos, const Vector &dir, float scale, int modelIndex, int rendermode, int renderfx, float a, float life, int flags );
	void	AttachTentToPlayer( int client, int modelIndex, float zoffset, float life );
	void	KillAttachedTents( int client );
	void	Sprite_Spray( const Vector &pos, const Vector &dir, int modelIndex, int count, int speed, int iRand, int renderMode = kRenderTransAlpha );
	void	Sprite_Trail( int type, const Vector &vecStart, const Vector &vecEnd, int modelIndex, int nCount, float flLife, float flSize, float flAmplitude, int nRenderamt, float flSpeed );
	void	RocketFlare( const Vector& pos );
	void	PlaySound( TEMPENTITY *pTemp, float damp );
	void	TracerEffect( const Vector &start, const Vector &end );
	void	WeaponFlash( cl_entity_t *pEnt, int iAttachment );
	void	PlaceDecal( Vector pos, int entityIndex, int decalIndex );
	void	PlaceDecal( Vector pos, int entityIndex, const char *decalname );
	void	AllocDLight( Vector pos, byte r, byte g, byte b, float radius, float time, float decay = 0.0f );
	void	AllocDLight( Vector pos, float radius, float time, float decay = 0.0f );
	void	RocketTrail( Vector start, Vector end, int type );
	void	Large_Funnel( Vector pos, int spriteIndex, int flags );
	void	SparkShower( const Vector& pos );
	void	StreakSplash( const Vector &pos, const Vector &dir, int color, int count, int speed, int velMin, int velMax );
// Data
private:
	int		m_iTempEntFrame;	// used for keyed dlights only

	// Global temp entity pool
	TEMPENTITY	m_TempEnts[MAX_TEMP_ENTITIES];

	// Free and active temp entity lists
	TEMPENTITY	*m_pFreeTempEnts;
	TEMPENTITY	*m_pActiveTempEnts;

	// muzzle flash sprites
	int		m_iMuzzleFlash[MAX_MUZZLEFLASH];

	void		TempEntFree( TEMPENTITY *pTemp, TEMPENTITY *pPrev );
	bool		FreeLowPriorityTempEnt( void );
public:
	TEMPENTITY	*TempEntAllocNoModel( const Vector& org );
	TEMPENTITY	*TempEntAlloc( const Vector& org, int modelindex );
	TEMPENTITY	*TempEntAllocHigh( const Vector& org, int modelIndex );
	TEMPENTITY	*TempEntAllocCustom( const Vector& org, int modelIndex, int high, void ( *callback )( struct tempent_s *ent, float frametime, float currenttime ));

	// misc utility shaders
	HSPRITE		hSprGlowShell;	// glowshell shader
};

extern CTempEnts *g_pTempEnts;

#endif//R_TEMPENTS_H