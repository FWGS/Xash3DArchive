//=======================================================================
//			Copyright XashXT Group 2010 ©
//			  keydefs.h - button keycodes
//=======================================================================
#ifndef KEYDEFS_H
#define KEYDEFS_H

//
// these are the key numbers that should be passed to Key_Event
//
#define K_TAB		9
#define K_ENTER		13
#define K_ESCAPE		27
#define K_SPACE		32

// normal keys should be passed as lowercased ascii

#define K_BACKSPACE		127
#define K_UPARROW		128
#define K_DOWNARROW		129
#define K_LEFTARROW		130
#define K_RIGHTARROW	131
#define K_ALT		132
#define K_CTRL		133
#define K_SHIFT		134
#define K_F1		135
#define K_F2		136
#define K_F3		137
#define K_F4		138
#define K_F5		139
#define K_F6		140
#define K_F7		141
#define K_F8		142
#define K_F9		143
#define K_F10		144
#define K_F11		145
#define K_F12		146
#define K_INS		147
#define K_DEL		148
#define K_PGDN		149
#define K_PGUP		150
#define K_HOME		151
#define K_END		152

#define K_KP_HOME		160
#define K_KP_UPARROW	161
#define K_KP_PGUP		162
#define K_KP_LEFTARROW	163
#define K_KP_5		164
#define K_KP_RIGHTARROW	165
#define K_KP_END		166
#define K_KP_DOWNARROW	167
#define K_KP_PGDN		168
#define K_KP_ENTER		169
#define K_KP_INS   		170
#define K_KP_DEL		171
#define K_KP_SLASH		172
#define K_KP_MINUS		173
#define K_KP_PLUS		174
#define K_CAPSLOCK		175
#define K_KP_NUMLOCK	176
	
// mouse buttons generate virtual keys

#define K_MWHEELDOWN	239
#define K_MWHEELUP		240
#define K_MOUSE1		241
#define K_MOUSE2		242
#define K_MOUSE3		243
#define K_MOUSE4		244
#define K_MOUSE5		245

#define K_PAUSE		255

#endif//KEYDEFS_H