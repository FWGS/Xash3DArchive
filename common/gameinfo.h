//=======================================================================
//			Copyright XashXT Group 2010 ©
//			gameinfo.h - current game info
//=======================================================================
#ifndef GAMEINFO_H
#define GAMEINFO_H

/*
========================================================================

GAMEINFO stuff

internal shared gameinfo structure (readonly for engine parts)
========================================================================
*/
typedef struct
{
	// filesystem info
	char		gamefolder[64];	// used for change game '-game x'
	char		startmap[64];	// map to start singleplayer game
	char		trainmap[64];	// map to start hazard course (if specified)
	char		title[64];	// Game Main Title
	char		version[16];	// game version (optional)

	// about mod info
	char		game_url[256];	// link to a developer's site
	char		update_url[256];	// link to updates page
	char		type[64];		// single, toolkit, multiplayer etc
	char		date[64];
	char		size[64];		// displayed mod size

	int		gamemode;
} GAMEINFO;

#endif//GAMEINFO_H