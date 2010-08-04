//=======================================================================
//			Copyright XashXT Group 2010 ©
//			net_encode.c - encode network messages
//=======================================================================

#include "common.h"
#include "byteorder.h"
#include "mathlib.h"
#include "net_encode.h"

#define DELTA_PATH		"delta.lst"
static bool		delta_init = false;
 
// list of all the struct names
static const delta_field_t cmd_fields[] =
{
{ UCMD_DEF(lerp_msec)	},
{ UCMD_DEF(msec)		},
{ UCMD_DEF(viewangles[0])	},
{ UCMD_DEF(viewangles[1])	},
{ UCMD_DEF(viewangles[2])	},
{ UCMD_DEF(forwardmove)	},
{ UCMD_DEF(sidemove)	},
{ UCMD_DEF(upmove)		},
{ UCMD_DEF(lightlevel)	},
{ UCMD_DEF(buttons)		},
{ UCMD_DEF(impulse)		},
{ UCMD_DEF(weaponselect)	},
{ UCMD_DEF(impact_index)	},
{ UCMD_DEF(impact_position[0])},
{ UCMD_DEF(impact_position[1])},
{ UCMD_DEF(impact_position[2])},
{ NULL },
};

static const delta_field_t ev_fields[] =
{
{ EVNT_DEF( flags )		},
{ EVNT_DEF( entindex )	},
{ EVNT_DEF( origin[0] )	},
{ EVNT_DEF( origin[1] )	},
{ EVNT_DEF( origin[2] )	},
{ EVNT_DEF( angles[0] )	},
{ EVNT_DEF( angles[1] )	},
{ EVNT_DEF( angles[2] )	},
{ EVNT_DEF( velocity[0] )	},
{ EVNT_DEF( velocity[1] )	},
{ EVNT_DEF( velocity[2] )	},
{ EVNT_DEF( ducking )	},
{ EVNT_DEF( fparam1 )	},
{ EVNT_DEF( fparam2 )	},
{ EVNT_DEF( iparam1 )	},
{ EVNT_DEF( iparam2 )	},
{ EVNT_DEF( bparam1 )	},
{ EVNT_DEF( bparam2 )	},
{ NULL },
};

static const delta_field_t cd_fields[] =
{
{ CLDT_DEF( origin[0] )	},
{ CLDT_DEF( origin[1] )	},
{ CLDT_DEF( origin[2] )	},
{ CLDT_DEF( velocity[0] )	},
{ CLDT_DEF( velocity[1] )	},
{ CLDT_DEF( velocity[2] )	},
{ CLDT_DEF( viewmodel )	},
{ CLDT_DEF( punchangle[0] )	},
{ CLDT_DEF( punchangle[1] )	},
{ CLDT_DEF( punchangle[2] )	},
{ CLDT_DEF( flags )		},
{ CLDT_DEF( waterlevel )	},
{ CLDT_DEF( watertype )	},
{ CLDT_DEF( view_ofs[0] )	},
{ CLDT_DEF( view_ofs[1] )	},
{ CLDT_DEF( view_ofs[2] )	},
{ CLDT_DEF( health )	},
{ CLDT_DEF( bInDuck )	},
{ CLDT_DEF( weapons )	},
{ CLDT_DEF( flTimeStepSound )	},
{ CLDT_DEF( flDuckTime )	},
{ CLDT_DEF( flSwimTime )	},
{ CLDT_DEF( waterjumptime )	},
{ CLDT_DEF( maxspeed )	},
{ CLDT_DEF( fov )		},
{ CLDT_DEF( weaponanim )	},
{ CLDT_DEF( m_iId )		},
{ CLDT_DEF( ammo_shells )	},
{ CLDT_DEF( ammo_nails )	},
{ CLDT_DEF( ammo_cells )	},
{ CLDT_DEF( ammo_rockets )	},
{ CLDT_DEF( m_flNextAttack )	},
{ CLDT_DEF( tfstate )	},
{ CLDT_DEF( pushmsec )	},
{ CLDT_DEF( deadflag )	},
{ CLDT_DEF( physinfo )	},
{ CLDT_DEF( iuser1 )	},
{ CLDT_DEF( iuser2 )	},
{ CLDT_DEF( iuser3 )	},
{ CLDT_DEF( iuser4 )	},
{ CLDT_DEF( fuser1 )	},
{ CLDT_DEF( fuser2 )	},
{ CLDT_DEF( fuser3 )	},
{ CLDT_DEF( fuser4 )	},
{ CLDT_DEF( vuser1[0] )	},
{ CLDT_DEF( vuser1[1] )	},
{ CLDT_DEF( vuser1[2] )	},
{ CLDT_DEF( vuser2[0] )	},
{ CLDT_DEF( vuser2[1] )	},
{ CLDT_DEF( vuser2[2] )	},
{ CLDT_DEF( vuser3[0] )	},
{ CLDT_DEF( vuser3[1] )	},
{ CLDT_DEF( vuser3[2] )	},
{ CLDT_DEF( vuser4[0] )	},
{ CLDT_DEF( vuser4[1] )	},
{ CLDT_DEF( vuser4[2] )	},
{ NULL },
};

