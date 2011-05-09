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

//=======================================================================
//
//			UNDER CONSTRUCTION
//
//=======================================================================

typedef struct
{
	char	*filename;
	int	num_items;
	file_t	*file;	// pointer to wadfile
} cachewad_t;

int CustomDecal_Init( cachewad_t *wad, byte *data, int size, int playernum )
{
	// TODO: implement
	return 0;
}

void *CustomDecal_Validate( byte *data, int size )
{
	// TODO: implement
	return NULL;
}

int SV_CreateCustomization( customization_t *pListHead, resource_t *pResource, int playernumber, int flags,
	customization_t **pCustomization, int *nLumps )
{
	customization_t	*pRes;
	cachewad_t	*pldecal;
	qboolean		found_problem;

	found_problem = 0;

	ASSERT( pResource != NULL );

	if( pCustomization != NULL )
		*pCustomization = NULL;

	pRes = Z_Malloc( sizeof( customization_t ));
	pRes->resource = *pResource;

	if( pResource->nDownloadSize <= 0 )
	{
		found_problem = true;
	}
	else
	{
		pRes->bInUse = true;

		if(( flags & FCUST_FROMHPAK ) && !HPAK_GetDataPointer( "custom.hpk", pResource, (byte **)&(pRes->pBuffer), NULL ))
		{
			found_problem = true;
		}
		else
		{
			pRes->pBuffer = FS_LoadFile( pResource->szFileName, NULL, false );

			if(!( pRes->resource.ucFlags & RES_CUSTOM ) || pRes->resource.type != t_decal )
			{
				// break, except we're not in a loop
			}
			else
			{
				pRes->resource.playernum = playernumber;

				if( !CustomDecal_Validate( pRes->pBuffer, pResource->nDownloadSize ))
				{
					found_problem = true;
				}
				else if( flags & RES_CUSTOM )
				{
				}
				else
				{
					pRes->pInfo = Z_Malloc( sizeof( cachewad_t ));
					pldecal = pRes->pInfo;

					if( pResource->nDownloadSize < 1024 || pResource->nDownloadSize > 16384 )
					{
						found_problem = true;
					}
					else if( !CustomDecal_Init( pldecal, pRes->pBuffer, pResource->nDownloadSize, playernumber ))
					{
						found_problem = true;
					}
					else if( pldecal->num_items <= 0 )
					{
						found_problem = true;
					}
					else
					{
						if( nLumps ) *nLumps = pldecal->num_items;

						pRes->bTranslated = 1;
						pRes->nUserData1 = 0;
						pRes->nUserData2 = pldecal->num_items;

						if( flags & FCUST_WIPEDATA )
						{
							Mem_Free( pldecal->filename );
							FS_Close( pldecal->file );
							Mem_Free( pldecal );
							pRes->pInfo = NULL;
						}
					}
				}
			}
		}
	}

	if( !found_problem )
	{
		if( pCustomization ) *pCustomization = pRes;

		pRes->pNext = pListHead->pNext;
		pListHead->pNext = pRes;
	}
	else
	{
		if( pRes->pBuffer ) Mem_Free( pRes->pBuffer );
		if( pRes->pInfo ) Mem_Free( pRes->pInfo );
		Mem_Free( pRes );
	}

	return !found_problem;
}

void SV_CreateCustomizationList( sv_client_t *cl )
{
	resource_t	*pRes;
	customization_t	*pCust, *pNewCust;
	int		duplicated, lump_count;

	cl->customization.pNext = NULL;

	for( pRes = cl->resource1.pNext; pRes != &cl->resource1; pRes = pRes->pNext )
	{
		duplicated = false;

		for( pCust = cl->customization.pNext; pCust != NULL; pCust = pCust->pNext )
		{
			if( !memcmp( pCust->resource.rgucMD5_hash, pRes->rgucMD5_hash, 16 ))
			{
				duplicated = true;
				break;
			}
		}

		if( duplicated )
		{
			MsgDev( D_WARN, "CreateCustomizationList: duplicate resource detected.\n" );
			continue;
		}

		// create it.
		lump_count = 0;

		if( !SV_CreateCustomization( &cl->customization, pRes, -1, 3, &pNewCust, &lump_count ))
		{
			MsgDev( D_WARN, "CreateCustomizationList: ignoring custom decal.\n" );
			continue;
		}

		pNewCust->nUserData2 = lump_count;
		svgame.dllFuncs.pfnPlayerCustomization( cl->edict, pNewCust );
	}
}

void SV_ClearCustomizationList( customization_t *pHead )
{
	customization_t	*pNext, *pCur;
	cachewad_t	*wad;

	if( !pHead || !pHead->pNext )
		return;

	pCur = pHead->pNext;

	do
	{
		pNext = pCur->pNext;

		if( pCur->bInUse && pCur->pBuffer )
			Mem_Free( pCur->pBuffer );

		if( pCur->bInUse && pCur->pInfo )
		{
			if( pCur->resource.type == t_decal )
			{
				wad = (cachewad_t *)pCur->pInfo; 
				Mem_Free( wad->filename );
				FS_Close( wad->file );
			}

			Mem_Free( pCur->pInfo );
		}

		Mem_Free( pCur );
		pCur = pNext;
	} while( pCur != NULL );

	pCur->pNext = NULL;
}