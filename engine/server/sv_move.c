/*
sv_move.c - monsters movement
Copyright (C) 2007 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"
#include "mathlib.h"
#include "server.h"
#include "const.h"
#include "pm_defs.h"

#define MOVE_NORMAL		0	// normal move in the direction monster is facing
#define MOVE_STRAFE		1	// moves in direction specified, no matter which way monster is facing

/*
=============
SV_CheckBottom

Returns false if any part of the bottom of the entity is off an edge that
is not a staircase.

=============
*/
qboolean SV_CheckBottom( edict_t *ent, int iMode )
{
	vec3_t	mins, maxs, start, stop;
	float	mid, bottom;
	trace_t	trace;
	int	x, y;

	VectorAdd( ent->v.origin, ent->v.mins, mins );
	VectorAdd( ent->v.origin, ent->v.maxs, maxs );

	// if all of the points under the corners are solid world, don't bother
	// with the tougher checks
	// the corners must be within 16 of the midpoint
	start[2] = mins[2] - 1;
	for( x = 0; x <= 1; x++ )
	{
		for( y = 0; y <= 1; y++ )
		{
			start[0] = x ? maxs[0] : mins[0];
			start[1] = y ? maxs[1] : mins[1];
			svs.groupmask = ent->v.groupinfo;

			if( SV_PointContents( start ) != CONTENTS_SOLID )
				goto realcheck;
		}
	}
	return true; // we got out easy
realcheck:
	// check it for real...
	start[2] = mins[2] + svgame.movevars.stepsize;

	// the midpoint must be within 16 of the bottom
	start[0] = stop[0] = (mins[0] + maxs[0]) * 0.5f;
	start[1] = stop[1] = (mins[1] + maxs[1]) * 0.5f;
	stop[2] = start[2] - 2 * svgame.movevars.stepsize;

	if( iMode == WALKMOVE_WORLDONLY )
		trace = SV_Move( start, vec3_origin, vec3_origin, stop, MOVE_WORLDONLY, ent );
	else trace = SV_Move( start, vec3_origin, vec3_origin, stop, MOVE_NORMAL, ent );

	if( trace.fraction == 1.0f )
		return false;
	mid = bottom = trace.endpos[2];

	// the corners must be within 16 of the midpoint
	for( x = 0; x <= 1; x++ )
	{
		for( y = 0; y <= 1; y++ )
		{
			start[0] = stop[0] = x ? maxs[0] : mins[0];
			start[1] = stop[1] = y ? maxs[1] : mins[1];

			if( iMode == WALKMOVE_WORLDONLY )
				trace = SV_Move( start, vec3_origin, vec3_origin, stop, MOVE_WORLDONLY, ent );
			else trace = SV_Move( start, vec3_origin, vec3_origin, stop, MOVE_NORMAL, ent );

			if( trace.fraction != 1.0f && trace.endpos[2] > bottom )
				bottom = trace.endpos[2];
			if( trace.fraction == 1.0f || mid - trace.endpos[2] > svgame.movevars.stepsize )
				return false;
		}
	}
	return true;
}

/*
=============
SV_VecToYaw

converts dir to yaw
=============
*/
float SV_VecToYaw( const vec3_t src )
{
	float	yaw;

	if( src[1] == 0.0f && src[0] == 0.0f )
	{
		yaw = 0;
	}
	else
	{
		yaw = (int)( atan2( src[1], src[0] ) * 180 / M_PI );
		if( yaw < 0 ) yaw += 360;
	}
	return yaw;
}

//============================================================================

