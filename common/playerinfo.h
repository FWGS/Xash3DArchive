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

#ifndef PLAYERINFO_H
#define PLAYERINFO_H

#define MAX_SCOREBOARDNAME	32
#define MAX_INFO_STRING	256

typedef struct player_info_s
{
	int		userid;			// User id on server
	char		userinfo[MAX_INFO_STRING];	// User info string
	char		name[MAX_SCOREBOARDNAME];	// Name (extracted from userinfo)
	int		spectator;		// Spectator or not, unused

	int		ping;
	int		packet_loss;

	// skin information
	char		model[64];
	int		topcolor;
	int		bottomcolor;

	// last frame rendered
	int		renderframe;	

	// Gait frame estimation
	int		gaitsequence;
	float		gaitframe;
	float		gaityaw;
	vec3_t		prevgaitorigin;
} player_info_t;

#endif//PLAYERINFO_H