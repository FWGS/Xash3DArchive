/*
cvar.c - dynamic variable tracking
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

#include "common.h"

convar_t	*cvar_vars; // head of list
convar_t	*cmd_scripting;

/*
============
Cvar_GetListHead
============
*/
cvar_t *Cvar_GetListHead( void )
{
	return (cvar_t *)cvar_vars;
}

/*
============
Cvar_FindVar

find the specified variable by name
============
*/
convar_t *Cvar_FindVarExt( const char *var_name, int ignore_group )
{
	convar_t	*var;

	if( !var_name )
		return NULL;

	for( var = cvar_vars; var; var = var->next )
	{
		if( ignore_group && FBitSet( ignore_group, var->flags ))
			continue;

		if( !Q_stricmp( var_name, var->name ))
			return var;
	}

	return NULL;
}

/*
============
Cvar_UpdateInfo

deal with userinfo etc
============
*/
static qboolean Cvar_UpdateInfo( convar_t *var, const char *value )
{
	if( FBitSet( var->flags, FCVAR_USERINFO ))
	{
		if ( host.type == HOST_DEDICATED )
		{
			// g-cont. this is a very strange behavior...
			Info_SetValueForKey( SV_Serverinfo(), var->name, value, MAX_SERVERINFO_STRING ),
			SV_BroadcastCommand( "fullserverinfo \"%s\"\n", SV_Serverinfo( ));
		}
		else
		{
			if( !Info_SetValueForKey( CL_Userinfo(), var->name, value, MAX_INFO_STRING ))
				return false; // failed to change value

			// time to update server copy of userinfo
			CL_ServerCommand( true, "setinfo \"%s\" \"%s\"\n", var->name, value );
		}
	}

	if( FBitSet( var->flags, FCVAR_SERVER ))
	{
		if( !FBitSet( var->flags, FCVAR_UNLOGGED ))
		{
			if( FBitSet( var->flags, FCVAR_PROTECTED ))
			{
				Log_Printf( "Server cvar \"%s\" = \"%s\"\n", var->name, "***PROTECTED***" );
				SV_BroadcastPrintf( NULL, PRINT_HIGH, "\"%s\" changed to \"%s\"\n", var->name, "***PROTECTED***" );
			}
			else
			{
				Log_Printf( "Server cvar \"%s\" = \"%s\"\n", var->name, value );
				SV_BroadcastPrintf( NULL, PRINT_HIGH, "\"%s\" changed to \"%s\"\n", var->name, value );
			}
		}
	}

	return true;
}

/*
============
Cvar_ValidateString

deal with userinfo etc
============
*/
const char *Cvar_ValidateString( convar_t *var, const char *value )
{
	const char	*pszValue;
	static char	szNew[MAX_STRING];

	pszValue = value;
	szNew[0] = 0;

	// this cvar's string must only contain printable characters.
	// strip out any other crap. we'll fill in "empty" if nothing is left
	if( FBitSet( var->flags, FCVAR_PRINTABLEONLY ))
	{
		char	*szVal = szNew;
		int	len = 0;

		// step through the string, only copying back in characters that are printable
		while( *pszValue && len < MAX_STRING )
		{
			if( ((byte)*pszValue) < 32 )
			{
				pszValue++;
				continue;
			}
			*szVal++ = *pszValue++;
			len++;
		}

		*szVal = '\0';
		pszValue = szNew;

		// g-cont. is this even need?
		if( !Q_strlen( szNew )) Q_strncpy( szNew, "empty", sizeof( szNew ));
	}

	if( FBitSet( var->flags, FCVAR_NOEXTRAWHITEPACE ))
	{
		char	*szVal = szNew;
		int	len = 0;

		// step through the string, only copying back in characters that are printable
		while( *pszValue && len < MAX_STRING )
		{
			if( *pszValue == ' ' )
			{
				pszValue++;
				continue;
			}
			*szVal++ = *pszValue++;
			len++;
		}

		*szVal = '\0';
		pszValue = szNew;
	}

	return pszValue;
}

