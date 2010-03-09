// 02/08/02 November235: Particle System
#pragma once

class ParticleType;
class ParticleSystem;
void CreateAurora( int idx, char *file ); //make new partsystem

#define COLLISION_NONE	0
#define COLLISION_DIE	1
#define COLLISION_BOUNCE	2

struct particle
{
	particle		*nextpart;
	particle		*m_pOverlay; // for making multi-layered particles
	ParticleType	*pType;

	vec3_t		origin;
	vec3_t		velocity;
	vec3_t		accel;
	vec3_t		m_vecWind;

	int		m_iEntIndex; // if non-zero, this particle is tied to the given entity

	float		m_fRed;
	float		m_fGreen;
	float		m_fBlue;
	float		m_fRedStep;
	float		m_fGreenStep;
	float		m_fBlueStep;

	float		m_fAlpha;
	float		m_fAlphaStep;

	float		frame;
	float		m_fFrameStep;

	float		m_fAngle;
	float		m_fAngleStep;

	float		m_fSize;
	float		m_fSizeStep;

	float		m_fDrag;

	float		age;
	float		age_death;
	float		age_spray;
};


class RandomRange
{
public:
	RandomRange() { m_fMin = m_fMax = 0; m_bDefined = false; }
	RandomRange(float fValue) { m_fMin = m_fMax = fValue; m_bDefined = true; }
	RandomRange(float fMin, float fMax) { m_fMin = fMin; m_fMax = fMax; m_bDefined = true; }
	RandomRange( char *szToken );

	float m_fMax;
	float m_fMin;
	bool m_bDefined;

	float GetInstance()
	{
		return RANDOM_FLOAT( m_fMin, m_fMax );
	}

	float GetOffset( float fBasis )
	{
		return GetInstance() - fBasis;
	}

	bool IsDefined( void )
	{
		return m_bDefined;
	}
};

#define MAX_TYPENAME	256

class ParticleType
{
public:
	ParticleType( ParticleType *pNext = NULL );
	ParticleType( char *szFilename );

	bool		m_bIsDefined; // is this ParticleType just a placeholder?
	kRenderMode_t	m_iRenderMode;
	int		m_iDrawCond;
	int		m_iCollision;
	RandomRange	m_Bounce;
	RandomRange	m_BounceFriction;
	bool		m_bBouncing;

	RandomRange	m_Life;

	RandomRange m_StartAlpha;
	RandomRange m_EndAlpha;
	RandomRange m_StartRed;
	RandomRange m_EndRed;
	RandomRange m_StartGreen;
	RandomRange m_EndGreen;
	RandomRange m_StartBlue;
	RandomRange m_EndBlue;

	RandomRange m_StartSize;
	RandomRange m_SizeDelta;
	RandomRange m_EndSize;

	RandomRange m_StartFrame;
	RandomRange m_EndFrame;
	RandomRange m_FrameRate; // incompatible with EndFrame
	bool m_bEndFrame;

	RandomRange m_StartAngle;
	RandomRange m_AngleDelta;

	RandomRange m_SprayRate;
	RandomRange m_SprayForce;
	RandomRange m_SprayPitch;
	RandomRange m_SprayYaw;
	RandomRange m_SprayRoll;
	ParticleType *m_pSprayType;

	RandomRange m_Gravity;
	RandomRange m_WindStrength;
	RandomRange m_WindYaw;

	int m_SpriteIndex;
	ParticleType *m_pOverlayType;

	RandomRange m_Drag;

	ParticleType *m_pNext;

	char m_szName[MAX_TYPENAME];

	// here is a particle system. Add a (set of) particles according to this type, and initialise their values.
	particle* CreateParticle(ParticleSystem *pSys);//particle *pPart);

	// initialise this particle. Does not define velocity or age.
	void InitParticle(particle *pPart, ParticleSystem *pSys);
};

class ParticleSystem
{
public:
	ParticleSystem( int entindex, char *szFilename );
	~ParticleSystem( void );
	void AllocateParticles( int iParticles );
	void CalculateDistance();

	ParticleType *GetType( const char *szName );
	ParticleType *AddPlaceholderType( const char *szName );
	ParticleType *ParseType( const char **szFile );

	edict_t *GetEntity() { return GetEntityByIndex( m_iEntIndex ); }

	static float c_fCosTable[360 + 90];
	static bool c_bCosTableInit;

	// General functions
	bool	UpdateSystem( float frametime ); //If this function returns false, the manager deletes the system
	void	DrawSystem();
	particle *ActivateParticle(); // adds one of the free particles to the active list, and returns it for initialisation.

	static float CosLookup(int angle) { return angle < 0? c_fCosTable[angle+360]: c_fCosTable[angle]; }
	static float SinLookup(int angle) { return angle < -90? c_fCosTable[angle+450]: c_fCosTable[angle+90]; }

	// returns false if the particle has died
	bool UpdateParticle( particle *part, float frametime );
	void DrawParticle( particle* part, vec3_t &right, vec3_t &up );

	// Utility functions that have to be public
	bool ParticleIsVisible( particle* part );

	// Pointer to next system for linked list structure	
	ParticleSystem* m_pNextSystem;

	particle*		m_pActiveParticle;
	float		m_fViewerDist;
	int		m_iEntIndex;
	int 		m_iEntAttachment;
	int		m_iKillCondition;
	int		enable;
private:
	// the block of allocated particles
	particle*		m_pAllParticles;
	// First particles in the linked list for the active particles and the dead particles
	particle*		m_pFreeParticle;
	particle*		m_pMainParticle; // the "source" particle.

	ParticleType	*m_pFirstType;
	ParticleType	*m_pMainType;
};

class ParticleSystemManager
{
public:
	ParticleSystemManager( void );
	~ParticleSystemManager( void );

	ParticleSystem	*FindSystem( edict_t* pEntity );
	void		SortSystems( void );
	void		AddSystem( ParticleSystem* );
	void		UpdateSystems( void );
	void		ClearSystems( void );
	ParticleSystem	*m_pFirstSystem;
};

extern ParticleSystemManager* g_pParticleSystems; 