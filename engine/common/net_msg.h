//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         net_msg.h - message io functions
//=======================================================================
#ifndef NET_MSG_H
#define NET_MSG_H

enum net_types_e
{
	NET_BAD = 0,
	NET_CHAR = 8,
	NET_BYTE = 8,
	NET_SHORT = 16,
	NET_WORD = 16,
	NET_LONG = 32,
	NET_FLOAT = 32,
	NET_ANGLE8 = 8,	// angle 2 char
	NET_ANGLE = 16,	// angle 2 short
	NET_SCALE = 8,
	NET_COORD = 32,
	NET_COLOR = 8,
	NET_TYPES,
};

typedef union
{
	float	f;
	long	l;
} ftol_t;

typedef struct
{
	bool	overflowed;	// set to true if the buffer size failed
	bool	error;

	byte	*data;
	int	maxsize;
	int	cursize;		// size in bytes
	int	readcount;
} sizebuf_t;

typedef struct net_desc_s
{
	int	type;	// pixelformat
	char	name[8];	// used for debug
	int	min_range;
	int	max_range;
} net_desc_t;

// communication state description
typedef struct net_field_s
{
	char	*name;
	int	offset;
	int	bits;
	bool	force;			// will be send for newentity
} net_field_t;

static const net_desc_t NWDesc[] =
{
{ NET_BAD,	"none",	0,		0	}, // full range
{ NET_CHAR,	"Char",	-128,		127	},
{ NET_BYTE,	"Byte",	0,		255	},
{ NET_SHORT,	"Short",	-32767,		32767	},
{ NET_WORD,	"Word",	0,		65535	},
{ NET_LONG,	"Long",	0,		0	}, // can't overflow
{ NET_FLOAT,	"Float",	0,		0	}, // can't overflow
{ NET_ANGLE8,	"Angle",	-360,		360	},
{ NET_ANGLE,	"Angle",	-360,		360	},
{ NET_SCALE,	"Scale",	-128,		127	},
{ NET_COORD,	"Coord",	-262140,		262140	},
{ NET_COLOR,	"Color",	0,		255	},
};

/*
==========================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

==========================================================
*/

#include "usercmd.h"
#include "event_api.h"
#include "pm_movevars.h"
#include "entity_state.h"
#include "net_buffer.h"

#define ES_FIELD( x )		#x,(int)&((entity_state_t*)0)->x
#define EV_FIELD( x )		#x,(int)&((event_args_t*)0)->x
#define PM_FIELD( x )		#x,(int)&((movevars_t*)0)->x
#define CM_FIELD( x )		#x,(int)&((usercmd_t*)0)->x

// config strings are a general means of communication from
// the server to all connected clients.
// each config string can be at most CS_SIZE characters.
#define CS_SIZE			64	// size of one config string
#define CS_TIME			16	// size of time string
#define CS_NAME			0	// map name
#define CS_MAPCHECKSUM		1	// level checksum (for catching cheater maps)
#define CS_SKYNAME			2	// skybox shader name
#define CS_BACKGROUND_TRACK		3	// basename of background track
#define CS_SERVERFLAGS		4	// shared server flags
#define CS_SKYCOLOR			5	// <float> <float> <float>
#define CS_SKYVEC			6	// <float> <float> <float>
#define CS_ZFAR			7	// zfar value came from server
#define CS_WATERAMP			8	// water amplitude for world water surfaces

// 8 - 32 it's a reserved strings

#define CS_MODELS			32				// configstrings starts here
#define CS_SOUNDS			(CS_MODELS+MAX_MODELS)		// sound names
#define CS_DECALS			(CS_SOUNDS+MAX_SOUNDS)		// server decal indexes
#define CS_EVENTS			(CS_DECALS+MAX_DECALNAMES)		// queue events
#define CS_GENERICS			(CS_EVENTS+MAX_EVENTS)		// edicts classnames
#define CS_CLASSNAMES		(CS_GENERICS+MAX_GENERICS)		// generic resources (e.g. color decals)
#define CS_LIGHTSTYLES		(CS_CLASSNAMES+MAX_CLASSNAMES)	// lightstyle patterns
#define MAX_CONFIGSTRINGS		(CS_LIGHTSTYLES+MAX_LIGHTSTYLES)	// total count