static const delta_field_t ent_fields[] =
{
{ ENTS_DEF( classname )	},
{ ENTS_DEF( ed_flags )	},
{ ENTS_DEF( origin[0] )	},
{ ENTS_DEF( origin[1] )	},
{ ENTS_DEF( origin[2] )	},
{ ENTS_DEF( angles[0] )	},
{ ENTS_DEF( angles[1] )	},
{ ENTS_DEF( angles[2] )	},
{ ENTS_DEF( modelindex )	},
{ ENTS_DEF( sequence )	},
{ ENTS_DEF( frame )		},
{ ENTS_DEF( colormap )	},
{ ENTS_DEF( skin )		},
{ ENTS_DEF( solid )		},
{ ENTS_DEF( effects )	},
{ ENTS_DEF( scale )		},
{ ENTS_DEF( eflags )	},
{ ENTS_DEF( rendermode )	},
{ ENTS_DEF( renderamt )	},
{ ENTS_DEF( rendercolor.r )	},
{ ENTS_DEF( rendercolor.g )	},
{ ENTS_DEF( rendercolor.b )	},
{ ENTS_DEF( renderfx )	},
{ ENTS_DEF( movetype )	},
{ ENTS_DEF( animtime )	},
{ ENTS_DEF( framerate )	},
{ ENTS_DEF( body )		},
{ ENTS_DEF( controller[0] )	},
{ ENTS_DEF( controller[1] )	},
{ ENTS_DEF( controller[2] )	},
{ ENTS_DEF( controller[3] )	},
{ ENTS_DEF( blending[0] )	},
{ ENTS_DEF( blending[1] )	},
{ ENTS_DEF( blending[2] )	},
{ ENTS_DEF( blending[3] )	},
{ ENTS_DEF( velocity[0] )	},
{ ENTS_DEF( velocity[1] )	},
{ ENTS_DEF( velocity[2] )	},
{ ENTS_DEF( mins[0] )	},
{ ENTS_DEF( mins[1] )	},
{ ENTS_DEF( mins[2] )	},
{ ENTS_DEF( maxs[0] )	},
{ ENTS_DEF( maxs[1] )	},
{ ENTS_DEF( maxs[2] )	},
{ ENTS_DEF( aiment )	},
{ ENTS_DEF( owner )		},
{ ENTS_DEF( friction )	},
{ ENTS_DEF( gravity )	},
{ ENTS_DEF( team )		},
{ ENTS_DEF( playerclass )	},
{ ENTS_DEF( health )	},
{ ENTS_DEF( spectator )	},
{ ENTS_DEF( weaponmodel )	},
{ ENTS_DEF( gaitsequence )	},
{ ENTS_DEF( basevelocity[0] )	},
{ ENTS_DEF( basevelocity[1] )	},
{ ENTS_DEF( basevelocity[2] )	},
{ ENTS_DEF( usehull )	},
{ ENTS_DEF( oldbuttons )	},	// probably never transmitted
{ ENTS_DEF( onground )	},
{ ENTS_DEF( iStepLeft )	},
{ ENTS_DEF( flFallVelocity )	},
{ ENTS_DEF( fov )		},
{ ENTS_DEF( weaponanim )	},
{ ENTS_DEF( startpos[0] )	},
{ ENTS_DEF( startpos[1] )	},
{ ENTS_DEF( startpos[2] )	},
{ ENTS_DEF( endpos[0] )	},
{ ENTS_DEF( endpos[1] )	},
{ ENTS_DEF( endpos[2] )	},
{ ENTS_DEF( impacttime )	},
{ ENTS_DEF( starttime )	},
{ ENTS_DEF( iuser1 )	},
{ ENTS_DEF( iuser2 )	},
{ ENTS_DEF( iuser3 )	},
{ ENTS_DEF( iuser4 )	},
{ ENTS_DEF( fuser1 )	},
{ ENTS_DEF( fuser2 )	},
{ ENTS_DEF( fuser3 )	},
{ ENTS_DEF( fuser4 )	},
{ ENTS_DEF( vuser1[0] )	},
{ ENTS_DEF( vuser1[1] )	},
{ ENTS_DEF( vuser1[2] )	},
{ ENTS_DEF( vuser2[0] )	},
{ ENTS_DEF( vuser2[1] )	},
{ ENTS_DEF( vuser2[2] )	},
{ ENTS_DEF( vuser3[0] )	},
{ ENTS_DEF( vuser3[1] )	},
{ ENTS_DEF( vuser3[2] )	},
{ ENTS_DEF( vuser4[0] )	},
{ ENTS_DEF( vuser4[1] )	},
{ ENTS_DEF( vuser4[2] )	},

// Xash3D legacy (needs to be removed)
{ ENTS_DEF( oldorigin[0] )	},
{ ENTS_DEF( oldorigin[1] )	},
{ ENTS_DEF( oldorigin[2] )	},
{ ENTS_DEF( flags )		},
{ ENTS_DEF( viewangles[0] )	},
{ ENTS_DEF( viewangles[1] )	},
{ ENTS_DEF( viewangles[2] )	},
{ ENTS_DEF( idealpitch )	},
{ ENTS_DEF( maxspeed )	},
{ NULL },
};

