//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        launch.h - launch.dll main header
//=======================================================================
#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <limits.h>
#include <fcntl.h>
#include <direct.h>
#include <sys/stat.h>
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <time.h>
#include <winreg.h>
#include <math.h>

#define LAUNCH_DLL				// ignore alias names
#include "launch_api.h"

#define XASH_VERSION		0.75f	// current version will be shared across gameinfo struct

#define MAX_NUM_ARGVS		128
#define MAX_CMD_TOKENS		80
#define LOG_QUEUE_SIZE		131072	// 128 kb intermediate buffer

// just for last chanse to view message (debug only)
#define MSGBOX(x)			MessageBox(NULL, x, "Xash Error", MB_OK|MB_SETFOREGROUND|MB_ICONSTOP );

enum state_e
{
	SYS_SHUTDOWN = 0,
	SYS_RESTART,
	SYS_CRASH,
	SYS_ABORT,
	SYS_ERROR,
	SYS_FRAME,
};

typedef struct system_s
{
	char			progname[64];		// instance keyword
	char			fmessage[64];		// shutdown final message
	int			app_name;
	int			app_state;

	int			developer;

	// command line parms
	int			argc;
	char			*argv[MAX_NUM_ARGVS];

	// log stuff
	qboolean			log_active;
	char			log_path[MAX_SYSPATH];
	FILE			*logfile;
	
	qboolean			stuffcmdsrun;

	HANDLE			hMutex;
	HINSTANCE			hInstance;
	LPTOP_LEVEL_EXCEPTION_FILTER	oldFilter;
	dll_info_t		*linked_dll;

	char			caption[64];
	qboolean			con_readonly;
	qboolean			con_showalways;
	qboolean			con_showcredits;
	qboolean			con_silentmode;
	byte			*basepool;
	byte			*scriptpool;
	byte			*stringpool;
	qboolean			shutdown_issued;
	qboolean			error;

	// simply profiling
	double			start, end;

	void (*Con_Print)( const char *msg );
	void ( *Init ) ( int argc, char **argv );
	void ( *Main ) ( void ); // host frame
	void ( *Free ) ( void ); // close host
	void (*CPrint)( const char *msg ); // console print
	void (*CmdFwd)( void ); // forward to server
	void (*CmdAuto)( char *complete_string );
} system_t;

// NOTE: if this is changed, it must be changed in cvardef.h too
typedef struct convar_s
{
	// this part shared with convar_t
	char		*name;
	char		*string;
	int		flags;
	float		value;
	struct convar_s	*next;

	// this part unique for convar_t
	int		integer;		// atoi( string )
	qboolean		modified;		// set each time the cvar is changed

	// this part are private to launch.dll
	char		*reset_string;	// cvar_restart will reset to this value
	char		*latched_string;	// for CVAR_LATCH vars
	char		*description;	// variable descrition info
};

extern system_t Sys;
extern sysinfo_t SI;
extern stdlib_api_t	com;

//
// console.c
//
void Con_ShowConsole( qboolean show );
void Con_PrintW(const char *pMsg);
void Con_CreateConsole( void );
void Con_DestroyConsole( void );
char *Sys_Input( void );
void Con_RegisterHotkeys( void );
void Con_DisableInput( void );

//
// system.c
//
void Sys_InitCPU( void );
gameinfo_t Sys_GameInfo( void );
void Sys_ParseCommandLine( LPSTR lpCmdLine );
void Sys_LookupInstance( void );
void Sys_NewInstance( const char *name, const char *fmsg );
double Sys_DoubleTime( void );
char *Sys_GetClipboardData( void );
char *Sys_GetCurrentUser( void );
qboolean Sys_GetModuleName( char *buffer, size_t length );
void Sys_Sleep( int msec );
void Sys_Init( void );
void Sys_Exit( void );
void Sys_Abort( void );
int Sys_CheckParm( const char *parm );
qboolean Sys_GetParmFromCmdLine( char *parm, char *out, size_t size );
qboolean Sys_LoadLibrary( const char *dll_name, dll_info_t *dll );
void* Sys_GetProcAddress ( dll_info_t *dll, const char* name );
void Sys_ShellExecute( const char *path, const char *parms, qboolean exit );
qboolean Sys_FreeLibrary ( dll_info_t *dll );
void Sys_WaitForQuit( void );
void Sys_InitLog( void );
void Sys_CloseLog( void );
void Sys_Error(const char *error, ...);
void Sys_Break(const char *error, ...);
void Sys_PrintLog( const char *pMsg );
void Sys_Print(const char *pMsg);
void Sys_Msg( const char *pMsg, ... );
void Sys_MsgDev( int level, const char *pMsg, ... );
sys_event_t Sys_GetEvent( void );
void Sys_QueEvent( ev_type_t type, int value, int value2, int length, void *ptr );

