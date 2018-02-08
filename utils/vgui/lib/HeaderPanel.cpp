//========= Copyright ?1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include<VGUI_HeaderPanel.h>
#include<VGUI_App.h>
#include<VGUI_InputSignal.h>
#include<VGUI_ChangeSignal.h>

using namespace vgui;

namespace
{
class HeaderPanelSignal : public InputSignal
{
private:
	HeaderPanel* _headerPanel;
public:
	HeaderPanelSignal(HeaderPanel* headerPanel)
	{
		_headerPanel=headerPanel;
	}
public:
	virtual void cursorMoved(int x,int y,Panel* panel)
	{
		_headerPanel->privateCursorMoved(x,y,panel);
	}
	virtual void cursorEntered(Panel* panel)
	{
	}
	virtual void cursorExited(Panel* panel)
	{
	}
	virtual void mouseDoublePressed(MouseCode code,Panel* panel)
	{
	}
	virtual void mousePressed(MouseCode code,Panel* panel)
	{
		_headerPanel->privateMousePressed(code,panel);
	}
	virtual void mouseReleased(MouseCode code,Panel* panel)
	{
		_headerPanel->privateMouseReleased(code,panel);
	}
	virtual void mouseWheeled(int delta,Panel* panel)
	{
	}
	virtual void keyPressed(KeyCode code,Panel* panel)
	{
	}
	virtual void keyTyped(KeyCode code,Panel* panel)
	{
	}
	virtual void keyReleased(KeyCode code,Panel* panel)
	{
	}
	virtual void keyFocusTicked(Panel* panel)
	{
	}
};
}

HeaderPanel::HeaderPanel(int x,int y,int wide,int tall) : Panel(x,y,wide,tall)
{
	_sliderWide=11;
	_dragging=false;
	_sectionLayer=new Panel(0,0,wide,tall);
	_sectionLayer->setPaintBorderEnabled(false);
	_sectionLayer->setPaintBackgroundEnabled(false);
	_sectionLayer->setPaintEnabled(false);
	_sectionLayer->setParent(this);
}

void HeaderPanel::addSectionPanel(Panel* panel)
{
	invalidateLayout(true);

	int x=0;
	int y=999999999,wide=999999999,tall=999999999; // uninitialize
	for(int i=0;i<_sectionPanelDar.getCount();i++)
	{
		_sectionPanelDar[i]->getBounds(x,y,wide,tall);
		x+=wide+_sliderWide;
	}
	_sectionPanelDar.addElement(panel);

	panel->setPos(x,0);
	panel->setParent(_sectionLayer);
	panel->setBounds(x,y,wide,tall);

	getPaintSize(wide,tall);

	Panel* slider=new Panel(0,0,_sliderWide,tall);
	slider->setPaintBorderEnabled(false);
	slider->setPaintBackgroundEnabled(false);
	slider->setPaintEnabled(false);
	slider->setPos(x+wide,0);
	slider->addInputSignal(new HeaderPanelSignal(this));
	slider->setCursor(getApp()->getScheme()->getCursor(Scheme::scu_sizewe));
	slider->setParent(this);
	_sliderPanelDar.addElement(slider);

	invalidateLayout(false);
	fireChangeSignal();
	repaint();
}

void HeaderPanel::setSliderPos(int sliderIndex,int pos)
{
	_sliderPanelDar[sliderIndex]->setPos(pos-_sliderWide/2,0);

	invalidateLayout(false);
	fireChangeSignal();
	repaint();
}

int HeaderPanel::getSectionCount()
{
	return _sectionPanelDar.getCount();
}

void HeaderPanel::getSectionExtents(int sectionIndex,int& x0,int& x1)
{
	int x,y,wide,tall;
	_sectionPanelDar[sectionIndex]->getBounds(x,y,wide,tall);
	x0=x;
	x1=x+wide;
}

void HeaderPanel::addChangeSignal(ChangeSignal* s)
{
	_changeSignalDar.putElement(s);
}

void HeaderPanel::fireChangeSignal()
{
	invalidateLayout(true);

	for(int i=0;i<_changeSignalDar.getCount();i++)
	{
		_changeSignalDar[i]->valueChanged(this);
	}
}

void HeaderPanel::performLayout()
{
	int x0=0;
	int x1;
	int x,y,wide,tall;
	getPaintSize(wide,tall);
	_sectionLayer->setBounds(0,0,wide,tall);

	for(int i=0;i<_sectionPanelDar.getCount();i++)
	{
		Panel* slider=_sliderPanelDar[i];
		slider->getPos(x,y);
		x1=x+(_sliderWide/2);

		Panel* panel=_sectionPanelDar[i];
		panel->setBounds(x0,0,x1-x0,tall);
		x0=x1;
	}
}

void HeaderPanel::privateCursorMoved(int x,int y,Panel* panel)
{
	if(!_dragging)
	{
		return;
	}

	getApp()->getCursorPos(x,y);
	screenToLocal(x,y);

	setSliderPos(_dragSliderIndex,x+_dragSliderStartPos-_dragSliderStartX);
	invalidateLayout(false);
	repaint();
}

void HeaderPanel::privateMousePressed(MouseCode code,Panel* panel)
{
	int mx,my;
	getApp()->getCursorPos(mx,my);
	screenToLocal(mx,my);

	for(int i=0;i<_sliderPanelDar.getCount();i++)
	{
		Panel* slider=_sliderPanelDar[i];
		if(slider==panel)
		{
			int x,y;
			panel->getPos(x,y);
			_dragging=true;
			_dragSliderIndex=i;
			_dragSliderStartPos=x+(_sliderWide/2);
			_dragSliderStartX=mx;
			panel->setAsMouseCapture(true);
			break;
		}
	}
}

void HeaderPanel::privateMouseReleased(MouseCode code,Panel* panel)
{
}
