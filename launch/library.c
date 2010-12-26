//=======================================================================
//			Copyright XashXT Group 2008 ©
//			library.c - custom dlls loader
//=======================================================================

#include "launch.h"
#include "library.h"

static const byte dosdata[0x100] =	// dos header (string 'This program cannot be run in DOS mode', etc)
"\x0e\x1f\xba\x0e\x00\xb4\x09\xcd\x21\xb8\x01\x4c\xcd\x21\x54\x68"
"\x69\x73\x20\x70\x72\x6f\x67\x72\x61\x6d\x20\x63\x61\x6e\x6e\x6f"
"\x74\x20\x62\x65\x20\x72\x75\x6e\x20\x69\x6e\x20\x44\x4f\x53\x20"
"\x6d\x6f\x64\x65\x2e\x0d\x0d\x0a\x24\x00\x00\x00\x00\x00\x00\x00"
"\xdb\xd6\xcc\x61\x9f\xb7\xa2\x32\x9f\xb7\xa2\x32\x9f\xb7\xa2\x32"
"\xe4\xab\xae\x32\x97\xb7\xa2\x32\xf0\xa8\xa9\x32\x90\xb7\xa2\x32"
"\x1c\xab\xac\x32\xae\xb7\xa2\x32\xf0\xa8\xa8\x32\x31\xb7\xa2\x32"
"\xc0\x95\xa8\x32\x9e\xb7\xa2\x32\x65\x93\xbb\x32\x9d\xb7\xa2\x32"
"\xc0\x95\xa9\x32\xb1\xb7\xa2\x32\x18\xab\xa0\x32\xb9\xb7\xa2\x32"
"\x70\x95\x92\x32\x9e\xb7\xa2\x32\x9f\xb7\xa3\x32\x6c\xb7\xa2\x32"
"\xfd\xa8\xb1\x32\x8e\xb7\xa2\x32\xe1\x95\xbe\x32\x9c\xb7\xa2\x32"
"\xac\x95\x87\x32\x9b\xb7\xa2\x32\xcb\x94\x93\x32\xab\xb7\xa2\x32"
"\xcb\x94\x92\x32\xf2\xb7\xa2\x32\x58\xb1\xa4\x32\x9e\xb7\xa2\x32"
"\x9f\xb7\xa2\x32\x80\xb7\xa2\x32\x60\x97\xa6\x32\x8c\xb7\xa2\x32"
"\x52\x69\x63\x68\x9f\xb7\xa2\x32\x00\x00\x00\x00\x00\x00\x00\x00"
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
        
/*
---------------------------------------------------------------

		Custom dlls loader

---------------------------------------------------------------
*/

typedef struct
{
	PIMAGE_NT_HEADERS	headers;
	byte		*codeBase;
	void		**modules;
	int		numModules;
	int		initialized;
} MEMORYMODULE, *PMEMORYMODULE;

// Protection flags for memory pages (Executable, Readable, Writeable)
static int ProtectionFlags[2][2][2] =
{
{
{ PAGE_NOACCESS, PAGE_WRITECOPY },		// not executable
{ PAGE_READONLY, PAGE_READWRITE },
},
{
{ PAGE_EXECUTE, PAGE_EXECUTE_WRITECOPY },	// executable
{ PAGE_EXECUTE_READ, PAGE_EXECUTE_READWRITE },
},
};

static FIXED_SECTIONS FixedSections[] =
{
{ ".text",  IMAGE_SCN_CNT_CODE|IMAGE_SCN_MEM_EXECUTE|IMAGE_SCN_MEM_READ },
{ ".rdata", IMAGE_SCN_CNT_INITIALIZED_DATA|IMAGE_SCN_MEM_READ },
{ ".data",  IMAGE_SCN_CNT_INITIALIZED_DATA|IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_WRITE },
{ ".rsrc",  IMAGE_SCN_CNT_INITIALIZED_DATA|IMAGE_SCN_MEM_DISCARDABLE|IMAGE_SCN_MEM_READ },
{ NULL,     0 }
};

typedef BOOL (WINAPI *DllEntryProc)( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved );

#define GET_HEADER_DICTIONARY( module, idx )	&(module)->headers->OptionalHeader.DataDirectory[idx]
#define CALCULATE_ADDRESS( base, offset )	(((DWORD)(base)) + (offset))

static void CopySections( const byte *data, PIMAGE_NT_HEADERS old_headers, PMEMORYMODULE module )
{
	int i, size;
	byte *dest;
	byte *codeBase = module->codeBase;
	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION( module->headers );

	for( i = 0; i < module->headers->FileHeader.NumberOfSections; i++, section++ )
	{
		if( section->SizeOfRawData == 0 )
		{
			// section doesn't contain data in the dll itself, but may define
			// uninitialized data
			size = old_headers->OptionalHeader.SectionAlignment;
			if( size > 0 )
			{
				dest = (byte *)VirtualAlloc((byte *)CALCULATE_ADDRESS(codeBase, section->VirtualAddress), size, MEM_COMMIT, PAGE_READWRITE );
				section->Misc.PhysicalAddress = (DWORD)dest;
				Mem_Set( dest, 0, size );
			}
			// section is empty
			continue;
		}

		// commit memory block and copy data from dll
		dest = (byte *)VirtualAlloc((byte *)CALCULATE_ADDRESS(codeBase, section->VirtualAddress), section->SizeOfRawData, MEM_COMMIT, PAGE_READWRITE );
		Mem_Copy( dest, (byte *)CALCULATE_ADDRESS(data, section->PointerToRawData), section->SizeOfRawData );
		section->Misc.PhysicalAddress = (DWORD)dest;
	}
}

