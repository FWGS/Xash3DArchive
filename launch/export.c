//=======================================================================
//			Copyright XashXT Group 2007 �
//			export.c - main engine launcher
//=======================================================================

#include "launch.h"

// main DLL entry point
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
	return TRUE;
}

/*
=================
Main Entry Point
=================
*/
EXPORT int CreateAPI( const char *hostname, qboolean console )
{
	com_strncpy( Sys.progname, hostname, sizeof( Sys.progname ));
	Sys.hooked_out = console;

	Sys_Init();
	Sys.Main();
	Sys_Exit();
                                                       
	return 0;
}