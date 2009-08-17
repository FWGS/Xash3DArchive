/* -------------------------------------------------------------------------------

Copyright (C) 1999-2007 id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

----------------------------------------------------------------------------------

This code has been altered significantly from its original form, to support
several games based on the Quake III Arena engine, in the form of "Q3Map2."

------------------------------------------------------------------------------- */



/* marker */
#define SURFACE_EXTRA_C



/* dependencies */
#include "q3map2.h"



/* -------------------------------------------------------------------------------

ydnar: srf file module

------------------------------------------------------------------------------- */

typedef struct surfaceExtra_s
{
	mapDrawSurface_t		*mds;
	shaderInfo_t		*si;
	int			parentSurfaceNum;
	int			entityNum;
	int			castShadows, recvShadows;
	int			sampleSize;
	float			longestCurve;
	vec3_t			lightmapAxis;
}
surfaceExtra_t;

#define GROW_SURFACE_EXTRAS	1024

int							numSurfaceExtras = 0;
int							maxSurfaceExtras = 0;
surfaceExtra_t				*surfaceExtras;
surfaceExtra_t				seDefault = { NULL, NULL, -1, 0, WORLDSPAWN_CAST_SHADOWS, WORLDSPAWN_RECV_SHADOWS, 0, 0, { 0, 0, 0 } };



/*
AllocSurfaceExtra()
allocates a new extra storage
*/

static surfaceExtra_t *AllocSurfaceExtra( void )
{
	surfaceExtra_t	*se;
	
	
	/* enough space? */
	if( numSurfaceExtras >= maxSurfaceExtras )
	{
		/* reallocate more room */
		maxSurfaceExtras += GROW_SURFACE_EXTRAS;
		se = Malloc( maxSurfaceExtras * sizeof( surfaceExtra_t ) );
		if( surfaceExtras != NULL )
		{
			Mem_Copy( se, surfaceExtras, numSurfaceExtras * sizeof( surfaceExtra_t ) );
			Mem_Free( surfaceExtras );
		}
		surfaceExtras = se;
	}
	
	/* add another */
	se = &surfaceExtras[ numSurfaceExtras ];
	numSurfaceExtras++;
	Mem_Copy( se, &seDefault, sizeof( surfaceExtra_t ) );
	
	/* return it */
	return se;
}



/*
SetDefaultSampleSize()
sets the default lightmap sample size
*/

void SetDefaultSampleSize( int sampleSize )
{
	seDefault.sampleSize = sampleSize;
}



/*
SetSurfaceExtra()
stores extra (q3map2) data for the specific numbered drawsurface
*/

void SetSurfaceExtra( mapDrawSurface_t *ds, int num )
{
	surfaceExtra_t	*se;
	
	
	/* dummy check */
	if( ds == NULL || num < 0 )
		return;
	
	/* get a new extra */
	se = AllocSurfaceExtra();
	
	/* copy out the relevant bits */
	se->mds = ds;
	se->si = ds->shaderInfo;
	se->parentSurfaceNum = ds->parent != NULL ? ds->parent->outputNum : -1;
	se->entityNum = ds->entityNum;
	se->castShadows = ds->castShadows;
	se->recvShadows = ds->recvShadows;
	se->sampleSize = ds->sampleSize;
	se->longestCurve = ds->longestCurve;
	VectorCopy( ds->lightmapAxis, se->lightmapAxis );
	
	/* debug code */
	//%	MsgDev( D_NOTE, "SetSurfaceExtra(): entityNum = %d\n", ds->entityNum );
}



/*
GetSurfaceExtra*()
getter functions for extra surface data
*/

static surfaceExtra_t *GetSurfaceExtra( int num )
{
	if( num < 0 || num >= numSurfaceExtras )
		return &seDefault;
	return &surfaceExtras[ num ];
}


shaderInfo_t *GetSurfaceExtraShaderInfo( int num )
{
	surfaceExtra_t	*se = GetSurfaceExtra( num );
	return se->si;
}