static delta_info_t dt_info[] =
{
{ "event_t", ev_fields, NUM_FIELDS( ev_fields ) },
{ "usercmd_t", cmd_fields, NUM_FIELDS( cmd_fields ) },
{ "clientdata_t", cd_fields, NUM_FIELDS( cd_fields ) },
{ "entity_state_t", ent_fields, NUM_FIELDS( ent_fields ) },
{ "entity_state_player_t", ent_fields, NUM_FIELDS( ent_fields ) },
{ "custom_entity_state_t", ent_fields, NUM_FIELDS( ent_fields ) },
{ NULL },
};

delta_info_t *Delta_FindStruct( const char *name )
{
	int	i;

	if( !name || !name[0] )
		return NULL;

	for( i = 0; i < NUM_FIELDS( dt_info ); i++ )
	{
		if( !com.stricmp( dt_info[i].pName, name ))
			return &dt_info[i];
	}

	MsgDev( D_WARN, "Struct %s not found in delta_info\n", name );

	// found nothing
	return NULL;
}

delta_field_t *Delta_FindFieldInfo( const delta_field_t *pInfo, const char *fieldName )
{
	if( !fieldName || !*fieldName )
		return NULL;	

	for( ; pInfo->name; pInfo++ )
	{
		if( !com.strcmp( pInfo->name, fieldName ))
			return (delta_field_t *)pInfo;
	}
	return NULL;
}

bool Delta_AddField( const char *pStructName, const char *pName, int flags, int bits, float multiplier )
{
	delta_info_t	*dt;
	delta_field_t	*pFieldInfo;
	delta_t		*pField;
	int		i;

	// get the delta struct
	dt = Delta_FindStruct( pStructName );
	ASSERT( dt != NULL );

	// check for coexisting field
	for( i = 0, pField = dt->pFields; i < dt->numFields; i++, pField++ )
	{
		if( !com.strcmp( pField->name, pName ))
		{
			MsgDev( D_ERROR, "Delta_Add: %s->%s alread existing\n", pStructName, pName );
			return false; // field already exist		
		}
	}

	// find field description
	pFieldInfo = Delta_FindFieldInfo( dt->pInfo, pName );
	if( !pFieldInfo )
	{
		MsgDev( D_ERROR, "Delta_Add: couldn't find description for %s->%s\n", pStructName, pName );
		return false;
	}

	if( dt->numFields + 1 > dt->maxFields )
	{
		MsgDev( D_WARN, "Delta_Add: can't add %s->%s encoder list is full\n", pStructName, pName );
		return false; // too many fields specified (duplicated ?)
	}

	// allocate a new one
	dt->pFields = Z_Realloc( dt->pFields, (dt->numFields + 1) * sizeof( delta_t ));	
	for( i = 0, pField = dt->pFields; i < dt->numFields; i++, pField++ );

	// copy info to new field
	pField->name = pFieldInfo->name;
	pField->offset = pFieldInfo->offset;
	pField->flags = flags;
	pField->bits = bits;
	pField->multiplier = multiplier;
	dt->numFields++;

	return true;
}

