//=======================================================================
//			Copyright XashXT Group 2007 ©
//		     game.cpp -- executable to run Xash Engine
//=======================================================================

#include <windows.h>

// NOTE: engine has a two methods to detect basedir:
// first method: by filename, e.g. spirit.exe will be use basedir 'spirit', hostname must be "normal"
// second method: by hostname with leading symbol '$'. e.g. hl.exe with hostname '$valve' set the basedir to 'valve'
// for dedicated servers rename 'YourExeName.exe' to '#YourExeName.exe'
// Keyword "normal" it's a reserved word which switch engine to game mode, other reserved words run Xash Tools.
//
// reserved words list:
//
// "bsplib"	- launch bsp compilers
// "sprite"	- launch the spritegen
// "studio"	- launch the studiomdl
// "wadlib"	- launch the xwad
// "ripper"	- launch the extragen
//

#define GAME_PATH	"$valve"	// '$' indicates a start of basefolder name

typedef int (*pfnExecute)( const char *hostname, int console );	// engine entry point format

void Sys_Error( const char *errorstring )
{
	MessageBox( NULL, errorstring, "Xash Error", MB_OK|MB_SETFOREGROUND|MB_ICONSTOP );
	exit( 1 );
}

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	HINSTANCE	hmain;

	if(( hmain = LoadLibrary( "launch.dll" )) == NULL )
	{
		Sys_Error( "Unable to load the launch.dll" );
	}

	pfnExecute mainFunc;

	if(( mainFunc = (pfnExecute)GetProcAddress( hmain, "CreateAPI" )) == NULL )
	{
		Sys_Error( "Unable to find entry point in the launch.dll" );
	}	

	return mainFunc( GAME_PATH, false );
}