qboolean SV_MoveStep( edict_t *ent, vec3_t move, qboolean relink )
{
	int	i;
	trace_t	trace;
	vec3_t	oldorg, neworg, end;
	edict_t	*enemy;
	float	dz;

	VectorCopy( ent->v.origin, oldorg );
	VectorAdd( ent->v.origin, move, neworg );

	// well, try it.  Flying and swimming monsters are easiest.
	if( ent->v.flags & ( FL_SWIM|FL_FLY ))
	{
		// try one move with vertical motion, then one without
		for( i = 0; i < 2; i++ )
		{
			VectorAdd( ent->v.origin, move, neworg );

			enemy = ent->v.enemy;
			if( i == 0 && enemy != NULL )
			{
				dz = ent->v.origin[2] - enemy->v.origin[2];

				if( dz > 40 ) neworg[2] -= 8;
				else if( dz < 30 ) neworg[2] += 8;
			}

			trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, neworg, MOVE_NORMAL, ent );

			if( trace.fraction == 1.0f )
			{
				svs.groupmask = ent->v.groupinfo;

				// that move takes us out of the water.
				// apparently though, it's okay to travel into solids, lava, sky, etc :)
				if(( ent->v.flags & FL_SWIM ) && SV_PointContents( trace.endpos ) == CONTENTS_EMPTY )
					return 0;

				VectorCopy( trace.endpos, ent->v.origin );
				if( relink ) SV_LinkEdict( ent, true );

				return 1;
			}
			else
			{
				if( !SV_IsValidEdict( enemy ))
					break;
			}
		}
		return 0;
	}
	else
	{

		dz = svgame.movevars.stepsize;
		neworg[2] += dz;
		VectorCopy( neworg, end );
		end[2] -= dz * 2.0f;

		trace = SV_Move( neworg, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent );
		if( trace.allsolid )
			return 0;

		if( trace.startsolid != 0 )
		{
			neworg[2] -= dz;
			trace = SV_Move( neworg, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent );

			if( trace.allsolid != 0 || trace.startsolid != 0 )
				return 0;
		}

		if( trace.fraction == 1.0f )
		{
			if( ent->v.flags & FL_PARTIALGROUND )
			{
				VectorAdd( ent->v.origin, move, ent->v.origin );
				if( relink ) SV_LinkEdict( ent, true );
				ent->v.flags &= ~FL_ONGROUND;
				return 1;
			}
			return 0;
		}
		else
		{
			VectorCopy( trace.endpos, ent->v.origin );

			if( SV_CheckBottom( ent, MOVE_NORMAL ) == 0 )
			{
				if( ent->v.flags & FL_PARTIALGROUND )
				{
					if( relink ) SV_LinkEdict( ent, true );
					return 1;
				}

				VectorCopy( oldorg, ent->v.origin );
				return 0;
			}
			else
			{
				ent->v.flags &= ~FL_PARTIALGROUND;
				ent->v.groundentity = trace.ent;
				if( relink ) SV_LinkEdict( ent, true );

				return 1;
			}
		}
	}
}

qboolean SV_MoveTest( edict_t *ent, vec3_t move, qboolean relink )
{
	float	temp;
	vec3_t	oldorg, neworg, end;
	trace_t	trace;

	VectorCopy( ent->v.origin, oldorg );
	VectorAdd( ent->v.origin, move, neworg );

	temp = svgame.movevars.stepsize;

	neworg[2] += temp;
	VectorCopy( neworg, end );
	end[2] -= temp * 2.0f;

	trace = SV_Move( neworg, ent->v.mins, ent->v.maxs, end, MOVE_WORLDONLY, ent );

	if( trace.allsolid != 0 )
		return 0;

	if( trace.startsolid != 0 )
	{
		neworg[2] -= temp;
		trace = SV_Move( neworg, ent->v.mins, ent->v.maxs, end, MOVE_WORLDONLY, ent );

		if( trace.allsolid != 0 || trace.startsolid != 0 )
			return 0;
	}

	if( trace.fraction == 1.0f )
	{
		if( ent->v.flags & FL_PARTIALGROUND )
		{
			VectorAdd( ent->v.origin, move, ent->v.origin );
			if( relink ) SV_LinkEdict( ent, true );
			ent->v.flags &= ~FL_ONGROUND;
			return 1;
		}
		return 0;
	}
	else
	{
		VectorCopy( trace.endpos, ent->v.origin );

		if( SV_CheckBottom( ent, MOVE_WORLDONLY ) == 0 )
		{
			if( ent->v.flags & FL_PARTIALGROUND )
			{
				if( relink ) SV_LinkEdict( ent, true );
				return 1;
			}

			VectorCopy( oldorg, ent->v.origin );
			return 0;
		}
		else
		{
			ent->v.flags &= ~FL_PARTIALGROUND;
			ent->v.groundentity = trace.ent;
			if( relink ) SV_LinkEdict( ent, true );

			return 1;
		}
	}
}

