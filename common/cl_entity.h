//=======================================================================
//			Copyright XashXT Group 2008 ©
//			  cl_entity.h - cient entity
//=======================================================================
#ifndef CL_ENTITY_H
#define CL_ENTITY_H

#include "entity_state.h"

#define HISTORY_MAX		64		// Must be power of 2
#define HISTORY_MASK	( HISTORY_MAX - 1 )

typedef struct cl_entity_s	cl_entity_t;

typedef struct efrag_s
{
	struct mleaf_s	*leaf;
	struct efrag_s	*leafnext;
	cl_entity_t	*entity;
	struct efrag_s	*entnext;
} efrag_t;

typedef struct
{
	byte		mouthopen;	// 0 = mouth closed, 255 = mouth agape
	byte		sndcount;		// counter for running average
	int		sndavg;		// running average
} mouth_t;

typedef struct
{
	float		prevanimtime;  
	float		sequencetime;
	byte		prevseqblending[16];//MAXSTUDIOBLENDINGS
	vec3_t		prevorigin;
	vec3_t		prevangles;

	int		prevsequence;
	float		prevframe;

	byte		prevcontroller[16];
	byte		prevblending[16];
} latchedvars_t;

typedef struct
{
	float		animtime;		// Time stamp for this movement
	vec3_t		origin;
	vec3_t		angles;
} position_history_t;

struct cl_entity_s
{
	int		index;      	// Index into cl_entities ( always match actual slot )
	int		player;     	// True if this entity is a "player"

	int		serverframe;	// TEMPORARY PLACED HERE
	entity_state_t	baseline;   	// The original state from which to delta during an uncompressed message
	entity_state_t	prevstate;  	// The state information from the penultimate message received from the server
	entity_state_t	curstate;   	// The state information from the last message received from server

	int		current_position;	// Last received history update index
	position_history_t	ph[HISTORY_MAX];	// History of position and angle updates for this player

	mouth_t		mouth;		// For synchronizing mouth movements.

	latchedvars_t	latched;		// Variables used by studio model rendering routines

	// Information based on interplocation, extrapolation, prediction, or just copied from last msg received.
	float		lastmove;

	// Actual render position and angles
	vec3_t		origin;
	vec3_t		angles;

	// Calculated on client-side
	vec3_t		absmin;
	vec3_t		absmax;

	// Attachment points
	vec3_t		attachment_origin[16];
	vec3_t		attachment_angles[16];

	// Other entity local information
	int		trivial_accept;
	float		m_flPrevEventFrame;	// previous event frame
	int		m_iEventSequence;	// current event sequence

	cl_entity_t	*onground;	// Entity standing on

	struct model_s	*model;		// all visible entities have a model
	struct efrag_s	*efrag;		// linked list of efrags
	struct mnode_s	*topnode;		// for bmodels, first world node that splits bmodel,
					// or NULL if not split
	link_t		area;		// linked to a division node or leaf
};

#endif//CL_ENTITY_H