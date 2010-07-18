//=======================================================================
//			Copyright XashXT Group 2010 ©
//			 screeninfo.h - screen info
//=======================================================================
#ifndef SCREENINFO_H
#define SCREENINFO_H

#define SCRINFO_VIRTUALSPACE	1

typedef struct
{
	int	iFlags;
	int	iRealWidth;
	int	iRealHeight;
	int	iWidth;
	int	iHeight;
	int	iCharHeight;
	byte	charWidths[256];
} SCREENINFO;

// rectangle definition
typedef struct wrect_s
{
	int	left, right, top, bottom;
} wrect_t;

#endif//SCREENINFO_H