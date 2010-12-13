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
#include<VGUI_ActionSignal.h>
#include<VGUI_BorderLayout.h>

using namespace vgui;

class CEngineSurface : public SurfaceBase
{
public:
	CEngineSurface( Panel *embeddedPanel ):SurfaceBase( embeddedPanel )
	{
		_embeddedPanel = embeddedPanel;
	}
public:
	virtual void setTitle( const char *title )
	{
		Msg( "SetTitle: %s\n", title );
	}

	virtual bool setFullscreenMode( int wide, int tall, int bpp )
	{
		return false;
	}
	
	virtual void setWindowedMode( void )
	{
	}

	virtual void setAsTopMost( bool state )
	{
	}

	virtual int  getModeInfoCount( void )
	{
		Msg( "getModeInfoCount()\n" );		
		return 0;
	}

	virtual void createPopup( Panel* embeddedPanel )
	{
	}

	virtual bool hasFocus( void )
	{
		Msg( "hasFocus()\n" );
		return false;
	}

	virtual bool isWithin( int x, int y )
	{
		Msg( "isWithin()\n" );
		return false;
	}
protected:
	virtual int createNewTextureID( void )
	{
		Msg( "createNewTextureID()\n" );
		return 0;
	}

	virtual void drawSetColor( int r, int g, int b, int a )
	{
	}

	virtual void drawFilledRect( int x0, int y0, int x1, int y1 )
	{
	}

	virtual void drawOutlinedRect( int x0,int y0,int x1,int y1 )
	{
	}
	
	virtual void drawSetTextFont( Font *font )
	{
	}

	virtual void drawSetTextColor( int r, int g, int b, int a )
	{
	}

	virtual void drawSetTextPos( int x, int y )
	{
	}

	virtual void drawPrintText( const char* text, int textLen )
	{
	}

	virtual void drawSetTextureRGBA( int id, const char* rgba, int wide, int tall )
	{
	}
	
	virtual void drawSetTexture( int id )
	{
	}
	
	virtual void drawTexturedRect( int x0, int y0, int x1, int y1 )
	{
	}

	virtual void invalidate( Panel *panel )
	{
	}
	
	virtual bool createPlat()
	{
		return false;
	}

	virtual bool recreateContext()
	{
		return false;
	}

	virtual void enableMouseCapture(bool state)
	{
	}
	
	virtual void setCursor(Cursor* cursor)
	{
	}
	
	virtual void swapBuffers()
	{
	}
	
	virtual void pushMakeCurrent(Panel* panel,bool useInsets)
	{
	}
	
	virtual void popMakeCurrent(Panel* panel)
	{
	}
	
	virtual void applyChanges( void )
	{
	}
protected:
	friend class App;
	friend class Panel;
};

class CEngineApp : public App
{
public:
	CEngineApp( void )
	{
		App::reset();
	}

	virtual void main( int argc, char* argv[] )
	{
		Msg( "App main()\n" );
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
	virtual App* getApp( void )
	{
		return this;
	}
};

CEngineSurface	*surface = NULL;
CEngineApp          *pApp = NULL;

class CEnginePanel : public Panel
{
public:
	CEnginePanel()
	{
		vgui::Panel();
	}
	CEnginePanel( int x, int y, int wide, int tall )
	{
		vgui::Panel( x, y, wide, tall );
	} 
	virtual SurfaceBase* getSurfaceBase( void )
	{
		return surface;
	}
	virtual App* getApp( void )
	{
		return pApp;
	}
	virtual void paintBackground( void )
	{
		for( int i = 0; i < getChildCount(); i++ )
		{
			Panel *pChild = getChild( i );
			pChild->repaintAll();
		}
	}
friend class Panel;
friend class App;
friend class SurfaceBase;
friend class Image;
};

CEnginePanel	*rootpanel = NULL;

void VGui_Startup( void )
{
	if( rootpanel )
	{
//		rootpanel->reset();
		rootpanel->setSize( menu.globals->scrWidth, menu.globals->scrHeight );
		return;
	}

	rootpanel = new CEnginePanel;
	rootpanel->setPaintBorderEnabled( false );
	rootpanel->setPaintBackgroundEnabled( false );
	rootpanel->setVisible( true );
	rootpanel->setCursor( new Cursor( Cursor::dc_none ));

	pApp = new CEngineApp;
	pApp->setMinimumTickMillisInterval( 0 );

	surface = new CEngineSurface( rootpanel );

	ASSERT( rootpanel->getApp() != NULL );
	ASSERT( rootpanel->getSurfaceBase() != NULL );
}

void VGui_Shutdown( void )
{
	delete rootpanel;
	delete surface;
	delete pApp;

	rootpanel = NULL;
	surface = NULL;
	pApp = NULL;
}

void VGui_Paint( void )
{
	if( !rootpanel ) return;

	rootpanel->paintBackground();
}

void VGui_ViewportPaintBackground( int extents[4] )
{
	if( !rootpanel ) return;

//	Msg( "ViewportPaintBackground()\n" );
	Panel *pVPanel = surface->getPanel();
	if( !pVPanel ) return;

	rootpanel->setBounds( extents[0], extents[1], extents[2], extents[3] );
	rootpanel->repaint();

	// paint everything 
	rootpanel->paintTraverse();
}

void *VGui_GetPanel( void )
{
	return (void *)rootpanel;
}