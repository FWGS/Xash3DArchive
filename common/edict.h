//=======================================================================
//			Copyright XashXT Group 2010 ©
//			   edict.h - server edicts
//=======================================================================
#ifndef EDICT_H
#define EDICT_H

#include "progdefs.h"

#define MAX_ENT_LEAFS	48

struct edict_s
{
	int		free;		// unused entity when true
	int		serialnumber;	// must match with entity num

	link_t		area;		// linked to a division node or leaf
	int		headnode;		// -1 to use normal leaf check

	int		num_leafs;
	short		leafnums[MAX_ENT_LEAFS];

	float		freetime;		// sv.time when the object was freed
	
	void		*pvPrivateData;	// alloced and freed by engine, used by DLLs
	entvars_t		v;		// C exported fields from progs

	// other fields from progs come immediately after
};

#endif//EDICT_H