/*
==============================================================================

			MESSAGE IO FUNCTIONS
	       Handles byte ordering and avoids alignment errors
==============================================================================
*/
void MSG_Init( sizebuf_t *buf, byte *data, size_t length );
void MSG_Clear( sizebuf_t *buf );
void MSG_Print( sizebuf_t *msg, const char *data );
void _MSG_WriteBits( sizebuf_t *msg, long value, const char *name, int bits, const char *filename, const int fileline );
long _MSG_ReadBits( sizebuf_t *msg, const char *name, int bits, const char *filename, const int fileline );
void _MSG_Begin( int dest, const char *filename, int fileline );
void _MSG_WriteString( sizebuf_t *sb, const char *s, const char *filename, int fileline );
void _MSG_WriteStringLine( sizebuf_t *sb, const char *src, const char *filename, int fileline );
void _MSG_WriteData( sizebuf_t *sb, const void *data, size_t length, const char *filename, int fileline );
bool _MSG_WriteDeltaMovevars( sizebuf_t *sb, movevars_t *from, movevars_t *cmd, const char *filename, const int fileline );
bool _MSG_Send( int dest, const vec3_t origin, const edict_t *ent, bool direct, const char *filename, int fileline );

#define MSG_Begin( x ) _MSG_Begin( x, __FILE__, __LINE__)
#define MSG_WriteChar(x,y) _MSG_WriteBits (x, y, NWDesc[NET_CHAR].name, NET_CHAR, __FILE__, __LINE__)
#define MSG_WriteByte(x,y) _MSG_WriteBits (x, y, NWDesc[NET_BYTE].name, NET_BYTE, __FILE__, __LINE__)
#define MSG_WriteShort(x,y) _MSG_WriteBits(x, y, NWDesc[NET_SHORT].name, NET_SHORT,__FILE__, __LINE__)
#define MSG_WriteWord(x,y) _MSG_WriteBits (x, y, NWDesc[NET_WORD].name, NET_WORD, __FILE__, __LINE__)
#define MSG_WriteLong(x,y) _MSG_WriteBits (x, y, NWDesc[NET_LONG].name, NET_LONG, __FILE__, __LINE__)
#define MSG_WriteFloat(x,y) _MSG_WriteFloat(x, y, __FILE__, __LINE__)
#define MSG_WriteDouble(x,y) _MSG_WriteDouble(x, y, __FILE__, __LINE__)
#define MSG_WriteString(x,y) _MSG_WriteString (x, y, __FILE__, __LINE__)
#define MSG_WriteStringLine(x,y) _MSG_WriteStringLine (x, y, __FILE__, __LINE__)
#define MSG_WriteData(x,y,z) _MSG_WriteData( x, y, z, __FILE__, __LINE__)
#define MSG_WriteDeltaMovevars(x, y, z) _MSG_WriteDeltaMovevars(x, y, z, __FILE__, __LINE__)
#define MSG_WriteBits( buf, value, name, bits ) _MSG_WriteBits( buf, value, name, bits, __FILE__, __LINE__ )
#define MSG_ReadBits( buf, name, bits ) _MSG_ReadBits( buf, name, bits, __FILE__, __LINE__ )
#define MSG_Send(x, y, z) _MSG_Send(x, y, z, false, __FILE__, __LINE__)
#define MSG_DirectSend(x, y, z) _MSG_Send(x, y, z, true, __FILE__, __LINE__)

