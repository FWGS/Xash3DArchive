/*
library.c - custom dlls loader
Copyright (C) 2008 Uncle Mike

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
#include "library.h"

/*
---------------------------------------------------------------

		Name for function stuff

---------------------------------------------------------------
*/
static void FsGetString( file_t *f, char *str )
{
	char	ch;

	while(( ch = FS_Getc( f )) != EOF )
	{
		*str++ = ch;
		if( !ch ) break;
	}
}

static void FreeNameFuncGlobals( dll_user_t *hInst )
{
	int	i;

	if( !hInst ) return;

	if( hInst->ordinals ) Mem_Free( hInst->ordinals );
	if( hInst->funcs ) Mem_Free( hInst->funcs );

	for( i = 0; i < hInst->num_ordinals; i++ )
	{
		if( hInst->names[i] )
			Mem_Free( hInst->names[i] );
	}

	hInst->num_ordinals = 0;
	hInst->ordinals = NULL;
	hInst->funcs = NULL;
}

char *GetMSVCName( const char *in_name )
{
	char	*pos, *out_name;

	if( in_name[0] == '?' )  // is this a MSVC C++ mangled name?
	{
		if(( pos = Q_strstr( in_name, "@@" )) != NULL )
		{
			int	len = pos - in_name;

			// strip off the leading '?'
			out_name = copystring( in_name + 1 );
			out_name[len-1] = 0; // terminate string at the "@@"
			return out_name;
		}
	}
	return copystring( in_name );
}

