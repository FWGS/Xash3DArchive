//=======================================================================
//			Copyright XashXT Group 2007 �
//		         cm_rigidbody.c - rigid body stuff
//=======================================================================

#include "physic.h"
#include "mathlib.h"

physbody_t *Phys_CreateBody( sv_edict_t *ed, void *buffer, matrix4x3 transform, int solid )
{
	NewtonCollision	*col;
	NewtonBody	*body;
	matrix4x4		trans, offset;
	float		*vertices;
	int		numvertices;		
	vec3_t		size, center, mins, maxs;
	studiohdr_t	*phdr = (studiohdr_t *)buffer;
	mstudioseqdesc_t	*pseqdesc;

	// identity matrixes
	MatrixLoadIdentity( trans );
	MatrixLoadIdentity( offset );

	if( phdr )
	{
		// custom convex hull
		pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
		VectorCopy( pseqdesc[0].bbmin, mins );
		VectorCopy( pseqdesc[0].bbmax, maxs );

		if( solid == SOLID_BOX )
		{
			CM_RoundUpHullSize( mins, false );
			CM_RoundUpHullSize( maxs, false );
		}
	}
	else
	{
		// default size
		VectorSet( mins, -32, -32, -32 );
		VectorSet( maxs,  32,  32,  32 );
		MsgWarn("can't compute bounding box, use default size\n");
	}

	// setup offset matrix
	VectorSubtract( maxs, mins, size );
	VectorAdd( mins, maxs, center );
	ConvertDimensionToPhysic( size );
	ConvertDimensionToPhysic( center );
	VectorScale( center, 0.5, offset[3] );
	
	// setup translation matrix
	VectorCopy( transform[0], trans[0] );
	VectorCopy( transform[1], trans[1] );
	VectorCopy( transform[2], trans[2] );
	VectorCopy( transform[3], trans[3] );

	switch(solid)
	{          
	case SOLID_BOX:
		col = NewtonCreateBox( gWorld, size[0], size[1], size[2], &offset[0][0] );
		break;
	case SOLID_SPHERE:
		col = NewtonCreateSphere( gWorld, size[0]/2, size[1]/2, size[2]/2, &offset[0][0] );
		break;
	case SOLID_CYLINDER:
		col = NewtonCreateCylinder( gWorld, size[0], size[1], &offset[0][0] );
		break;
	case SOLID_MESH:
		vertices = pi.GetModelVerts( ed, &numvertices );
		if(!vertices || !numvertices ) return NULL;
		col = NewtonCreateConvexHull( gWorld, numvertices, vertices, sizeof(vec3_t), &offset[0][0] );
		break;
	default:
		Host_Error("Phys_CreateBody: unsupported solid type\n");
		return NULL;
	}
	body = NewtonCreateBody( gWorld, col );
	NewtonBodySetUserData( body, ed );
	NewtonBodySetMassMatrix( body, 10.0f, size[0], size[1], size[2] ); // 10 kg

	// setup generic callback to engine.dll
	NewtonBodySetTransformCallback(body, Callback_ApplyTransform );
	NewtonBodySetForceAndTorqueCallback( body, Callback_ApplyForce );
	NewtonBodySetMatrix(body, &trans[0][0]);// origin
	NewtonReleaseCollision( gWorld, col );

	return (physbody_t *)body;
}

void Phys_RemoveBody( physbody_t *body )
{
	if( body ) NewtonDestroyBody( gWorld, (NewtonBody*)body );
}

bool Phys_GetForce( physbody_t *body, vec3_t velocity, vec3_t avelocity, vec3_t force, vec3_t torque )
{
	if(!body) return false;

	NewtonBodyGetOmega( body, avelocity );
	NewtonBodyGetVelocity( body, velocity );
	NewtonBodyGetForce( body, force );
	NewtonBodyGetTorque( body, torque );

	ConvertPositionToGame( avelocity );
	ConvertPositionToGame( velocity );	
	return true;
}

void Phys_SetForce( physbody_t *body, vec3_t velocity, vec3_t avelocity, vec3_t force, vec3_t torque )
{
	if(!body) return;
	ConvertPositionToPhysic( avelocity );
	ConvertPositionToPhysic( velocity );
	NewtonBodySetOmega( body, avelocity );
	NewtonBodySetVelocity( body, velocity );
	NewtonBodySetForce( body, force );
	NewtonBodySetTorque( body, torque );
}

bool Phys_GetMassCentre( physbody_t *body, matrix3x3 mass )
{
	if(!body) return false;
	NewtonBodyGetCentreOfMass( body, (float *)mass );
	return true;
}

void Phys_SetMassCentre( physbody_t *body, matrix3x3 mass )
{
	if(!body) return;
	NewtonBodySetCentreOfMass( body, (float *)mass );
}