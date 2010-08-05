//=======================================================================
//			Copyright XashXT Group 2007 ©
//			net_msg.c - network messages
//=======================================================================

#include "common.h"
#include "protocol.h"
#include "byteorder.h"
#include "mathlib.h"

// angles pack methods
#define ANGLE2CHAR(x)	((int)((x)*256 / 360) & 255)
#define CHAR2ANGLE(x)	((x)*(360.0f / 256))
#define ANGLE2SHORT(x)	((int)((x)*65536 / 360) & 65535)
#define SHORT2ANGLE(x)	((x)*(360.0f / 65536))

// probably movevars_t never reached 32 field integer limit (in theory of course)
static net_field_t move_fields[] =
{
{ PM_FIELD(gravity),	NET_FLOAT, false	},
{ PM_FIELD(stopspeed),	NET_FLOAT, false	},
{ PM_FIELD(maxspeed),	NET_FLOAT, false	},
{ PM_FIELD(spectatormaxspeed),NET_FLOAT, false	},
{ PM_FIELD(accelerate),	NET_FLOAT, false	},
{ PM_FIELD(airaccelerate),	NET_FLOAT, false	},
{ PM_FIELD(wateraccelerate),	NET_FLOAT, false	},
{ PM_FIELD(friction),	NET_FLOAT, false	},
{ PM_FIELD(edgefriction),	NET_FLOAT, false	},
{ PM_FIELD(waterfriction),	NET_FLOAT, false	},
{ PM_FIELD(bounce),		NET_FLOAT, false	},
{ PM_FIELD(stepsize),	NET_FLOAT, false	},
{ PM_FIELD(maxvelocity),	NET_FLOAT, false	},
{ PM_FIELD(footsteps),	NET_FLOAT, false	},
{ PM_FIELD(rollangle),	NET_FLOAT, false	},
{ PM_FIELD(rollspeed),	NET_FLOAT, false	},
{ NULL },
};