static void FreeSections( PIMAGE_NT_HEADERS old_headers, PMEMORYMODULE module )
{
	int	i, size;
	byte	*codeBase = module->codeBase;
	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(module->headers);

	for( i = 0; i < module->headers->FileHeader.NumberOfSections; i++, section++ )
	{
		if( section->SizeOfRawData == 0 )
		{
			size = old_headers->OptionalHeader.SectionAlignment;
			if( size > 0 )
			{
				VirtualFree( codeBase + section->VirtualAddress, size, MEM_DECOMMIT );
				section->Misc.PhysicalAddress = 0;
			}
			continue;
		}

		VirtualFree( codeBase + section->VirtualAddress, section->SizeOfRawData, MEM_DECOMMIT );
		section->Misc.PhysicalAddress = 0;
	}
}

static void FinalizeSections( MEMORYMODULE *module )
{
	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION( module->headers );
	int	i;
	
	// loop through all sections and change access flags
	for( i = 0; i < module->headers->FileHeader.NumberOfSections; i++, section++ )
	{
		DWORD	protect, oldProtect, size;
		int	executable = (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
		int	readable = (section->Characteristics & IMAGE_SCN_MEM_READ) != 0;
		int	writeable = (section->Characteristics & IMAGE_SCN_MEM_WRITE) != 0;

		if( section->Characteristics & IMAGE_SCN_MEM_DISCARDABLE )
		{
			// section is not needed any more and can safely be freed
			VirtualFree((LPVOID)section->Misc.PhysicalAddress, section->SizeOfRawData, MEM_DECOMMIT);
			continue;
		}

		// determine protection flags based on characteristics
		protect = ProtectionFlags[executable][readable][writeable];
		if( section->Characteristics & IMAGE_SCN_MEM_NOT_CACHED )
			protect |= PAGE_NOCACHE;

		// determine size of region
		size = section->SizeOfRawData;
		if( size == 0 )
		{
			if( section->Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA )
				size = module->headers->OptionalHeader.SizeOfInitializedData;
			else if( section->Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA )
				size = module->headers->OptionalHeader.SizeOfUninitializedData;
		}

		if( size > 0 )
		{         
			// change memory access flags
			if( !VirtualProtect((LPVOID)section->Misc.PhysicalAddress, size, protect, &oldProtect ))
				Sys_Error( "Com_FinalizeSections: error protecting memory page\n" );
		}
	}
}

static void PerformBaseRelocation( MEMORYMODULE *module, DWORD delta )
{
	DWORD	i;
	byte	*codeBase = module->codeBase;
	PIMAGE_DATA_DIRECTORY directory = GET_HEADER_DICTIONARY( module, IMAGE_DIRECTORY_ENTRY_BASERELOC );

	if( directory->Size > 0 )
	{
		PIMAGE_BASE_RELOCATION relocation = (PIMAGE_BASE_RELOCATION)CALCULATE_ADDRESS( codeBase, directory->VirtualAddress );
		for( ; relocation->VirtualAddress > 0; )
		{
			byte	*dest = (byte *)CALCULATE_ADDRESS( codeBase, relocation->VirtualAddress );
			word	*relInfo = (word *)((byte *)relocation + IMAGE_SIZEOF_BASE_RELOCATION );

			for( i = 0; i<((relocation->SizeOfBlock-IMAGE_SIZEOF_BASE_RELOCATION) / 2); i++, relInfo++ )
			{
				DWORD	*patchAddrHL;
				int	type, offset;

				// the upper 4 bits define the type of relocation
				type = *relInfo >> 12;
				// the lower 12 bits define the offset
				offset = *relInfo & 0xfff;
				
				switch( type )
				{
				case IMAGE_REL_BASED_ABSOLUTE:
					// skip relocation
					break;
				case IMAGE_REL_BASED_HIGHLOW:
					// change complete 32 bit address
					patchAddrHL = (DWORD *)CALCULATE_ADDRESS( dest, offset );
					*patchAddrHL += delta;
					break;
				default:
					MsgDev( D_ERROR, "PerformBaseRelocation: unknown relocation: %d\n", type );
					break;
				}
			}

			// advance to next relocation block
			relocation = (PIMAGE_BASE_RELOCATION)CALCULATE_ADDRESS( relocation, relocation->SizeOfBlock );
		}
	}
}

static FARPROC MemoryGetProcAddress( void *module, const char *name )
{
	int	idx = -1;
	DWORD	i, *nameRef;
	WORD	*ordinal;
	PIMAGE_EXPORT_DIRECTORY exports;
	byte	*codeBase = ((PMEMORYMODULE)module)->codeBase;
	PIMAGE_DATA_DIRECTORY directory = GET_HEADER_DICTIONARY((MEMORYMODULE *)module, IMAGE_DIRECTORY_ENTRY_EXPORT );

	if( directory->Size == 0 )
	{
		// no export table found
		return NULL;
	}

	exports = (PIMAGE_EXPORT_DIRECTORY)CALCULATE_ADDRESS( codeBase, directory->VirtualAddress );
	if( exports->NumberOfNames == 0 || exports->NumberOfFunctions == 0 )
	{
		// DLL doesn't export anything
		return NULL;
	}

	// search function name in list of exported names
	nameRef = (DWORD *)CALCULATE_ADDRESS( codeBase, exports->AddressOfNames );
	ordinal = (WORD *)CALCULATE_ADDRESS( codeBase, exports->AddressOfNameOrdinals );
	for( i = 0; i < exports->NumberOfNames; i++, nameRef++, ordinal++ )
	{
		// GetProcAddress case insensative ?????
		if( !com.stricmp( name, (const char *)CALCULATE_ADDRESS( codeBase, *nameRef )))
		{
			idx = *ordinal;
			break;
		}
	}

	if( idx == -1 )
	{
		// exported symbol not found
		return NULL;
	}

	if((DWORD)idx > exports->NumberOfFunctions )
	{
		// name <-> ordinal number don't match
		return NULL;
	}

	// addressOfFunctions contains the RVAs to the "real" functions
	return (FARPROC)CALCULATE_ADDRESS( codeBase, *(DWORD *)CALCULATE_ADDRESS( codeBase, exports->AddressOfFunctions + (idx * 4)));
}

static int BuildImportTable( MEMORYMODULE *module )
{
	int	result=1;
	byte	*codeBase = module->codeBase;
	PIMAGE_DATA_DIRECTORY directory = GET_HEADER_DICTIONARY( module, IMAGE_DIRECTORY_ENTRY_IMPORT );

	if( directory->Size > 0 )
	{
		PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)CALCULATE_ADDRESS( codeBase, directory->VirtualAddress );

		for( ; !IsBadReadPtr( importDesc, sizeof( IMAGE_IMPORT_DESCRIPTOR )) && importDesc->Name; importDesc++ )
		{
			DWORD	*thunkRef, *funcRef;
			LPCSTR	libname;
			void	*handle;

			libname = (LPCSTR)CALCULATE_ADDRESS( codeBase, importDesc->Name );
			handle = Com_LoadLibraryExt( libname, false, true );

			if( handle == NULL )
			{
				MsgDev( D_ERROR, "couldn't load library %s\n", libname );
				result = 0;
				break;
			}

			module->modules = (void *)Mem_Realloc( Sys.basepool, module->modules, (module->numModules + 1) * (sizeof( void* )));
			module->modules[module->numModules++] = handle;

			if( importDesc->OriginalFirstThunk )
			{
				thunkRef = (DWORD *)CALCULATE_ADDRESS( codeBase, importDesc->OriginalFirstThunk );
				funcRef = (DWORD *)CALCULATE_ADDRESS( codeBase, importDesc->FirstThunk );
			}
			else
			{
				// no hint table
				thunkRef = (DWORD *)CALCULATE_ADDRESS( codeBase, importDesc->FirstThunk );
				funcRef = (DWORD *)CALCULATE_ADDRESS( codeBase, importDesc->FirstThunk );
			}

			for( ; *thunkRef; thunkRef++, funcRef++ )
			{
				if( IMAGE_SNAP_BY_ORDINAL( *thunkRef ))
				{
					LPCSTR	funcName = (LPCSTR)IMAGE_ORDINAL( *thunkRef );
					*funcRef = (DWORD)Com_GetProcAddress( handle, funcName );
				}
				else
				{
					PIMAGE_IMPORT_BY_NAME thunkData = (PIMAGE_IMPORT_BY_NAME)CALCULATE_ADDRESS( codeBase, *thunkRef );
					LPCSTR	funcName = (LPCSTR)&thunkData->Name;
					*funcRef = (DWORD)Com_GetProcAddress( handle, funcName );
				}

				if( *funcRef == 0 )
				{
					result = 0;
					break;
				}
			}
			if( !result ) break;
		}
	}
	return result;
}

