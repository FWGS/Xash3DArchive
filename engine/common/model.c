/*
model.c - modelloader
Copyright (C) 2007 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "mod_local.h"
#include "sprite.h"
#include "mathlib.h"
#include "alias.h"
#include "studio.h"
#include "wadfile.h"
#include "world.h"
#include "gl_local.h"
#include "features.h"
#include "client.h"
#include "server.h"			// LUMP_ error codes

static model_t	*com_models[MAX_MODELS];	// shared replacement modeltable
static model_t	cm_models[MAX_MODELS];
static int	cm_nummodels = 0;
byte		*com_studiocache;		// cache for submodels
convar_t		*mod_studiocache;
convar_t		*r_wadtextures;
model_t		*loadmodel;

/*
===============================================================================

			MOD COMMON UTILS

===============================================================================
*/
/*
================
Mod_Modellist_f
================
*/
static void Mod_Modellist_f( void )
{
	int	i, nummodels;
	model_t	*mod;

	Msg( "\n" );
	Msg( "-----------------------------------\n" );

	for( i = nummodels = 0, mod = cm_models; i < cm_nummodels; i++, mod++ )
	{
		if( !mod->name[0] )
			continue; // free slot

		Msg( "%s%s\n", mod->name, (mod->type == mod_bad) ? " (DEFAULTED)" : "" );
		nummodels++;
	}

	Msg( "-----------------------------------\n" );
	Msg( "%i total models\n", nummodels );
	Msg( "\n" );
}

/*
================
Mod_FreeUserData
================
*/
static void Mod_FreeUserData( model_t *mod )
{
	// already freed?
	if( !mod || !mod->name[0] )
		return;

	if( host.type == HOST_DEDICATED )
	{
		if( svgame.physFuncs.Mod_ProcessUserData != NULL )
		{
			// let the server.dll free custom data
			svgame.physFuncs.Mod_ProcessUserData( mod, false, NULL );
		}
	}
	else
	{
		if( clgame.drawFuncs.Mod_ProcessUserData != NULL )
		{
			// let the client.dll free custom data
			clgame.drawFuncs.Mod_ProcessUserData( mod, false, NULL );
		}
	}
}

/*
================
Mod_FreeModel
================
*/
static void Mod_FreeModel( model_t *mod )
{
	// already freed?
	if( !mod || !mod->name[0] )
		return;

	Mod_FreeUserData( mod );

	// select the properly unloader
	switch( mod->type )
	{
	case mod_sprite:
		Mod_UnloadSpriteModel( mod );
		break;
	case mod_studio:
		Mod_UnloadStudioModel( mod );
		break;
	case mod_brush:
		Mod_UnloadBrushModel( mod );
		break;
	case mod_alias:
		Mod_UnloadAliasModel( mod );
		break;
	}
}

/*
===============================================================================

			MODEL INITALIZE\SHUTDOWN

===============================================================================
*/
void Mod_Init( void )
{
	com_studiocache = Mem_AllocPool( "Studio Cache" );
	mod_studiocache = Cvar_Get( "r_studiocache", "1", FCVAR_ARCHIVE, "enables studio cache for speedup tracing hitboxes" );
	r_wadtextures = Cvar_Get( "r_wadtextures", "0", 0, "completely ignore textures in the bsp-file if enabled" );

	Cmd_AddCommand( "mapstats", Mod_PrintWorldStats_f, "show stats for currently loaded map" );
	Cmd_AddCommand( "modellist", Mod_Modellist_f, "display loaded models list" );

	Mod_ResetStudioAPI ();
	Mod_InitStudioHull ();
}

void Mod_ClearAll( qboolean keep_playermodel )
{
	model_t	*plr = com_models[MAX_MODELS-1];
	model_t	*mod;
	int	i;

	for( i = 0, mod = cm_models; i < cm_nummodels; i++, mod++ )
	{
		if( keep_playermodel && mod == plr )
			continue;

		Mod_FreeModel( mod );
		memset( mod, 0, sizeof( *mod ));
	}

	// g-cont. may be just leave unchanged?
	if( !keep_playermodel ) cm_nummodels = 0;
}

void Mod_ClearUserData( void )
{
	int	i;

	for( i = 0; i < cm_nummodels; i++ )
		Mod_FreeUserData( &cm_models[i] );
}

void Mod_Shutdown( void )
{
	Mod_ClearAll( false );
	Mem_FreePool( &com_studiocache );
}

