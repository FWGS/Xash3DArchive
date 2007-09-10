/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#ifndef CONST_H
#define CONST_H
	
// const.h -- included first by ALL program modules

#include "materials.h"

#ifdef _WIN32
// unknown pragmas are SUPPOSED to be ignored, but....
#pragma warning(disable : 4244)     // MIPS
#pragma warning(disable : 4136)     // X86
#pragma warning(disable : 4051)     // ALPHA

#pragma warning(disable : 4018)     // signed/unsigned mismatch
#pragma warning(disable : 4305)		// truncation from const double to float

#endif

// MSVC has a different name for several standard functions
#ifdef WIN32
# define strcasecmp stricmp
# define strncasecmp strnicmp
#endif

#if (defined _M_IX86 || defined __i386__) && !defined C_ONLY && !defined __sun__
#define id386	1
#else
#define id386	0
#endif

#if defined _M_ALPHA && !defined C_ONLY
#define idaxp	1
#else
#define idaxp	0
#endif

#define	MAX_STRING_CHARS	1024	// max length of a string passed to Cmd_TokenizeString
#define	MAX_STRING_TOKENS	80		// max tokens resulting from Cmd_TokenizeString
#define	MAX_TOKEN_CHARS		128		// max length of an individual token

#define	MAX_QPATH			64		// max length of a quake game pathname
#define	MAX_OSPATH			128		// max length of a filesystem pathname

//
// per-level limits
//
#define	MAX_CLIENTS			256		// absolute limit
#define	MAX_EDICTS			1024	// must change protocol to increase more
#define	MAX_LIGHTSTYLES		256
#define	MAX_MODELS			256		// these are sent over the net as bytes
#define	MAX_SOUNDS			256		// so they cannot be blindly increased
#define	MAX_IMAGES			256
#define	MAX_ITEMS			256
#define MAX_GENERAL			(MAX_CLIENTS*2)	// general config strings


// game print flags
#define	PRINT_LOW			0		// pickup messages
#define	PRINT_MEDIUM		1		// death messages
#define	PRINT_HIGH			2		// critical messages
#define	PRINT_CHAT			3		// chat messages

#define	SPAWNFLAG_NOT_EASY			0x00000100
#define	SPAWNFLAG_NOT_MEDIUM		0x00000200
#define	SPAWNFLAG_NOT_HARD			0x00000400
#define	SPAWNFLAG_NOT_DEATHMATCH	0x00000800

// entity_state_t->renderfx flags
#define	RF_MINLIGHT		1		// allways have some light (viewmodel)
#define	RF_VIEWERMODEL		2		// don't draw through eyes, only mirrors
#define	RF_WEAPONMODEL		4		// only draw through eyes
#define	RF_FULLBRIGHT		8		// allways draw full intensity
#define	RF_DEPTHHACK		16		// for view weapon Z crunching
#define	RF_TRANSLUCENT		32
#define	RF_FRAMELERP		64
#define	RF_BEAM			128
#define	RF_CUSTOMSKIN		256		// skin is an index in image_precache
#define	RF_GLOW			512		// pulse lighting for bonus items
#define	RF_SHELL_RED		1024
#define	RF_SHELL_GREEN		2048
#define	RF_SHELL_BLUE		4096
#define	RF_IR_VISIBLE		0x00008000		// 32768
#define	RF_SHELL_DOUBLE		0x00010000		// 65536
#define	RF_SHELL_HALF_DAM	0x00020000
#define	RF_USE_DISGUISE		0x00040000

//lazarus
#define RF_VAMPIRE			0x00080000		// 524288

// player_state_t->refdef flags
#define	RDF_UNDERWATER		1		// warp the screen as apropriate
#define RDF_NOWORLDMODEL		2		// used for player configuration screen
#define	RDF_IRGOGGLES		4
#define	RDF_UVGOGGLES		8

#define RDF_BLOOM			32 
#define RDF_PAIN            		64
#define RDF_WATER			128
#define RDF_LAVA			256
#define RDF_SLIME			512

// edict->movetype values

// edict->flags
#define	FL_FLY					0x00000001
#define	FL_SWIM					0x00000002	// implied immunity to drowining
#define FL_IMMUNE_LASER			0x00000004
#define	FL_INWATER				0x00000008
#define	FL_GODMODE				0x00000010
#define	FL_NOTARGET				0x00000020
#define FL_IMMUNE_SLIME			0x00000040
#define FL_IMMUNE_LAVA			0x00000080
#define	FL_PARTIALGROUND		0x00000100	// not all corners are valid
#define	FL_WATERJUMP			0x00000200	// player jumping out of water
#define	FL_ONGROUND		0x00000400

typedef enum
{
	DAMAGE_NO,
	DAMAGE_YES,			// will take damage if hit
	DAMAGE_AIM			// auto targeting recognizes this
} damage_t;

typedef enum 
{
	WEAPON_READY, 
	WEAPON_ACTIVATING,
	WEAPON_DROPPING,
	WEAPON_FIRING
} weaponstate_t;