static void MemoryFreeLibrary( void *hInstance )
{
	MEMORYMODULE	*module = (MEMORYMODULE *)hInstance;

	if( module != NULL )
	{
		int	i;
	
		if( module->initialized != 0 )
		{
			// notify library about detaching from process
			DllEntryProc DllEntry = (DllEntryProc)CALCULATE_ADDRESS( module->codeBase, module->headers->OptionalHeader.AddressOfEntryPoint );
			(*DllEntry)((HINSTANCE)module->codeBase, DLL_PROCESS_DETACH, 0 );
			module->initialized = 0;
		}

		if( module->modules != NULL )
		{
			// free previously opened libraries
			for( i = 0; i < module->numModules; i++ )
			{
				if( module->modules[i] != NULL )
				{
					Com_FreeLibrary( module->modules[i] );
				}
			}
			Mem_Free( module->modules ); // Mem_Realloc end
		}

		FreeSections( module->headers, module );

		if( module->codeBase != NULL )
		{
			// release memory of library
			VirtualFree( module->codeBase, 0, MEM_RELEASE );
		}
		HeapFree( GetProcessHeap(), 0, module );
	}
}

static void ImageFindTables( byte *base, DWORD baseoff, DWORD imagebase, DWORD *impoff, DWORD *impsz, DWORD *expoff, DWORD *expsz, DWORD *iatoff, DWORD *iatsz )
{
	IMAGE_IMPORT_DESCRIPTOR	*iid;
	IMAGE_THUNK_DATA		*itd;
	IMAGE_EXPORT_DIRECTORY	*ied;
	DWORD			off, maxiat, maxoff;
	byte			*p;
	int			i;

	*iatoff = 0xffffffff;
	maxiat = maxoff = 0;

	for( iid = (IMAGE_IMPORT_DESCRIPTOR *)(base + ( *impoff - baseoff )); iid->Name; iid++ )
	{
		if( iid->Name > maxoff )
			maxoff = iid->Name;

		if( iid->FirstThunk < *iatoff )
			*iatoff = iid->FirstThunk;

		for( itd = (IMAGE_THUNK_DATA *)(base + iid->FirstThunk - ( baseoff - imagebase )); itd->u1.AddressOfData; itd++ )
		{
			off = (((byte *)( itd + 1 ) - base ) + ( baseoff - imagebase )) + 4;
			if( off > maxiat ) maxiat = off;

			if( !IMAGE_SNAP_BY_ORDINAL( itd->u1.Ordinal ))
			{
				off = itd->u1.Ordinal + 2;
				if( off > maxoff ) maxoff = off;
			}
		}
	}

	iid++;
	*impsz = (byte *)iid - ( base + ( *impoff - baseoff ));

	*iatsz = maxiat - *iatoff;
	*iatoff += imagebase;

	for( p = base + maxoff - (baseoff - imagebase); *p; p++ );

	for( p++; !*p; p++ );	// we get the timestamp value of the export directory
	p -= ( p - base ) & 3;	// simple check of the alignment, enough
	p -= 4;			// skip the characteristics for finding the export table
	*expoff = (p - base) + baseoff;

	ied = (IMAGE_EXPORT_DIRECTORY *)p;

	for( itd = (IMAGE_THUNK_DATA *)( base + ied->AddressOfNames - ( baseoff - imagebase )), i = 0; i < ied->NumberOfNames; itd++, i++ )
	{
		if( !IMAGE_SNAP_BY_ORDINAL( itd->u1.Ordinal ))
		{
			off = itd->u1.Ordinal;
			if( off > maxoff ) maxoff = off;
		}
	}

	for( p = base + maxoff - ( baseoff - imagebase ); *p; p++ );
	for( p++; ( p - base ) & 3; p++ );
	*expsz = (( p - base ) + baseoff ) - *expoff;

	// print some stats
	MsgDev( D_NOTE, "DecryptImage: import table: %08x of %s\n", *impoff - baseoff, com_pretifymem( *impsz, 2 ));
	MsgDev( D_NOTE, "DecryptImage: export table: %08x of %s\n", *expoff - baseoff, com_pretifymem( *expsz, 2 ));
}