/*
============
Cvar_Changed

Tell the engine parts about cvar changing
============
*/
static void Cvar_Changed( convar_t *var )
{
	ASSERT( var != NULL );

	// tell about changes
	SetBits( var->flags, FCVAR_CHANGED );

	// tell the engine parts with global state
	if( FBitSet( var->flags, FCVAR_USERINFO ))
		host.userinfo_changed = true;

	if( FBitSet( var->flags, FCVAR_MOVEVARS ))
		host.movevars_changed = true;

	if( FBitSet( var->flags, FCVAR_RENDERINFO ))
		host.renderinfo_changed = true;

	if( !Q_strcmp( var->name, "sv_cheats" ))
		host.allow_cheats = Q_atoi( var->string );
}

/*
============
Cvar_LookupVars
============
*/
void Cvar_LookupVars( int checkbit, void *buffer, void *ptr, setpair_t callback )
{
	convar_t	*var;

	// nothing to process ?
	if( !callback ) return;

	// force checkbit to 0 for lookup all cvars
	for( var = cvar_vars; var; var = var->next )
	{
		if( checkbit && !FBitSet( var->flags, checkbit ))
			continue;

		if( buffer )
		{
			callback( var->name, var->string, buffer, ptr );
		}
		else
		{
			// NOTE: dlls cvars doesn't have description
			if( FBitSet( var->flags, FCVAR_ALLOCATED|FCVAR_EXTENDED ))
				callback( var->name, var->string, var->desc, ptr );
			else callback( var->name, var->string, "", ptr );
		}
	}
}

/*
============
Cvar_Get

If the variable already exists, the value will not be set
The flags will be or'ed in if the variable exists.
============
*/
convar_t *Cvar_Get( const char *name, const char *value, int flags, const char *var_desc )
{
	convar_t	*cur, *find, *var;
	
	ASSERT( name != NULL );

	// check for command coexisting
	if( Cmd_Exists( name ))
	{
		MsgDev( D_ERROR, "can't register variable '%s', is already defined as command\n", name );
		return NULL;
	}

	var = Cvar_FindVar( name );

	if( var )
	{
		// already existed?
		if( FBitSet( flags, FCVAR_GLCONFIG ))
		{
			// directly set value
			freestring( var->string );
			var->string = copystring( value );
			var->value = Q_atof( var->string );
			SetBits( var->flags, flags );

			// tell engine about changes
			Cvar_Changed( var );
		}
		else
		{
			SetBits( var->flags, flags );
			Cvar_DirectSet( var, value );
		}

		if( FBitSet( var->flags, FCVAR_ALLOCATED ) && Q_strcmp( var_desc, var->desc ))
		{
			if( !FBitSet( flags, FCVAR_GLCONFIG ))
				MsgDev( D_INFO, "%s change description from %s to %s\n", var->name, var->desc, var_desc );
			// update description if needs
			freestring( var->desc );
			var->desc = copystring( var_desc );
		}

		return var;
	}

	// allocate a new cvar
	var = Z_Malloc( sizeof( *var ));
	var->name = copystring( name );
	var->string = copystring( value );
	var->def_string = copystring( value );
	var->desc = copystring( var_desc );
	var->value = Q_atof( var->string );
	var->flags = flags|FCVAR_ALLOCATED;

	// link the variable in alphanumerical order
	for( cur = NULL, find = cvar_vars; find && Q_strcmp( find->name, var->name ) < 0; cur = find, find = find->next );

	if( cur ) cur->next = var;
	else cvar_vars = var;
	var->next = find;

	// fill it cls.userinfo, svs.serverinfo
	Cvar_UpdateInfo( var, var->string );

	// tell engine about changes
	Cvar_Changed( var );

	return var;
}

