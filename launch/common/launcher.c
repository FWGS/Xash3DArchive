//=======================================================================
//			Copyright XashXT Group 2007 �
//			launcher.c - main engine launcher
//=======================================================================

#include "launch.h"

/*
=================
Base Entry Point
=================
*/
DLLEXPORT int CreateAPI( char *funcname, bool console )
{
	// memeber name
	com_strncpy( Sys.progname, funcname, sizeof(Sys.progname));
	Sys.hooked_out = console; // set mode

	Sys_Init();
	Sys.Main();
	Sys_Exit();
                                                       
	return 0;
}