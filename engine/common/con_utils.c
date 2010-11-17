//=======================================================================
//			Copyright XashXT Group 2008 �
//			con_utils.c - console helpers
//=======================================================================

#include "common.h"
#include "client.h"
#include "byteorder.h"
#include "const.h"
#include "bspfile.h"
#include "../cl_dll/kbutton.h"

#ifdef _DEBUG
void DBG_AssertFunction( qboolean fExpr, const char* szExpr, const char* szFile, int szLine, const char* szMessage )
{
	if( fExpr ) return;

	if( szMessage != NULL )
		MsgDev( at_error, "ASSERT FAILED:\n %s \n(%s@%d)\n%s", szExpr, szFile, szLine, szMessage );
	else MsgDev( at_error, "ASSERT FAILED:\n %s \n(%s@%d)\n", szExpr, szFile, szLine );
}
#endif	// DEBUG

/*
=======================================================================

			FILENAME AUTOCOMPLETION
=======================================================================
*/
/*
=====================================
Cmd_GetMapList

Prints or complete map filename
=====================================
*/
qboolean Cmd_GetMapList( const char *s, char *completedname, int length )
{
	search_t		*t;
	file_t		*f;
	string		message;
	string		matchbuf;
	byte		buf[MAX_SYSPATH]; // 1 kb
	int		i, nummaps;

	t = FS_Search( va( "maps/%s*.bsp", s ), true );
	if( !t ) return false;

	FS_FileBase( t->filenames[0], matchbuf ); 
	com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for( i = 0, nummaps = 0; i < t->numfilenames; i++ )
	{
		const char	*data = NULL;
		char		entfilename[CS_SIZE];
		int		ver = -1, lumpofs = 0, lumplen = 0;
		const char	*ext = FS_FileExtension( t->filenames[i] ); 
		script_t		*ents = NULL;
		qboolean		gearbox;
			
		if( com.stricmp( ext, "bsp" )) continue;
		com.strncpy( message, "^1error^7", sizeof( message ));
		f = FS_Open( t->filenames[i], "rb" );
	
		if( f )
		{
			dheader_t	*header;

			Mem_Set( buf, 0, MAX_SYSPATH );
			FS_Read( f, buf, MAX_SYSPATH );
			ver = LittleLong(*(uint *)buf);
                              
			switch( ver )
			{
			case Q1BSP_VERSION:
			case HLBSP_VERSION:
				header = (dheader_t *)buf;
				if( LittleLong( header->lumps[LUMP_PLANES].filelen ) % sizeof( dplane_t ))
				{
					lumpofs = LittleLong( header->lumps[LUMP_PLANES].fileofs );
					lumplen = LittleLong( header->lumps[LUMP_PLANES].filelen );
					gearbox = true;
				}
				else
				{
					lumpofs = LittleLong( header->lumps[LUMP_ENTITIES].fileofs );
					lumplen = LittleLong( header->lumps[LUMP_ENTITIES].filelen );
					gearbox = false;
				}
				break;
			}

			com.strncpy( entfilename, t->filenames[i], sizeof( entfilename ));
			FS_StripExtension( entfilename );
			FS_DefaultExtension( entfilename, ".ent" );
			ents = Com_OpenScript( entfilename, NULL, 0 );

			if( !ents && lumplen >= 10 )
			{
				char *entities = NULL;
		
				FS_Seek( f, lumpofs, SEEK_SET );
				entities = (char *)Mem_Alloc( host.mempool, lumplen + 1 );
				FS_Read( f, entities, lumplen );
				ents = Com_OpenScript( "ents", entities, lumplen + 1 );
				Mem_Free( entities ); // no reason to keep it
			}

			if( ents )
			{
				// if there are entities to parse, a missing message key just
				// means there is no title, so clear the message string now
				token_t	token;

				message[0] = 0;
				while( Com_ReadToken( ents, SC_ALLOW_NEWLINES|SC_ALLOW_PATHNAMES2, &token ))
				{
					if( !com.strcmp( token.string, "{" )) continue;
					else if(!com.strcmp( token.string, "}" )) break;
					else if(!com.strcmp( token.string, "message" ))
					{
						// get the message contents
						Com_ReadString( ents, SC_ALLOW_PATHNAMES2, message );
					}
				}
				Com_CloseScript( ents );
			}
		}

		if( f ) FS_Close(f);
		FS_FileBase( t->filenames[i], matchbuf );

		switch( ver )
		{
		case Q1BSP_VERSION:
			com.strncpy( buf, "Quake", sizeof( buf ));
			break;
		case HLBSP_VERSION:
			if( gearbox ) com.strncpy( buf, "Blue-Shift", sizeof( buf ));
			else com.strncpy( buf, "Half-Life", sizeof( buf ));
			break;
		default:	com.strncpy( buf, "??", sizeof( buf )); break;
		}
		Msg( "%16s (%s) ^3%s^7\n", matchbuf, buf, message );
		nummaps++;
	}
	Msg( "\n^3 %i maps found.\n", nummaps );
	Mem_Free( t );

	// cut shortestMatch to the amount common with s
	for( i = 0; matchbuf[i]; i++ )
	{
		if(com.tolower( completedname[i] ) != com.tolower( matchbuf[i] ))
			completedname[i] = 0;
	}
	return true;
}

