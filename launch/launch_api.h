//=======================================================================
//			Copyright XashXT Group 2008 �
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
typedef int		sound_t;
typedef float		vec_t;
typedef vec_t		vec2_t[2];
typedef vec_t		vec3_t[3];
typedef vec_t		vec4_t[4];
typedef vec_t		quat_t[4];
typedef byte		rgba_t[4];	// unsigned byte colorpack
typedef byte		rgb_t[3];		// unsigned byte colorpack
typedef vec_t		matrix3x4[3][4];
typedef vec_t		matrix4x4[4][4];
typedef char		string[MAX_STRING];

#include "const.h"

// platform instances
typedef enum
{	
	HOST_OFFLINE = 0,	// host_init( g_Instance ) same much as:
	HOST_CREDITS,	// "splash"	"�anyname"	(easter egg)
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
typedef struct convar_s convar_t;	// console variable
typedef struct { const char *name; void **func; } dllfunc_t; // Sys_LoadLibrary stuff
typedef struct { int numfilenames; char **filenames; char *filenamesbuffer; } search_t;
typedef void ( *setpair_t )( const char *key, const char *value, void *buffer, void *numpairs );
typedef void ( *xcommand_t )( void );

// command buffer modes
enum
{
	EXEC_NOW	= 0,
	EXEC_INSERT,
	EXEC_APPEND,
};

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

// cvar flags
typedef enum
{
	CVAR_ARCHIVE	= BIT(0),	// set to cause it to be saved to config.cfg
	CVAR_USERINFO	= BIT(1),	// added to userinfo  when changed
	CVAR_SERVERNOTIFY	= BIT(2),	// notifies players when changed
	CVAR_EXTDLL	= BIT(3),	// defined by external DLL
	CVAR_CLIENTDLL	= BIT(4),	// defined by the client dll
	CVAR_PROTECTED	= BIT(5),	// it's a server cvar, but we don't send the data since it's a password, etc.
	CVAR_SPONLY	= BIT(6),	// this cvar cannot be changed by clients connected to a multiplayer server.
	CVAR_PRINTABLEONLY	= BIT(7),	// this cvar's string cannot contain unprintable characters ( player name )
	CVAR_UNLOGGED	= BIT(8),	// if this is a FCVAR_SERVER, don't log changes to the log file / console
	CVAR_SERVERINFO	= BIT(9),	// added to serverinfo when changed
	CVAR_PHYSICINFO	= BIT(10),// added to physinfo when changed
	CVAR_RENDERINFO	= BIT(11),// save to a seperate config called opengl.cfg
	CVAR_CHEAT	= BIT(12),// can not be changed if cheats are disabled
	CVAR_INIT		= BIT(13),// don't allow change from console at all, but can be set from the command line
	CVAR_LATCH	= BIT(14),// save changes until server restart
	CVAR_READ_ONLY	= BIT(15),// display only, cannot be set by user at all
	CVAR_LATCH_VIDEO	= BIT(16),// save changes until render restart
	CVAR_USER_CREATED	= BIT(17),// created by a set command (dll's used)
	CVAR_GLCONFIG	= BIT(18),// set to cause it to be saved to opengl.cfg
} cvar_flags_t;

#include "cvardef.h"

/*
========================================================================

SYS EVENT

keep console cmds, network messages, mouse reletives and key buttons
========================================================================
*/
typedef enum
{
	SE_NONE = 0,	// end of events queue
	SE_KEY,		// ev.value[0] is a key code, ev.value[1] is the down flag
	SE_CHAR,		// ev.value[0] is an ascii char
	SE_CONSOLE,	// ev.data is a char*
	SE_MOUSE,		// ev.value[0] and ev.value[1] are reletive signed x / y moves
} ev_type_t;

typedef struct
{
	ev_type_t		type;
	int		value[2];
	void		*data;
	size_t		length;
} sys_event_t;

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

	vec3_t		client_mins[4];	// 4 hulls allowed
	vec3_t		client_maxs[4];	// 4 hulls allowed

	int		max_edicts;	// min edicts is 600, max edicts is 4096
	int		max_tents;	// min temp ents is 300, max is 2048
	int		max_beams;	// min beams is 64, max beams is 512
	int		max_particles;	// min particles is 512, max particles is 8192
} gameinfo_t;