bool Delta_ParseField( script_t *delta_script, const delta_field_t *pInfo, delta_t *pField, bool bPost )
{
	token_t		token;
	delta_field_t	*pFieldInfo;

	Com_ReadToken( delta_script, 0, &token );
	if( com.strcmp( token.string, "(" ))
	{
		MsgDev( D_ERROR, "Delta_ParseField: expected '(', found '%s' instead\n", token.string );
		return false;
	}

	// read the variable name
	if( !Com_ReadToken( delta_script, SC_ALLOW_PATHNAMES2, &token ))
	{
		MsgDev( D_ERROR, "Delta_ParseField: missing field name\n" );
		return false;
	}

	pFieldInfo = Delta_FindFieldInfo( pInfo, token.string );
	if( !pFieldInfo )
	{
		MsgDev( D_ERROR, "Delta_ParseField: unable to find field %s\n", token.string );
		return false;
	}

	Com_ReadToken( delta_script, 0, &token );
	if( com.strcmp( token.string, "," ))
	{
		MsgDev( D_ERROR, "Delta_ParseField: expected ',', found '%s' instead\n", token.string );
		return false;
	}

	// copy base info to new field
	pField->name = pFieldInfo->name;
	pField->offset = pFieldInfo->offset;
	pField->flags = 0;

	// read delta-flags
	while( Com_ReadToken( delta_script, false, &token ))
	{
		if( !com.strcmp( token.string, "," ))
			break;	// end of flags argument

		if( !com.strcmp( token.string, "|" ))
			continue;

		if( !com.strcmp( token.string, "DT_BYTE" ))
			pField->flags |= DT_BYTE;
		else if( !com.strcmp( token.string, "DT_SHORT" ))
			pField->flags |= DT_SHORT;
		else if( !com.strcmp( token.string, "DT_FLOAT" ))
			pField->flags |= DT_FLOAT;
		else if( !com.strcmp( token.string, "DT_INTEGER" ))
			pField->flags |= DT_INTEGER;
		else if( !com.strcmp( token.string, "DT_ANGLE" ))
			pField->flags |= DT_ANGLE;
		else if( !com.strncmp( token.string, "DT_TIMEWINDOW", 13 ))
			pField->flags |= DT_TIMEWINDOW;
		else if( !com.strcmp( token.string, "DT_STRING" ))
			pField->flags |= DT_STRING;
		else if( !com.strcmp( token.string, "DT_SIGNED" ))
			pField->flags |= DT_SIGNED;
	}

	if( com.strcmp( token.string, "," ))
	{
		MsgDev( D_ERROR, "Delta_ParseField: expected ',', found '%s' instead\n", token.string );
		return false;
	}

	// read delta-bits
	if( !Com_ReadLong( delta_script, false, &pField->bits ))
	{
		MsgDev( D_ERROR, "Delta_ReadField: %s field bits argument is missing\n", pField->name );
		return false;
	}

	Com_ReadToken( delta_script, 0, &token );
	if( com.strcmp( token.string, "," ))
	{
		MsgDev( D_ERROR, "Delta_ReadField: expected ',', found '%s' instead\n", token.string );
		return false;
	}

	// read delta-multiplier
	if( !Com_ReadFloat( delta_script, false, &pField->multiplier ))
	{
		MsgDev( D_ERROR, "Delta_ReadField: %s missing 'multiplier' argument\n", pField->name );
		return false;
	}

	if( bPost )
	{
		Com_ReadToken( delta_script, 0, &token );
		if( com.strcmp( token.string, "," ))
		{
			MsgDev( D_ERROR, "Delta_ReadField: expected ',', found '%s' instead\n", token.string );
			return false;
		}

		// read delta-postmultiplier
		if( !Com_ReadFloat( delta_script, false, &pField->post_multiplier ))
		{
			MsgDev( D_ERROR, "Delta_ReadField: %s missing 'post_multiply' argument\n", pField->name );
			return false;
		}
	}
	else
	{
		// to avoid division by zero
		pField->post_multiplier = 1.0f;
	}

	// closing brace...
	Com_ReadToken( delta_script, 0, &token );
	if( com.strcmp( token.string, ")" ))
	{
		MsgDev( D_ERROR, "Delta_ParseField: expected ')', found '%s' instead\n", token.string );
		return false;
	}

	// ... and trying to parse optional ',' post-symbol
	Com_ReadToken( delta_script, 0, &token );

	return true;
}

