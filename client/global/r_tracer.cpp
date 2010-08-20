//=======================================================================
//			Copyright XashXT Group 2010 ©
//		      r_tracer.cpp - draw & cull the tracer
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "triangle_api.h"
#include "r_efx.h"
#include "pm_movevars.h"
#include "ev_hldm.h"
#include "hud.h"
#include "r_particle.h"
#include "r_tempents.h"

//-----------------------------------------------------------------------------
// Sees if the tracer is behind the camera or should be culled
//-----------------------------------------------------------------------------
static bool CullTracer( const Vector &start, const Vector &end )
{
	Vector mins, maxs;

	// compute the bounding box
	for ( int i = 0; i < 3; i++ )
	{
		if ( start[i] < end[i] )
		{
			mins[i] = start[i];
			maxs[i] = end[i];
		}
		else
		{
			mins[i] = end[i];
			maxs[i] = start[i];
		}
		
		// Don't let it be zero sized
		if ( mins[i] == maxs[i] )
		{
			maxs[i] += 1;
		}
	}

	// check bbox
	if( gEngfuncs.pEfxAPI->R_CullBox( mins, maxs ) )
		return true; // culled

	return false;
}

//-----------------------------------------------------------------------------
// Computes the four verts to draw the tracer with
//-----------------------------------------------------------------------------
bool CParticleSystem :: TracerComputeVerts( const Vector &pos, const Vector &delta, float width, Vector *pVerts )
{
	Vector start, end;

	start = pos;
	end = start + delta;

	// Clip the tracer
	if ( CullTracer( start, end ))
		return false;

	Vector screenStart, screenEnd;

	// transform point into the screen space
	gEngfuncs.pTriAPI->WorldToScreen( (float *)start, screenStart );
	gEngfuncs.pTriAPI->WorldToScreen( (float *)end, screenEnd );
	
	Vector tmp, normal;
	
	tmp = screenStart - screenEnd;
	// We don't need Z, we're in screen space
	tmp.z = 0;
	tmp = tmp.Normalize();

	normal = m_vecUp * tmp.x;	// Build point along noraml line (normal is -y, x)
	normal = normal - ( m_vecRight * -tmp.y );

	// compute four vertexes
	pVerts[0] = start - normal * width;
	pVerts[1] = start + normal * width;
	pVerts[2] = pVerts[0] + delta;
	pVerts[3] = pVerts[1] + delta;

	return true;
}


//-----------------------------------------------------------------------------
// draw a tracer.
//-----------------------------------------------------------------------------
void CParticleSystem :: DrawTracer( HSPRITE hSprite, Vector& start, Vector& delta, float width, byte *color, float startV, float endV )
{
	// Clip the tracer
	Vector verts[4];

	if ( !TracerComputeVerts( start, delta, width, verts ))
		return;

	// NOTE: Gotta get the winding right so it's not backface culled
	// (we need to turn of backface culling for these bad boys)

	gEngfuncs.pTriAPI->Enable( TRI_SHADER );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
	if ( color ) gEngfuncs.pTriAPI->Color4ub( color[0], color[1], color[2], color[3] );
	else gEngfuncs.pTriAPI->Color4ub( 255, 255, 255, 255 );

	gEngfuncs.pTriAPI->Bind( hSprite, 0 );
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );

	gEngfuncs.pTriAPI->TexCoord2f( 0.0f, endV );
	gEngfuncs.pTriAPI->Vertex3fv( verts[2] );

	gEngfuncs.pTriAPI->TexCoord2f( 1.0f, endV );
	gEngfuncs.pTriAPI->Vertex3fv( verts[3] );

	gEngfuncs.pTriAPI->TexCoord2f( 1.0f, startV );
	gEngfuncs.pTriAPI->Vertex3fv( verts[1] );

	gEngfuncs.pTriAPI->TexCoord2f( 0.0f, startV );
	gEngfuncs.pTriAPI->Vertex3fv( verts[0] );

	gEngfuncs.pTriAPI->End();
	gEngfuncs.pTriAPI->Disable( TRI_SHADER );
}