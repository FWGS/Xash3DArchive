//=======================================================================
//			Copyright XashXT Group 2008 �
//			  hud.h - hud primary header
//=======================================================================

#define RGB_YELLOWISH	0x00FFA000	// 255, 160, 0
#define RGB_REDISH		0x00FF1010	// 255, 160, 0
#define RGB_GREENISH	0x0000A000	// 0, 160, 0

#include "hud_ammo.h"

#define DHN_DRAWZERO	1
#define DHN_2DIGITS		2
#define DHN_3DIGITS		4
#define MIN_ALPHA		100

#define HUDELEM_ACTIVE	1
#define HUD_MAX_FADES	8		// can be blindly increased

typedef struct
{
	int	x, y;
} POSITION;

// This structure is sent over the net to describe a screen shake event
typedef struct
{
	float	time;
	float	duration;
	float	amplitude;
	float	frequency;
	float	nextShake;
	Vector	offset;
	float	angle;
	Vector	appliedOffset;
	float	appliedAngle;
} ScreenShake;

typedef struct
{
	float	fadeSpeed;	// How fast to fade (tics / second) (+ fade in, - fade out)
	float	fadeEnd;		// When the fading hits maximum
	float	fadeReset;	// When to reset to not fading (for fadeout and hold)
	Vector	fadeColor;
	float	fadeAlpha;
	int	fadeFlags;	// Fading flags
} ScreenFade;

typedef struct
{
	int	dripsPerSecond;
	float	distFromPlayer;
	float	windX, windY;
	float	randX, randY;
	int	weatherMode;	// 0 - snow, 1 - rain
	float	globalHeight;
} RainData;

typedef struct
{
	byte	r, g, b, a;
} RGBA;

#define HUD_ACTIVE			1
#define HUD_INTERMISSION		2
#define MAX_SEC_AMMO_VALUES		4
#define MAX_PLAYER_NAME_LENGTH	32
#define MAX_MOTD_LENGTH		1536
#define FADE_TIME			100
#define maxHUDMessages		16
#define MAX_SPRITE_NAME_LENGTH	24

//
//-----------------------------------------------------
//
class CHudBase
{
public:
	POSITION		m_pos;
	int		m_type;
	int		m_iFlags;		// active, moving,

	virtual ~CHudBase(){ }
	virtual int Init( void ){ return 0; }
	virtual int VidInit( void ){ return 0; }
	virtual int Draw( float flTime ){ return 0; }
	virtual void Think( void ){ return; }
	virtual void Reset( void ){ return; }
	virtual void InitHUDData( void ){ }	// called every time a server is connected to
};

struct HUDLIST
{
	CHudBase	*p;
	HUDLIST	*pNext;
};

//
//-----------------------------------------------------
//
class CHudAmmo: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw(float flTime);
	void Think(void);
	void Reset(void);
	int DrawWList(float flTime);
	int MsgFunc_CurWeapon( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_WeaponList( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_AmmoX(const char *pszName, int iSize, void *pbuf );
	int MsgFunc_AmmoPickup( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_WeapPickup( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_ItemPickup( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_HideWeapon( const char *pszName, int iSize, void *pbuf );

	void SlotInput( int iSlot );
	void _cdecl UserCmd_Slot1( void );
	void _cdecl UserCmd_Slot2( void );
	void _cdecl UserCmd_Slot3( void );
	void _cdecl UserCmd_Slot4( void );
	void _cdecl UserCmd_Slot5( void );
	void _cdecl UserCmd_Slot6( void );
	void _cdecl UserCmd_Slot7( void );
	void _cdecl UserCmd_Slot8( void );
	void _cdecl UserCmd_Slot9( void );
	void _cdecl UserCmd_Slot10( void );
	void _cdecl UserCmd_Close( void );
	void _cdecl UserCmd_NextWeapon( void );
	void _cdecl UserCmd_PrevWeapon( void );

private:
	float	m_fFade;
	RGBA	m_rgba;
	WEAPON	*m_pWeapon;
	int	m_HUD_bucket0;
	int	m_HUD_selection;

};

//
//-----------------------------------------------------
//
class CHudAmmoSecondary: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	void Reset( void );
	int Draw(float flTime);

	int MsgFunc_SecAmmoVal( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_SecAmmoIcon( const char *pszName, int iSize, void *pbuf );

private:
	int m_HUD_ammoicon; // sprite indices
	int m_iAmmoAmounts[MAX_SEC_AMMO_VALUES];
	float m_fFade;
};


#include "hud_health.h"

//
//-----------------------------------------------------
//
class CHudGeiger: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_Geiger( const char *pszName, int iSize, void *pbuf );

private:
	int m_iGeigerRange;
};

//
//-----------------------------------------------------
//
class CHudTrain: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_Train( const char *pszName, int iSize, void *pbuf );

private:
	HSPRITE m_hSprite;
	int m_iPos;

};

class CHudMOTD : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Reset( void );

	int MsgFunc_MOTD( const char *pszName, int iSize, void *pbuf );

protected:
	static int MOTD_DISPLAY_TIME;
	char m_szMOTD[MAX_MOTD_LENGTH];
	float m_flActiveTill;
	int m_iLines;
};

//
//-----------------------------------------------------
//
class CHudSound: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int MsgFunc_Fsound( const char *pszName, int iSize, void *pbuf );
	int PlayStream( const char* name );
	int Draw( float flTime ); // used for get pause
	int Close( void );
private:
	int m_iStatus;
	int m_iTime;
	float m_flVolume;
};

