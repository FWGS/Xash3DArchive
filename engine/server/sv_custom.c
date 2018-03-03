/*
sv_custom.c - downloading custom resources
Copyright (C) 2010 Uncle Mike

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
#include "server.h"

void SV_CreateCustomizationList( sv_client_t *cl )
{
	resource_t	*pResource;
	customization_t	*pList, *pCust;
	qboolean		bFound;
	int		nLumps;

	cl->customdata.pNext = NULL;

	for( pResource = cl->resourcesonhand.pNext; pResource != &cl->resourcesonhand; pResource = pResource->pNext )
	{
		bFound = false;

		for( pList = cl->customdata.pNext; pList != NULL; pList = pList->pNext )
		{
			if( !memcmp( pList->resource.rgucMD5_hash, pResource->rgucMD5_hash, 16 ))
			{
				bFound = true;
				break;
			}
		}

		if( !bFound )
		{
			nLumps = 0;

			if( COM_CreateCustomization( &cl->customdata, pResource, -1, FCUST_FROMHPAK|FCUST_WIPEDATA, &pCust, &nLumps ))
			{
				pCust->nUserData2 = nLumps;
				svgame.dllFuncs.pfnPlayerCustomization( cl->edict, pCust );
			}
			else
			{
				if( sv_allow_upload.value )
					Con_Printf( "Ignoring invalid custom decal from %s\n", cl->name );
				else Con_Printf( "Ignoring custom decal from %s\n", cl->name );
			}
		}
		else
		{
			MsgDev( D_REPORT, "SV_CreateCustomization list, ignoring dup. resource for player %s\n", cl->name );
		}
	}
}

qboolean SV_FileInConsistencyList( const char *filename, consistency_t **ppconsist )
{
	int	i;

	for( i = 0; i < MAX_MODELS && sv.consistency_list[i].filename != NULL; i++ )
	{
		if( !Q_stricmp( sv.consistency_list[i].filename, filename ) )
		{
			if( ppconsist != NULL )
				*ppconsist = &sv.consistency_list[i];
			return true;
		}
	}
	return false;
}

void SV_ParseConsistencyResponse( sv_client_t *cl, sizebuf_t *msg )
{
	int				idx;
	int				value;
	int				c;
	int				i;
	resource_t		*r;
	int				badresindex;
	vec3_t			mins, maxs;
	vec3_t			cmins, cmaxs;
	byte			readbuffer[32];
	byte			nullbuffer[32];
	byte			resbuffer[32];
	qboolean			invalid_type;
	FORCE_TYPE		ft;
	int			length;

	memset( nullbuffer, 0, sizeof( nullbuffer ));
	length = MSG_ReadShort( msg );
	invalid_type = false;
	badresindex = 0;
	c = 0;

	while( MSG_ReadOneBit( msg ))
	{
		idx = MSG_ReadUBitLong( msg, MAX_RESOURCE_BITS );

		if( idx < 0 || idx >= sv.num_resources )
			break;

		r = &sv.resources[idx];

		if( !FBitSet( r->ucFlags, RES_CHECKFILE ))
			break;

		memcpy( readbuffer, r->rguc_reserved, 32 );

		if( !memcmp( readbuffer, nullbuffer, 32 ))
		{
			value = MSG_ReadUBitLong( msg, 32 );

			// will be compare only first 4 bytes
			if( value != *(int *)r->rgucMD5_hash )
				badresindex = idx + 1;
		}
		else
		{
			MSG_ReadBytes( msg, cmins, sizeof( cmins ));
			MSG_ReadBytes( msg, cmaxs, sizeof( cmaxs ));

			memcpy( resbuffer, r->rguc_reserved, 32 );
			ft = resbuffer[0];

			switch( ft )
			{
			case force_model_samebounds:
				memcpy( mins, &resbuffer[0x01], sizeof( mins ));
				memcpy( maxs, &resbuffer[0x0D], sizeof( maxs ));

				if( !VectorCompare( cmins, mins ) || !VectorCompare( cmaxs, maxs ))
					badresindex = idx + 1;
				break;
			case force_model_specifybounds:
				memcpy( mins, &resbuffer[0x01], sizeof( mins ));
				memcpy( maxs, &resbuffer[0x0D], sizeof( maxs ));

				for( i = 0; i < 3; i++ )
				{
					if( cmins[i] < mins[i] || cmaxs[i] > maxs[i] )
					{
						badresindex = idx + 1;
						break;
					}
				}
				break;
			default:
				invalid_type = true;
				break;
			}
		}

		if( invalid_type )
			break;
		c++;
	}

	if( sv.num_consistency != c )
	{
		Con_Printf( "%s:%s sent bad file data\n", cl->name, NET_AdrToString( cl->netchan.remote_address ));
		SV_DropClient( cl );
		return;
	}

	if( badresindex != 0 )
	{
		char	dropmessage[256];

		dropmessage[0] = 0;
		if( svgame.dllFuncs.pfnInconsistentFile( cl->edict, sv.resources[badresindex - 1].szFileName, dropmessage ))
		{
			if( COM_CheckString( dropmessage ))
				SV_ClientPrintf( cl, dropmessage );
			SV_DropClient( cl );
		}
	}
	else
	{
		ClearBits( cl->flags, FCL_FORCE_UNMODIFIED );
	}
}

int SV_TransferConsistencyInfo( void )
{
	vec3_t		mins, maxs;
	int		i, total = 0;
	resource_t	*pResource;
	string		filepath;
	consistency_t	*pc;

	for( i = 0; i < sv.num_resources; i++ )
	{
		pResource = &sv.resources[i];

		if( FBitSet( pResource->ucFlags, RES_CHECKFILE ))
			continue;	// already checked?

		if( !SV_FileInConsistencyList( pResource->szFileName, &pc ))
			continue;

		SetBits( pResource->ucFlags, RES_CHECKFILE );

		if( pResource->type == t_sound )
			Q_snprintf( filepath, sizeof( filepath ), "sound/%s", pResource->szFileName );
		else Q_strncpy( filepath, pResource->szFileName, sizeof( filepath ));

		MD5_HashFile( pResource->rgucMD5_hash, filepath, NULL );

		if( pResource->type == t_model )
		{
			switch( pc->check_type )
			{
			case force_exactfile:
				// only MD5 hash compare
				break;
			case force_model_samebounds:
				if( !Mod_GetStudioBounds( filepath, mins, maxs ))
					Host_Error( "Mod_GetStudioBounds: couldn't get bounds for %s\n", filepath );
				memcpy( &pResource->rguc_reserved[0x01], mins, sizeof( mins ));
				memcpy( &pResource->rguc_reserved[0x0D], maxs, sizeof( maxs ));
				pResource->rguc_reserved[0] = pc->check_type;
				break;
			case force_model_specifybounds:
				memcpy( &pResource->rguc_reserved[0x01], pc->mins, sizeof( pc->mins ));
				memcpy( &pResource->rguc_reserved[0x0D], pc->maxs, sizeof( pc->maxs ));
				pResource->rguc_reserved[0] = pc->check_type;
				break;
			}
		}
		total++;
	}

	return total;
}

void SV_SendConsistencyList( sv_client_t *cl, sizebuf_t *msg )
{
	int	i, lastcheck;
	int	delta;

	if( svs.maxclients == 1 || !sv_consistency.value || !sv.num_consistency || FBitSet( cl->flags, FCL_HLTV_PROXY ))
	{
		ClearBits( cl->flags, FCL_FORCE_UNMODIFIED );
		MSG_WriteOneBit( msg, 0 );
		return;
	}

	SetBits( cl->flags, FCL_FORCE_UNMODIFIED );
	MSG_WriteOneBit( msg, 1 );
	lastcheck = 0;

	for( i = 0; i < sv.num_resources; i++ )
	{
		if( !FBitSet( sv.resources[i].ucFlags, RES_CHECKFILE ))
			continue;

		delta = i - lastcheck;
		MSG_WriteOneBit( msg, 1 );

		if( delta > 31 )
		{
			MSG_WriteOneBit( msg, 0 );
			MSG_WriteUBitLong( msg, i, MAX_RESOURCE_BITS );
		}
		else
		{
			MSG_WriteOneBit( msg, 1 );
			MSG_WriteUBitLong( msg, delta, 5 );
		}

		lastcheck = i;
	}

	// write end of the list
	MSG_WriteOneBit( msg, 0 );
}

qboolean SV_CheckFile( sizebuf_t *msg, const char *filename )
{
	resource_t	p;

	memset( &p, 0, sizeof( resource_t ));

	if( Q_strlen( filename ) == 36 && !Q_strnicmp( filename, "!MD5", 4 ))
	{
		COM_HexConvert( filename + 4, 32, p.rgucMD5_hash );

		if( HPAK_GetDataPointer( CUSTOM_RES_PATH, &p, NULL, NULL ))
			return true;
	}

	if( !sv_allow_upload.value )
		return true;

	MSG_WriteByte( msg, svc_stufftext );
	MSG_WriteString( msg, va( "upload \"!MD5%s\"\n", MD5_Print( p.rgucMD5_hash )));

	return false;
}

void SV_MoveToOnHandList( sv_client_t *cl, resource_t *pResource )
{
	if( !pResource )
	{
		MsgDev( D_REPORT, "Null resource passed to SV_MoveToOnHandList\n" );
		return;
	}

	SV_RemoveFromResourceList( pResource );
	SV_AddToResourceList( pResource, &cl->resourcesonhand );
}

void SV_AddToResourceList( resource_t *pResource, resource_t *pList )
{
	if( pResource->pPrev != NULL || pResource->pNext != NULL )
	{
		MsgDev( D_ERROR, "Resource already linked\n" );
		return;
	}

	pResource->pPrev = pList->pPrev;
	pResource->pNext = pList;
	pList->pPrev->pNext = pResource;
	pList->pPrev = pResource;
}

void SV_RemoveFromResourceList( resource_t *pResource )
{
	pResource->pPrev->pNext = pResource->pNext;
	pResource->pNext->pPrev = pResource->pPrev;
	pResource->pPrev = NULL;
	pResource->pNext = NULL;
}

void SV_ClearResourceList( resource_t *pList )
{
	resource_t *p;
	resource_t *n;

	for ( p = pList->pNext; pList != p && p; p = n )
	{
		n = p->pNext;

		SV_RemoveFromResourceList( p );
		Mem_Free( p );
	}

	pList->pPrev = pList;
	pList->pNext = pList;
}

int SV_EstimateNeededResources( sv_client_t *cl )
{
	int		missing = 0;
	int		size = 0;
	resource_t	*p;

	for( p = cl->resourcesneeded.pNext; p != &cl->resourcesneeded; p = p->pNext )
	{
		if( p->type != t_decal )
			continue;

		if( HPAK_ResourceForHash( CUSTOM_RES_PATH, p->rgucMD5_hash, NULL ))
		{
			if( p->nDownloadSize != 0 )
			{
				p->ucFlags |= RES_WASMISSING;
				size += p->nDownloadSize;
			}
			else
			{
				missing++;
			}
		}
	}

	return size;
}

void SV_BatchUploadRequest( sv_client_t *cl )
{
	string		filename;
	resource_t	*p, *n;

	for( p = cl->resourcesneeded.pNext; p != &cl->resourcesneeded; p = n )
	{
		n = p->pNext;

		if( !FBitSet( p->ucFlags, RES_WASMISSING ))
		{
			SV_MoveToOnHandList( cl, p );
			continue;
		}

		if( p->type == t_decal )
		{
			if( FBitSet( p->ucFlags, RES_CUSTOM ))
			{
				Q_snprintf( filename, sizeof( filename ) - 4, "!MD5%s", MD5_Print( p->rgucMD5_hash ));

				if( SV_CheckFile( &cl->netchan.message, filename ))
					SV_MoveToOnHandList( cl, p );
			}
			else
			{
				MsgDev( D_ERROR, "Non customization in upload queue!\n" );
				SV_MoveToOnHandList( cl, p );
			}
		}
	}
}
   
void SV_SendResources( sv_client_t *cl, sizebuf_t *msg )
{
	byte	nullrguc[36];
	int	i;

	memset( nullrguc, 0, sizeof( nullrguc ));

	MSG_WriteByte( msg, svc_resourcerequest );
	MSG_WriteLong( msg, svs.spawncount );
	MSG_WriteLong( msg, 0 );

	if( sv_downloadurl.string && sv_downloadurl.string[0] && Q_strlen( sv_downloadurl.string ) < 256 )
	{
		MSG_WriteByte( msg, svc_resourcelocation );
		MSG_WriteString( msg, sv_downloadurl.string );
	}

	MSG_BeginServerCmd( msg, svc_resourcelist );
	MSG_WriteUBitLong( msg, sv.num_resources, MAX_RESOURCE_BITS );

	for( i = 0; i < sv.num_resources; i++ )
	{
		MSG_WriteUBitLong( msg, sv.resources[i].type, 4 );
		MSG_WriteString( msg, sv.resources[i].szFileName );
		MSG_WriteUBitLong( msg, sv.resources[i].nIndex, MAX_MODEL_BITS );
		MSG_WriteUBitLong( msg, sv.resources[i].nDownloadSize, 24 );	// prevent to download a very big files?
		MSG_WriteUBitLong( msg, sv.resources[i].ucFlags & ( RES_FATALIFMISSING|RES_WASMISSING ), 3 );

		if( FBitSet( sv.resources[i].ucFlags, RES_CUSTOM ))
			MSG_WriteBytes( msg, sv.resources[i].rgucMD5_hash, sizeof( sv.resources[i].rgucMD5_hash ));

		if( memcmp( nullrguc, sv.resources[i].rguc_reserved, sizeof( nullrguc )))
		{
			MSG_WriteOneBit( msg, 1 );
			MSG_WriteBytes( msg, sv.resources[i].rguc_reserved, sizeof( sv.resources[i].rguc_reserved ));
		}
		else MSG_WriteOneBit( msg, 0 );
	}

	SV_SendConsistencyList( cl, msg );
}