void Delta_ParseTable( script_t *delta_script, delta_info_t *dt, const char *encodeDll, const char *encodeFunc )
{
	token_t		token;
	delta_t		*pField;
	const delta_field_t	*pInfo;

	// allocate the delta-structures
	if( !dt->pFields ) dt->pFields = (delta_t *)Z_Malloc( dt->maxFields * sizeof( delta_t ));

	pField = dt->pFields;
	pInfo = dt->pInfo;
	dt->numFields = 0;

	// assume we have handled '{'
	while( Com_ReadToken( delta_script, SC_ALLOW_NEWLINES|SC_ALLOW_PATHNAMES2, &token ))
	{
		ASSERT( dt->numFields <= dt->maxFields );

		if( !com.strcmp( token.string, "DEFINE_DELTA" ))
		{
			if( Delta_ParseField( delta_script, pInfo, &pField[dt->numFields], false ))
				dt->numFields++;
			else Com_SkipRestOfLine( delta_script );
		}
		else if( !com.strcmp( token.string, "DEFINE_DELTA_POST" ))
		{
			if( Delta_ParseField( delta_script, pInfo, &pField[dt->numFields], true ))
				dt->numFields++;
			else Com_SkipRestOfLine( delta_script );
		}
		else if( token.string[0] == '}' )
		{
			// end of the section
			break;
		}
	}

	// copy function name
	com.strncpy( dt->funcName, encodeFunc, sizeof( dt->funcName ));

	if( !com.stricmp( encodeDll, "none" ))
		dt->customEncode = CUSTOM_NONE;
	else if( !com.stricmp( encodeDll, "gamedll" ))
		dt->customEncode = CUSTOM_SEREVR_ENCODE;
	else if( !com.stricmp( encodeDll, "clientdll" ))
		dt->customEncode = CUSTOM_CLIENT_ENCODE;

	// adjust to fit memory size
	if( dt->numFields < dt->maxFields )
	{
		dt->pFields = Z_Realloc( dt->pFields, dt->numFields * sizeof( delta_t ));
	}

	dt->bInitialized = true; // table is ok
}

void Delta_InitFields( void )
{
	script_t		*delta_script;
	delta_info_t	*dt;
	string		encodeDll, encodeFunc;	
	token_t		token;

	// already initialized
	if( delta_init ) return;
		
	delta_script = Com_OpenScript( DELTA_PATH, NULL, 0 );
	if( !delta_script ) com.error( "DELTA_Load: couldn't load file %s\n", DELTA_PATH );

	while( Com_ReadToken( delta_script, SC_ALLOW_NEWLINES|SC_ALLOW_PATHNAMES2, &token ))
	{
		dt = Delta_FindStruct( token.string );

		Com_ReadString( delta_script, false, encodeDll );

		if( !com.stricmp( encodeDll, "none" ))
			com.strcpy( encodeFunc, "null" );
		else Com_ReadString( delta_script, false, encodeFunc );

		// jump to '{'
		Com_ReadToken( delta_script, SC_ALLOW_NEWLINES, &token );
		ASSERT( token.string[0] == '{' );
			
		if( dt ) Delta_ParseTable( delta_script, dt, encodeDll, encodeFunc );
		else Com_SkipBracedSection( delta_script, 1 );
	}
	Com_CloseScript( delta_script );

	// adding some requrid fields fields that use may forget or don't know how to specified
	Delta_AddField( "event_t", "flags", DT_INTEGER, 8, 1.0f );

	delta_init = true;
}

void Delta_Init( void )
{
	Delta_InitFields ();	// initialize fields
}

void Delta_Shutdown( void )
{
}

/*
=====================
Delta_CompareField

compare fields by offsets
assume from and to is valid
=====================
*/
bool Delta_CompareField( delta_t *pField, void *from, void *to )
{
	int	fromF, toF;

	ASSERT( pField );
	ASSERT( from );
	ASSERT( to );

	fromF = toF = 0;

	if( pField->flags & DT_BYTE )
	{
		if( pField->flags & DT_SIGNED )
		{
			fromF = *(signed char *)((byte *)from + pField->offset );
			toF = *(signed char *)((byte *)to + pField->offset );
		}
		else
		{
			fromF = *(byte *)((byte *)from + pField->offset );
			toF = *(byte *)((byte *)to + pField->offset );
		}
	}
	else if( pField->flags & DT_SHORT )
	{
		if( pField->flags & DT_SIGNED )
		{
			fromF = *(short *)((byte *)from + pField->offset );
			toF = *(short *)((byte *)to + pField->offset );
		}
		else
		{
			fromF = *(word *)((byte *)from + pField->offset );
			toF = *(word *)((byte *)to + pField->offset );
		}
	}
	else if( pField->flags & DT_INTEGER )
	{
		if( pField->flags & DT_SIGNED )
		{
			fromF = *(int *)((byte *)from + pField->offset );
			toF = *(int *)((byte *)to + pField->offset );
		}
		else
		{
			fromF = *(uint *)((byte *)from + pField->offset );
			toF = *(uint *)((byte *)to + pField->offset );
		}
	}
	else if( pField->flags & ( DT_FLOAT|DT_ANGLE|DT_TIMEWINDOW ))
	{
		// don't convert floats to integers
		fromF = *((int *)((byte *)from + pField->offset ));
		toF = *((int *)((byte *)to + pField->offset ));
	}
	else if( pField->flags & DT_STRING )
	{
		// compare strings
		char	*s1 = (char *)((byte *)from + pField->offset );
		char	*s2 = (char *)((byte *)to + pField->offset );

		// 0 is equal, otherwise not equal
		toF = com.strcmp( s1, s2 );
	}

	return ( fromF == toF ) ? true : false;
}

