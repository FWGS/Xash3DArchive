//=======================================================================
//			Copyright XashXT Group 2010 ©
//		     progdefs.h - global and entity variables
//=======================================================================
#ifndef PROGDEFS_H
#define PROGDEFS_H

typedef struct globalvars_s
{	
	float		time;
	float		frametime;
	int		force_retouch;
	string_t		mapname;
	string_t		startspot;
	float		deathmatch;
	float		coop;
	float		teamplay;
	int		serverflags;
	int		found_secrets;
	vec3_t		v_forward;
	vec3_t		v_up;
	vec3_t		v_right;
	int		trace_allsolid;
	int		trace_startsolid;
	float		trace_fraction;
	vec3_t		trace_endpos;
	vec3_t		trace_plane_normal;
	float		trace_plane_dist;
	edict_t		*trace_ent;
	int		trace_inopen;
	int		trace_inwater;
	int		trace_hitgroup;
	int		trace_flags;
	int		changelevel;	// transition in progress when true (was msg_entity)
	int		cdAudioTrack;
	int		maxClients;
	int		maxEntities;
	const char	*pStringBase;	// set to NULL during initialize if you want use StringTable system
	void		*pSaveData;	// (SAVERESTOREDATA *) pointer
	vec3_t		vecLandmarkOffset;
} globalvars_t;

typedef struct entvars_s
{
	string_t		classname;
	string_t		globalname;	// global entity name transmitted across levels
	
	vec3_t		origin;
	vec3_t		oldorigin;
	vec3_t		velocity;
	vec3_t		basevelocity;
	vec3_t		clbasevelocity;	// Base velocity that was passed in to server physics so 
					// client can predict conveyors correctly.
					// Server zeroes it, so we need to store here, too.
	vec3_t		movedir;

	vec3_t		angles;
	vec3_t		avelocity;	// angular velocity (degrees per second)
	vec3_t		punchangle;	// auto-decaying view angle adjustment
	vec3_t		v_angle;		// Viewing angle (player only)

	// For parametric entities
	vec3_t		endpos;
	vec3_t		startpos;
	float		impacttime;
	float		starttime;

	int		fixangle;		// 0 - nothing, 1 - force view angles, 2 - add avelocity	
	float		idealpitch;
	float		pitch_speed;
	float		ideal_yaw;
	float		yaw_speed;

	int		modelindex;

	string_t		model;		// model name
	int		viewmodel;	// player's viewmodel
	int		weaponmodel;	// what other players see

	vec3_t		absmin;		// BB max translated to world coord
	vec3_t		absmax;		// BB max translated to world coord
	vec3_t		mins;		// local BB min
	vec3_t		maxs;		// local BB max
	vec3_t		size;		// maxs - mins

	float		ltime;
	float		nextthink;	// time to next call of think function

	int		movetype;
	int		solid;

	int		skin;
	int		body;		// sub-model selection for studiomodels
	int 		effects;
	float		gravity;		// % of "normal" gravity
	float		friction;		// inverse elasticity of MOVETYPE_BOUNCE

	int		light_level;	// entity current lightlevel

	int		sequence;		// animation sequence
	int		gaitsequence;	// movement animation sequence for player (0 for none)
	float		frame;		// % playback position in animation sequences (0..255)
	float		animtime;		// world time when frame was set
	float		framerate;	// animation playback rate (-8x to 8x)
	byte		controller[4];	// bone controller setting (0..255)
	byte		blending[2];	// blending amount between sub-sequences (0..255)

	float		scale;		// sprites and models rendering scale (0..255)
	int		rendermode;
	float		renderamt;
	vec3_t		rendercolor;
	int		renderfx;

	float		health;
	float		frags;
	int		weapons;		// bit mask for available weapons
	float		takedamage;

	int		deadflag;
	vec3_t		view_ofs;		// eye position

	int		button;
	int		impulse;

	edict_t		*chain;		// linked list for EntitiesInPVS\PHS
	edict_t		*dmg_inflictor;
	edict_t		*enemy;
	edict_t		*aiment;		// entity pointer when MOVETYPE_FOLLOW
	edict_t		*owner;
	edict_t		*groundentity;	// only if FL_ONGROUND is set

	int		spawnflags;	// spwanflags are used only during level loading
	int		flags;		// generic flags that can be send to client

	short		colormap;		// lowbyte topcolor, highbyte bottomcolor
	int		team;		// for teamplay

	float		max_health;
	float		teleport_time;	// engine will be reset this value on next frame
	int		armortype;
	float		armorvalue;
	int		waterlevel;
	int		watertype;

	string_t		target;		// various server strings
	string_t		targetname;
	string_t		netname;
	string_t		message;

	float		dmg_take;
	float		dmg_save;
	float		dmg;
	float		dmgtime;

	string_t		noise;
	string_t		noise1;
	string_t		noise2;
	string_t		noise3;

	float		speed;
	float		air_finished;
	float		pain_finished;
	float		radsuit_finished;

	edict_t		*pContainingEntity;	// filled by engine, don't save, don't modifiy

	int		playerclass;
	float		maxspeed;		// uses to limit speed for current client

	float		fov;		// client fov, used instead m_iFov
	int		weaponanim;	// FIXME: shorten these ?
	int		pushmsec;		// g-cont. used to store last network frame to prevent add entities
					// twice through portals (in HL does nothing)

	int		bInDuck;
	int		flTimeStepSound;	// Next time we can play a step sound
	int		flSwimTime;         // In process of ducking or ducked already?
	int		flDuckTime;	// Time we started duck
	int		iStepLeft;         	// 0 - 4
	float		flFallVelocity;	// falling velocity z

	int		gamestate;
	int		oldbuttons;	// buttons last usercmd
	int		groupinfo;	// entities culling (on server)

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

	edict_t		*euser1;
	edict_t		*euser2;
	edict_t		*euser3;
	edict_t		*euser4;
} entvars_t;

#endif//PROGDEFS_H