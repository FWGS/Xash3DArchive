//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         net_msg.h - message io functions
//=======================================================================
#ifndef NET_MSG_H
#define NET_MSG_H

/*
==========================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

==========================================================
*/

#include "usercmd.h"
#include "pm_movevars.h"
#include "entity_state.h"
#include "net_buffer.h"

// config strings are a general means of communication from
// the server to all connected clients.
// each config string can be at most CS_SIZE characters.
#define CS_SIZE			64	// size of one config string
#define CS_TIME			16	// size of time string

// FIXME: eliminate this. Configstrings must be started from CS_MODELS

#define CS_NAME			0	// map name
#define CS_MAPCHECKSUM		1	// level checksum (for catching cheater maps)
#define CS_BACKGROUND_TRACK		3	// basename of background track

// 8 - 32 it's a reserved strings

#define CS_MODELS			8				// configstrings starts here
#define CS_SOUNDS			(CS_MODELS+MAX_MODELS)		// sound names
#define CS_DECALS			(CS_SOUNDS+MAX_SOUNDS)		// server decal indexes
#define CS_EVENTS			(CS_DECALS+MAX_DECALNAMES)		// queue events
#define CS_GENERICS			(CS_EVENTS+MAX_EVENTS)		// generic resources (e.g. color decals)
#define CS_LIGHTSTYLES		(CS_GENERICS+MAX_GENERICS)		// lightstyle patterns 
#define MAX_CONFIGSTRINGS		(CS_LIGHTSTYLES+MAX_LIGHTSTYLES)	// total count

// huffman compression
void Huff_Init( void );
void Huff_CompressPacket( sizebuf_t *msg, int offset );
void Huff_DecompressPacket( sizebuf_t *msg, int offset );

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
	sizebuf_t			message;				// writing buffer to send to server
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
extern sizebuf_t		net_message;
extern byte		net_message_buffer[MAX_MSGLEN];
extern cvar_t		*net_speeds;

#define PORT_MASTER		27900
#define PORT_CLIENT		27901
#define PORT_SERVER		27910
#define MULTIPLAYER_BACKUP	64	// how many data slots to use when in multiplayer (must be power of 2)
#define SINGLEPLAYER_BACKUP	16	// same for single player   

void Netchan_Init( void );
void Netchan_Setup( netsrc_t sock, netchan_t *chan, netadr_t adr, int qport );
bool Netchan_NeedReliable( netchan_t *chan );
void Netchan_Transmit( netchan_t *chan, int lengthInBytes, byte *data );
void Netchan_TransmitBits( netchan_t *chan, int lengthInBits, byte *data );
void Netchan_OutOfBand( int net_socket, netadr_t adr, int length, byte *data );
void Netchan_OutOfBandPrint( int net_socket, netadr_t adr, char *format, ... );
bool Netchan_Process( netchan_t *chan, sizebuf_t *msg );

#endif//NET_MSG_H