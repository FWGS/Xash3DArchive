#include "extdll.h"
#include "basemenu.h"
#include "utils.h"
#include "ui_title_anim.h"

cvar_t* cr_banner_fix_x;
cvar_t* cr_banner_fix_y;

#define BANNER_X_FIX -16
#define BANNER_Y_FIX -20

// Title Transition Time period
#define TTT_PERIOD 200.0f

quad_t TitleLerpQuads[2];
int transition_initial_time;
int transition_state;

HIMAGE TransPic=0;

int PreClickDepth;

void UI_TACheckMenuDepth()
{
	PreClickDepth=uiStatic.menuDepth;
}

menuPicButton_s *ButtonStack[UI_MAX_MENUDEPTH];
int ButtonStackDepth;

void UI_PopPButtonStack()
{
	UI_SetTitleAnim(AS_TO_BUTTON,ButtonStack[ButtonStackDepth]);
	ButtonStackDepth--;
}

void UI_PushPButtonStack(menuPicButton_s*button)
{
	if (ButtonStack[ButtonStackDepth]==button) return;
	// UI_PushMenu поматериться за нас если глубина превысила допустимое значение, надеюсь
	ButtonStackDepth++;
	ButtonStack[ButtonStackDepth]=button;
}

float UI_GetTitleTransFraction()
{
	float fraction=(float)(uiStatic.realTime-transition_initial_time)/TTT_PERIOD;

	if (fraction>1) fraction=1;

	return fraction;
}

void LerpQuad(quad_t a,quad_t b,float frac,quad_t * c)
{
	c->x=a.x+(b.x-a.x)*frac;
	c->y=a.y+(b.y-a.y)*frac;
	c->lx=a.lx+(b.lx-a.lx)*frac;
	c->ly=a.ly+(b.ly-a.ly)*frac;
}

void UI_SetupTitleQuad()
{
	TitleLerpQuads[1].x=UI_BANNER_POSX+BANNER_X_FIX;
	TitleLerpQuads[1].y=UI_BANNER_POSY+BANNER_Y_FIX;
	TitleLerpQuads[1].lx=UI_BANNER_WIDTH-125;
	TitleLerpQuads[1].ly=UI_BANNER_HEIGHT-40;
}

void UI_DrawTitleAnim()
{
	UI_SetupTitleQuad();

	if (!TransPic) return;

	wrect_t r={ 0, uiStatic.buttons_width, 26, 51 };

	float frac=UI_GetTitleTransFraction();/*(sin(gpGlobals->time*4)+1)/2*/;

	if (frac==1) return;

	quad_t c;
	
	int f_idx=(transition_state==AS_TO_TITLE) ? 0 : 1;
	int s_idx=(transition_state==AS_TO_TITLE) ? 1 : 0;

	LerpQuad(TitleLerpQuads[f_idx],TitleLerpQuads[s_idx],frac,&c);

	PIC_Set(TransPic,255,255,255,255);
	PIC_DrawAdditive(c.x,c.y,c.lx,c.ly,&r);
}

void UI_SetTitleAnim(int anim_state,menuPicButton_s* button)
{
	// skip buttons which don't call new menu
	if (PreClickDepth==uiStatic.menuDepth && anim_state==AS_TO_TITLE) return;
	// replace cancel\done button with button which called this menu 
	if (PreClickDepth>uiStatic.menuDepth && anim_state==AS_TO_TITLE) 
	{
		anim_state=AS_TO_BUTTON;
		// HACK HACK HACK
		if (ButtonStack[ButtonStackDepth+1])
			button=ButtonStack[ButtonStackDepth+1];
	}
	
	// don't reset anim if dialog buttons pressed
	if (button->generic.id==130 || button->generic.id==131) return;

	if (anim_state==AS_TO_TITLE)
		UI_PushPButtonStack(button);

	transition_state=anim_state;

	TitleLerpQuads[0].x=button->generic.x;
	TitleLerpQuads[0].y=button->generic.y;
	TitleLerpQuads[0].lx=button->generic.width;
	TitleLerpQuads[0].ly=button->generic.height;
	
	transition_initial_time=uiStatic.realTime;
		
	TransPic=button->pic;
}

void UI_InitTitleAnim()
{
//	cr_banner_fix_x=g_engfuncs.pfnRegisterVariable("cr_banner_fix_x","1",0);
//	cr_banner_fix_y=g_engfuncs.pfnRegisterVariable("cr_banner_fix_y","1",0);

	memset(TitleLerpQuads,0,sizeof(quad_t)*2);

	UI_SetupTitleQuad();

	ButtonStackDepth=0;
	memset(ButtonStack,0,sizeof(ButtonStack));
}
