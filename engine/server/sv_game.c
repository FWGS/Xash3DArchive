//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        sv_game.c - server dlls interaction
//=======================================================================

#include "common.h"
#include "server.h"
#include "net_encode.h"
#include "event_flags.h"
#include "pm_defs.h"
#include "const.h"

// fatpvs stuff
static byte fatpvs[MAX_MAP_LEAFS/8];
static byte fatphs[MAX_MAP_LEAFS/8];
static byte *bitvector;
static int fatbytes;

// exports
typedef void (*LINK_ENTITY_FUNC)( entvars_t *pev );
typedef void (__stdcall *GIVEFNPTRSTODLL)( enginefuncs_t* engfuncs, globalvars_t *pGlobals );

/*
=============
EntvarsDescription

entavrs table for FindEntityByString
=============
*/
#define ENTVARS_COUNT	( sizeof( gEntvarsDescription ) / sizeof( gEntvarsDescription[0] ))

static TYPEDESCRIPTION gEntvarsDescription[] = 
{
	DEFINE_ENTITY_FIELD( classname, FIELD_STRING ),
	DEFINE_ENTITY_FIELD( globalname, FIELD_STRING ),
	DEFINE_ENTITY_FIELD( model, FIELD_MODELNAME ),
	DEFINE_ENTITY_FIELD( viewmodel, FIELD_MODELNAME ),
	DEFINE_ENTITY_FIELD( weaponmodel, FIELD_MODELNAME ),
	DEFINE_ENTITY_FIELD( target, FIELD_STRING ),
	DEFINE_ENTITY_FIELD( targetname, FIELD_STRING ),
	DEFINE_ENTITY_FIELD( netname, FIELD_STRING ),
	DEFINE_ENTITY_FIELD( message, FIELD_STRING ),
	DEFINE_ENTITY_FIELD( noise, FIELD_SOUNDNAME ),
	DEFINE_ENTITY_FIELD( noise1, FIELD_SOUNDNAME ),
	DEFINE_ENTITY_FIELD( noise2, FIELD_SOUNDNAME ),
	DEFINE_ENTITY_FIELD( noise3, FIELD_SOUNDNAME ),
};

/*
=============
SV_GetEntvarsDescription

entavrs table for FindEntityByString
=============
*/
TYPEDESCRIPTION *SV_GetEntvarsDescirption( int number )
{
	if( number < 0 && number >= ENTVARS_COUNT )
		return NULL;
	return &gEntvarsDescription[number];
}

void SV_SysError( const char *error_string )
{
	if( svgame.hInstance ) svgame.dllFuncs.pfnSys_Error( error_string );
}

void SV_SetMinMaxSize( edict_t *e, const float *min, const float *max )
{
	float	scale = 1.0f;
	int	i;

	if( !SV_IsValidEdict( e ) || !min || !max )
		return;

	for( i = 0; i < 3; i++ )
	{
		if( min[i] > max[i] )
		{
			MsgDev( D_ERROR, "SV_SetMinMaxSize: %s backwards mins/maxs\n", SV_ClassName( e ));
			SV_LinkEdict( e, false ); // just relink edict and exit
			return;
		}
	}
#if 0
	// FIXME: enable this when other server parts will be done and tested
	if( e->v.scale > 0.0f && e->v.scale != 1.0f )
	{
		switch( Mod_GetType( e->v.modelindex ))
		{
		case mod_sprite:
		case mod_studio:
			scale = e->v.scale;
			break;
		}
	}
#endif
	VectorScale( min, scale, e->v.mins );
	VectorScale( max, scale, e->v.maxs );
	VectorSubtract( max, min, e->v.size );

	SV_LinkEdict( e, false );
}

void SV_CopyTraceToGlobal( trace_t *trace )
{
	svgame.globals->trace_allsolid = trace->allsolid;
	svgame.globals->trace_startsolid = trace->startsolid;
	svgame.globals->trace_fraction = trace->fraction;
	svgame.globals->trace_plane_dist = trace->plane.dist;
	svgame.globals->trace_ent = trace->ent;
	svgame.globals->trace_flags = 0;
	svgame.globals->trace_inopen = trace->inopen;
	svgame.globals->trace_inwater = trace->inwater;
	VectorCopy( trace->endpos, svgame.globals->trace_endpos );
	VectorCopy( trace->plane.normal, svgame.globals->trace_plane_normal );
	svgame.globals->trace_hitgroup = trace->hitgroup;
}

void SV_SetModel( edict_t *ent, const char *name )
{
	vec3_t	mins = { 0.0f, 0.0f, 0.0f };
	vec3_t	maxs = { 0.0f, 0.0f, 0.0f };
	int	i, mod_type;

	i = SV_ModelIndex( name );
	if( i == 0 ) return;

	ent->v.model = MAKE_STRING( sv.model_precache[i] );
	ent->v.modelindex = i;

	mod_type = Mod_GetType( ent->v.modelindex );

	// studio models set to zero sizes as default
	switch( mod_type )
	{
	case mod_brush:
	case mod_sprite:
		Mod_GetBounds( ent->v.modelindex, mins, maxs );
		break;
	}

	SV_SetMinMaxSize( ent, mins, maxs );
}

float SV_AngleMod( float ideal, float current, float speed )
{
	float	move;

	if( anglemod( current ) == ideal ) // already there?
		return current; 

	move = ideal - current;
	if( ideal > current )
	{
		if( move >= 180 )
			move = move - 360;
	}
	else
	{
		if( move <= -180 )
			move = move + 360;
	}
	if( move > 0 )
	{
		if( move > speed )
			move = speed;
	}
	else
	{
		if( move < -speed )
			move = -speed;
	}
	return anglemod( current + move );
}

/*
=============
SV_ConvertTrace

convert trace_t to TraceResult
=============
*/
void SV_ConvertTrace( TraceResult *dst, trace_t *src )
{
	ASSERT( src != NULL && dst != NULL );

	dst->fAllSolid = src->allsolid;
	dst->fStartSolid = src->startsolid;
	dst->fInOpen = src->inopen;
	dst->fInWater = src->inwater;
	dst->flFraction = src->fraction;
	VectorCopy( src->endpos, dst->vecEndPos );
	dst->flPlaneDist = src->plane.dist;
	VectorCopy( src->plane.normal, dst->vecPlaneNormal );
	dst->pHit = src->ent;
	dst->iHitgroup = src->hitgroup;
}

/*
=================
SV_Send

Sends the contents of sv.multicast to a subset of the clients,
then clears sv.multicast.

MSG_ONE	send to one client (ent can't be NULL)
MSG_ALL	same as broadcast (origin can be NULL)
MSG_PVS	send to clients potentially visible from org
MSG_PHS	send to clients potentially hearable from org
=================
*/
qboolean SV_Send( int dest, const vec3_t origin, const edict_t *ent )
{
	byte		*mask = NULL;
	int		j, numclients = sv_maxclients->integer;
	sv_client_t	*cl, *current = svs.clients;
	qboolean		reliable = false;
	qboolean		specproxy = false;
	float		*viewOrg = NULL;
	int		numsends = 0;
	mleaf_t		*leaf;

	switch( dest )
	{
	case MSG_INIT:
		if( sv.state == ss_loading )
		{
			// copy signon buffer
			BF_WriteBits( &sv.signon, BF_GetData( &sv.multicast ), BF_GetNumBitsWritten( &sv.multicast ));
			BF_Clear( &sv.multicast );
			return true;
		}
		// intentional fallthrough (in-game MSG_INIT it's a MSG_ALL reliable)
	case MSG_ALL:
		reliable = true;
		// intentional fallthrough
	case MSG_BROADCAST:
		// nothing to sort	
		break;
	case MSG_PAS_R:
		reliable = true;
		// intentional fallthrough
	case MSG_PAS:
		if( origin == NULL ) return false;
		leaf = Mod_PointInLeaf( origin, sv.worldmodel->nodes );
		mask = Mod_LeafPHS( leaf, sv.worldmodel );
		break;
	case MSG_PVS_R:
		reliable = true;
		// intentional fallthrough
	case MSG_PVS:
		if( origin == NULL ) return false;
		leaf = Mod_PointInLeaf( origin, sv.worldmodel->nodes );
		mask = Mod_LeafPVS( leaf, sv.worldmodel );
		break;
	case MSG_ONE:
		reliable = true;
		// intentional fallthrough
	case MSG_ONE_UNRELIABLE:
		if( ent == NULL ) return false;
		j = NUM_FOR_EDICT( ent );
		if( j < 1 || j > numclients ) return false;
		current = svs.clients + (j - 1);
		numclients = 1; // send to one
		break;
	case MSG_SPEC:
		specproxy = reliable = true;
		break;
	default:
		Host_Error( "SV_Multicast: bad dest: %i\n", dest );
		return false;
	}

	// send the data to all relevent clients (or once only)
	for( j = 0, cl = current; j < numclients; j++, cl++ )
	{
		if( cl->state == cs_free || cl->state == cs_zombie )
			continue;

		if( cl->state != cs_spawned && !reliable )
			continue;

		if( specproxy && !( cl->edict->v.flags & FL_PROXY ))
			continue;

		if( !cl->edict || cl->fakeclient )
			continue;

		if( mask )
		{
			int	leafnum;

			if( SV_IsValidEdict( cl->pViewEntity ))
				viewOrg = cl->pViewEntity->v.origin;
			else viewOrg = cl->edict->v.origin;

			// -1 is because pvs rows are 1 based, not 0 based like leafs
			leafnum = Mod_PointLeafnum( viewOrg ) - 1;
			if( mask && (!(mask[leafnum>>3] & (1<<( leafnum & 7 )))))
				continue;
		}

		if( reliable ) BF_WriteBits( &cl->netchan.message, BF_GetData( &sv.multicast ), BF_GetNumBitsWritten( &sv.multicast ));
		else BF_WriteBits( &cl->datagram, BF_GetData( &sv.multicast ), BF_GetNumBitsWritten( &sv.multicast ));
		numsends++;
	}

	BF_Clear( &sv.multicast );

	return numsends;	// debug
}

void SV_CreateDecal( const float *origin, int decalIndex, int entityIndex, int modelIndex, int flags )
{
	if( sv.state != ss_loading ) return;

	ASSERT( origin );

	// this can happens if serialized map contain 4096 static decals...
	if(( BF_GetNumBytesWritten( &sv.signon ) + 20 ) >= BF_GetMaxBytes( &sv.signon ))
		return;

	// static decals are posters, it's always reliable
	BF_WriteByte( &sv.signon, svc_bspdecal );
	BF_WriteBitVec3Coord( &sv.signon, origin );
	BF_WriteWord( &sv.signon, decalIndex );
	BF_WriteShort( &sv.signon, entityIndex );
	if( entityIndex > 0 )
		BF_WriteWord( &sv.signon, modelIndex );
	BF_WriteByte( &sv.signon, flags );
}

static qboolean SV_OriginIn( int mode, const vec3_t v1, const vec3_t v2 )
{
	int	leafnum;
	mleaf_t	*leaf;
	byte	*mask;

	leaf = Mod_PointInLeaf( v1, sv.worldmodel->nodes );

	switch( mode )
	{
	case DVIS_PVS:
		mask = Mod_LeafPVS( leaf, sv.worldmodel );
		break;
	case DVIS_PHS:
		mask = Mod_LeafPHS( leaf, sv.worldmodel );
		break;
	default:
		// skip any checks
		return true;
	}

	// -1 is because pvs rows are 1 based, not 0 based like leafs
	leafnum = Mod_PointLeafnum( v2 ) - 1;

	if( mask && (!( mask[leafnum>>3] & (1<<( leafnum & 7 )))))
		return false;
	return true;
}

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/
static void SV_AddToFatPVS( const vec3_t org, int type, mnode_t *node )
{
	byte	*vis;
	float	d;

	while( 1 )
	{
		// if this is a leaf, accumulate the pvs bits
		if( node->contents < 0 )
		{
			if( node->contents != CONTENTS_SOLID )
			{
				mleaf_t	*leaf;
				int	i;

				leaf = (mleaf_t *)node;			

				if( type == DVIS_PVS )
					vis = Mod_LeafPVS( leaf, sv.worldmodel );
				else if( type == DVIS_PHS )
					vis = Mod_LeafPHS( leaf, sv.worldmodel );
				else vis = Mod_DecompressVis( NULL ); // get full visibility

				for( i = 0; i < fatbytes; i++ )
					bitvector[i] |= vis[i];
			}
			return;
		}
	
		d = PlaneDiff( org, node->plane );
		if( d > 8 ) node = node->children[0];
		else if( d < -8 ) node = node->children[1];
		else
		{
			// go down both
			SV_AddToFatPVS( org, type, node->children[0] );
			node = node->children[1];
		}
	}
}

/*
==============
SV_BoxInPVS

check brush boxes in fat pvs
==============
*/
static qboolean SV_BoxInPVS( const vec3_t org, const vec3_t absmin, const vec3_t absmax )
{
	mleaf_t	*leaf;
	byte	*vis;

	leaf = Mod_PointInLeaf( org, sv.worldmodel->nodes );
	vis = Mod_LeafPVS( leaf, sv.worldmodel );

	if( !Mod_BoxVisible( absmin, absmax, vis ))
		return false;
	return true;
}

void SV_WriteEntityPatch( const char *filename )
{
	file_t		*f;
	dheader_t		*header;
	int		ver = -1, lumpofs = 0, lumplen = 0;
	byte		buf[MAX_SYSPATH]; // 1 kb
	qboolean		result = false;
			
	f = FS_Open( va( "maps/%s.bsp", filename ), "rb" );
	if( !f ) return;

	Mem_Set( buf, 0, MAX_SYSPATH );
	FS_Read( f, buf, MAX_SYSPATH );
	ver = *(uint *)buf;
                              
	switch( ver )
	{
	case Q1BSP_VERSION:
	case HLBSP_VERSION:
		header = (dheader_t *)buf;
		if( header->lumps[LUMP_PLANES].filelen % sizeof( dplane_t ))
		{
			lumpofs = header->lumps[LUMP_PLANES].fileofs;
			lumplen = header->lumps[LUMP_PLANES].filelen;
		}
		else
		{
			lumpofs = header->lumps[LUMP_ENTITIES].fileofs;
			lumplen = header->lumps[LUMP_ENTITIES].filelen;
		}
		break;
	default:
		FS_Close( f );
		return;
	}

	if( lumplen >= 10 )
	{
		char	*entities = NULL;
		
		FS_Seek( f, lumpofs, SEEK_SET );
		entities = (char *)Z_Malloc( lumplen + 1 );
		FS_Read( f, entities, lumplen );
		FS_WriteFile( va( "maps/%s.ent", filename ), entities, lumplen );
		Msg( "Write 'maps/%s.ent'\n", filename );
		Mem_Free( entities );
	}
	FS_Close( f );
}

