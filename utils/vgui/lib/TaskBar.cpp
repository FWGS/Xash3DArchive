//========= Copyright ?1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include<stdio.h>
#include<time.h>
#include<VGUI_TaskBar.h>
#include<VGUI_FocusChangeSignal.h>
#include<VGUI_FrameSignal.h>
#include<VGUI_ActionSignal.h>
#include<VGUI_TickSignal.h>
#include<VGUI_RaisedBorder.h>
#include<VGUI_LoweredBorder.h>
#include<VGUI_App.h>
#include<VGUI_Button.h>
#include<VGUI_Frame.h>

using namespace vgui;

namespace
{
class FooClock : public Panel, public TickSignal
{
public:
	FooClock(int x,int y,int wide,int tall) : Panel(x,y,wide,tall)
	{
		_hour=0;
		_minute=0;
		_second=0;
		getApp()->addTickSignal(this);
	}
	virtual void paintBackground()
	{
		char buf[50];
		if(_hour>12)
		{
			sprintf(buf,"%d:%.2d:%.2d PM",_hour-12,_minute,_second);
		}
		else
		{
			sprintf(buf,"%d:%.2d:%.2d AM",_hour,_minute,_second);
		}

		Panel::paintBackground();
		drawSetTextFont(Scheme::sf_primary1);
		drawSetTextColor(Scheme::sc_black);
		drawPrintText(buf,strlen(buf));
	}
	virtual void ticked()
	{
		struct tm* newtime;
		time_t aclock;
		time(&aclock);
		newtime=localtime(&aclock);

		int hour=newtime->tm_hour;
		int minute=newtime->tm_min;
		int second=newtime->tm_sec;
		if((_hour!=hour)||(_minute!=minute)||(_second!=second))
		{
			_hour=hour;
			_minute=minute;
			_second=second;
			repaint();
		}
	}
protected:
	int _hour;
	int _minute;
	int _second;
};

class FooTaskBarButtonHandler : public ActionSignal, public FrameSignal, public FocusChangeSignal
{
public:
	FooTaskBarButtonHandler(Button* button,Frame* frame)
	{
		_button=button;
		_frame=frame;
	}
public:
	virtual void actionPerformed(Panel* panel)
	{
		_frame->setVisible(true);
		_frame->requestFocus();
	}
	virtual void closing(Frame* frame)
	{
	}
	virtual void minimizing(Frame* frame,bool toTray)
	{
		_frame->setVisible(false);
		_frame->getApp()->requestFocus(null);
	}
	virtual void focusChanged(bool lost,Panel* panel)
	{
		_button->setSelected(!lost);
	}
protected:
	Button* _button;
	Frame* _frame;
};
}

TaskBar::TaskBar(int x,int y,int wide,int tall) : Panel(x,y,wide,tall)
{
	setBorder(new RaisedBorder());

	_tray=new Panel(100,0,120,26);
	_tray->setBorder(new LoweredBorder());
	_tray->setParent(this);

	FooClock* clock=new FooClock(25,2,85,20);
	clock->setParent(_tray);
}

void TaskBar::addFrame(Frame* frame)
{
	_frameDar.addElement(frame);

	char buf[256];
	frame->getTitle(buf,sizeof(buf));

	Button* button=new Button(buf,2,2);
	FooTaskBarButtonHandler* handler=new FooTaskBarButtonHandler(button,frame);
	button->setParent(this);
	button->addActionSignal(handler);
	frame->addFrameSignal(handler);
	frame->addFocusChangeSignal(handler);
	_taskButtonDar.addElement(button);

	invalidateLayout(false);
}

void TaskBar::performLayout()
{
	int wide,tall;
	getPaintSize(wide,tall);

	int twide,ttall;
	_tray->getSize(twide,ttall);
	_tray->setBounds(wide-twide-3,2,twide,tall-4);

	int xx=3;
	for(int i=0;i<_taskButtonDar.getCount();i++)
	{
		Button* button=_taskButtonDar[i];

		int x,y;
		button->getBounds(x,y,wide,tall);
		button->setPos(xx,y);
		xx+=wide+3;
	}
}