#define MOVETYPE_NONE		0	// never moves
#define MOVETYPE_NOCLIP		1	// origin and angles change with no interaction
#define MOVETYPE_PUSH		2	// no clip to world, push on box contact
#define MOVETYPE_STOP		3	// no clip to world, stops on box contact
#define MOVETYPE_WALK		4	// gravity
#define MOVETYPE_STEP		5	// gravity, special edge handling
#define MOVETYPE_FLY		6
#define MOVETYPE_TOSS		7	// gravity
#define MOVETYPE_FLYMISSILE		8	// extra size to monsters
#define MOVETYPE_BOUNCE		9
#define MOVETYPE_FOLLOW		10	// attached models
#define MOVETYPE_VEHICLE		11
#define MOVETYPE_PUSHABLE		12
#define MOVETYPE_DEBRIS		13	// non-solid debris that can still hurt you
#define MOVETYPE_RAIN		14	// identical to MOVETYPE_FLYMISSILE, but doesn't cause splash noises when touching water.
#define MOVETYPE_PENDULUM		15	// same as MOVETYPE_PUSH, but used only for pendulums to grab special-case problems
#define MOVETYPE_CONVEYOR		16

/*
==============================================================

MATHLIB

==============================================================
*/

typedef	int	fixed4_t;
typedef	int	fixed8_t;
typedef	int	fixed16_t;

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

struct cplane_s;

extern vec3_t vec3_origin;

#define	nanmask (255<<23)

#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

// microsoft's fabs seems to be ungodly slow...
#define Q_ftol( f ) ( long ) (f)
#define DotProduct(x,y)			(x[0]*y[0]+x[1]*y[1]+x[2]*y[2])

void ClearBounds (vec3_t mins, vec3_t maxs);
void AddPointToBounds (vec3_t v, vec3_t mins, vec3_t maxs);
void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross);
vec_t VectorNormalize (vec3_t v);		// returns vector length
vec_t VectorNormalize2 (vec3_t v, vec3_t out);
void VectorInverse (vec3_t v);
int Q_log2(int val);

void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4]);

float LerpAngle (float a1, float a2, float frac);

void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal );
void PerpendicularVector( vec3_t dst, const vec3_t src );
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );



void Com_PageInMemory (byte *buffer, int size);

//=============================================

//
// key / value info strings
//
char *Info_ValueForKey (char *s, char *key);
void Info_RemoveKey (char *s, char *key);
void Info_SetValueForKey (char *s, char *key, char *value);
bool Info_Validate (char *s);

/*
==============================================================

COLLISION DETECTION

==============================================================
*/
// content masks
#define	MASK_ALL				(-1)
#define	MASK_SOLID				(CONTENTS_SOLID|CONTENTS_WINDOW)
#define	MASK_PLAYERSOLID		(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define	MASK_DEADSOLID			(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW)
#define	MASK_MONSTERSOLID		(CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define	MASK_WATER				(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)
#define	MASK_OPAQUE				(CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA)
#define	MASK_SHOT				(CONTENTS_SOLID|CONTENTS_MONSTER|CONTENTS_WINDOW|CONTENTS_DEADMONSTER)
#define MASK_CURRENT			(CONTENTS_CURRENT_0|CONTENTS_CURRENT_90|CONTENTS_CURRENT_180|CONTENTS_CURRENT_270|CONTENTS_CURRENT_UP|CONTENTS_CURRENT_DOWN)


// gi.BoxEdicts() can return a list of either solid or trigger entities
// FIXME: eliminate AREA_ distinction?
#define	AREA_SOLID		1
#define	AREA_TRIGGERS	2


// a trace is returned when a box is swept through the world
typedef struct
{
	bool	allsolid;	// if true, plane is not valid
	bool	startsolid;	// if true, the initial point was in a solid area
	float		fraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;		// final position
	cplane_t	plane;		// surface normal at impact
	csurface_t	*surface;	// surface hit
	int			contents;	// contents on other side of surface hit
	struct edict_s	*ent;		// not set by CM_*() functions
} trace_t;



// pmove_state_t is the information necessary for client side movement
// prediction
typedef enum 
{
	// can accelerate and turn
	PM_NORMAL,
	PM_SPECTATOR,
	// no acceleration or turning
	PM_DEAD,
	PM_GIB,		// different bounding box
	PM_FREEZE
} pmtype_t;

// pmove->pm_flags
#define	PMF_DUCKED			1
#define	PMF_JUMP_HELD		2
#define	PMF_ON_GROUND		4
#define	PMF_TIME_WATERJUMP	8	// pm_time is waterjump
#define	PMF_TIME_LAND		16	// pm_time is time before rejump
#define	PMF_TIME_TELEPORT	32	// pm_time is non-moving time
#define PMF_NO_PREDICTION	64	// temporarily disables prediction (used for grappling hook)

// this structure needs to be communicated bit-accurate
// from the server to the client to guarantee that
// prediction stays in sync, so no floats are used.
// if any part of the game code modifies this struct, it
// will result in a prediction error of some degree.
typedef struct
{
	pmtype_t	pm_type;

	short		origin[3];		// 12.3
	short		velocity[3];	// 12.3
	byte		pm_flags;		// ducked, jump_held, etc
	byte		pm_time;		// each unit = 8 ms
	short		gravity;
	short		delta_angles[3];	// add to command angles to get view direction
									// changed by spawns, rotating objects, and teleporters
} pmove_state_t;


