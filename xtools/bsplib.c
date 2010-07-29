#include "xtools.h"
#include "utils.h"

// FIXME: implement bsplib

void Bsp_PrintLog( const char *pMsg )
{
	if( !enable_log ) return;
	if( !bsplog ) bsplog = FS_Open( va( "maps/%s.log", gs_filename ), "ab" );
	FS_Print( bsplog, pMsg );
}

bool PrepareBSPModel( int argc, char **argv )
{
	Sys_Break( "\r\rbsplib not implemented. Wait for Xash 0.72\r\r" );
	return false;
}

bool CompileBSPModel ( void )
{
	return false;
}