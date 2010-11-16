//=======================================================================
//			Copyright XashXT Group 2010 ©
//		        vgi_int.c - vgui dll interaction
//=======================================================================

#include "common.h"
#include "client.h"
#include "const.h"

#include<VGUI.h>
#include<VGUI_App.h>
#include<VGUI_Panel.h>
#include<VGUI_SurfaceBase.h>
#include<VGUI_Surface.h>
#include<VGUI_SurfaceGL.h>
#include<VGUI_ActionSignal.h>
#include<VGUI_BorderLayout.h>

using namespace vgui;

SurfaceGL::SurfaceGL( Panel* embeddedPanel ):Surface( embeddedPanel )
{
	_embeddedPanel = embeddedPanel;
}

void SurfaceGL::texturedRect( int x0, int y0, int x1, int y1 )
{
	Msg( "texturedRect: %i %i %i %i\n", x0, y0, x1, y1 );
}

bool SurfaceGL:: recreateContext()
{
	return true;
}

void SurfaceGL:: createPopup( Panel *embeddedPanel )
{
}
	
void SurfaceGL:: pushMakeCurrent( Panel* panel, bool useInsets )
{
}

void SurfaceGL:: popMakeCurrent( Panel* panel )
{
}

void SurfaceGL:: makeCurrent( void )
{
}

void SurfaceGL::swapBuffers( void )
{
}

void SurfaceGL::setColor( int r, int g, int b )
{
}

void SurfaceGL::filledRect( int x0, int y0, int x1, int y1 )
{
}

void SurfaceGL::outlinedRect( int x0, int y0, int x1, int y1 )
{
}

void SurfaceGL::setTextFont( Font* font )
{
}

void SurfaceGL::setTextColor( int r, int g, int b )
{
	Msg( "setTextColor: %i %i %i\n", r, g, b );
}

void SurfaceGL::setDrawPos( int x, int y )
{
	Msg( "setDrawPos: %i %i\n", x, y );
}

void SurfaceGL::printText( const char *str, int strlen )
{
	Msg( "printText: %s\n", str );
}

void SurfaceGL::setTextureRGBA( int id, const char *rgba, int wide, int tall )
{
	Msg( "setTextureRGBA: %i\n", id );
}

void SurfaceGL::setTexture( int id )
{
	Msg( "setTexture: %i\n", id );
}

class CEngineApp : public App
{
public:
	CEngineApp( void )
	{
		reset();
	}

	virtual void main( int argc, char* argv[] )
	{
	}

	virtual void setCursorPos( int x, int y )
	{
		Msg( "setCursorPos: %i %i\n", x, y );
	}
	
	virtual void getCursorPos( int &x,int &y )
	{
		App::getCursorPos( x, y );
		Msg( "getCursorPos: %i %i\n", x, y );
	}
};
	
class CEnginePanel : public Panel, public CEngineApp
{
	typedef Panel BaseClass;
public:
	CEnginePanel( void )
	{
		_surfaceBase = new SurfaceGL( this );
		setSurfaceBaseTraverse( _surfaceBase );
		setParent( NULL );
	}
};

CEnginePanel	*rootpanel = NULL;

void VGui_Startup( void )
{
	if( rootpanel )
	{
		rootpanel->reset();
		rootpanel->setSize( menu.globals->scrWidth, menu.globals->scrHeight );
		return;
	}

	rootpanel = new CEnginePanel();
	rootpanel->setLayout( new BorderLayout( 0 ));
	rootpanel->setEnabled( true );
	rootpanel->setVisible( true );

	ASSERT( rootpanel->getApp() != NULL );
	ASSERT( rootpanel->getSurfaceBase() != NULL );
}

void VGui_Paint( void )
{
	if( !rootpanel ) return;

	rootpanel->solveTraverse();
	rootpanel->paintTraverse();
}

void VGui_ViewportPaintBackground( int extents[4] )
{
	Msg( "paint()\n" );
}

void *VGui_GetPanel( void )
{
	return (void *)rootpanel;
}