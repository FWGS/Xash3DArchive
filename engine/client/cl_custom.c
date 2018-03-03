/*
cl_custom.c - downloading custom resources
Copyright (C) 2018 Uncle Mike

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
#include "client.h"
#include "net_encode.h"

qboolean CL_CheckFile( sizebuf_t *msg, const char *filename )
{
	string	filepath;

	Q_strncpy( filepath, filename, sizeof( filepath ));

	if( Q_strstr( filepath, ".." ) || Q_strstr( filepath, "server.cfg" ))
	{
		MsgDev( D_REPORT, "refusing to download a path with '..'\n" );
		return true;
	}

	if( !cl_allow_download.value )
	{
		MsgDev( D_REPORT, "Download refused, cl_allow_download is 0\n" );
		return true;
	}

	if( cls.state == ca_active && !cl_download_ingame.value )
	{
		MsgDev( D_REPORT, "In-game download refused...\n" );
		return true;
	}

	if( FS_FileExists( filepath, false ))
		return true;

	if( cls.demoplayback )
	{
		MsgDev( D_WARN, "file %s missing during demo playback.\n", filepath );
		return true;
	}
#if 0
	// TODO: implement
	if( CL_CanUseHTTPDownload( ))
	{
		CL_QueueHTTPDownload( filepath );
		return false;
	}
#endif
	MSG_WriteByte( msg, clc_stringcmd );
	MSG_WriteString( msg, va( "dlfile %s", filepath ));
	return false;
}

void CL_AddToResourceList( resource_t *pResource, resource_t *pList )
{
	if( pResource->pPrev != NULL || pResource->pNext != NULL )
	{
		MsgDev( D_ERROR, "Resource already linked\n" );
		return;
	}

	if( pList->pPrev == NULL || pList->pNext == NULL )
		Host_Error( "Resource list corrupted.\n" );

	pResource->pPrev = pList->pPrev;
	pResource->pNext = pList;
	pList->pPrev->pNext = pResource;
	pList->pPrev = pResource;
}

void CL_RemoveFromResourceList( resource_t *pResource )
{
	if( pResource->pPrev == NULL || pResource->pNext == NULL )
		Host_Error( "mislinked resource in CL_RemoveFromResourceList\n" );

	if ( pResource->pNext == pResource || pResource->pPrev == pResource )
		Host_Error( "attempt to free last entry in list.\n" );

	pResource->pPrev->pNext = pResource->pNext;
	pResource->pNext->pPrev = pResource->pPrev;
	pResource->pPrev = NULL;
	pResource->pNext = NULL;
}

void CL_MoveToOnHandList( resource_t *pResource )
{
	if( !pResource )
	{
		MsgDev( D_REPORT, "Null resource passed to CL_MoveToOnHandList\n" );
		return;
	}

	CL_RemoveFromResourceList( pResource );
	CL_AddToResourceList( pResource, &cl.resourcesonhand );
}

void CL_ClearResourceList( resource_t *pList )
{
	resource_t	*p, *n;

	for( p = pList->pNext; p != pList && p; p = n )
	{
		n = p->pNext;

		CL_RemoveFromResourceList( p );
		Mem_Free( p );
	}

	pList->pPrev = pList;
	pList->pNext = pList;
}

void CL_ClearResourceLists( void )
{
	CL_ClearResourceList( &cl.resourcesneeded );
	CL_ClearResourceList( &cl.resourcesonhand );
}