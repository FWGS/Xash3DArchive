//=======================================================================
//			Copyright XashXT Group 2008 �
//			library.h - custom dlls loader
//=======================================================================
#ifndef LIBRARY_H
#define LIBRARY_H

#define DOS_SIGNATURE		0x5A4D		// MZ
#define NT_SIGNATURE		0x00004550	// PE00
#define NT_OPTIONAL_HDR32_MAGIC	0x10b
#define FILE_MACHINE_I386		0x014c
#define NUMBER_OF_DIRECTORY_ENTRIES	16
#define MAX_LIBRARY_EXPORTS		4096
#define IMAGE_ALIGN			4096
#define ALIGN( x )			(( x + ( IMAGE_ALIGN - 1 )) & (~( IMAGE_ALIGN - 1 )))
#define SECTION_DEF_CHARACTERISTIC	(IMAGE_SCN_MEM_READ|IMAGE_SCN_CNT_INITIALIZED_DATA|IMAGE_SCN_MEM_WRITE)

typedef struct
{	
	// dos .exe header
	word	e_magic;		// magic number
	word	e_cblp;		// bytes on last page of file
	word	e_cp;		// pages in file
	word	e_crlc;		// relocations
	word	e_cparhdr;	// size of header in paragraphs
	word	e_minalloc;	// minimum extra paragraphs needed
	word	e_maxalloc;	// maximum extra paragraphs needed
	word	e_ss;		// initial (relative) SS value
	word	e_sp;		// initial SP value
	word	e_csum;		// checksum
	word	e_ip;		// initial IP value
	word	e_cs;		// initial (relative) CS value
	word	e_lfarlc;		// file address of relocation table
	word	e_ovno;		// overlay number
	word	e_res[4];		// reserved words
	word	e_oemid;		// OEM identifier (for e_oeminfo)
	word	e_oeminfo;	// OEM information; e_oemid specific
	word	e_res2[10];	// reserved words
	long	e_lfanew;		// file address of new exe header
} DOS_HEADER;

typedef struct
{	
	// win .exe header
	word	Machine;
	word	NumberOfSections;
	dword	TimeDateStamp;
	dword	PointerToSymbolTable;
	dword	NumberOfSymbols;
	word	SizeOfOptionalHeader;
	word	Characteristics;
} PE_HEADER;

typedef struct
{
	byte	Name[8];	// dos name length

	union
	{
		dword	PhysicalAddress;
		dword	VirtualSize;
	} Misc;

	dword	VirtualAddress;
	dword	SizeOfRawData;
	dword	PointerToRawData;
	dword	PointerToRelocations;
	dword	PointerToLinenumbers;
	word	NumberOfRelocations;
	word	NumberOfLinenumbers;
	dword	Characteristics;
} SECTION_HEADER;

typedef struct
{
	dword	VirtualAddress;
	dword	Size;
} DATA_DIRECTORY;

typedef struct
{
	word	Magic;
	byte	MajorLinkerVersion;
	byte	MinorLinkerVersion;
	dword	SizeOfCode;
	dword	SizeOfInitializedData;
	dword	SizeOfUninitializedData;
	dword	AddressOfEntryPoint;
	dword	BaseOfCode;
	dword	BaseOfData;
	dword	ImageBase;
	dword	SectionAlignment;
	dword	FileAlignment;
	word	MajorOperatingSystemVersion;
	word	MinorOperatingSystemVersion;
	word	MajorImageVersion;
	word	MinorImageVersion;
	word	MajorSubsystemVersion;
	word	MinorSubsystemVersion;
	dword	Win32VersionValue;
	dword	SizeOfImage;
	dword	SizeOfHeaders;
	dword	CheckSum;
	word	Subsystem;
	word	DllCharacteristics;
	dword	SizeOfStackReserve;
	dword	SizeOfStackCommit;
	dword	SizeOfHeapReserve;
	dword	SizeOfHeapCommit;
	dword	LoaderFlags;
	dword	NumberOfRvaAndSizes;

	DATA_DIRECTORY	DataDirectory[NUMBER_OF_DIRECTORY_ENTRIES];
} OPTIONAL_HEADER;

typedef struct
{
	dword	Characteristics;
	dword	TimeDateStamp;
	word	MajorVersion;
	word	MinorVersion;
	dword	Name;
	dword	Base;
	dword	NumberOfFunctions;
	dword	NumberOfNames;
	dword	AddressOfFunctions;		// RVA from base of image
	dword	AddressOfNames;		// RVA from base of image
	dword	AddressOfNameOrdinals;	// RVA from base of image
} EXPORT_DIRECTORY;

typedef struct
{
	char	*Name;
	DWORD	Characteristics;
} FIXED_SECTIONS;

typedef struct
{
	dword	Ident;
	dword	Sections;
	dword	copywhat;
	dword	ImageBase;
	dword	EntryPoint;
	dword	ImportTable;
} VALVE_HEADER;

typedef struct
{
	dword	rva;
	dword	rva_size;
	dword	file_size;
	dword	file_offset;
	dword	reloc_addr;
} VAVLE_SECTIONS;

typedef struct dll_user_s
{
	void	*hInstance;		// to avoid possible hacks
	qboolean	custom_loader;		// a bit who indicated loader type
	qboolean	encrypted;		// dll is crypted (some client.dll in HL, CS etc)
	char	dllName[32];		// for debug messages
	string	fullPath, shortPath;	// actual dll paths

	// ordinals stuff
	word	*ordinals;
	dword	*funcs;
	char	*names[MAX_LIBRARY_EXPORTS];	// max 4096 exports supported
	int	num_ordinals;		// actual exports count
	dword	funcBase;			// base offset
} dll_user_t;

dll_user_t *FS_FindLibrary( const char *dllname, qboolean directpath );
void *Com_LoadLibrary( const char *dllname, int build_ordinals_table );
void *Com_LoadLibraryExt( const char *dllname, int build_ordinals_table, qboolean directpath );
void *Com_GetProcAddress( void *hInstance, const char *name );
const char *Com_NameForFunction( void *hInstance, dword function );
dword Com_FunctionFromName( void *hInstance, const char *pName );
void Com_FreeLibrary( void *hInstance );

#endif//LIBRARY_H