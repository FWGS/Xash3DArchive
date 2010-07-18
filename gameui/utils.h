//=======================================================================
//			Copyright XashXT Group 2010 ©
//			     utils.h - draw helper
//=======================================================================

#ifndef UTILS_H
#define UTILS_H

extern ui_enginefuncs_t g_engfuncs;

#include "enginecallback.h"
#include "gameinfo.h"

#define FILE_GLOBAL	static
#define DLL_GLOBAL

#define MAX_INFO_STRING	512	// engine limit

//
// How did I ever live without ASSERT?
//
#ifdef _DEBUG
void DBG_AssertFunction( BOOL fExpr, const char* szExpr, const char* szFile, int szLine, const char* szMessage );
#define ASSERT( f )		DBG_AssertFunction( f, #f, __FILE__, __LINE__, NULL )
#define ASSERTSZ( f, sz )	DBG_AssertFunction( f, #f, __FILE__, __LINE__, sz )
#else
#define ASSERT( f )
#define ASSERTSZ( f, sz )
#endif

extern ui_globalvars_t		*gpGlobals;

// exports
extern int UI_VidInit( void );
extern void UI_Init( void );
extern void UI_Shutdown( void );
extern void UI_UpdateMenu( float flTime );
extern void UI_KeyEvent( int key, int down );
extern void UI_MouseMove( int x, int y );
extern void UI_SetActiveMenu( int fActive );
extern void UI_AddServerToList( netadr_t adr, const char *info );
extern void UI_GetCursorPos( int *pos_x, int *pos_y );
extern void UI_SetCursorPos( int pos_x, int pos_y );
extern void UI_ShowCursor( int show );
extern void UI_CharEvent( int key );
extern int UI_MouseInRect( void );
extern int UI_IsVisible( void );
extern int UI_CreditsActive( void );
extern void UI_FinalCredits( void );

typedef void* dllhandle_t;
typedef struct dllfunction_s
{
	const char *name;
	void **funcvariable;
} dllfunction_t;

#include "cvardef.h"

// ScreenHeight returns the virtual height of the screen, in pixels
#define ScreenHeight	(gMenu.m_scrinfo.iHeight)
// ScreenWidth returns the virtual width of the screen, in pixels
#define ScreenWidth		(gMenu.m_scrinfo.iWidth)

// ScreenHeight returns the height of the screen, in pixels
#define ActualHeight	(gMenu.m_scrinfo.iRealHeight)
// ScreenWidth returns the width of the screen, in pixels
#define ActualWidth		(gMenu.m_scrinfo.iRealWidth)

inline dword PackRGB( int r, int g, int b )
{
	return ((0xFF)<<24|(r)<<16|(g)<<8|(b));
}

inline dword PackRGBA( int r, int g, int b, int a )
{
	return ((a)<<24|(r)<<16|(g)<<8|(b));
}

inline void UnpackRGB( int &r, int &g, int &b, dword ulRGB )
{
	r = (ulRGB & 0xFF0000) >> 16;
	g = (ulRGB & 0xFF00) >> 8;
	b = (ulRGB & 0xFF) >> 0;
}

inline void UnpackRGBA( int &r, int &g, int &b, int &a, dword ulRGBA )
{
	a = (ulRGBA & 0xFF000000) >> 24;
	r = (ulRGBA & 0xFF0000) >> 16;
	g = (ulRGBA & 0xFF00) >> 8;
	b = (ulRGBA & 0xFF) >> 0;
}

inline int PackAlpha( dword ulRGB, dword ulAlpha )
{
	return (ulRGB)|(ulAlpha<<24);
}

inline int UnpackAlpha( dword ulRGBA )
{
	return ((ulRGBA & 0xFF000000) >> 24);	
}

extern int ColorStrlen( const char *str );	// returns string length without color symbols
extern const int g_iColorTable[8];
extern void COM_FileBase( const char *in, char *out );		// ripped out from hlsdk 2.3
extern void UI_FadeAlpha( int starttime, int endtime, int &color );
extern void StringConcat( char *dst, const char *src, size_t size );	// strncat safe prototype
extern char *Info_ValueForKey( const char *s, const char *key );
extern int KEY_GetKey( const char *binding );			// ripped out from engine

#endif//UTILS_H