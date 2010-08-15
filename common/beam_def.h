//=======================================================================
//			Copyright XashXT Group 2010 ©
//		        beam_def.h - view beam rendering
//=======================================================================
#ifndef BEAM_DEF_H
#define BEAM_DEF_H

// beam types
typedef enum 
{
	BEAM_POINTS = 0,
	BEAM_ENTPOINT,
	BEAM_ENTS,
	BEAM_HOSE,
} kBeamType_t;



// Start/End Entity is encoded as 12 bits of entity index, and 4 bits of attachment (4:12)
#define BEAMENT_ENTITY( x )		((x) & 0xFFF)
#define BEAMENT_ATTACHMENT( x )	(((x)>>12) & 0xF)

#endif//BEAM_DEF_H