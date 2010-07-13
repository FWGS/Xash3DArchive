//=======================================================================
//			Copyright XashXT Group 2010 ©
//		      r_tracer.cpp - draw & cull the tracer
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "triangle_api.h"
#include "effects_api.h"
#include "ref_params.h"
#include "pm_movevars.h"
#include "ev_hldm.h"
#include "hud.h"
#include "r_particle.h"
#include "r_tempents.h"

//-----------------------------------------------------------------------------
// Sees if the tracer is behind the camera or should be culled
//-----------------------------------------------------------------------------
static bool ClipTracer( const Vector &start, const Vector &delta, Vector &clippedStart, Vector &clippedDelta )
{
	// dist1 = start dot forward - origin dot forward
	// dist2 = (start + delta ) dot forward - origin dot forward
	// in camera space this is -start[2] since origin = 0 and vecForward = (0, 0, -1)
	float dist1 = -start[2];
	float dist2 = dist1 - delta[2];
	
	// clipped, skip this tracer
	if( dist1 <= 0 && dist2 <= 0 )
		return true;

	clippedStart = start;
	clippedDelta = delta;
	
	// needs to be clipped
	if ( dist1 <= 0 || dist2 <= 0 )
	{
		float fraction = dist2 - dist1;

		// Too close to clipping plane
		if ( fraction < 1e-3 )
			return true;

		fraction = -dist1 / fraction;

		if ( dist1 <= 0 )
		{
			clippedStart = start + delta * fraction;
		}
		else
		{
			clippedDelta = delta * fraction;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Computes the four verts to draw the tracer with
//-----------------------------------------------------------------------------
static bool Tracer_ComputeVerts( const Vector &start, const Vector &delta, float width, Vector *pVerts )
{
	Vector clippedStart, clippedDelta;

	// Clip the tracer
	if ( ClipTracer( start, delta, clippedStart, clippedDelta ))
		return false;

	// Figure out direction in camera space of the normal
	Vector normal;

	normal = CrossProduct( clippedDelta, clippedStart );
					  
	// don't draw if they are parallel
	float sqLength = DotProduct( normal, normal );
	if ( sqLength < 1e-3 )
		return false;

	// Resize the normal to be appropriate based on the width
	normal *= ( 0.5f * width / sqrt( sqLength ));

	// compute four vertexes
	pVerts[0] = clippedStart - normal;
	pVerts[1] = clippedStart + normal;
	pVerts[2] = pVerts[0] + clippedDelta;
	pVerts[3] = pVerts[1] + clippedDelta;

	return true;
}


//-----------------------------------------------------------------------------
// draw a tracer.
//-----------------------------------------------------------------------------
void Tracer_Draw( HSPRITE hSprite, Vector& start, Vector& delta, float width, byte *color, float startV, float endV )
{
	// Clip the tracer
	Vector verts[4];

	if ( !Tracer_ComputeVerts( start, delta, width, verts ))
		return;

	// NOTE: Gotta get the winding right so it's not backface culled
	// (we need to turn of backface culling for these bad boys)

	g_engfuncs.pTriAPI->Enable( TRI_SHADER );
	g_engfuncs.pTriAPI->RenderMode( kRenderTransTexture );
	if ( color ) g_engfuncs.pTriAPI->Color4ub( color[0], color[1], color[2], color[3] );
	else g_engfuncs.pTriAPI->Color4ub( 255, 255, 255, 255 );

	g_engfuncs.pTriAPI->Bind( hSprite, 0 );
	g_engfuncs.pTriAPI->Begin( TRI_QUADS );

	g_engfuncs.pTriAPI->TexCoord2f( 0.0f, startV );
	g_engfuncs.pTriAPI->Vertex3fv( verts[0] );

	g_engfuncs.pTriAPI->TexCoord2f( 1.0f, startV );
	g_engfuncs.pTriAPI->Vertex3fv( verts[1] );

	g_engfuncs.pTriAPI->TexCoord2f( 1.0f, endV );
	g_engfuncs.pTriAPI->Vertex3fv( verts[3] );

	g_engfuncs.pTriAPI->TexCoord2f( 0.0f, endV );
	g_engfuncs.pTriAPI->Vertex3fv( verts[2] );

	g_engfuncs.pTriAPI->End();
	g_engfuncs.pTriAPI->Disable( TRI_SHADER );
}