//========= Copyright ?1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include<VGUI_App.h>
#include<VGUI_SurfaceBase.h>
#include"vgui_win32.h"

using namespace vgui;

void App::platTick()
{
	::MSG msg;
	while (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		BOOL ret = ::GetMessage(&msg, NULL, 0, 0);
		if (ret == 0)
			break;

		if (ret == -1)
		{
			stop();
			break;
		}

      	::DispatchMessage(&msg);
	}
}

void App::setCursorPos(int x,int y)
{
}

void App::getCursorPos(int& x,int& y)
{
}

void App::internalSetMouseArena(int x0,int y0,int x1,int y1,bool enabled)
{
	RECT rc;
	rc.left = x0;
	rc.top = y0;
	rc.bottom = y1;
	rc.right = x1;
	ClipCursor(enabled?&rc:NULL);
}

long App::getTimeMillis()
{
	return GetTickCount();
}

void App::setClipboardText(const char* text,int textLen)
{
	if (!text)
		return;

	if (textLen <= 0)
		return;

	if (!OpenClipboard(NULL))
		return;

	EmptyClipboard();

	HANDLE hmem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, textLen + 1);
	if (hmem)
	{
		void *ptr = GlobalLock(hmem);
		if (ptr != null)
		{
			memset(ptr, 0, textLen + 1);
			memcpy(ptr, text, textLen);
			GlobalUnlock(hmem);

			SetClipboardData(CF_TEXT, hmem);
		}
	}
	
	CloseClipboard();
}

int App::getClipboardTextCount()
{
	int count = 0;
	if (!OpenClipboard(NULL))
		return 0;
	
	HANDLE hmem = GetClipboardData(CF_TEXT);
	if (hmem)
	{
		count = GlobalSize(hmem);
	}

	CloseClipboard();
	return count;
}

int App::getClipboardText(int offset,char* buf,int bufLen)
{
	if (!buf)
		return 0;

	if (bufLen <= 0)
		return 0;

	int count = 0;
	if (!OpenClipboard(NULL))
		return 0;
	
	HANDLE hmem = GetClipboardData(CF_UNICODETEXT);
	if (hmem)
	{
		int len = GlobalSize(hmem);
		count = len - offset;
		if (count <= 0)
		{
			count = 0;
		}
		else
		{
			if (bufLen < count)
			{
				count = bufLen;
			}
			void *ptr = GlobalLock(hmem);
			if (ptr)
			{
				memcpy(buf, ((char *)ptr) + offset, count);
				GlobalUnlock(hmem);
			}
		}
	}

	CloseClipboard();
	return count;
}

static bool staticSplitRegistryKey(const char *key, char *key0, int key0Len, char *key1, int key1Len)
{
	if(key==null)
	{
		return false;
	}
	
	int len=strlen(key);
	if(len<=0)
	{
		return false;
	}

	int state=0;
	int Start=-1;
	for(int i=len-1;i>=0;i--)
	{
		if(key[i]=='\\')
		{
			break;
		}
		else
		{
			Start=i;
		}
	}

	if(Start==-1)
	{
		return false;
	}
	
	vgui_strcpy(key0,Start+1,key);
	vgui_strcpy(key1,(len-Start)+1,key+Start);

	return true;
}

bool App::setRegistryString(const char* key,const char* value)
{
	HKEY hKey;

	HKEY hSlot = HKEY_CURRENT_USER;
	if (!strncmp(key, "HKEY_LOCAL_MACHINE", 18))
	{
		hSlot = HKEY_LOCAL_MACHINE;
		key += 19;
	}
	else if (!strncmp(key, "HKEY_CURRENT_USER", 17))
	{
		hSlot = HKEY_CURRENT_USER;
		key += 18;
	}

	char key0[256],key1[256];
	if(!staticSplitRegistryKey(key,key0,256,key1,256))
	{
		return false;
	}

	if(RegCreateKeyEx(hSlot,key0,null,null,REG_OPTION_NON_VOLATILE,KEY_WRITE,null,&hKey,null)!=ERROR_SUCCESS)
	{
		return false;
	}
		
	if(RegSetValueEx(hKey,key1,null,REG_SZ,(uchar*)value,strlen(value)+1)==ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return true;
	}

	RegCloseKey(hKey);

	return false;
}

bool App::getRegistryString(const char* key,char* value,int valueLen)
{
	HKEY hKey;

	HKEY hSlot = HKEY_CURRENT_USER;
	if (!strncmp(key, "HKEY_LOCAL_MACHINE", 18))
	{
		hSlot = HKEY_LOCAL_MACHINE;
		key += 19;
	}
	else if (!strncmp(key, "HKEY_CURRENT_USER", 17))
	{
		hSlot = HKEY_CURRENT_USER;
		key += 18;
	}

	char key0[256],key1[256];
	if(!staticSplitRegistryKey(key,key0,256,key1,256))
	{
		return false;
	}

	if(RegOpenKeyEx(hSlot,key0,null,KEY_READ,&hKey)!=ERROR_SUCCESS)
	{
		return false;
	}

	ulong len=valueLen;
	if(RegQueryValueEx(hKey,key1,null,null,(uchar*)value,&len)==ERROR_SUCCESS)
	{		
		RegCloseKey(hKey);
		return true;
	}

	RegCloseKey(hKey);
	return false;
}

bool App::setRegistryInteger(const char* key,int value)
{
	HKEY hKey;
	HKEY hSlot = HKEY_CURRENT_USER;
	if (!strncmp(key, "HKEY_LOCAL_MACHINE", 18))
	{
		hSlot = HKEY_LOCAL_MACHINE;
		key += 19;
	}
	else if (!strncmp(key, "HKEY_CURRENT_USER", 17))
	{
		hSlot = HKEY_CURRENT_USER;
		key += 18;
	}

	char key0[256],key1[256];
	if(!staticSplitRegistryKey(key,key0,256,key1,256))
	{
		return false;
	}

	if(RegCreateKeyEx(hSlot,key0,null,null,REG_OPTION_NON_VOLATILE,KEY_WRITE,null,&hKey,null)!=ERROR_SUCCESS)
	{
		return false;
	}
		
	if(RegSetValueEx(hKey,key1,null,REG_DWORD,(uchar*)&value,4)==ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return true;
	}

	RegCloseKey(hKey);
	return false;
}

bool App::getRegistryInteger(const char* key,int& value)
{
	HKEY hKey;
	HKEY hSlot = HKEY_CURRENT_USER;
	if (!strncmp(key, "HKEY_LOCAL_MACHINE", 18))
	{
		hSlot = HKEY_LOCAL_MACHINE;
		key += 19;
	}
	else if (!strncmp(key, "HKEY_CURRENT_USER", 17))
	{
		hSlot = HKEY_CURRENT_USER;
		key += 18;
	}

	char key0[256],key1[256];
	if(!staticSplitRegistryKey(key,key0,256,key1,256))
	{
		return false;
	}

	if(RegOpenKeyEx(hSlot,key0,null,KEY_READ,&hKey)!=ERROR_SUCCESS)
	{
		return false;
	}

	ulong len=4;
	if(RegQueryValueEx(hKey,key1,null,null,(uchar*)&value,&len)==ERROR_SUCCESS)
	{		
		RegCloseKey(hKey);
		return true;
	}

	RegCloseKey(hKey);
	return false;
}

App* App::getInstance()
{
	return _instance;
}