/*
=====================
Delta_WriteField

write fields by offsets
assume from and to is valid
=====================
*/
bool Delta_WriteField( bitbuf_t *msg, delta_t *pField, void *from, void *to, float timebase )
{
	bool	bSigned = ( pField->flags & DT_SIGNED ) ? true : false;
	float	flValue, flAngle, flTime;
	uint	iValue;

	if( Delta_CompareField( pField, from, to ))
	{
		BF_WriteOneBit( msg, 0 );	// unchanged
		return false;
	}

	BF_WriteOneBit( msg, 1 );	// changed

	if( pField->flags & DT_BYTE )
	{
		iValue = *(byte *)((byte *)to + pField->offset );
		iValue *= pField->multiplier;
		BF_WriteBitLong( msg, iValue, pField->bits, bSigned );
	}
	else if( pField->flags & DT_SHORT )
	{
		iValue = *(word *)((byte *)to + pField->offset );
		iValue *= pField->multiplier;
		BF_WriteBitLong( msg, iValue, pField->bits, bSigned );
	}
	else if( pField->flags & DT_INTEGER )
	{
		iValue = *(uint *)((byte *)to + pField->offset );
		iValue *= pField->multiplier;
		BF_WriteBitLong( msg, iValue, pField->bits, bSigned );
	}
	else if( pField->flags & DT_FLOAT )
	{
		flValue = *(float *)((byte *)to + pField->offset );
		iValue = (int)(flValue * pField->multiplier);
		BF_WriteBitLong( msg, iValue, pField->bits, bSigned );
	}
	else if( pField->flags & DT_ANGLE )
	{
		flAngle = *(float *)((byte *)to + pField->offset );

		// NOTE: never applies multipliers to angle because
		// result may be wrong on client-side
		BF_WriteBitAngle( msg, flAngle, pField->bits );
	}
	else if( pField->flags & DT_TIMEWINDOW )
	{
		flValue = *(float *)((byte *)to + pField->offset );
		flTime = ( timebase - flValue );
		iValue = (uint)( flTime * 1000 );

		BF_WriteBitLong( msg, iValue, pField->bits, bSigned );
	}

	// FIXME: implement support for DT_STRING
	return true;
}

/*
=====================
Delta_ReadField

read fields by offsets
assume from and to is valid
=====================
*/
bool Delta_ReadField( bitbuf_t *msg, delta_t *pField, void *from, void *to, float timebase )
{
	bool	bSigned = ( pField->flags & DT_SIGNED ) ? true : false;
	float	flValue, flAngle, flTime;
	bool	bChanged;
	uint	iValue;	

	bChanged = BF_ReadOneBit( msg );

	ASSERT( pField->multiplier != 0.0f );

	if( pField->flags & DT_BYTE )
	{
		if( bChanged )
		{
			iValue = BF_ReadBitLong( msg, pField->bits, bSigned );
			iValue /= pField->multiplier;
		}
		else
		{
			iValue = *(byte *)((byte *)from + pField->offset );
		}
		*(byte *)((byte *)to + pField->offset ) = iValue;
	}
	else if( pField->flags & DT_SHORT )
	{
		if( bChanged )
		{
			iValue = BF_ReadBitLong( msg, pField->bits, bSigned );
			iValue /= pField->multiplier;
		}
		else
		{
			iValue = *(word *)((byte *)from + pField->offset );
		}
		*(word *)((byte *)to + pField->offset ) = iValue;
	}
	else if( pField->flags & DT_INTEGER )
	{
		if( bChanged )
		{
			iValue = BF_ReadBitLong( msg, pField->bits, bSigned );
			iValue /= pField->multiplier;
		}
		else
		{
			iValue = *(uint *)((byte *)from + pField->offset );
		}
		*(uint *)((byte *)to + pField->offset ) = iValue;
	}
	else if( pField->flags & DT_FLOAT )
	{
		if( bChanged )
		{
			iValue = BF_ReadBitLong( msg, pField->bits, bSigned );
			flValue = (int)iValue * ( 1.0f / pField->multiplier );
		}
		else
		{
			flValue = *(float *)((byte *)from + pField->offset );
		}
		*(float *)((byte *)to + pField->offset ) = flValue;
	}
	else if( pField->flags & DT_ANGLE )
	{
		if( bChanged )
		{
			flAngle = BF_ReadBitAngle( msg, pField->bits );
		}
		else
		{
			flAngle = *(float *)((byte *)from + pField->offset );
		}
		*(float *)((byte *)to + pField->offset ) = flAngle;
	}
	else if( pField->flags & DT_TIMEWINDOW )
	{
		if( bChanged )
		{
			iValue = BF_ReadBitLong( msg, pField->bits, bSigned );
			flValue = (float)((int)(iValue * 0.001f ));
			flTime = timebase + flValue;
		}
		else
		{
			flTime = *(float *)((byte *)from + pField->offset );
		}
		*(float *)((byte *)to + pField->offset ) = flTime;
	}

	// FIXME: implement support for DT_STRING
	// FIXME: implement post_multiplier
	return bChanged;
}