script_t *SV_GetEntityScript( const char *filename )
{
	file_t		*f;
	dheader_t		*header;
	string		entfilename;
	script_t		*ents = NULL;
	int		ver = -1, lumpofs = 0, lumplen = 0;
	byte		buf[MAX_SYSPATH]; // 1 kb
	qboolean		result = false;
			
	f = FS_Open( va( "maps/%s.bsp", filename ), "rb" );
	if( !f ) return NULL;

	Mem_Set( buf, 0, MAX_SYSPATH );
	FS_Read( f, buf, MAX_SYSPATH );
	ver = *(uint *)buf;
                              
	switch( ver )
	{
	case Q1BSP_VERSION:
	case HLBSP_VERSION:
		header = (dheader_t *)buf;
		if( header->lumps[LUMP_PLANES].filelen % sizeof( dplane_t ))
		{
			lumpofs = header->lumps[LUMP_PLANES].fileofs;
			lumplen = header->lumps[LUMP_PLANES].filelen;
		}
		else
		{
			lumpofs = header->lumps[LUMP_ENTITIES].fileofs;
			lumplen = header->lumps[LUMP_ENTITIES].filelen;
		}
		break;
	default:
		FS_Close( f );
		return NULL;
	}

	// check for entfile too
	com.strncpy( entfilename, va( "maps/%s.ent", filename ), sizeof( entfilename ));
	ents = Com_OpenScript( entfilename, NULL, 0 );

	if( !ents && lumplen >= 10 )
	{
		char	*entities = NULL;
		
		FS_Seek( f, lumpofs, SEEK_SET );
		entities = (char *)Z_Malloc( lumplen + 1 );
		FS_Read( f, entities, lumplen );
		ents = Com_OpenScript( "ents", entities, lumplen + 1 );
		Mem_Free( entities ); // no reason to keep it
	}
	FS_Close( f ); // all done

	return ents;
}

int SV_MapIsValid( const char *filename, const char *spawn_entity, const char *landmark_name )
{
	script_t	*ents = NULL;
	int	flags = 0;

	ents = SV_GetEntityScript( filename );

	if( ents )
	{
		// if there are entities to parse, a missing message key just
		// means there is no title, so clear the message string now
		token_t	token;
		string	check_name;
		qboolean	need_landmark = com.strlen( landmark_name ) > 0 ? true : false;

		if( !need_landmark && host.developer >= 2 )
		{
			// not transition, 
			Com_CloseScript( ents );

			// skip spawnpoint checks in devmode
			return (MAP_IS_EXIST|MAP_HAS_SPAWNPOINT);
		}

		flags |= MAP_IS_EXIST; // map is exist

		while( Com_ReadToken( ents, SC_ALLOW_NEWLINES|SC_ALLOW_PATHNAMES2, &token ))
		{
			if( !com.strcmp( token.string, "classname" ))
			{
				// check classname for spawn entity
				Com_ReadString( ents, SC_ALLOW_PATHNAMES2, check_name );
				if( !com.strcmp( spawn_entity, check_name ))
				{
					flags |= MAP_HAS_SPAWNPOINT;

					// we already find landmark, stop the parsing
					if( need_landmark && flags & MAP_HAS_LANDMARK )
						break;
				}
			}
			else if( need_landmark && !com.strcmp( token.string, "targetname" ))
			{
				// check targetname for landmark entity
				Com_ReadString( ents, SC_ALLOW_PATHNAMES2, check_name );

				if( !com.strcmp( landmark_name, check_name ))
				{
					flags |= MAP_HAS_LANDMARK;

					// we already find spawnpoint, stop the parsing
					if( flags & MAP_HAS_SPAWNPOINT )
						break;
				}
			}
		}
		Com_CloseScript( ents );
	}
	return flags;
}

void SV_InitEdict( edict_t *pEdict )
{
	ASSERT( pEdict );
	ASSERT( pEdict->pvPrivateData == NULL );

	pEdict->v.pContainingEntity = pEdict; // make cross-links for consistency
	pEdict->pvPrivateData = NULL;	// will be alloced later by pfnAllocPrivateData
	pEdict->serialnumber = NUM_FOR_EDICT( pEdict );
	pEdict->free = false;
}

void SV_FreeEdict( edict_t *pEdict )
{
	ASSERT( pEdict );
	ASSERT( pEdict->free == false );

	// unlink from world
	SV_UnlinkEdict( pEdict );

	// never remove global entities from map
	if( pEdict->v.globalname && sv.state == ss_active )
	{
		pEdict->v.solid = SOLID_NOT;
		pEdict->v.flags &= ~FL_KILLME;
		pEdict->v.effects = EF_NODRAW;
		pEdict->v.movetype = MOVETYPE_NONE;
		pEdict->v.modelindex = 0;
		pEdict->v.nextthink = -1;
		return;
	}

	if( pEdict->pvPrivateData )
	{
		if( svgame.dllFuncs2.pfnOnFreeEntPrivateData )
		{
			// NOTE: new interface can be missing
			svgame.dllFuncs2.pfnOnFreeEntPrivateData( pEdict );
		}
		Mem_Free( pEdict->pvPrivateData );
	}
	Mem_Set( pEdict, 0, sizeof( *pEdict ));

	// mark edict as freed
	pEdict->freetime = sv_time();
	pEdict->v.nextthink = -1;
	pEdict->free = true;
}

edict_t *SV_AllocEdict( void )
{
	edict_t	*pEdict;
	int	i;

	for( i = svgame.globals->maxClients + 1; i < svgame.numEntities; i++ )
	{
		pEdict = EDICT_NUM( i );
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if( pEdict->free && ( pEdict->freetime < 2.0 || sv_time() - pEdict->freetime > 0.5 ))
		{
			SV_InitEdict( pEdict );
			return pEdict;
		}
	}

	if( i >= svgame.globals->maxEntities )
		Host_Error( "ED_AllocEdict: no free edicts\n" );

	svgame.numEntities++;
	pEdict = EDICT_NUM( i );
	SV_InitEdict( pEdict );

	return pEdict;
}

edict_t* SV_AllocPrivateData( edict_t *ent, string_t className )
{
	const char	*pszClassName;
	LINK_ENTITY_FUNC	SpawnEdict;

	pszClassName = STRING( className );

	if( !ent )
	{
		// allocate new one
		ent = SV_AllocEdict();
	}
	else if( ent->free )
	{
		SV_InitEdict( ent ); // re-init edict
		MsgDev( D_WARN, "SV_AllocPrivateData: entity %s is freed!\n", STRING( className ));
	}

	ent->v.classname = className;
	ent->v.pContainingEntity = ent; // re-link

	VectorSet( ent->v.rendercolor, 255, 255, 255 ); // assume default color
	
	// allocate edict private memory (passed by dlls)
	SpawnEdict = (LINK_ENTITY_FUNC)FS_GetProcAddress( svgame.hInstance, pszClassName );
	if( !SpawnEdict )
	{
		// attempt to create custom entity
		if( svgame.dllFuncs2.pfnCreate && svgame.dllFuncs2.pfnCreate( ent, pszClassName ) != -1 )
			return ent;

		ent->v.flags |= FL_KILLME;
		MsgDev( D_ERROR, "No spawn function for %s\n", STRING( className ));
		return ent; // this edict will be removed from map
	}
	else SpawnEdict( &ent->v );

	return ent;
}

void SV_FreeEdicts( void )
{
	int	i = 0;
	edict_t	*ent;

	for( i = 0; i < svgame.numEntities; i++ )
	{
		ent = EDICT_NUM( i );
		if( ent->free ) continue;
		SV_FreeEdict( ent );
	}
}

void SV_PlaybackEvent( sizebuf_t *msg, event_info_t *info )
{
	event_args_t	nullargs;

	ASSERT( msg );
	ASSERT( info );

	Mem_Set( &nullargs, 0, sizeof( nullargs ));

	BF_WriteWord( msg, info->index );			// send event index
	BF_WriteWord( msg, (int)( info->fire_time * 100.0f ));	// send event delay
	MSG_WriteDeltaEvent( msg, &nullargs, &info->args );	// FIXME: zero-compressing
}

const char *SV_ClassName( const edict_t *e )
{
	if( !e ) return "(null)";
	if( e->free ) return "freed";
	return STRING( e->v.classname );
}

static qboolean SV_IsValidCmd( const char *pCmd )
{
	size_t	len;
                              	
	len = com.strlen( pCmd );

	// valid commands all have a ';' or newline '\n' as their last character
	if( len && ( pCmd[len-1] == '\n' || pCmd[len-1] == ';' ))
		return true;
	return false;
}

sv_client_t *SV_ClientFromEdict( const edict_t *pEdict, qboolean spawned_only )
{
	sv_client_t	*client;
	int		i;

	if( !SV_IsValidEdict( pEdict ))
		return NULL;

	i = NUM_FOR_EDICT( pEdict ) - 1;
	if( i < 0 || i >= sv_maxclients->integer )
		return NULL;

	if( spawned_only )
	{
		if( svs.clients[i].state != cs_spawned )
			return NULL;
	}
#if 0
	else
	{
		if( svs.clients[i].state < cs_connected )
			return NULL;
	}
#endif
	client = svs.clients + i;

	return client;
}

/*
=========
SV_BaselineForEntity

assume pEdict is valid
=========
*/
void SV_BaselineForEntity( edict_t *pEdict )
{
	int		usehull, player;
	int		modelindex;
	entity_state_t	baseline;
	float		*mins, *maxs;
	sv_client_t	*cl;

	if( pEdict->v.flags & FL_CLIENT && ( cl = SV_ClientFromEdict( pEdict, false )))
	{
		usehull = ( pEdict->v.flags & FL_DUCKING ) ? true : false;
		modelindex = cl->modelindex ? cl->modelindex : pEdict->v.modelindex;
		mins = svgame.player_mins[usehull]; 
		maxs = svgame.player_maxs[usehull]; 
		player = true;
	}
	else
	{
		if( pEdict->v.effects == EF_NODRAW )
			return;

		if( !pEdict->v.modelindex || !STRING( pEdict->v.model ))
			return; // invisible

		modelindex = pEdict->v.modelindex;
		mins = pEdict->v.mins; 
		maxs = pEdict->v.maxs; 
		player = false;
	}

	// take current state as baseline
	Mem_Set( &baseline, 0, sizeof( baseline )); 
	baseline.number = pEdict->serialnumber;
	svgame.dllFuncs.pfnCreateBaseline( player, baseline.number, &baseline, pEdict, modelindex, mins, maxs );

	// set entity type
	if( pEdict->v.flags & FL_CUSTOMENTITY )
		baseline.entityType = ENTITY_BEAM;
	else baseline.entityType = ENTITY_NORMAL;

	svs.baselines[pEdict->serialnumber] = baseline;
}

void SV_SetClientMaxspeed( sv_client_t *cl, float fNewMaxspeed )
{
	// fakeclients must be changed speed too
	fNewMaxspeed = bound( -svgame.movevars.maxspeed, fNewMaxspeed, svgame.movevars.maxspeed );

	cl->edict->v.maxspeed = fNewMaxspeed;
	Info_SetValueForKey( cl->physinfo, "maxspd", va( "%.f", fNewMaxspeed ));
}

/*
===============================================================================

	Game Builtin Functions

===============================================================================
*/
/*
=========
pfnPrecacheModel

=========
*/
int pfnPrecacheModel( const char *s )
{
	int modelIndex = SV_ModelIndex( s );

	Mod_RegisterModel( s, modelIndex );

	return modelIndex;
}

/*
=========
pfnPrecacheSound

=========
*/
int pfnPrecacheSound( const char *s )
{
	return SV_SoundIndex( s );
}

/*
=================
pfnSetModel

=================
*/
void pfnSetModel( edict_t *e, const char *m )
{
	if( !SV_IsValidEdict( e ))
	{
		MsgDev( D_WARN, "SV_SetModel: invalid entity %s\n", SV_ClassName( e ));
		return;
	}

	if( !m || m[0] <= ' ' )
	{
		MsgDev( D_WARN, "SV_SetModel: null name\n" );
		return;
	}
	SV_SetModel( e, m ); 
}

/*
=================
pfnModelIndex

=================
*/
int pfnModelIndex( const char *m )
{
	int	i;

	if( !m || !m[0] )
		return 0;

	for( i = 1; i < MAX_MODELS && sv.model_precache[i][0]; i++ )
	{
		if( !com.strcmp( sv.model_precache[i], m ))
			return i;
	}
	MsgDev( D_ERROR, "SV_ModelIndex: %s not precached\n", m );

	return 0; 
}

/*
=================
pfnModelFrames

=================
*/
int pfnModelFrames( int modelIndex )
{
	int	numFrames = 0;

	Mod_GetFrames( modelIndex, &numFrames );

	return numFrames;
}

/*
=================
pfnSetSize

=================
*/
void pfnSetSize( edict_t *e, const float *rgflMin, const float *rgflMax )
{
	if( !SV_IsValidEdict( e ))
	{
		MsgDev( D_WARN, "SV_SetSize: invalid entity %s\n", SV_ClassName( e ));
		return;
	}

	// ignore world silently
	if( e == EDICT_NUM( 0 ))
		return;

	SV_SetMinMaxSize( e, rgflMin, rgflMax );
}

/*
=================
pfnChangeLevel

=================
*/
void pfnChangeLevel( const char* s1, const char* s2 )
{
	static uint	last_spawncount = 0;

	if( !s1 || s1[0] <= ' ' ) return;

	// make sure we don't issue two changelevels
	if( svs.spawncount == last_spawncount )
		return;

	last_spawncount = svs.spawncount;

	// make sure we don't issue two changelevels
	if( svs.changelevel_next_time > host.realtime )
		return;

	svs.changelevel_next_time = host.realtime + 1.0f;		// rest 1 secs if failed

	SV_SkipUpdates ();

	if( !s2 ) Cbuf_AddText( va( "changelevel %s\n", s1 ));	// Quake changlevel
	else Cbuf_AddText( va( "changelevel %s %s\n", s1, s2 ));	// Half-Life changelevel
}

/*
=================
pfnGetSpawnParms

obsolete
=================
*/
void pfnGetSpawnParms( edict_t *ent )
{
	Host_Error( "SV_GetSpawnParms: %s [%i]\n", SV_ClassName( ent ), NUM_FOR_EDICT( ent ));
}

/*
=================
pfnSaveSpawnParms

obsolete
=================
*/
void pfnSaveSpawnParms( edict_t *ent )
{
	Host_Error( "SV_SaveSpawnParms: %s [%i]\n", SV_ClassName( ent ), NUM_FOR_EDICT( ent ));
}

/*
=================
pfnVecToYaw

=================
*/
float pfnVecToYaw( const float *rgflVector )
{
	if( !rgflVector ) return 0;
	return SV_VecToYaw( rgflVector );
}

/*
=================
pfnMoveToOrigin

=================
*/
void pfnMoveToOrigin( edict_t *ent, const float *pflGoal, float dist, int iMoveType )
{
	if( !SV_IsValidEdict( ent ))
	{
		MsgDev( D_WARN, "SV_MoveToOrigin: invalid entity %s\n", SV_ClassName( ent ));
		return;
	}

	if( !pflGoal )
	{
		MsgDev( D_WARN, "SV_MoveToOrigin: invalid goal pos\n" );
		return;
	}

	SV_MoveToOrigin( ent, pflGoal, dist, iMoveType );
}

/*
==============
pfnChangeYaw

==============
*/
void pfnChangeYaw( edict_t* ent )
{
	if( !SV_IsValidEdict( ent ))
	{
		MsgDev( D_WARN, "SV_ChangeYaw: invalid entity %s\n", SV_ClassName( ent ));
		return;
	}

	ent->v.angles[YAW] = SV_AngleMod( ent->v.ideal_yaw, ent->v.angles[YAW], ent->v.yaw_speed );
}

/*
==============
pfnChangePitch

==============
*/
void pfnChangePitch( edict_t* ent )
{
	if( !SV_IsValidEdict( ent ))
	{
		MsgDev( D_WARN, "SV_ChangePitch: invalid entity %s\n", SV_ClassName( ent ));
		return;
	}

	ent->v.angles[PITCH] = SV_AngleMod( ent->v.idealpitch, ent->v.angles[PITCH], ent->v.pitch_speed );	
}

