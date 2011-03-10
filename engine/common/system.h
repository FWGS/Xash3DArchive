//=======================================================================
//			Copyright XashXT Group 2011 ©
//		        system.h - platform dependent code
//=======================================================================

#ifndef SYSTEM_H
#define SYSTEM_H

#include <setjmp.h>
#include <stdio.h>
#include <time.h>
#include <windows.h>

// basic typedefs
typedef int		sound_t;
typedef float		vec_t;
typedef vec_t		vec2_t[2];
typedef vec_t		vec3_t[3];
typedef vec_t		vec4_t[4];
typedef vec_t		quat_t[4];
typedef byte		rgba_t[4];	// unsigned byte colorpack
typedef byte		rgb_t[3];		// unsigned byte colorpack
typedef vec_t		matrix3x4[3][4];
typedef vec_t		matrix4x4[4][4];

#include "const.h"

/*
========================================================================

SYS EVENT

keep console cmds, network messages, mouse reletives and key buttons
========================================================================
*/
typedef enum
{
	SE_NONE = 0,	// end of events queue
	SE_KEY,		// ev.value[0] is a key code, ev.value[1] is the down flag
	SE_CHAR,		// ev.value[0] is an ascii char
	SE_CONSOLE,	// ev.data is a char*
	SE_MOUSE,		// ev.value[0] and ev.value[1] are reletive signed x / y moves
} ev_type_t;

typedef struct
{
	ev_type_t	type;
	int	value[2];
	void	*data;
	size_t	length;
} sys_event_t;

void Sys_Sleep( int msec );
double Sys_DoubleTime( void );
char *Sys_GetClipboardData( void );
char *Sys_GetCurrentUser( void );
void Sys_ShellExecute( const char *path, const char *parms, qboolean exit );
void Sys_QueEvent( ev_type_t type, int value, int value2, int length, void *ptr );
sys_event_t Sys_GetEvent( void );
qboolean Sys_CheckMMX( void );
qboolean Sys_CheckSSE( void );

#endif//SYSTEM_H