/*
=====================================
Cmd_GetFontList

Prints or complete font filename
=====================================
*/
qboolean Cmd_GetFontList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numfonts;

	t = FS_Search( va( "gfx/fonts/%s*.*", s ), true);
	if( !t ) return false;

	FS_FileBase( t->filenames[0], matchbuf ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for( i = 0, numfonts = 0; i < t->numfilenames; i++ )
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp( ext, "png" ) && com.stricmp( ext, "dds" ) && com.stricmp( ext, "tga" ))
			continue;
		FS_FileBase( t->filenames[i], matchbuf );
		Msg( "%16s\n", matchbuf );
		numfonts++;
	}
	Msg( "\n^3 %i fonts found.\n", numfonts );
	Mem_Free( t );

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(com.tolower( completedname[i] ) != com.tolower( matchbuf[i] ))
				completedname[i] = 0;
		}
	}
	return true;
}

/*
=====================================
Cmd_GetDemoList

Prints or complete demo filename
=====================================
*/
qboolean Cmd_GetDemoList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numdems;

	t = FS_SearchExt( va( "demos/%s*.dem", s ), true, true );	// lookup only in gamedir
	if( !t ) return false;

	FS_FileBase( t->filenames[0], matchbuf ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for( i = 0, numdems = 0; i < t->numfilenames; i++ )
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp( ext, "dem" )) continue;
		FS_FileBase( t->filenames[i], matchbuf );
		Msg( "%16s\n", matchbuf );
		numdems++;
	}
	Msg( "\n^3 %i demos found.\n", numdems );
	Mem_Free( t );

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if( com.tolower( completedname[i] ) != com.tolower( matchbuf[i] ))
				completedname[i] = 0;
		}
	}
	return true;
}

/*
=====================================
Cmd_GetMovieList

Prints or complete movie filename
=====================================
*/
qboolean Cmd_GetMovieList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, nummovies;

	t = FS_Search( va( "media/%s*.avi", s ), true );
	if( !t ) return false;

	FS_FileBase( t->filenames[0], matchbuf ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for(i = 0, nummovies = 0; i < t->numfilenames; i++)
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp( ext, "avi" )) continue;
		FS_FileBase( t->filenames[i], matchbuf );
		Msg( "%16s\n", matchbuf );
		nummovies++;
	}
	Msg( "\n^3 %i movies found.\n", nummovies );
	Mem_Free( t );

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if( com.tolower( completedname[i] ) != com.tolower( matchbuf[i] ))
				completedname[i] = 0;
		}
	}

	return true;
}