/*
=========
pfnFindEntityByString

=========
*/
edict_t* pfnFindEntityByString( edict_t *pStartEdict, const char *pszField, const char *pszValue )
{
	int		index = 0, e = 0;
	TYPEDESCRIPTION	*desc = NULL;
	edict_t		*ed;
	const char	*t;

	if( pStartEdict ) e = NUM_FOR_EDICT( pStartEdict );
	if( !pszValue || !*pszValue ) return svgame.edicts;

	while(( desc = SV_GetEntvarsDescirption( index++ )) != NULL )
	{
		if( !com.strcmp( pszField, desc->fieldName ))
			break;
	}

	if( desc == NULL )
	{
		MsgDev( D_ERROR, "SV_FindEntityByString: field %s not a string\n", pszField );
		return svgame.edicts;
	}

	for( e++; e < svgame.numEntities; e++ )
	{
		ed = EDICT_NUM( e );

		if( !SV_IsValidEdict( ed )) continue;
		if( ed->v.flags & FL_KILLME ) continue;

		switch( desc->fieldType )
		{
		case FIELD_STRING:
		case FIELD_MODELNAME:
		case FIELD_SOUNDNAME:
			t = STRING( *(string_t *)&((byte *)&ed->v)[desc->fieldOffset] );
			if( !t ) t = "";
			if( !com.strcmp( t, pszValue ))
				return ed;
			break;
		}
	}
	return svgame.edicts;
}

/*
==============
pfnGetEntityIllum

returns weighted lightvalue for entity position
==============
*/
int pfnGetEntityIllum( edict_t* pEnt )
{
	if( !SV_IsValidEdict( pEnt ))
	{
		MsgDev( D_WARN, "SV_GetEntityIllum: invalid entity %s\n", SV_ClassName( pEnt ));
		return 0;
	}
	return SV_LightForEntity( pEnt );
}

/*
=================
pfnFindEntityInSphere

return NULL instead of world
=================
*/
edict_t* pfnFindEntityInSphere( edict_t *pStartEdict, const float *org, float flRadius )
{
	edict_t	*ent;
	float	distSquared;
	float	eorg;
	int	j, e = 0;

	flRadius *= flRadius;

	if( SV_IsValidEdict( pStartEdict ))
		e = NUM_FOR_EDICT( pStartEdict );

	for( e++; e < svgame.numEntities; e++ )
	{
		ent = EDICT_NUM( e );
		if( !SV_IsValidEdict( ent )) continue;
		if( ent->v.flags & FL_KILLME ) continue;

		distSquared = 0;
		for( j = 0; j < 3 && distSquared <= flRadius; j++ )
		{
			if( org[j] < ent->v.absmin[j] )
				eorg = org[j] - ent->v.absmin[j];
			else if( org[j] > ent->v.absmax[j] )
				eorg = org[j] - ent->v.absmax[j];
			else eorg = 0;

			distSquared += eorg * eorg;
		}
		if( distSquared > flRadius )
			continue;
		return ent;
	}
	return svgame.edicts;
}

/*
=================
pfnFindClientInPVS

return NULL instead of world
=================
*/
edict_t* pfnFindClientInPVS( edict_t *pEdict )
{
	edict_t		*pClient;
	sv_client_t	*cl;
	const float	*org;
	int		i;

	if( !SV_IsValidEdict( pEdict ))
		return NULL;

	for( i = 0; i < svgame.globals->maxClients; i++ )
	{
		pClient = EDICT_NUM( i + 1 );
		if(( cl = SV_ClientFromEdict( pClient, true )) == NULL )
			continue;

		// check for SET_VIEW
		if( SV_IsValidEdict( cl->pViewEntity ))
			org = cl->pViewEntity->v.origin;
		else org = pClient->v.origin;

		if( SV_OriginIn( DVIS_PVS, pEdict->v.origin, org ))
			return pClient;
	}
	return NULL;
}

/*
=================
pfnFindClientInPHS

return NULL instead of world
=================
*/
edict_t* pfnFindClientInPHS( edict_t *pEdict )
{
	edict_t		*pClient;
	sv_client_t	*cl;
	const float	*org;
	int		i;

	if( !SV_IsValidEdict( pEdict ))
		return NULL;

	for( i = 0; i < svgame.globals->maxClients; i++ )
	{
		pClient = EDICT_NUM( i + 1 );
		if(( cl = SV_ClientFromEdict( pClient, true )) == NULL )
			continue;

		// check for SET_VIEW
		if( SV_IsValidEdict( cl->pViewEntity ))
			org = cl->pViewEntity->v.origin;
		else org = pClient->v.origin;

		if( SV_OriginIn( DVIS_PHS, pEdict->v.origin, org ))
			return pClient;
	}
	return NULL;
}

/*
=================
pfnEntitiesInPVS

=================
*/
edict_t *pfnEntitiesInPVS( edict_t *pplayer )
{
	edict_t	*pEdict, *chain;
	int	i, result;

	if( !SV_IsValidEdict( pplayer ))
		return NULL;

	for( chain = NULL, i = svgame.globals->maxClients + 1; i < svgame.numEntities; i++ )
	{
		pEdict = EDICT_NUM( i );

		if( !SV_IsValidEdict( pEdict )) continue;
		if( pEdict->v.flags & FL_KILLME ) continue;
		
		if( Mod_GetType( pEdict->v.modelindex ) == mod_brush )
			result = SV_BoxInPVS( pplayer->v.origin, pEdict->v.absmin, pEdict->v.absmax );
		else result = SV_OriginIn( DVIS_PVS, pplayer->v.origin, pEdict->v.origin );

		if( result )
		{
			pEdict->v.chain = chain;
			chain = pEdict;
		}
	}
	return chain;
}

/*
=================
pfnEntitiesInPHS

=================
*/
edict_t *pfnEntitiesInPHS( edict_t *pplayer )
{
	edict_t	*pEdict, *chain;
	vec3_t	checkPos;
	int	i;

	if( !SV_IsValidEdict( pplayer ))
		return NULL;

	for( chain = NULL, i = svgame.globals->maxClients + 1; i < svgame.numEntities; i++ )
	{
		pEdict = EDICT_NUM( i );

		if( !SV_IsValidEdict( pEdict )) continue;
		if( pEdict->v.flags & FL_KILLME ) continue;

		if( Mod_GetType( pEdict->v.modelindex ) == mod_brush )
			VectorAverage( pEdict->v.absmin, pEdict->v.absmax, checkPos );
		else VectorCopy( pEdict->v.origin, checkPos );

		if( SV_OriginIn( DVIS_PHS, pplayer->v.origin, checkPos ))
		{
			pEdict->v.chain = chain;
			chain = pEdict;
		}
	}
	return chain;
}

/*
==============
pfnMakeVectors

==============
*/
void pfnMakeVectors( const float *rgflVector )
{
	ASSERT( rgflVector != NULL );
	AngleVectors( rgflVector, svgame.globals->v_forward, svgame.globals->v_right, svgame.globals->v_up );
}

/*
==============
pfnCreateEntity

just allocate new one
==============
*/
edict_t* pfnCreateEntity( void )
{
	return SV_AllocEdict();
}

/*
==============
pfnRemoveEntity

free edict private mem, unlink physics etc
==============
*/
void pfnRemoveEntity( edict_t* e )
{
	if( !SV_IsValidEdict( e ))
	{
		MsgDev( D_ERROR, "SV_RemoveEntity: entity already freed\n" );
		return;
	}

	// never free client or world entity
	if( e->serialnumber < ( svgame.globals->maxClients + 1 ))
	{
		MsgDev( D_ERROR, "SV_RemoveEntity: can't delete %s\n", (e == EDICT_NUM( 0 )) ? "world" : "client" );
		return;
	}

	SV_FreeEdict( e );
}

/*
==============
pfnCreateNamedEntity

==============
*/
edict_t* pfnCreateNamedEntity( string_t className )
{
	return SV_AllocPrivateData( NULL, className );
}

/*
=============
pfnMakeStatic

disable entity updates to client
=============
*/
static void pfnMakeStatic( edict_t *ent )
{
	int	index, i;

	if( !SV_IsValidEdict( ent ))
	{
		MsgDev( D_WARN, "SV_MakeStatic: invalid entity %s\n", SV_ClassName( ent ));
		return;
	}

	index = SV_ModelIndex( STRING( ent->v.model ));

	BF_WriteByte( &sv.signon, svc_spawnstatic );
	BF_WriteShort(&sv.signon, index );
	BF_WriteByte( &sv.signon, ent->v.sequence );
	BF_WriteByte( &sv.signon, ent->v.frame );
	BF_WriteWord( &sv.signon, ent->v.colormap );
	BF_WriteByte( &sv.signon, ent->v.skin );

	for(i = 0; i < 3; i++ )
	{
		BF_WriteBitCoord( &sv.signon, ent->v.origin[i] );
		BF_WriteBitAngle( &sv.signon, ent->v.angles[i], 16 );
	}

	BF_WriteByte( &sv.signon, ent->v.rendermode );

	if( ent->v.rendermode != kRenderNormal )
	{
		BF_WriteByte( &sv.signon, ent->v.renderamt );
		BF_WriteByte( &sv.signon, ent->v.rendercolor[0] );
		BF_WriteByte( &sv.signon, ent->v.rendercolor[1] );
		BF_WriteByte( &sv.signon, ent->v.rendercolor[2] );
		BF_WriteByte( &sv.signon, ent->v.renderfx );
	}

	// remove at end of the frame
	ent->v.flags |= FL_KILLME;
}

/*
=============
pfnEntIsOnFloor

legacy builtin
=============
*/
static int pfnEntIsOnFloor( edict_t *e )
{
	if( !SV_IsValidEdict( e ))
	{
		MsgDev( D_WARN, "SV_CheckBottom: invalid entity %s\n", SV_ClassName( e ));
		return 0;
	}

	return SV_CheckBottom( e, MOVE_NORMAL );
}
	
/*
===============
pfnDropToFloor

===============
*/
int pfnDropToFloor( edict_t* e )
{
	vec3_t	end;
	trace_t	trace;

	if( sv.loadgame )
		return 0;

	if( !SV_IsValidEdict( e ))
	{
		MsgDev( D_ERROR, "SV_DropToFloor: invalid entity %s\n", SV_ClassName( e ));
		return false;
	}

	VectorCopy( e->v.origin, end );
	end[2] -= 256;

	trace = SV_Move( e->v.origin, e->v.mins, e->v.maxs, end, MOVE_NORMAL, e );

	if( trace.allsolid )
		return -1;

	if( trace.fraction == 1.0f )
		return 0;

	VectorCopy( trace.endpos, e->v.origin );
	SV_LinkEdict( e, false );
	e->v.flags |= FL_ONGROUND;
	e->v.groundentity = trace.ent;

	return 1;
}

/*
===============
pfnWalkMove

===============
*/
int pfnWalkMove( edict_t *ent, float yaw, float dist, int iMode )
{
	vec3_t	move;

	if( !SV_IsValidEdict( ent ))
	{
		MsgDev( D_WARN, "SV_WalkMove: invalid entity %s\n", SV_ClassName( ent ));
		return false;
	}

	if(!( ent->v.flags & ( FL_FLY|FL_SWIM|FL_ONGROUND )))
		return false;

	yaw = yaw * M_PI * 2 / 360;
	VectorSet( move, com.cos( yaw ) * dist, com.sin( yaw ) * dist, 0.0f );

	switch( iMode )
	{
	case WALKMOVE_NORMAL:
		return SV_MoveStep( ent, move, true );
	case WALKMOVE_WORLDONLY:
		return SV_MoveTest( ent, move, true );
	case WALKMOVE_CHECKONLY:
		return SV_MoveStep( ent, move, false);
	default:
		MsgDev( D_ERROR, "SV_WalkMove: invalid walk mode %i.\n", iMode );
		break;
	}
	return false;
}

/*
=================
pfnSetOrigin

=================
*/
void pfnSetOrigin( edict_t *e, const float *rgflOrigin )
{
	if( !SV_IsValidEdict( e ))
	{
		MsgDev( D_WARN, "SV_SetOrigin: invalid entity %s\n", SV_ClassName( e ));
		return;
	}

	VectorCopy( rgflOrigin, e->v.origin );
	SV_LinkEdict( e, false );
}

/*
=================
SV_BuildSoundMsg

=================
*/
int SV_BuildSoundMsg( edict_t *ent, int chan, const char *samp, int vol, float attn, int flags, int pitch, const vec3_t pos )
{
	int	sound_idx;
	int	entityIndex;

	if( vol < 0 || vol > 255 )
	{
		MsgDev( D_ERROR, "SV_StartSound: volume = %i\n", vol );
		return 0;
	}

	if( attn < ATTN_NONE || attn > ATTN_IDLE )
	{
		MsgDev( D_ERROR, "SV_StartSound: attenuation = %g\n", attn );
		return 0;
	}

	if( chan < 0 || chan > 7 )
	{
		MsgDev( D_ERROR, "SV_StartSound: channel = %i\n", chan );
		return 0;
	}

	if( pitch < 0 || pitch > 255 )
	{
		MsgDev( D_ERROR, "SV_StartSound: pitch = %i\n", pitch );
		return 0;
	}

	if( !samp || !*samp )
	{
		MsgDev( D_ERROR, "SV_StartSound: passed NULL sample\n" );
		return 0;
	}

	if( samp[0] == '!' && com.is_digit( samp + 1 ))
	{
		flags |= SND_SENTENCE;
		sound_idx = com.atoi( samp + 1 );

		if( sound_idx >= 1536 )
		{
			MsgDev( D_ERROR, "SV_StartSound: invalid sentence number %s.\n", samp );
			return 0;
		}
	}
	else if( samp[0] == '#' && com.is_digit( samp + 1 ))
	{
		flags |= SND_SENTENCE;
		sound_idx = com.atoi( samp + 1 ) + 1536;
	}
	else
	{
		sound_idx = SV_SoundIndex( samp );
	}

	if( !ent->v.modelindex || !ent->v.model )
		entityIndex = 0;
	else if( SV_IsValidEdict( ent->v.aiment ))
		entityIndex = ent->v.aiment->serialnumber;
	else entityIndex = ent->serialnumber;

	if( vol != 255 ) flags |= SND_VOLUME;
	if( attn != ATTN_NONE ) flags |= SND_ATTENUATION;
	if( pitch != PITCH_NORM ) flags |= SND_PITCH;

	BF_WriteByte( &sv.multicast, svc_sound );
	BF_WriteWord( &sv.multicast, flags );
	BF_WriteWord( &sv.multicast, sound_idx );
	BF_WriteByte( &sv.multicast, chan );

	if( flags & SND_VOLUME ) BF_WriteByte( &sv.multicast, vol );
	if( flags & SND_ATTENUATION ) BF_WriteByte( &sv.multicast, attn * 64 );
	if( flags & SND_PITCH ) BF_WriteByte( &sv.multicast, pitch );

	BF_WriteWord( &sv.multicast, entityIndex );
	if( flags & SND_FIXED_ORIGIN ) BF_WriteBitVec3Coord( &sv.multicast, pos );

	return 1;
}