//
// button bits
//
//
// button bits
//
#define	BUTTON_ATTACK		1
#define	BUTTON_USE		2
#define	BUTTON_ATTACK2		4
#define	BUTTONS_ATTACK (BUTTON_ATTACK | BUTTON_ATTACK2)
#define	BUTTON_ANY		128	// any key whatsoever


// usercmd_t is sent to the server each client frame
typedef struct usercmd_s
{
	byte	msec;
	byte	buttons;
	short	angles[3];
	short	forwardmove, sidemove, upmove;
	byte	impulse;		// remove?
	byte	lightlevel;		// light level the player is standing on
} usercmd_t;


#define	MAXTOUCH	32
typedef struct
{
	// state (in / out)
	pmove_state_t	s;

	// command (in)
	usercmd_t		cmd;
	bool		snapinitial;	// if s has been changed outside pmove

	// results (out)
	int			numtouch;
	struct edict_s	*touchents[MAXTOUCH];

	vec3_t		viewangles;			// clamped
	float		viewheight;

	vec3_t		mins, maxs;			// bounding box size

	struct edict_s	*groundentity;
	int			watertype;
	int			waterlevel;

	// callbacks to test the world
	trace_t		(*trace) (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end);
	int			(*pointcontents) (vec3_t point);
} pmove_t;


// entity_state_t->effects
// Effects are things handled on the client side (lights, particles, frame animations)
// that happen constantly on the given entity.
// An entity that has effects will be sent to the client
// even if it has a zero index model.
#define	EF_ROTATE			0x00000001		// rotate (bonus items)
#define	EF_GIB				0x00000002		// leave a trail
#define	EF_BLASTER			0x00000008		// redlight + trail
#define	EF_ROCKET			0x00000010		// redlight + trail
#define	EF_GRENADE			0x00000020
#define	EF_HYPERBLASTER		0x00000040
#define	EF_BFG				0x00000080
#define EF_COLOR_SHELL		0x00000100
#define EF_POWERSCREEN		0x00000200
#define	EF_ANIM01			0x00000400		// automatically cycle between frames 0 and 1 at 2 hz
#define	EF_ANIM23			0x00000800		// automatically cycle between frames 2 and 3 at 2 hz
#define EF_ANIM_ALL			0x00001000		// automatically cycle through all frames at 2hz
#define EF_ANIM_ALLFAST		0x00002000		// automatically cycle through all frames at 10hz
#define	EF_FLIES			0x00004000
#define	EF_QUAD				0x00008000
#define	EF_PENT				0x00010000
#define	EF_TELEPORTER		0x00020000		// particle fountain
#define EF_FLAG1			0x00040000
#define EF_FLAG2			0x00080000
// RAFAEL
#define EF_IONRIPPER		0x00100000
#define EF_GREENGIB			0x00200000
#define	EF_BLUEHYPERBLASTER 0x00400000
#define EF_SPINNINGLIGHTS	0x00800000
#define EF_PLASMA			0x01000000
#define EF_TRAP				0x02000000

//
// muzzle flashes / player effects
//
#define	MZ_BLASTER			0
#define MZ_MACHINEGUN		1
#define	MZ_SHOTGUN			2
#define	MZ_CHAINGUN1		3
#define	MZ_CHAINGUN2		4
#define	MZ_CHAINGUN3		5
#define	MZ_RAILGUN			6
#define	MZ_ROCKET			7
#define	MZ_GRENADE			8
#define	MZ_LOGIN			9
#define	MZ_LOGOUT			10
#define	MZ_RESPAWN			11
#define	MZ_BFG				12
#define	MZ_SSHOTGUN			13
#define	MZ_HYPERBLASTER		14
#define	MZ_ITEMRESPAWN		15
// RAFAEL
#define MZ_IONRIPPER		16
#define MZ_BLUEHYPERBLASTER 17
#define MZ_PHALANX			18
#define MZ_SILENCED			128		// bit flag ORed with one of the above numbers

//
// monster muzzle flashes
//
#define MZ2_TANK_BLASTER_1				1
#define MZ2_TANK_BLASTER_2				2
#define MZ2_TANK_BLASTER_3				3
#define MZ2_TANK_MACHINEGUN_1			4
#define MZ2_TANK_MACHINEGUN_2			5
#define MZ2_TANK_MACHINEGUN_3			6
#define MZ2_TANK_MACHINEGUN_4			7
#define MZ2_TANK_MACHINEGUN_5			8
#define MZ2_TANK_MACHINEGUN_6			9
#define MZ2_TANK_MACHINEGUN_7			10
#define MZ2_TANK_MACHINEGUN_8			11
#define MZ2_TANK_MACHINEGUN_9			12
#define MZ2_TANK_MACHINEGUN_10			13
#define MZ2_TANK_MACHINEGUN_11			14
#define MZ2_TANK_MACHINEGUN_12			15
#define MZ2_TANK_MACHINEGUN_13			16
#define MZ2_TANK_MACHINEGUN_14			17
#define MZ2_TANK_MACHINEGUN_15			18
#define MZ2_TANK_MACHINEGUN_16			19
#define MZ2_TANK_MACHINEGUN_17			20
#define MZ2_TANK_MACHINEGUN_18			21
#define MZ2_TANK_MACHINEGUN_19			22
#define MZ2_TANK_ROCKET_1				23
#define MZ2_TANK_ROCKET_2				24
#define MZ2_TANK_ROCKET_3				25

