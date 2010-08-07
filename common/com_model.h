//=======================================================================
//			Copyright XashXT Group 2010 ©
//		       com_model.h - cient model structures
//=======================================================================
#ifndef COM_MODEL_H
#define COM_MODEL_H

#define MAX_INFO_STRING		512
#define MAX_SCOREBOARDNAME		32

typedef struct player_info_s
{
	int	userid;			// User id on server
	char	userinfo[MAX_INFO_STRING];	// User info string
	char	name[MAX_SCOREBOARDNAME];	// Name (extracted from userinfo)
	int	spectator;		// Spectator or not, unused

	int	ping;
	int	packet_loss;

	// skin information
	char	model[64];
	int	topcolor;
	int	bottomcolor;

	// last frame rendered
	int	renderframe;	

	// Gait frame estimation
	int	gaitsequence;
	float	gaitframe;
	float	gaityaw;
	vec3_t	prevgaitorigin;
} player_info_t;

#endif//COM_MODEL_H