//
//-----------------------------------------------------
//
class CHudScoreboard: public CHudBase
{
public:
	int Init( void );
	void InitHUDData( void );
	int VidInit( void );
	int Draw( float flTime );
	int DrawPlayers( int xoffset, float listslot, int nameoffset = 0, char *team = NULL ); // returns the ypos where it finishes drawing
	void UserCmd_ShowScores( void );
	void UserCmd_HideScores( void );
	int MsgFunc_ScoreInfo( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_TeamInfo( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_TeamScore( const char *pszName, int iSize, void *pbuf );
	void DeathMsg( int killer, int victim );

	enum
	{ 
		MAX_PLAYERS = 64,
		MAX_TEAMS = 64,
		MAX_TEAM_NAME = 16,
	};

	struct extra_player_info_t
	{
		short frags;
		short deaths;
		char teamname[MAX_TEAM_NAME];
	};

	struct team_info_t
	{
		char name[MAX_TEAM_NAME];
		short frags;
		short deaths;
		short ping;
		short packetloss;
		short ownteam;
		short players;
		int already_drawn;
		int scores_overriden;
	};

	hud_player_info_t m_PlayerInfoList[MAX_PLAYERS+1]; // player info from the engine
	extra_player_info_t m_PlayerExtraInfo[MAX_PLAYERS+1];  // additional player info sent directly to the client.dll
	team_info_t m_TeamInfo[MAX_TEAMS+1];

	int m_iNumTeams;
	int m_iLastKilledBy;
	int m_fLastKillTime;
	int m_iPlayerNum;
	int m_iShowscoresHeld;

	void GetAllPlayersInfo( void );
};

//
//-----------------------------------------------------
//
class CHudStatusBar : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Reset( void );
	void ParseStatusString( int line_num );

	int MsgFunc_StatusText( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_StatusValue( const char *pszName, int iSize, void *pbuf );

protected:
	enum
	{
		MAX_STATUSTEXT_LENGTH = 128,
		MAX_STATUSBAR_VALUES = 8,
		MAX_STATUSBAR_LINES = 2,
	};

	char m_szStatusText[MAX_STATUSBAR_LINES][MAX_STATUSTEXT_LENGTH]; // a text string describing how the status bar is to be drawn
	char m_szStatusBar[MAX_STATUSBAR_LINES][MAX_STATUSTEXT_LENGTH]; // the constructed bar that is drawn
	int m_iStatusValues[MAX_STATUSBAR_VALUES]; // an array of values for use in the status bar

	int m_bReparseString; // set to TRUE whenever the m_szStatusBar needs to be recalculated

	// an array of colors...one color for each line
	float *m_pflNameColors[MAX_STATUSBAR_LINES];
};

//
//-----------------------------------------------------
//
class CHudDeathNotice : public CHudBase
{
public:
	int Init( void );
	void InitHUDData( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_DeathMsg( const char *pszName, int iSize, void *pbuf );

private:
	int m_HUD_d_skull;  // sprite index of skull icon
};

//
//-----------------------------------------------------
//
class CHudMenu : public CHudBase
{
public:
	int Init( void );
	void InitHUDData( void );
	int VidInit( void );
	void Reset( void );
	int Draw( float flTime );
	int MsgFunc_ShowMenu( const char *pszName, int iSize, void *pbuf );

	void SelectMenuItem( int menu_item );

	int m_fMenuDisplayed;
	int m_bitsValidSlots;
	float m_flShutoffTime;
	int m_fWaitingForMore;
};

//
//-----------------------------------------------------
//
class CHudSayText : public CHudBase
{
public:
	int Init( void );
	void InitHUDData( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_SayText( const char *pszName, int iSize, void *pbuf );
	void SayTextPrint( const char *pszBuf, int iBufSize, int clientIndex = -1 );
	void EnsureTextFitsInOneLineAndWrapIfHaveTo( int line );
private:

	float m_HUD_saytext;
	float m_HUD_saytext_time;
};

//
//-----------------------------------------------------
//
class CHudBattery: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_Battery(const char *pszName,  int iSize, void *pbuf );

private:
	HSPRITE m_hSprite1;
	HSPRITE m_hSprite2;
	wrect_t *m_prc1;
	wrect_t *m_prc2;
	int m_iBat;
	float m_fFade;
	int m_iHeight; // width of the battery innards
};


//
//-----------------------------------------------------
//
class CHudFlashlight: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw(float flTime);
	void Reset( void );
	int MsgFunc_Flashlight( const char *pszName,  int iSize, void *pbuf );
	int MsgFunc_FlashBat( const char *pszName,  int iSize, void *pbuf );

private:
	HSPRITE m_hSprite1;
	HSPRITE m_hSprite2;
	HSPRITE m_hBeam;
	wrect_t *m_prc1;
	wrect_t *m_prc2;
	wrect_t *m_prcBeam;
	float m_flBat;
	int m_iBat;
	int m_fOn;
	float m_fFade;
	int m_iWidth;	// width of the battery innards
};

class CHudRedeemer: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_WarHUD( const char *pszName,  int iSize, void *pbuf );
	int m_iHudMode;
	int m_iOldHudMode;

private:
	HSPRITE m_hSprite;
	HSPRITE m_hCrosshair;
	HSPRITE m_hStatic;
	HSPRITE m_hCamera;
	HSPRITE m_hCamRec;
};

class CHudZoom: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_ZoomHUD( const char *pszName,  int iSize, void *pbuf );
	int m_iHudMode;
private:
	HSPRITE m_hCrosshair;
	HSPRITE m_hLines;
};
//
//-----------------------------------------------------
//
struct message_parms_t
{
	client_textmessage_t *pMessage;
	float time;
	int x, y;
	int totalWidth, totalHeight;
	int width;
	int lines;
	int lineLength;
	int length;
	int r, g, b;
	int text;
	int fadeBlend;
	float charTime;
	float fadeTime;
};

