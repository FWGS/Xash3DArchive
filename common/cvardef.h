//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     cvardef.h - pointer to console variable
//=======================================================================
#ifndef CVARDEF_H
#define CVARDEF_H

// cvar flags
#define FCVAR_ARCHIVE	(1<<0)	// set to cause it to be saved to config.cfg
#define FCVAR_USERINFO	(1<<1)	// changes the client's info string
#define FCVAR_SERVER	(1<<2)	// changes the serevrinfo string, notifies players when changed
#define FCVAR_EXTDLL	(1<<3)	// defined by external DLL
#define FCVAR_CLIENTDLL	(1<<4)	// defined by the client dll
#define FCVAR_PROTECTED	(1<<5)	// It's a server cvar, but we don't send the data since it's a password, etc.
#define FCVAR_SPONLY	(1<<6)	// This cvar cannot be changed by clients connected to a multiplayer server.
#define FCVAR_PRINTABLEONLY	(1<<7)	// This cvar's string cannot contain unprintable characters ( player name )
#define FCVAR_UNLOGGED	(1<<8)	// If this is a FCVAR_SERVER, don't log changes to the log file / console

/*
========================================================================
console variables
external and internal cvars struct have some differences
========================================================================
*/
typedef struct cvar_s
{
	char		*name;
	char		*string;
	int		flags;
	float		value;
	struct cvar_s	*next;
} cvar_t;

#endif//CVARDEF_H