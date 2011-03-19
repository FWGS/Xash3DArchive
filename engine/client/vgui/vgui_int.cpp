//=======================================================================
//			Copyright XashXT Group 2010 ©
//		        vgui_int.c - vgui dll interaction
//=======================================================================

#include "common.h"
#include "client.h"
#include "const.h"
#include "vgui_draw.h"
#include "vgui_main.h"

CEnginePanel	*rootpanel = NULL;
CEngineSurface	*surface = NULL;
CEngineApp          *pApp = NULL;

SurfaceBase* CEnginePanel::getSurfaceBase( void )
{
	return surface;
}

App* CEnginePanel::getApp( void )
{
	return pApp;
}

void CEngineApp :: setCursorPos( int x, int y )
{
	POINT pt;

	pt.x = x;
	pt.y = y;

	ClientToScreen( (HWND)host.hWnd, &pt );

	::SetCursorPos( pt.x, pt.y );
}
	
void CEngineApp :: getCursorPos( int &x,int &y )
{
	POINT	pt;

	// find mouse movement
	::GetCursorPos( &pt );
	ScreenToClient((HWND)host.hWnd, &pt );

	x = pt.x;
	y = pt.y;
}

void VGui_SetBounds( void )
{
}

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
	pApp->start();
	pApp->setMinimumTickMillisInterval( 0 );

	surface = new CEngineSurface( rootpanel );

	ASSERT( rootpanel->getApp() != NULL );
	ASSERT( rootpanel->getSurfaceBase() != NULL );

	VGUI_DrawInit ();
}

void VGui_Shutdown( void )
{
	if( pApp ) pApp->stop();

	delete rootpanel;
	delete surface;
	delete pApp;

	rootpanel = NULL;
	surface = NULL;
	pApp = NULL;
}

void Vgui_Paint( int x, int y, int right, int bottom )
{
	if( !rootpanel ) return;

	// setup the base panel to cover the screen
	Panel *pVPanel = surface->getEmbeddedPanel();
	if( !pVPanel ) return;

	if( cls.key_dest == key_game )
		pApp->externalTick();

	pVPanel->setBounds( 0, 0, right, bottom );
	pVPanel->repaintAll();

	// paint everything 
	pVPanel->paintTraverse();
}

void VGui_Paint( void )
{
	POINT	pnt = { 0, 0 };
	RECT	rect;

	if( cls.state != ca_active || !rootpanel )
		return;

	host.input_enabled = rootpanel->isVisible();
	ClientToScreen( host.hWnd, &pnt );
	GetClientRect( host.hWnd, &rect );
	EnableScissor( true );

	Vgui_Paint( pnt.x, pnt.y, rect.right, rect.bottom );

	EnableScissor( false );
}

void VGui_ViewportPaintBackground( int extents[4] )
{
	if( !rootpanel ) return;

//	Msg( "Vgui_ViewportPaintBackground( %i, %i, %i, %i )\n", extents[0], extents[1], extents[2], extents[3] );
	Panel *pVPanel = surface->getPanel();
	if( !pVPanel ) return;

//	rootpanel->paintBackground();
}

void *VGui_GetPanel( void )
{
	return (void *)rootpanel;
}