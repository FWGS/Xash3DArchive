//=======================================================================
//			Copyright XashXT Group 2009 ©
//		        usercmd.h - usercmd communication
//=======================================================================
#ifndef USERCMD_H
#define USERCMD_H

// usercmd_t communication (a part of network protocol)
typedef struct usercmd_s
{
	short		lerp_msec;	// interpolation time on client
	int		msec;		// duration in ms of command
	vec3_t		viewangles;	// command view angles

	// intended velocities
	float		forwardmove;	// forward velocity
	float		sidemove;		// sideways velocity
	float		upmove;		// upward velocity
	byte		lightlevel;	// light level at spot where we are standing.
	word		buttons;		// attack and move buttons
	byte		impulse;		// impulse command issued
	byte		weaponselect;	// current weapon id

	// experimental player impact stuff.
	int		impact_index;
	vec3_t		impact_position;
} usercmd_t;

#endif//USERCMD_H