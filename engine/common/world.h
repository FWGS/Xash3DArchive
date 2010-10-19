//=======================================================================
//			Copyright XashXT Group 2009 ©
//		         world.h - shared world trace
//=======================================================================
#ifndef WORLD_H
#define WORLD_H

#define MOVE_NORMAL		0	// normal trace
#define MOVE_NOMONSTERS	1	// ignore monsters (edicts with flags (FL_MONSTER|FL_FAKECLIENT|FL_CLIENT) set)
#define MOVE_MISSILE	2	// extra size for monsters
#define MOVE_WORLDONLY	3	// clip only world

#define FMOVE_IGNORE_GLASS	0x100
#define FMOVE_SIMPLEBOX	0x200

#define CONTENTS_NONE	0	// no custom contents specified

/*
===============================================================================

ENTITY AREA CHECKING

===============================================================================
*/
#define EDICT_FROM_AREA( l )		EDICT_NUM( l->entnum )
#define MAX_TOTAL_ENT_LEAFS		128
#define AREA_NODES			32
#define AREA_DEPTH			4

typedef enum
{
	AREA_SOLID,		// find any solid edicts
	AREA_TRIGGERS,		// find all SOLID_TRIGGER edicts
	AREA_CUSTOM,		// find all edicts with custom contents - water, lava, fog, laders etc
} AREA_TYPE;

typedef struct areanode_s
{
	int		axis;		// -1 = leaf node
	float		dist;
	struct areanode_s	*children[2];
	link_t		trigger_edicts;
	link_t		solid_edicts;
	link_t		water_edicts;	// func water
} areanode_t;

typedef struct area_s
{
	const float	*mins;
	const float	*maxs;
	void		**list;
	int		count;
	int		maxcount;
	int		type;
} area_t;

extern const char		*et_name[];

// linked list
void InsertLinkBefore( link_t *l, link_t *before, int entnum );
void RemoveLink( link_t *l );
void ClearLink( link_t *l );

// trace common
void World_MoveBounds( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, vec3_t boxmins, vec3_t boxmaxs );
trace_t World_CombineTraces( trace_t *cliptrace, trace_t *trace, edict_t *touch );
int BoxOnPlaneSide( const vec3_t emins, const vec3_t emaxs, const mplane_t *p );
int RankForContents( int contents );

#define BOX_ON_PLANE_SIDE( emins, emaxs, p )			\
	((( p )->type < 3 ) ?				\
	(						\
		((p)->dist <= (emins)[(p)->type]) ?		\
			1				\
		:					\
		(					\
			((p)->dist >= (emaxs)[(p)->type]) ?	\
				2			\
			:				\
				3			\
		)					\
	)						\
	:						\
		BoxOnPlaneSide(( emins ), ( emaxs ), ( p )))


#include "bspfile.h"
#include "pm_shared.h"

/*
===============================================================================

	EVENTS QUEUE (hl1 events code)

===============================================================================
*/
#include "event_api.h"

#define MAX_EVENT_QUEUE	64		// 16 simultaneous events, max

typedef struct event_info_s
{
	word		index;		// 0 implies not in use
	short		packet_index;	// Use data from state info for entity in delta_packet .
					// -1 implies separate info based on event
					// parameter signature
	short		entity_index;	// The edict this event is associated with
	float		fire_time;	// if non-zero, the time when the event should be fired
					// ( fixed up on the client )
	event_args_t	args;
	int		flags;		// reliable or not, etc. ( CLIENT ONLY )
} event_info_t;

typedef struct event_state_s
{
	event_info_t	ei[MAX_EVENT_QUEUE];
} event_state_t;
	
#endif//WORLD_H