/*
=============================================================================

SZ BUFFER (io functions)
  
=============================================================================
*/
/*
=======================
MSG_Init

init new buffer
=======================
*/
void MSG_Init( sizebuf_t *buf, byte *data, size_t length )
{
	Mem_Set( buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
}

/*
=======================
MSG_GetSpace

get some space for write 
=======================
*/
void *MSG_GetSpace( sizebuf_t *msg, size_t length )
{
	void	*data;
	
	if( msg->cursize + length > msg->maxsize )
	{
		if( length > msg->maxsize )
			Host_Error("MSG_GetSpace: length[%i] > buffer maxsize [%i]\n", length, msg->maxsize );
		MsgDev( D_WARN, "MSG_GetSpace: overflow\n", msg->cursize + length, msg->maxsize );
		MSG_Clear( msg ); 
		msg->overflowed = true;
	}
	data = msg->data + msg->cursize;
	msg->cursize += length;

	return data;
}

/*
=======================
MSG_Print

used for write sv.forward cmds
=======================
*/
void MSG_Print( sizebuf_t *msg, const char *data )
{
	size_t	length = com.strlen(data) + 1;

	if( msg->cursize )
	{
		if(msg->data[msg->cursize - 1]) Mem_Copy((byte *)MSG_GetSpace( msg, length ), data, length );
		else Mem_Copy((byte *)MSG_GetSpace( msg, length - 1) - 1, data, length ); // write over trailing 0
	}
	else Mem_Copy((byte *)MSG_GetSpace( msg, length ), data, length );
}

/*
=======================
MSG_WriteData

used for swap buffers
=======================
*/
void _MSG_WriteData( sizebuf_t *buf, const void *data, size_t length, const char *filename, int fileline )
{
	Mem_Copy( MSG_GetSpace( buf, length ), data, length );	
}


void MSG_Clear( sizebuf_t *buf )
{
	buf->cursize = 0;
	buf->overflowed = false;
}

void MSG_BeginReading( sizebuf_t *msg )
{
	msg->readcount = 0;
}

/*
=======================
MSG_WriteBits

write # of bytes
=======================
*/

void _MSG_WriteBits( sizebuf_t *msg, long value, const char *name, int net_type, const char *filename, const int fileline )
{
	ftol_t	dat;
	byte	*buf;

	// this isn't an exact overflow check, but close enough
	if( msg->maxsize - msg->cursize < 4 )
	{
		MsgDev( D_ERROR, "MSG_WriteBits: overflowed %i > %i (called at %s:%i)\n", msg->cursize, msg->maxsize, filename, fileline );
		msg->overflowed = true;
		return;
	}
	dat.l = value;
/*
	switch( net_type )
	{
	case NET_SCALE:
		value = dat.f * 4;	
		buf = MSG_GetSpace( msg, 1 );
		buf[0] = value;
		break;
	case NET_COLOR:
		value = bound( 0, dat.f, 255 );
		buf = MSG_GetSpace( msg, 1 );
		buf[0] = value;
		break;
	case NET_CHAR:
	case NET_BYTE:
		buf = MSG_GetSpace( msg, 1 );
		buf[0] = value;
		break;
	case NET_SHORT:
	case NET_WORD:
		buf = MSG_GetSpace( msg, 2 );
		buf[0] = value & 0xff;
		buf[1] = value>>8;
		break;
	case NET_LONG:
	case NET_FLOAT:
		buf = MSG_GetSpace( msg, 4 );
		buf[0] = (value>>0 ) & 0xff;
		buf[1] = (value>>8 ) & 0xff;
		buf[2] = (value>>16) & 0xff;
		buf[3] = (value>>24);
		break;
	case NET_ANGLE8:
		if( dat.f > 360 ) dat.f -= 360; 
		else if( dat.f < 0 ) dat.f += 360;
		value = ANGLE2CHAR( dat.f );
		buf = MSG_GetSpace( msg, 1 );
		buf[0] = value;
		break;
	case NET_ANGLE:
		if( dat.f > 360 ) dat.f -= 360; 
		else if( dat.f < 0 ) dat.f += 360;
		value = ANGLE2SHORT( dat.f );
		buf = MSG_GetSpace( msg, 2 );
		buf[0] = value & 0xff;
		buf[1] = value>>8;
		break;
	case NET_COORD:
		value = dat.f * 8;	
		buf = MSG_GetSpace( msg, 2 );
		buf[0] = value & 0xff;
		buf[1] = value>>8;
		break;
	default:
		Host_Error( "MSG_WriteBits: bad net.type %i (called at %s:%i)\n", net_type, filename, fileline );			
		break;
	}
*/
	if((NWDesc[net_type].min_range + NWDesc[net_type].max_range) != 0 )
	{
		// check range
		if( value < NWDesc[net_type].min_range || value > NWDesc[net_type].max_range )
		{
			MsgDev( D_INFO, "MSG_Write%s: ", NWDesc[net_type].name );
			if( name ) MsgDev( D_INFO, "'%s' ", name );
			MsgDev( D_INFO, "range error %i should be in range (%i", value, NWDesc[net_type].min_range );
			MsgDev( D_INFO, " %i)(called at %s:%i)\n", NWDesc[net_type].max_range, filename, fileline );
          	}
          }
}

/*
=======================
MSG_ReadBits

read # of bytes
=======================
*/
long _MSG_ReadBits( sizebuf_t *msg, const char *name, int net_type, const char *filename, const int fileline )
{
	ftol_t	dat;
	long	value = 0;
/*
	switch( net_type )
	{
	case NET_SCALE:
		value = (signed char)(msg->data[msg->readcount]);
		dat.f = value * 0.25f;
		msg->readcount += 1;
		break;
	case NET_COLOR:
		value = (byte)(msg->data[msg->readcount]);
		dat.f = value;
		msg->readcount += 1;
		break;
	case NET_CHAR:
		dat.l = (signed char)msg->data[msg->readcount];
		msg->readcount += 1;
		break;
	case NET_BYTE:
		dat.l = (byte)msg->data[msg->readcount];
		msg->readcount += 1;
		break;
	case NET_WORD:
	case NET_SHORT:
		dat.l = (short)BuffLittleShort( msg->data + msg->readcount );
		msg->readcount += 2;
		break;
	case NET_LONG:
	case NET_FLOAT:
		dat.l = (long)BuffLittleLong( msg->data + msg->readcount );
		msg->readcount += 4;
		break;
	case NET_ANGLE8:
		value = (unsigned char)msg->data[msg->readcount];
		dat.f = CHAR2ANGLE( value );
		if( dat.f < -180 ) dat.f += 360; 
		else if( dat.f > 180 ) dat.f -= 360;
		msg->readcount += 1;
		break;
	case NET_ANGLE:
		value = (unsigned short)BuffLittleShort( msg->data + msg->readcount );
		dat.f = SHORT2ANGLE( value );
		if( dat.f < -180 ) dat.f += 360; 
		else if( dat.f > 180 ) dat.f -= 360;
		msg->readcount += 2;
		break;		
	case NET_COORD:
		value = (short)BuffLittleShort( msg->data + msg->readcount );
		dat.f = value * 0.125f;
		msg->readcount += 2;
		break;		
	default:
		Host_Error( "MSG_ReadBits: bad net.type %i, (called at %s:%i)\n", net_type, filename, fileline );			
		break;
	}
*/
	value = dat.l;

	// end of message or error reading
	if( msg->readcount > msg->cursize )
	{
		if(( msg->readcount - msg->cursize ) > 1 )
		{
			MsgDev( D_ERROR, "MSG_Read%s: ", NWDesc[net_type].name );
			MsgDev( D_ERROR, "msg total size %i, reading %i\n", msg->cursize, msg->readcount );
			msg->error = true;
		}
		return -1;
	}
	return value;
}

/*
==============================================================================

			MESSAGE IO FUNCTIONS
	       Handles byte ordering and avoids alignment errors
==============================================================================
*/
void _MSG_WriteString( sizebuf_t *sb, const char *src, const char *filename, int fileline )
{
	if( !src )
	{
		_MSG_WriteData( sb, "", 1, filename, fileline );
	}
	else
	{

		char	string[MAX_SYSPATH];
		int	l = com.strlen( src ) + 1;		

		if( l >= MAX_SYSPATH )
		{
			MsgDev( D_ERROR, "MSG_WriteString: exceeds %i symbols (called at %s:%i\n", MAX_SYSPATH, filename, fileline );
			_MSG_WriteData( sb, "", 1, filename, fileline );
			return;
		}

		com.strncpy( string, src, sizeof( string ));
		_MSG_WriteData( sb, string, l, filename, fileline );
	}
}

void _MSG_WriteStringLine( sizebuf_t *sb, const char *src, const char *filename, int fileline )
{
	if( !src )
	{
		_MSG_WriteData( sb, "", 1, filename, fileline );
	}
	else
	{
		int	l;
		char	*dst, string[MAX_SYSPATH];
                    
		l = com.strlen( src ) + 1;		
		if( l >= MAX_SYSPATH )
		{
			MsgDev( D_ERROR, "MSG_WriteString: exceeds %i symbols (called at %s:%i\n", MAX_SYSPATH, filename, fileline );
			_MSG_WriteData( sb, "", 1, filename, fileline );
			return;
		}

		dst = string;

		while( 1 )
		{
			// some escaped chars parsed as two symbols - merge it here
			if( src[0] == '\\' && src[1] == 'n' )
			{
				*dst++ = '\n';
				src += 2;
				l -= 1;
			}
			if( src[0] == '\\' && src[1] == 'r' )
			{
				*dst++ = '\r';
				src += 2;
				l -= 1;
			}
			if( src[0] == '\\' && src[1] == 't' )
			{
				*dst++ = '\t';
				src += 2;
				l -= 1;
			}
			else if(( *dst++ = *src++ ) == 0 )
				break;
		}
		*dst = '\0'; // string end

		_MSG_WriteData( sb, string, l, filename, fileline );
	}
}

char *MSG_ReadString( sizebuf_t *msg )
{
	static char	string[MAX_SYSPATH];
	int		l = 0, c;
	
	do
	{
		// use MSG_ReadByte so -1 is out of bounds
		c = MSG_ReadByte( msg );
		if( c == -1 || c == '\0' )
			break;

		// translate all fmt spec to avoid crash bugs
		if( c == '%' ) c = '.';

		string[l] = c;
		l++;
	} while( l < sizeof(string) - 1 );
	string[l] = 0; // terminator
	
	return string;
}

char *MSG_ReadStringLine( sizebuf_t *msg )
{
	static char	string[MAX_SYSPATH];
	int		l = 0, c;
	
	do
	{
		// use MSG_ReadByte so -1 is out of bounds
		c = MSG_ReadByte( msg );
		if( c == -1 || c == '\0' || c == '\n' )
			break;

		// translate all fmt spec to avoid crash bugs
		if( c == '%' ) c = '.';

		string[l] = c;
		l++;
	} while( l < sizeof(string) - 1 );
	string[l] = 0; // terminator
	
	return string;
}

/*
=============================================================================

movevars_t communication
  
=============================================================================
*/
bool BF_WriteDeltaMovevars( bitbuf_t *msg, movevars_t *from, movevars_t *to )
{
	int		num_fields;
	net_field_t	*field;
	int		*fromF, *toF;
	int		i, flags = 0;
	
	num_fields = (sizeof( move_fields ) / sizeof( move_fields[0] )) - 1;
	if( num_fields > MASK_FLAGS ) return false; // this should never happen

	// compare fields
	for( i = 0, field = move_fields; i < num_fields; i++, field++ )
	{
		fromF = (int *)((byte *)from + field->offset );
		toF = (int *)((byte *)to + field->offset );
		if(*fromF != *toF || field->force) flags |= 1<<i;
	}

	// nothing at all changed
	if( flags == 0 ) return false;

	BF_WriteByte( msg, svc_movevars );
	BF_WriteLong( msg, flags );	// send flags who indicates changes
	for( i = 0, field = move_fields; i < num_fields; i++, field++ )
	{
		toF = (int *)((byte *)to + field->offset );
		if( flags & 1<<i ) BF_WriteBits( msg, toF, field->bits );
	}
	return true;
}

void BF_ReadDeltaMovevars( bitbuf_t *msg, movevars_t *from, movevars_t *to )
{
	net_field_t	*field;
	int		i, flags;
	int		*fromF, *toF;

	*to = *from;

	for( i = 0, field = move_fields; field->name; i++, field++ )
	{
		// get flags of next packet if LONG out of range
		if(( i & MASK_FLAGS ) == 0) flags = BF_ReadLong( msg );
		fromF = (int *)((byte *)from + field->offset );
		toF = (int *)((byte *)to + field->offset );
		
		if( flags & ( 1<<( i & MASK_FLAGS )))
			BF_ReadBits( msg, toF, field->bits );
		else *toF = *fromF;	// no change
	}
}