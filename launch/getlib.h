//=======================================================================
//			Copyright (C) XashXT Group 2007
//		       getlib.c - used for find engine library
//		       getting direct path or from environment
//======================================================================= 

#ifndef GETLIB_H
#define GETLIB_H

#define NDEBUG

#include <sys\types.h>
#include <windows.h>
#include <winreg.h>
#include <stdio.h>

#pragma comment(lib, "msvcrt")

#pragma comment(linker,"/MERGE:.rdata=.text")
#pragma comment(linker,"/FILEALIGN:512 /SECTION:.text, EWRX /IGNORE:4078")

#define Run32( prog ) int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)\
{ return CreateMain32()( #prog ); }

//console format
typedef int (*winmain_t)( char *funcname );
char szSearch[ 5 ][ 1024 ];
char szFsPath[ 4096 ];
HINSTANCE	hmain;

void GetError( const char *errorstring )
{
	HINSTANCE user32_dll = LoadLibrary( "user32.dll" );
	static int (*pMessageBoxA)(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);	

	if(!errorstring) exit( 0 );

	if( user32_dll ) pMessageBoxA = (void *)GetProcAddress( user32_dll, "MessageBoxA" );
	if(pMessageBoxA) pMessageBoxA( 0, errorstring, "Error", MB_OK );
	if( user32_dll ) FreeLibrary( user32_dll ); // no need anymore...

	exit( 1 );
}

BOOL GetBin( void )
{
	//grab direct path?
	return TRUE;
}

BOOL GetEnv( void )
{
	char *pEnvPath = getenv("Xash3D");
	if(!pEnvPath) return FALSE;

	strcpy(szFsPath, pEnvPath );
	return TRUE;
}

BOOL GetReg( void )
{
	HINSTANCE advapi32_dll = LoadLibrary( "advapi32.dll" );
	static long(_stdcall *pRegOpenKeyEx)(HKEY,LPCSTR,DWORD,REGSAM,PHKEY);
	static long(_stdcall *pRegQueryValueEx)(HKEY,LPCSTR,LPDWORD,LPDWORD,LPBYTE,LPDWORD);
	static long(_stdcall *pRegCloseKey)(HKEY);
	DWORD dwBufLen = 4096; // max env length
	HKEY hKey;
	long lRet;

	if(advapi32_dll)
	{
		pRegCloseKey = (void *)GetProcAddress( advapi32_dll, "RegCloseKey" );
		pRegOpenKeyEx = (void *)GetProcAddress( advapi32_dll, "RegOpenKeyExA" );
		pRegQueryValueEx = (void *)GetProcAddress( advapi32_dll, "RegQueryValueExA" );
	}
	else goto failure; 

	if(pRegCloseKey && pRegOpenKeyEx && pRegQueryValueEx) 
	{
		lRet = pRegOpenKeyEx( HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Control\\Session Manager\\Environment", 0, KEY_READ, &hKey);
  		if(lRet != ERROR_SUCCESS) goto failure;
		lRet = pRegQueryValueEx( hKey, "Xash3D", NULL, NULL, (byte *)szFsPath, &dwBufLen);
		if(lRet != ERROR_SUCCESS) goto failure;
		pRegCloseKey( hKey );
	}
	else goto failure;

	return TRUE;
failure:
	if( advapi32_dll ) FreeLibrary( advapi32_dll ); //don't forget freeing

	return FALSE;
}

void GetLibrary ( void )
{
	int i, count = 0;
	
	if(GetEnv()) //get environment
	{
		sprintf( szSearch[count], "%s\\bin\\launch.dll", szFsPath );
		count++;
	}

	if(GetReg()) //get registry
	{
		sprintf( szSearch[count], "%s\\bin\\launch.dll", szFsPath );
		count++;
	}
	
	if(GetBin()) //get direct
	{
		strcpy( szSearch[count], "bin\\launch.dll" );
		count++;
	}

	if(GetBin()) //get direct
	{
		strcpy( szSearch[count], "launch.dll" );
		count++;
	}	
	
	for(i = 0; i < count; i++) { if(hmain = LoadLibrary( szSearch[i])) break; }
}

winmain_t CreateMain32( void )
{
	winmain_t	main;
	
	GetLibrary();
	
	if (hmain) 
	{
		main = (winmain_t)GetProcAddress( hmain, "CreateAPI" );
		if(!main) GetError("Unable to load the launch.dll" );
		return main;
	}

	GetError("Unable to load the launch.dll" );
	return 0; //make compiller happy
}

#endif//GETLIB_H 