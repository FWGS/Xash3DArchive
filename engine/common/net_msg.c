//=======================================================================
//			Copyright XashXT Group 2007 �
//			net_msg.c - network messages
//=======================================================================

#include "common.h"
#include "mathlib.h"
#include "builtin.h"

// if (int)f == f and (int)f + (1<<(FLOAT_INT_BITS-1) ) < (1<<FLOAT_INT_BITS)
// the float will be sent with FLOAT_INT_BITS, otherwise all 32 bits will be sent
#define FLOAT_INT_BITS	13
#define FLOAT_INT_BIAS	(1<<(FLOAT_INT_BITS-1))
#define HMAX		256			// maximum symbol
#define NYT		HMAX			// NYT = Not Yet Transmitted
#define INTERNAL_NODE	(HMAX + 1)

typedef struct node_s
{
	struct node_s	*left;	// tree structure
	struct node_s	*right;
	struct node_s	*parent;
	struct node_s	*next;	// doubly-linked list
	struct node_s	*prev;
	struct node_s	**head;	// highest ranked node in block
	int		weight;
	int		symbol;
} node_t;

typedef struct huff_tree_s
{
	int		blocNode;
	int		blocPtrs;

	node_t		*tree;
	node_t		*lhead;
	node_t		*ltail;
	node_t		*loc[HMAX+1];
	node_t		**freelist;

	node_t		nodeList[768];
	node_t		*nodePtrs[768];
} huff_tree_t;

typedef struct
{
	huff_tree_t	compressor;
	huff_tree_t	decompressor;
} huffman_t;

static bool huffman_init = false;
static huffman_t msg_huff;
static int bloc = 0;

/*
==============================================================================

			HUFFMAN COMPRESSION TREE
		   based on original code from QIII Arena
==============================================================================
*/
void Huff_putBit( int bit, byte *fout, int *offset )
{
	bloc = *offset;
	if((bloc&7) == 0 ) fout[(bloc>>3)] = 0;
	fout[(bloc>>3)] |= bit << (bloc&7);
	bloc++;
	*offset = bloc;
}

int Huff_getBit( byte *fin, int *offset )
{
	int	t;

	bloc = *offset;
	t = (fin[(bloc>>3)] >> (bloc&7)) & 0x1;
	bloc++;
	*offset = bloc;
	return t;
}

// add a bit to the output file (buffered)
static void add_bit( char bit, byte *fout )
{
	if((bloc & 7) == 0) fout[(bloc>>3)] = 0;
	fout[(bloc>>3)] |= bit << (bloc&7);
	bloc++;
}

// receive one bit from the input file (buffered)
static int get_bit( byte *fin )
{
	int	t;

	t = (fin[(bloc>>3)] >> (bloc&7)) & 0x1;
	bloc++;
	return t;
}

static node_t **get_ppnode( huff_tree_t* huff )
{
	node_t	**tppnode;

	if( !huff->freelist )
	{
		return &(huff->nodePtrs[huff->blocPtrs++]);
	}
	else
	{
		tppnode = huff->freelist;
		huff->freelist = (node_t **)*tppnode;
		return tppnode;
	}
}

static void free_ppnode( huff_tree_t* huff, node_t **ppnode )
{
	*ppnode = (node_t *)huff->freelist;
	huff->freelist = ppnode;
}

// swap the location of these two nodes in the tree
static void swap( huff_tree_t* huff, node_t *node1, node_t *node2 )
{ 
	node_t	*par1, *par2;

	par1 = node1->parent;
	par2 = node2->parent;

	if( par1 )
	{
		if( par1->left == node1 )
			par1->left = node2;
		else par1->right = node2;
	}
	else huff->tree = node2;

	if( par2 )
	{
		if( par2->left == node2 )
			par2->left = node1;
		else par2->right = node1;
	}
	else huff->tree = node1;
  
	node1->parent = par2;
	node2->parent = par1;
}

// swap these two nodes in the linked list ( update ranks )
static void swaplist( node_t *node1, node_t *node2 )
{
	node_t	*par1;

	par1 = node1->next;
	node1->next = node2->next;
	node2->next = par1;

	par1 = node1->prev;
	node1->prev = node2->prev;
	node2->prev = par1;

	if( node1->next == node1 )
		node1->next = node2;
	if( node2->next == node2 )
		node2->next = node1;
	if( node1->next )
		node1->next->prev = node1;
	if( node2->next )
		node2->next->prev = node2;
	if( node1->prev )
		node1->prev->next = node1;
	if( node2->prev )
		node2->prev->next = node2;
}

// do the increments
static void increment( huff_tree_t* huff, node_t *node )
{
	node_t	*lnode;

	if( !node ) return;

	if( node->next != NULL && node->next->weight == node->weight )
	{
		lnode = *node->head;
		if( lnode != node->parent )
			swap(huff, lnode, node);
		swaplist( lnode, node );
	}
	if( node->prev && node->prev->weight == node->weight )
	{
		*node->head = node->prev;
	}
	else
	{
		*node->head = NULL;
		free_ppnode( huff, node->head );
	}

	node->weight++;

	if( node->next && node->next->weight == node->weight )
	{
		node->head = node->next->head;
	}
	else
	{ 
		node->head = get_ppnode( huff );
		*node->head = node;
	}
	if( node->parent )
	{
		increment( huff, node->parent );
		if( node->prev == node->parent )
		{
			swaplist( node, node->parent );
			if( *node->head == node )
				*node->head = node->parent;
		}
	}
}