//
//-----------------------------------------------------
//
class CHudTextMessage: public CHudBase
{
public:
	int Init( void );
	static char *LocaliseTextString( const char *msg, char *dst_buffer, int buffer_size );
	static char *BufferedLocaliseTextString( const char *msg );
	char *LookupString( const char *msg_name, int *msg_dest = NULL );
	int MsgFunc_TextMsg( const char *pszName, int iSize, void *pbuf );
};

//
//-----------------------------------------------------
//

class CHudMessage: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_HudText( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_GameTitle( const char *pszName, int iSize, void *pbuf );

	float FadeBlend( float fadein, float fadeout, float hold, float localTime );
	int XPosition( float x, int width, int lineWidth );
	int YPosition( float y, int height );

	void MessageAdd( const char *pName, float time );
	void MessageAdd( client_textmessage_t * newMessage );
	void MessageDrawScan( client_textmessage_t *pMessage, float time );
	void MessageScanStart( void );
	void MessageScanNextChar( void );
	void Reset( void );

private:
	client_textmessage_t *m_pMessages[maxHUDMessages];
	float m_startTime[maxHUDMessages];
	message_parms_t m_parms;
	float m_gameTitleTime;
	client_textmessage_t *m_pGameTitle;
	int HUD_Logo; // display logo
};

//
//-----------------------------------------------------
//
class CHudStatusIcons: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	void Reset( void );
	int Draw( float flTime );
	int MsgFunc_StatusIcon( const char *pszName, int iSize, void *pbuf );

	enum
	{
		MAX_ICONSPRITENAME_LENGTH = MAX_SPRITE_NAME_LENGTH,
		MAX_ICONSPRITES = 4,
	};

	// had to make these public so CHud could access them (to enable concussion icon)
	// could use a friend declaration instead...
	void EnableIcon( char *pszIconName, byte red, byte green, byte blue );
	void DisableIcon( char *pszIconName );

private:

	typedef struct
	{
		char szSpriteName[MAX_ICONSPRITENAME_LENGTH];
		HSPRITE spr;
		wrect_t rc;
		byte r, g, b;
	} icon_sprite_t;

	icon_sprite_t m_IconList[MAX_ICONSPRITES];
};

//
//-----------------------------------------------------
//

#define SKY_OFF	0
#define SKY_ON	1