void MSG_BeginReading (sizebuf_t *sb);
#define MSG_ReadChar( x ) _MSG_ReadBits( x, NWDesc[NET_CHAR].name, NET_CHAR, __FILE__, __LINE__ )
#define MSG_ReadByte( x ) _MSG_ReadBits( x, NWDesc[NET_BYTE].name, NET_BYTE, __FILE__, __LINE__ )
#define MSG_ReadShort( x) _MSG_ReadBits( x, NWDesc[NET_SHORT].name, NET_SHORT, __FILE__, __LINE__ )
#define MSG_ReadWord( x ) _MSG_ReadBits( x, NWDesc[NET_WORD].name, NET_WORD, __FILE__, __LINE__ )
#define MSG_ReadLong( x ) _MSG_ReadBits( x, NWDesc[NET_LONG].name, NET_LONG, __FILE__, __LINE__ )
char *MSG_ReadString( sizebuf_t *sb );
char *MSG_ReadStringLine( sizebuf_t *sb );
void MSG_ReadData( sizebuf_t *sb, void *buffer, size_t size );
void MSG_ReadDeltaMovevars( sizebuf_t *sb, movevars_t *from, movevars_t *cmd );

// huffman compression
void Huff_Init( void );
void Huff_CompressPacket( bitbuf_t *msg, int offset );
void Huff_DecompressPacket( bitbuf_t *msg, int offset );

/*
==============================================================

NET

==============================================================
*/

typedef struct netchan_s
{
	bool			fatal_error;
	netsrc_t			sock;

	int			dropped;			// between last packet and previous
	bool			compress;			// enable huffman compression

	long			last_received;		// for timeouts
	long			last_sent;		// for retransmits

	int			drop_count;		// dropped packets, cleared each level
	int			good_count;		// cleared each level

	netadr_t			remote_address;
	int			qport;			// qport value to write when transmitting

	// sequencing variables
	int			incoming_sequence;
	int			incoming_acknowledged;
	int			incoming_reliable_acknowledged;	// single bit

	int			incoming_reliable_sequence;		// single bit, maintained local

	int			outgoing_sequence;
	int			reliable_sequence;			// single bit
	int			last_reliable_sequence;		// sequence number of last send

	// reliable staging and holding areas
	bitbuf_t			message;				// writing buffer to send to server
	byte			message_buf[MAX_MSGLEN-16];		// leave space for header

	// message is copied to this buffer when it is first transfered
	int			reliable_length;
	byte			reliable_buf[MAX_MSGLEN-16];		// unacked reliable message

	// added for net_speeds
	size_t			total_sended;
	size_t			total_sended_uncompressed;

	size_t			total_received;
	size_t			total_received_uncompressed;
} netchan_t;

extern netadr_t		net_from;
extern bitbuf_t		net_message;
extern byte		net_message_buffer[MAX_MSGLEN];
extern cvar_t		*net_speeds;

#define PORT_MASTER		27900
#define PORT_CLIENT		27901
#define PORT_SERVER		27910
#define MULTIPLAYER_BACKUP	64	// how many data slots to use when in multiplayer (must be power of 2)
#define SINGLEPLAYER_BACKUP	16	// same for single player   
#define MAX_FLAGS		32	// 32 bits == 32 flags
#define MASK_FLAGS		(MAX_FLAGS - 1)

void Netchan_Init( void );
void Netchan_Setup( netsrc_t sock, netchan_t *chan, netadr_t adr, int qport );
bool Netchan_NeedReliable( netchan_t *chan );
void Netchan_Transmit( netchan_t *chan, int lengthInBytes, byte *data );
void Netchan_TransmitBits( netchan_t *chan, int lengthInBits, byte *data );
void Netchan_OutOfBand( int net_socket, netadr_t adr, int length, byte *data );
void Netchan_OutOfBandPrint( int net_socket, netadr_t adr, char *format, ... );
bool Netchan_Process( netchan_t *chan, bitbuf_t *msg );

#endif//NET_MSG_H