qboolean SV_StepDirection( edict_t *ent, float yaw, float dist )
{
	int	ret;
	vec3_t	move;

	yaw = yaw * M_PI * 2 / 360;
	VectorSet( move, cos( yaw ) * dist, sin( yaw ) * dist, 0.0f );

	ret = SV_MoveStep( ent, move, 0 );
	SV_LinkEdict( ent, true );

	return ret;
}

qboolean SV_FlyDirection( edict_t *ent, vec3_t move )
{
	int	ret;

	ret = SV_MoveStep( ent, move, 0 );
	SV_LinkEdict( ent, true );

	return ret;
}

void SV_NewChaseDir( edict_t *actor, vec3_t destination, float dist )
{
	float	deltax, deltay;
	float	tempdir, olddir, turnaround;
	vec3_t	d;

	olddir = anglemod(((int)( actor->v.ideal_yaw / 45.0f )) * 45.0f );
	turnaround = anglemod( olddir - 180 );

	deltax = destination[0] - actor->v.origin[0];
	deltay = destination[1] - actor->v.origin[1];

	if( deltax > 10 )
		d[1] = 0.0f;
	else if( deltax < -10 )
		d[1] = 180.0f;
	else d[1] = -1;

	if( deltay < -10 )
		d[2] = 270.0f;
	else if( deltay > 10 )
		d[2] = 90.0f;
	else d[2] = -1;

	// try direct route
	if( d[1] != -1 && d[2] != -1 )
	{
		if( d[1] == 0.0f )
			tempdir = ( d[2] == 90.0f ) ? 45.0f : 315.0f;
		else tempdir = ( d[2] == 90.0f ) ? 135.0f : 215.0f;

		if( tempdir != turnaround && SV_StepDirection( actor, tempdir, dist ))
			return;
	}

	// try other directions
	if( Com_RandomLong( 0, 1 ) != 0 || fabs( deltay ) > fabs( deltax ))
	{
		tempdir = d[1];
		d[1] = d[2];
		d[2] = tempdir;
	}

	if( d[1] != -1 && d[1] != turnaround && SV_StepDirection( actor, d[1], dist ))
		return;

	if( d[2] != -1 && d[2] != turnaround && SV_StepDirection( actor, d[2], dist ))
		return;

	// there is no direct path to the player, so pick another direction
	if( olddir != -1 && SV_StepDirection( actor, olddir, dist ))
		return;

	// fine, just run somewhere.
	if( Com_RandomLong( 0, 1 ) != 1 )
	{
		for( tempdir = 0; tempdir <= 315; tempdir += 45 )
		{
			if( tempdir != turnaround && SV_StepDirection( actor, tempdir, dist ))
				return;
		}
	}
	else
	{
		for( tempdir = 315; tempdir >= 0; tempdir -= 45 )
		{
			if( tempdir != turnaround && SV_StepDirection( actor, tempdir, dist ))
				return;
		}
	}

	// we tried. run backwards. that ought to work...
	if( turnaround != -1 && SV_StepDirection( actor, turnaround, dist ))
		return;

	// well, we're stuck somehow.
	actor->v.ideal_yaw = olddir;

	// if a bridge was pulled out from underneath a monster, it may not have
	// a valid standing position at all.
	if( !SV_CheckBottom( actor, MOVE_NORMAL ))
	{
		actor->v.flags |= FL_PARTIALGROUND;
	}
}

void SV_MoveToOrigin( edict_t *ent, const vec3_t pflGoal, float dist, int iMoveType )
{
	vec3_t	vecDist;

	VectorCopy( pflGoal, vecDist );

	if( ent->v.flags & ( FL_FLY|FL_SWIM|FL_ONGROUND ))
	{
		if( iMoveType == MOVE_NORMAL )
		{
			if( SV_StepDirection( ent, ent->v.ideal_yaw, dist ) == 0 )
			{
				SV_NewChaseDir( ent, vecDist, dist );
			}
		}
		else
		{
			vecDist[0] -= ent->v.origin[0];
			vecDist[1] -= ent->v.origin[1];

			if( ent->v.flags & ( FL_FLY|FL_SWIM ))
				vecDist[2] -= ent->v.origin[2];
			else vecDist[2] = 0.0f;

			VectorNormalize( vecDist );
			VectorScale( vecDist, dist, vecDist );
			SV_FlyDirection( ent, vecDist );
		}
	}
}