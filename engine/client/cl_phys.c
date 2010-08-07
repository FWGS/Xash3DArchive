//=======================================================================
//			Copyright XashXT Group 2008 ©
//		   cl_physics.c - client physic and prediction
//=======================================================================

#include "common.h"
#include "client.h"
#include "matrix_lib.h"
#include "const.h"
#include "pm_defs.h"

/*
================
CL_CheckVelocity
================
*/
void CL_CheckVelocity( cl_entity_t *ent )
{
	int	i;
	float	maxvel;

	maxvel = fabs( clgame.movevars.maxvelocity );

	// bound velocity
	for( i = 0; i < 3; i++ )
	{
		if( IS_NAN( ent->curstate.velocity[i] ))
		{
			MsgDev( D_INFO, "Got a NaN velocity on %s\n", STRING( ent->curstate.classname ));
			ent->curstate.velocity[i] = 0;
		}
		if( IS_NAN( ent->curstate.origin[i] ))
		{
			MsgDev( D_INFO, "Got a NaN origin on %s\n", STRING( ent->curstate.classname ));
			ent->curstate.origin[i] = 0;
		}
	}

	if( VectorLength( ent->curstate.velocity ) > maxvel )
		VectorScale( ent->curstate.velocity, maxvel / VectorLength( ent->curstate.velocity ), ent->curstate.velocity );
}

/*
================
CL_UpdateBaseVelocity
================
*/
void CL_UpdateBaseVelocity( cl_entity_t *ent )
{
#if 0
	// FIXME: Rewrite it with client predicting
	if( ent->curstate.flags & FL_ONGROUND )
	{
		cl_entity_t	*groundentity = CL_GetEntityByIndex( ent->curstate.onground );

		if( groundentity )
		{
			// On conveyor belt that's moving?
			if( groundentity->curstate.flags & FL_CONVEYOR )
			{
				vec3_t	new_basevel;

				VectorScale( groundentity->curstate.movedir, groundentity->curstate.speed, new_basevel );
				if( ent->curstate.flags & FL_BASEVELOCITY )
					VectorAdd( new_basevel, ent->curstate.basevelocity, new_basevel );

				ent->curstate.flags |= FL_BASEVELOCITY;
				VectorCopy( new_basevel, ent->curstate.basevelocity );
			}
		}
	}
#endif
}