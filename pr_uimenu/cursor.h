///////////////////////////////////////////////
// Cursor Header File
///////////////////////
// This file belongs to dpmod/darkplaces
// AK contains all cursor specific constants as the pulse initialization list
////////////////////////////////

// cursor constants (cursor type)
const float CT_NORMAL 		= 0;
const float CT_PULSE		= 1;
const float CT_FIRST_PULSE	= 1; // pulse frames 0 - 6
const float CT_LAST_PULSE	= 7;
const float CT_GLOW			= 8;

const float	CURSOR_SCALE	= 1;

const vector CURSOR_COLOR	= '1 1 1';
const float	 CURSOR_TRANSPARENCY = 1;

// cursor speed
const float CURSOR_SPEED = 0.75;

// cursor animation
const float CA_PULSE_SPEED = 0.14257142 ; // = 1 / 7 -> 1 secs total time

// cursor filenames
var string CF_NORMAL = "ui/mousepointer.tga";

// enforce loading everything else qc will break
const float CURSOR_ENFORCELOADING = false;

// pulse animation
string CF_PULSE[7];

// global ui vars
vector cursor;
vector cursor_rel;
vector cursor_color;
float  cursor_transparency;
float  cursor_type;
float  cursor_last_frame_time;

vector cursor_speed;

// function prototypes

void(void) cursor_init;
void(void) cursor_reset;
void(void) cursor_toggle;
void(void) cursor_frame;
void(void) cursor_draw;
void(void) cursor_shutdown;
