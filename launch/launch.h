//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        launch.h - launch.dll main header
//=======================================================================
#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <limits.h>
#include <fcntl.h>
#include <direct.h>
#include <sys/stat.h>
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <time.h>
#include <winreg.h>
#include <math.h>

#define LAUNCH_DLL				// ignore alias names
#include "launch_api.h"

#define MAX_NUM_ARGVS		128

// just for last chanse to view message (debug only)
#define MSGBOX( x )			MessageBox(NULL, x, "Xash Error", MB_OK|MB_SETFOREGROUND|MB_ICONSTOP );

enum state_e
{
	SYS_SHUTDOWN = 0,
	SYS_RESTART,
	SYS_CRASH,
	SYS_ERROR,
	SYS_FRAME,
};

/*
========================================================================
internal dll's loader

two main types - native dlls and other win32 libraries will be recognized automatically
NOTE: never change this structure because all dll descriptions in xash code
writes into struct by offsets not names
========================================================================
*/
typedef struct { const char *name; void **func; } dllfunc_t; // Sys_LoadLibrary stuff
typedef struct dll_info_s
{
	const char	*name;	// name of library

	// generic interface
	const dllfunc_t	*fcts;	// list of dll exports	
	const char	*entry;	// entrypoint name (internal libs only)
	void		*link;	// hinstance of loading library

	// xash interface
	void		*(*main)( void*, void* );
	qboolean		crash;	// crash if dll not found

	size_t		api_size;	// interface size
	size_t		com_size;	// main interface size == sizeof( stdilib_api_t )
} dll_info_t;

typedef struct system_s
{
	char			progname[64];		// instance keyword
	char			fmessage[64];		// shutdown final message
	int			app_name;
	int			app_state;

	int			developer;

	// command line parms
	int			argc;
	char			*argv[MAX_NUM_ARGVS];

	// log stuff
	qboolean			log_active;
	char			log_path[MAX_SYSPATH];
	FILE			*logfile;

	HANDLE			hMutex;
	HINSTANCE			hInstance;
	LPTOP_LEVEL_EXCEPTION_FILTER	oldFilter;
	dll_info_t		*linked_dll;

	char			caption[64];
	qboolean			con_readonly;
	qboolean			con_showalways;
	qboolean			con_showcredits;
	qboolean			con_silentmode;
	qboolean			shutdown_issued;
	qboolean			error;

	void ( *Init ) ( int argc, char **argv );
	void ( *Main ) ( void ); // host frame
	void ( *Free ) ( void ); // close host
	void (*CPrint)( const char *msg ); // console print
	void (*CmdAuto)( char *complete_string );
	void (*Crashed)( void );
} system_t;

extern system_t Sys;
extern sysinfo_t SI;
extern stdlib_api_t	com;

//
// console.c
//
void Con_ShowConsole( qboolean show );
void Con_Print( const char *pMsg );
void Con_CreateConsole( void );
void Con_DestroyConsole( void );
void Con_RegisterHotkeys( void );
void Con_DisableInput( void );
char *Con_Input( void );

//
// system.c
//
void Sys_ParseCommandLine( LPSTR lpCmdLine );
void Sys_LookupInstance( void );
void Sys_NewInstance( const char *name, const char *fmsg );
double Sys_DoubleTime( void );
void Sys_Sleep( int msec );
void Sys_Init( void );
void Sys_Exit( void );
int Sys_CheckParm( const char *parm );
qboolean Sys_GetParmFromCmdLine( char *parm, char *out, size_t size );
qboolean Sys_LoadLibrary( const char *dll_name, dll_info_t *dll );
void* Sys_GetProcAddress ( dll_info_t *dll, const char* name );
qboolean Sys_FreeLibrary ( dll_info_t *dll );
void Sys_WaitForQuit( void );
void Sys_InitLog( void );
void Sys_CloseLog( void );
void Sys_Error(const char *error, ...);
void Sys_Break(const char *error, ...);
void Sys_PrintLog( const char *pMsg );
void Sys_Print( const char *pMsg );
void Sys_Msg( const char *pMsg, ... );
void Sys_MsgDev( int level, const char *pMsg, ... );

#define Msg Sys_Msg
#define MsgDev Sys_MsgDev

//
// stdlib.c
//
char *va( const char *format, ... );
const char* timestamp( int format );
void Com_FileBase( const char *in, char *out );

#endif//LAUNCHER_H