/*
=================
SV_StartSound

=================
*/
void SV_StartSound( edict_t *ent, int chan, const char *sample, float vol, float attn, int flags, int pitch )
{
	int 	sound_idx;
	int	entityIndex;
	int	msg_dest;
	vec3_t	origin;

	if( attn < ATTN_NONE || attn > ATTN_IDLE )
	{
		MsgDev( D_ERROR, "SV_StartSound: attenuation must be in range 0-2\n" );
		return;
	}

	if( chan < 0 || chan > 7 )
	{
		MsgDev( D_ERROR, "SV_StartSound: channel must be in range 0-7\n" );
		return;
	}

	if( !SV_IsValidEdict( ent ))
	{
		MsgDev( D_ERROR, "SV_StartSound: edict == NULL\n" );
		return;
	}

	if( vol != VOL_NORM ) flags |= SND_VOLUME;
	if( attn != ATTN_NONE ) flags |= SND_ATTENUATION;
	if( pitch != PITCH_NORM ) flags |= SND_PITCH;
	if( sv.state == ss_loading ) flags |= SND_SPAWNING;

	// can't track this entity on the client.
	// write static sound
//	if( !ent->v.modelindex || !ent->v.model )
		flags |= SND_FIXED_ORIGIN;

	// ultimate method for detect bsp models with invalid solidity (e.g. func_pushable)
	if( Mod_GetType( ent->v.modelindex ) == mod_brush )
	{
		VectorAverage( ent->v.absmin, ent->v.absmax, origin );

		if( flags & SND_SPAWNING )
		{
			msg_dest = MSG_INIT;
			flags |= SND_FIXED_ORIGIN;	// first-time spatialize
		}
		else
		{
			if( chan == CHAN_STATIC )
				msg_dest = MSG_ALL;
			else msg_dest = MSG_PAS_R;
		}
	}
	else
	{
		VectorAverage( ent->v.mins, ent->v.maxs, origin );
		VectorAdd( origin, ent->v.origin, origin );

		if( flags & SND_SPAWNING )
		{
			msg_dest = MSG_INIT;
			flags |= SND_FIXED_ORIGIN;	// first-time spatialize
		}
		else msg_dest = MSG_PAS_R;
	}

	// always sending stop sound command
	if( flags & SND_STOP ) msg_dest = MSG_ALL;

	if( sample[0] == '!' && com.is_digit( sample + 1 ))
	{
		flags |= SND_SENTENCE;
		sound_idx = com.atoi( sample + 1 );
	}
	else
	{
		// precache_sound can be used twice: cache sounds when loading
		// and return sound index when server is active
		sound_idx = SV_SoundIndex( sample );
	}

	if( SV_IsValidEdict( ent->v.aiment ))
		entityIndex = ent->v.aiment->serialnumber;
	else entityIndex = ent->serialnumber;

	BF_WriteByte( &sv.multicast, svc_sound );
	BF_WriteWord( &sv.multicast, flags );
	BF_WriteWord( &sv.multicast, sound_idx );
	BF_WriteByte( &sv.multicast, chan );

	if( flags & SND_VOLUME ) BF_WriteByte( &sv.multicast, vol * 255 );
	if( flags & SND_ATTENUATION ) BF_WriteByte( &sv.multicast, attn * 64 );
	if( flags & SND_PITCH ) BF_WriteByte( &sv.multicast, pitch );

	BF_WriteWord( &sv.multicast, entityIndex );
	if( flags & SND_FIXED_ORIGIN ) BF_WriteBitVec3Coord( &sv.multicast, origin );

	SV_Send( msg_dest, origin, NULL );
}

/*
=================
pfnEmitAmbientSound

=================
*/
void pfnEmitAmbientSound( edict_t *ent, float *pos, const char *sample, float vol, float attn, int flags, int pitch )
{
	int 	number = 0, sound_idx;
	int	msg_dest = MSG_PAS_R;
	vec3_t	origin;

	if( attn < ATTN_NONE || attn > ATTN_IDLE )
	{
		MsgDev( D_ERROR, "SV_AmbientSound: attenuation must be in range 0-2\n" );
		return;
	}

	if( !pos )
	{
		MsgDev( D_ERROR, "SV_AmbientSound: pos == NULL!\n" );
		return;
	}

	if( sv.state == ss_loading ) flags |= SND_SPAWNING;
	if( vol != VOL_NORM ) flags |= SND_VOLUME;
	if( attn != ATTN_NONE ) flags |= SND_ATTENUATION;
	if( pitch != PITCH_NORM ) flags |= SND_PITCH;

	if( flags & SND_SPAWNING )
		msg_dest = MSG_INIT;
	else msg_dest = MSG_ALL;

	// ultimate method for detect bsp models with invalid solidity (e.g. func_pushable)
	if( SV_IsValidEdict( ent ))
	{
		if( Mod_GetType( ent->v.modelindex ) == mod_brush )
		{
			VectorAverage( ent->v.absmin, ent->v.absmax, origin );
			number = ent->serialnumber;
		}
		else
		{
			VectorAverage( ent->v.mins, ent->v.maxs, origin );
			VectorAdd( origin, ent->v.origin, origin );
		}
	}
	else
	{
		VectorCopy( pos, origin );
	}

	// always sending stop sound command
	if( flags & SND_STOP ) msg_dest = MSG_ALL;
	flags |= SND_FIXED_ORIGIN;

	if( sample[0] == '!' && com.is_digit( sample + 1 ))
	{
		flags |= SND_SENTENCE;
		sound_idx = com.atoi( sample + 1 );
	}
	else
	{
		// precache_sound can be used twice: cache sounds when loading
		// and return sound index when server is active
		sound_idx = SV_SoundIndex( sample );
	}

	BF_WriteByte( &sv.multicast, svc_ambientsound );
	BF_WriteWord( &sv.multicast, flags );
	BF_WriteWord( &sv.multicast, sound_idx );
	BF_WriteByte( &sv.multicast, CHAN_STATIC );

	if( flags & SND_VOLUME ) BF_WriteByte( &sv.multicast, vol * 255 );
	if( flags & SND_ATTENUATION ) BF_WriteByte( &sv.multicast, attn * 64 );
	if( flags & SND_PITCH ) BF_WriteByte( &sv.multicast, pitch );

	// plays from fixed position
	BF_WriteWord( &sv.multicast, number );
	BF_WriteBitVec3Coord( &sv.multicast, pos );

	SV_Send( msg_dest, origin, NULL );
}

/*
=================
pfnTraceLine

=================
*/
static void pfnTraceLine( const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr )
{
	trace_t	trace;

	if( !ptr ) return;

	if( svgame.globals->trace_flags & 1 )
		fNoMonsters |= FMOVE_SIMPLEBOX;
	svgame.globals->trace_flags = 0;

	trace = SV_Move( v1, vec3_origin, vec3_origin, v2, fNoMonsters, pentToSkip );
	SV_ConvertTrace( ptr, &trace );
	SV_CopyTraceToGlobal( &trace );
}

/*
=================
pfnTraceToss

=================
*/
static void pfnTraceToss( edict_t* pent, edict_t* pentToIgnore, TraceResult *ptr )
{
	trace_t	trace;

	if( !ptr ) return;

	if( !SV_IsValidEdict( pent ))
	{
		MsgDev( D_WARN, "SV_MoveToss: invalid entity %s\n", SV_ClassName( pent ));
		return;
	}

	trace = SV_MoveToss( pent, pentToIgnore );
	SV_ConvertTrace( ptr, &trace );
	SV_CopyTraceToGlobal( &trace );
}

/*
=================
pfnTraceHull

=================
*/
static void pfnTraceHull( const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr )
{
	trace_t	trace;

	if( !ptr ) return;

	if( svgame.globals->trace_flags & 1 )
		fNoMonsters |= FMOVE_SIMPLEBOX;
	svgame.globals->trace_flags = 0;

	trace = SV_MoveHull( v1, hullNumber, v2, fNoMonsters, pentToSkip );
	SV_ConvertTrace( ptr, &trace );
	SV_CopyTraceToGlobal( &trace );
}

/*
=============
pfnTraceMonsterHull

=============
*/
static int pfnTraceMonsterHull( edict_t *pEdict, const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr )
{
	trace_t	trace;

	if( !SV_IsValidEdict( pEdict ))
	{
		MsgDev( D_WARN, "SV_TraceMonsterHull: invalid entity %s\n", SV_ClassName( pEdict ));
		return 1;
	}

	if( svgame.globals->trace_flags & 1 )
		fNoMonsters |= FMOVE_SIMPLEBOX;
	svgame.globals->trace_flags = 0;

	trace = SV_Move( v1, pEdict->v.mins, pEdict->v.maxs, v2, fNoMonsters, pentToSkip );
	if( ptr )
	{
		SV_ConvertTrace( ptr, &trace );
		SV_CopyTraceToGlobal( &trace );
	}

	if( trace.allsolid || trace.fraction != 1.0f )
		return true;
	return false;
}

/*
=============
pfnTraceModel

=============
*/
static void pfnTraceModel( const float *v1, const float *v2, int hullNumber, edict_t *pent, TraceResult *ptr )
{
	float	*mins, *maxs;
	trace_t	trace;

	if( !ptr ) return;

	if( !SV_IsValidEdict( pent ))
	{
		MsgDev( D_WARN, "TraceModel: invalid entity %s\n", SV_ClassName( pent ));
		return;
	}

	hullNumber = bound( 0, hullNumber, 3 );
	mins = sv.worldmodel->hulls[hullNumber].clip_mins;
	maxs = sv.worldmodel->hulls[hullNumber].clip_maxs;

	trace = SV_TraceHull( pent, hullNumber, v1, mins, maxs, v2 );
	SV_ConvertTrace( ptr, &trace );
	SV_CopyTraceToGlobal( &trace );
}

/*
=============
pfnTraceTexture

returns texture basename
=============
*/
static const char *pfnTraceTexture( edict_t *pTextureEntity, const float *v1, const float *v2 )
{
	if( !SV_IsValidEdict( pTextureEntity ))
	{
		MsgDev( D_WARN, "TraceTexture: invalid entity %s\n", SV_ClassName( pTextureEntity ));
		return NULL;
	}

	return SV_TraceTexture( pTextureEntity, v1, v2 );
}

/*
=============
pfnTraceSphere

trace sphere instead of bbox
=============
*/
void pfnTraceSphere( const float *v1, const float *v2, int fNoMonsters, float radius, edict_t *pentToSkip, TraceResult *ptr )
{
	// FIXME: implement
}

/*
=============
pfnBoxVisible

=============
*/
static int pfnBoxVisible( const float *mins, const float *maxs, const byte *pset )
{
	return Mod_BoxVisible( mins, maxs, pset );
}

/*
=============
pfnGetAimVector

FIXME: use speed for reduce aiming accuracy
=============
*/
void pfnGetAimVector( edict_t* ent, float speed, float *rgflReturn )
{
	edict_t		*check, *bestent;
	vec3_t		start, dir, end, bestdir;
	float		dist, bestdist;
	qboolean		fNoFriendlyFire;
	int		i, j;
	trace_t		tr;

	// these vairable defined in game.dll	
	fNoFriendlyFire = Cvar_VariableValue( "mp_friendlyfire" );
	VectorCopy( svgame.globals->v_forward, rgflReturn );	// assume failure if it returns early

	if( !SV_IsValidEdict( ent ))
	{
		MsgDev( D_WARN, "SV_GetAimVector: invalid entity %s\n", SV_ClassName( ent ));
		return;
	}

	VectorCopy( ent->v.origin, start );
	start[2] += 20;

	// try sending a trace straight
	VectorCopy( svgame.globals->v_forward, dir );
	VectorMA( start, 2048, dir, end );
	tr = SV_Move( start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent );

	if( tr.ent && (tr.ent->v.takedamage == DAMAGE_AIM && fNoFriendlyFire || ent->v.team <= 0 || ent->v.team != tr.ent->v.team ))
	{
		VectorCopy( svgame.globals->v_forward, rgflReturn );
		return;
	}

	// try all possible entities
	VectorCopy( dir, bestdir );
	bestdist = 0.5f;
	bestent = NULL;

	check = EDICT_NUM( 1 ); // start at first client
	for( i = 1; i < svgame.numEntities; i++, check++ )
	{
		if( check->v.takedamage != DAMAGE_AIM ) continue;
		if( check == ent ) continue;
		if( fNoFriendlyFire && ent->v.team > 0 && ent->v.team == check->v.team )
			continue;	// don't aim at teammate
		for( j = 0; j < 3; j++ )
			end[j] = check->v.origin[j] + 0.5f * (check->v.mins[j] + check->v.maxs[j]);
		VectorSubtract( end, start, dir );
		VectorNormalize( dir );
		dist = DotProduct( dir, svgame.globals->v_forward );
		if( dist < bestdist ) continue; // to far to turn
		tr = SV_Move( start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent );
		if( tr.ent == check )
		{	
			// can shoot at this one
			bestdist = dist;
			bestent = check;
		}
	}

	if( bestent )
	{
		VectorSubtract( bestent->v.origin, ent->v.origin, dir );
		dist = DotProduct( dir, svgame.globals->v_forward );
		VectorScale( svgame.globals->v_forward, dist, end );
		end[2] = dir[2];
		VectorNormalize( end );
		VectorCopy( end, rgflReturn );
	}
	else VectorCopy( bestdir, rgflReturn );
}

/*
=========
pfnServerCommand

=========
*/
void pfnServerCommand( const char* str )
{
	if( SV_IsValidCmd( str )) Cbuf_AddText( str );
	else MsgDev( D_ERROR, "bad server command %s\n", str );
}

/*
=========
pfnServerExecute

=========
*/
void pfnServerExecute( void )
{
	Cbuf_Execute();
}

/*
=========
pfnClientCommand

=========
*/
void pfnClientCommand( edict_t* pEdict, char* szFmt, ... )
{
	sv_client_t	*client;
	string		buffer;
	va_list		args;

	if( sv.state != ss_active )
	{
		MsgDev( D_ERROR, "SV_ClientCommand: server is not active!\n" );
		return;
	}

	client = SV_ClientFromEdict( pEdict, true );
	if( client == NULL )
	{
		MsgDev( D_ERROR, "SV_ClientCommand: client is not spawned!\n" );
		return;
	}

	if( client->fakeclient )
		return;

	va_start( args, szFmt );
	com.vsnprintf( buffer, MAX_STRING, szFmt, args );
	va_end( args );

	if( SV_IsValidCmd( buffer ))
	{
		BF_WriteByte( &client->netchan.message, svc_stufftext );
		BF_WriteString( &client->netchan.message, buffer );
	}
	else MsgDev( D_ERROR, "Tried to stuff bad command %s\n", buffer );
}

/*
=================
pfnParticleEffect

Make sure the event gets sent to all clients
=================
*/
void pfnParticleEffect( const float *org, const float *dir, float color, float count )
{
	int	i, v;

	if( !org || !dir )
	{
		if( !org ) MsgDev( D_ERROR, "SV_StartParticle: NULL origin. Ignored\n" );
		if( !dir ) MsgDev( D_ERROR, "SV_StartParticle: NULL dir. Ignored\n" );
		return;
	}

	BF_WriteByte( &sv.datagram, svc_particle );
	BF_WriteBitVec3Coord( &sv.datagram, org );

	for( i = 0; i < 3; i++ )
	{
		v = bound( -128, dir[i] * 16, 127 );
		BF_WriteChar( &sv.datagram, v );
	}

	BF_WriteByte( &sv.datagram, count );
	BF_WriteByte( &sv.datagram, color );
}

/*
===============
pfnLightStyle

===============
*/
void pfnLightStyle( int style, const char* val )
{
	if( style < 0 ) style = 0;
	if( style >= MAX_LIGHTSTYLES )
		Host_Error( "SV_LightStyle: style: %i >= %d", style, MAX_LIGHTSTYLES );

	SV_SetLightStyle( style, val ); // set correct style
}

/*
=================
pfnDecalIndex

register decal shader on client
=================
*/
int pfnDecalIndex( const char *m )
{
	int	i;

	if( !m || !m[0] )
		return 0;

	for( i = 1; i < MAX_DECALS && host.draw_decals[i][0]; i++ )
	{
		if( !com.stricmp( host.draw_decals[i], m ))
			return i;
	}

	// throw warning
	MsgDev( D_WARN, "Can't find decal %s\n", m );

	return 0;	
}

