//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        stdlib.c - std lib portable utils
//=======================================================================

#include "launch.h"

/*
====================
timestamp
====================
*/
const char *timestamp( int format )
{
	static string	timestamp;
	time_t		crt_time;
	const struct tm	*crt_tm;
	string		timestring;

	time( &crt_time );
	crt_tm = localtime( &crt_time );

	switch( format )
	{
	case TIME_FULL:
		// Build the full timestamp (ex: "Apr03 2007 [23:31.55]");
		strftime( timestring, sizeof (timestring), "%b%d %Y [%H:%M.%S]", crt_tm );
		break;
	case TIME_DATE_ONLY:
		// Build the date stamp only (ex: "Apr03 2007");
		strftime( timestring, sizeof (timestring), "%b%d %Y", crt_tm );
		break;
	case TIME_TIME_ONLY:
		// Build the time stamp only (ex: "23:31.55");
		strftime( timestring, sizeof (timestring), "%H:%M.%S", crt_tm );
		break;
	case TIME_NO_SECONDS:
		// Build the time stamp exclude seconds (ex: "13:46");
		strftime( timestring, sizeof (timestring), "%H:%M", crt_tm );
		break;
	case TIME_YEAR_ONLY:
		// Build the date stamp year only (ex: "2006");
		strftime( timestring, sizeof (timestring), "%Y", crt_tm );
		break;
	case TIME_FILENAME:
		// Build a timestamp that can use for filename (ex: "Nov2006-26 (19.14.28)");
		strftime( timestring, sizeof (timestring), "%b%Y-%d_%H.%M.%S", crt_tm );
		break;
	default: return NULL;
	}

	strncpy( timestamp, timestring, sizeof( timestamp ));
	return timestamp;
}

/*
============
va

does a varargs printf into a temp buffer,
so I don't need to have varargs versions
of all text functions.
============
*/
char *va( const char *format, ... )
{
	va_list		argptr;
	static char	string[256][1024], *s;	// g-cont. 256 temporary strings should be enough...
	static int	stringindex = 0;

	s = string[stringindex];
	stringindex = (stringindex + 1) & 255;
	va_start( argptr, format );
	_vsnprintf( s, sizeof( string[0] ), format, argptr );
	va_end( argptr );

	return s;
}

/*
============
Com_FileBase

TEMPORARY PLACE. dont forget to remove it
============
*/
void Com_FileBase( const char *in, char *out )
{
	int	len, start, end;

	len = strlen( in );
	if( !len ) return;
	
	// scan backward for '.'
	end = len - 1;

	while( end && in[end] != '.' && in[end] != '/' && in[end] != '\\' )
		end--;
	
	if( in[end] != '.' )
		end = len-1; // no '.', copy to end
	else end--; // found ',', copy to left of '.'


	// scan backward for '/'
	start = len - 1;

	while( start >= 0 && in[start] != '/' && in[start] != '\\' )
		start--;

	if( start < 0 || ( in[start] != '/' && in[start] != '\\' ))
		start = 0;
	else start++;

	// length of new sting
	len = end - start + 1;

	// Copy partial string
	strncpy( out, &in[start], len + 1 );
	out[len] = 0;
}