/*
=====================================
Cmd_GetMusicList

Prints or complete background track filename
=====================================
*/
qboolean Cmd_GetMusicList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numtracks;

	t = FS_Search( va( "media/%s*.*", s ), true );
	if( !t ) return false;

	FS_FileBase( t->filenames[0], matchbuf ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for(i = 0, numtracks = 0; i < t->numfilenames; i++)
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( !com.stricmp( ext, "wav" ) || !com.stricmp( ext, "ogg" ));
		else continue;

		FS_FileBase( t->filenames[i], matchbuf );
		Msg( "%16s\n", matchbuf );
		numtracks++;
	}
	Msg( "\n^3 %i soundtracks found.\n", numtracks );
	Mem_Free(t);

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if( com.tolower( completedname[i]) != com.tolower( matchbuf[i] ))
				completedname[i] = 0;
		}
	}
	return true;
}

/*
=====================================
Cmd_GetSavesList

Prints or complete savegame filename
=====================================
*/
qboolean Cmd_GetSavesList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numsaves;

	t = FS_SearchExt( va( "save/%s*.sav", s ), true, true );	// lookup only in gamedir
	if( !t ) return false;

	FS_FileBase( t->filenames[0], matchbuf ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for( i = 0, numsaves = 0; i < t->numfilenames; i++ )
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp( ext, "sav" )) continue;
		FS_FileBase( t->filenames[i], matchbuf );
		Msg( "%16s\n", matchbuf );
		numsaves++;
	}
	Msg( "\n^3 %i saves found.\n", numsaves );
	Mem_Free( t );

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if( com.tolower( completedname[i] ) != com.tolower( matchbuf[i] ))
				completedname[i] = 0;
		}
	}

	return true;
}

/*
=====================================
Cmd_GetConfigList

Prints or complete .cfg filename
=====================================
*/
qboolean Cmd_GetConfigList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numconfigs;

	t = FS_Search( va( "%s*.cfg", s ), true );
	if( !t ) return false;

	FS_FileBase( t->filenames[0], matchbuf ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for( i = 0, numconfigs = 0; i < t->numfilenames; i++ )
	{
		const char *ext = FS_FileExtension( t->filenames[i] );

		if( com.stricmp( ext, "cfg" )) continue;
		FS_FileBase( t->filenames[i], matchbuf );
		Msg( "%16s\n", matchbuf );
		numconfigs++;
	}
	Msg( "\n^3 %i configs found.\n", numconfigs );
	Mem_Free( t );

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if( com.tolower( completedname[i] ) != com.tolower( matchbuf[i] ))
				completedname[i] = 0;
		}
	}

	return true;
}

/*
=====================================
Cmd_GetSoundList

Prints or complete sound filename
=====================================
*/
qboolean Cmd_GetSoundList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numsounds;
	const char	*snddir = "sound/"; // constant

	t = FS_Search( va( "%s%s*.*", snddir, s ), true );
	if( !t ) return false;

	com.strncpy( matchbuf, t->filenames[0] + com.strlen( snddir ), MAX_STRING ); 
	FS_StripExtension( matchbuf ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for(i = 0, numsounds = 0; i < t->numfilenames; i++)
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( !com.stricmp( ext, "wav" ) || !com.stricmp( ext, "ogg" ));
		else continue;

		com.strncpy( matchbuf, t->filenames[i] + com.strlen(snddir), MAX_STRING ); 
		FS_StripExtension( matchbuf );
		Msg( "%16s\n", matchbuf );
		numsounds++;
	}
	Msg( "\n^3 %i sounds found.\n", numsounds );
	Mem_Free( t );

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(com.tolower(completedname[i]) != com.tolower(matchbuf[i]))
				completedname[i] = 0;
		}
	}

	return true;
}

qboolean Cmd_GetStringTablesList( const char *s, char *completedname, int length )
{
	int	i, numtables;
	string	tables[MAX_STRING_TABLES];
	string	matchbuf;
	const char *name;

	// compare gamelist with current keyword
	for( i = 0, numtables = 0; i < MAX_STRING_TABLES; i++ )
	{
		name = StringTable_GetName( i );
		if( name && ( *s == '*' || !com.strnicmp( name, s, com.strlen( s ))))
			com.strcpy( tables[numtables++], name ); 
	}
	if( !numtables ) return false;

	com.strncpy( matchbuf, tables[0], MAX_STRING ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( numtables == 1 ) return true;

	for( i = 0; i < numtables; i++ )
	{
		com.strncpy( matchbuf, tables[i], MAX_STRING ); 
		Msg("%16s\n", matchbuf );
	}
	Msg( "\n^3 %i stringtables found.\n", numtables );

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if( com.tolower( completedname[i]) != com.tolower( matchbuf[i] ))
				completedname[i] = 0;
		}
	}
	return true;
}

