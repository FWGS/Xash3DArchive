//=======================================================================
//			Copyright XashXT Group 2010 ©
//			   edict.h - server edicts
//=======================================================================
#ifndef EDICT_H
#define EDICT_H

#include "progdefs.h"

struct edict_s
{
	int		free;		// unused entity when true
	float		freetime;		// sv.time when the object was freed
	int		serialnumber;	// must match with entity num

	struct sv_priv_s	*pvEngineData;	// alloced, freed and used by engine only
	void		*pvPrivateData;	// alloced and freed by engine, used by DLLs
	entvars_t		v;		// C exported fields from progs

	// other fields from progs come immediately after
};

#endif//EDICT_H