void Huff_addRef( huff_tree_t* huff, byte ch )
{
	node_t	*tnode, *tnode2;
	if( huff->loc[ch] == NULL )
	{
		// if this is the first transmission of this node
		tnode = &(huff->nodeList[huff->blocNode++]);
		tnode2 = &(huff->nodeList[huff->blocNode++]);

		tnode2->symbol = INTERNAL_NODE;
		tnode2->weight = 1;
		tnode2->next = huff->lhead->next;
		if( huff->lhead->next )
		{
			huff->lhead->next->prev = tnode2;
			if( huff->lhead->next->weight == 1 )
			{
				tnode2->head = huff->lhead->next->head;
			}
			else
			{
				tnode2->head = get_ppnode( huff );
				*tnode2->head = tnode2;
			}
		}
		else
		{
			tnode2->head = get_ppnode( huff );
			*tnode2->head = tnode2;
		}
		huff->lhead->next = tnode2;
		tnode2->prev = huff->lhead;
 
		tnode->symbol = ch;
		tnode->weight = 1;
		tnode->next = huff->lhead->next;

		if( huff->lhead->next )
		{
			huff->lhead->next->prev = tnode;
			if( huff->lhead->next->weight == 1 )
			{
				tnode->head = huff->lhead->next->head;
			}
			else
			{
				// this should never happen
				tnode->head = get_ppnode( huff );
				*tnode->head = tnode2;
			}
		}
		else
		{
			// this should never happen
			tnode->head = get_ppnode( huff );
			*tnode->head = tnode;
		}

		huff->lhead->next = tnode;
		tnode->prev = huff->lhead;
		tnode->left = tnode->right = NULL;
 
		if( huff->lhead->parent )
		{
			// lhead is guaranteed to by the NYT
			if( huff->lhead->parent->left == huff->lhead )
				huff->lhead->parent->left = tnode2;
			else huff->lhead->parent->right = tnode2;
		}
		else huff->tree = tnode2; 
 
		tnode2->right = tnode;
		tnode2->left = huff->lhead;
 
		tnode2->parent = huff->lhead->parent;
		huff->lhead->parent = tnode->parent = tnode2;
     
		huff->loc[ch] = tnode;
 
		increment( huff, tnode2->parent );
	}
	else increment( huff, huff->loc[ch] );
}

// get a symbol
int Huff_Receive( node_t *node, int *ch, byte *fin )
{
	while( node && node->symbol == INTERNAL_NODE )
	{
		if( get_bit(fin)) node = node->right;
		else node = node->left;
	}

	if( !node ) return 0;
	return (*ch = node->symbol);
}

// get a symbol from offest
void Huff_offsetReceive( node_t *node, int *ch, byte *fin, int *offset )
{
	bloc = *offset;
	while( node && node->symbol == INTERNAL_NODE )
	{
		if( get_bit(fin)) node = node->right;
		else node = node->left;
	}
	if( !node )
	{
		*ch = 0;
		return;
	}

	*ch = node->symbol;
	*offset = bloc;
}

// send the prefix code for this node
static void sendprefix( node_t *node, node_t *child, byte *fout )
{
	if( node->parent ) sendprefix(node->parent, node, fout);
	if( child )
	{
		if( node->right == child )
			add_bit( 1, fout );
		else add_bit( 0, fout );
	}
}

// send a symbol
void Huff_transmit( huff_tree_t *huff, int ch, byte *fout )
{
	int	i;

	if( huff->loc[ch] == NULL )
	{ 
		// node_t hasn't been transmitted, send a NYT, then the symbol
		Huff_transmit( huff, NYT, fout );
		for( i = 7; i >= 0; i-- )
			add_bit((char)((ch >> i) & 0x1), fout);
	}
	else sendprefix( huff->loc[ch], NULL, fout );
}

void Huff_offsetTransmit( huff_tree_t *huff, int ch, byte *fout, int *offset )
{
	bloc = *offset;
	sendprefix( huff->loc[ch], NULL, fout );
	*offset = bloc;
}

void Huff_Decompress( sizebuf_t *buf, int offset )
{
	byte		seq[65536];
	int		i, j, size;
	int		ch, cch;
	byte*		buffer;
	huff_tree_t	huff;

	size = buf->cursize - offset;
	buffer = buf->data + offset;

	if( size <= 0 ) return;

	memset( &huff, 0, sizeof(huff_tree_t));

	// initialize the tree & list with the NYT node 
	huff.tree = huff.lhead = huff.ltail = huff.loc[NYT] = &(huff.nodeList[huff.blocNode++]);
	huff.tree->symbol = NYT;
	huff.tree->weight = 0;
	huff.lhead->next = huff.lhead->prev = NULL;
	huff.tree->parent = huff.tree->left = huff.tree->right = NULL;

	cch = buffer[0] * 256 + buffer[1];

	// don't overflow with bad messages
	if( cch > buf->maxsize - offset )
		cch = buf->maxsize - offset;
	bloc = 16;

	for( j = 0; j < cch; j++ )
	{
		ch = 0;
		// don't overflow reading from the messages
		if((bloc >> 3) > size )
		{
			seq[j] = 0;
			break;
		}

		Huff_Receive( huff.tree, &ch, buffer );	// get a character

		if( ch == NYT )
		{	
			// we got a NYT, get the symbol associated with it
			ch = 0;
			for( i = 0; i < 8; i++ )
				ch = (ch<<1) + get_bit( buffer );
		}

		seq[j] = ch;			// write symbol
		Huff_addRef( &huff, (byte)ch );	// increment node
	}

	buf->cursize = cch + offset;
	Mem_Copy( buf->data + offset, seq, cch );
}

