//=======================================================================
//			Copyright XashXT Group 2009 ©
//		      ref_params.h - client rendering state
//=======================================================================
#ifndef REF_PARAMS_H
#define REF_PARAMS_H

// renderer flags
#define RDF_NOWORLDMODEL		(1<<0) 	// used for player configuration screen
#define RDF_PORTALINVIEW		(1<<1)	// draw portal pass
#define RDF_SKYPORTALINVIEW		(1<<2)	// draw skyportal instead of regular sky
#define RDF_NOFOVADJUSTMENT		(1<<3)	// do not adjust fov for widescreen
#define RDF_THIRDPERSON		(1<<4)	// enable chase cam instead firstperson

typedef struct skyportal_s
{
	float		fov;
	float		scale;
	vec3_t		vieworg;
	vec3_t		viewanglesOffset;
} skyportal_t;

typedef struct ref_params_s
{
	// output
	vec3_t		vieworg;
	vec3_t		viewangles;

	vec3_t		forward;
	vec3_t		right;
	vec3_t		up;
	
	float		frametime;	// client frametime
	float		time;		// client time

	int		intermission;
	int		paused;
	int		spectator;
	int		onground;		// true if client is onground
	int		waterlevel;

	vec3_t		simvel;		// client velocity (came from server)
	vec3_t		simorg;		// client origin (without viewheight)

	vec3_t		viewheight;
	float		idealpitch;

	vec3_t		cl_viewangles;	// predicted angles

	int		health;
	vec3_t		crosshairangle;	// pfnCrosshairAngle values from server
	float		viewsize;

	vec3_t		punchangle;	// receivied from server
	int		maxclients;
	int		viewentity;	// entity that set with svc_setview else localclient
	int		num_entities;	// entities actual count (was int playernum;)
	int		max_entities;
	int		demoplayback;	
	int		movetype;		// client movetype (was int hardware;)
	int		smoothing;	// client movement predicting is running

	struct usercmd_s	*cmd;		// last issued usercmd
	struct movevars_s	*movevars;	// sv.movevars

	int		viewport[4];	// x, y, width, height
	int		nextView;		// the renderer calls ClientDLL_CalcRefdef() and Renderview
					// so long in cycles until this value is 0 (multiple views)
	int		flags;		// renderer setup flags (was int onlyClientDraw;)

	// Xash Renderer Specifics
	skyportal_t	skyportal;	// sky protal setup is done in HUD_UpdateEntityVars
	float		blend[4];		// rgba 0-1 full screen blend

	float		fov_x;
	float		fov_y;		// fov_y = V_CalcFov( fov_x, viewport[2], viewport[3] );
} ref_params_t;

#endif//REF_PARAMS_H