#define Msg Sys_Msg
#define MsgDev Sys_MsgDev

//
// stdlib.c
//
void com_strnupr(const char *in, char *out, size_t size_out);
void com_strupr(const char *in, char *out);
void com_strnlwr(const char *in, char *out, size_t size_out);
void com_strlwr(const char *in, char *out);
int com_strlen( const char *string );
int com_cstrlen( const char *string );
char com_toupper(const char in );
char com_tolower(const char in );
size_t com_strncat(char *dst, const char *src, size_t siz);
size_t com_strcat(char *dst, const char *src );
size_t com_strncpy(char *dst, const char *src, size_t siz);
size_t com_strcpy(char *dst, const char *src );
char *com_stralloc(byte *mempool, const char *s, const char *filename, int fileline);
qboolean com_isdigit( const char *str );
int com_atoi(const char *str);
float com_atof(const char *str);
void com_atov( float *vec, const char *str, size_t siz );
char *com_strchr(const char *s, char c);
char *com_strrchr(const char *s, char c);
int com_strnicmp(const char *s1, const char *s2, int n);
int com_stricmp(const char *s1, const char *s2);
int com_strncmp (const char *s1, const char *s2, int n);
int com_strcmp (const char *s1, const char *s2);
qboolean com_stricmpext( const char *s1, const char *s2 );
const char* com_timestamp( int format );
char *com_stristr( const char *string, const char *string2 );
char *com_strstr( const char *string, const char *string2 );
int com_vsnprintf(char *buffer, size_t buffersize, const char *format, va_list args);
int com_vsprintf(char *buffer, const char *format, va_list args);
int com_snprintf(char *buffer, size_t buffersize, const char *format, ...);
int com_sprintf(char *buffer, const char *format, ...);
char *com_pretifymem( float value, int digitsafterdecimal );
char *va(const char *format, ...);
#define copystring( str ) com_stralloc( NULL, str, __FILE__, __LINE__)
#define copystring2( pool, str ) com_stralloc( pool, str, __FILE__, __LINE__)

//
// memlib.c
//
void Memory_Init( void );
void Memory_Shutdown( void );
void Memory_Init_Commands( void );
void _mem_move(byte *poolptr, void **dest, void *src, size_t size, const char *filename, int fileline);
void *_mem_realloc(byte *poolptr, void *memptr, size_t size, const char *filename, int fileline);
void _com_mem_copy(void *dest, const void *src, size_t size, const char *filename, int fileline);
void _crt_mem_copy(void *dest, const void *src, size_t size, const char *filename, int fileline);
void _asm_mem_copy(void *dest, const void *src, size_t size, const char *filename, int fileline);
void _mmx_mem_copy(void *dest, const void *src, size_t size, const char *filename, int fileline);
void _amd_mem_copy(void *dest, const void *src, size_t size, const char *filename, int fileline);
void _com_mem_set(void *dest, int set, size_t size, const char *filename, int fileline);
void _crt_mem_set(void *dest, int set, size_t size, const char *filename, int fileline);
void _asm_mem_set(void* dest, int set, size_t size, const char *filename, int fileline);
void _mmx_mem_set(void* dest, int set, size_t size, const char *filename, int fileline);
void *_mem_alloc(byte *poolptr, size_t size, const char *filename, int fileline);
byte *_mem_allocpool(const char *name, const char *filename, int fileline);
void _mem_freepool(byte **poolptr, const char *filename, int fileline);
void _mem_emptypool(byte *poolptr, const char *filename, int fileline);
void _mem_free(void *data, const char *filename, int fileline);
void _mem_check(const char *filename, int fileline);
qboolean _is_allocated( byte *poolptr, void *data );

