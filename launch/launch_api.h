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
#define Sys_NewInstance		com.instance
#define Sys_Print			com.print
#define Sys_Quit			com.exit
#define Sys_Break			com.abort
#define Sys_CheckParm		com.Com_CheckParm
#define Sys_GetParmFromCmdLine( a, b )	com.Com_GetParm( a, b, sizeof( b ))
#define Sys_Input			com.input

#endif//LAUNCH_DLL
#endif//LAUNCH_APH_H