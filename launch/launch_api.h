//=======================================================================
//			Copyright XashXT Group 2008 ©
//		      launch_api.h - main header for all dll's
//=======================================================================
#ifndef LAUNCH_APH_H
#define LAUNCH_APH_H

// disable some warnings
#pragma warning(disable : 4244)	// MIPS
#pragma warning(disable : 4018)	// signed/unsigned mismatch
#pragma warning(disable : 4305)	// truncation from const double to float

#define MAX_STRING		256	// generic string
#define MAX_INFO_STRING	256	// infostrings are transmitted across network
#define MAX_SYSPATH		1024	// system filepath
#define bound(min, num, max)	((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))
#define MAX_MODS		512	// environment games that engine can keep visible
#define EXPORT		__declspec( dllexport )
#define BIT( n )		(1<<( n ))

#ifndef NULL
#define NULL		((void *)0)
#endif

// color strings
#define IsColorString( p )	( p && *( p ) == '^' && *(( p ) + 1) && *(( p ) + 1) >= '0' && *(( p ) + 1 ) <= '9' )

typedef unsigned long	dword;
typedef unsigned int	uint;
typedef char		string[MAX_STRING];

#ifdef LAUNCH_DLL
#ifndef __cplusplus
typedef enum { false, true }	qboolean;
#else 
typedef int qboolean;
#endif
#endif

// platform instances
typedef enum
{	
	HOST_OFFLINE = 0,	// host_init( g_Instance ) same much as:
	HOST_CREDITS,	// "splash"	"©anyname"	(easter egg)
	HOST_DEDICATED,	// "normal"	"#gamename"
	HOST_NORMAL,	// "normal"	"gamename"
	HOST_MAXCOUNT,	// terminator
} instance_t;

enum dev_level
{
	D_INFO = 1,	// "-dev 1", shows various system messages
	D_WARN,		// "-dev 2", shows not critical system warnings
	D_ERROR,		// "-dev 3", shows critical warnings 
	D_AICONSOLE,	// "-dev 4", special case for game aiconsole
	D_NOTE,		// "-dev 5", show system notifications for engine developers
};

typedef long fs_offset_t;
typedef struct file_s file_t;		// normal file
typedef struct wfile_s wfile_t;	// wad file
typedef struct { int numfilenames; char **filenames; char *filenamesbuffer; } search_t;
typedef struct stream_s stream_t;	// sound stream for background music playing
typedef struct { const char *name; void **func; } dllfunc_t; // Sys_LoadLibrary stuff

// timestamp modes
enum
{
	TIME_FULL = 0,
	TIME_DATE_ONLY,
	TIME_TIME_ONLY,
	TIME_NO_SECONDS,
	TIME_YEAR_ONLY,
	TIME_FILENAME,
};

/*
========================================================================

GAMEINFO stuff

internal shared gameinfo structure (readonly for engine parts)
========================================================================
*/
typedef struct gameinfo_s
{
	// filesystem info
	char		gamefolder[64];	// used for change game '-game x'
	char		basedir[64];	// main game directory (like 'id1' for Quake or 'valve' for Half-Life)
	char		gamedir[64];	// game directory (can be match with basedir, used as primary dir and as write path
	char		startmap[64];	// map to start singleplayer game
	char		trainmap[64];	// map to start hazard course (if specified)
	char		title[64];	// Game Main Title
	float		version;		// game version (optional)

	// .dll pathes
	char		dll_path[64];	// e.g. "bin" or "cl_dlls"
	char		game_dll[64];	// custom path for game.dll

	// about mod info
	string		game_url;		// link to a developer's site
	string		update_url;	// link to updates page
	char		type[64];		// single, toolkit, multiplayer etc
	char		date[64];
	size_t		size;

	int		gamemode;

	char		sp_entity[32];	// e.g. info_player_start
	char		mp_entity[32];	// e.g. info_player_deathmatch

	float		client_mins[4][3];	// 4 hulls allowed
	float		client_maxs[4][3];	// 4 hulls allowed

	int		max_edicts;	// min edicts is 600, max edicts is 4096
	int		max_tents;	// min temp ents is 300, max is 2048
	int		max_beams;	// min beams is 64, max beams is 512
	int		max_particles;	// min particles is 512, max particles is 8192
} gameinfo_t;

typedef struct sysinfo_s
{
	char		instance;		// global engine instance

	int		developer;	// developer level ( 1 - 7 )
	string		ModuleName;	// exe.filename
	
	gameinfo_t	*GameInfo;	// current GameInfo
	gameinfo_t	*games[MAX_MODS];	// environment games (founded at each engine start)
	int		numgames;
} sysinfo_t;