void Huff_Compress( sizebuf_t *buf, int offset )
{
	int		i, ch, size;
	byte		seq[65536];
	byte*		buffer;
	huff_tree_t	huff;

	size = buf->cursize - offset;
	buffer = buf->data+ + offset;

	if( size <= 0 ) return;

	memset( &huff, 0, sizeof(huff_tree_t));
	// add the NYT ( not yet transmitted ) node into the tree/list
	huff.tree = huff.lhead = huff.loc[NYT] =  &(huff.nodeList[huff.blocNode++]);
	huff.tree->symbol = NYT;
	huff.tree->weight = 0;
	huff.lhead->next = huff.lhead->prev = NULL;
	huff.tree->parent = huff.tree->left = huff.tree->right = NULL;
	huff.loc[NYT] = huff.tree;

	seq[0] = (size>>8);
	seq[1] = size&0xff;

	bloc = 16;

	for( i = 0; i < size; i++ )
	{
		ch = buffer[i];
		Huff_transmit( &huff, ch, seq );	// transmit symbol
		Huff_addRef( &huff, (byte)ch );	// do update
	}
	bloc += 8;				// next byte

	buf->cursize = (bloc>>3) + offset;
	Mem_Copy( buf->data+offset, seq, (bloc>>3));
}

void Huff_Init( huffman_t *huff )
{
	memset( &huff->compressor, 0, sizeof(huff_tree_t));
	memset( &huff->decompressor, 0, sizeof(huff_tree_t));

	// initialize the tree & list with the NYT node 
	huff->decompressor.loc[NYT] = &(huff->decompressor.nodeList[huff->decompressor.blocNode++]);
	huff->decompressor.tree = huff->decompressor.lhead = huff->decompressor.ltail = huff->decompressor.loc[NYT];
	huff->decompressor.tree->symbol = NYT;
	huff->decompressor.tree->weight = 0;
	huff->decompressor.lhead->next = huff->decompressor.lhead->prev = NULL;
	huff->decompressor.tree->parent = huff->decompressor.tree->left = huff->decompressor.tree->right = NULL;

	// add the NYT (not yet transmitted) node into the tree/list
	huff->compressor.tree = huff->compressor.lhead = huff->compressor.loc[NYT] =  &(huff->compressor.nodeList[huff->compressor.blocNode++]);
	huff->compressor.tree->symbol = NYT;
	huff->compressor.tree->weight = 0;
	huff->compressor.lhead->next = huff->compressor.lhead->prev = NULL;
	huff->compressor.tree->parent = huff->compressor.tree->left = huff->compressor.tree->right = NULL;
	huff->compressor.loc[NYT] = huff->compressor.tree;
}


/*
=======================
   SZ BUFFER
=======================
*/
void MSG_InitHuffman( void )
{
	int	i, j;

	Huff_Init( &msg_huff );
	for( i = 0; i < 256; i++ )
	{
		for( j = 0; j < huff_tree[i]; j++ )
		{
			Huff_addRef( &msg_huff.compressor,  (byte)i );	// do update
			Huff_addRef( &msg_huff.decompressor,(byte)i );	// do update
		}
	}
	huffman_init = true;
}