int GetSurfaceExtraParentSurfaceNum( int num )
{
	surfaceExtra_t	*se = GetSurfaceExtra( num );
	return se->parentSurfaceNum;
}


int GetSurfaceExtraEntityNum( int num )
{
	surfaceExtra_t	*se = GetSurfaceExtra( num );
	return se->entityNum;
}


int GetSurfaceExtraCastShadows( int num )
{
	surfaceExtra_t	*se = GetSurfaceExtra( num );
	return se->castShadows;
}


int GetSurfaceExtraRecvShadows( int num )
{
	surfaceExtra_t	*se = GetSurfaceExtra( num );
	return se->recvShadows;
}


int GetSurfaceExtraSampleSize( int num )
{
	surfaceExtra_t	*se = GetSurfaceExtra( num );
	return se->sampleSize;
}


float GetSurfaceExtraLongestCurve( int num )
{
	surfaceExtra_t	*se = GetSurfaceExtra( num );
	return se->longestCurve;
}


void GetSurfaceExtraLightmapAxis( int num, vec3_t lightmapAxis )
{
	surfaceExtra_t	*se = GetSurfaceExtra( num );
	VectorCopy( se->lightmapAxis, lightmapAxis );
}




/*
WriteSurfaceExtraFile()
writes out a surface info file (<map>.srf)
*/

void WriteSurfaceExtraFile( const char *path )
{
	char			srfPath[ 1024 ];
	file_t			*sf;
	surfaceExtra_t	*se;
	int				i;
	
	
	/* dummy check */
	if( path == NULL || path[ 0 ] == '\0' )
		return;
	
	/* note it */
	MsgDev( D_NOTE, "--- WriteSurfaceExtraFile ---\n" );
	
	/* open the file */
	com.strcpy( srfPath, path );
	FS_StripExtension( srfPath );
	com.strcat( srfPath, ".srf" );
	Msg( "Writing %s\n", srfPath );
	sf = FS_Open( srfPath, "w" );
	if( sf == NULL ) Sys_Error( "Error opening %s for writing", srfPath );
	
	/* lap through the extras list */
	for( i = -1; i < numSurfaceExtras; i++ )
	{
		/* get extra */
		se = GetSurfaceExtra( i );
		
		/* default or surface num? */
		if( i < 0 )
			FS_Printf( sf, "default" );
		else
			FS_Printf( sf, "%d", i );
		
		/* valid map drawsurf? */
		if( se->mds == NULL )
			FS_Printf( sf, "\n" );
		else
		{
			FS_Printf( sf, " // %s V: %d I: %d %s\n",
				surfaceTypes[ se->mds->type ],
				se->mds->numVerts,
				se->mds->numIndexes,
				(se->mds->planar ? "planar" : "") );
		}
		
		/* open braces */
		FS_Printf( sf, "{\n" );
		
			/* shader */
			if( se->si != NULL )
				FS_Printf( sf, "\tshader %s\n", se->si->shader );
			
			/* parent surface number */
			if( se->parentSurfaceNum != seDefault.parentSurfaceNum )
				FS_Printf( sf, "\tparent %d\n", se->parentSurfaceNum );
			
			/* entity number */
			if( se->entityNum != seDefault.entityNum )
				FS_Printf( sf, "\tentity %d\n", se->entityNum );
			
			/* cast shadows */
			if( se->castShadows != seDefault.castShadows || se == &seDefault )
				FS_Printf( sf, "\tcastShadows %d\n", se->castShadows );
			
			/* recv shadows */
			if( se->recvShadows != seDefault.recvShadows || se == &seDefault )
				FS_Printf( sf, "\treceiveShadows %d\n", se->recvShadows );
			
			/* lightmap sample size */
			if( se->sampleSize != seDefault.sampleSize || se == &seDefault )
				FS_Printf( sf, "\tsampleSize %d\n", se->sampleSize );
			
			/* longest curve */
			if( se->longestCurve != seDefault.longestCurve || se == &seDefault )
				FS_Printf( sf, "\tlongestCurve %f\n", se->longestCurve );
			
			/* lightmap axis vector */
			if( VectorCompare( se->lightmapAxis, seDefault.lightmapAxis ) == false )
				FS_Printf( sf, "\tlightmapAxis ( %f %f %f )\n", se->lightmapAxis[ 0 ], se->lightmapAxis[ 1 ], se->lightmapAxis[ 2 ] );
		
		/* close braces */
		FS_Printf( sf, "}\n\n" );
	}
	
	/* close the file */
	FS_Close( sf );
}