static void *DecryptImage( byte *data, size_t size )
{
	SECTION_HEADER	section;
	OPTIONAL_HEADER	optional;
	VALVE_HEADER	*hlhdr;
	VAVLE_SECTIONS	*hlsec;
	DOS_HEADER	doshdr;
	PE_HEADER		pehdr;
	DWORD		i, tmp, len;
	byte		buff[IMAGE_ALIGN];
	DWORD		section_offset, import_rva, import_size;
	DWORD		export_rva, export_size, iat_rva, iat_size;
	byte		symbol, *newimage;
	vfile_t		*f;

	data += 68;	// skip all zeroes
	size -= 68;

	symbol = 'W';

	// run XOR decryption
	for( i = 0; i < size; i++ )
	{
		data[i] ^= symbol;
		symbol += data[i] + 'W';
	}

	hlhdr = (void *)data;
	hlsec = (void *)(data + sizeof( VALVE_HEADER ));
	data -= 68;	// restore all zeroes
	size += 68;

	// FIXME: convert Ident to properly Magic
	hlhdr->copywhat ^= 0x7a32bc85;
	hlhdr->ImageBase ^= 0x49c042d1;
	hlhdr->ImportTable ^= 0x872c3d47;
	hlhdr->EntryPoint -= 12;
	hlhdr->Sections++;

	// when all the section have been placed in memory
	// hl.exe calls hlhdr->EntryPoint and then hlhdr->copywhat
	// copying a zone of the dll in the hl.exe process

	Mem_Set( &optional, 0, sizeof( optional ));
	optional.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
	optional.MajorLinkerVersion = 6;
	optional.MinorLinkerVersion = 0;
	optional.AddressOfEntryPoint = hlhdr->EntryPoint - hlhdr->ImageBase;
	optional.BaseOfCode = hlsec[0].rva - hlhdr->ImageBase; // .text
	optional.BaseOfData = hlsec[1].rva - hlhdr->ImageBase; // .rdata
	optional.ImageBase = hlhdr->ImageBase;
	optional.SectionAlignment = IMAGE_ALIGN;
	optional.FileAlignment = IMAGE_ALIGN;
	optional.MajorOperatingSystemVersion = 4;
	optional.MinorOperatingSystemVersion = 0;
	optional.MajorImageVersion = 0;
	optional.MinorImageVersion = 0;
	optional.MajorSubsystemVersion = 4;
	optional.MinorSubsystemVersion = 0;
	optional.Win32VersionValue = 0;
	optional.SizeOfHeaders = IMAGE_ALIGN;    // it's ever less than the default alignment
	optional.CheckSum = 0;
	optional.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_GUI;
	optional.DllCharacteristics = 0;
	optional.SizeOfStackReserve = IMAGE_ALIGN * 256;
	optional.SizeOfStackCommit = IMAGE_ALIGN;
	optional.SizeOfHeapReserve = IMAGE_ALIGN * 256;
	optional.SizeOfHeapCommit = IMAGE_ALIGN;
	optional.LoaderFlags = 0;
	optional.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

	for( i = 0; i < hlhdr->Sections; i++ )
	{
		tmp = ( i < 4 ) ? FixedSections[i].Characteristics : SECTION_DEF_CHARACTERISTIC;
		optional.SizeOfImage += ALIGN( hlsec[i].rva_size );
		if( tmp & IMAGE_SCN_CNT_CODE ) optional.SizeOfCode += ALIGN( hlsec[i].rva_size );
		if( tmp & IMAGE_SCN_CNT_INITIALIZED_DATA ) optional.SizeOfInitializedData += ALIGN( hlsec[i].rva_size );
		if( tmp & IMAGE_SCN_CNT_UNINITIALIZED_DATA ) optional.SizeOfUninitializedData += ALIGN( hlsec[i].rva_size );
	}

	optional.SizeOfImage += optional.SizeOfHeaders;
	import_rva = hlhdr->ImportTable;

	ImageFindTables( data + hlsec[1].file_offset, hlsec[1].rva, hlhdr->ImageBase, &import_rva, &import_size, &export_rva, &export_size, &iat_rva, &iat_size );

	optional.DataDirectory[0].VirtualAddress  = export_rva - hlhdr->ImageBase;
	optional.DataDirectory[0].Size = export_size;
	optional.DataDirectory[1].VirtualAddress  = import_rva - hlhdr->ImageBase;
	optional.DataDirectory[1].Size = import_size;

	if( hlhdr->Sections > 3 )
	{
		optional.DataDirectory[2].VirtualAddress = hlsec[3].rva - hlhdr->ImageBase;
		optional.DataDirectory[2].Size = hlsec[3].rva_size;
	}

	optional.DataDirectory[12].VirtualAddress = iat_rva - hlhdr->ImageBase;
	optional.DataDirectory[12].Size = iat_size;

	f = VFS_Open( NULL, "wb" );

	Mem_Set( &doshdr, 0, sizeof( doshdr ));
	doshdr.e_magic = IMAGE_DOS_SIGNATURE;
	doshdr.e_cblp = 0x0090;
	doshdr.e_cp = 0x0003;
	doshdr.e_cparhdr = 0x0004;
	doshdr.e_maxalloc = 0xffff;
	doshdr.e_sp = 0x00b8;
	doshdr.e_lfarlc = 0x0040;
	doshdr.e_lfanew = sizeof( doshdr ) + sizeof( dosdata );

	VFS_Write( f, &doshdr, sizeof( doshdr ));	// write DOS header
	VFS_Write( f, &dosdata, sizeof( dosdata ));	// write default dos executable stub

	tmp = NT_SIGNATURE;
    	Mem_Set( &pehdr, 0, sizeof( pehdr ));
    	pehdr.Machine = IMAGE_FILE_MACHINE_I386;
	pehdr.NumberOfSections = hlhdr->Sections;
	pehdr.TimeDateStamp = time( NULL );
	pehdr.SizeOfOptionalHeader = sizeof( OPTIONAL_HEADER );
	pehdr.Characteristics = (IMAGE_FILE_LOCAL_SYMS_STRIPPED|IMAGE_FILE_EXECUTABLE_IMAGE|IMAGE_FILE_LINE_NUMS_STRIPPED|IMAGE_FILE_32BIT_MACHINE|IMAGE_FILE_DLL);
	VFS_Write( f, &tmp, sizeof( int ));		// write NT signature
	VFS_Write( f, &pehdr, sizeof( pehdr ));		// write PE header

	// write optional header
	VFS_Write( f, &optional, sizeof( optional ));

	section_offset = optional.SizeOfHeaders;

	// write section headers
	for( i = 0; i < hlhdr->Sections; i++ )
	{
		Mem_Set( &section, 0, sizeof( section ));

		if( i < 4 ) com.strncpy( section.Name, FixedSections[i].Name, IMAGE_SIZEOF_SHORT_NAME );
		else com.snprintf( section.Name, IMAGE_SIZEOF_SHORT_NAME, "sec%u", i );

		section.Misc.VirtualSize = hlsec[i].rva_size;
		section.VirtualAddress = hlsec[i].rva - hlhdr->ImageBase;
		section.SizeOfRawData = ALIGN( hlsec[i].file_size );
		section.PointerToRawData = section_offset;
		section.PointerToRelocations = hlsec[i].reloc_addr;
		section.Characteristics = ( i < 4 ) ? FixedSections[i].Characteristics : SECTION_DEF_CHARACTERISTIC;
		VFS_Write( f, &section, sizeof( section ));
		section_offset += ALIGN( hlsec[i].file_size );
	}

	// write sections
	VFS_Seek( f, optional.SizeOfHeaders, SEEK_SET );

	for( i = 0; i < hlhdr->Sections; i++ )
	{
		if(( hlsec[i].file_offset + hlsec[i].file_size ) > size )
		{
			MsgDev( D_WARN, "DecryptImage: section %d is larger than source (%u %d)\n", i, (hlsec[i].file_offset + hlsec[i].file_size), size );
			VFS_Write( f, data + hlsec[i].file_offset, size - hlsec[i].file_offset );
			VFS_Seek( f, (hlsec[i].file_offset + hlsec[i].file_size) - size, SEEK_CUR );
		}
		else VFS_Write( f, data + hlsec[i].file_offset, hlsec[i].file_size );

		// write sections alignment
		tmp = ALIGN( hlsec[i].file_size ) - hlsec[i].file_size;
		for( len = sizeof( buff ); tmp > 0; tmp -= len )
		{
			if( len > tmp ) len = tmp;
			VFS_Write( f, buff, len );
		}
	}

	VFS_Seek( f, 0, SEEK_END );
	size = VFS_Tell( f );

	// realloc image size if needs
	newimage = Mem_Realloc( Sys.basepool, data, size );
	Mem_Copy( newimage, VFS_GetBuffer( f ), size );
	VFS_Close( f );

	return newimage;
}