class CHud
{
private:
	HUDLIST		*m_pHudList;
	client_sprite_t	*m_pSpriteList;
	int		m_iSpriteCount;
	float		m_flMouseSensitivity;
	int		m_iConcussionEffect;

public:
	HSPRITE m_hsprCursor;
	float m_flTime;	 // the current client time
	float m_fOldTime;	 // the time at which the HUD was last redrawn
	double m_flTimeDelta;// the difference between flTime and fOldTime
	float m_vecOrigin[3];
	float m_vecAngles[3];
	int m_iKeyBits;
	int m_iHideHUDDisplay;
	float m_flFOV;
	int m_Teamplay;
	int m_iRes;
	Vector m_vecSkyPos;
	int m_iSkyMode;
	int m_iCameraMode;
	int m_iLastCameraMode;
	int m_iFontHeight;
	int DrawHudNumber( int x, int y, int iFlags, int iNumber, int r, int g, int b );
	int DrawHudString( int x, int y, int iMaxX, char *szString, int r, int g, int b );
	int DrawHudStringReverse( int xpos, int ypos, int iMinX, char *szString, int r, int g, int b );
	int DrawHudNumberString( int xpos, int ypos, int iMinX, int iNumber, int r, int g, int b );
	int GetNumWidth( int iNumber, int iFlags );
	int viewEntityIndex;
	int m_iHUDColor;
	int viewFlags;
private:
	// the memory for these arrays are allocated in the first call to CHud::VidInit()
	// when the hud.txt and associated sprites are loaded. freed in ~CHud()
	HSPRITE *m_rghSprites; // the sprites loaded from hud.txt
	wrect_t *m_rgrcRects;
	wrect_t nullRect;
	char *m_rgszSpriteNames;
public:
	HSPRITE GetSprite( int index ) { return (index < 0) ? 0 : m_rghSprites[index]; }
	wrect_t& GetSpriteRect( int index ) { return (index < 0) ? nullRect : m_rgrcRects[index]; }
	int InitMessages( void ); // init hud messages
	int GetSpriteIndex( const char *SpriteName );

	CHudAmmo		m_Ammo;
	CHudHealth	m_Health;
	CHudGeiger	m_Geiger;
	CHudBattery	m_Battery;
	CHudTrain		m_Train;
	CHudFlashlight	m_Flash;
	CHudRedeemer	m_Redeemer;
	CHudZoom		m_Zoom;
	CHudMessage	m_Message;
	CHudScoreboard	m_Scoreboard;
	CHudStatusBar	m_StatusBar;
	CHudDeathNotice	m_DeathNotice;
	CHudSayText	m_SayText;
	CHudMenu		m_Menu;
	CHudAmmoSecondary	m_AmmoSecondary;
	CHudTextMessage	m_TextMessage;
	CHudStatusIcons	m_StatusIcons;
          CHudSound		m_Sound;
	CHudMOTD		m_MOTD;
	
	void Init( void );
	void VidInit( void );
	void Think( void );
	int Redraw( float flTime );
	int UpdateClientData( client_data_t *cdata, float time );

	CHud() : m_pHudList(NULL) { }
	~CHud();	// destructor, frees allocated memory

	// user messages
	int _cdecl MsgFunc_Damage( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_GameMode( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_RoomType( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_ScreenFade( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_ServerName( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_ResetHUD( const char *pszName,  int iSize, void *pbuf);
	int _cdecl MsgFunc_InitHUD( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_Intermission( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_ViewMode( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_Concuss( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_ScreenShake( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_RainData( const char *pszName, int iSize, void *pbuf ); 
	int _cdecl MsgFunc_HUDColor( const char *pszName,  int iSize, void *pbuf);
	int _cdecl MsgFunc_SetFog( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_SetSky( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_CamData( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_SetBody( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_SetSkin( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_WeaponAnim( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_AddScreen( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_AddMirror( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_AddPortal( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_Particle( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_TempEntity( const char *pszName, int iSize, void *pbuf );

	// user commansds
	void _cdecl UserCmd_LoadingPlaque( void );

	// Screen information
	SCREENINFO	m_scrinfo;
		
	int	m_iWeaponBits;
	int	m_fPlayerDead;
	int	m_iIntermission;
	int	m_iDrawPlaque;

	RainData	Rain;	// buz rain

	// fog stuff
	Vector	m_FogColor;
	float	m_fStartDist;
	float	m_fEndDist;
	int	m_iFinalEndDist;
	float	m_fFadeDuration;

	// sprite indexes
	int m_HUD_number_0;

	// screen shake handler
	ScreenShake	m_Shake;

	// screen fade handler
	ScreenFade	m_FadeList[HUD_MAX_FADES];

	Vector		m_vFadeColor;
	float		m_fFadeAlpha;
	BOOL		m_bModulate;
		
	// error sprite
	int m_HUD_error;
	HSPRITE m_hHudError;
	HSPRITE m_hHudFont;

	// some const shaders
	HSPRITE m_hDefaultParticle;
	HSPRITE m_hGlowParticle;
	HSPRITE m_hDroplet;
	HSPRITE m_hBubble;
	HSPRITE m_hSparks;
	HSPRITE m_hSmoke;
	
	void AddHudElem( CHudBase *p );
	float GetSensitivity() { return m_flMouseSensitivity; }
};

extern CHud gHUD;