qboolean LibraryLoadSymbols( dll_user_t *hInst )
{
	file_t		*f;
	string		errorstring;
	DOS_HEADER	dos_header;
	LONG		nt_signature;
	PE_HEADER		pe_header;
	SECTION_HEADER	section_header;
	qboolean		rdata_found;
	OPTIONAL_HEADER	optional_header;
	long		rdata_delta = 0;
	EXPORT_DIRECTORY	export_directory;
	long		name_offset;
	long		exports_offset;
	long		ordinal_offset;
	long		function_offset;
	string		function_name;
	dword		*p_Names = NULL;
	int		i, index;

	// can only be done for loaded libraries
	if( !hInst ) return false;

	for( i = 0; i < hInst->num_ordinals; i++ )
		hInst->names[i] = NULL;

	f = FS_Open( hInst->shortPath, "rb", false );
	if( !f )
	{
		Q_sprintf( errorstring, "couldn't load %s", hInst->shortPath );
		goto table_error;
	}

	if( FS_Read( f, &dos_header, sizeof( dos_header )) != sizeof( dos_header ))
	{
		Q_sprintf( errorstring, "%s has corrupted EXE header", hInst->shortPath );
		goto table_error;
	}

	if( dos_header.e_magic != DOS_SIGNATURE )
	{
		Q_sprintf( errorstring, "%s does not have a valid dll signature", hInst->shortPath );
		goto table_error;
	}

	if( FS_Seek( f, dos_header.e_lfanew, SEEK_SET ) == -1 )
	{
		Q_sprintf( errorstring, "%s error seeking for new exe header", hInst->shortPath );
		goto table_error;
	}

	if( FS_Read( f, &nt_signature, sizeof( nt_signature )) != sizeof( nt_signature ))
	{
		Q_sprintf( errorstring, "%s has corrupted NT header", hInst->shortPath );
		goto table_error;
	}

	if( nt_signature != NT_SIGNATURE )
	{
		Q_sprintf( errorstring, "%s does not have a valid NT signature", hInst->shortPath );
		goto table_error;
	}

	if( FS_Read( f, &pe_header, sizeof( pe_header )) != sizeof( pe_header ))
	{
		Q_sprintf( errorstring, "%s does not have a valid PE header", hInst->shortPath );
		goto table_error;
	}

	if( !pe_header.SizeOfOptionalHeader )
	{
		Q_sprintf( errorstring, "%s does not have an optional header", hInst->shortPath );
		goto table_error;
	}

	if( FS_Read( f, &optional_header, sizeof( optional_header )) != sizeof( optional_header ))
	{
		Q_sprintf( errorstring, "%s optional header probably corrupted", hInst->shortPath );
		goto table_error;
	}

	rdata_found = false;

	for( i = 0; i < pe_header.NumberOfSections; i++ )
	{
		if( FS_Read( f, &section_header, sizeof( section_header )) != sizeof( section_header ))
		{
			Q_sprintf( errorstring, "%s error during reading section header", hInst->shortPath );
			goto table_error;
		}

		if( !Q_strcmp( section_header.Name, ".rdata" ))
		{
			rdata_found = true;
			break;
		}
	}

	if( rdata_found )
	{
		rdata_delta = section_header.VirtualAddress - section_header.PointerToRawData; 
	}

	exports_offset = optional_header.DataDirectory[0].VirtualAddress - rdata_delta;

	if( FS_Seek( f, exports_offset, SEEK_SET ) == -1 )
	{
		Q_sprintf( errorstring, "%s does not have a valid exports section", hInst->shortPath );
		goto table_error;
	}

	if( FS_Read( f, &export_directory, sizeof( export_directory )) != sizeof( export_directory ))
	{
		Q_sprintf( errorstring, "%s does not have a valid optional header", hInst->shortPath );
		goto table_error;
	}

	hInst->num_ordinals = export_directory.NumberOfNames;	// also number of ordinals

	if( hInst->num_ordinals > MAX_LIBRARY_EXPORTS )
	{
		Q_sprintf( errorstring, "%s too many exports %i", hInst->shortPath, hInst->num_ordinals );
		hInst->num_ordinals = 0;
		goto table_error;
	}

	ordinal_offset = export_directory.AddressOfNameOrdinals - rdata_delta;

	if( FS_Seek( f, ordinal_offset, SEEK_SET ) == -1 )
	{
		Q_sprintf( errorstring, "%s does not have a valid ordinals section", hInst->shortPath );
		goto table_error;
	}

	hInst->ordinals = Mem_Alloc( host.mempool, hInst->num_ordinals * sizeof( word ));

	if( FS_Read( f, hInst->ordinals, hInst->num_ordinals * sizeof( word )) != (hInst->num_ordinals * sizeof( word )))
	{
		Q_sprintf( errorstring, "%s error during reading ordinals table", hInst->shortPath );
		goto table_error;
	}

	function_offset = export_directory.AddressOfFunctions - rdata_delta;

	if( FS_Seek( f, function_offset, SEEK_SET ) == -1 )
	{
		Q_sprintf( errorstring, "%s does not have a valid export address section", hInst->shortPath );
		goto table_error;
	}

	hInst->funcs = Mem_Alloc( host.mempool, hInst->num_ordinals * sizeof( dword ));

	if( FS_Read( f, hInst->funcs, hInst->num_ordinals * sizeof( dword )) != (hInst->num_ordinals * sizeof( dword )))
	{
		Q_sprintf( errorstring, "%s error during reading export address section", hInst->shortPath );
		goto table_error;
	}

	name_offset = export_directory.AddressOfNames - rdata_delta;

	if( FS_Seek( f, name_offset, SEEK_SET ) == -1 )
	{
		Q_sprintf( errorstring, "%s file does not have a valid names section", hInst->shortPath );
		goto table_error;
	}

	p_Names = Mem_Alloc( host.mempool, hInst->num_ordinals * sizeof( dword ));

	if( FS_Read( f, p_Names, hInst->num_ordinals * sizeof( dword )) != (hInst->num_ordinals * sizeof( dword )))
	{
		Q_sprintf( errorstring, "%s error during reading names table", hInst->shortPath );
		goto table_error;
	}

	for( i = 0; i < hInst->num_ordinals; i++ )
	{
		name_offset = p_Names[i] - rdata_delta;

		if( name_offset != 0 )
		{
			if( FS_Seek( f, name_offset, SEEK_SET ) != -1 )
			{
				FsGetString( f, function_name );
				hInst->names[i] = GetMSVCName( function_name );
			}
			else break;
		}
	}

	if( i != hInst->num_ordinals )
	{
		Q_sprintf( errorstring, "%s error during loading names section", hInst->shortPath );
		goto table_error;
	}
	FS_Close( f );

	for( i = 0; i < hInst->num_ordinals; i++ )
	{
		if( !Q_strcmp( "GiveFnptrsToDll", hInst->names[i] ))	// main entry point for user dlls
		{
			void	*fn_offset;

			index = hInst->ordinals[i];
			fn_offset = (void *)Com_GetProcAddress( hInst, "GiveFnptrsToDll" );
			hInst->funcBase = (dword)(fn_offset) - hInst->funcs[index];
			break;
		}
	}

	if( p_Names ) Mem_Free( p_Names );
	return true;
table_error:
	// cleanup
	if( f ) FS_Close( f );
	if( p_Names ) Mem_Free( p_Names );
	FreeNameFuncGlobals( hInst );
	MsgDev( D_ERROR, "LoadLibrary: %s\n", errorstring );

	return false;
}

