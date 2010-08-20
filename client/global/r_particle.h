//=======================================================================
//			Copyright XashXT Group 2010 ©
//	    r_particle.h - another particle manager (come from q2e 0.40)
//=======================================================================

#ifndef R_PARTICLE_H
#define R_PARTICLE_H

#define NUMVERTEXNORMALS		162
#define MAX_PARTICLES		4096

#define SIMSHIFT			10
#define SPARK_COLORCOUNT		9

typedef enum
{
	pt_static, 
	pt_grav,
	pt_slowgrav,
	pt_fire,
	pt_explode,
	pt_explode2,
	pt_blob,
	pt_blob2,
	pt_vox_slowgrav,
	pt_vox_grav,
	pt_clientcustom		// Must have callback function specified
} ptype_t;

class CBaseParticle
{
public:
	CBaseParticle	*m_pNext;		// linked list
	HSPRITE		m_hSprite;	// custom texture

	float		m_flLifetime;
	byte		m_Color[4];	// RGBA - not all effects need to use this.
	ptype_t		m_Type;
	Vector		m_Pos;
	Vector		m_Velocity;

	word		m_Ramp;
	byte		context;		// for deathfunc etc

	// tracer stuff
	float		m_flWidth;
	float		m_flLength;

	// Color and alpha values are 0 - 1
	void		SetColor( int pcolor );
	void		SetColor( float r, float g, float b );	//  0 - 1
	void		SetColor( int color[3] );	//  0 - 255
	void		SetAlpha( float a );

	void		SetType( ptype_t ptype ) { m_Type = ptype; }
	ptype_t		GetType( void ) { return m_Type; };

	void		SetLifetime( float life ) { m_flLifetime = GetClientTime() + life; }
	float		GetLifetime( void ) { return m_flLifetime; }

	void		SetTexture( HSPRITE hSpr ) { m_hSprite = hSpr; }

	void		(*pfnCallback)( CBaseParticle *pPart, float frametime );	// for pt_clientcustom
	void		(*pfnDeathFunc)( CBaseParticle *pPart );
};

inline void CBaseParticle :: SetColor( int pcolor )
{
	float	entry[3];

	CL_GetPaletteColor( pcolor, entry );

	m_Color[0] = (byte)entry[0];
	m_Color[1] = (byte)entry[1];
	m_Color[2] = (byte)entry[2];
	m_Color[3] = 0xFF;	// no alpha
}

inline void CBaseParticle :: SetColor( int color[3] )
{
	m_Color[0] = color[0];
	m_Color[1] = color[1];
	m_Color[2] = color[2];
	m_Color[3] = 0xFF;	// no alpha
}

inline void CBaseParticle :: SetColor( float r, float g, float b )
{
	m_Color[0] = (byte)(r * 255.9f);
	m_Color[1] = (byte)(g * 255.9f);
	m_Color[2] = (byte)(b * 255.9f);
}

inline void CBaseParticle :: SetAlpha( float a )
{
	m_Color[3] = (byte)(a * 255.9f);
}

class CParticleSystem
{
	CBaseParticle	*m_pActiveParticles;
	CBaseParticle	*m_pFreeParticles;
	CBaseParticle	m_pParticles[MAX_PARTICLES];	// particle pool

	Vector		m_vecAvertexNormals[NUMVERTEXNORMALS];
	Vector		m_vecAvelocities[NUMVERTEXNORMALS];

	// this is a cached local copy of Q1 palette (comed from engine)
	byte		m_uchPalette[256][3];

	// private partsystem shaders
	HSPRITE		m_hDefaultParticle;

	float		m_flTime;		// the current client time
	float		m_fOldTime;	// the time at which the HUD was last redrawn

	// view vectors
	Vector		m_vecForward;
	Vector		m_vecRight;
	Vector		m_vecUp;
	Vector		m_vecOrigin;	// view origin

public:
			CParticleSystem( void );
	virtual		~CParticleSystem( void );

	void		Clear( void );
	void		Update( void );
	void		SimulateAndRender( CBaseParticle *pCur );
	void		FreeParticle( CBaseParticle *pCur );
	CBaseParticle	*AllocParticle( HSPRITE m_hSpr = 0 );
	float		GetTimeDelta( void ) { return m_flTime - m_fOldTime; }

	// draw methods
	void		DrawParticle( HSPRITE hSpr, const Vector &pos, const byte color[4], float size );
	bool		TracerComputeVerts( const Vector &pos, const Vector &delta, float width, Vector *pVerts );
	void		DrawTracer( HSPRITE hSpr, Vector& start, Vector& delta, float width, byte *color,
			float startV = 0.0f, float endV = 1.0f );

	// begin user effects here
	void		EntityParticles( cl_entity_t *ent );
	void		ParticleEffect( const Vector org, const Vector dir, int color, int count );
	void		ParticleExplosion( const Vector org );
	void		ParticleExplosion2( const Vector org, int colorStart, int colorLength );
	void		BlobExplosion( const Vector org );
	void		LavaSplash( const Vector org );
	void		TeleportSplash( const Vector org );
	void		RocketTrail( const Vector org, const Vector end, int type );
	void		SparkleTracer( const Vector& pos, const Vector& dir );
	void		BulletTracer( const Vector& pos, const Vector& end );
	void		StreakTracer( const Vector& pos, const Vector& velocity, int color );
	void		BulletImpactParticles( const Vector &pos );
};

extern CParticleSystem	*g_pParticles;

#endif//R_PARTICLE_H