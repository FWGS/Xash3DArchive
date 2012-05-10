/*
physint.h - Server Physics Interface
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef PHYSINT_H
#define PHYSINT_H

#define SV_PHYSICS_INTERFACE_VERSION		6

#define STRUCT_FROM_LINK( l, t, m )		((t *)((byte *)l - (int)&(((t *)0)->m)))
#define EDICT_FROM_AREA( l )			STRUCT_FROM_LINK( l, edict_t, area )

// values that can be returned with pfnServerState
#define SERVER_DEAD		0
#define SERVER_LOADING	1
#define SERVER_ACTIVE	2

typedef struct areanode_s
{
	int		axis;		// -1 = leaf node
	float		dist;
	struct areanode_s	*children[2];
	link_t		trigger_edicts;
	link_t		solid_edicts;
	link_t		water_edicts;	// func water
} areanode_t;

typedef struct server_physics_api_s
{
	// unlink edict from old position and link onto new
	void		( *pfnLinkEdict) ( edict_t *ent, qboolean touch_triggers );
	double		( *pfnGetServerTime )( void ); // unclamped
	double		( *pfnGetFrameTime )( void );	// unclamped
	void*		( *pfnGetModel)( int modelindex );
	areanode_t*	( *pfnGetHeadnode)( void ); // BSP tree for all physic entities
	int		( *pfnServerState)( void );
	void		( *pfnHost_Error)( const char *error, ... );	// cause Host Error
} server_physics_api_t;
// ONLY ADD NEW FUNCTIONS TO THE END OF THIS STRUCT.  INTERFACE VERSION IS FROZEN AT 6

// physic callbacks
typedef struct physics_interface_s
{
	int		version;
	// passed through pfnCreate (0 is attempt to create, -1 is reject)
	int		( *SV_CreateEntity	)( edict_t *pent, const char *szName );
	// run custom physics for each entity (return 0 to use built-in engine physic)
	int		( *SV_PhysicsEntity	)( edict_t *pEntity );
	// spawn entities with internal mod function e.g. for re-arrange spawn order (0 - use engine parser, 1 - use mod parser)
	int		( *SV_LoadEntities )( const char *mapname, char *entities );
	// update conveyor belt for clients
	void		( *SV_UpdatePlayerBaseVelocity )( edict_t *ent );
	// The game .dll should return 1 if save game should be allowed
	int		( *SV_AllowSaveGame )( void );
	// override trigger area checking and touching
	int		( *SV_TriggerTouch )( edict_t *pent, edict_t *trigger );
} physics_interface_t;

#endif//PHYSINT_H