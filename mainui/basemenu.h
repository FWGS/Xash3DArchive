//=======================================================================
//			Copyright XashXT Group 2010 �
//		         basemenu.h - menu basic header 
//=======================================================================

#ifndef BASEMENU_H
#define BASEMENU_H

// engine constants
#define GAME_NORMAL			0
#define GAME_SINGLEPLAYER_ONLY	1
#define GAME_MULTIPLAYER_ONLY		2

#define KEY_CONSOLE			0
#define KEY_GAME			1
#define KEY_MENU			2

#define CS_SIZE			64	// size of one config string
#define CS_TIME			16	// size of time string

// color strings
#define ColorIndex( c )		((( c ) - '0' ) & 7 )
#define IsColorString( p )		( p && *( p ) == '^' && *(( p ) + 1) && *(( p ) + 1) >= '0' && *(( p ) + 1 ) <= '9' )

#include "netadr.h"

#define ART_BACKGROUND		"gfx/shell/splash"
#define UI_CURSOR_NORMAL		"cursor"
#define UI_CURSOR_TYPING		"typing"
#define UI_SLIDER_MAIN		"gfx/shell/slider"
#define UI_LEFTARROW		"gfx/shell/larrowdefault"
#define UI_LEFTARROWFOCUS		"gfx/shell/larrowflyover"
#define UI_LEFTARROWPRESSED		"gfx/shell/larrowpressed"
#define UI_RIGHTARROW		"gfx/shell/rarrowdefault"
#define UI_RIGHTARROWFOCUS		"gfx/shell/rarrowflyover"
#define UI_RIGHTARROWPRESSED		"gfx/shell/rarrowpressed"
#define UI_UPARROW			"gfx/shell/uparrowd"
#define UI_UPARROWFOCUS		"gfx/shell/uparrowf"
#define UI_UPARROWPRESSED		"gfx/shell/uparrowp"
#define UI_DOWNARROW		"gfx/shell/dnarrowd"
#define UI_DOWNARROWFOCUS		"gfx/shell/dnarrowf"
#define UI_DOWNARROWPRESSED		"gfx/shell/dnarrowp"
#define UI_CHECKBOX_EMPTY		"gfx/shell/cb_empty"
#define UI_CHECKBOX_GRAYED		"gfx/shell/cb_disabled"
#define UI_CHECKBOX_FOCUS		"gfx/shell/cb_over"
#define UI_CHECKBOX_PRESSED		"gfx/shell/cb_down"
#define UI_CHECKBOX_ENABLED		"gfx/shell/cb_checked"

#define UI_CURSOR_SIZE		40

#define UI_MAX_MENUDEPTH		8
#define UI_MAX_MENUITEMS		64

#define UI_PULSE_DIVISOR		75
#define UI_BLINK_TIME		250
#define UI_BLINK_MASK		499

#define UI_SMALL_CHAR_WIDTH		10
#define UI_SMALL_CHAR_HEIGHT		20
#define UI_MED_CHAR_WIDTH		18
#define UI_MED_CHAR_HEIGHT		26
#define UI_BIG_CHAR_WIDTH		20
#define UI_BIG_CHAR_HEIGHT		40

#define UI_MAX_FIELD_LINE		256
#define UI_OUTLINE_WIDTH		uiStatic.outlineWidth	// outline thickness

#define UI_MAXGAMES			100	// slots for savegame/demos
#define UI_MAX_SERVERS		32

#define MAX_HINT_TEXT		512

// menu banners used fiexed rectangle (virtual screenspace at 640x480)
#define UI_BANNER_POSX		72
#define UI_BANNER_POSY		72
#define UI_BANNER_WIDTH		736
#define UI_BANNER_HEIGHT		128

// Generic types
typedef enum
{
	QMTYPE_SCROLLLIST,
	QMTYPE_SPINCONTROL,
	QMTYPE_CHECKBOX,
	QMTYPE_SLIDER,
	QMTYPE_FIELD,
	QMTYPE_ACTION,
	QMTYPE_BITMAP
} menuType_t;

// Generic flags
#define QMF_LEFT_JUSTIFY		(1<<0)
#define QMF_CENTER_JUSTIFY		(1<<1)
#define QMF_RIGHT_JUSTIFY		(1<<2)
#define QMF_GRAYED			(1<<3)	// Grays and disables
#define QMF_INACTIVE		(1<<4)	// Disables any input
#define QMF_HIDDEN			(1<<5)	// Doesn't draw
#define QMF_NUMBERSONLY		(1<<6)	// Edit field is only numbers
#define QMF_LOWERCASE		(1<<7)	// Edit field is all lower case
#define QMF_UPPERCASE		(1<<8)	// Edit field is all upper case
#define QMF_DRAW_ADDITIVE		(1<<9)	// enable additive for this bitmap
#define QMF_PULSEIFFOCUS		(1<<10)
#define QMF_HIGHLIGHTIFFOCUS		(1<<11)
#define QMF_SMALLFONT		(1<<12)
#define QMF_BIGFONT			(1<<13)
#define QMF_DROPSHADOW		(1<<14)
#define QMF_SILENT			(1<<15)	// Don't play sounds
#define QMF_HASMOUSEFOCUS		(1<<16)
#define QMF_MOUSEONLY		(1<<17)	// Only mouse input allowed
#define QMF_FOCUSBEHIND		(1<<18)	// Focus draws behind normal item
#define QMF_NOTIFY			(1<<19)	// draw notify at right screen side
#define QMF_ACT_ONRELEASE		(1<<20)	// call Key_Event when button is released
#define QMF_ALLOW_COLORSTRINGS	(1<<21)	// allow colorstring in MENU_FIELD