void MSG_Init( sizebuf_t *buf, byte *data, size_t length )
{
	if( !huffman_init ) MSG_InitHuffman();
	memset( buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
	buf->huffman = true;
}

void MSG_UseHuffman( sizebuf_t *buf, bool state )
{
	buf->huffman = state;
}


void MSG_Clear( sizebuf_t *buf )
{
	buf->bit = 0;
	buf->cursize = 0;
	buf->overflowed = false;
}

/*
=============================================================================

bit functions
  
=============================================================================
*/
// debug
int oldsize = 0;
int overflows;

// negative bit values include signs
void _MSG_WriteBits( sizebuf_t *msg, int value, int bits, const char *filename, int fileline )
{
	int	i;

	oldsize += bits;

	// this isn't an exact overflow check, but close enough
	if( msg->maxsize - msg->cursize < 4 )
	{
		msg->overflowed = true;
		return;
	}

	if( bits == 0 || bits < -31 || bits > 32 )
	{
		Host_Error( "MSG_WriteBits: bad bits %i (called at %s:%i)\n", bits, filename, fileline );
	}

	// check for overflows
	if( bits != 32 )
	{
		if( bits > 0 )
		{
			if( value > ((1<<bits) - 1) || value < 0 )
				overflows++;   
		}
		else
		{
			int	r;

			r = 1<<(bits-1);
			if( value >  r - 1 || value < -r )
				overflows++;
		}
	}

	if( bits < 0 ) bits = -bits;

	if( !msg->huffman )
	{
		word	*sp;
		uint	*ip;		

		switch( bits )
		{
		case 8:
			msg->data[msg->cursize] = value;
			msg->cursize += 1;
			msg->bit += 8;
			break;
		case 16:
			sp = (word *)&msg->data[msg->cursize];
			*sp = LittleShort( value );
			msg->cursize += 2;
			msg->bit += 16;
			break;
		case 32:
			ip = (uint *)&msg->data[msg->cursize];
			*ip = LittleLong( value );
			msg->cursize += 4;
			msg->bit += 32;
			break;
		default:
			Host_Error( "MSG_WriteBits: can't write %d bits ( called %s:%i)\n", bits, filename, fileline );
			break;
		}
	}
	else
	{
		value &= (0xffffffff>>(32-bits));
		if( bits & 7 )
		{
			int nbits = bits&7;
			for( i = 0; i < nbits; i++ )
			{
				Huff_putBit( (value&1), msg->data, &msg->bit );
				value = (value>>1);
			}
			bits = bits - nbits;
		}
		if( bits )
		{
			for( i = 0; i < bits; i += 8 )
			{
				Huff_offsetTransmit( &msg_huff.compressor, (value&0xff), msg->data, &msg->bit );
				value = (value>>8);
			}
		}
		msg->cursize = (msg->bit>>3) + 1;
	}
}

int _MSG_ReadBits( sizebuf_t *msg, int bits, const char *filename, int fileline )
{
	int	get, value = 0;
	int	i, nbits = 0;
	bool	sgn;

	if( bits < 0 )
	{
		bits = -bits;
		sgn = true;
	}
	else sgn = false;

	if( !msg->huffman )
	{
		word	*sp;
		uint	*ip;
		
		switch( bits )
		{
		case 8:
			value = msg->data[msg->readcount];
			msg->readcount += 1;
			msg->bit += 8;
			break;
		case 16:
			sp = (word *)&msg->data[msg->readcount];
			value = LittleShort( *sp );
			msg->readcount += 2;
			msg->bit += 16;
			break;
		case 32:
			ip = (uint *)&msg->data[msg->readcount];
			value = LittleLong(*ip);
			msg->readcount += 4;
			msg->bit += 32;
			break;
		default:
			Host_Error( "MSG_ReadBits: can't read %d bits (called at %s:%i)\n", bits, filename, fileline );
			break;
		}
	}
	else
	{
		if( bits & 7 )
		{
			nbits = bits & 7;
			for( i = 0; i < nbits; i++ )
			{
				value |= (Huff_getBit( msg->data, &msg->bit )<<i );
			}
			bits = bits - nbits;
		}
		if( bits )
		{
			for( i = 0; i < bits; i += 8 )
			{
				Huff_offsetReceive( msg_huff.decompressor.tree, &get, msg->data, &msg->bit );
				value |= (get<<(i + nbits));
			}
		}
		msg->readcount = (msg->bit>>3) + 1;
	}
	if( sgn )
	{
		if( value & (1<<(bits-1)))
			value |= -1 ^ ((1<<bits)-1);
	}
	return value;
}

/*
==============================================================================

			MESSAGE IO FUNCTIONS
	       Handles byte ordering and avoids alignment errors
==============================================================================
*/
/*
=======================
   writing functions
=======================
*/
void _MSG_WriteChar (sizebuf_t *sb, int c, const char *filename, int fileline)
{
	if( c < -128 || c > 127 ) 
		MsgDev( D_ERROR, "MSG_WriteChar: range error %d (called at %s:%i)\n", c, filename, fileline);

	_MSG_WriteBits( sb, c, 8, filename, fileline );
}

void _MSG_WriteByte (sizebuf_t *sb, int c, const char *filename, int fileline )
{
	if( c < 0 || c > 255 )
		MsgDev( D_ERROR, "MSG_WriteByte: range error %d (called at %s:%i)\n", c, filename, fileline);

	_MSG_WriteBits( sb, c, 8, filename, fileline );
}

void _MSG_WriteData( sizebuf_t *buf, const void *data, size_t length, const char *filename, int fileline )
{
	int	i;

	for( i = 0; i < length; i++ )
		_MSG_WriteByte( buf, ((byte *)data)[i], filename, fileline );
}

void _MSG_WriteShort( sizebuf_t *sb, int c, const char *filename, int fileline )
{
	if( c < -32767 || c > 32767 )
		MsgDev( D_ERROR, "MSG_WriteShort: range error %d (called at %s:%i)\n", c, filename, fileline);

	_MSG_WriteBits( sb, c, 16, filename, fileline );
}

void _MSG_WriteWord( sizebuf_t *sb, int c, const char *filename, int fileline )
{
	if( c < 0 || c > 65535 )
		MsgDev( D_ERROR, "MSG_WriteWord: range error %d (called at %s:%i)\n", c, filename, fileline);

	_MSG_WriteBits( sb, c, 16, filename, fileline );
}

void _MSG_WriteLong( sizebuf_t *sb, int c, const char *filename, int fileline )
{
	MSG_WriteBits( sb, c, 32 );
}

void _MSG_WriteFloat( sizebuf_t *sb, float f, const char *filename, int fileline )
{
	union
	{
		float	f;
		int	l;
	} dat;
	
	dat.f = f;
	MSG_WriteBits( sb, dat.l, 32 );
}

void _MSG_WriteString( sizebuf_t *sb, const char *s, const char *filename, int fileline )
{
	if( !s ) _MSG_WriteData( sb, "", 1, filename, fileline );
	else
	{
		int	l,i;
		char	string[MAX_STRING_CHARS];

		l = com.strlen( s );
		if( l >= MAX_STRING_CHARS )
		{
			MsgDev( D_ERROR, "MSG_WriteString: exceeds MAX_STRING_CHARS (called at %s:%i\n", filename, fileline );
			_MSG_WriteData( sb, "", 1, filename, fileline );
			return;
		}
		com.strncpy( string, s, sizeof( string ));

		// get rid of 0xff chars, because old clients don't like them
		for( i = 0 ; i < l ; i++ )
		{
			if(((byte *)string)[i] > 127 )
				string[i] = '.';
		}
		_MSG_WriteData( sb, string, l+1, filename, fileline );
	}
}

void _MSG_WriteCoord16(sizebuf_t *sb, int f, const char *filename, int fileline)
{
	_MSG_WriteLong(sb, f * SV_COORD_FRAC, filename, fileline );
}

void _MSG_WriteAngle16 (sizebuf_t *sb, float f, const char *filename, int fileline)
{
	_MSG_WriteWord(sb, ANGLE2SHORT(f), filename, fileline );
}

void _MSG_WriteCoord32(sizebuf_t *sb, float f, const char *filename, int fileline)
{
	_MSG_WriteFloat(sb, f, filename, fileline );
}

void _MSG_WriteAngle32(sizebuf_t *sb, float f, const char *filename, int fileline)
{
	_MSG_WriteFloat(sb, f, filename, fileline );
}

void _MSG_WritePos16(sizebuf_t *sb, int pos[3], const char *filename, int fileline)
{
	_MSG_WriteCoord16(sb, pos[0], filename, fileline );
	_MSG_WriteCoord16(sb, pos[1], filename, fileline );
	_MSG_WriteCoord16(sb, pos[2], filename, fileline );
}

void _MSG_WritePos32(sizebuf_t *sb, vec3_t pos, const char *filename, int fileline)
{
	_MSG_WriteCoord32(sb, pos[0], filename, fileline );
	_MSG_WriteCoord32(sb, pos[1], filename, fileline );
	_MSG_WriteCoord32(sb, pos[2], filename, fileline );
}

/*
=======================
   reading functions
=======================
*/
void MSG_BeginReading( sizebuf_t *msg )
{
	msg->huffman = false;
	msg->readcount = 0;
	msg->errorcount = 0;
	msg->bit = 0;
}

void MSG_EndReading( sizebuf_t *msg )
{
	if(!msg->errorcount) return;
	MsgDev( D_ERROR, "MSG_EndReading: received with errors\n");
}

// returns -1 if no more characters are available
int MSG_ReadChar( sizebuf_t *msg )
{
	int	c;
	
	c = (signed char)MSG_ReadBits( msg, 8 );
	if( msg->readcount > msg->cursize )
	{
		msg->errorcount++;
		c = -1;
	}	

	return c;
}

int MSG_ReadByte( sizebuf_t *msg )
{
	int	c;
	
	c = (byte)MSG_ReadBits( msg, 8 );
	if( msg->readcount > msg->cursize )
	{
		msg->errorcount++;
		c = -1;
	}	

	return c;
}

int MSG_ReadShort( sizebuf_t *msg )
{
	int	c;
	
	c = (short)MSG_ReadBits( msg, 16 );
	if( msg->readcount > msg->cursize )
	{
		msg->errorcount++;
		c = -1;
	}	

	return c;
}

int MSG_ReadWord( sizebuf_t *msg )
{
	int	c;
	
	c = (word)MSG_ReadBits( msg, 16 );
	if( msg->readcount > msg->cursize )
	{
		msg->errorcount++;
		c = -1;
	}	

	return c;
}

int MSG_ReadLong( sizebuf_t *msg )
{
	int	c;
	
	c = MSG_ReadBits( msg, 32 );
	if ( msg->readcount > msg->cursize )
	{
		msg->errorcount++;
		c = -1;
	}	
	
	return c;
}

float MSG_ReadFloat( sizebuf_t *msg )
{
	union
	{
		float	f;
		int	l;
	} dat;
	
	dat.l = MSG_ReadBits( msg, 32 );
	if( msg->readcount > msg->cursize )
	{
		msg->errorcount++;
		dat.f = -1;
	}	
	
	return dat.f;	
}

char *MSG_ReadString( sizebuf_t *msg )
{
	static char	string[MAX_STRING_CHARS];
	int		l = 0, c;
	
	do
	{
		// use MSG_ReadByte so -1 is out of bounds
		c = MSG_ReadByte( msg );
		if( c == -1 || c == 0 )
			break;

		// translate all fmt spec to avoid crash bugs
		if( c == '%' ) c = '.';
		// don't allow higher ascii values
		if( c > 127 ) c = '.';

		string[l] = c;
		l++;
	} while( l < sizeof(string) - 1 );
	string[l] = 0; // terminator
	
	return string;
}

char *MSG_ReadStringLine( sizebuf_t *msg )
{
	static char	string[MAX_STRING_CHARS];
	int		l = 0, c;
	
	do
	{
		// use MSG_ReadByte so -1 is out of bounds
		c = MSG_ReadByte( msg );
		if( c == -1 || c == 0 || c == '\n' )
			break;

		// translate all fmt spec to avoid crash bugs
		if( c == '%' ) c = '.';

		string[l] = c;
		l++;
	} while( l < sizeof(string) - 1 );
	string[l] = 0; // terminator
	
	return string;
}

void MSG_ReadData( sizebuf_t *msg, void *data, size_t len )
{
	int		i;

	for( i = 0; i < len; i++ )
		((byte *)data)[i] = MSG_ReadByte( msg );
}

long MSG_ReadCoord16( sizebuf_t *msg_read )
{
	return MSG_ReadLong( msg_read ) * CL_COORD_FRAC;
}

float MSG_ReadCoord32( sizebuf_t *msg_read )
{
	return MSG_ReadFloat( msg_read );
}

void MSG_ReadPos32( sizebuf_t *msg_read, vec3_t pos )
{
	pos[0] = MSG_ReadFloat(msg_read);
	pos[1] = MSG_ReadFloat(msg_read);
	pos[2] = MSG_ReadFloat(msg_read);
}

void MSG_ReadPos16( sizebuf_t *msg_read, int pos[3] )
{
	pos[0] = MSG_ReadCoord16(msg_read);
	pos[1] = MSG_ReadCoord16(msg_read);
	pos[2] = MSG_ReadCoord16(msg_read);
}

float MSG_ReadAngle16(sizebuf_t *msg_read)
{
	return SHORT2ANGLE(MSG_ReadShort(msg_read));
}

float MSG_ReadAngle32(sizebuf_t *msg_read)
{
	return MSG_ReadFloat(msg_read);
}

/*
=============================================================================

delta functions
  
=============================================================================
*/
void _MSG_WriteDelta( sizebuf_t *msg, int oldV, int newV, int bits, const char *filename, const int fileline )
{
	if( oldV == newV )
	{
		_MSG_WriteBits( msg, 0, 1, filename, fileline );
		return;
	}
	_MSG_WriteBits( msg, 1, 1, filename, fileline );
	_MSG_WriteBits( msg, newV, bits, filename, fileline );
}

int MSG_ReadDelta( sizebuf_t *msg, int oldV, int bits )
{
	if(MSG_ReadBits( msg, 1 ))
		return MSG_ReadBits( msg, bits );
	return oldV;
}

void _MSG_WriteDeltaFloat( sizebuf_t *msg, float oldV, float newV, const char *filename, const int fileline )
{
	if( oldV == newV )
	{
		_MSG_WriteBits( msg, 0, 1, filename, fileline );
		return;
	}
	_MSG_WriteBits( msg, 1, 1, filename, fileline );
	_MSG_WriteBits( msg, *(int *)&newV, 32, filename, fileline );
}

float MSG_ReadDeltaFloat( sizebuf_t *msg, float oldV )
{
	if(MSG_ReadBits( msg, 1 ))
	{
		float	newV;
		*(int *)&newV = MSG_ReadBits( msg, 32 );
		return newV;
	}
	return oldV;
}

/*
=====================
MSG_WriteDeltaUsercmd
=====================
*/
void _MSG_WriteDeltaUsercmd( sizebuf_t *msg, usercmd_t *from, usercmd_t *to, const char *filename, const int fileline )
{
	_MSG_WriteByte( msg, to->msec, filename, fileline );

	_MSG_WriteDelta( msg, from->angles[0], to->angles[0], 16, filename, fileline );
	_MSG_WriteDelta( msg, from->angles[1], to->angles[1], 16, filename, fileline );
	_MSG_WriteDelta( msg, from->angles[2], to->angles[2], 16, filename, fileline );
	_MSG_WriteDelta( msg, from->forwardmove, to->forwardmove, 16, filename, fileline );
	_MSG_WriteDelta( msg, from->sidemove, to->sidemove, 16, filename, fileline );
	_MSG_WriteDelta( msg, from->upmove, to->upmove, 16, filename, fileline );
	_MSG_WriteDelta( msg, from->buttons, to->buttons, 8, filename, fileline );
	_MSG_WriteDelta( msg, from->impulse, to->impulse, 8, filename, fileline );
	_MSG_WriteDelta( msg, from->lightlevel, to->lightlevel, 8, filename, fileline );
}

/*
=====================
MSG_ReadDeltaUsercmd
=====================
*/
void MSG_ReadDeltaUsercmd( sizebuf_t *msg, usercmd_t *from, usercmd_t *to )
{
	to->msec = MSG_ReadByte( msg );

	to->angles[0] = MSG_ReadDelta( msg, from->angles[0], 16 );
	to->angles[1] = MSG_ReadDelta( msg, from->angles[1], 16 );
	to->angles[2] = MSG_ReadDelta( msg, from->angles[2], 16 );
	to->forwardmove = MSG_ReadDelta( msg, from->forwardmove, 16 );
	to->sidemove = MSG_ReadDelta( msg, from->sidemove, 16 );
	to->upmove = MSG_ReadDelta( msg, from->upmove, 16 );
	to->buttons = MSG_ReadDelta( msg, from->buttons, 8 );
	to->impulse = MSG_ReadDelta( msg, from->impulse, 8 );
	to->lightlevel = MSG_ReadDelta( msg, from->lightlevel, 8 );
}

/*
=============================================================================

entity_state_t communication
  
=============================================================================
*/
/*
==================
MSG_WriteDeltaEntity

Writes part of a packetentities message, including the entity number.
Can delta from either a baseline or a previous packet_entity
If to is NULL, a remove entity update will be sent
If force is not set, then nothing at all will be generated if the entity is
identical, under the assumption that the in-order delta code will catch it.
==================
*/
void _MSG_WriteDeltaEntity( entity_state_t *from, entity_state_t *to, sizebuf_t *msg, bool force, const char *filename, int fileline ) 
{
	int		num_fields;
	net_field_t	*field;
	int		trunc;
	float		fullFloat;
	int		*fromF, *toF;
	int		i, lc = 0;

	num_fields = sizeof(ent_fields) / sizeof(ent_fields[0]);

	// all fields should be 32 bits to avoid any compiler packing issues
	// the "number" field is not part of the field list
	// if this assert fails, someone added a field to the entity_state_t
	// struct without updating the message fields
	if( num_fields + 1 == sizeof(*from ) / 4 ) Sys_Error( "ent_fileds s&3\n" ); 

	// a NULL to is a delta remove message
	if( to == NULL )
	{
		if( from == NULL ) return;
		MSG_WriteBits( msg, from->number, MAXEDICT_BITS );
		MSG_WriteBits( msg, 1, 1 );
		return;
	}

	if( to->number < 0 || to->number >= MAX_EDICTS )
		Host_Error( "MSG_WriteDeltaEntity: Bad entity number: %i (called at %s:%i)\n", to->number, filename, fileline );

	// build the change vector as bytes so it is endien independent
	for( i = 0, field = ent_fields; i < num_fields; i++, field++ )
	{
		fromF = (int *)((byte *)from + field->offset );
		toF = (int *)((byte *)to + field->offset );
		if(*fromF != *toF) lc = i + 1;
	}

	if( lc == 0 )
	{
		// nothing at all changed
		if( !force ) return;		// nothing at all

		// write two bits for no change
		MSG_WriteBits( msg, to->number, MAXEDICT_BITS );
		MSG_WriteBits( msg, 0, 1 );		// not removed
		MSG_WriteBits( msg, 0, 1 );		// no delta
		return;
	}

	MSG_WriteBits( msg, to->number, MAXEDICT_BITS );
	MSG_WriteBits( msg, 0, 1 );			// not removed
	MSG_WriteBits( msg, 1, 1 );			// we have a delta
	MSG_WriteByte( msg, lc );			// # of changes

	oldsize += num_fields;

	for( i = 0, field = ent_fields; i < lc; i++, field++ )
	{
		fromF = (int *)((byte *)from + field->offset );
		toF = (int *)((byte *)to + field->offset );

		if( *fromF == *toF )
		{
			MSG_WriteBits( msg, 0, 1 );	// no change
			continue;
		}
		MSG_WriteBits( msg, 1, 1 );	// changed

		if( field->bits == 0 )
		{
			// float
			fullFloat = *(float *)toF;
			trunc = (int)fullFloat;

			if( fullFloat == 0.0f )
			{
				MSG_WriteBits( msg, 0, 1 );
				oldsize += FLOAT_INT_BITS;
			}
			else
			{
				MSG_WriteBits( msg, 1, 1 );
				if( trunc == fullFloat && trunc + FLOAT_INT_BIAS >= 0 && trunc + FLOAT_INT_BIAS < (1<<FLOAT_INT_BITS))
				{
					// send as small integer
					MSG_WriteBits( msg, 0, 1 );
					MSG_WriteBits( msg, trunc + FLOAT_INT_BIAS, FLOAT_INT_BITS );
				}
				else
				{
					// send as full floating point value
					MSG_WriteBits( msg, 1, 1 );
					MSG_WriteBits( msg, *toF, 32 );
				}
			}
		}
		else
		{
			if( *toF == 0 )
			{
				MSG_WriteBits( msg, 0, 1 );
			}
			else
			{
				MSG_WriteBits( msg, 1, 1 );
				MSG_WriteBits( msg, *toF, field->bits );	// integer
			}
		}
	}
}

/*
==================
MSG_ReadDeltaEntity

The entity number has already been read from the message, which
is how the from state is identified.

If the delta removes the entity, entityState_t->number will be set to MAX_GENTITIES-1

Can go from either a baseline or a previous packet_entity
==================
*/
void MSG_ReadDeltaEntity( sizebuf_t *msg, entity_state_t *from, entity_state_t *to, int number )
{
	int		i, lc;
	int		num_fields;
	net_field_t	*field;
	int		*fromF, *toF;
	int		print;
	int		trunc;
	int		startBit, endBit;

	if( number < 0 || number >= MAX_EDICTS )
		Host_Error( "MSG_ReadDeltaEntity: bad delta entity number: %i\n", number );

	if( msg->bit == 0 ) startBit = msg->readcount * 8 - MAXEDICT_BITS;
	else startBit = ( msg->readcount - 1 ) * 8 + msg->bit - MAXEDICT_BITS;

	// check for a remove
	if( MSG_ReadBits( msg, 1 ) == 1 )
	{
		memset( to, 0, sizeof( *to ));	
		to->number = MAX_ENTNUMBER;
		MsgDev( D_INFO, "MSG_ReadDeltaEntity: %3i: #%-3i remove\n", msg->readcount, number );
		return;
	}

	// check for no delta
	if(MSG_ReadBits( msg, 1 ) == 0 )
	{
		*to = *from;
		to->number = number;
		return;
	}

	num_fields = sizeof( ent_fields)/sizeof(ent_fields[0]);
	lc = MSG_ReadByte( msg );

	// shownet 2/3 will interleave with other printed info, -1 will
	// just print the delta records`
	if( cl_shownet->integer >= 2 || cl_shownet->integer == -1 )
	{
		print = 1;
		MsgDev( D_INFO, "%3i: #%-3i ", msg->readcount, to->number );
	}
	else print = 0;

	to->number = number;

	for( i = 0, field = ent_fields; i < lc; i++, field++ )
	{
		fromF = (int *)( (byte *)from + field->offset );
		toF = (int *)( (byte *)to + field->offset );

		if(!MSG_ReadBits( msg, 1 ))
		{
			// no change
			*toF = *fromF;
		}
		else
		{
			if( field->bits == 0 )
			{
				// float
				if(MSG_ReadBits( msg, 1 ) == 0 )
				{
					*(float *)toF = 0.0f; 
				}
				else
				{
					if(MSG_ReadBits( msg, 1 ) == 0 )
					{
						// integral float
						trunc = MSG_ReadBits( msg, FLOAT_INT_BITS );
						// bias to allow equal parts positive and negative
						trunc -= FLOAT_INT_BIAS;
						*(float *)toF = trunc; 
						if( print ) MsgDev( D_INFO, "%s:%i ", field->name, trunc );
					}
					else
					{
						// full floating point value
						*toF = MSG_ReadBits( msg, 32 );
						if( print ) MsgDev( D_INFO, "%s:%f ", field->name, *(float *)toF );
					}
				}
			}
			else
			{
				if(MSG_ReadBits( msg, 1 ) == 0 )
				{
					*toF = 0;
				}
				else
				{
					// integer
					*toF = MSG_ReadBits( msg, field->bits );
					if( print ) MsgDev( D_INFO, "%s:%i ", field->name, *toF );
				}
			}
		}
	}
	for( i = lc, field = &ent_fields[lc]; i < num_fields; i++, field++ )
	{
		fromF = (int *)( (byte *)from + field->offset );
		toF = (int *)( (byte *)to + field->offset );
		*toF = *fromF; // no change
	}

	if( print )
	{
		if( msg->bit == 0 ) endBit = msg->readcount * 8 - MAXEDICT_BITS;
		else endBit = ( msg->readcount - 1 ) * 8 + msg->bit - MAXEDICT_BITS;
		MsgDev( D_INFO, " (%i bits)\n", endBit - startBit  );
	}
}

/*
============================================================================

player_state_t communication

============================================================================
*/
/*
=============
MSG_WriteDeltaPlayerstate

=============
*/
void MSG_WriteDeltaPlayerstate( player_state_t *from, player_state_t *to, sizebuf_t *msg )
{
	player_state_t	dummy;
	int		statsbits;
	int		num_fields;
	int		i, c;
	net_field_t	*field;
	int		*fromF, *toF;
	float		fullFloat;
	int		trunc, lc = 0;

	if( !from )
	{
		from = &dummy;
		memset( &dummy, 0, sizeof(dummy));
	}

	c = msg->cursize;
	num_fields = sizeof(ps_fields) / sizeof(ps_fields[0]);

	// all fields should be 32 bits to avoid any compiler packing issues
	// the "number" field is not part of the field list
	// if this assert fails, someone added a field to the entity_state_t
	// struct without updating the message fields
	if( num_fields + 1 != sizeof(*from ) / 4 ) Sys_Error( "ps_fields s&3\n" ); 

	for( i = 0, field = ps_fields; i < num_fields; i++, field++ )
	{
		fromF = (int *)((byte *)from + field->offset );
		toF = (int *)((byte *)to + field->offset );
		if( *fromF != *toF ) lc = i + 1;
	}

	MSG_WriteByte( msg, lc );			// # of changes
	oldsize += num_fields - lc;

	for( i = 0, field = ps_fields; i < lc; i++, field++ )
	{
		fromF = (int *)((byte *)from + field->offset );
		toF = (int *)((byte *)to + field->offset );

		if( *fromF == *toF )
		{
			MSG_WriteBits( msg, 0, 1 );	// no change
			continue;
		}

		MSG_WriteBits( msg, 1, 1 );		// changed

		if( field->bits == 0 )
		{
			// float
			fullFloat = *(float *)toF;
			trunc = (int)fullFloat;

			if( trunc == fullFloat && trunc + FLOAT_INT_BIAS >= 0 && trunc + FLOAT_INT_BIAS < (1<<FLOAT_INT_BITS))
			{
				// send as small integer
				MSG_WriteBits( msg, 0, 1 );
				MSG_WriteBits( msg, trunc + FLOAT_INT_BIAS, FLOAT_INT_BITS );
			}
			else
			{
				// send as full floating point value
				MSG_WriteBits( msg, 1, 1 );
				MSG_WriteBits( msg, *toF, 32 );
			}
		}
		else
		{
			// integer
			MSG_WriteBits( msg, *toF, field->bits );
		}
	}
	c = msg->cursize - c;


	// send the arrays
	statsbits = 0;
	for( i = 0; i < MAX_STATS; i++ )
	{
		if( to->stats[i] != from->stats[i] )
			statsbits |= 1<<i;
	}
	if( !statsbits )
	{
		MSG_WriteBits( msg, 0, 1 );	// no change
		oldsize += 4;
		return;
	}
	MSG_WriteBits( msg, 1, 1 );		// changed

	if( statsbits )
	{
		MSG_WriteBits( msg, 1, 1 );	// changed
		MSG_WriteShort( msg, statsbits );
		for( i = 0; i < MAX_STATS; i++ )
		{
			if( statsbits & (1<<i))
				MSG_WriteShort( msg, to->stats[i]);
		}
	}
	else MSG_WriteBits( msg, 0, 1 );	// no change
}


/*
===================
MSG_ReadDeltaPlayerstate
===================
*/
void MSG_ReadDeltaPlayerstate( sizebuf_t *msg, player_state_t *from, player_state_t *to )
{
	int		i, lc;
	int		bits;
	net_field_t	*field;
	int		num_fields;
	int		startBit, endBit;
	int		*fromF, *toF;
	int		print;
	int		trunc;
	player_state_t	dummy;

	if( !from )
	{
		from = &dummy;
		memset( &dummy, 0, sizeof( dummy ));
	}
	*to = *from;

	if( msg->bit == 0 ) startBit = msg->readcount * 8 - MAXEDICT_BITS;
	else startBit = ( msg->readcount - 1 ) * 8 + msg->bit - MAXEDICT_BITS;

	// shownet 2/3 will interleave with other printed info, -2 will
	// just print the delta records
	if( cl_shownet->integer >= 2 || cl_shownet->integer == -2 )
	{
		print = 1;
		MsgDev( D_INFO, "%3i: playerstate ", msg->readcount );
	}
	else print = 0;

	num_fields = sizeof(ps_fields) / sizeof(ps_fields[0] );
	lc = MSG_ReadByte( msg );

	for( i = 0, field = ps_fields; i < lc; i++, field++ )
	{
		fromF = (int *)( (byte *)from + field->offset );
		toF = (int *)( (byte *)to + field->offset );

		if(!MSG_ReadBits( msg, 1 ))
		{
			// no change
			*toF = *fromF;
		}
		else
		{
			if( field->bits == 0 )
			{
				// float
				if( MSG_ReadBits( msg, 1 ) == 0 )
				{
					// integral float
					trunc = MSG_ReadBits( msg, FLOAT_INT_BITS );
					// bias to allow equal parts positive and negative
					trunc -= FLOAT_INT_BIAS;
					*(float *)toF = trunc; 
					if( print ) MsgDev( D_INFO, "%s:%i ", field->name, trunc );
				}
				else
				{
					// full floating point value
					*toF = MSG_ReadBits( msg, 32 );
					if( print ) MsgDev( D_INFO, "%s:%f ", field->name, *(float *)toF );
				}
			}
			else
			{
				// integer
				*toF = MSG_ReadBits( msg, field->bits );
				if( print ) MsgDev( D_INFO, "%s:%i ", field->name, *toF );
			}
		}
	}
	for( i = lc, field = &ps_fields[lc]; i < num_fields; i++, field++ )
	{
		fromF = (int *)( (byte *)from + field->offset );
		toF = (int *)( (byte *)to + field->offset );
		// no change
		*toF = *fromF;
	}


	// read the arrays
	if( MSG_ReadBits( msg, 1 ))
	{
		// parse stats
		if( MSG_ReadBits( msg, 1 ))
		{
			bits = MSG_ReadShort( msg );
			for( i = 0; i < MAX_STATS; i++ )
			{
				if( bits & (1<<i))
					to->stats[i] = MSG_ReadShort( msg );
			}
		}
	}

	if( print )
	{
		if( msg->bit == 0 ) endBit = msg->readcount * 8 - MAXEDICT_BITS;
		else endBit = ( msg->readcount - 1 ) * 8 + msg->bit - MAXEDICT_BITS;
		MsgDev( D_INFO, " (%i bits)\n", endBit - startBit );
	}
}