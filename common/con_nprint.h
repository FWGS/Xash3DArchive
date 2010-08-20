//=======================================================================
//			Copyright XashXT Group 2010 ©
//		     con_nprint.h - print into console notify
//=======================================================================
#ifndef CON_NPRINT_H
#define CON_NPRINT_H

typedef struct con_nprint_s
{
	int	index;		// Row #
	float	time_to_live;	// # of seconds before it dissappears
	float	color[3];		// RGB colors ( 0.0 -> 1.0 scale )
} con_nprint_t;

#endif//CON_NPRINT_H