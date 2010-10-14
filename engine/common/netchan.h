//=======================================================================
//			Copyright XashXT Group 2007 ©
//		     netchan.h - net channel abstraction layer
//=======================================================================
#ifndef NET_MSG_H
#define NET_MSG_H

/*
==========================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

==========================================================
*/
#include "net_buffer.h"

// 0 == regular, 1 == file stream
#define MAX_STREAMS			2    

// flow control bytes per second limits
#define MAX_RATE			20000				
#define MIN_RATE			1000

// default data rate
#define DEFAULT_RATE		(9999.0f)

// NETWORKING INFO

// This is the packet payload without any header bytes (which are attached for actual sending)
#define NET_MAX_PAYLOAD		80000

// This is the payload plus any header info (excluding UDP header)

// Packet header is:
//  4 bytes of outgoing seq
//  4 bytes of incoming seq
//  and for each stream
// {
//  byte (on/off)
//  int (fragment id)
//  short (startpos)
//  short (length)
// }
#define HEADER_BYTES		( 8 + MAX_STREAMS * 9 )

// Pad this to next higher 16 byte boundary
// This is the largest packet that can come in/out over the wire, before processing the header
//  bytes will be stripped by the networking channel layer
#define NET_MAX_MESSAGE		PAD_NUMBER(( NET_MAX_PAYLOAD + HEADER_BYTES ), 16 )

#define PORT_MASTER			27900
#define PORT_CLIENT			27901
#define PORT_SERVER			27910
#define MULTIPLAYER_BACKUP		64	// how many data slots to use when in multiplayer (must be power of 2)
#define SINGLEPLAYER_BACKUP		16	// same for single player  

/*
==============================================================

NET

==============================================================
*/
#define MAX_FLOWS			2

#define FLOW_OUTGOING		0
#define FLOW_INCOMING		1
#define MAX_LATENT			32

// size of fragmentation buffer internal buffers
#define FRAGMENT_SIZE 		1400

#define FRAG_NORMAL_STREAM		0
#define FRAG_FILE_STREAM		1

// message data
typedef struct
{
	int		size;		// size of message sent/received
	double		time;		// time that message was sent/received
} flowstats_t;

typedef struct
{
	flowstats_t	stats[MAX_LATENT];	// data for last MAX_LATENT messages
	int		current;		// current message position
	double		nextcompute; 	// time when we should recompute k/sec data
	float		kbytespersec;	// average data
	float		avgkbytespersec;
	int		totalbytes;
} flow_t;

// generic fragment structure
typedef struct fragbuf_s
{
	struct fragbuf_s	*next;		// next buffer in chain
	int		bufferid;		// id of this buffer
	sizebuf_t		frag_message;	// message buffer where raw data is stored
	byte		frag_message_buf[FRAGMENT_SIZE];	// the actual data sits here
	bool		isfile;		// is this a file buffer?
	bool		isbuffer;		// is this file buffer from memory ( custom decal, etc. ).
	char		filename[CS_SIZE];	// name of the file to save out on remote host
	int		foffset;		// offset in file from which to read data  
	int		size;		// size of data to read at that offset
} fragbuf_t;

// Waiting list of fragbuf chains
typedef struct fragbufwaiting_s
{
	struct fragbufwaiting_s	*next;	// next chain in waiting list
	int		fragbufcount;	// number of buffers in this chain
	fragbuf_t		*fragbufs;	// the actual buffers
} fragbufwaiting_t;

