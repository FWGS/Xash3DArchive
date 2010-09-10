//=======================================================================
//			Copyright XashXT Group 2007 ©
//			export.c - engine entry point
//=======================================================================

#include "common.h"

// main DLL entry point
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
	return TRUE;
}

/*
=================
Engine entry point
=================
*/
launch_exp_t DLLEXPORT *CreateAPI( stdlib_api_t *input, void *unused )
{
         	static launch_exp_t Host;

	com = *input;
	Host.api_size = sizeof( launch_exp_t );
	Host.com_size = sizeof( stdlib_api_t );

	Host.Init = Host_Init;
	Host.Main = Host_Main;
	Host.Free = Host_Free;
	Host.CPrint = Host_Print;
	Host.CmdForward = Cmd_ForwardToServer;
	Host.CmdComplete = Cmd_AutoComplete;

	return &Host;
}