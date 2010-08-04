//=======================================================================
//			Copyright XashXT Group 2006 ©
//			basetypes.h - general typedefs
//=======================================================================
#ifndef BASETYPES_H
#define BASETYPES_H

typedef unsigned char 	byte;
typedef unsigned short	word;
typedef unsigned long	dword;
typedef unsigned __int64	qword;
typedef unsigned int	uint;
typedef int		BOOL;
typedef signed __int64	int64;
typedef int		func_t;
typedef int		sound_t;
typedef int		model_t;
typedef int		string_t;
typedef int		shader_t;
typedef struct cvar_s	cvar_t;
typedef struct edict_s	edict_t;
typedef struct pmove_s	pmove_t;
typedef struct movevars_s	movevars_t;
typedef struct usercmd_s	usercmd_t;
typedef struct cl_priv_s	cl_priv_t;
typedef struct sv_priv_s	sv_priv_t;
typedef struct netadr_s	netadr_t;
typedef unsigned short	CRC16_t;
typedef unsigned long	CRC32_t;
typedef float		vec_t;

#define DLLEXPORT		__declspec( dllexport )

#define FALSE		0
#define TRUE		1

#ifndef NULL
#define NULL		((void *)0)
#endif

#ifndef BIT
#define BIT( n )		(1<<( n ))
#endif

// color strings
#define ColorIndex( c )	((( c ) - '0' ) & 7 )
#define IsColorString( p )	( p && *( p ) == '^' && *(( p ) + 1) && *(( p ) + 1) >= '0' && *(( p ) + 1 ) <= '9' )

typedef struct { byte r; byte g; byte b; } color24;

#endif//BASETYPES_H