/*
================
Com_LoadLibrary

smart dll loader - can loading dlls from pack or wad files
================
*/
void *Com_LoadLibraryExt( const char *dllname, int build_ordinals_table, qboolean directpath )
{
	dll_user_t *hInst;

	hInst = FS_FindLibrary( dllname, directpath );
	if( !hInst ) return NULL; // nothing to load
		
	if( hInst->custom_loader )
	{
          	if( hInst->encrypted )
			MsgDev( D_ERROR, "Sys_LoadLibrary: couldn't load encrypted library %s\n", dllname );
		else MsgDev( D_ERROR, "Sys_LoadLibrary: couldn't load library %s from packfile\n", dllname );
		return NULL;
	}
	else hInst->hInstance = LoadLibrary( hInst->fullPath );

	if( !hInst->hInstance )
	{
		MsgDev( D_NOTE, "Sys_LoadLibrary: Loading %s - failed\n", dllname );
		Com_FreeLibrary( hInst );
		return NULL;
	}

	// if not set - FunctionFromName and NameForFunction will not working
	if( build_ordinals_table )
	{
		if( !LibraryLoadSymbols( hInst ))
		{
			MsgDev( D_NOTE, "Sys_LoadLibrary: Loading %s - failed\n", dllname );
			Com_FreeLibrary( hInst );
			return NULL;
		}
	}
	MsgDev( D_NOTE, "Sys_LoadLibrary: Loading %s - ok\n", dllname );

	return hInst;
}

void *Com_LoadLibrary( const char *dllname, int build_ordinals_table )
{
	return Com_LoadLibraryExt( dllname, build_ordinals_table, false );
}

void *Com_GetProcAddress( void *hInstance, const char *name )
{
	dll_user_t *hInst = (dll_user_t *)hInstance;

	if( !hInst || !hInst->hInstance )
		return NULL;

	if( !hInst->custom_loader )
		return GetProcAddress( hInst->hInstance, name );
	return NULL;
}

void Com_FreeLibrary( void *hInstance )
{
	dll_user_t *hInst = (dll_user_t *)hInstance;

	if( !hInst || !hInst->hInstance )
		return; // already freed

	if( host.state == HOST_CRASHED )
	{
		// we need to hold down all modules, while MSVC can find error
		MsgDev( D_NOTE, "Sys_FreeLibrary: hold %s for debugging\n", hInst->dllName );
		return;
	}
	else MsgDev( D_NOTE, "Sys_FreeLibrary: Unloading %s\n", hInst->dllName );
	
	if( !hInst->custom_loader )
		FreeLibrary( hInst->hInstance );

	hInst->hInstance = NULL;

	if( hInst->num_ordinals )
		FreeNameFuncGlobals( hInst );
	Mem_Free( hInst );	// done
}

dword Com_FunctionFromName( void *hInstance, const char *pName )
{
	dll_user_t	*hInst = (dll_user_t *)hInstance;
	int		i, index;

	if( !hInst || !hInst->hInstance )
		return 0;

	for( i = 0; i < hInst->num_ordinals; i++ )
	{
		if( !Q_strcmp( pName, hInst->names[i] ))
		{
			index = hInst->ordinals[i];
			return hInst->funcs[index] + hInst->funcBase;
		}
	}
	// couldn't find the function name to return address
	return 0;
}

const char *Com_NameForFunction( void *hInstance, dword function )
{
	dll_user_t	*hInst = (dll_user_t *)hInstance;
	int		i, index;

	if( !hInst || !hInst->hInstance )
		return NULL;

	for( i = 0; i < hInst->num_ordinals; i++ )
	{
		index = hInst->ordinals[i];

		if(( function - hInst->funcBase ) == hInst->funcs[index] )
			return hInst->names[i];
	}
	// couldn't find the function address to return name
	return NULL;
}