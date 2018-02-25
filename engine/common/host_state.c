/*
host_cmd.c - dedicated and normal host
Copyright (C) 2017 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"
#include "netchan.h"
#include "protocol.h"
#include "mod_local.h"
#include "mathlib.h"
#include "input.h"
#include "features.h"
#include "render_api.h"	// decallist_t

void COM_InitHostState( void )
{
	memset( GameState, 0, sizeof( game_status_t ));
	GameState->curstate = STATE_RUNFRAME;
	GameState->nextstate = STATE_RUNFRAME;
}

static void HostState_SetState( host_state_t newState, qboolean clearNext )
{
	if( clearNext )
		GameState->nextstate = newState;
	GameState->curstate = newState;
}

static void HostState_SetNextState( host_state_t nextState )
{
	ASSERT( GameState->curstate == STATE_RUNFRAME );
	GameState->nextstate = nextState;
}

void COM_NewGame( char const *pMapName )
{
	Q_strncpy( GameState->levelName, pMapName, sizeof( GameState->levelName ));
	HostState_SetNextState( STATE_LOAD_LEVEL );

	GameState->backgroundMap = false;
	GameState->landmarkName[0] = 0;
	GameState->newGame = true;
}

void COM_LoadLevel( char const *pMapName, qboolean background )
{
	Q_strncpy( GameState->levelName, pMapName, sizeof( GameState->levelName ));
	HostState_SetNextState( STATE_LOAD_LEVEL );

	GameState->backgroundMap = background;
	GameState->landmarkName[0] = 0;
	GameState->newGame = false;
}

void COM_LoadGame( char const *pMapName )
{
	Q_strncpy( GameState->levelName, pMapName, sizeof( GameState->levelName ));
	HostState_SetNextState( STATE_LOAD_GAME );
	GameState->backgroundMap = false;
	GameState->newGame = false;
	GameState->loadGame = true;
}

void COM_ChangeLevel( char const *pNewLevel, char const *pLandmarkName )
{
	Q_strncpy( GameState->levelName, pNewLevel, sizeof( GameState->levelName ));

	if( COM_CheckString( pLandmarkName ))
	{
		Q_strncpy( GameState->landmarkName, pLandmarkName, sizeof( GameState->landmarkName ));
		GameState->loadGame = true;
	}
	else
	{
		GameState->landmarkName[0] = 0;
		GameState->loadGame = false;
	}

	HostState_SetNextState( STATE_CHANGELEVEL );
	GameState->newGame = false;
}

void HostState_LoadLevel( void )
{
	if( SV_SpawnServer( GameState->levelName, NULL, GameState->backgroundMap ))
	{
		SV_LevelInit( GameState->levelName, NULL, NULL, false );
		SV_ActivateServer ();
	}

	HostState_SetState( STATE_RUNFRAME, true );
}

void HostState_LoadGame( void )
{
	if( SV_SpawnServer( GameState->levelName, NULL, false ))
	{
		SV_LevelInit( GameState->levelName, NULL, NULL, true );
		SV_ActivateServer ();
	}

	HostState_SetState( STATE_RUNFRAME, true );
}

void HostState_ChangeLevel( void )
{
	SV_ChangeLevel( GameState->loadGame, GameState->levelName, GameState->landmarkName );
	HostState_SetState( STATE_RUNFRAME, true );
}

void HostState_ShutdownGame( void )
{
	if( !GameState->loadGame )
		SV_ClearSaveDir();

	S_StopBackgroundTrack();

	if( GameState->newGame )
	{
		Host_EndGame( false, DEFAULT_ENDGAME_MESSAGE );
	}
	else
	{
		S_StopAllSounds( true );
		SV_DeactivateServer();
	}

	switch( GameState->nextstate )
	{
	case STATE_LOAD_GAME:
	case STATE_LOAD_LEVEL:
		HostState_SetState( GameState->nextstate, true );
		break;
	default:
		HostState_SetState( STATE_RUNFRAME, true );
		break;
	}
}

void HostState_Run( float time )
{
	// engine main frame
	Host_Frame( time );

	switch( GameState->nextstate )
	{
	case STATE_RUNFRAME:
		break;
	case STATE_LOAD_GAME:
	case STATE_LOAD_LEVEL:
		SCR_BeginLoadingPlaque( GameState->backgroundMap );
		// intentionally fallthrough
	case STATE_GAME_SHUTDOWN:
		HostState_SetState( STATE_GAME_SHUTDOWN, false );
		break;
	case STATE_CHANGELEVEL:
		SCR_BeginLoadingPlaque( false );
		HostState_SetState( GameState->nextstate, true );
		break;
	default:
		HostState_SetState( STATE_RUNFRAME, true );
		break;
	}
}

void COM_Frame( float time )
{
	int	loopCount = 0;

	while( 1 )
	{
		int	oldState = GameState->curstate;

		// execute the current state (and transition to the next state if not in HS_RUN)
		switch( GameState->curstate )
		{
		case STATE_LOAD_LEVEL:
			HostState_LoadLevel();
			break;
		case STATE_LOAD_GAME:
			HostState_LoadGame();
			break;
		case STATE_CHANGELEVEL:
			HostState_ChangeLevel();
			break;
		case STATE_RUNFRAME:
			HostState_Run( time );
			break;
		case STATE_GAME_SHUTDOWN:
			HostState_ShutdownGame();
			break;
		}

		if( oldState == STATE_RUNFRAME )
			break;

		if(( GameState->curstate == oldState ) || ( ++loopCount > 8 ))
			Sys_Error( "state infinity loop!\n" );
	}
}