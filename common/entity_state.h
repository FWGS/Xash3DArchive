//=======================================================================
//			Copyright XashXT Group 2009 ©
//		   entity_state.h - a part of network protocol
//=======================================================================
#ifndef ENTITY_STATE_H
#define ENTITY_STATE_H

// engine edict types that shared across network
typedef enum
{
	ED_SPAWNED = 0,	// this entity requris to set own type with SV_ClassifyEdict
	ED_WORLDSPAWN,	// this is a worldspawn
	ED_STATIC,	// this is a logic without model or entity with static model
	ED_AMBIENT,	// this is entity emitted ambient sounds only
	ED_NORMAL,	// normal entity with model (and\or) sound
	ED_BSPBRUSH,	// brush entity (a part of level)
	ED_CLIENT,	// this is a client entity
	ED_MONSTER,	// monster or bot (generic npc with AI)
	ED_TEMPENTITY,	// this edict will be removed on server when "lifetime" exceeds 
	ED_BEAM,		// laser beam (needs to recalculate pvs and frustum)
	ED_MOVER,		// func_train, func_door and another bsp or mdl movers
	ED_VIEWMODEL,	// client or bot viewmodel (for spectating)
	ED_RIGIDBODY,	// simulated physic
	ED_TRIGGER,	// just for sorting on a server
	ED_PORTAL,	// realtime portal or mirror brush or model
	ED_SKYPORTAL,	// realtime 3D-sky camera
	ED_SCREEN,	// realtime monitor (like portal but without perspective)
	ED_MAXTYPES,
} edtype_t;

// entity_state_t->ed_flags
#define ESF_LINKEDICT	BIT( 0 )		// needs to relink edict on client
#define ESF_NODELTA		BIT( 1 )		// force no delta frame
#define ESF_NO_PREDICTION	BIT( 2 )		// e.g. teleport time

typedef struct entity_state_s
{
	// engine specific
	int		number;		// edict index
	edtype_t		ed_type;		// edict type
	string_t		classname;	// edict classname
	int		ed_flags;		// engine clearing this at end of server frame

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

	// FIXME: old xash variables needs to be removed
	int		flags;
	float		maxspeed;
	float		idealpitch;
	vec3_t		oldorigin;	// FIXME: needs to be removed
	vec3_t		viewangles;	// already calculated view angles on server-side
} entity_state_t;

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
	char		physinfo[512];	// MAX_PHYSINFO_STRING

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

#endif//ENTITY_STATE_H