/*
========================================================================
internal dll's loader

two main types - native dlls and other win32 libraries will be recognized automatically
NOTE: never change this structure because all dll descriptions in xash code
writes into struct by offsets not names
========================================================================
*/
typedef struct dll_info_s
{
	const char	*name;	// name of library

	// generic interface
	const dllfunc_t	*fcts;	// list of dll exports	
	const char	*entry;	// entrypoint name (internal libs only)
	void		*link;	// hinstance of loading library

	// xash interface
	void		*(*main)( void*, void* );
	qboolean		crash;	// crash if dll not found

	size_t		api_size;	// interface size
	size_t		com_size;	// main interface size == sizeof( stdilib_api_t )
} dll_info_t;

/*
==============================================================================

STDLIB SYSTEM INTERFACE
==============================================================================
*/
typedef struct stdilib_api_s
{
	// interface validator
	size_t		api_size;				// must matched with sizeof(launch_exp_t)
	size_t		com_size;				// must matched with sizeof(stdlib_api_t)

	sysinfo_t		*SysInfo;				// engine sysinfo (filled by launcher)

	// base events
	void (*instance)( const char *name, const char *fmsg );	// restart engine with new instance
	void (*print)( const char *msg );			// basic text message
	void (*printf)( const char *msg, ... );			// formatted text message
	void (*dprintf)( int level, const char *msg, ...);	// developer text message
	void (*error)( const char *msg, ... );			// abnormal termination with message
	void (*abort)( const char *msg, ... );			// normal tremination with message
	void (*exit)( void );				// normal silent termination
	char *(*input)( void );				// win32 console input (dedicated server)
	int  (*Com_CheckParm)( const char *parm );		// check parm in cmdline  
	qboolean (*Com_GetParm)( char *parm, char *out, size_t size );// get parm from cmdline

	// memlib.c funcs
	void (*memcpy)(void *dest, const void *src, size_t size, const char *file, int line);
	void (*memset)(void *dest, int set, size_t size, const char *filename, int fileline);
	void *(*realloc)(byte *pool, void *mem, size_t size, const char *file, int line);
	void (*move)(byte *pool, void **dest, void *src, size_t size, const char *file, int line); // not a memmove
	void *(*malloc)(byte *pool, size_t size, const char *file, int line);
	void (*free)(void *data, const char *file, int line);
	byte *(*mallocpool)(const char *name, const char *file, int line);
	void (*freepool)(byte **poolptr, const char *file, int line);
	void (*clearpool)(byte *poolptr, const char *file, int line);
	void (*memcheck)(const char *file, int line);		// check memory pools for consistensy
	qboolean (*is_allocated)( byte *poolptr, void *data );	// return true is memory is allocated
	void (*memlist)( size_t minallocationsize );
	void (*memstats)( void );

	qboolean (*Com_LoadLibrary)( const char *name, dll_info_t *dll );	// load library 
	qboolean (*Com_FreeLibrary)( dll_info_t *dll );			// free library
	void*(*Com_GetProcAddress)( dll_info_t *dll, const char* name );	// gpa

	// stdlib.c funcs
	void (*strnupr)(const char *in, char *out, size_t size_out);	// convert string to upper case
	void (*strnlwr)(const char *in, char *out, size_t size_out);	// convert string to lower case
	void (*strupr)(const char *in, char *out);			// convert string to upper case
	void (*strlwr)(const char *in, char *out);			// convert string to lower case
	int (*strlen)( const char *string );				// returns string real length
	int (*cstrlen)( const char *string );				// strlen that stripped color prefixes
	char (*toupper)(const char in );				// convert one charcster to upper case
	char (*tolower)(const char in );				// convert one charcster to lower case
	size_t (*strncat)(char *dst, const char *src, size_t n);		// add new string at end of buffer
	size_t (*strcat)(char *dst, const char *src);			// add new string at end of buffer
	size_t (*strncpy)(char *dst, const char *src, size_t n);		// copy string to existing buffer
	size_t (*strcpy)(char *dst, const char *src);			// copy string to existing buffer
	qboolean (*is_digit)( const char *str );			// check string for digits
	int (*atoi)(const char *str);					// convert string to integer
	float (*atof)(const char *str);				// convert string to float
	void (*atov)( float *dst, const char *src, size_t n );		// convert string to vector
	char *(*strchr)(const char *s, char c);				// find charcster at start of string
	char *(*strrchr)(const char *s, char c);			// find charcster at end of string
	int (*strnicmp)(const char *s1, const char *s2, int n);		// compare strings with case insensative
	int (*stricmp)(const char *s1, const char *s2);			// compare strings with case insensative
	int (*strncmp)(const char *s1, const char *s2, int n);		// compare strings with case sensative
	int (*strcmp)(const char *s1, const char *s2);			// compare strings with case sensative
	char *(*stristr)( const char *s1, const char *s2 );		// find s2 in s1 with case insensative
	char *(*strstr)( const char *s1, const char *s2 );		// find s2 in s1 with case sensative
	int (*vsprintf)(char *buf, const char *fmt, va_list args);		// format message
	int (*sprintf)(char *buffer, const char *format, ...);		// print into buffer
	qboolean (*stricmpext)( const char *s1, const char *s2 );		// allow '*', '?' etc
	char *(*va)(const char *format, ...);				// print into temp buffer
	int (*vsnprintf)( char *buf, size_t size, const char *fmt, va_list args );	// format message
	int (*snprintf)( char *buffer, size_t buffersize, const char *format, ... );	// print into buffer
	char *(*pretifymem)( float value, int digitsafterdecimal );		// pretify memory string
	const char* (*timestamp)( int format );				// returns current time stamp
} stdlib_api_t;