// Callback notifications
#define QM_GOTFOCUS			1
#define QM_LOSTFOCUS		2
#define QM_ACTIVATED		3
#define QM_CHANGED			4
#define QM_PRESSED			5

typedef struct
{
	int		cursor;
	int		cursorPrev;

	void		*items[UI_MAX_MENUITEMS];
	int		numItems;

	void		(*drawFunc)( void );
	const char	*(*keyFunc)( int key, int down );
	void		(*activateFunc)( void );
} menuFramework_s;

typedef struct
{
	menuType_t	type;
	const char	*name;
	int		id;

	unsigned int	flags;

	int		x;
	int		y;
	int		width;
	int		height;

	int		x2;
	int		y2;
	int		width2;
	int		height2;

	int		color;
	int		focusColor;

	int		charWidth;
	int		charHeight;

	const char	*statusText;

	menuFramework_s	*parent;

	void		(*callback) (void *self, int event);
	void		(*ownerdraw) (void *self);
} menuCommon_s;

typedef struct
{
	menuCommon_s	generic;
 	const char	*background;
	const char	*upArrow;
	const char	*upArrowFocus;
	const char	*downArrow;
	const char	*downArrowFocus;
	const char	**itemNames;
	int		numItems;
	int		curItem;
	int		topItem;
	int		numRows;
} menuScrollList_s;

typedef struct
{
	menuCommon_s	generic;
	const char	*background;
	const char	*leftArrow;
	const char	*rightArrow;
	const char	*leftArrowFocus;
	const char	*rightArrowFocus;
	float		minValue;
	float		maxValue;
	float		curValue;
	float		range;
} menuSpinControl_s;

typedef struct
{
	menuCommon_s	generic;
	const char	*background;
	int		maxLength;		// can't be more than UI_MAX_FIELD_LINE
	char		buffer[UI_MAX_FIELD_LINE];
	int		widthInChars;
	int		cursor;
	int		scroll;
} menuField_s;

typedef struct
{
	menuCommon_s	generic;
	const char	*background;
} menuAction_s;

typedef struct
{
	menuCommon_s	generic;
	const char	*pic;
	const char	*focusPic;
} menuBitmap_s;

typedef struct
{
	menuCommon_s	generic;
	int		enabled;
	const char	*emptyPic;
	const char	*focusPic;	// can be replaced with pressPic manually
	const char	*checkPic;
	const char	*grayedPic;	// when QMF_GRAYED is set
} menuCheckBox_s;

typedef struct
{
	menuCommon_s	generic;
	float		minValue;
	float		maxValue;
	float		curValue;
	float		drawStep;
	int		numSteps;
	float		range;
	int		keepSlider;	// when mouse button is holds
} menuSlider_s;

void UI_ScrollList_Init( menuScrollList_s *sl );
const char *UI_ScrollList_Key( menuScrollList_s *sl, int key, int down );
void UI_ScrollList_Draw( menuScrollList_s *sl );

void UI_SpinControl_Init( menuSpinControl_s *sc );
const char *UI_SpinControl_Key( menuSpinControl_s *sc, int key, int down );
void UI_SpinControl_Draw( menuSpinControl_s *sc );

void UI_Slider_Init( menuSlider_s *sl );
const char *UI_Slider_Key( menuSlider_s *sl, int key, int down );
void UI_Slider_Draw( menuSlider_s *sl );

void UI_CheckBox_Init( menuCheckBox_s *cb );
const char *UI_CheckBox_Key( menuCheckBox_s *cb, int key, int down );
void UI_CheckBox_Draw( menuCheckBox_s *cb );

void UI_Field_Init( menuField_s *f );
const char *UI_Field_Key( menuField_s *f, int key, int down );
void UI_Field_Char( menuField_s *f, int key );
void UI_Field_Draw( menuField_s *f );

void UI_Action_Init( menuAction_s *t );
const char *UI_Action_Key( menuAction_s *t, int key, int down );
void UI_Action_Draw( menuAction_s *t );

void UI_Bitmap_Init( menuBitmap_s *b );
const char *UI_Bitmap_Key( menuBitmap_s *b, int key, int down );
void UI_Bitmap_Draw( menuBitmap_s *b );

// =====================================================================
// Main menu interface

extern cvar_t	*ui_mainfont;
extern cvar_t	*ui_namefont;
extern cvar_t	*ui_precache;
extern cvar_t	*ui_sensitivity;