/*
=============================================================================

usercmd_t communication
  
=============================================================================
*/
/*
=====================
MSG_WriteDeltaUsercmd
=====================
*/
void MSG_WriteDeltaUsercmd( bitbuf_t *msg, usercmd_t *from, usercmd_t *to )
{
	delta_t		*pField;
	delta_info_t	*dt;
	int		i;

	dt = Delta_FindStruct( "usercmd_t" );
	ASSERT( dt && dt->bInitialized );

	pField = dt->pFields;
	ASSERT( pField );

	// process fields
	for( i = 0; i < dt->numFields; i++, pField++ )
	{
		Delta_WriteField( msg, pField, from, to, 0.0f );
	}
}

/*
=====================
MSG_ReadDeltaUsercmd
=====================
*/
void MSG_ReadDeltaUsercmd( bitbuf_t *msg, usercmd_t *from, usercmd_t *to )
{
	delta_t		*pField;
	delta_info_t	*dt;
	int		i;

	dt = Delta_FindStruct( "usercmd_t" );
	ASSERT( dt && dt->bInitialized );

	pField = dt->pFields;
	ASSERT( pField );

	*to = *from;

	// process fields
	for( i = 0; i < dt->numFields; i++, pField++ )
	{
		Delta_ReadField( msg, pField, from, to, 0.0f );
	}
}

/*
============================================================================

event_args_t communication

============================================================================
*/
/*
=====================
MSG_WriteDeltaEvent
=====================
*/
void MSG_WriteDeltaEvent( bitbuf_t *msg, event_args_t *from, event_args_t *to )
{
	delta_t		*pField;
	delta_info_t	*dt;
	int		i;

	dt = Delta_FindStruct( "event_t" );
	ASSERT( dt && dt->bInitialized );

	pField = dt->pFields;
	ASSERT( pField );

	// process fields
	for( i = 0; i < dt->numFields; i++, pField++ )
	{
		Delta_WriteField( msg, pField, from, to, 0.0f );
	}
}

/*
=====================
MSG_ReadDeltaEvent
=====================
*/
void MSG_ReadDeltaEvent( bitbuf_t *msg, event_args_t *from, event_args_t *to )
{
	delta_t		*pField;
	delta_info_t	*dt;
	int		i;

	dt = Delta_FindStruct( "event_t" );
	ASSERT( dt && dt->bInitialized );

	pField = dt->pFields;
	ASSERT( pField );

	*to = *from;

	// process fields
	for( i = 0; i < dt->numFields; i++, pField++ )
	{
		Delta_ReadField( msg, pField, from, to, 0.0f );
	}
}

/*
=============================================================================

clientdata_t communication

=============================================================================
*/
/*
==================
MSG_WriteClientData

Writes current client data only for local client
Other clients can grab the client state from entity_state_t
==================
*/
void MSG_WriteClientData( bitbuf_t *msg, clientdata_t *from, clientdata_t *to, int timebase )
{
	delta_t		*pField;
	delta_info_t	*dt;
	float		flTime = timebase * 0.001f;
	int		i;

	dt = Delta_FindStruct( "clientdata_t" );
	ASSERT( dt && dt->bInitialized );

	pField = dt->pFields;
	ASSERT( pField );

	// process fields
	for( i = 0; i < dt->numFields; i++, pField++ )
	{
		 Delta_WriteField( msg, pField, from, to, flTime );
	}
}

/*
==================
MSG_ReadClientData

Read the clientdata
==================
*/
void MSG_ReadClientData( bitbuf_t *msg, clientdata_t *from, clientdata_t *to, int timebase )
{
	delta_t		*pField;
	delta_info_t	*dt;
	float		flTime = timebase * 0.001f;
	int		i;

	dt = Delta_FindStruct( "clientdata_t" );
	ASSERT( dt && dt->bInitialized );

	pField = dt->pFields;
	ASSERT( pField );

	*to = *from;

	// process fields
	for( i = 0; i < dt->numFields; i++, pField++ )
	{
		Delta_ReadField( msg, pField, from, to, flTime );
	}
}

