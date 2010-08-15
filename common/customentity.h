//=======================================================================
//			Copyright XashXT Group 2010 ©
//		      customentity.h - beam encode specific
//=======================================================================
#ifndef CUSTOMENTITY_H
#define CUSTOMENTITY_H

// beam types, encoded as a byte
enum 
{
	BEAM_POINTS = 0,
	BEAM_ENTPOINT,
	BEAM_ENTS,
	BEAM_HOSE,
};

#define BEAM_FSINE		0x10
#define BEAM_FSOLID		0x20
#define BEAM_FSHADEIN	0x40
#define BEAM_FSHADEOUT	0x80

// Start/End Entity is encoded as 12 bits of entity index, and 4 bits of attachment (4:12)
#define BEAMENT_ENTITY( x )		((x) & 0xFFF)
#define BEAMENT_ATTACHMENT( x )	(((x)>>12) & 0xF)

#endif//CUSTOMENTITY_H