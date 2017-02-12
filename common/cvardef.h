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
#ifndef CVARDEF_H
#define CVARDEF_H

#define FCVAR_ARCHIVE		(1<<0)	// set to cause it to be saved to vars.rc
#define FCVAR_USERINFO		(1<<1)	// changes the client's info string
#define FCVAR_SERVER		(1<<2)	// notifies players when changed
#define FCVAR_EXTDLL		(1<<3)	// defined by external DLL
#define FCVAR_CLIENTDLL		(1<<4)	// defined by the client dll
#define FCVAR_PROTECTED		(1<<5)	// It's a server cvar, but we don't send the data since it's a password, etc.  Sends 1 if it's not bland/zero, 0 otherwise as value
#define FCVAR_SPONLY		(1<<6)	// This cvar cannot be changed by clients connected to a multiplayer server.
#define FCVAR_PRINTABLEONLY		(1<<7)	// This cvar's string cannot contain unprintable characters ( e.g., used for player name etc ).
#define FCVAR_UNLOGGED		(1<<8)	// If this is a FCVAR_SERVER, don't log changes to the log file / console if we are creating a log
#define FCVAR_NOEXTRAWHITEPACE	(1<<9)	// strip trailing/leading white space from this cvar

#define FCVAR_MOVEVARS		(1<<10)	// this cvar is a part of movevars_t struct that shared between client and server
#define FCVAR_LATCH			(1<<11)	// notify client what this cvar will be applied only after server restart (but don't does more nothing)
#define FCVAR_GLCONFIG		(1<<12)	// write it into opengl.cfg
#define FCVAR_CHANGED		(1<<13)	// set each time the cvar is changed
#define FCVAR_GAMEUIDLL		(1<<14)	// defined by the menu DLL
#define FCVAR_CHEAT			(1<<15)	// can not be changed if cheats are disabled
		
typedef struct cvar_s
{
	char		*name;
	char		*string;
	int		flags;
	float		value;
	struct cvar_s	*next;
} cvar_t;

#endif//CVARDEF_H