/*
===============================================================================

			MODELS MANAGEMENT

===============================================================================
*/
/*
==================
Mod_FindName

==================
*/
model_t *Mod_FindName( const char *filename, qboolean create )
{
	model_t	*mod;
	char	name[64];
	int	i;
	
	if( !filename || !filename[0] )
		return NULL;

	if( *filename == '!' ) filename++;
	Q_strncpy( name, filename, sizeof( name ));
	COM_FixSlashes( name );
		
	// search the currently loaded models
	for( i = 0, mod = cm_models; i < cm_nummodels; i++, mod++ )
	{
		if( !mod->name[0] ) continue;
		if( !Q_stricmp( mod->name, name ))
		{
			// prolonge registration
			mod->needload = world.load_sequence;
			return mod;
		}
	}

	if( !create ) return NULL;			

	// find a free model slot spot
	for( i = 0, mod = cm_models; i < cm_nummodels; i++, mod++ )
		if( !mod->name[0] ) break; // this is a valid spot

	if( i == cm_nummodels )
	{
		if( cm_nummodels == MAX_MODELS )
			Host_Error( "Mod_ForName: MAX_MODELS limit exceeded\n" );
		cm_nummodels++;
	}

	// copy name, so model loader can find model file
	Q_strncpy( mod->name, name, sizeof( mod->name ));

	return mod;
}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t *Mod_LoadModel( model_t *mod, qboolean crash )
{
	char	tempname[64];
	qboolean	loaded;
	byte	*buf;

	if( !mod )
	{
		if( crash ) Host_Error( "Mod_ForName: NULL model\n" );
		else MsgDev( D_ERROR, "Mod_ForName: NULL model\n" );
		return NULL;		
	}

	// check if already loaded (or inline bmodel)
	if( mod->mempool || mod->name[0] == '*' )
		return mod;

	// store modelname to show error
	Q_strncpy( tempname, mod->name, sizeof( tempname ));
	COM_FixSlashes( tempname );

	buf = FS_LoadFile( tempname, NULL, false );

	if( !buf )
	{
		memset( mod, 0, sizeof( model_t ));

		if( crash ) Host_Error( "Mod_ForName: %s couldn't load\n", tempname );
		else MsgDev( D_ERROR, "Mod_ForName: %s couldn't load\n", tempname );

		return NULL;
	}

	MsgDev( D_NOTE, "Mod_LoadModel: %s\n", mod->name );
	mod->needload = world.load_sequence; // register mod
	mod->type = mod_bad;
	loadmodel = mod;

	// call the apropriate loader
	switch( *(uint *)buf )
	{
	case IDSTUDIOHEADER:
		Mod_LoadStudioModel( mod, buf, &loaded );
		break;
	case IDSPRITEHEADER:
		Mod_LoadSpriteModel( mod, buf, &loaded, 0 );
		break;
	case IDALIASHEADER:
		Mod_LoadAliasModel( mod, buf, &loaded );
		break;
	case Q1BSP_VERSION:
	case HLBSP_VERSION:
	case QBSP2_VERSION:
		Mod_LoadBrushModel( mod, buf, &loaded );
		break;
	default:
		Mem_Free( buf );
		if( crash ) Host_Error( "Mod_ForName: %s unknown format\n", tempname );
		else MsgDev( D_ERROR, "Mod_ForName: %s unknown format\n", tempname );
		return NULL;
	}

	if( !loaded )
	{
		Mod_FreeModel( mod );
		Mem_Free( buf );

		if( crash ) Host_Error( "Mod_ForName: %s couldn't load\n", tempname );
		else MsgDev( D_ERROR, "Mod_ForName: %s couldn't load\n", tempname );

		return NULL;
	}
	else
	{
		if( host.type == HOST_DEDICATED )
		{
			if( svgame.physFuncs.Mod_ProcessUserData != NULL )
			{
				// let the server.dll load custom data
				svgame.physFuncs.Mod_ProcessUserData( mod, true, buf );
			}
		}
		else
		{
			if( clgame.drawFuncs.Mod_ProcessUserData != NULL )
			{
				// let the client.dll load custom data
				clgame.drawFuncs.Mod_ProcessUserData( mod, true, buf );
			}
		}
	}

	Mem_Free( buf );

	return mod;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName( const char *name, qboolean crash )
{
	model_t	*mod;
	
	mod = Mod_FindName( name, true );
	return Mod_LoadModel( mod, crash );
}

/*
==================
Mod_LoadWorld

Loads in the map and all submodels
==================
*/
void Mod_LoadWorld( const char *name, uint *checksum, qboolean multiplayer )
{
	int	i;

	// now replacement table is invalidate
	memset( com_models, 0, sizeof( com_models ));
	com_models[1] = cm_models; // make link to world

	if( !Q_stricmp( cm_models[0].name, name ))
	{
		// recalc the checksum in force-mode
		CRC32_MapFile( &world.checksum, cm_models[0].name, multiplayer );

		// singleplayer mode: server already loaded map
		if( checksum ) *checksum = world.checksum;

		// still have the right version
		return;
	}

	// clear all studio submodels on restart
	for( i = 1; i < cm_nummodels; i++ )
	{
		if( cm_models[i].type == mod_studio )
			cm_models[i].submodels = NULL;
		else if( cm_models[i].type == mod_brush )
			Mod_FreeModel( cm_models + i );
	}

	// purge all submodels
	Mod_FreeModel( &cm_models[0] );
	Mem_EmptyPool( com_studiocache );
	world.load_sequence++;	// now all models are invalid

	// load the newmap
	world.loading = true;
	Mod_ForName( name, true );
	CRC32_MapFile( &world.checksum, cm_models[0].name, multiplayer );
	world.loading = false;

	if( checksum ) *checksum = world.checksum;
}

/*
==================
Mod_FreeUnused

Purge all unused models
==================
*/
void Mod_FreeUnused( void )
{
	model_t	*mod;
	int	i;

	for( i = 0, mod = cm_models; i < cm_nummodels; i++, mod++ )
	{
		if( !mod->name[0] ) continue;
		if( mod->needload != world.load_sequence )
			Mod_FreeModel( mod );
	}
}

/*
===============================================================================

			MODEL ROUTINES

===============================================================================
*/
/*
===================
Mod_GetType
===================
*/
modtype_t Mod_GetType( int handle )
{
	model_t	*mod = Mod_Handle( handle );

	if( !mod ) return mod_bad;
	return mod->type;
}

/*
===================
Mod_GetFrames
===================
*/
void Mod_GetFrames( int handle, int *numFrames )
{
	model_t	*mod = Mod_Handle( handle );

	if( !numFrames ) return;
	if( !mod )
	{
		*numFrames = 1;
		return;
	}

	if( mod->type == mod_brush )
		*numFrames = 2; // regular and alternate animation
	else *numFrames = mod->numframes;
	if( *numFrames < 1 ) *numFrames = 1;
}

/*
===================
Mod_FrameCount

model_t as input
===================
*/
int Mod_FrameCount( model_t *mod )
{
	if( !mod ) return 1;

	switch( mod->type )
	{
	case mod_sprite:
	case mod_studio:
		return mod->numframes;
	case mod_brush:
		return 2; // regular and alternate animation
	default:
		return 1;
	}
}

/*
===================
Mod_GetBounds
===================
*/
void Mod_GetBounds( int handle, vec3_t mins, vec3_t maxs )
{
	model_t	*cmod;

	if( handle <= 0 ) return;
	cmod = Mod_Handle( handle );

	if( cmod )
	{
		if( mins ) VectorCopy( cmod->mins, mins );
		if( maxs ) VectorCopy( cmod->maxs, maxs );
	}
	else
	{
		MsgDev( D_ERROR, "Mod_GetBounds: NULL model %i\n", handle );
		if( mins ) VectorClear( mins );
		if( maxs ) VectorClear( maxs );
	}
}

/*
===============
Mod_Calloc

===============
*/
void *Mod_Calloc( int number, size_t size )
{
	cache_user_t	*cu;

	if( number <= 0 || size <= 0 ) return NULL;
	cu = (cache_user_t *)Mem_Alloc( com_studiocache, sizeof( cache_user_t ) + number * size );
	cu->data = (void *)cu; // make sure what cu->data is not NULL

	return cu;
}

/*
===============
Mod_CacheCheck

===============
*/
void *Mod_CacheCheck( cache_user_t *c )
{
	return Cache_Check( com_studiocache, c );
}

/*
===============
Mod_LoadCacheFile

===============
*/
void Mod_LoadCacheFile( const char *filename, cache_user_t *cu )
{
	byte	*buf;
	string	name;
	size_t	i, j, size;

	Assert( cu != NULL );

	if( !filename || !filename[0] ) return;

	// eliminate '!' symbol (i'm doesn't know what this doing)
	for( i = j = 0; i < Q_strlen( filename ); i++ )
	{
		if( filename[i] == '!' ) continue;
		else if( filename[i] == '\\' ) name[j] = '/';
		else name[j] = Q_tolower( filename[i] );
		j++;
	}
	name[j] = '\0';

	buf = FS_LoadFile( name, &size, false );
	if( !buf || !size ) Host_Error( "LoadCacheFile: ^1can't load %s^7\n", filename );
	cu->data = Mem_Alloc( com_studiocache, size );
	memcpy( cu->data, buf, size );
	Mem_Free( buf );
}

/*
===================
Mod_RegisterModel

register model with shared index
===================
*/
qboolean Mod_RegisterModel( const char *name, int index )
{
	model_t	*mod;

	if( index < 0 || index > MAX_MODELS )
		return false;

	// this array used for acess to servermodels
	mod = Mod_ForName( name, false );
	com_models[index] = mod;

	return ( mod != NULL );
}

/*
===============
Mod_AliasExtradata

===============
*/
void *Mod_AliasExtradata( model_t *mod )
{
	if( mod && mod->type == mod_alias )
		return mod->cache.data;
	return NULL;
}

/*
===============
Mod_StudioExtradata

===============
*/
void *Mod_StudioExtradata( model_t *mod )
{
	if( mod && mod->type == mod_studio )
		return mod->cache.data;
	return NULL;
}

/*
==================
Mod_Handle

==================
*/
model_t *Mod_Handle( int handle )
{
	if( handle < 0 || handle >= MAX_MODELS )
	{
		MsgDev( D_NOTE, "Mod_Handle: bad handle #%i\n", handle );
		return NULL;
	}
	return com_models[handle];
}