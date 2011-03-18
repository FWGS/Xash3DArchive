//=======================================================================
//			Copyright XashXT Group 2010 ©
//		        vgui_int.c - vgui dll interaction
//=======================================================================

#include "common.h"
#include "client.h"
#include "const.h"
#include "vgui_draw.h"
#include "font_cache.h"

static FontCache g_FontCache;

class CEngineSurface : public SurfaceBase
{
private:
	struct paintState_t
	{
		Panel	*m_pPanel;
		int	iTranslateX;
		int	iTranslateY;
		int	iScissorLeft;
		int	iScissorRight;
		int	iScissorTop;
		int	iScissorBottom;
	};

	// point translation for current panel
	int		_translateX;
	int		_translateY;

	// the size of the window to draw into
	int		_surfaceExtents[4];

	CUtlVector <paintState_t> _paintStack;

	void SetupPaintState( const paintState_t &paintState )
	{
		_translateX = paintState.iTranslateX;
		_translateY = paintState.iTranslateY;
		SetScissorRect( paintState.iScissorLeft, paintState.iScissorTop, 
		paintState.iScissorRight, paintState.iScissorBottom );
	}

	void InitVertex( vpoint_t &vertex, int x, int y, float u, float v )
	{
		vertex.point[0] = x + _translateX;
		vertex.point[1] = y + _translateY;
		vertex.coord[0] = u;
		vertex.coord[1] = v;
	}
public:
	CEngineSurface( Panel *embeddedPanel ):SurfaceBase( embeddedPanel )
	{
		_embeddedPanel = embeddedPanel;
		_drawColor[0] = _drawColor[1] = _drawColor[2] = _drawColor[3] = 255;
		_drawTextColor[0] = _drawTextColor[1] = _drawTextColor[2] = _drawTextColor[3] = 255;

		// FIXME: this is right?
		_surfaceExtents[0] = _surfaceExtents[1] = 0;
		_surfaceExtents[2] = menu.globals->scrWidth;
		_surfaceExtents[3] = menu.globals->scrHeight;
		_drawTextPos[0] = _drawTextPos[1] = 0;
		_hCurrentFont = null;
	}
public:
	virtual void setTitle( const char *title )
	{
		Msg( "SetTitle: %s\n", title );
	}