#define MZ2_INFANTRY_MACHINEGUN_1		26
#define MZ2_INFANTRY_MACHINEGUN_2		27
#define MZ2_INFANTRY_MACHINEGUN_3		28
#define MZ2_INFANTRY_MACHINEGUN_4		29
#define MZ2_INFANTRY_MACHINEGUN_5		30
#define MZ2_INFANTRY_MACHINEGUN_6		31
#define MZ2_INFANTRY_MACHINEGUN_7		32
#define MZ2_INFANTRY_MACHINEGUN_8		33
#define MZ2_INFANTRY_MACHINEGUN_9		34
#define MZ2_INFANTRY_MACHINEGUN_10		35
#define MZ2_INFANTRY_MACHINEGUN_11		36
#define MZ2_INFANTRY_MACHINEGUN_12		37
#define MZ2_INFANTRY_MACHINEGUN_13		38

#define MZ2_SOLDIER_BLASTER_1			39
#define MZ2_SOLDIER_BLASTER_2			40
#define MZ2_SOLDIER_SHOTGUN_1			41
#define MZ2_SOLDIER_SHOTGUN_2			42
#define MZ2_SOLDIER_MACHINEGUN_1		43
#define MZ2_SOLDIER_MACHINEGUN_2		44

#define MZ2_GUNNER_MACHINEGUN_1			45
#define MZ2_GUNNER_MACHINEGUN_2			46
#define MZ2_GUNNER_MACHINEGUN_3			47
#define MZ2_GUNNER_MACHINEGUN_4			48
#define MZ2_GUNNER_MACHINEGUN_5			49
#define MZ2_GUNNER_MACHINEGUN_6			50
#define MZ2_GUNNER_MACHINEGUN_7			51
#define MZ2_GUNNER_MACHINEGUN_8			52
#define MZ2_GUNNER_GRENADE_1			53
#define MZ2_GUNNER_GRENADE_2			54
#define MZ2_GUNNER_GRENADE_3			55
#define MZ2_GUNNER_GRENADE_4			56

#define MZ2_CHICK_ROCKET_1				57

#define MZ2_FLYER_BLASTER_1				58
#define MZ2_FLYER_BLASTER_2				59

#define MZ2_MEDIC_BLASTER_1				60

#define MZ2_GLADIATOR_RAILGUN_1			61

#define MZ2_HOVER_BLASTER_1				62

#define MZ2_ACTOR_MACHINEGUN_1			63

#define MZ2_SUPERTANK_MACHINEGUN_1		64
#define MZ2_SUPERTANK_MACHINEGUN_2		65
#define MZ2_SUPERTANK_MACHINEGUN_3		66
#define MZ2_SUPERTANK_MACHINEGUN_4		67
#define MZ2_SUPERTANK_MACHINEGUN_5		68
#define MZ2_SUPERTANK_MACHINEGUN_6		69
#define MZ2_SUPERTANK_ROCKET_1			70
#define MZ2_SUPERTANK_ROCKET_2			71
#define MZ2_SUPERTANK_ROCKET_3			72

#define MZ2_BOSS2_MACHINEGUN_L1			73
#define MZ2_BOSS2_MACHINEGUN_L2			74
#define MZ2_BOSS2_MACHINEGUN_L3			75
#define MZ2_BOSS2_MACHINEGUN_L4			76
#define MZ2_BOSS2_MACHINEGUN_L5			77
#define MZ2_BOSS2_ROCKET_1				78
#define MZ2_BOSS2_ROCKET_2				79
#define MZ2_BOSS2_ROCKET_3				80
#define MZ2_BOSS2_ROCKET_4				81

#define MZ2_FLOAT_BLASTER_1				82

#define MZ2_SOLDIER_BLASTER_3			83
#define MZ2_SOLDIER_SHOTGUN_3			84
#define MZ2_SOLDIER_MACHINEGUN_3		85
#define MZ2_SOLDIER_BLASTER_4			86
#define MZ2_SOLDIER_SHOTGUN_4			87
#define MZ2_SOLDIER_MACHINEGUN_4		88
#define MZ2_SOLDIER_BLASTER_5			89
#define MZ2_SOLDIER_SHOTGUN_5			90
#define MZ2_SOLDIER_MACHINEGUN_5		91
#define MZ2_SOLDIER_BLASTER_6			92
#define MZ2_SOLDIER_SHOTGUN_6			93
#define MZ2_SOLDIER_MACHINEGUN_6		94
#define MZ2_SOLDIER_BLASTER_7			95
#define MZ2_SOLDIER_SHOTGUN_7			96
#define MZ2_SOLDIER_MACHINEGUN_7		97
#define MZ2_SOLDIER_BLASTER_8			98
#define MZ2_SOLDIER_SHOTGUN_8			99
#define MZ2_SOLDIER_MACHINEGUN_8		100