// Network Connection Channel
typedef struct netchan_s
{
	netsrc_t		sock;		// NS_SERVER or NS_CLIENT, depending on channel.
	netadr_t		remote_address;	// address this channel is talking to.  
	int		qport;		// qport value to write when transmitting
	
	bool		compress;		// enable huffman compression
			
	double		last_received;	// for timeouts
	double		last_sent;	// for retransmits		

	double		rate;		// bandwidth choke. bytes per second
	long		cleartime;	// if realtime > cleartime, free to send next packet

	int		drop_count;	// dropped packets, cleared each level
	int		good_count;	// cleared each level

	// Sequencing variables
	int		incoming_sequence;		// increasing count of sequence numbers               
	int		incoming_acknowledged;	// # of last outgoing message that has been ack'd.          
	int		incoming_reliable_acknowledged;	// toggles T/F as reliable messages are received.	
	int		incoming_reliable_sequence;	// single bit, maintained local	    
	int		outgoing_sequence;		// message we are sending to remote              
	int		reliable_sequence;		// whether the message contains reliable payload, single bit
	int		last_reliable_sequence; // outgoing sequence number of last send that had reliable data

	// staging and holding areas
	sizebuf_t		message;
	byte		message_buf[NET_MAX_PAYLOAD];

	// reliable message buffer.
	// we keep adding to it until reliable is acknowledged.  Then we clear it.
	int		reliable_length;
	byte		reliable_buf[NET_MAX_PAYLOAD];	// unacked reliable message

	// Waiting list of buffered fragments to go onto queue.
	// Multiple outgoing buffers can be queued in succession
	fragbufwaiting_t	*waitlist[MAX_STREAMS]; 

	int		reliable_fragment[MAX_STREAMS];	// is reliable waiting buf a fragment?          
	uint		reliable_fragid[MAX_STREAMS];		// buffer id for each waiting fragment

	fragbuf_t		*fragbufs[MAX_STREAMS];	// the current fragment being set
	int		fragbufcount[MAX_STREAMS];	// the total number of fragments in this stream

	short		frag_startpos[MAX_STREAMS];	// position in outgoing buffer where frag data starts
	short		frag_length[MAX_STREAMS];	// length of frag data in the buffer

	fragbuf_t		*incomingbufs[MAX_STREAMS];	// incoming fragments are stored here
	bool		incomingready[MAX_STREAMS];	// set to true when incoming data is ready

	// Only referenced by the FRAG_FILE_STREAM component
	char		incomingfilename[CS_SIZE];	// Name of file being downloaded

	// incoming and outgoing flow metrics
	flow_t		flow[MAX_FLOWS];  

	// added for net_speeds
	size_t		total_sended;
	size_t		total_sended_uncompressed;

	size_t		total_received;
	size_t		total_received_uncompressed;
} netchan_t;

extern netadr_t		net_from;
extern sizebuf_t		net_message;
extern byte		net_message_buffer[MAX_MSGLEN];
extern cvar_t		*net_speeds;
extern int		net_drop;

void Netchan_Init( void );
void Netchan_Shutdown( void );
void Netchan_Setup( netsrc_t sock, netchan_t *chan, netadr_t adr, int qport );
bool Netchan_CopyNormalFragments( netchan_t *chan, sizebuf_t *msg );
bool Netchan_CopyFileFragments( netchan_t *chan, sizebuf_t *msg );
void Netchan_CreateFragments( bool server, netchan_t *chan, sizebuf_t *msg );
void Netchan_Transmit( netchan_t *chan, int lengthInBytes, byte *data );
void Netchan_TransmitBits( netchan_t *chan, int lengthInBits, byte *data );
void Netchan_OutOfBand( int net_socket, netadr_t adr, int length, byte *data );
void Netchan_OutOfBandPrint( int net_socket, netadr_t adr, char *format, ... );
bool Netchan_Process( netchan_t *chan, sizebuf_t *msg );
void Netchan_UpdateProgress( netchan_t *chan );
bool Netchan_IncomingReady( netchan_t *chan );
bool Netchan_CanPacket( netchan_t *chan );
void Netchan_Clear( netchan_t *chan );

// huffman compression
void Huff_Init( void );
void Huff_CompressPacket( sizebuf_t *msg, int offset );
void Huff_DecompressPacket( sizebuf_t *msg, int offset );

#endif//NET_MSG_H