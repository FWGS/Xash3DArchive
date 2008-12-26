/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#include "extdll.h"
#include "utils.h"
#include "game.h"

// Register your console variables here
// This gets called one time when the game is initialied
void GameDLLInit( void )
{
	ALERT( at_console, "GameDLLInit();\n" );

	// register cvars here:
	CVAR_REGISTER( "sv_soundlist", "0", 0, "show server sound list" );

	CVAR_REGISTER( "mp_teamplay", "0", CVAR_SERVERINFO, "sets to 1 to indicate teamplay" );
	CVAR_REGISTER( "mp_fraglimit", "0", CVAR_SERVERINFO, "limit of frags for current server" );
	CVAR_REGISTER( "mp_timelimit", "0", CVAR_SERVERINFO, "server timelimit" );

	CVAR_REGISTER( "mp_fragsleft", "0", CVAR_SERVERINFO, "counter that indicated how many frags remaining" );
	CVAR_REGISTER( "mp_timeleft", "0" , CVAR_SERVERINFO, "counter that indicated how many time remaining" );

	CVAR_REGISTER( "mp_friendlyfire", "0", CVAR_SERVERINFO, "enables firedlyfire for teamplay" );
	CVAR_REGISTER( "mp_falldamage", "0", CVAR_SERVERINFO, "falldamage multiplier" );
	CVAR_REGISTER( "mp_weaponstay", "0", CVAR_SERVERINFO, "weapon leave stays on ground" );
	CVAR_REGISTER( "mp_forcerespawn", "1", CVAR_SERVERINFO, "force client respawn after his death" );
	CVAR_REGISTER( "mp_flashlight", "0", CVAR_SERVERINFO, "attempt to use flashlight in multiplayer" );
	CVAR_REGISTER( "mp_autocrosshair", "1", CVAR_SERVERINFO, "enables auto-aim in multiplayer" );
	CVAR_REGISTER( "decalfrequency", "30", CVAR_SERVERINFO, "how many decals can be spawned" );
	CVAR_REGISTER( "mp_teamlist", "hgrunt,scientist", CVAR_SERVERINFO, "names of default teams" );
	CVAR_REGISTER( "mp_teamoverride", "1", 0, "can ovveride teams from map settings ?" );
	CVAR_REGISTER( "mp_defaultteam", "0", 0, "use default team instead ?" );
	CVAR_REGISTER( "mp_chattime", "10", CVAR_SERVERINFO, "time beetween messages" );
}