/*
=============
pfnPointContents

=============
*/
static int pfnPointContents( const float *rgflVector )
{
	return SV_PointContents( rgflVector );
}

/*
=============
pfnMessageBegin

=============
*/
void pfnMessageBegin( int msg_dest, int msg_num, const float *pOrigin, edict_t *ed )
{
	int	i, iSize;

	if( svgame.msg_started )
		Host_Error( "MessageBegin: New message started when msg '%s' has not been sent yet\n", svgame.msg_name );
	svgame.msg_started = true;

	// check range
	msg_num = bound( svc_bad, msg_num, 255 );

	if( msg_num < svc_lastmsg )
	{
		svgame.msg_name = NULL;
		svgame.msg_index = -1; // this is a system message

		if( msg_num == svc_temp_entity )
			iSize = -1; // temp entity have variable size
		else iSize = 0;
	}
	else
	{
		// check for existing
		for( i = 0; i < MAX_USER_MESSAGES && svgame.msg[i].name[0]; i++ )
		{
			if( svgame.msg[i].number == msg_num )
				break; // found
		}

		if( i == MAX_USER_MESSAGES )
		{
			Host_Error( "MessageBegin: tired to send unregistered message %i\n", msg_num );
			return;
		}

		svgame.msg_name = svgame.msg[i].name;
		iSize = svgame.msg[i].size;
		svgame.msg_index = i;
	}

	BF_WriteByte( &sv.multicast, msg_num );

	// save message destination
	if( pOrigin ) VectorCopy( pOrigin, svgame.msg_org );
	else VectorClear( svgame.msg_org );

	if( iSize == -1 )
	{
		// variable sized messages sent size as first byte
		svgame.msg_size_index = BF_GetNumBytesWritten( &sv.multicast );
		BF_WriteByte( &sv.multicast, 0 ); // reserve space for now
	}
	else svgame.msg_size_index = -1; // message has constant size

	svgame.msg_realsize = 0;
	svgame.msg_dest = msg_dest;
	svgame.msg_ent = ed;
}

/*
=============
pfnMessageEnd

=============
*/
void pfnMessageEnd( void )
{
	const char	*name = "Unknown";
	float		*org = NULL;

	if( svgame.msg_name ) name = svgame.msg_name;
	if( !svgame.msg_started ) Host_Error( "MessageEnd: called with no active message\n" );
	svgame.msg_started = false;

	// check for system message
	if( svgame.msg_index == -1 )
	{
		if( svgame.msg_size_index != -1 )
		{
			// variable sized message
			if( svgame.msg_realsize >= 255 )
			{
				MsgDev( D_ERROR, "SV_Message: %s too long (more than 255 bytes)\n", name );
				BF_Clear( &sv.multicast );
				return;
			}
			else if( svgame.msg_realsize < 0 )
			{
				MsgDev( D_ERROR, "SV_Message: %s writes NULL message\n", name );
				BF_Clear( &sv.multicast );
				return;
			}
		}
		sv.multicast.pData[svgame.msg_size_index] = svgame.msg_realsize;
	}
	else if( svgame.msg[svgame.msg_index].size != -1 )
	{
		int expsize = svgame.msg[svgame.msg_index].size;
		int realsize = svgame.msg_realsize;
	
		// compare bounds
		if( expsize != realsize )
		{
			MsgDev( D_ERROR, "SV_Message: %s expected %i bytes, it written %i. Ignored.\n", name, expsize, realsize );
			BF_Clear( &sv.multicast );
			return;
		}
	}
	else if( svgame.msg_size_index != -1 )
	{
		// variable sized message
		if( svgame.msg_realsize >= 255 )
		{
			MsgDev( D_ERROR, "SV_Message: %s too long (more than 255 bytes)\n", name );
			BF_Clear( &sv.multicast );
			return;
		}
		else if( svgame.msg_realsize < 0 )
		{
			MsgDev( D_ERROR, "SV_Message: %s writes NULL message\n", name );
			BF_Clear( &sv.multicast );
			return;
		}

		sv.multicast.pData[svgame.msg_size_index] = svgame.msg_realsize;
	}
	else
	{
		// this should never happen
		MsgDev( D_ERROR, "SV_Message: %s have encountered error\n", name );
		BF_Clear( &sv.multicast );
		return;
	}

	if( !VectorIsNull( svgame.msg_org )) org = svgame.msg_org;
	svgame.msg_dest = bound( MSG_BROADCAST, svgame.msg_dest, MSG_SPEC );
	SV_Send( svgame.msg_dest, org, svgame.msg_ent );
}

/*
=============
pfnWriteByte

=============
*/
void pfnWriteByte( int iValue )
{
	if( iValue == -1 ) iValue = 0xFF; // convert char to byte 
	BF_WriteByte( &sv.multicast, iValue );
	svgame.msg_realsize++;
}

/*
=============
pfnWriteChar

=============
*/
void pfnWriteChar( int iValue )
{
	BF_WriteChar( &sv.multicast, iValue );
	svgame.msg_realsize++;
}

/*
=============
pfnWriteShort

=============
*/
void pfnWriteShort( int iValue )
{
	BF_WriteShort( &sv.multicast, (short)iValue );
	svgame.msg_realsize += 2;
}

/*
=============
pfnWriteLong

=============
*/
void pfnWriteLong( int iValue )
{
	BF_WriteLong( &sv.multicast, iValue );
	svgame.msg_realsize += 4;
}

/*
=============
pfnWriteAngle

this is low-res angle
=============
*/
void pfnWriteAngle( float flValue )
{
	int	iAngle = ((int)(( flValue ) * 256 / 360) & 255);

	BF_WriteChar( &sv.multicast, iAngle );
	svgame.msg_realsize += 1;
}

/*
=============
pfnWriteCoord

=============
*/
void pfnWriteCoord( float flValue )
{
	BF_WriteShort( &sv.multicast, (int)( flValue * 8.0f ));
	svgame.msg_realsize += 2;
}

/*
=============
pfnWriteString

=============
*/
void pfnWriteString( const char *src )
{
	char	*dst, string[MAX_SYSPATH];
	int	len = com.strlen( src ) + 1;

	if( len >= MAX_SYSPATH )
	{
		MsgDev( D_ERROR, "pfnWriteString: exceeds %i symbols\n", MAX_SYSPATH );
		BF_WriteChar( &sv.multicast, 0 );
		svgame.msg_realsize += 1; 
		return;
	}

	// prepare string to sending
	dst = string;

	while( 1 )
	{
		// some escaped chars parsed as two symbols - merge it here
		if( src[0] == '\\' && src[1] == 'n' )
		{
			*dst++ = '\n';
			src += 2;
			len -= 1;
		}
		else if( src[0] == '\\' && src[1] == 'r' )
		{
			*dst++ = '\r';
			src += 2;
			len -= 1;
		}
		else if( src[0] == '\\' && src[1] == 't' )
		{
			*dst++ = '\t';
			src += 2;
			len -= 1;
		}
		else if(( *dst++ = *src++ ) == 0 )
			break;
	}

	*dst = '\0'; // string end (not included in count)
	BF_WriteString( &sv.multicast, string );

	// NOTE: some messages with constant string length can be marked as known sized
	svgame.msg_realsize += len;
}

/*
=============
pfnWriteEntity

=============
*/
void pfnWriteEntity( int iValue )
{
	if( iValue < 0 || iValue >= svgame.numEntities )
		Host_Error( "BF_WriteEntity: invalid entnumber %i\n", iValue );
	BF_WriteShort( &sv.multicast, iValue );
	svgame.msg_realsize += 2;
}

/*
=============
pfnCVarRegister

=============
*/
void pfnCVarRegister( cvar_t *pCvar )
{
	Cvar_Register( pCvar );
}

/*
=============
pfnCvar_DirectSet

=============
*/
void pfnCvar_DirectSet( cvar_t *var, char *value )
{
	Cvar_DirectSet( var, value );
}

/*
=============
pfnAlertMessage

=============
*/
static void pfnAlertMessage( ALERT_TYPE level, char *szFmt, ... )
{
	char	buffer[2048];	// must support > 1k messages
	va_list	args;

	va_start( args, szFmt );
	com.vsnprintf( buffer, 2048, szFmt, args );
	va_end( args );

	if( level == at_notice )
	{
		com.print( buffer ); // notice printing always
	}
	else if( level == at_console && host.developer >= D_INFO )
	{
		com.print( buffer );
	}
	else if( level == at_aiconsole && host.developer >= D_AICONSOLE )
	{
		com.print( buffer );
	}
	else if( level == at_warning && host.developer >= D_WARN )
	{
		com.print( va( "^3Warning:^7 %s", buffer ));
	}
	else if( level == at_error && host.developer >= D_ERROR )
	{
		com.print( va( "^1Error:^7 %s", buffer ));
	} 
}

/*
=============
pfnEngineFprintf

legacy. probably was a part of early save\restore system
=============
*/
static void pfnEngineFprintf( FILE *pfile, char *szFmt, ... )
{
	char	buffer[2048];	// must support > 1k messages
	va_list	args;

	va_start( args, szFmt );
	com.vsnprintf( buffer, 2048, szFmt, args );
	va_end( args );

	fprintf( pfile, buffer );
}
	
/*
=============
pfnPvAllocEntPrivateData

=============
*/
void pfnBuildSoundMsg( edict_t *pSource, int chan, const char *samp, float fvol, float attn, int fFlags, int pitch, int msg_dest, int msg_type, const float *pOrigin, edict_t *pSend )
{
	pfnMessageBegin( msg_dest, msg_type, pOrigin, pSend );
	SV_BuildSoundMsg( pSource, chan, samp, fvol * 255, attn, fFlags, pitch, pOrigin );
	pfnMessageEnd();
}

/*
=============
pfnPvAllocEntPrivateData

=============
*/
void *pfnPvAllocEntPrivateData( edict_t *pEdict, long cb )
{
	ASSERT( pEdict );
	ASSERT( pEdict->free == false );

	// to avoid multiple alloc
	pEdict->pvPrivateData = (void *)Mem_Realloc( svgame.mempool, pEdict->pvPrivateData, cb );

	return pEdict->pvPrivateData;
}

/*
=============
pfnPvEntPrivateData

=============
*/
void *pfnPvEntPrivateData( edict_t *pEdict )
{
	if( pEdict )
		return pEdict->pvPrivateData;
	return NULL;
}

/*
=============
pfnFreeEntPrivateData

=============
*/
void pfnFreeEntPrivateData( edict_t *pEdict )
{
	if( !pEdict ) return;
	if( pEdict->pvPrivateData )
		Mem_Free( pEdict->pvPrivateData );
	pEdict->pvPrivateData = NULL; // freed
}

/*
=============
SV_AllocString

=============
*/
string_t SV_AllocString( const char *szValue )
{
	const char	*newString;

	newString = com.stralloc( svgame.stringspool, szValue, __FILE__, __LINE__ );
	return newString - svgame.globals->pStringBase;
}		

/*
=============
SV_GetString

=============
*/
const char *SV_GetString( string_t iString )
{
	return (svgame.globals->pStringBase + iString);
}

/*
=============
pfnGetVarsOfEnt

=============
*/
entvars_t *pfnGetVarsOfEnt( edict_t *pEdict )
{
	if( pEdict )
		return &pEdict->v;
	return NULL;
}

/*
=============
pfnPEntityOfEntOffset

=============
*/
edict_t* pfnPEntityOfEntOffset( int iEntOffset )
{
	return (&((edict_t*)svgame.vp)[iEntOffset]);
}

/*
=============
pfnEntOffsetOfPEntity

=============
*/
int pfnEntOffsetOfPEntity( const edict_t *pEdict )
{
	return ((byte *)pEdict - (byte *)svgame.vp);
}

/*
=============
pfnIndexOfEdict

=============
*/
int pfnIndexOfEdict( const edict_t *pEdict )
{
	if( !SV_IsValidEdict( pEdict ))
		return 0;
	return NUM_FOR_EDICT( pEdict );
}

/*
=============
pfnPEntityOfEntIndex

=============
*/
edict_t* pfnPEntityOfEntIndex( int iEntIndex )
{
	if( iEntIndex < 0 || iEntIndex >= svgame.numEntities )
		return NULL; // out of range

	if( EDICT_NUM( iEntIndex )->free )
		return NULL;
	return EDICT_NUM( iEntIndex );
}

/*
=============
pfnFindEntityByVars

debug routine
=============
*/
edict_t* pfnFindEntityByVars( entvars_t *pvars )
{
	edict_t	*e;
	int	i;

	// don't pass invalid arguments
	if( !pvars ) return NULL;

	for( i = 0; i < svgame.numEntities; i++ )
	{
		e = EDICT_NUM( i );
		if( &e->v == pvars )
			return e;	// found it
	}
	return NULL;
}

/*
=============
pfnGetModelPtr

returns pointer to a studiomodel
=============
*/
static void *pfnGetModelPtr( edict_t* pEdict )
{
	model_t	*mod;

	if( !pEdict || pEdict->free )
		return NULL;
	mod = CM_ClipHandleToModel( pEdict->v.modelindex );
	return Mod_Extradata( mod );
}

/*
=============
pfnRegUserMsg

=============
*/
int pfnRegUserMsg( const char *pszName, int iSize )
{
	int	i;
	
	if( !pszName || !pszName[0] )
		return svc_bad;

	if( com.strlen( pszName ) >= sizeof( svgame.msg[0].name ))
	{
		MsgDev( D_ERROR, "REG_USER_MSG: too long name %s\n", pszName );
		return svc_bad; // force error
	}

	if( iSize > 255 )
	{
		MsgDev( D_ERROR, "REG_USER_MSG: %s has too big size %i\n", pszName, iSize );
		return svc_bad; // force error
	}

	// make sure what size inrange
	iSize = bound( -1, iSize, 255 );

	// message 0 is reserved for svc_bad
	for( i = 0; i < MAX_USER_MESSAGES && svgame.msg[i].name[0]; i++ )
	{
		// see if already registered
		if( !com.strcmp( svgame.msg[i].name, pszName ))
			return svc_lastmsg + i; // offset
	}

	if( i == MAX_USER_MESSAGES ) 
	{
		MsgDev( D_ERROR, "REG_USER_MSG: user messages limit exceeded\n" );
		return svc_bad;
	}

	// register new message
	com.strncpy( svgame.msg[i].name, pszName, sizeof( svgame.msg[i].name ));
	svgame.msg[i].number = svc_lastmsg + i;
	svgame.msg[i].size = iSize;

	// catch some user messages
	if( !com.strcmp( pszName, "HudText" ))
		svgame.gmsgHudText = svc_lastmsg + i;

	if( sv.state == ss_active )
	{
		// tell the client about new user message
		BF_WriteByte( &sv.reliable_datagram, svc_usermessage );
		BF_WriteByte( &sv.reliable_datagram, svgame.msg[i].number );
		BF_WriteByte( &sv.reliable_datagram, (byte)iSize );
		BF_WriteString( &sv.reliable_datagram, svgame.msg[i].name );
	}

	return svgame.msg[i].number;
}

/*
=============
pfnAnimationAutomove

animating studiomodel
=============
*/
void pfnAnimationAutomove( const edict_t* pEdict, float flTime )
{
	// this is empty in the original HL
}