/*
==============================================================================

 Generic LAUNCH.DLL INTERFACE
==============================================================================
*/
typedef struct launch_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(launch_api_t)
	size_t	com_size;		// must matched with sizeof(stdlib_api_t)

	void (*Init)( const int argc, const char **argv );	// init host
	void (*Main)( void );				// host frame
	void (*Free)( void );				// close host
	void (*CPrint)( const char *msg );			// host print
	void (*CmdComplete)( char *complete_string );		// cmd autocomplete for system console
	void (*Crashed)( void );				// tell host about crash
} launch_exp_t;

// this is the only function actually exported at the linker level
typedef void *(*launch_t)( stdlib_api_t*, void* );
typedef struct { size_t api_size; size_t com_size; } generic_api_t;

// moved here to enable assertation feature in launch.dll
#define ASSERT( exp )	if(!( exp )) com.abort( "assert failed at %s:%i\n", __FILE__, __LINE__ );

#ifndef LAUNCH_DLL

/*
==========================================
	memory manager funcs
==========================================
*/
#define Mem_Alloc(pool, size)		com.malloc(pool, size, __FILE__, __LINE__)
#define Mem_Realloc(pool, ptr, size)	com.realloc(pool, ptr, size, __FILE__, __LINE__)
#define Mem_Move(pool, ptr, data, size)	com.move(pool, ptr, data, size, __FILE__, __LINE__)
#define Mem_Free(mem)		com.free(mem, __FILE__, __LINE__)
#define Mem_AllocPool(name)		com.mallocpool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool)		com.freepool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool)		com.clearpool(pool, __FILE__, __LINE__)
#define Mem_Copy(dest, src, size )	com.memcpy(dest, src, size, __FILE__, __LINE__)
#define Mem_Set(dest, val, size )	com.memset(dest, val, size, __FILE__, __LINE__)
#define Mem_Check()			com.memcheck(__FILE__, __LINE__)
#define Mem_IsAllocated( pool, ptr )	com.is_allocated( pool, ptr )
#define Mem_PrintList( size )		com.memlist( size );
#define Mem_PrintStats()		com.memstats()

/*
===========================================
filesystem manager
===========================================
*/
#define FS_Gamedir()		com.SysInfo->GameInfo->gamedir
#define FS_Title()			com.SysInfo->GameInfo->title
#define g_Instance()		com.SysInfo->instance

/*
===========================================
misc utils
===========================================
*/
#define SI			com.SysInfo
#define GI			com.SysInfo->GameInfo
#define Msg			com.printf
#define MsgDev			com.dprintf
#define Sys_LoadLibrary		com.Com_LoadLibrary
#define Sys_FreeLibrary		com.Com_FreeLibrary
#define Sys_GetProcAddress		com.Com_GetProcAddress
#define Sys_NewInstance		com.instance
#define Sys_Print			com.print
#define Sys_Quit			com.exit
#define Sys_Break			com.abort
#define Sys_CheckParm		com.Com_CheckParm
#define Sys_GetParmFromCmdLine( a, b )	com.Com_GetParm( a, b, sizeof( b ))
#define Sys_Input			com.input

#endif//LAUNCH_DLL
#endif//LAUNCH_APH_H