/*
=====================================
Cmd_GetItemsList

Prints or complete item classname
=====================================
*/
qboolean Cmd_GetItemsList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numitems;

	if( !clgame.itemspath[0] ) return false; // not in game yet
	t = FS_Search( va( "%s/%s*.txt", clgame.itemspath, s ), true );
	if( !t ) return false;

	FS_FileBase( t->filenames[0], matchbuf ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for(i = 0, numitems = 0; i < t->numfilenames; i++)
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp(ext, "txt" )) continue;
		FS_FileBase( t->filenames[i], matchbuf );
		Msg( "%16s\n", matchbuf );
		numitems++;
	}
	Msg("\n^3 %i items found.\n", numitems );
	Mem_Free( t );

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(com.tolower(completedname[i]) != com.tolower(matchbuf[i]))
				completedname[i] = 0;
		}
	}
	return true;
}

/*
=====================================
Cmd_GetCustomList

Prints or complete .HPK filenames
=====================================
*/
qboolean Cmd_GetCustomList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numitems;

	if( !clgame.itemspath[0] ) return false; // not in game yet
	t = FS_Search( va( "%s*.hpk", s ), true );
	if( !t ) return false;

	FS_FileBase( t->filenames[0], matchbuf ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for(i = 0, numitems = 0; i < t->numfilenames; i++)
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp( ext, "hpk" )) continue;
		FS_FileBase( t->filenames[i], matchbuf );
		Msg( "%16s\n", matchbuf );
		numitems++;
	}

	Msg( "\n^3 %i items found.\n", numitems );
	Mem_Free( t );

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if( com.tolower( completedname[i] ) != com.tolower( matchbuf[i] ))
				completedname[i] = 0;
		}
	}
	return true;
}

/*
=====================================
Cmd_GetTexturemodes

Prints or complete sound filename
=====================================
*/
qboolean Cmd_GetTexturemodes( const char *s, char *completedname, int length )
{
	int	i, numtexturemodes;
	string	texturemodes[6];	// keep an actual ( sizeof( gl_texturemode) / sizeof( gl_texturemode[0] ))
	string	matchbuf;

	const char *gl_texturemode[] =
	{
	"GL_LINEAR",
	"GL_LINEAR_MIPMAP_LINEAR",
	"GL_LINEAR_MIPMAP_NEAREST",
	"GL_NEAREST",
	"GL_NEAREST_MIPMAP_LINEAR",
	"GL_NEAREST_MIPMAP_NEAREST",
	};

	// compare gamelist with current keyword
	for( i = 0, numtexturemodes = 0; i < 6; i++ )
	{
		if(( *s == '*' ) || !com.strnicmp( gl_texturemode[i], s, com.strlen( s )))
			com.strcpy( texturemodes[numtexturemodes++], gl_texturemode[i] ); 
	}

	if( !numtexturemodes ) return false;
	com.strncpy( matchbuf, gl_texturemode[0], MAX_STRING ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( numtexturemodes == 1 ) return true;

	for( i = 0; i < numtexturemodes; i++ )
	{
		com.strncpy( matchbuf, texturemodes[i], MAX_STRING ); 
		Msg( "%16s\n", matchbuf );
	}
	Msg("\n^3 %i filters found.\n", numtexturemodes );

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if( com.tolower( completedname[i] ) != com.tolower( matchbuf[i] ))
				completedname[i] = 0;
		}
	}
	return true;
}

