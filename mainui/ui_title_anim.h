#define AS_TO_TITLE 1
#define AS_TO_BUTTON 2

void UI_SetTitleAnim(int anim_state,menuPicButton_s* picid);
void UI_DrawTitleAnim();
void UI_InitTitleAnim();
void UI_TACheckMenuDepth();
float UI_GetTitleTransFraction();

typedef struct  
{
	int x,y,lx,ly;
}quad_t;

void UI_PopPButtonStack();