// --- Xian shit below ---
#define	MZ2_MAKRON_BFG					101
#define MZ2_MAKRON_BLASTER_1			102
#define MZ2_MAKRON_BLASTER_2			103
#define MZ2_MAKRON_BLASTER_3			104
#define MZ2_MAKRON_BLASTER_4			105
#define MZ2_MAKRON_BLASTER_5			106
#define MZ2_MAKRON_BLASTER_6			107
#define MZ2_MAKRON_BLASTER_7			108
#define MZ2_MAKRON_BLASTER_8			109
#define MZ2_MAKRON_BLASTER_9			110
#define MZ2_MAKRON_BLASTER_10			111
#define MZ2_MAKRON_BLASTER_11			112
#define MZ2_MAKRON_BLASTER_12			113
#define MZ2_MAKRON_BLASTER_13			114
#define MZ2_MAKRON_BLASTER_14			115
#define MZ2_MAKRON_BLASTER_15			116
#define MZ2_MAKRON_BLASTER_16			117
#define MZ2_MAKRON_BLASTER_17			118
#define MZ2_MAKRON_RAILGUN_1			119
#define	MZ2_JORG_MACHINEGUN_L1			120
#define	MZ2_JORG_MACHINEGUN_L2			121
#define	MZ2_JORG_MACHINEGUN_L3			122
#define	MZ2_JORG_MACHINEGUN_L4			123
#define	MZ2_JORG_MACHINEGUN_L5			124
#define	MZ2_JORG_MACHINEGUN_L6			125
#define	MZ2_JORG_MACHINEGUN_R1			126
#define	MZ2_JORG_MACHINEGUN_R2			127
#define	MZ2_JORG_MACHINEGUN_R3			128
#define	MZ2_JORG_MACHINEGUN_R4			129
#define MZ2_JORG_MACHINEGUN_R5			130
#define	MZ2_JORG_MACHINEGUN_R6			131
#define MZ2_JORG_BFG_1					132
#define MZ2_BOSS2_MACHINEGUN_R1			133
#define MZ2_BOSS2_MACHINEGUN_R2			134
#define MZ2_BOSS2_MACHINEGUN_R3			135
#define MZ2_BOSS2_MACHINEGUN_R4			136
#define MZ2_BOSS2_MACHINEGUN_R5			137

extern	vec3_t monster_flash_offset [];


// temp entity events
//
// Temp entity events are for things that happen
// at a location seperate from any existing entity.
// Temporary entity messages are explicitly constructed
// and broadcast.
typedef enum
{
	TE_GUNSHOT,
	TE_BLOOD,
	TE_BLASTER,
	TE_RAILTRAIL,
	TE_SHOTGUN,
	TE_EXPLOSION1,
	TE_EXPLOSION2,
	TE_ROCKET_EXPLOSION,
	TE_GRENADE_EXPLOSION,
	TE_SPARKS,
	TE_SPLASH,
	TE_BUBBLETRAIL,
	TE_SCREEN_SPARKS,
	TE_SHIELD_SPARKS,
	TE_BULLET_SPARKS,
	TE_LASER_SPARKS,
	TE_PARASITE_ATTACK,
	TE_ROCKET_EXPLOSION_WATER,
	TE_GRENADE_EXPLOSION_WATER,
	TE_MEDIC_CABLE_ATTACK,
	TE_BFG_EXPLOSION,
	TE_BFG_BIGEXPLOSION,
	TE_BOSSTPORT,			// used as '22' in a map, so DON'T RENUMBER!!!
	TE_BFG_LASER,
	TE_GRAPPLE_CABLE,
	TE_WELDING_SPARKS,
	TE_GREENBLOOD,
	TE_BLUEHYPERBLASTER,
	TE_PLASMA_EXPLOSION,
	TE_TUNNEL_SPARKS,
	TE_FLASHLIGHT,
	TE_DEBUGTRAIL,
} temp_event_t;

#define SPLASH_UNKNOWN		0
#define SPLASH_SPARKS		1
#define SPLASH_BLUE_WATER	2
#define SPLASH_BROWN_WATER	3
#define SPLASH_SLIME		4
#define	SPLASH_LAVA			5
#define SPLASH_BLOOD		6


// sound channels
// channel 0 never willingly overrides
// other channels (1-7) allways override a playing sound on that channel
#define	CHAN_AUTO               0
#define	CHAN_WEAPON             1
#define	CHAN_VOICE              2
#define	CHAN_ITEM               3
#define	CHAN_BODY               4
#define	CHAN_GIZMO              5
// modifier flags
#define	CHAN_NO_PHS_ADD			8	// send to all clients, not just ones in PHS (ATTN 0 will also do this)
#define	CHAN_RELIABLE			16	// send by reliable message, not datagram


// sound attenuation values
#define	ATTN_NONE               0	// full volume the entire level
#define	ATTN_NORM               1
#define	ATTN_IDLE               2
#define	ATTN_STATIC             3	// diminish very rapidly with distance


// player_state->stats[] indexes
#define STAT_HEALTH_ICON		0
#define	STAT_HEALTH				1
#define	STAT_AMMO_ICON			2
#define	STAT_AMMO				3
#define	STAT_ARMOR_ICON			4
#define	STAT_ARMOR				5
#define	STAT_SELECTED_ICON		6
#define	STAT_PICKUP_ICON		7
#define	STAT_PICKUP_STRING		8
#define	STAT_TIMER_ICON			9
#define	STAT_TIMER				10
#define	STAT_HELPICON			11
#define	STAT_SELECTED_ITEM		12
#define	STAT_LAYOUTS			13
#define	STAT_FRAGS				14
#define	STAT_FLASHES			15		// cleared each frame, 1 = health, 2 = armor
#define STAT_CHASE				16
#define STAT_SPECTATOR			17
#define STAT_SPEED                                22
#define STAT_ZOOM                                 23
#define	MAX_STATS				32