/*
=====================================
Cmd_GetGameList

Prints or complete gamedir name
=====================================
*/
qboolean Cmd_GetGamesList( const char *s, char *completedname, int length )
{
	int	i, numgamedirs;
	string	gamedirs[MAX_MODS];
	string	matchbuf;

	// compare gamelist with current keyword
	for( i = 0, numgamedirs = 0; i < SI->numgames; i++ )
	{
		if(( *s == '*' ) || !com.strnicmp( SI->games[i]->gamefolder, s, com.strlen( s )))
			com.strcpy( gamedirs[numgamedirs++], SI->games[i]->gamefolder ); 
	}

	if( !numgamedirs ) return false;
	com.strncpy( matchbuf, gamedirs[0], MAX_STRING ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( numgamedirs == 1) return true;

	for( i = 0; i < numgamedirs; i++ )
	{
		com.strncpy( matchbuf, gamedirs[i], MAX_STRING ); 
		Msg("%16s\n", matchbuf );
	}
	Msg("\n^3 %i games found.\n", numgamedirs );

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(com.tolower(completedname[i]) != com.tolower(matchbuf[i]))
				completedname[i] = 0;
		}
	}
	return true;
}

qboolean Cmd_CheckMapsList_R( qboolean fRefresh, qboolean onlyingamedir )
{
	byte	buf[MAX_SYSPATH];
	char	*buffer;
	string	result;
	int	i, size;
	search_t	*t;
	file_t	*f;

	if( FS_FileSizeEx( "maps.lst", onlyingamedir ) > 0 && !fRefresh )
		return true; // exist 

	t = FS_SearchExt( "maps/*.bsp", false, onlyingamedir );
	if( !t ) return false;

	buffer = Mem_Alloc( host.mempool, t->numfilenames * 2 * sizeof( result ));
	for( i = 0; i < t->numfilenames; i++ )
	{
		script_t		*ents = NULL;
		int		ver = -1, lumpofs = 0, lumplen = 0;
		string		mapname, message, entfilename;
		const char	*ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp( ext, "bsp" )) continue;
		f = FS_OpenEx( t->filenames[i], "rb", onlyingamedir );
		FS_FileBase( t->filenames[i], mapname );

		if( f )
		{
			int	num_spawnpoints = 0;
			dheader_t	*header;

			Mem_Set( buf, 0, MAX_SYSPATH );
			FS_Read( f, buf, MAX_SYSPATH );
			ver = LittleLong(*(uint *)buf);
                              
			switch( ver )
			{
			case Q1BSP_VERSION:
			case HLBSP_VERSION:
				header = (dheader_t *)buf;
				if( LittleLong( header->lumps[LUMP_PLANES].filelen ) % sizeof( dplane_t ))
				{
					lumpofs = LittleLong( header->lumps[LUMP_PLANES].fileofs );
					lumplen = LittleLong( header->lumps[LUMP_PLANES].filelen );
				}
				else
				{
					lumpofs = LittleLong( header->lumps[LUMP_ENTITIES].fileofs );
					lumplen = LittleLong( header->lumps[LUMP_ENTITIES].filelen );
				}
				break;
			}

			com.strncpy( entfilename, t->filenames[i], sizeof( entfilename ));
			FS_StripExtension( entfilename );
			FS_DefaultExtension( entfilename, ".ent" );
			ents = Com_OpenScript( entfilename, NULL, 0 );

			if( !ents && lumplen >= 10 )
			{
				char *entities = NULL;
		
				FS_Seek( f, lumpofs, SEEK_SET );
				entities = (char *)Mem_Alloc( host.mempool, lumplen + 1 );
				FS_Read( f, entities, lumplen );
				ents = Com_OpenScript( "ents", entities, lumplen + 1 );
				Mem_Free( entities ); // no reason to keep it
			}

			if( ents )
			{
				// if there are entities to parse, a missing message key just
				// means there is no title, so clear the message string now
				token_t	token;
				qboolean	worldspawn = true;

				message[0] = 0;
				com.strncpy( message, "No Title", MAX_STRING );

				while( Com_ReadToken( ents, SC_ALLOW_NEWLINES|SC_ALLOW_PATHNAMES2, &token ))
				{
					if( token.string[0] == '}' && worldspawn )
						worldspawn = false;
					else if( !com.strcmp( token.string, "message" ) && worldspawn )
					{
						// get the message contents
						Com_ReadString( ents, SC_ALLOW_PATHNAMES2, message );
					}
					else if( !com.strcmp( token.string, "classname" ))
					{
						Com_ReadToken( ents, SC_ALLOW_PATHNAMES2, &token );
						if( !com.strcmp( token.string, GI->mp_entity ))
							num_spawnpoints++;
					}
					if( num_spawnpoints ) break; // valid map
				}
				Com_CloseScript( ents );
			}

			if( f ) FS_Close( f );

			if( num_spawnpoints )
			{
				// format: mapname "maptitle"\n
				com.sprintf( result, "%s \"%s\"\n", mapname, message );
				com.strcat( buffer, result ); // add new string
			}
		}
	}
	if( t ) Mem_Free( t ); // free search result

	size = com.strlen( buffer );
	if( !size && onlyingamedir )
	{
          	if( buffer ) Mem_Free( buffer );
		return Cmd_CheckMapsList_R( fRefresh, false );
	}

	// write generated maps.lst
	if( FS_WriteFile( "maps.lst", buffer, com.strlen( buffer )))
	{
          	if( buffer ) Mem_Free( buffer );
		return true;
	}
	return false;
}