/*
=============================================================================

entity_state_t communication

=============================================================================
*/
/*
==================
MSG_WriteDeltaEntity

Writes part of a packetentities message, including the entity number.
Can delta from either a baseline or a previous packet_entity
If to is NULL, a remove entity update will be sent
If force is not set, then nothing at all will be generated if the entity is
identical, under the assumption that the in-order delta code will catch it.
==================
*/
void MSG_WriteDeltaEntity( entity_state_t *from, entity_state_t *to, bitbuf_t *msg, bool force, int timebase ) 
{
	delta_info_t	*dt;
	delta_t		*pField;
	int		i, startBit;
	int		numChanges = 0;
	float		flTime = timebase * 0.001f;

	if( to == NULL )
	{
		if( from == NULL ) return;

		// a NULL to is a delta remove message
		BF_WriteWord( msg, from->number );
		BF_WriteOneBit( msg, 1 );	// remove message
		return;
	}

	startBit = msg->iCurBit;

	if( to->number < 0 || to->number >= GI->max_edicts )
		Host_Error( "MSG_WriteDeltaEntity: Bad entity number: %i\n", to->number );

	BF_WriteWord( msg, to->number );
	BF_WriteOneBit( msg, 0 );	// alive

	if( to->ed_type != from->ed_type )
	{
		BF_WriteOneBit( msg, 1 );
		BF_WriteUBitLong( msg, to->ed_type, 5 );
	}
	else BF_WriteOneBit( msg, 0 ); 

	switch( to->ed_type )
	{
	case ED_CLIENT:
		dt = Delta_FindStruct( "entity_state_player_t" );
		break;
	default:
		dt = Delta_FindStruct( "entity_state_t" );
		break;
	}

	ASSERT( dt && dt->bInitialized );
		
	pField = dt->pFields;
	ASSERT( pField );

	// process fields
	for( i = 0; i < dt->numFields; i++, pField++ )
	{
		if( Delta_WriteField( msg, pField, from, to, flTime ))
			numChanges++;
	}

	// if we have no changes - kill the message
	if( !numChanges && !force ) BF_SeekToBit( msg, startBit );
}

/*
==================
MSG_ReadDeltaEntity

The entity number has already been read from the message, which
is how the from state is identified.
                             
If the delta removes the entity, entity_state_t->number will be set to MAX_EDICTS
Can go from either a baseline or a previous packet_entity
==================
*/
void MSG_ReadDeltaEntity( bitbuf_t *msg, entity_state_t *from, entity_state_t *to, int number, int timebase )
{
	delta_info_t	*dt;
	delta_t		*pField;
	float		flTime = timebase * 0.001f;
	int		i;

	if( number < 0 || number >= GI->max_edicts )
		Host_Error( "MSG_ReadDeltaEntity: bad delta entity number: %i\n", number );

	*to = *from;
	to->number = number;

	if( BF_ReadOneBit( msg ))
	{
		// check for a remove
		Mem_Set( to, 0, sizeof( *to ));	
		to->number = MAX_EDICTS;	// entity was removed
		return;
	}

	if( BF_ReadOneBit( msg ))
		to->ed_type = BF_ReadUBitLong( msg, 5 );

	switch( to->ed_type )
	{
	case ED_CLIENT:
		dt = Delta_FindStruct( "entity_state_player_t" );
		break;
	default:
		dt = Delta_FindStruct( "entity_state_t" );
		break;
	}

	ASSERT( dt && dt->bInitialized );

	pField = dt->pFields;
	ASSERT( pField );

	// process fields
	for( i = 0; i < dt->numFields; i++, pField++ )
	{
		Delta_ReadField( msg, pField, from, to, flTime );
	}
}

// UNDER CONSTRUCTION!!!

void MSG_DeltaAddEncoder( char *name, pfnDeltaEncode encodeFunc )
{
}

int MSG_DeltaFindField( delta_t *pFields, const char *fieldname )
{
	return 0;
}

void MSG_DeltaSetField( delta_t *pFields, const char *fieldname )
{
}

void MSG_DeltaUnsetField( delta_t *pFields, const char *fieldname )
{
}

void MSG_DeltaSetFieldByIndex( struct delta_s *pFields, int fieldNumber )
{
}

void MSG_DeltaUnsetFieldByIndex( struct delta_s *pFields, int fieldNumber )
{
}