/*
=============
pfnGetBonePosition

=============
*/
static void pfnGetBonePosition( const edict_t* pEdict, int iBone, float *rgflOrigin, float *rgflAngles )
{
	if( !SV_IsValidEdict( pEdict ))
	{
		MsgDev( D_WARN, "SV_GetBonePos: invalid entity %s\n", SV_ClassName( pEdict ));
		return;
	}

	SV_GetBonePosition( (edict_t *)pEdict, iBone, rgflOrigin, rgflAngles );
}

/*
=============
pfnFunctionFromName

=============
*/
dword pfnFunctionFromName( const char *pName )
{
	return FS_FunctionFromName( svgame.hInstance, pName );
}

/*
=============
pfnNameForFunction

=============
*/
const char *pfnNameForFunction( dword function )
{
	return FS_NameForFunction( svgame.hInstance, function );
}

/*
=============
pfnClientPrintf

=============
*/
void pfnClientPrintf( edict_t* pEdict, PRINT_TYPE ptype, const char *szMsg )
{
	sv_client_t	*client;

	if( sv.state != ss_active )
	{
		// send message into console during loading
		MsgDev( D_INFO, szMsg );
		return;
	}

	client = SV_ClientFromEdict( pEdict, true );
	if( client == NULL )
	{
		MsgDev( D_ERROR, "SV_ClientPrintf: client is not spawned!\n" );
		return;
	}

	switch( ptype )
	{
	case print_console:
		if( client->fakeclient ) MsgDev( D_INFO, szMsg );
		else SV_ClientPrintf( client, PRINT_HIGH, "%s", szMsg );
		break;
	case print_chat:
		if( client->fakeclient ) return;
		SV_ClientPrintf( client, PRINT_CHAT, "%s", szMsg );
		break;
	case print_center:
		if( client->fakeclient ) return;
		BF_WriteByte( &client->netchan.message, svc_centerprint );
		BF_WriteString( &client->netchan.message, szMsg );
		break;
	}
}

/*
=============
pfnServerPrint

=============
*/
void pfnServerPrint( const char *szMsg )
{
	// while loading in-progress we can sending message only for local client
	if( sv.state != ss_active ) MsgDev( D_INFO, szMsg );	
	else SV_BroadcastPrintf( PRINT_HIGH, "%s", szMsg );
}

/*
=============
pfnGetAttachment

=============
*/
static void pfnGetAttachment( const edict_t *pEdict, int iAttachment, float *rgflOrigin, float *rgflAngles )
{
	if( !SV_IsValidEdict( pEdict ))
	{
		MsgDev( D_WARN, "SV_GetAttachment: invalid entity %s\n", SV_ClassName( pEdict ));
		return;
	}
	SV_StudioGetAttachment(( edict_t *)pEdict, iAttachment, rgflOrigin, rgflAngles );
}

/*
=============
pfnCRC32_Init

=============
*/
void pfnCRC32_Init( dword *pulCRC )
{
	CRC32_Init( pulCRC );
}

/*
=============
pfnCRC32_ProcessBuffer

=============
*/
void pfnCRC32_ProcessBuffer( dword *pulCRC, void *p, int len )
{
	CRC32_ProcessBuffer( pulCRC, p, len );
}

/*
=============
pfnCRC32_ProcessByte

=============
*/
void pfnCRC32_ProcessByte( dword *pulCRC, byte ch )
{
	CRC32_ProcessByte( pulCRC, ch );
}

/*
=============
pfnCRC32_Final

=============
*/
dword pfnCRC32_Final( dword pulCRC )
{
	CRC32_Final( &pulCRC );

	return pulCRC;
}				

/*
=============
pfnCrosshairAngle

=============
*/
void pfnCrosshairAngle( const edict_t *pClient, float pitch, float yaw )
{
	sv_client_t	*client;

	client = SV_ClientFromEdict( pClient, true );
	if( client == NULL )
	{
		MsgDev( D_ERROR, "SV_SetCrosshairAngle: invalid client!\n" );
		return;
	}

	// fakeclients ignores it silently
	if( client->fakeclient ) return;

	if( pitch > 180.0f ) pitch -= 360;
	if( pitch < -180.0f ) pitch += 360;
	if( yaw > 180.0f ) yaw -= 360;
	if( yaw < -180.0f ) yaw += 360;

	BF_WriteByte( &client->netchan.message, svc_crosshairangle );
	BF_WriteChar( &client->netchan.message, pitch * 5 );
	BF_WriteChar( &client->netchan.message, yaw * 5 );
}

/*
=============
pfnSetView

=============
*/
void pfnSetView( const edict_t *pClient, const edict_t *pViewent )
{
	sv_client_t	*client;

	if( pClient == NULL || pClient->free )
	{
		MsgDev( D_ERROR, "PF_SetView: invalid client!\n" );
		return;
	}

	client = SV_ClientFromEdict( pClient, true );
	if( !client )
	{
		MsgDev( D_ERROR, "PF_SetView: not a client!\n" );
		return;
	}

	if( pViewent == NULL || pViewent->free )
	{
		MsgDev( D_ERROR, "PF_SetView: invalid viewent!\n" );
		return;
	}

	if( pClient == pViewent ) client->pViewEntity = NULL;
	else client->pViewEntity = (edict_t *)pViewent;

	// fakeclients ignore to send client message
	if( client->fakeclient ) return;

	BF_WriteByte( &client->netchan.message, svc_setview );
	BF_WriteWord( &client->netchan.message, NUM_FOR_EDICT( pViewent ));
}

/*
=============
pfnCompareFileTime

=============
*/
int pfnCompareFileTime( const char *filename1, const char *filename2, int *iCompare )
{
	int	bRet = 0;

	*iCompare = 0;

	if( filename1 && filename2 )
	{
		long ft1 = FS_FileTime( filename1 );
		long ft2 = FS_FileTime( filename2 );

		*iCompare = Host_CompareFileTime( ft1,  ft2 );
		bRet = 1;
	}
	return bRet;
}

/*
=============
pfnStaticDecal

=============
*/
void pfnStaticDecal( const float *origin, int decalIndex, int entityIndex, int modelIndex )
{
	if( !origin )
	{
		MsgDev( D_ERROR, "SV_StaticDecal: NULL origin. Ignored\n" );
		return;
	}

	SV_CreateDecal( origin, decalIndex, entityIndex, modelIndex, FDECAL_PERMANENT );
}

/*
=============
pfnPrecacheGeneric

can be used for precache scripts
=============
*/
int pfnPrecacheGeneric( const char *s )
{
	return SV_GenericIndex( s );
}

/*
=============
pfnIsDedicatedServer

=============
*/
int pfnIsDedicatedServer( void )
{
	return (host.type == HOST_DEDICATED);
}

/*
=============
pfnGetPlayerWONId

=============
*/
uint pfnGetPlayerWONId( edict_t *e )
{
	int		i;
	sv_client_t	*cl;

	if( sv.state != ss_active )
		return -1;

	if( !SV_ClientFromEdict( e, false ))
		return -1;

	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( cl->edict == e && cl->authentication_method == 0 )
		{
			return cl->WonID;
		}
	}
	return -1;
}

/*
=============
pfnIsMapValid

vaild map must contain one info_player_deatchmatch
=============
*/
int pfnIsMapValid( char *filename )
{
	char	*spawn_entity;
	int	flags;

	// determine spawn entity classname
	// determine spawn entity classname
	if( sv_maxclients->integer == 1 )
		spawn_entity = GI->sp_entity;
	else spawn_entity = GI->mp_entity;

	flags = SV_MapIsValid( filename, spawn_entity, NULL );

	if(( flags & MAP_IS_EXIST ) && ( flags & MAP_HAS_SPAWNPOINT ))
		return true;
	return false;
}

/*
=============
pfnFadeClientVolume

=============
*/
void pfnFadeClientVolume( const edict_t *pEdict, int fadePercent, int fadeOutSeconds, int holdTime, int fadeInSeconds )
{
	sv_client_t *cl;

	cl = SV_ClientFromEdict( pEdict, true );
	if( !cl )
	{
		MsgDev( D_ERROR, "SV_FadeClientVolume: client is not spawned!\n" );
		return;
	}

	if( cl->fakeclient ) return;

	BF_WriteByte( &cl->netchan.message, svc_soundfade );
	BF_WriteByte( &cl->netchan.message, fadePercent );
	BF_WriteByte( &cl->netchan.message, holdTime );
	BF_WriteByte( &cl->netchan.message, fadeOutSeconds );
	BF_WriteByte( &cl->netchan.message, fadeInSeconds );
}

/*
=============
pfnSetClientMaxspeed

fakeclients can be changed speed to
=============
*/
void pfnSetClientMaxspeed( const edict_t *pEdict, float fNewMaxspeed )
{
	sv_client_t *cl;

	cl = SV_ClientFromEdict( pEdict, false ); // connected clients allowed
	if( !cl )
	{
		MsgDev( D_ERROR, "SV_SetClientMaxspeed: client is not active!\n" );
		return;
	}

	SV_SetClientMaxspeed( cl, fNewMaxspeed );
}

/*
=============
pfnCreateFakeClient

=============
*/
edict_t *pfnCreateFakeClient( const char *netname )
{
	return SV_FakeConnect( netname );
}

/*
=============
pfnRunPlayerMove

=============
*/
void pfnRunPlayerMove( edict_t *pClient, const float *v_angle, float fmove, float smove, float upmove, word buttons, byte impulse, byte msec )
{
	sv_client_t	*cl;
	usercmd_t		cmd;

	if( sv.paused ) return;

	if(( cl = SV_ClientFromEdict( pClient, true )) == NULL )
	{
		MsgDev( D_ERROR, "SV_ClientThink: fakeclient is not spawned!\n" );
		return;
	}

	if( !cl->fakeclient )
		return;	// only fakeclients allows

	Mem_Set( &cmd, 0, sizeof( cmd ));
	if( v_angle ) VectorCopy( v_angle, cmd.viewangles );
	cmd.forwardmove = fmove;
	cmd.sidemove = smove;
	cmd.upmove = upmove;
	cmd.buttons = buttons;
	cmd.impulse = impulse;
	cmd.msec = msec;

	cl->random_seed = Com_RandomLong( 0, 0x7fffffff ); // full range

	SV_PreRunCmd( cl, &cmd, cl->random_seed );
	SV_RunCmd( cl, &cmd, cl->random_seed );
	SV_PostRunCmd( cl );

	cl->lastcmd = cmd;
	cl->lastcmd.buttons = 0; // avoid multiple fires on lag
}

/*
=============
pfnNumberOfEntities

returns actual entity count
=============
*/
int pfnNumberOfEntities( void )
{
	int	i, total = 0;

	for( i = 0; i < svgame.numEntities; i++ )
	{
		if( !svgame.edicts[i].free )
			total++;
	}

	return total;
}
	
/*
=============
pfnInfo_RemoveKey

=============
*/
void pfnInfo_RemoveKey( char *s, const char *key )
{
	Info_RemoveKey( s, key );
}

/*
=============
pfnInfoKeyValue

=============
*/
char *pfnInfoKeyValue( char *infobuffer, char *key )
{
	return Info_ValueForKey( infobuffer, key );
}

/*
=============
pfnSetKeyValue

=============
*/
void pfnSetKeyValue( char *infobuffer, char *key, char *value )
{
	Info_SetValueForKey( infobuffer, key, value );
}

/*
=============
pfnGetInfoKeyBuffer

=============
*/
char *pfnGetInfoKeyBuffer( edict_t *e )
{
	sv_client_t	*cl;

	cl = SV_ClientFromEdict( e, false ); // pfnUserInfoChanged passed
	if( cl == NULL )
	{
		MsgDev( D_ERROR, "SV_GetClientUserinfo: client is not connected!\n" );
		return Cvar_Serverinfo(); // otherwise return ServerInfo
	}
	return cl->userinfo;
}

/*
=============
pfnSetClientKeyValue

=============
*/
void pfnSetClientKeyValue( int clientIndex, char *infobuffer, char *key, char *value )
{
	sv_client_t	*cl;

	if( clientIndex < 0 || clientIndex >= sv_maxclients->integer )
		return;
	if( svs.clients[clientIndex].state < cs_spawned )
		return;

	cl = svs.clients + clientIndex;
	Info_SetValueForKey( cl->userinfo, key, value );
	cl->sendinfo = true;
}

/*
=============
pfnGetPhysicsKeyValue

=============
*/
const char *pfnGetPhysicsKeyValue( const edict_t *pClient, const char *key )
{
	sv_client_t	*cl;

	cl = SV_ClientFromEdict( pClient, false ); // pfnUserInfoChanged passed
	if( cl == NULL )
	{
		MsgDev( D_ERROR, "SV_GetClientPhysKey: client is not connected!\n" );
		return "";
	}
	return Info_ValueForKey( cl->physinfo, key );
}

/*
=============
pfnSetPhysicsKeyValue

=============
*/
void pfnSetPhysicsKeyValue( const edict_t *pClient, const char *key, const char *value )
{
	sv_client_t	*cl;

	cl = SV_ClientFromEdict( pClient, false ); // pfnUserInfoChanged passed
	if( cl == NULL )
	{
		MsgDev( D_ERROR, "SV_SetClientPhysinfo: client is not connected!\n" );
		return;
	}
	Info_SetValueForKey( cl->physinfo, key, value );
}

/*
=============
pfnGetPhysicsInfoString

=============
*/
const char *pfnGetPhysicsInfoString( const edict_t *pClient )
{
	sv_client_t	*cl;

	cl = SV_ClientFromEdict( pClient, false ); // pfnUserInfoChanged passed
	if( cl == NULL )
	{
		MsgDev( D_ERROR, "SV_GetClientPhysinfo: client is not connected!\n" );
		return "";
	}
	return cl->physinfo;
}

/*
=============
pfnPrecacheEvent

register or returns already registered event id
a type of event is ignored at this moment
=============
*/
word pfnPrecacheEvent( int type, const char *psz )
{
	return (word)SV_EventIndex( psz );
}

