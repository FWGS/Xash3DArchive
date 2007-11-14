//=======================================================================
//			Copyright XashXT Group 2007 �
//			basetypes.h - base engine types
//=======================================================================
#ifndef BASETYPES_H
#define BASETYPES_H

#pragma warning(disable : 4244)	// MIPS
#pragma warning(disable : 4136)	// X86
#pragma warning(disable : 4051)	// ALPHA
#pragma warning(disable : 4018)	// signed/unsigned mismatch
#pragma warning(disable : 4305)	// truncation from const double to float
#pragma warning(disable : 4201)	// nameless struct/union
#pragma warning(disable : 4514)	// unreferenced inline function removed
#pragma warning(disable : 4100)	// unreferenced formal parameter

#define DLLEXPORT	__declspec(dllexport)
#define DLLIMPORT	__declspec(dllimport)

#define MIN_PATH		64
#define MAX_QPATH		64
#define MAX_OSPATH		128
#define MAX_NUM_ARGVS	128
#define MAX_STRING		256
#define MAX_SYSPATH		1024
#define MAX_INPUTLINE	16384
#define MAX_INFO_KEY	64
#define MAX_INFO_VALUE	64
#define MAX_INFO_STRING	512

#define COLOR_BLACK		'0'
#define COLOR_RED		'1'
#define COLOR_GREEN		'2'
#define COLOR_YELLOW	'3'
#define COLOR_BLUE		'4'
#define COLOR_CYAN		'5'
#define COLOR_MAGENTA	'6'
#define COLOR_WHITE		'7'
#define ColorIndex(c)	(((c) - '0') & 7)
#define STRING_COLOR_TAG	'^'
#define IsColorString(p)	( p && *(p) == STRING_COLOR_TAG && *((p)+1) && *((p)+1) != STRING_COLOR_TAG )

#define PITCH		0
#define YAW		1
#define ROLL		2

#ifndef NULL
#define NULL		((void *)0)
#endif

typedef enum{false, true}	bool;
typedef unsigned char 	byte;
typedef unsigned short	word;
typedef unsigned long	dword;
typedef unsigned int	uint;
typedef signed __int64	int64;
typedef struct file_s	file_t;
typedef struct vfile_s	vfile_t;
typedef struct image_s	image_t;
typedef struct model_s	model_t;
typedef int		func_t;
typedef struct edict_s	edict_t;
typedef int		sound_t;
typedef int		string_t;
typedef void (*xcommand_t)	(void);
typedef struct gclient_s	gclient_t;
typedef struct sv_edict_s	sv_edict_t;
typedef struct cl_edict_s	cl_edict_t;
typedef int		progsnum_t;
typedef struct progfuncs_s	progfuncs_t;
typedef float		vec_t;
typedef vec_t		vec3_t[3];
typedef vec_t		vec4_t[4];
typedef long		fs_offset_t;
typedef vec_t		matrix3x4[3][4];
typedef vec_t		matrix4x4[4][4];
typedef struct rgbdata_s	rgbdata_t;
typedef struct physbody_s	NewtonBody;
typedef struct physworld_s	NewtonWorld;
typedef struct physjoint_s	NewtonJoint;
typedef struct physcontact_s	NewtonContact;
typedef struct physragdoll_s	NewtonRagDoll;
typedef struct physmaterial_s	NewtonMaterial;
typedef struct physcolision_s	NewtonCollision;
typedef struct physragbone_s	NewtonRagDollBone;
typedef struct { int fileofs; int filelen; } lump_t;
typedef struct { int type; char *name; } activity_map_t;
typedef struct { uint b:5; uint g:6; uint r:5; } color16;
typedef struct { byte r:8; byte g:8; byte b:8; } color24;
typedef struct { byte r; byte g; byte b; byte a; } color32;
typedef struct { const char *name; void **func; } dllfunc_t;

static vec4_t g_color_table[8] =
{
{0.0, 0.0, 0.0, 1.0},
{1.0, 0.0, 0.0, 1.0},
{0.0, 1.0, 0.0, 1.0},
{1.0, 1.0, 0.0, 1.0},
{0.0, 0.0, 1.0, 1.0},
{0.0, 1.0, 1.0, 1.0},
{1.0, 0.0, 1.0, 1.0},
{1.0, 1.0, 1.0, 1.0},
};


#include "byteorder.h" // byte ordering swap functions

#endif//BASETYPES_H