qboolean Cmd_CheckMapsList( qboolean fRefresh )
{
	return Cmd_CheckMapsList_R( fRefresh, true );
}

autocomplete_list_t cmd_list[] =
{
{ "gl_texturemode", Cmd_GetTexturemodes },
{ "stringlist", Cmd_GetStringTablesList },
{ "changelevel", Cmd_GetMapList },
{ "playdemo", Cmd_GetDemoList, },
{ "menufont", Cmd_GetFontList, },
{ "setfont", Cmd_GetFontList, },
{ "hpkval", Cmd_GetCustomList },
{ "music", Cmd_GetMusicList, },
{ "movie", Cmd_GetMovieList },
{ "exec", Cmd_GetConfigList },
{ "give", Cmd_GetItemsList },
{ "drop", Cmd_GetItemsList },
{ "game", Cmd_GetGamesList },
{ "save", Cmd_GetSavesList },
{ "load", Cmd_GetSavesList },
{ "play", Cmd_GetSoundList },
{ "map", Cmd_GetMapList },
{ NULL }, // termiantor
};

/*
============
Cmd_WriteVariables

Appends lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/

static void Cmd_WriteCvar(const char *name, const char *string, const char *desc, void *f )
{
	if( !desc || !*desc ) return; // ignore cvars without description (fantom variables)
	FS_Printf(f, "seta %s \"%s\"\n", name, string );
}

static void Cmd_WriteServerCvar(const char *name, const char *string, const char *desc, void *f )
{
	if( !desc || !*desc ) return; // ignore cvars without description (fantom variables)
	FS_Printf(f, "set %s \"%s\"\n", name, string );
}

static void Cmd_WriteOpenGLCvar( const char *name, const char *string, const char *desc, void *f )
{
	if( !desc || !*desc ) return; // ignore cvars without description (fantom variables)
	FS_Printf( f, "setr %s \"%s\"\n", name, string );
}

static void Cmd_WriteHelp(const char *name, const char *unused, const char *desc, void *f )
{
	if( !desc ) return;				// ignore fantom cmds
	if( !com.strcmp( desc, "" )) return;		// blank description
	if( name[0] == '+' || name[0] == '-' ) return;	// key bindings	
	FS_Printf( f, "%s\t\t\t\"%s\"\n", name, desc );
}

void Cmd_WriteVariables( file_t *f )
{
	FS_Printf( f, "unsetall\n" );
	Cvar_LookupVars( CVAR_ARCHIVE, NULL, f, Cmd_WriteCvar ); 
}

void Cmd_WriteServerVariables( file_t *f )
{
	Cvar_LookupVars( CVAR_SERVERNOTIFY, NULL, f, Cmd_WriteServerCvar ); 
}

void Cmd_WriteOpenGLVariables( file_t *f )
{
	Cvar_LookupVars( CVAR_RENDERINFO, NULL, f, Cmd_WriteOpenGLCvar ); 
}

/*
===============
Host_WriteConfig

Writes key bindings and archived cvars to config.cfg
===============
*/
void Host_WriteConfig( void )
{
	file_t	*f;
	kbutton_t	*mlook, *jlook;

	if( !cls.initialized ) return;

	f = FS_Open( "config.cfg", "w" );
	if( f )
	{
		FS_Printf( f, "//=======================================================================\n");
		FS_Printf( f, "//\t\t\tCopyright XashXT Group %s �\n", timestamp( TIME_YEAR_ONLY ));
		FS_Printf( f, "//\t\t\tconfig.cfg - archive of cvars\n" );
		FS_Printf( f, "//=======================================================================\n" );
		Key_WriteBindings( f );
		Cmd_WriteVariables( f );

		mlook = (kbutton_t *)clgame.dllFuncs.KB_Find( "in_mlook" );
		jlook = (kbutton_t *)clgame.dllFuncs.KB_Find( "in_jlook" );

		if( mlook && ( mlook->state & 1 )) 
			FS_Printf( f, "+mlook\n" );

		if( jlook && ( jlook->state & 1 ))
			FS_Printf( f, "+jlook\n" );

		FS_Close( f );
	}
	else MsgDev( D_ERROR, "Couldn't write config.cfg.\n" );
}