/*
=============
pfnPlaybackEvent

=============
*/
void SV_PlaybackEventFull( int flags, const edict_t *pInvoker, word eventindex, float delay, float *origin,
	float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	sv_client_t	*cl;
	event_state_t	*es;
	event_args_t	args;
	event_info_t	*ei = NULL;
	int		j, leafnum, slot, bestslot;
	int		invokerIndex = 0;
	byte		*mask = NULL;
	vec3_t		pvspoint;

	// first check event for out of bounds
	if( eventindex < 1 || eventindex > MAX_EVENTS )
	{
		MsgDev( D_ERROR, "SV_PlaybackEvent: invalid eventindex %i\n", eventindex );
		return;
	}

	// check event for precached
	if( !sv.event_precache[eventindex][0] )
	{
		MsgDev( D_ERROR, "SV_PlaybackEvent: event %i was not precached\n", eventindex );
		return;		
	}

	args.flags = 0;
	if( SV_IsValidEdict( pInvoker ))
		args.entindex = NUM_FOR_EDICT( pInvoker );
	else args.entindex = 0;
	VectorCopy( origin, args.origin );
	VectorCopy( angles, args.angles );

	args.fparam1 = fparam1;
	args.fparam2 = fparam2;
	args.iparam1 = iparam1;
	args.iparam2 = iparam2;
	args.bparam1 = bparam1;
	args.bparam2 = bparam2;

	if(!( flags & FEV_GLOBAL ))
          {
		// PVS message - trying to get a pvspoint
		// args.origin always have higher priority than invoker->origin
		if( !VectorIsNull( args.origin ))
		{
			VectorCopy( args.origin, pvspoint );
		}
		else if( SV_IsValidEdict( pInvoker ))
		{
			VectorCopy( pInvoker->v.origin, pvspoint );
		}
		else
		{
			const char *ev_name = sv.event_precache[eventindex];
			MsgDev( D_ERROR, "%s: not a FEV_GLOBAL event missing origin. Ignored.\n", ev_name );
			return;
		}
	}

	// check event for some user errors
	if( flags & (FEV_NOTHOST|FEV_HOSTONLY))
	{
		if( !SV_ClientFromEdict( pInvoker, true ))
		{
			const char *ev_name = sv.event_precache[eventindex];
			if( flags & FEV_NOTHOST )
				MsgDev( D_WARN, "%s: specified FEV_NOTHOST when invoker not a client\n", ev_name );
			if( flags & FEV_HOSTONLY )
				MsgDev( D_WARN, "%s: specified FEV_HOSTONLY when invoker not a client\n", ev_name );
			// pInvoker isn't a client
			flags &= ~(FEV_NOTHOST|FEV_HOSTONLY);
		}
	}

	flags |= FEV_SERVER; // it's a server event

	if( delay < 0.0f )
		delay = 0.0f; // fixup negative delays

	if( SV_IsValidEdict( pInvoker ))
		invokerIndex = NUM_FOR_EDICT( pInvoker );

	if( flags & FEV_RELIABLE )
	{
		args.ducking = 0;
		VectorClear( args.velocity );
	}
	else if( invokerIndex )
	{
		// get up some info from invoker
		if( VectorIsNull( args.origin )) 
			VectorCopy( pInvoker->v.origin, args.origin );
		if( VectorIsNull( args.angles ))
		{ 
			if( SV_ClientFromEdict( pInvoker, true ))
				VectorCopy( pInvoker->v.v_angle, args.angles );
			else VectorCopy( pInvoker->v.angles, args.angles );
		}
		else if( SV_ClientFromEdict( pInvoker, true ) && VectorCompare( pInvoker->v.angles, args.angles ))
		{
			// NOTE: if user specified pPlayer->pev->angles
			// silently replace it with viewangles, client expected this
			VectorCopy( pInvoker->v.v_angle, args.angles );
		}
		VectorCopy( pInvoker->v.velocity, args.velocity );
		args.ducking = (pInvoker->v.flags & FL_DUCKING) ? true : false;
	}

	if(!( flags & FEV_GLOBAL ))
	{
		mleaf_t	*leaf;

		// setup pvs cluster for invoker
		leaf = Mod_PointInLeaf( pvspoint, sv.worldmodel->nodes );
		mask = Mod_LeafPVS( leaf, sv.worldmodel );
	}

	// process all the clients
	for( slot = 0, cl = svs.clients; slot < sv_maxclients->integer; slot++, cl++ )
	{
		if( cl->state != cs_spawned || !cl->edict || cl->fakeclient )
			continue;

		if( flags & FEV_NOTHOST && cl->edict == pInvoker && cl->local_weapons )
			continue;	// will be played on client side

		if( flags & FEV_HOSTONLY && cl->edict != pInvoker )
			continue;	// sending only to invoker

		if(!( flags & FEV_GLOBAL ))
		{
			// -1 is because pvs rows are 1 based, not 0 based like leafs
			leafnum = Mod_PointLeafnum( cl->edict->v.origin ) - 1;
			if( mask && (!( mask[leafnum>>3] & (1<<( leafnum & 7 )))))
				continue;
		}

		// all checks passed, send the event

		// reliable event
		if( flags & FEV_RELIABLE )
		{
			event_info_t	info;

			info.index = eventindex;
			info.fire_time = delay;
			info.args = args;
			info.entity_index = invokerIndex;
			info.packet_index = -1;
			info.flags = 0; // server ignore flags

			// skipping queue, write in reliable datagram
			BF_WriteByte( &cl->netchan.message, svc_event_reliable );
			SV_PlaybackEvent( &cl->netchan.message, &info );
			continue;
		}

		// unreliable event (stores in queue)
		es = &cl->events;
		bestslot = -1;

		for( j = 0; j < MAX_EVENT_QUEUE; j++ )
		{
			ei = &es->ei[j];
		
			if( ei->index == 0 )
			{
				// found an empty slot
				bestslot = j;
				break;
			}
		}
				
		// no slot found for this player, oh well
		if( bestslot == -1 ) continue;

		ei->index = eventindex;
		ei->fire_time = delay;
		ei->args = args;
		ei->entity_index = invokerIndex;
		ei->packet_index = -1;
		ei->flags = 0; // server ignore flags
	}
}

/*
=============
pfnSetFatPVS

The client will interpolate the view position,
so we can't use a single PVS point
=============
*/
byte *pfnSetFatPVS( const float *org )
{
	if( !sv.worldmodel->visdata || sv_novis->integer || !org )
		return Mod_DecompressVis( NULL );

	bitvector = fatpvs;
	fatbytes = (sv.worldmodel->numleafs+31)>>3;
	if(!( sv.hostflags & SVF_PORTALPASS ))
		Mem_Set( bitvector, 0, fatbytes );
	SV_AddToFatPVS( org, DVIS_PVS, sv.worldmodel->nodes );

	return bitvector;
}

/*
=============
pfnSetFatPHS

The client will interpolate the hear position,
so we can't use a single PHS point
=============
*/
byte *pfnSetFatPAS( const float *org )
{
	if( !sv.worldmodel->visdata || sv_novis->integer || !org )
		return Mod_DecompressVis( NULL );

	bitvector = fatphs;
	fatbytes = (sv.worldmodel->numleafs+31)>>3;
	if(!( sv.hostflags & SVF_PORTALPASS ))
		Mem_Set( bitvector, 0, fatbytes );
	SV_AddToFatPVS( org, DVIS_PHS, sv.worldmodel->nodes );

	return bitvector;
}

/*
=============
pfnCheckVisibility

=============
*/
int pfnCheckVisibility( const edict_t *ent, byte *pset )
{
	int	result = 0;

	if( !SV_IsValidEdict( ent ))
	{
		MsgDev( D_WARN, "SV_CheckVisibility: invalid entity %s\n", SV_ClassName( ent ));
		return 0;
	}

	// vis not set - fullvis enabled
	if( !pset ) return 1;

	if( ent->headnode == -1 )
	{
		int	i;

		// check individual leafs
		for( i = 0; i < ent->num_leafs; i++ )
		{
			if( pset[ent->leafnums[i] >> 3] & (1 << (ent->leafnums[i] & 7 )))
				break;
		}

		if( i == ent->num_leafs )
			return 0;	// not visible

		result = 1;	// visible passed by leafs
	}
	else
	{
		mnode_t	*node;

		if( ent->headnode < 0 )
			node = (mnode_t *)(sv.worldmodel->leafs + (-1 - ent->headnode));			
		else node = sv.worldmodel->nodes + ent->headnode;

		// too many leafs for individual check, go by headnode
		if( !SV_HeadnodeVisible( node, pset ))
			return 0;

		result = 2;	// visible passed by headnode
	}

#if 0
	// NOTE: uncommenat this if you want to get more accuracy culling on large brushes
	if( Mod_GetType( ent->v.modelindex ) == mod_brush )
	{
		if( !Mod_BoxVisible( ent->v.absmin, ent->v.absmax, pset ))
			return 0;
		result = 3;	// visible passed by BoxVisible
	}
#endif
	return result;
}

/*
=============
pfnCanSkipPlayer

=============
*/
int pfnCanSkipPlayer( const edict_t *player )
{
	sv_client_t	*cl;

	cl = SV_ClientFromEdict( player, false );
	if( cl == NULL )
	{
		MsgDev( D_ERROR, "SV_CanSkip: client is not connected!\n" );
		return false;
	}

	return cl->local_weapons;
}

/*
=============
pfnGetCurrentPlayer

=============
*/
int pfnGetCurrentPlayer( void )
{
	if( svs.currentPlayer )
		return svs.currentPlayer - svs.clients;
	return -1;
}

/*
=============
pfnSetGroupMask

=============
*/
void pfnSetGroupMask( int mask, int op )
{
	svs.groupmask = mask;
	svs.groupop = op;
}

/*
=============
pfnCreateInstancedBaseline

=============
*/
int pfnCreateInstancedBaseline( int classname, struct entity_state_s *baseline )
{
	int	i;

	if( !baseline ) return -1;

	i = sv.instanced.count;
	if( i > 62 ) return 0;

	sv.instanced.classnames[i] = classname;
	sv.instanced.baselines[i] = *baseline;
	sv.instanced.count++;

	return i+1;
}

/*
=============
pfnEndSection

=============
*/
void pfnEndSection( const char *pszSection )
{
	if( !com.stricmp( "credits", pszSection ))
		Host_Credits ();
	else Host_EndGame( pszSection );
}

/*
=============
pfnGetPlayerUserId

=============
*/
int pfnGetPlayerUserId( edict_t *e )
{
	sv_client_t	*cl;
	int		i;
		
	if( sv.state != ss_active )
		return -1;

	if( !SV_ClientFromEdict( e, false ))
		return -1;

	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( cl->edict == e )
		{
			return cl->userid;
		}
	}

	// couldn't find it
	return -1;
}

/*
=============
pfnSoundTrack

empty trackname - stop previous
=============
*/
void pfnSoundTrack( const char *trackname, int flags )
{
	sizebuf_t	*msg;

	if( !trackname ) return;

	if( sv.state == ss_loading )
		msg = &sv.signon;
	else msg = &sv.reliable_datagram;

	// tell the client about new user message
	BF_WriteByte( &sv.reliable_datagram, svc_cdtrack );
	BF_WriteString( &sv.reliable_datagram, trackname );
}

/*
=============
pfnDropClient

=============
*/
void pfnDropClient( int clientIndex )
{
	if( clientIndex < 0 || clientIndex >= svgame.globals->maxClients )
	{
		MsgDev( D_ERROR, "SV_DropClient: not a client\n" );
		return;
	}
	if( svs.clients[clientIndex].state != cs_spawned )
	{
		MsgDev( D_ERROR, "SV_DropClient: that client slot is not connected\n" );
		return;
	}
	SV_DropClient( svs.clients + clientIndex );
}

/*
=============
pfnGetPlayerPing

=============
*/
void pfnGetPlayerStats( const edict_t *pClient, int *ping, int *packet_loss )
{
	sv_client_t	*cl;

	cl = SV_ClientFromEdict( pClient, false );
	if( cl == NULL )
	{
		MsgDev( D_ERROR, "SV_GetPlayerStats: client is not connected!\n" );
		return;
	}

	if( ping ) *ping = cl->ping * 1000;	// this is should be cl->latency not ping!
	if( packet_loss ) *packet_loss = cl->packet_loss;
}
	
/*
=============
pfnForceUnmodified

=============
*/
void pfnForceUnmodified( FORCE_TYPE type, float *mins, float *maxs, const char *filename )
{
	sv_consistency_t	*pData;
	int		i;

	if( !filename || !*filename )
	{
		Host_Error( "SV_ForceUnmodified: bad filename string.\n" );
	}

	if( sv.state == ss_loading )
	{
		for( i = 0, pData = sv.consistency_files; i < MAX_MODELS; i++, pData++ )
		{
			if( !pData->name )
			{
				pData->name = filename;
				pData->force_state = type;

				if( mins ) VectorCopy( mins, pData->mins );
				if( maxs ) VectorCopy( maxs, pData->maxs );
				return;
			}
			else if( !com.strcmp( filename, pData->name ))
			{
				return;
			}
		}
		Host_Error( "SV_ForceUnmodified: MAX_MODELS limit exceeded\n" );
	}
	else
	{
		for( i = 0, pData = sv.consistency_files; i < MAX_MODELS; i++, pData++ )
		{
			if( !pData->name || com.strcmp( filename, pData->name ))
				continue;

			// if we are here' we found a match.
			return;
		}
		Host_Error( "SV_ForceUnmodified: can only be done during precache\n" );
	}
}

/*
=============
pfnAddServerCommand

=============
*/
void pfnAddServerCommand( const char *cmd_name, void (*function)(void) )
{
	Cmd_AddGameCommand( cmd_name, function );
}

/*
=============
pfnVoice_GetClientListening

=============
*/
qboolean pfnVoice_GetClientListening( int iReceiver, int iSender )
{
	int	iMaxClients = sv_maxclients->integer;

	if( !svs.initialized ) return false;

	if( iReceiver <= 0 || iReceiver > iMaxClients || iSender <= 0 || iSender > iMaxClients )
	{
		MsgDev( D_ERROR, "Voice_GetClientListening: invalid client indexes (%i, %i).\n", iReceiver, iSender );
		return false;
	}

	return ((svs.clients[iSender-1].listeners & ( 1 << iReceiver )) != 0 );
}

/*
=============
pfnVoice_SetClientListening

=============
*/
qboolean pfnVoice_SetClientListening( int iReceiver, int iSender, qboolean bListen )
{
	int	iMaxClients = sv_maxclients->integer;

	if( !svs.initialized ) return false;

	if( iReceiver <= 0 || iReceiver > iMaxClients || iSender <= 0 || iSender > iMaxClients )
	{
		MsgDev( D_ERROR, "Voice_SetClientListening: invalid client indexes (%i, %i).\n", iReceiver, iSender );
		return false;
	}

	if( bListen )
	{
		svs.clients[iSender-1].listeners |= (1 << iReceiver);
	}
	else
	{
		svs.clients[iSender-1].listeners &= ~(1 << iReceiver);
	}
	return true;
}

/*
=============
pfnGetPlayerAuthId

These function must returns cd-key hashed value
but Xash3D currently doesn't have any security checks
return nullstring for now
=============
*/
const char *pfnGetPlayerAuthId( edict_t *e )
{
	sv_client_t	*cl;
	static string	result;
	int		i;

	if( sv.state != ss_active || !SV_IsValidEdict( e ))
	{
		result[0] = '\0';
		return result;
	}

	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( cl->edict == e )
		{
			if( cl->fakeclient )
			{
				com.strncat( result, "BOT", sizeof( result ));
			}
			else if( cl->authentication_method == 0 )
			{
				com.snprintf( result, sizeof( result ), "%u", (uint)cl->WonID );
			}
			else
			{
				com.snprintf( result, sizeof( result ), "%s", SV_GetClientIDString( cl ));
			}
			return result;
		}
	}

	result[0] = '\0';
	return result;
}
					
