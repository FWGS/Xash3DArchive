//=======================================================================
//			Copyright XashXT Group 2009 ©
//		   entity_state.h - a part of network protocol
//=======================================================================
#ifndef ENTITY_STATE_H
#define ENTITY_STATE_H

typedef struct entity_state_s
{
	// Fields which are filled in by routines outside of delta compression
	int		entityType;	// hint for engine
	int		number;		// edict index
	float		msg_time;

	// Message number last time the player/entity state was updated.
	int		messagenum;

	// Fields which can be transitted and reconstructed over the network stream
	vec3_t		origin;
	vec3_t		angles;		// entity angles, not viewangles
	int		modelindex;
	int		sequence;
	float		frame;
	int		colormap;
	short		skin;
	short		solid;
	int		effects;
	float		scale;
	byte		eflags;

	// Render information
	int		rendermode;
	int		renderamt;
	color24		rendercolor;
	int		renderfx;

	int		movetype;
	float		animtime;
	float		framerate;
	int		body;
	byte		controller[4];
	byte		blending[4];
	vec3_t		velocity;

	// Send bbox down to client for use during prediction.
	vec3_t		mins;    
	vec3_t		maxs;

	int		aiment;
	int		owner;		// If owned by a player, the index of that player ( for projectiles )
	float		friction;		// Friction, for prediction.       
	float		gravity;		// Gravity multiplier		

	// client specific
	int		team;
	int		playerclass;
	int		health;
	int		spectator;  
	int		weaponmodel;
	int		gaitsequence;
	vec3_t		basevelocity;	// If standing on conveyor, e.g.   
	int		usehull;		// Use the crouched hull, or the regular player hull.		
	int		oldbuttons;	// Latched buttons last time state updated.     
	int		onground;		// -1 = in air, else pmove entity number		
	int		iStepLeft;
	float		flFallVelocity;	// How fast we are falling  
	float		fov;
	int		weaponanim;

	// parametric movement overrides
	vec3_t		startpos;
	vec3_t		endpos;
	float		impacttime;
	float		starttime;

	// for mods
	int		iuser1;
	int		iuser2;
	int		iuser3;
	int		iuser4;
	float		fuser1;
	float		fuser2;
	float		fuser3;
	float		fuser4;
	vec3_t		vuser1;
	vec3_t		vuser2;
	vec3_t		vuser3;
	vec3_t		vuser4;

	// xash shared strings
	char		classname[32];
	char		targetname[32];
	char		target[32];
	char		netname[32];
} entity_state_t;

#include "pm_info.h"

typedef struct clientdata_s
{
	vec3_t		origin;
	vec3_t		velocity;

	int		viewmodel;
	vec3_t		punchangle;
	int		flags;
	int		waterlevel;
	int		watertype;
	vec3_t		view_ofs;
	float		health;

	int		bInDuck;
	int		weapons; // remove?
	
	int		flTimeStepSound;
	int		flDuckTime;
	int		flSwimTime;
	int		waterjumptime;

	float		maxspeed;

	float		fov;
	int		weaponanim;

	int		m_iId;
	int		ammo_shells;
	int		ammo_nails;
	int		ammo_cells;
	int		ammo_rockets;
	float		m_flNextAttack;
	
	int		tfstate;
	int		pushmsec;
	int		deadflag;
	char		physinfo[MAX_PHYSINFO_STRING];

	// for mods
	int		iuser1;
	int		iuser2;
	int		iuser3;
	int		iuser4;
	float		fuser1;
	float		fuser2;
	float		fuser3;
	float		fuser4;
	vec3_t		vuser1;
	vec3_t		vuser2;
	vec3_t		vuser3;
	vec3_t		vuser4;

} clientdata_t;

#include "weaponinfo.h"

typedef struct local_state_s
{
	entity_state_t	playerstate;
	clientdata_t	client;
	weapon_data_t	weapondata[32];	// WEAPON_BACKUP
} local_state_t;

#endif//ENTITY_STATE_H