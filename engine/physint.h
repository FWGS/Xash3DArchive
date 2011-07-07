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

#define SV_PHYSICS_INTERFACE_VERSION	1

typedef struct server_physics_api_s
{
	// unlink edict from old position and link onto new
	void		( *pfnLinkEdict) ( edict_t *ent, qboolean touch_triggers );
	double		( *pfnGetServerTime )( void ); // unclamped
} server_physics_api_t;

// physic callbacks
typedef struct physics_interface_s
{
	int		version;
	// passed through pfnCreate (0 is attempt to create, -1 is reject)
	int		( *SV_CreateEntity	)( edict_t *pent, const char *szName );
	// run custom physics for each entity (return 0 to use built-in engine physic)
	int		( *SV_PhysicsEntity	)( edict_t *pEntity );
} physics_interface_t;

#endif//PHYSINT_H