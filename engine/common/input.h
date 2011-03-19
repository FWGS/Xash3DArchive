//=======================================================================
//			Copyright XashXT Group 2007 �
//			input.h - win32 input devices
//=======================================================================

#ifndef INPUT_H
#define INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
==============================================================

INPUT

==============================================================
*/

#include "keydefs.h"

#define WM_MOUSEWHEEL	( WM_MOUSELAST + 1 ) // message that will be supported by the OS
#define MK_XBUTTON1		0x0020
#define MK_XBUTTON2		0x0040
#define MK_XBUTTON3		0x0080
#define MK_XBUTTON4		0x0100
#define MK_XBUTTON5		0x0200
#define WM_XBUTTONUP	0x020C
#define WM_XBUTTONDOWN	0x020B

//
// input.c
//
void IN_Init( void );
void Host_InputFrame( void );
void IN_Shutdown( void );
void IN_MouseEvent( int mstate );
void IN_ActivateMouse( void );
void IN_DeactivateMouse( void );
void IN_ToggleClientMouse( int newstate, int oldstate );
long IN_WndProc( void *hWnd, uint uMsg, uint wParam, long lParam );

#ifdef __cplusplus
}
#endif
#endif//INPUT_H