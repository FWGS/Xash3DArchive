//=======================================================================
//			Copyright XashXT Group 2007 ©
//			input.h - win32 input devices
//=======================================================================

#ifndef INPUT_H
#define INPUT_H

/*
==============================================================

INPUT

==============================================================
*/

#include "keydefs.h"

//
// input.c
//
void IN_Init( void );
void Host_InputFrame( void );
void IN_Shutdown( void );
void IN_MouseEvent( int mstate );
void IN_ActivateMouse( void );
void IN_DeactivateMouse( void );
long IN_WndProc( void *hWnd, uint uMsg, uint wParam, long lParam );

#endif//INPUT_H