	virtual Panel *GetEmbeddedPanel( void )
	{
		return _embeddedPanel;
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

	virtual int getModeInfoCount( void )
	{
		Msg( "getModeInfoCount()\n" );		
		return 0;
	}

	virtual void createPopup( Panel* embeddedPanel )
	{
	}

	virtual bool hasFocus( void )
	{
		return false;
	}

	virtual bool isWithin( int x, int y )
	{
		return false;
	}
protected:
	virtual int createNewTextureID( void )
	{
		return VGUI_GenerateTexture();
	}

	virtual void drawSetColor( int r, int g, int b, int a )
	{
		_drawColor[0] = (byte)r;
		_drawColor[1] = (byte)g;
		_drawColor[2] = (byte)b;
		_drawColor[3] = (byte)a;
	}

	virtual void drawSetTextColor( int r, int g, int b, int a )
	{
		_drawTextColor[0] = (byte)r;
		_drawTextColor[1] = (byte)g;
		_drawTextColor[2] = (byte)b;
		_drawTextColor[3] = (byte)a;
	}

	virtual void drawFilledRect( int x0, int y0, int x1, int y1 )
	{
		vpoint_t rect[2];
		vpoint_t clippedRect[2];

		InitVertex( rect[0], x0, y0, 0, 0 );
		InitVertex( rect[1], x1, y1, 0, 0 );

		// fully clipped?
		if( !ClipRect( rect[0], rect[1], &clippedRect[0], &clippedRect[1] ))
			return;	

		VGUI_SetupDrawingRect( _drawColor );	
		VGUI_EnableTexture( false );
		VGUI_DrawQuad( &clippedRect[0], &clippedRect[1] );
		VGUI_EnableTexture( true );
	}
	virtual void drawOutlinedRect( int x0,int y0,int x1,int y1 )
	{
		drawFilledRect( x0, y0, x1, y0 + 1 );		// top
		drawFilledRect( x0, y1 - 1, x1, y1 );		// bottom
		drawFilledRect( x0, y0 + 1, x0 + 1, y1 - 1 );	// left
		drawFilledRect( x1 - 1, y0 + 1, x1, y1 - 1 );	// right
	}
	
	virtual void drawSetTextFont( Font *font )
	{
		_hCurrentFont = font;
	}

	virtual void drawSetTextPos( int x, int y )
	{
		_drawTextPos[0] = x;
		_drawTextPos[1] = y;
	}

	virtual void drawPrintText( const char* text, int textLen )
	{
		if( !text || !_hCurrentFont )
			return;

		int x = _drawTextPos[0] + _translateX;
		int y = _drawTextPos[1] + _translateY;

		int iTall = _hCurrentFont->getTall();

		int iTotalWidth = 0;
		for( int i = 0; i < textLen; i++ )
		{
			char ch = text[i];

			int abcA,abcB,abcC;
			_hCurrentFont->getCharABCwide( ch, abcA, abcB, abcC );

			iTotalWidth += abcA;
			int iWide = abcB;

			if( !iswspace( ch ))
			{
				// get the character texture from the cache
				int iTexId = 0;
				float *texCoords = NULL;

				if( !g_FontCache.GetTextureForChar( _hCurrentFont, ch, &iTexId, &texCoords ))
					continue;

				Assert( texCoords != NULL );

				vpoint_t ul, lr;

				ul.point[0] = x + iTotalWidth;
				ul.point[1] = y;
				lr.point[0] = ul.point[0] + iWide;
				lr.point[1] = ul.point[1] + iTall;

				// gets at the texture coords for this character in its texture page
				ul.coord[0] = texCoords[0];
				ul.coord[1] = texCoords[1];
				lr.coord[0] = texCoords[2];
				lr.coord[1] = texCoords[3];
                                        
				drawSetTexture( iTexId );
				VGUI_SetupDrawingText( _drawTextColor );
				VGUI_DrawQuad( &ul, &lr );	// draw the letter
			}

			iTotalWidth += iWide + abcC;
		}

		_drawTextPos[0] += iTotalWidth;
	}

	virtual void drawSetTextureRGBA( int id, const char* rgba, int wide, int tall )
	{
		VGUI_UploadTexture( id, rgba, wide, tall );
	}
	
	virtual void drawSetTexture( int id )
	{
		VGUI_BindTexture( id );
	}
	
	virtual void drawTexturedRect( int x0, int y0, int x1, int y1 )
	{
		vpoint_t rect[2];
		vpoint_t clippedRect[2];

		InitVertex( rect[0], x0, y0, 0, 0 );
		InitVertex( rect[1], x1, y1, 1, 1 );

		// fully clipped?
		if( !ClipRect( rect[0], rect[1], &clippedRect[0], &clippedRect[1] ))
			return;	

		VGUI_SetupDrawingImage();	
		VGUI_DrawQuad( &clippedRect[0], &clippedRect[1] );
	}

	virtual void invalidate( Panel *panel )
	{
//		Msg( "invalidate()\n" );
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
	
	virtual void pushMakeCurrent( Panel* panel, bool useInsets )
	{
		int inSets[4] = { 0, 0, 0, 0 };
		int absExtents[4];
		int clipRect[4];

		if( useInsets )
		{
			panel->getInset( inSets[0], inSets[1], inSets[2], inSets[3] );
		}

		panel->getAbsExtents( absExtents[0], absExtents[1], absExtents[2], absExtents[3] );
		panel->getClipRect( clipRect[0], clipRect[1], clipRect[2], clipRect[3] );

		int i = _paintStack.AddToTail();
		paintState_t &paintState = _paintStack[i];
		paintState.m_pPanel = panel;

		// determine corrected top left origin
		paintState.iTranslateX = inSets[0] + absExtents[0] - _surfaceExtents[0];	
		paintState.iTranslateY = inSets[1] + absExtents[1] - _surfaceExtents[1];

		// setup clipping rectangle for scissoring
		paintState.iScissorLeft = clipRect[0] - _surfaceExtents[0];
		paintState.iScissorTop = clipRect[1] - _surfaceExtents[1];
		paintState.iScissorRight = clipRect[2] - _surfaceExtents[0];
		paintState.iScissorBottom = clipRect[3] - _surfaceExtents[1];

		SetupPaintState( paintState );
	}
	
	virtual void popMakeCurrent( Panel* panel )
	{
		int top = _paintStack.Count() - 1;

		// more pops that pushes?
		Assert( top >= 0 );

		// didn't pop in reverse order of push?
		Assert( _paintStack[top].m_pPanel == panel );

		_paintStack.Remove( top );
	
		if( top > 0 ) SetupPaintState( _paintStack[top-1] );
	}
	
	virtual void applyChanges( void )
	{
	}
protected:
	Font* _hCurrentFont;
	int   _drawTextPos[2];
	uchar _drawColor[4];
	uchar _drawTextColor[4];
	friend class App;
	friend class Panel;
};

class CEngineApp : public App
{
public:
	CEngineApp( bool externalMain = true ):App( externalMain )
	{
	}

	virtual void main( int argc, char* argv[] )
	{
		Msg( "App main()\n" );
	}

	virtual void setCursorPos( int x, int y )
	{
		App::setCursorPos( x, y );
	}
	
	virtual void getCursorPos( int &x,int &y )
	{
		App::getCursorPos( x, y );
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
friend class Panel;
friend class App;
friend class SurfaceBase;
friend class Image;
};

CEnginePanel	*rootpanel = NULL;

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
	Panel *pVPanel = surface->GetEmbeddedPanel();
	if( !pVPanel ) return;

	if( cls.key_dest == key_game )
		pApp->externalTick();

	pVPanel->setBounds( 0, 0, right, bottom );
	pVPanel->repaint();

	// paint everything 
	pVPanel->paintTraverse();
}

void VGui_Paint( void )
{
	POINT	pnt = { 0, 0 };
	RECT	rect;

	if( cls.state != ca_active || !rootpanel )
		return;

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