/*
===============
Host_WriteServerConfig

save serverinfo variables into server.cfg (using for dedicated server too)
===============
*/
void Host_WriteServerConfig( const char *name )
{
	file_t	*f;

	SV_InitGameProgs();	// collect user variables
	
	if(( f = FS_Open( name, "w" )) != NULL )
	{
		FS_Printf( f, "//=======================================================================\n" );
		FS_Printf( f, "//\t\t\tCopyright XashXT Group %s �\n", timestamp( TIME_YEAR_ONLY ));
		FS_Printf( f, "//\t\t\tserver.cfg - server temporare config\n" );
		FS_Printf( f, "//=======================================================================\n" );
		Cmd_WriteServerVariables( f );
		FS_Close( f );
	}
	else MsgDev( D_ERROR, "Couldn't write %s.\n", name );
}


/*
===============
Host_WriteOpenGLConfig

save renderinfo variables into opengl.cfg
===============
*/
void Host_WriteOpenGLConfig( void )
{
	file_t	*f;

	f = FS_Open( "opengl.cfg", "w" );
	if( f )
	{
		FS_Printf( f, "//=======================================================================\n" );
		FS_Printf( f, "//\t\t\tCopyright XashXT Group %s �\n", timestamp( TIME_YEAR_ONLY ));
		FS_Printf( f, "//\t\t    opengl.cfg - archive of opengl extension cvars\n");
		FS_Printf( f, "//=======================================================================\n" );
		Cmd_WriteOpenGLVariables( f );
		FS_Close( f );	
	}                                                
	else MsgDev( D_ERROR, "can't update opengl.cfg.\n" );
}

void Key_EnumCmds_f( void )
{
	file_t	*f;

	FS_AllowDirectPaths( true );
	if( FS_FileExists( "../help.txt" ))
	{
		Msg( "help.txt already exist\n" );
		FS_AllowDirectPaths( false );
		return;
	}

	f = FS_Open( "../help.txt", "w" );
	if( f )
	{
		FS_Printf( f, "//=======================================================================\n");
		FS_Printf( f, "//\t\t\tCopyright XashXT Group %s �\n", timestamp( TIME_YEAR_ONLY ));
		FS_Printf( f, "//\t\thelp.txt - xash commands and console variables\n");
		FS_Printf( f, "//=======================================================================\n");

		FS_Printf( f, "\n\n\t\t\tconsole variables\n\n");
		Cvar_LookupVars( 0, NULL, f, Cmd_WriteHelp ); 
		FS_Printf( f, "\n\n\t\t\tconsole commands\n\n");
		Cmd_LookupCmds( NULL, f, Cmd_WriteHelp ); 
  		FS_Printf( f, "\n\n");
		FS_Close( f );
		Msg( "help.txt created\n" );
	}
	else MsgDev( D_ERROR, "Couldn't write help.txt.\n");
	FS_AllowDirectPaths( false );
}