// engine callbacks
static enginefuncs_t gEngfuncs = 
{
	pfnPrecacheModel,
	pfnPrecacheSound,	
	pfnSetModel,
	pfnModelIndex,
	pfnModelFrames,
	pfnSetSize,	
	pfnChangeLevel,
	pfnGetSpawnParms,
	pfnSaveSpawnParms,
	pfnVecToYaw,
	VectorAngles,
	pfnMoveToOrigin,
	pfnChangeYaw,
	pfnChangePitch,
	pfnFindEntityByString,
	pfnGetEntityIllum,
	pfnFindEntityInSphere,
	pfnFindClientInPVS,
	pfnEntitiesInPVS,
	pfnMakeVectors,
	AngleVectors,
	pfnCreateEntity,
	pfnRemoveEntity,
	pfnCreateNamedEntity,
	pfnMakeStatic,
	pfnEntIsOnFloor,
	pfnDropToFloor,
	pfnWalkMove,
	pfnSetOrigin,
	SV_StartSound,
	pfnEmitAmbientSound,
	pfnTraceLine,
	pfnTraceToss,
	pfnTraceMonsterHull,
	pfnTraceHull,
	pfnTraceModel,
	pfnTraceTexture,
	pfnTraceSphere,
	pfnGetAimVector,
	pfnServerCommand,
	pfnServerExecute,
	pfnClientCommand,
	pfnParticleEffect,
	pfnLightStyle,
	pfnDecalIndex,
	pfnPointContents,
	pfnMessageBegin,
	pfnMessageEnd,
	pfnWriteByte,
	pfnWriteChar,
	pfnWriteShort,
	pfnWriteLong,
	pfnWriteAngle,
	pfnWriteCoord,
	pfnWriteString,
	pfnWriteEntity,
	pfnCVarRegister,
	pfnCVarGetValue,
	pfnCVarGetString,
	pfnCVarSetValue,
	pfnCVarSetString,
	pfnAlertMessage,
	pfnEngineFprintf,
	pfnPvAllocEntPrivateData,
	pfnPvEntPrivateData,
	pfnFreeEntPrivateData,
	SV_GetString,
	SV_AllocString,
	pfnGetVarsOfEnt,
	pfnPEntityOfEntOffset,
	pfnEntOffsetOfPEntity,
	pfnIndexOfEdict,
	pfnPEntityOfEntIndex,
	pfnFindEntityByVars,
	pfnGetModelPtr,
	pfnRegUserMsg,
	pfnAnimationAutomove,
	pfnGetBonePosition,
	pfnFunctionFromName,
	pfnNameForFunction,	
	pfnClientPrintf,
	pfnServerPrint,	
	pfnCmd_Args,
	pfnCmd_Argv,
	pfnCmd_Argc,
	pfnGetAttachment,
	pfnCRC32_Init,
	pfnCRC32_ProcessBuffer,
	pfnCRC32_ProcessByte,
	pfnCRC32_Final,
	pfnRandomLong,
	pfnRandomFloat,
	pfnSetView,
	pfnTime,
	pfnCrosshairAngle,
	pfnLoadFile,
	pfnFreeFile,
	pfnEndSection,
	pfnCompareFileTime,
	pfnGetGameDir,
	pfnCVarRegister,
	pfnFadeClientVolume,
	pfnSetClientMaxspeed,
	pfnCreateFakeClient,
	pfnRunPlayerMove,
	pfnNumberOfEntities,
	pfnGetInfoKeyBuffer,
	pfnInfoKeyValue,
	pfnSetKeyValue,
	pfnSetClientKeyValue,
	pfnIsMapValid,
	pfnStaticDecal,
	pfnPrecacheGeneric,
	pfnGetPlayerUserId,
	pfnBuildSoundMsg,
	pfnIsDedicatedServer,
	pfnCVarGetPointer,
	pfnGetPlayerWONId,
	pfnInfo_RemoveKey,
	pfnGetPhysicsKeyValue,
	pfnSetPhysicsKeyValue,
	pfnGetPhysicsInfoString,
	pfnPrecacheEvent,
	SV_PlaybackEventFull,
	pfnSetFatPVS,
	pfnSetFatPAS,
	pfnCheckVisibility,
	Delta_SetField,
	Delta_UnsetField,
	Delta_AddEncoder,
	pfnGetCurrentPlayer,
	pfnCanSkipPlayer,
	Delta_FindField,
	Delta_SetFieldByIndex,
	Delta_UnsetFieldByIndex,
	pfnSetGroupMask,	
	pfnCreateInstancedBaseline,
	pfnCvar_DirectSet,
	pfnForceUnmodified,
	pfnGetPlayerStats,
	pfnAddServerCommand,
	pfnVoice_GetClientListening,
	pfnVoice_SetClientListening,
	pfnGetPlayerAuthId,
};

/*
====================
SV_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
====================
*/
qboolean SV_ParseEdict( script_t *script, edict_t *ent )
{
	KeyValueData	pkvd[256]; // per one entity
	int		i, numpairs = 0;
	const char	*classname = NULL;
	qboolean		anglehack;
	token_t		token;

	// go through all the dictionary pairs
	while( 1 )
	{	
		string	keyname;

		// parse key
		if( !Com_ReadToken( script, SC_ALLOW_NEWLINES|SC_ALLOW_PATHNAMES2, &token ))
			Host_Error( "ED_ParseEdict: EOF without closing brace\n" );
		if( token.string[0] == '}' ) break; // end of desc

		// anglehack is to allow QuakeEd to write single scalar angles
		// and allow them to be turned into vectors.
		if( !com.strcmp( token.string, "angle" ))
		{
			com.strncpy( token.string, "angles", sizeof( token.string ));
			anglehack = true;
		}
		else anglehack = false;

		com.strncpy( keyname, token.string, sizeof( keyname ));

		// parse value	
		if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &token ))
			Host_Error( "ED_ParseEdict: EOF without closing brace\n" );

		if( token.string[0] == '}' )
			Host_Error( "ED_ParseEdict: closing brace without data\n" );

		// ignore attempts to set key ""
		if( !keyname[0] ) continue;

		// "wad" field is completely ignored
		if( !com.strcmp( keyname, "wad" ))
			continue;

		// keynames with a leading underscore are used for utility comments,
		// and are immediately discarded by engine
		if( world.version == Q1BSP_VERSION && keyname[0] == '_' )
			continue;

		if( anglehack )
		{
			string	temp;
			com.strncpy( temp, token.string, sizeof( temp ));
			com.snprintf( token.string, sizeof( token.string ), "0 %s 0", temp );
		}

		// create keyvalue strings
		pkvd[numpairs].szClassName = (char *)classname;	// unknown at this moment
		pkvd[numpairs].szKeyName = copystring( keyname );
		pkvd[numpairs].szValue = copystring( token.string );
		pkvd[numpairs].fHandled = false;		

		if( !com.strcmp( keyname, "classname" ) && classname == NULL )
			classname = pkvd[numpairs].szValue;
		if( ++numpairs >= 256 ) break;
	}
	
	ent = SV_AllocPrivateData( ent, MAKE_STRING( classname ));

	if( ent->v.flags & FL_KILLME )
		return false;

	for( i = 0; i < numpairs; i++ )
	{
		if( !pkvd[i].fHandled )
		{
			pkvd[i].szClassName = (char *)classname;
			svgame.dllFuncs.pfnKeyValue( ent, &pkvd[i] );
		}

		// no reason to keep this data
		Mem_Free( pkvd[i].szKeyName );
		Mem_Free( pkvd[i].szValue );
	}
	return true;
}

/*
================
SV_LoadFromFile

The entities are directly placed in the array, rather than allocated with
ED_Alloc, because otherwise an error loading the map would have entity
number references out of order.

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.
================
*/
void SV_LoadFromFile( script_t *entities )
{
	token_t	token;
	int	inhibited, spawned, died;
	int	current_skill = Cvar_VariableInteger( "skill" ); // lock skill level
	qboolean	inhibits_ents = (world.version == Q1BSP_VERSION) ? true : false;
	qboolean	deathmatch = Cvar_VariableInteger( "deathmatch" );
	qboolean	create_world = true;
	edict_t	*ent;

	inhibited = 0;
	spawned = 0;
	died = 0;

	// parse ents
	while( Com_ReadToken( entities, SC_ALLOW_NEWLINES|SC_ALLOW_PATHNAMES2, &token ))
	{
		if( token.string[0] != '{' )
			Host_Error( "ED_LoadFromFile: found %s when expecting {\n", token.string );

		if( create_world )
		{
			create_world = false;
			ent = EDICT_NUM( 0 ); // already initialized
		}
		else ent = SV_AllocEdict();

		if( !SV_ParseEdict( entities, ent ))
			continue;

		// remove things from different skill levels or deathmatch
		if( inhibits_ents && deathmatch )
		{
			if( ent->v.spawnflags & (1<<11))
			{
				SV_FreeEdict( ent );
				inhibited++;
				continue;
			}
		}
		else if( inhibits_ents && current_skill == 0 && ent->v.spawnflags & (1<<8))
		{
			SV_FreeEdict( ent );
			inhibited++;
			continue;
		}
		else if( inhibits_ents && current_skill == 1 && ent->v.spawnflags & (1<<9))
		{
			SV_FreeEdict( ent );
			inhibited++;
			continue;
		}
		else if( inhibits_ents && current_skill >= 2 && ent->v.spawnflags & (1<<10))
		{
			SV_FreeEdict( ent );
			inhibited++;
			continue;
		}

		if( svgame.dllFuncs.pfnSpawn( ent ) == -1 )
			died++;
		else spawned++;
	}

	MsgDev( D_INFO, "\n%i entities inhibited\n", inhibited );
}

/*
==============
SpawnEntities

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.
==============
*/
void SV_SpawnEntities( const char *mapname, script_t *entities )
{
	edict_t	*ent;

	MsgDev( D_NOTE, "SV_SpawnEntities()\n" );

	// reset misc parms
	Cvar_Reset( "sv_zmax" );
	Cvar_Reset( "sv_wateramp" );

	// reset sky parms	
	Cvar_Reset( "sv_skycolor_r" );
	Cvar_Reset( "sv_skycolor_g" );
	Cvar_Reset( "sv_skycolor_b" );
	Cvar_Reset( "sv_skyvec_x" );
	Cvar_Reset( "sv_skyvec_y" );
	Cvar_Reset( "sv_skyvec_z" );
	Cvar_Reset( "sv_skyname" );

	ent = EDICT_NUM( 0 );
	if( ent->free ) SV_InitEdict( ent );
	ent->v.model = MAKE_STRING( sv.model_precache[1] );
	ent->v.modelindex = 1;	// world model
	ent->v.solid = SOLID_BSP;
	ent->v.movetype = MOVETYPE_PUSH;

	svgame.globals->maxEntities = GI->max_edicts;
	svgame.globals->maxClients = sv_maxclients->integer;
	svgame.globals->mapname = MAKE_STRING( sv.name );
	svgame.globals->startspot = MAKE_STRING( sv.startspot );
	svgame.globals->time = sv_time();

	// spawn the rest of the entities on the map
	SV_LoadFromFile( entities );
	Com_CloseScript( entities );

	MsgDev( D_NOTE, "Total %i entities spawned\n", svgame.numEntities );
}

void SV_UnloadProgs( void )
{
	SV_DeactivateServer ();
	Delta_Shutdown ();

	Mem_FreePool( &svgame.stringspool );

	if( svgame.dllFuncs2.pfnGameShutdown )
	{
		svgame.dllFuncs2.pfnGameShutdown ();
	}

	// now we can unload cvars
	Cvar_FullSet( "host_gameloaded", "0", CVAR_INIT );

	// must unlink all game cvars,
	// before pointers on them will be lost...
	Cmd_ExecuteString( "@unlink\n" );

	FS_FreeLibrary( svgame.hInstance );
	Mem_FreePool( &svgame.mempool );
	Mem_Set( &svgame, 0, sizeof( svgame ));
}

qboolean SV_LoadProgs( const char *name )
{
	int			i, version;
	static APIFUNCTION		GetEntityAPI;
	static APIFUNCTION2		GetEntityAPI2;
	static GIVEFNPTRSTODLL	GiveFnptrsToDll;
	static NEW_DLL_FUNCTIONS_FN	GiveNewDllFuncs;
	static enginefuncs_t	gpEngfuncs;
	static globalvars_t		gpGlobals;
	static playermove_t		gpMove;
	edict_t			*e;

	if( svgame.hInstance ) SV_UnloadProgs();

	// fill it in
	svgame.pmove = &gpMove;
	svgame.globals = &gpGlobals;
	svgame.mempool = Mem_AllocPool( "Server Edicts Zone" );
	svgame.hInstance = FS_LoadLibrary( name, true );
	if( !svgame.hInstance ) return false;

	// make sure what new dll functions is cleared
	Mem_Set( &svgame.dllFuncs2, 0, sizeof( svgame.dllFuncs2 ));

	// make local copy of engfuncs to prevent overwrite it with bots.dll
	Mem_Copy( &gpEngfuncs, &gEngfuncs, sizeof( gpEngfuncs ));

	GetEntityAPI = (APIFUNCTION)FS_GetProcAddress( svgame.hInstance, "GetEntityAPI" );
	GetEntityAPI2 = (APIFUNCTION2)FS_GetProcAddress( svgame.hInstance, "GetEntityAPI2" );
	GiveNewDllFuncs = (NEW_DLL_FUNCTIONS_FN)FS_GetProcAddress( svgame.hInstance, "GetNewDLLFunctions" );

	if( !GetEntityAPI )
	{
		FS_FreeLibrary( svgame.hInstance );
         		MsgDev( D_NOTE, "SV_LoadProgs: failed to get address of GetEntityAPI proc\n" );
		svgame.hInstance = NULL;
		return false;
	}

	GiveFnptrsToDll = (GIVEFNPTRSTODLL)FS_GetProcAddress( svgame.hInstance, "GiveFnptrsToDll" );

	if( !GiveFnptrsToDll )
	{
		FS_FreeLibrary( svgame.hInstance );
		MsgDev( D_NOTE, "SV_LoadProgs: failed to get address of GiveFnptrsToDll proc\n" );
		svgame.hInstance = NULL;
		return false;
	}

	GiveFnptrsToDll( &gpEngfuncs, svgame.globals );

	// get extended callbacks
	if( GiveNewDllFuncs )
	{
		version = NEW_DLL_FUNCTIONS_VERSION;
	
		if( !GiveNewDllFuncs( &svgame.dllFuncs2, &version ))
		{
			if( version != NEW_DLL_FUNCTIONS_VERSION )
				MsgDev( D_WARN, "SV_LoadProgs: new interface version %i should be %i\n", NEW_DLL_FUNCTIONS_VERSION, version );
			Mem_Set( &svgame.dllFuncs2, 0, sizeof( svgame.dllFuncs2 ));
		}
	}

	version = INTERFACE_VERSION;

	if( !GetEntityAPI( &svgame.dllFuncs, version ))
	{
		if( !GetEntityAPI2 )
		{
			FS_FreeLibrary( svgame.hInstance );
			MsgDev( D_ERROR, "SV_LoadProgs: couldn't get entity API\n" );
			svgame.hInstance = NULL;
			return false;
		}
		else if( !GetEntityAPI2( &svgame.dllFuncs, &version ))
		{
			FS_FreeLibrary( svgame.hInstance );
			MsgDev( D_ERROR, "SV_LoadProgs: interface version %i should be %i\n", INTERFACE_VERSION, version );
			svgame.hInstance = NULL;
			return false;
		}
	}

	if( !SV_InitStudioAPI( ))
	{
		FS_FreeLibrary( svgame.hInstance );
		MsgDev( D_ERROR, "SV_LoadProgs: couldn't get studio API\n" );
		svgame.hInstance = NULL;
		return false;
	}

	svgame.globals->pStringBase = ""; // setup string base

	svgame.globals->maxEntities = GI->max_edicts;
	svgame.globals->maxClients = sv_maxclients->integer;
	svgame.edicts = Mem_Alloc( svgame.mempool, sizeof( edict_t ) * svgame.globals->maxEntities );
	svgame.numEntities = svgame.globals->maxClients + 1; // clients + world
	for( i = 0, e = svgame.edicts; i < svgame.globals->maxEntities; i++, e++ )
		e->free = true; // mark all edicts as freed

	// clear user messages
	svgame.gmsgHudText = -1;

	Cvar_FullSet( "host_gameloaded", "1", CVAR_INIT );
	svgame.stringspool = Mem_AllocPool( "Server Strings" );

	// all done, initialize game
	svgame.dllFuncs.pfnGameInit();

	// initialize pm_shared
	SV_InitClientMove();

	Delta_Init ();

	// register custom encoders
	svgame.dllFuncs.pfnRegisterEncoders();

	// fire once
	MsgDev( D_INFO, "Dll loaded for mod %s\n", svgame.dllFuncs.pfnGetGameDescription() );

	return true;
}