// dmflags->value flags
#define	DF_NO_HEALTH		0x00000001	// 1
#define	DF_NO_ITEMS			0x00000002	// 2
#define	DF_WEAPONS_STAY		0x00000004	// 4
#define	DF_NO_FALLING		0x00000008	// 8
#define	DF_INSTANT_ITEMS	0x00000010	// 16
#define	DF_SAME_LEVEL		0x00000020	// 32
#define DF_SKINTEAMS		0x00000040	// 64
#define DF_MODELTEAMS		0x00000080	// 128
#define DF_NO_FRIENDLY_FIRE	0x00000100	// 256
#define	DF_SPAWN_FARTHEST	0x00000200	// 512
#define DF_FORCE_RESPAWN	0x00000400	// 1024
#define DF_NO_ARMOR			0x00000800	// 2048
#define DF_ALLOW_EXIT		0x00001000	// 4096
#define DF_INFINITE_AMMO	0x00002000	// 8192
#define DF_QUAD_DROP		0x00004000	// 16384
#define DF_FIXED_FOV		0x00008000	// 32768

// RAFAEL
#define	DF_QUADFIRE_DROP	0x00010000	// 65536

/*
==========================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

==========================================================
*/

#define	ANGLE2SHORT(x)	((int)((x)*65536/360) & 65535)
#define	SHORT2ANGLE(x)	((x)*(360.0/65536))


//
// config strings are a general means of communication from
// the server to all connected clients.
// Each config string can be at most MAX_QPATH characters.
//
#define	CS_NAME			0
#define	CS_CDTRACK		1
#define	CS_SKY			2
#define	CS_SKYAXIS		3		// %f %f %f format
#define	CS_SKYROTATE		4
#define	CS_STATUSBAR		5		// display program string

#define	CS_AIRACCEL		29		// air acceleration control
#define	CS_MAXCLIENTS		30
#define	CS_MAPCHECKSUM		31		// for catching cheater maps

#define	CS_MODELS			32
#define	CS_SOUNDS			(CS_MODELS+MAX_MODELS)
#define	CS_IMAGES			(CS_SOUNDS+MAX_SOUNDS)
#define	CS_LIGHTS			(CS_IMAGES+MAX_IMAGES)
#define	CS_ITEMS			(CS_LIGHTS+MAX_LIGHTSTYLES)
#define	CS_PLAYERSKINS		(CS_ITEMS+MAX_ITEMS)
#define	CS_GENERAL		(CS_PLAYERSKINS+MAX_CLIENTS)
#define	MAX_CONFIGSTRINGS		(CS_GENERAL+MAX_GENERAL)


//==============================================


// entity_state_t->event values
// ertity events are for effects that take place reletive
// to an existing entities origin.  Very network efficient.
// All muzzle flashes really should be converted to events...
typedef enum
{
	EV_NONE,
	EV_ITEM_RESPAWN,
	EV_FOOTSTEP,
	EV_FALLSHORT,
	EV_FALL,
	EV_FALLFAR,
	EV_PLAYER_TELEPORT,
	EV_OTHER_TELEPORT
} entity_event_t;


// entity_state_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
typedef struct entity_state_s
{
	int		number;			// edict index

	vec3_t		origin;
	vec3_t		angles;
	vec3_t		old_origin;		// for lerping animation
	int		modelindex;
	int		weaponmodel;

	short		skin;			// skin for studiomodels
	short		frame;			// % playback position in animation sequences (0..512)
	byte		body;			// sub-model selection for studiomodels
	byte		sequence;			// animation sequence (0 - 255)
	uint		effects;			// PGM - we're filling it, so it needs to be unsigned
	int		renderfx;
	int		solid;			// for client side prediction, 8*(bits 0-4) is x/y radius
						// 8*(bits 5-9) is z down distance, 8(bits10-15) is z up
						// gi.linkentity sets this properly
	int		sound;			// for looping sounds, to guarantee shutoff
	int		event;			// impulse events -- muzzle flashes, footsteps, etc
						// events only go out for a single frame, they
						// are automatically cleared each frame
	float		alpha;			// alpha value
} entity_state_t;

//==============================================


// player_state_t is the information needed in addition to pmove_state_t
// to rendered a view.  There will only be 10 player_state_t sent each second,
// but the number of pmove_state_t changes will be reletive to client
// frame rates
typedef struct
{
	pmove_state_t	pmove;		// for prediction

	// these fields do not need to be communicated bit-precise

	vec3_t		viewangles;		// for fixed views
	vec3_t		viewoffset;		// add to pmovestate->origin
	vec3_t		kick_angles;	// add to view direction to get render angles
								// set by weapon kicks, pain effects, etc

	vec3_t		gunangles;
	vec3_t		gunoffset;
	int		gunindex;
	int		gunframe;		// studio frame
	int		sequence;		// stuido animation sequence
	int		gunbody;
	int		gunskin; 

	float		blend[4];		// rgba full screen effect
	
	float		fov;		// horizontal field of view
	int		rdflags;		// refdef flags
	short		stats[MAX_STATS];	// fast status bar updates
} player_state_t;