typedef struct
{
	menuFramework_s	*menuActive;
	menuFramework_s	*menuStack[UI_MAX_MENUDEPTH];
	int		menuDepth;

	netadr_t		serverAddresses[UI_MAX_SERVERS];
	char		serverNames[UI_MAX_SERVERS][80];
	int		numServers;
	int		updateServers;	// true is receive new info about servers

	HIMAGE		hFont;		// mainfont

	float		scaleX;
	float		scaleY;
	int		outlineWidth;
	int		sliderWidth;

	int		cursorX;
	int		cursorY;
	int		realTime;
	int		firstDraw;
	float		enterSound;
	int		mouseInRect;
	int		hideCursor;
	int		visible;
	int		framecount;	// how many frames menu visible
	int		initialized;
} uiStatic_t;

extern uiStatic_t		uiStatic;

extern char		uiEmptyString[256];	// HACKHACK
extern const char		*uiSoundIn;
extern const char		*uiSoundOut;
extern const char		*uiSoundKey;
extern const char		*uiSoundRemoveKey;
extern const char		*uiSoundLaunch;
extern const char		*uiSoundBuzz;
extern const char		*uiSoundGlow;
extern const char		*uiSoundMove;
extern const char		*uiSoundNull;

extern int	uiColorHelp;
extern int	uiPromptBgColor;
extern int	uiPromptTextColor;
extern int	uiPromptFocusColor;
extern int	uiInputTextColor;
extern int	uiInputBgColor;
extern int	uiInputFgColor;

extern int	uiColorWhite;
extern int	uiColorDkGrey;
extern int	uiColorBlack;

void UI_ScaleCoords( int *x, int *y, int *w, int *h );
int UI_CursorInRect( int x, int y, int w, int h );
void UI_DrawPic( int x, int y, int w, int h, const int color, const char *pic );
void UI_DrawPicAdditive( int x, int y, int w, int h, const int color, const char *pic );
void UI_FillRect( int x, int y, int w, int h, const int color );
#define UI_DrawRectangle( x, y, w, h, color ) UI_DrawRectangleExt( x, y, w, h, color, uiStatic.outlineWidth )
void UI_DrawRectangleExt( int in_x, int in_y, int in_w, int in_h, const int color, int outlineWidth );
void UI_DrawString( int x, int y, int w, int h, const char *str, const int col, int forceCol, int charW, int charH, int justify, int shadow );
void UI_StartSound( const char *sound );

void UI_AddItem ( menuFramework_s *menu, void *item );
void UI_CursorMoved( menuFramework_s *menu );
void UI_SetCursor( menuFramework_s *menu, int cursor );
void UI_SetCursorToItem( menuFramework_s *menu, void *item );
void *UI_ItemAtCursor( menuFramework_s *menu );
void UI_AdjustCursor( menuFramework_s *menu, int dir );
void UI_DrawMenu( menuFramework_s *menu );
const char *UI_DefaultKey( menuFramework_s *menu, int key, int down );
const char *UI_ActivateItem( menuFramework_s *menu, menuCommon_s *item );
void UI_RefreshServerList( void );
int UI_CreditsActive( void );

void UI_CloseMenu( void );
void UI_PushMenu( menuFramework_s *menu );
void UI_PopMenu( void );

// Precache
void UI_Main_Precache( void );
void UI_NewGame_Precache( void );
void UI_LoadGame_Precache( void );
void UI_SaveGame_Precache( void );
void UI_SaveLoad_Precache( void );
void UI_MultiPlayer_Precache( void );
void UI_Options_Precache( void );
void UI_LanGame_Precache( void );
void UI_PlayerSetup_Precache( void );
void UI_Controls_Precache( void );
void UI_AdvControls_Precache( void );
void UI_GameOptions_Precache( void );
void UI_CreateGame_Precache( void );
void UI_RecDemo_Precache( void );
void UI_PlayDemo_Precache( void );
void UI_PlayRec_Precache( void );
void UI_Audio_Precache( void );
void UI_Video_Precache( void );
void UI_VidOptions_Precache( void );
void UI_VidModes_Precache( void );
void UI_CustomGame_Precache( void );
void UI_Credits_Precache( void );
void UI_GoToSite_Precache( void );

// Menus
void UI_Main_Menu( void );
void UI_NewGame_Menu( void );
void UI_LoadGame_Menu( void );
void UI_SaveGame_Menu( void );
void UI_SaveLoad_Menu( void );
void UI_MultiPlayer_Menu( void );
void UI_Options_Menu( void );
void UI_LanGame_Menu( void );
void UI_PlayerSetup_Menu( void );
void UI_Controls_Menu( void );
void UI_AdvControls_Menu( void );
void UI_GameOptions_Menu( void );
void UI_CreateGame_Menu( void );
void UI_RecDemo_Menu( void );
void UI_PlayDemo_Menu( void );
void UI_PlayRec_Menu( void );
void UI_Audio_Menu( void );
void UI_Video_Menu( void );
void UI_VidOptions_Menu( void );
void UI_VidModes_Menu( void );
void UI_CustomGame_Menu( void );
void UI_Credits_Menu( void );

//
//-----------------------------------------------------
//
class CMenu
{
public:
	// Game information
	GAMEINFO		m_gameinfo;
};

extern CMenu gMenu;

#endif // BASEMENU_H