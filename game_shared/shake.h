//=======================================================================
//			Copyright XashXT Group 2009 ©
//		    shake.h - screenshake and screenfade events
//=======================================================================
#ifndef SHAKE_H
#define SHAKE_H

// Screen / View effects

// screen shake
extern int gmsgShake;

typedef struct
{
	unsigned short	amplitude;	// FIXED 4.12 amount of shake
	unsigned short 	duration;		// FIXED 4.12 seconds duration
	unsigned short	frequency;	// FIXED 8.8 low frequency is a jerk, high frequency is a rumble
} ScreenShake;

// Fade in/out
extern int gmsgFade;

#define FFADE_IN		0x0000	// Fade in (not out)
#define FFADE_OUT		0x0001	// Fade out (not in)
#define FFADE_MODULATE	0x0002	// Modulate (don't blend)
#define FFADE_STAYOUT	0x0004	// ignores the duration, stays faded out until new ScreenFade message received

// This structure is sent over the net to describe a screen fade event
typedef struct
{
	unsigned short	duration;		// FIXED 4.12 seconds duration
	unsigned short	holdTime;		// FIXED 4.12 seconds duration until reset (fade & hold)
	short		fadeFlags;	// flags
	byte		r, g, b, a;	// fade to color ( max alpha )
} ScreenFade;

#endif // SHAKE_H