typedef enum
{
FOOTSTEP_METAL1,
FOOTSTEP_METAL2,
FOOTSTEP_METAL3,
FOOTSTEP_METAL4,
FOOTSTEP_DIRT1,
FOOTSTEP_DIRT2,
FOOTSTEP_DIRT3,
FOOTSTEP_DIRT4,
FOOTSTEP_VENT1,
FOOTSTEP_VENT2,
FOOTSTEP_VENT3,
FOOTSTEP_VENT4,
FOOTSTEP_GRATE1,
FOOTSTEP_GRATE2,
FOOTSTEP_GRATE3,
FOOTSTEP_GRATE4,
FOOTSTEP_TILE1,
FOOTSTEP_TILE2,
FOOTSTEP_TILE3,
FOOTSTEP_TILE4,
FOOTSTEP_GRASS1,
FOOTSTEP_GRASS2,
FOOTSTEP_GRASS3,
FOOTSTEP_GRASS4,
FOOTSTEP_SNOW1,
FOOTSTEP_SNOW2,
FOOTSTEP_SNOW3,
FOOTSTEP_SNOW4,
FOOTSTEP_CARPET1,
FOOTSTEP_CARPET2,
FOOTSTEP_CARPET3,
FOOTSTEP_CARPET4,
FOOTSTEP_FORCE1,
FOOTSTEP_FORCE2,
FOOTSTEP_FORCE3,
FOOTSTEP_FORCE4,
FOOTSTEP_SLOSH1,
FOOTSTEP_SLOSH2,
FOOTSTEP_SLOSH3,
FOOTSTEP_SLOSH4,
FOOTSTEP_LADDER1,
FOOTSTEP_LADDER2,
FOOTSTEP_LADDER3,
FOOTSTEP_LADDER4
} footstep_t;