/*
============
Cvar_RegisterVariable

Adds a freestanding variable to the variable list.
============
*/
void Cvar_RegisterVariable( convar_t *var )
{
	convar_t	*cur, *find;

	ASSERT( var != NULL );

	// first check to see if it has allready been defined
	if( Cvar_FindVar( var->name ))
	{
		MsgDev( D_ERROR, "can't register variable '%s', is already defined\n", var->name );
		return;
	}

	// check for overlap with a command
	if( Cmd_Exists( var->name ))
	{
		MsgDev( D_ERROR, "can't register variable '%s', is already defined as command\n", var->name );
		return;
	}

	// NOTE: all the 'long' engine cvars have an special setntinel on static declaration
	// (all the engine cvars should be declared through CVAR_DEFINE macros or they shouldn't working properly anyway)
	// so we can determine long version 'convar_t' and short version 'cvar_t' more reliable than by FCVAR_EXTDLL flag
	if( CVAR_CHECK_SENTINEL( var )) SetBits( var->flags, FCVAR_EXTENDED );

	// copy the value off, because future sets will free it
	if( FBitSet( var->flags, FCVAR_EXTENDED ))
		var->def_string = var->string; // just swap pointers

	var->string = copystring( var->string );	
	var->value = Q_atof( var->string );

	// find the supposed position in chain (alphanumerical order)
	for( cur = NULL, find = cvar_vars; find && Q_strcmp( find->name, var->name ) < 0; cur = find, find = find->next );

	// now link variable	
	if( cur ) cur->next = var;
	else cvar_vars = var;
	var->next = find;

	// fill it cls.userinfo, svs.serverinfo
	Cvar_UpdateInfo( var, var->string );

	// tell engine about changes
	Cvar_Changed( var );
}

/*
============
Cvar_DirectSet

way to change value for many cvars
============
*/
void Cvar_DirectSet( convar_t *var, const char *value )
{
	const char	*pszValue;
	
	if( !var ) return;	// ???

	// lookup for registration
	if( CVAR_CHECK_SENTINEL( var ) || var->next == NULL )
	{
		// need to registering cvar fisrt
		MsgDev( D_WARN, "Cvar_DirectSet: called for unregistered cvar '%s'\n", var->name );
		Cvar_RegisterVariable( var );	// ok, register it
	}

	// lookup for registration again
	if( var != Cvar_FindVar( var->name ))
	{
		MsgDev( D_ERROR, "Cvar_DirectSet: couldn't find cvar '%s' in linked list\n", var->name );
		return;
	}

	if( FBitSet( var->flags, FCVAR_READ_ONLY|FCVAR_GLCONFIG ))
	{
		MsgDev( D_INFO, "%s is read-only.\n", var->name );
		return;
	}
	
	if( FBitSet( var->flags, FCVAR_CHEAT ) && !host.allow_cheats )
	{
		MsgDev( D_INFO, "%s is cheat protected.\n", var->name );
		return;
	}

	// just tell user about deferred changes
	if( FBitSet( var->flags, FCVAR_LATCH ) && ( SV_Active() || CL_Active( )))
		MsgDev( D_INFO, "%s will be changed upon restarting.\n", var->name );

	// check value
	if( !value )
	{
		if( !FBitSet( var->flags, FCVAR_EXTENDED ))
		{
			MsgDev( D_INFO, "%s has no default value and can't be reset.\n", var->name );
			return;
		}

		value = var->def_string; // reset to default value
	}

	pszValue = Cvar_ValidateString( var, value );

	// nothing to change
	if( !Q_strcmp( pszValue, var->string ))
		return;

	// fill it cls.userinfo, svs.serverinfo
	if( !Cvar_UpdateInfo( var, pszValue ))
		return;

	// and finally changed the cvar itself
	freestring( var->string );
	var->string = copystring( pszValue );
	var->value = Q_atof( var->string );

	// tell engine about changes
	Cvar_Changed( var );
}

/*
============
Cvar_FullSet

can set any protected cvars
============
*/
void Cvar_FullSet( const char *var_name, const char *value, int flags )
{
	convar_t	*var = Cvar_FindVar( var_name );

	if( !var )
	{
		Cvar_Get( var_name, value, flags, "" );
		return;
	}

	freestring( var->string );
	var->string = copystring( value );
	var->value = Q_atof( var->string );
	SetBits( var->flags, flags );

	// tell engine about changes
	Cvar_Changed( var );
}