typedef struct sysinfo_s
{
	string		username;		// OS current username
	float		version;		// engine version
	int		cpunum;		// count of cpu's
	float		cpufreq;		// cpu frequency in MHz
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

// filesystem flags
#define FS_STATIC_PATH	1	// FS_ClearSearchPath will be ignore this path
#define FS_NOWRITE_PATH	2	// default behavior - last added gamedir set as writedir. This flag disables it
#define FS_GAMEDIR_PATH	4	// just a marker for gamedir path

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
	void (*sleep)( int msec );				// sleep for some msec
	char *(*clipboard)( void );				// get clipboard data
	void (*queevent)( ev_type_t type, int value, int value2, int length, void *ptr );
	sys_event_t (*getevent)( void );			// get system events
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

	search_t *(*Com_Search)( const char *pattern, int casecmp, int gamedironly ); // returned list of found files

	// console variables
	convar_t *(*Cvar_Get)( const char *name, const char *value, int flags, const char *desc );
	void (*Cvar_LookupVars)( int checkbit, void *buffer, void *ptr, setpair_t callback );
	void (*Cvar_SetString)( const char *name, const char *value );
	void (*Cvar_SetLatched)( const char *name, const char *value );
	void (*Cvar_FullSet)( const char *name, const char *value, int flags );
	void (*Cvar_SetFloat)( const char *name, float value );
	long (*Cvar_GetInteger)(const char *name );
	float (*Cvar_GetValue)(const char *name );
	char *(*Cvar_GetString)(const char *name );
	convar_t *(*Cvar_FindVar)(const char *name );
	void (*Cvar_DirectSet)( cvar_t *var, const char *value );
	void (*Cvar_Register)( cvar_t *variable );		// register game.dll variables

	// console commands
	void (*Cmd_Exec)(int exec_when, const char *text);	// process cmd buffer
	uint  (*Cmd_Argc)( void );
	char *(*Cmd_Args)( void );
	char *(*Cmd_Argv)( uint arg ); 
	void (*Cmd_LookupCmds)( char *buffer, void *ptr, setpair_t callback );
	void (*Cmd_AddCommand)( const char *name, xcommand_t function, const char *desc );
	void (*Cmd_AddGameCommand)( const char *cmd_name, xcommand_t function );
	void (*Cmd_TokenizeString)( const char *text_in );
	void (*Cmd_DelCommand)( const char *name );

	qboolean (*Com_LoadLibrary)( const char *name, dll_info_t *dll );	// load library 
	qboolean (*Com_FreeLibrary)( dll_info_t *dll );			// free library
	void*(*Com_GetProcAddress)( dll_info_t *dll, const char* name );	// gpa
	double (*Com_DoubleTime)( void );				// hi-res timer
	void (*Com_ShellExecute)( const char *p1, const char *p2, qboolean exit );// execute shell programs

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
	char *(*stralloc)(byte *mp,const char *in,const char *file,int line);	// create buffer and copy string here
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
	void (*CmdForward)( void );				// cmd forward to server
	void (*CmdComplete)( char *complete_string );		// cmd autocomplete for system console
} launch_exp_t;

// this is the only function actually exported at the linker level
typedef void *(*launch_t)( stdlib_api_t*, void* );
typedef struct { size_t api_size; size_t com_size; } generic_api_t;

// moved here to enable assertation feature in launch.dll
#define ASSERT( exp )	if(!( exp )) com.abort( "assert failed at %s:%i\n", __FILE__, __LINE__ );

#ifndef LAUNCH_DLL
/*
==============================================================================
		STDLIB GENERIC ALIAS NAMES
don't add aliases for launch.dll because it may be conflicted with real names
==============================================================================
*/
struct convar_s
{
	// this part shared with cvar_t
	char		*name;
	char		*string;
	int		flags;
	float		value;
	struct convar_s	*next;

	// this part unique for convar_t
	int		integer;
	qboolean		modified;
};

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
console variables
===========================================
*/
#define Cvar_Get			com.Cvar_Get
#define Cvar_LookupVars		com.Cvar_LookupVars
#define Cvar_Set			com.Cvar_SetString
#define Cvar_FullSet		com.Cvar_FullSet
#define Cvar_SetLatched		com.Cvar_SetLatched
#define Cvar_Reset( name )		Cvar_SetLatched( name, NULL )
#define Cvar_SetFloat		com.Cvar_SetFloat
#define Cvar_VariableValue		com.Cvar_GetValue
#define Cvar_VariableInteger		com.Cvar_GetInteger
#define Cvar_VariableString		com.Cvar_GetString
#define Cvar_FindVar		com.Cvar_FindVar
#define Cvar_DirectSet		com.Cvar_DirectSet
#define Cvar_Register		com.Cvar_Register

/*
===========================================
console commands
===========================================
*/
#define Cbuf_ExecuteText		com.Cmd_Exec
#define Cbuf_AddText( text )		com.Cmd_Exec( EXEC_APPEND, text )
#define Cmd_ExecuteString( text )	com.Cmd_Exec( EXEC_NOW, text )
#define Cbuf_InsertText( text ) 	com.Cmd_Exec( EXEC_INSERT, text )
#define Cbuf_Execute()		com.Cmd_Exec( EXEC_NOW, NULL )

#define Cmd_Argc()			com.Cmd_Argc()
#define Cmd_Args()			com.Cmd_Args()
#define Cmd_Argv( x )		com.Cmd_Argv( x )
#define Cmd_TokenizeString		com.Cmd_TokenizeString
#define Cmd_LookupCmds		com.Cmd_LookupCmds
#define Cmd_AddCommand		com.Cmd_AddCommand
#define Cmd_AddGameCommand		com.Cmd_AddGameCommand
#define Cmd_RemoveCommand		com.Cmd_DelCommand

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
#define Sys_ShellExecute		com.Com_ShellExecute
#define Sys_NewInstance		com.instance
#define Sys_Sleep			com.sleep
#define Sys_Print			com.print
#define Sys_GetEvent		com.getevent
#define Sys_QueEvent		com.queevent
#define Sys_GetClipboardData		com.clipboard
#define Sys_Quit			com.exit
#define Sys_Break			com.abort
#define Sys_DoubleTime		com.Com_DoubleTime
#define Sys_CheckParm		com.Com_CheckParm
#define Sys_GetParmFromCmdLine( a, b )	com.Com_GetParm( a, b, sizeof( b ))

/*
===========================================
stdlib function names that not across with windows stdlib
===========================================
*/
#define timestamp			com.timestamp
#define copystring( str )		com.stralloc( NULL, str, __FILE__, __LINE__ )
#define memprint( x )		com.pretifymem( x, 2 )
#define va			com.va

#endif//LAUNCH_DLL
#endif//LAUNCH_APH_H