typedef enum
{
ENTITY_DONT_USE_THIS_ONE,
ENTITY_ITEM_HEALTH,
ENTITY_ITEM_HEALTH_SMALL,
ENTITY_ITEM_HEALTH_LARGE,
ENTITY_ITEM_HEALTH_MEGA,
ENTITY_INFO_PLAYER_START,
ENTITY_INFO_PLAYER_DEATHMATCH,
ENTITY_INFO_PLAYER_COOP,
ENTITY_INFO_PLAYER_INTERMISSION,
ENTITY_FUNC_PLAT,
ENTITY_FUNC_BUTTON,
ENTITY_FUNC_DOOR,
ENTITY_FUNC_DOOR_SECRET,
ENTITY_FUNC_DOOR_ROTATING,
ENTITY_FUNC_ROTATING,
ENTITY_FUNC_TRAIN,
ENTITY_FUNC_WATER,
ENTITY_FUNC_CONVEYOR,
ENTITY_FUNC_AREAPORTAL,
ENTITY_FUNC_CLOCK,
ENTITY_FUNC_WALL,
ENTITY_FUNC_OBJECT,
ENTITY_FUNC_TIMER,
ENTITY_FUNC_EXPLOSIVE,
ENTITY_FUNC_KILLBOX,
ENTITY_TARGET_ACTOR,
ENTITY_TARGET_ANIMATION,
ENTITY_TARGET_BLASTER,
ENTITY_TARGET_CHANGELEVEL,
ENTITY_TARGET_CHARACTER,
ENTITY_TARGET_CROSSLEVEL_TARGET,
ENTITY_TARGET_CROSSLEVEL_TRIGGER,
ENTITY_TARGET_EARTHQUAKE,
ENTITY_TARGET_EXPLOSION,
ENTITY_TARGET_GOAL,
ENTITY_TARGET_HELP,
ENTITY_TARGET_LASER,
ENTITY_TARGET_LIGHTRAMP,
ENTITY_TARGET_SECRET,
ENTITY_TARGET_SPAWNER,
ENTITY_TARGET_SPEAKER,
ENTITY_TARGET_SPLASH,
ENTITY_TARGET_STRING,
ENTITY_TARGET_TEMP_ENTITY,
ENTITY_TRIGGER_ALWAYS,
ENTITY_TRIGGER_COUNTER,
ENTITY_TRIGGER_ELEVATOR,
ENTITY_TRIGGER_GRAVITY,
ENTITY_TRIGGER_HURT,
ENTITY_TRIGGER_KEY,
ENTITY_TRIGGER_ONCE,
ENTITY_TRIGGER_MONSTERJUMP,
ENTITY_TRIGGER_MULTIPLE,
ENTITY_TRIGGER_PUSH,
ENTITY_TRIGGER_RELAY,
ENTITY_VIEWTHING,
ENTITY_WORLDSPAWN,
ENTITY_LIGHT,
ENTITY_LIGHT_MINE1,
ENTITY_LIGHT_MINE2,
ENTITY_INFO_NOTNULL,
ENTITY_PATH_CORNER,
ENTITY_POINT_COMBAT,
ENTITY_MISC_EXPLOBOX,
ENTITY_MISC_BANNER,
ENTITY_MISC_SATELLITE_DISH,
ENTITY_MISC_ACTOR,
ENTITY_MISC_GIB_ARM,
ENTITY_MISC_GIB_LEG,
ENTITY_MISC_GIB_HEAD,
ENTITY_MISC_INSANE,
ENTITY_MISC_DEADSOLDIER,
ENTITY_MISC_VIPER,
ENTITY_MISC_VIPER_BOMB,
ENTITY_MISC_BIGVIPER,
ENTITY_MISC_STROGG_SHIP,
ENTITY_MISC_TELEPORTER,
ENTITY_MISC_TELEPORTER_DEST,
ENTITY_MISC_BLACKHOLE,
ENTITY_MISC_EASTERTANK,
ENTITY_MISC_EASTERCHICK,
ENTITY_MISC_EASTERCHICK2,
ENTITY_MONSTER_BERSERK,
ENTITY_MONSTER_GLADIATOR,
ENTITY_MONSTER_GUNNER,
ENTITY_MONSTER_INFANTRY,
ENTITY_MONSTER_SOLDIER_LIGHT,
ENTITY_MONSTER_SOLDIER,
ENTITY_MONSTER_SOLDIER_SS,
ENTITY_MONSTER_TANK,
ENTITY_MONSTER_MEDIC,
ENTITY_MONSTER_FLIPPER,
ENTITY_MONSTER_CHICK,
ENTITY_MONSTER_PARASITE,
ENTITY_MONSTER_FLYER,
ENTITY_MONSTER_BRAIN,
ENTITY_MONSTER_FLOATER,
ENTITY_MONSTER_HOVER,
ENTITY_MONSTER_MUTANT,
ENTITY_MONSTER_SUPERTANK,
ENTITY_MONSTER_BOSS2,
ENTITY_MONSTER_BOSS3_STAND,
ENTITY_MONSTER_JORG,
ENTITY_MONSTER_COMMANDER_BODY,
ENTITY_TURRET_BREACH,
ENTITY_TURRET_BASE,
ENTITY_TURRET_DRIVER,
ENTITY_CRANE_BEAM,
ENTITY_CRANE_HOIST,
ENTITY_CRANE_HOOK,
ENTITY_CRANE_CONTROL,
ENTITY_CRANE_RESET,
ENTITY_FUNC_BOBBINGWATER,
ENTITY_FUNC_DOOR_SWINGING,
ENTITY_FUNC_FORCE_WALL,
ENTITY_FUNC_MONITOR,
ENTITY_FUNC_PENDULUM,
ENTITY_FUNC_PIVOT,
ENTITY_FUNC_PUSHABLE,
ENTITY_FUNC_REFLECT,
ENTITY_FUNC_TRACKCHANGE,
ENTITY_FUNC_TRACKTRAIN,
ENTITY_FUNC_TRAINBUTTON,
ENTITY_FUNC_VEHICLE,
ENTITY_HINT_PATH,
ENTITY_INFO_TRAIN_START,
ENTITY_MISC_LIGHT,
ENTITY_MODEL_SPAWN,
ENTITY_MODEL_TRAIN,
ENTITY_MODEL_TURRET,
ENTITY_MONSTER_MAKRON,
ENTITY_PATH_TRACK,
ENTITY_TARGET_ANGER,
ENTITY_TARGET_ATTRACTOR,
ENTITY_TARGET_CD,
ENTITY_TARGET_CHANGE,
ENTITY_TARGET_CLONE,
ENTITY_TARGET_EFFECT,
ENTITY_TARGET_FADE,
ENTITY_TARGET_FAILURE,
ENTITY_TARGET_FOG,
ENTITY_TARGET_FOUNTAIN,
ENTITY_TARGET_LIGHTSWITCH,
ENTITY_TARGET_LOCATOR,
ENTITY_TARGET_LOCK,
ENTITY_TARGET_LOCK_CLUE,
ENTITY_TARGET_LOCK_CODE,
ENTITY_TARGET_LOCK_DIGIT,
ENTITY_TARGET_MONITOR,
ENTITY_TARGET_MONSTERBATTLE,
ENTITY_TARGET_MOVEWITH,
ENTITY_TARGET_PRECIPITATION,
ENTITY_TARGET_ROCKS,
ENTITY_TARGET_ROTATION,
ENTITY_TARGET_SET_EFFECT,
ENTITY_TARGET_SKILL,
ENTITY_TARGET_SKY,
ENTITY_TARGET_PLAYBACK,
ENTITY_TARGET_TEXT,
ENTITY_THING,
ENTITY_TREMOR_TRIGGER_MULTIPLE,
ENTITY_TRIGGER_BBOX,
ENTITY_TRIGGER_DISGUISE,
ENTITY_TRIGGER_FOG,
ENTITY_TRIGGER_INSIDE,
ENTITY_TRIGGER_LOOK,
ENTITY_TRIGGER_MASS,
ENTITY_TRIGGER_SCALES,
ENTITY_TRIGGER_SPEAKER,
ENTITY_TRIGGER_SWITCH,
ENTITY_TRIGGER_TELEPORTER,
ENTITY_TRIGGER_TRANSITION,
ENTITY_BOLT,
ENTITY_DEBRIS,
ENTITY_GIB,
ENTITY_GIBHEAD,
ENTITY_GRENADE,
ENTITY_HANDGRENADE,
ENTITY_ROCKET,
ENTITY_CHASECAM,
ENTITY_CAMPLAYER,
ENTITY_PLAYER_NOISE
} entity_id;

#define DEG2RAD( a ) ( a * M_PI ) / 180.0F

#ifdef __LCC__
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#define _mkdir mkdir
#endif

#endif//CONST_H