#define Mem_Alloc(pool, size) _mem_alloc(pool, size, __FILE__, __LINE__)
#define Mem_Realloc(pool, ptr, size) _mem_realloc(pool, ptr, size, __FILE__, __LINE__)
#define Mem_Free(mem) _mem_free(mem, __FILE__, __LINE__)
#define Mem_AllocPool(name) _mem_allocpool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool) _mem_freepool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool) _mem_emptypool(pool, __FILE__, __LINE__)
#define Mem_Move(pool, dest, src, size ) _mem_move(pool, dest, src, size, __FILE__, __LINE__)
#define Mem_Copy(dest, src, size ) com.memcpy(dest, src, size, __FILE__, __LINE__)
#define Mem_Set(dest, val, size ) com.memset(dest, val, size, __FILE__, __LINE__)
#define Mem_IsAllocated( mem ) _is_allocated( NULL, mem )
#define Mem_Check() _mem_check(__FILE__, __LINE__)
#define Mem_Pretify( x ) com_pretifymem(x, 3)
#define Malloc( size ) Mem_Alloc( Sys.basepool, size )

//
// cvar.c
//
convar_t *Cvar_FindVar( const char *var_name );
void Cvar_RegisterVariable( cvar_t *variable );
convar_t *Cvar_Get( const char *var_name, const char *value, int flags, const char *description );
void Cvar_Set( const char *var_name, const char *value );
convar_t *Cvar_Set2( const char *var_name, const char *value, qboolean force );
void Cvar_LookupVars( int checkbit, void *buffer, void *ptr, setpair_t callback );
void Cvar_FullSet( const char *var_name, const char *value, int flags );
void Cvar_SetLatched( const char *var_name, const char *value );
void Cvar_SetFloat( const char *var_name, float value );
float Cvar_VariableValue( const char *var_name );
int Cvar_VariableInteger( const char *var_name );
char *Cvar_VariableString( const char *var_name );
void Cvar_DirectSet( cvar_t *var, const char *value );
qboolean Cvar_Command( void );
void Cvar_WriteVariables( file_t *f );
void Cvar_Init( void );
char *Cvar_Userinfo( void );
char *Cvar_Serverinfo( void );
extern qboolean userinfo_modified;
char *Info_ValueForKey( char *s, char *key );
void Info_RemoveKey( char *s, char *key );
void Info_SetValueForKey( char *s, char *key, char *value );
qboolean Info_Validate( char *s );
void Info_Print( char *s );
extern convar_t *cvar_vars;

//
// cmd.c
//
void Cbuf_Init( void );
void Cbuf_AddText( const char *text );
void Cbuf_InsertText( const char *text );
void Cbuf_ExecuteText( int exec_when, const char *text );
void Cbuf_Execute (void);
uint Cmd_Argc( void );
char *Cmd_Args( void );
char *Cmd_Argv( uint arg );
void Cmd_Init( void );
void Cmd_Unlink( void );
void Cmd_AddCommand( const char *cmd_name, xcommand_t function, const char *cmd_desc );
void Cmd_AddGameCommand( const char *cmd_name, xcommand_t function );
void Cmd_RemoveCommand( const char *cmd_name );
qboolean Cmd_Exists( const char *cmd_name );
void Cmd_LookupCmds( char *buffer, void *ptr, setpair_t callback );
qboolean Cmd_GetMapList( const char *s, char *completedname, int length );
qboolean Cmd_GetDemoList( const char *s, char *completedname, int length );
qboolean Cmd_GetMovieList( const char *s, char *completedname, int length );
void Cmd_TokenizeString( const char *text );
void Cmd_ExecuteString( const char *text );
void Cmd_ForwardToServer( void );

//
// parselib.c
//
void Com_FileBase( const char *in, char *out );

#endif//LAUNCHER_H