/*
LoadSurfaceExtraFile()
reads a surface info file (<map>.srf)
*/

void LoadSurfaceExtraFile( const char *path )
{
	char			srfPath[ 1024 ];
	surfaceExtra_t	*se;
	int				surfaceNum, size;
	byte			*buffer;
	
	
	/* dummy check */
	if( path == NULL || path[ 0 ] == '\0' )
		return;
	
	/* load the file */
	com.strcpy( srfPath, path );
	FS_StripExtension( srfPath );
	com.strcat( srfPath, ".srf" );
	Msg( "Loading %s\n", srfPath );
	buffer = (void *)FS_LoadFile( srfPath, &size );
	if( size <= 0 )
	{
		MsgDev( D_WARN, "Unable to find surface file %s, using defaults.\n", srfPath );
		return;
	}
	
	/* parse the file */
	ParseFromMemory( buffer, size );
	
	/* tokenize it */
	while( 1 )
	{
		/* test for end of file */
		if( !GetToken( true ) )
			break;
		
		/* default? */
		if( !com.stricmp( token, "default" ) )
			se = &seDefault;

		/* surface number */
		else
		{
			surfaceNum = atoi( token );
			if( surfaceNum < 0 || surfaceNum > MAX_MAP_DRAW_SURFS )
				Sys_Error( "ReadSurfaceExtraFile(): %s, line %d: bogus surface num %d", srfPath, scriptline, surfaceNum );
			while( surfaceNum >= numSurfaceExtras )
				se = AllocSurfaceExtra();
			se = &surfaceExtras[ surfaceNum ];
		}
		
		/* handle { } section */
		if( !GetToken( true ) || strcmp( token, "{" ) )
			Sys_Error( "ReadSurfaceExtraFile(): %s, line %d: { not found", srfPath, scriptline );
		while( 1 )
		{
			if( !GetToken( true ) )
				break;
			if( !strcmp( token, "}" ) )
				break;
			
			/* shader */
			if( !com.stricmp( token, "shader" ) )
			{
				GetToken( false );
				se->si = ShaderInfoForShader( token );
			}
			
			/* parent surface number */
			else if( !com.stricmp( token, "parent" ) )
			{
				GetToken( false );
				se->parentSurfaceNum = atoi( token );
			}
			
			/* entity number */
			else if( !com.stricmp( token, "entity" ) )
			{
				GetToken( false );
				se->entityNum = atoi( token );
			}
			
			/* cast shadows */
			else if( !com.stricmp( token, "castShadows" ) )
			{
				GetToken( false );
				se->castShadows = atoi( token );
			}
			
			/* recv shadows */
			else if( !com.stricmp( token, "receiveShadows" ) )
			{
				GetToken( false );
				se->recvShadows = atoi( token );
			}
			
			/* lightmap sample size */
			else if( !com.stricmp( token, "sampleSize" ) )
			{
				GetToken( false );
				se->sampleSize = atoi( token );
			}
			
			/* longest curve */
			else if( !com.stricmp( token, "longestCurve" ) )
			{
				GetToken( false );
				se->longestCurve = atof( token );
			}
			
			/* lightmap axis vector */
			else if( !com.stricmp( token, "lightmapAxis" ) )
				Parse1DMatrix( 3, se->lightmapAxis );
			
			/* ignore all other tokens on the line */
			while( TokenAvailable() )
				GetToken( false );
		}
	}
	
	/* free the buffer */
	Mem_Free( buffer );
}