void *MemoryLoadLibrary( const char *name, qboolean encrypted )
{
	PIMAGE_DOS_HEADER	dos_header;
	PIMAGE_NT_HEADERS	old_header;
	MEMORYMODULE	*result = NULL;
	byte		*code, *headers;
	DWORD		locationDelta;
	DllEntryProc	DllEntry;
	string		errorstring;
	qboolean		successfull;
	void		*data = NULL;
	size_t		size = 0;

	if( encrypted )
	{
		// encypted dll support is disabled for now (doesn't working properly)
		com.sprintf( errorstring, "couldn't load encrypted library %s", name );
		goto library_error;
	}

	data = FS_LoadFile( name, &size );
	if( !data )
	{
		com.sprintf( errorstring, "couldn't load %s", name );
		goto library_error;
	}

	// if this image encrypted we need decrypting it first
	if( encrypted ) data = DecryptImage( data, size );

	dos_header = (PIMAGE_DOS_HEADER)data;
	if( dos_header->e_magic != IMAGE_DOS_SIGNATURE )
	{
		com.sprintf( errorstring, "%s it's not a valid executable file", name );
		goto library_error;
	}

	old_header = (PIMAGE_NT_HEADERS)&((const byte *)(data))[dos_header->e_lfanew];
	if( old_header->Signature != IMAGE_NT_SIGNATURE )
	{
		com.sprintf( errorstring, "%s missing PE header", name );
		goto library_error;
	}

	// reserve memory for image of library
	code = (byte *)VirtualAlloc((LPVOID)(old_header->OptionalHeader.ImageBase), old_header->OptionalHeader.SizeOfImage, MEM_RESERVE, PAGE_READWRITE );

	if( code == NULL )
	{
		// try to allocate memory at arbitrary position
		code = (byte *)VirtualAlloc( NULL, old_header->OptionalHeader.SizeOfImage, MEM_RESERVE, PAGE_READWRITE );
	}    
	if( code == NULL )
	{
		com.sprintf( errorstring, "%s can't reserve memory", name );
		goto library_error;
	}

	result = (MEMORYMODULE *)HeapAlloc( GetProcessHeap(), 0, sizeof( MEMORYMODULE ));
	result->codeBase = code;
	result->numModules = 0;
	result->modules = NULL;
	result->initialized = 0;

	// XXX: is it correct to commit the complete memory region at once?
	// calling DllEntry raises an exception if we don't...
	VirtualAlloc( code, old_header->OptionalHeader.SizeOfImage, MEM_COMMIT, PAGE_READWRITE );

	// commit memory for headers
	headers = (byte *)VirtualAlloc( code, old_header->OptionalHeader.SizeOfHeaders, MEM_COMMIT, PAGE_READWRITE );
	
	// copy PE header to code
	Mem_Copy( headers, dos_header, dos_header->e_lfanew + old_header->OptionalHeader.SizeOfHeaders );
	result->headers = (PIMAGE_NT_HEADERS)&((const byte *)(headers))[dos_header->e_lfanew];

	// update position
	result->headers->OptionalHeader.ImageBase = (DWORD)code;

	// copy sections from DLL file block to new memory location
	CopySections( data, old_header, result );

	// adjust base address of imported data
	locationDelta = (DWORD)(code - old_header->OptionalHeader.ImageBase);
	if( locationDelta != 0 ) PerformBaseRelocation( result, locationDelta );

	// load required dlls and adjust function table of imports
	if( !BuildImportTable( result ))
	{
		com.sprintf( errorstring, "%s failed to build import table", name );
		goto library_error;
	}

	// mark memory pages depending on section headers and release
	// sections that are marked as "discardable"
	FinalizeSections( result );

	// get entry point of loaded library
	if( result->headers->OptionalHeader.AddressOfEntryPoint != 0 )
	{
		DllEntry = (DllEntryProc)CALCULATE_ADDRESS( code, result->headers->OptionalHeader.AddressOfEntryPoint );
		if( DllEntry == 0 )
		{
			com.sprintf( errorstring, "%s has no entry point", name );
			goto library_error;
		}

		// notify library about attaching to process
		successfull = (*DllEntry)((HINSTANCE)code, DLL_PROCESS_ATTACH, 0 );
		if( !successfull )
		{
			com.sprintf( errorstring, "can't attach library %s", name );
			goto library_error;
		}
		result->initialized = 1;
	}

	Mem_Free( data ); // release memory
	return (void *)result;
library_error:
	// cleanup
	if( data ) Mem_Free( data );
	MemoryFreeLibrary( result );
	MsgDev( D_ERROR, "%s\n", errorstring );

	return NULL;
}

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
		if(( pos = com.strstr( in_name, "@@" )) != NULL )
		{
			int	len = pos - in_name;

			// strip off the leading '?'
			out_name = com.stralloc( Sys.stringpool, in_name + 1, __FILE__, __LINE__ );
			out_name[len-1] = 0; // terminate string at the "@@"
			return out_name;
		}
	}
	return com.stralloc( Sys.stringpool, in_name, __FILE__, __LINE__ );
}

