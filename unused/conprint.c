dword g_color_table[8] =
{
FOREGROUND_INTENSITY,				// black
FOREGROUND_RED|FOREGROUND_INTENSITY,			// red
FOREGROUND_GREEN|FOREGROUND_INTENSITY,			// green
FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY,	// yellow
FOREGROUND_BLUE|FOREGROUND_INTENSITY,			// blue
FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY,	// cyan
FOREGROUND_RED|FOREGROUND_BLUE|FOREGROUND_INTENSITY,	// magenta
FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE,		// default color (white)
};

/*
================
ConsolePrint

print into win32 console
================
*/
void ConsolePrint( const char *pMsg )
{
	const char	*msg = pMsg;
	char		buffer[32768];
	char		logbuf[32768];
	char		*b = buffer;
	char		*c = logbuf;	
	int		i = 0;

	char tmpBuf[8192];
	HANDLE hOut = GetStdHandle( STD_OUTPUT_HANDLE );
	char *pTemp = tmpBuf;
	dword cbWritten;

	while( pMsg && *pMsg )
	{
		if( IsColorString( pMsg ))
		{
			if(( pTemp - tmpBuf ) > 0 )
			{
				// dump accumulated text before change color
				*pTemp = 0; // terminate string
				WriteFile( hOut, tmpBuf, Q_strlen( tmpBuf ), &cbWritten, 0 );
				PrintLog( tmpBuf );
				pTemp = tmpBuf;
			}

			// set new color
			SetConsoleTextAttribute( hOut, g_color_table[ColorIndex( *(pMsg + 1))] );
			pMsg += 2; // skip color info
		}
		else if(( pTemp - tmpBuf ) < sizeof( tmpBuf ) - 1 )
		{
			*pTemp++ = *pMsg++;
		}
		else
		{
			// temp buffer is full, dump it now
			*pTemp = 0; // terminate string
			WriteFile( hOut, tmpBuf, Q_strlen( tmpBuf ), &cbWritten, 0 );
			PrintLog( tmpBuf );
			pTemp = tmpBuf;
		}
	}

	// check for last portion
	if(( pTemp - tmpBuf ) > 0 )
	{
		// dump accumulated text
		*pTemp = 0; // terminate string
		WriteFile( hOut, tmpBuf, Q_strlen( tmpBuf ), &cbWritten, 0 );
		PrintLog( tmpBuf );
		pTemp = tmpBuf;
	}
}