/*
============
Cvar_Set
============
*/
void Cvar_Set( const char *var_name, const char *value )
{
	convar_t	*var = Cvar_FindVar( var_name );

	if( !var )
	{	// there is an error in C code if this happens
		MsgDev( D_ERROR, "Cvar_Set: variable '%s' not found\n", var_name );
		return;
	}

	Cvar_DirectSet( var, value );
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue( const char *var_name, float value )
{
	char	val[32];
	
	if( fabs( value - (int)value ) < 0.000001 )
		Q_snprintf( val, sizeof( val ), "%d", (int)value );
	else Q_snprintf( val, sizeof( val ), "%f", value );

	Cvar_Set( var_name, val );
}

/*
============
Cvar_Reset
============
*/
void Cvar_Reset( const char *var_name )
{
	Cvar_Set( var_name, NULL );
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue( const char *var_name )
{
	convar_t	*var;

	var = Cvar_FindVar( var_name );
	if( !var ) return 0.0f;

	return Q_atof( var->string );
}

/*
============
Cvar_VariableInteger
============
*/
int Cvar_VariableInteger( const char *var_name )
{
	convar_t	*var;

	var = Cvar_FindVar( var_name );
	if( !var ) return 0;

	return Q_atoi( var->string );
}

/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString( const char *var_name )
{
	convar_t	*var;

	var = Cvar_FindVar( var_name );
	if( !var ) return "";

	return var->string;
}

/*
============
Cvar_SetCheatState

Any testing variables will be reset to the safe values
============
*/
void Cvar_SetCheatState( void )
{
	convar_t	*var;

	// set all default vars to the safe value
	for( var = cvar_vars; var; var = var->next )
	{
		// can't process dll cvars - missed def_string
		if( !FBitSet( var->flags, FCVAR_ALLOCATED|FCVAR_EXTENDED ))
			continue;

		if( FBitSet( var->flags, FCVAR_CHEAT ))
		{
			if( Q_strcmp( var->def_string, var->string ))
				Cvar_DirectSet( var, var->def_string );
		}
	}
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean Cvar_Command( void )
{
	convar_t	*v;

	// check variables
	v = Cvar_FindVar( Cmd_Argv( 0 ));
	if( !v ) return false;

	// perform a variable print or set
	if( Cmd_Argc() == 1 )
	{
		if( FBitSet( v->flags, FCVAR_ALLOCATED|FCVAR_EXTENDED ))
			Msg( "\"%s\" is \"%s\" ( ^3\"%s\"^7 )\n", v->name, v->string, v->def_string );
		else Msg( "\"%s\" is \"%s\"\n", v->name, v->string );

		return true;
	}

	if( FBitSet( v->flags, FCVAR_SPONLY ) && CL_GetMaxClients() > 1 )
	{
		Msg( "can't set \"%s\" in multiplayer\n", v->name );
		return false;
	}
	else
	{
		Cvar_DirectSet( v, Cmd_Argv( 1 ));
		return true;
	}
}

/*
============
Cvar_WriteVariables

Writes lines containing "variable value" for all variables
with the specified flag set to true.
============
*/
void Cvar_WriteVariables( file_t *f, int group )
{
	convar_t	*var;
	
	for( var = cvar_vars; var; var = var->next )
	{
		if( FBitSet( var->flags, group ))
			FS_Printf( f, "%s \"%s\"\n", var->name, var->string );
	}
}

/*
============
Cvar_Toggle_f

Toggles a cvar for easy single key binding
============
*/
void Cvar_Toggle_f( void )
{
	int	v;

	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: toggle <variable>\n" );
		return;
	}

	v = !Cvar_VariableInteger( Cmd_Argv( 1 ));

	Cvar_Set( Cmd_Argv( 1 ), va( "%i", v ));
}

/*
============
Cvar_SetR_f

As Cvar_Set, but also flags it as renderinfo
============
*/
void Cvar_SetR_f( void )
{
	if( Cmd_Argc() != 3 )
	{
		Msg( "Usage: setr <variable> <value>\n" );
		return;
	}

	Cvar_Set( Cmd_Argv( 1 ), Cmd_Argv( 2 ));
}

/*
============
Cvar_SetGL_f

As Cvar_Set, but also flags it as glconfig
============
*/
void Cvar_SetGL_f( void )
{
	if( Cmd_Argc() != 3 )
	{
		Msg( "Usage: setgl <variable> <value>\n" );
		return;
	}

	Cvar_FullSet( Cmd_Argv( 1 ), Cmd_Argv( 2 ), 0 );
}

/*
============
Cvar_Reset_f
============
*/
void Cvar_Reset_f( void )
{
	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: reset <variable>\n" );
		return;
	}

	Cvar_Reset( Cmd_Argv( 1 ));
}

/*
============
Cvar_List_f
============
*/
void Cvar_List_f( void )
{
	convar_t	*var;
	char	*match = NULL;
	char	*value;
	int	count = 0;

	if( Cmd_Argc() > 1 )
		match = Cmd_Argv( 1 );

	for( var = cvar_vars; var; var = var->next )
	{
		if( var->name[0] == '@' )
			continue;	// never shows system cvars

		if( match && !Q_stricmpext( match, var->name ))
			continue;

		if( Q_colorstr( var->string ))
			value = va( "\"%s\"", var->string );
		else value = va( "\"^2%s^7\"", var->string );

		if( FBitSet( var->flags, FCVAR_EXTENDED|FCVAR_ALLOCATED ))
			Msg( " %-*s %s ^3%s^7\n", 32, var->name, value, var->desc );
		else Msg( " %-*s %s\n", 32, var->name, value );

		count++;
	}

	Msg( "\n%i cvars\n", count );
}

/*
============
Cvar_Unlink

unlink all cvars with specified flag
============
*/
void Cvar_Unlink( int group )
{
	convar_t	*var;
	convar_t	**prev;
	int	count = 0;

	if( Cvar_VariableInteger( "host_gameloaded" ) && FBitSet( group, FCVAR_EXTDLL ))
	{
		MsgDev( D_INFO, "can't unlink variables while game is loaded\n" );
		return;
	}

	if( Cvar_VariableInteger( "host_clientloaded" ) && FBitSet( group, FCVAR_CLIENTDLL ))
	{
		MsgDev( D_INFO, "can't unlink variables while client is loaded\n" );
		return;
	}

	if( Cvar_VariableInteger( "host_gameuiloaded" ) && FBitSet( group, FCVAR_GAMEUIDLL ))
	{
		MsgDev( D_INFO, "can't unlink variables while GameUI is loaded\n" );
		return;
	}

	prev = &cvar_vars;

	while( 1 )
	{
		var = *prev;
		if( !var ) break;

		// do filter by specified group
		if( group && !FBitSet( var->flags, group ))
		{
			prev = &var->next;
			continue;
		}

		// unlink variable from list
		freestring( var->string );
		*prev = var->next;

		// only allocated cvars can throw these fields
		if( FBitSet( var->flags, FCVAR_ALLOCATED ))
		{
			freestring( var->name );
			freestring( var->def_string );
			freestring( var->desc );
			Mem_Free( var );
		}
		count++;
	}

	MsgDev( D_INFO, "unlink %i cvars\n", count );
}

/*
============
Cvar_Init

Reads in all archived cvars
============
*/
void Cvar_Init( void )
{
	cvar_vars = NULL;
	cmd_scripting = Cvar_Get( "cmd_scripting", "0", FCVAR_ARCHIVE, "enable simple condition checking and variable operations" );

	Cmd_AddCommand( "setr", Cvar_SetR_f, "create or change the value of a renderinfo variable" );
	Cmd_AddCommand( "setgl", Cvar_SetGL_f, "create or change the value of a opengl variable" );
	Cmd_AddCommand( "toggle", Cvar_Toggle_f, "toggles a console variable's values (use for more info)" );
	Cmd_AddCommand( "reset", Cvar_Reset_f, "reset any type variable to initial value" );
	Cmd_AddCommand( "cvarlist", Cvar_List_f, "display all console variables beginning with the specified prefix" );
}