qboolean LibraryLoadSymbols( dll_user_t *hInst )
{
	file_t		*f;
	string		errorstring;
	DOS_HEADER	dos_header;
	LONG		nt_signature;
	PE_HEADER		pe_header;
	SECTION_HEADER	section_header;
	qboolean		edata_found;
	OPTIONAL_HEADER	optional_header;
	long		edata_offset;
	long		edata_delta;
	EXPORT_DIRECTORY	export_directory;
	long		name_offset;
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
		com.sprintf( errorstring, "couldn't load %s", hInst->shortPath );
		goto table_error;
	}

	if( FS_Read( f, &dos_header, sizeof( dos_header )) != sizeof( dos_header ))
	{
		com.sprintf( errorstring, "%s has corrupted EXE header", hInst->shortPath );
		goto table_error;
	}

	if( dos_header.e_magic != DOS_SIGNATURE )
	{
		com.sprintf( errorstring, "%s does not have a valid dll signature", hInst->shortPath );
		goto table_error;
	}

	if( FS_Seek( f, dos_header.e_lfanew, SEEK_SET ) == -1 )
	{
		com.sprintf( errorstring, "%s error seeking for new exe header", hInst->shortPath );
		goto table_error;
	}

	if( FS_Read( f, &nt_signature, sizeof( nt_signature )) != sizeof( nt_signature ))
	{
		com.sprintf( errorstring, "%s has corrupted NT header", hInst->shortPath );
		goto table_error;
	}

	if( nt_signature != NT_SIGNATURE )
	{
		com.sprintf( errorstring, "%s does not have a valid NT signature", hInst->shortPath );
		goto table_error;
	}

	if( FS_Read( f, &pe_header, sizeof( pe_header )) != sizeof( pe_header ))
	{
		com.sprintf( errorstring, "%s does not have a valid PE header", hInst->shortPath );
		goto table_error;
	}

	if( !pe_header.SizeOfOptionalHeader )
	{
		com.sprintf( errorstring, "%s does not have an optional header", hInst->shortPath );
		goto table_error;
	}

	if( FS_Read( f, &optional_header, sizeof( optional_header )) != sizeof( optional_header ))
	{
		com.sprintf( errorstring, "%s optional header probably corrupted", hInst->shortPath );
		goto table_error;
	}

	edata_found = false;

	for( i = 0; i < pe_header.NumberOfSections; i++ )
	{

		if( FS_Read( f, &section_header, sizeof( section_header )) != sizeof( section_header ))
		{
			com.sprintf( errorstring, "%s error during reading section header", hInst->shortPath );
			goto table_error;
		}

		if( !com.strcmp((char *)section_header.Name, ".edata" ))
		{
			edata_found = true;
			break;
		}
	}

	if( edata_found )
	{
		edata_offset = section_header.PointerToRawData;
		edata_delta = section_header.VirtualAddress - section_header.PointerToRawData; 
	}
	else
	{
		edata_offset = optional_header.DataDirectory[0].VirtualAddress;
		edata_delta = 0;
	}

	if( FS_Seek( f, edata_offset, SEEK_SET ) == -1 )
	{
		com.sprintf( errorstring, "%s does not have a valid exports section", hInst->shortPath );
		goto table_error;
	}

	if( FS_Read( f, &export_directory, sizeof( export_directory )) != sizeof( export_directory ))
	{
		com.sprintf( errorstring, "%s does not have a valid optional header", hInst->shortPath );
		goto table_error;
	}

	hInst->num_ordinals = export_directory.NumberOfNames;	// also number of ordinals

	if( hInst->num_ordinals > MAX_LIBRARY_EXPORTS )
	{
		com.sprintf( errorstring, "%s too many exports", hInst->shortPath );
		goto table_error;
	}

	ordinal_offset = export_directory.AddressOfNameOrdinals - edata_delta;

	if( FS_Seek( f, ordinal_offset, SEEK_SET ) == -1 )
	{
		com.sprintf( errorstring, "%s does not have a valid ordinals section", hInst->shortPath );
		goto table_error;
	}

	hInst->ordinals = Mem_Alloc( Sys.basepool, hInst->num_ordinals * sizeof( word ));

	if( FS_Read( f, hInst->ordinals, hInst->num_ordinals * sizeof( word )) != (hInst->num_ordinals * sizeof( word )))
	{
		com.sprintf( errorstring, "%s error during reading ordinals table", hInst->shortPath );
		goto table_error;
	}

	function_offset = export_directory.AddressOfFunctions - edata_delta;
	if( FS_Seek( f, function_offset, SEEK_SET ) == -1 )
	{
		com.sprintf( errorstring, "%s does not have a valid export address section", hInst->shortPath );
		goto table_error;
	}

	hInst->funcs = Mem_Alloc( Sys.basepool, hInst->num_ordinals * sizeof( dword ));

	if( FS_Read( f, hInst->funcs, hInst->num_ordinals * sizeof( dword )) != (hInst->num_ordinals * sizeof( dword )))
	{
		com.sprintf( errorstring, "%s error during reading export address section", hInst->shortPath );
		goto table_error;
	}

	name_offset = export_directory.AddressOfNames - edata_delta;

	if( FS_Seek( f, name_offset, SEEK_SET ) == -1 )
	{
		com.sprintf( errorstring, "%s file does not have a valid names section", hInst->shortPath );
		goto table_error;
	}

	p_Names = Mem_Alloc( Sys.basepool, hInst->num_ordinals * sizeof( dword ));

	if( FS_Read( f, p_Names, hInst->num_ordinals * sizeof( dword )) != (hInst->num_ordinals * sizeof( dword )))
	{
		com.sprintf( errorstring, "%s error during reading names table", hInst->shortPath );
		goto table_error;
	}

	for( i = 0; i < hInst->num_ordinals; i++ )
	{
		name_offset = p_Names[i] - edata_delta;

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
		com.sprintf( errorstring, "%s error during loading names section", hInst->shortPath );
		goto table_error;
	}
	FS_Close( f );

	for( i = 0; i < hInst->num_ordinals; i++ )
	{
		if( !com.strcmp( "GiveFnptrsToDll", hInst->names[i] ))	// main entry point for user dlls
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
		hInst->hInstance = MemoryLoadLibrary( hInst->fullPath, hInst->encrypted );
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

	if( hInst->custom_loader )
		return MemoryGetProcAddress( hInst->hInstance, name );
	return GetProcAddress( hInst->hInstance, name );
}

void Com_FreeLibrary( void *hInstance )
{
	dll_user_t *hInst = (dll_user_t *)hInstance;

	if( !hInst || !hInst->hInstance )
		return; // already freed

	if( Sys.app_state == SYS_CRASH )
	{
		// we need to hold down all modules, while MSVC can find error
		MsgDev( D_NOTE, "Sys_FreeLibrary: hold %s for debugging\n", hInst->dllName );
		return;
	}
	else MsgDev( D_NOTE, "Sys_FreeLibrary: Unloading %s\n", hInst->dllName );
	
	if( hInst->custom_loader )
		MemoryFreeLibrary( hInst->hInstance );
	else FreeLibrary( hInst->hInstance );

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
		if( !com.strcmp( pName, hInst->names[i] ))
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