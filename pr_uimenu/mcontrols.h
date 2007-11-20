///////////////////////////////////////////////
// Controls/Item Header File
///////////////////////
// This file belongs to dpmod/darkplaces
// AK contains all item specific stuff
////////////////////////////////////////////////
// constants
/*
Items/Controls:

This control is supported/required by the menu manager :
ITEM_WINDOW
(ITEM_REFERENCE)

The rest is not required:
ITEM_CUSTIOM

ITEM_PICTURE

ITEM_TEXT

ITEM_BUTTON
ITEM_TEXTBUTTON

ITEM_RECTANGLE

ITEM_SLIDER

ITEM_TEXTSWITCH
*/

// item constants

// colors

// text colors
const vector ITEM_TEXT_NORMAL_COLOR 	= '1 1 1';
const float	 ITEM_TEXT_NORMAL_ALPHA		= 1;

const vector ITEM_TEXT_SELECTED_COLOR	= '0.5 0 0';
const float	 ITEM_TEXT_SELECTED_ALPHA	= 1;

const vector ITEM_TEXT_PRESSED_COLOR	= '1 0 0';
const float	 ITEM_TEXT_PRESSED_ALPHA	= 1;


// pitures
const vector ITEM_PICTURE_NORMAL_COLOR = '1 1 1';
const float	 ITEM_PICTURE_NORMAL_ALPHA = 1;

const vector ITEM_PICTURE_SELECTED_COLOR = '1 1 1';
const float	 ITEM_PICTURE_SELECTED_ALPHA = 1;

const vector ITEM_PICTURE_PRESSED_COLOR = '1 1 1';
const float	 ITEM_PICTURE_PRESSED_ALPHA = 1;

// slider color
const vector ITEM_SLIDER_COLOR	= '1 1 1';
const float	 ITEM_SLIDER_ALPHA	= 1;

// ITEM_SLIDER/ITEM_TEXTSLIDER
const float	 ITEM_SLIDER_STEP		= 1;

// used only in non-picture mode
const vector ITEM_SLIDER_BAR_COLOR_DELTA = '-0.8 -0.8 0';

const vector ITEM_SLIDER_SIZE 		= '10 10 0';

// ITEM_BUTTON/ITEM_TEXTBUTTON

const float		ITEM_BUTTON_HOLD_PRESSED	= 0.2;

const float		TEXTBUTTON_STYLE_BOX 		= 2;
const float		TEXTBUTTON_STYLE_OUTLINE 	= 1;
const float		TEXTBUTTON_STYLE_TEXT		= 0;

const float		TEXTBUTTON_OUTLINE_WIDTH	= 1.0;

//ITEM_TEXT
const vector	ITEM_TEXT_FONT_SIZE = '8 8 0';

const float 	TEXT_ALIGN_LEFT		= 0;
const float		TEXT_ALIGN_CENTER	= 1;
const float 	TEXT_ALIGN_RIGHT	= 2;
const float		TEXT_ALIGN_RIGHTPOS 	= 4;	// |text - actually this isnt necessary
const float		TEXT_ALIGN_CENTERPOS	= 8;	// te|xt
const float 	TEXT_ALIGN_LEFTPOS 		= 16;	// text|

// ITEM_BUTTON/ITEM_TEXTBUTTON states
const float		BUTTON_NORMAL	= 0;
const float		BUTTON_SELECTED = 1;
const float		BUTTON_PRESSED	= 2;

// flags constant

const float	FLAG_STANDARD		= 1;	// use this flag if you want the standard behavior and not what the control recommends
const float FLAG_HIDDEN 		= 2;	// events wont be called and it wont be drawn, etc.
const float FLAG_NOSELECT 		= 4; 	// cant be selected (but events will be called)
const float FLAG_CONNECTEDONLY	= 8;	// only if connected (i.e. playing)
const float FLAG_SERVERONLY		= 16;	// only displayed if server
const float FLAG_DEVELOPERONLY  = 32;	// only displayed if developer
const float _FLAG_MOUSEINAREA 	= 64; 	// used to determine wheter to call mouse_enter/_leave
const float FLAG_DRAWONLY		= 128; 	// only the draw event will be called
const float	FLAG_AUTOSETCLICK 	= 256;	// used to make click_pos and click_size always the same as pos and size
const float FLAG_AUTOSETCLIP	= 512;	// used to make clip_pos/_size always the same like pos and size
const float FLAG_CHILDDRAWONLY	= 1024;	// used to make the children only drawable
const float FLAG_DRAWREFRESHONLY= 2048; // only the draw and refresh event get called
const float FLAG_CHILDDRAWREFRESHONLY = 4096;

// control fields
// fields used by multiple items
.vector color;
.float 	alpha;
.float  drawflag;

// ITEM_REFERENCE
.string link; // window link

// ITEM_PICTURE
.string picture;
.vector pos;
.vector size;

// ITEM_TEXT
.string text;
.vector font_size;
.float	alignment;

// ITEM_BUTTON
.string picture_selected;
.string picture_pressed;
.string sound_selected;
.string sound_pressed;

.vector	color_selected;
.vector	color_pressed;
.float	alpha_selected;
.float 	alpha_pressed;
.float	drawflag_selected;
.float	drawflag_pressed;

.float	_press_time;
.float 	hold_pressed;

.float  _button_state;

// ITEM_TEXTBUTTON
//.string	text;
//.vector 	font_size;
//.float	alignment;
.float		style;

// ITEM_SLIDER
//.string 	picture_slider; = picture
.string		picture_bar;
.string		sound_changed;

.float		min_value;
.float		max_value;
.float		value;

.float		step;

.vector		slider_size;

.void(void) slidermove;

// ITEM_TEXTSWITCH (derived from ITEM_TEXT mostly)
.string	text;	// like above
.float	value;	// the current displayed/selected text

.void(void) switchchange;

// functions

/*void(void) ctinit_picture;
void(void) ctinit_button;
void(void) ctinit_textbutton;*/
