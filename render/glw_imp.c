/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/*
** GLW_IMP.C
**
** This file contains ALL Win32 specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_Shutdown
** GLimp_SwitchFullscreen
**
*/

#include <assert.h>
#include "gl_local.h"

static bool GLimp_SwitchFullscreen( int width, int height );
bool GLimp_InitGL (void);

glwstate_t glw_state;
static char wndname[128];

extern cvar_t *vid_fullscreen;

static bool VerifyDriver( void )
{
	char buffer[1024];

	strcpy( buffer, qglGetString( GL_RENDERER ));
	strlwr( buffer );

	if (!strcmp( buffer, "gdi generic" ))
	{
		if ( !glw_state.mcd_accelerated )
			return false;
	}
	return true;
}

/*
** VID_CreateWindow
*/
#define	WINDOW_CLASS_NAME	"Xash Window"

bool VID_CreateWindow( int width, int height, bool fullscreen )
{
	WNDCLASS		wc;
	RECT			r;
	cvar_t			*vid_xpos, *vid_ypos;
	int				stylebits;
	int				x, y, w, h;
	int				exstyle;

	strcpy(wndname, FS_Title());//critical stuff.

	// Register the frame class
	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC)glw_state.wndproc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = glw_state.hInstance;
	wc.hIcon         = LoadIcon( glw_state.hInstance, MAKEINTRESOURCE(101));
	wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = (void *)COLOR_3DSHADOW;
	wc.lpszMenuName  = 0;
	wc.lpszClassName = WINDOW_CLASS_NAME;

	if (!RegisterClass (&wc))
	{ 
		Msg("Couldn't register window class");
		return false;
	}

	if (fullscreen)
	{
		exstyle = WS_EX_TOPMOST;
		stylebits = WS_POPUP|WS_VISIBLE;
	}
	else
	{
		exstyle = 0;
		stylebits = WINDOW_STYLE;
	}

	r.left = 0;
	r.top = 0;
	r.right  = width;
	r.bottom = height;

	AdjustWindowRect (&r, stylebits, FALSE);

	w = r.right - r.left;
	h = r.bottom - r.top;

	if (fullscreen)
	{
		x = 0;
		y = 0;
	}
	else
	{
		vid_xpos = ri.Cvar_Get ("vid_xpos", "0", 0);
		vid_ypos = ri.Cvar_Get ("vid_ypos", "0", 0);
		x = vid_xpos->value;
		y = vid_ypos->value;
	}

	glw_state.hWnd = CreateWindowEx( exstyle, WINDOW_CLASS_NAME, wndname, stylebits, x, y, w, h, NULL, NULL, glw_state.hInstance, NULL);

	if (!glw_state.hWnd) 
	{
		Msg("Couldn't create window");
		return false;
	}
	
	ShowWindow( glw_state.hWnd, SW_SHOW );
	UpdateWindow( glw_state.hWnd );

	// init all the gl stuff for the window
	if (!GLimp_InitGL ())
	{
		Msg("VID_CreateWindow() - GLimp_InitGL failed\n");
		return false;
	}

	SetForegroundWindow( glw_state.hWnd );
	SetFocus( glw_state.hWnd );

	// let the sound and input subsystems know about the new window
	ri.Vid_NewWindow (width, height);

	return true;
}


/*
** GLimp_SetMode
*/
rserr_t GLimp_SetMode( int *pwidth, int *pheight, int mode, bool fullscreen )
{
	int width, height;
	const char *win_fs[] = { "W", "FS" };

	Msg("Initializing OpenGL display\n");

	Msg("...setting mode %d:", mode );

	if ( !ri.Vid_GetModeInfo( &width, &height, mode ) )
	{
		Msg(" invalid mode\n" );
		return rserr_invalid_mode;
	}

	Msg(" %d %d %s\n", width, height, win_fs[fullscreen] );

	// destroy the existing window
	if (glw_state.hWnd)
	{
		GLimp_Shutdown ();
	}

	// do a CDS if needed
	if ( fullscreen )
	{
		DEVMODE dm;

		Msg("...attempting fullscreen\n" );

		memset( &dm, 0, sizeof( dm ) );

		dm.dmSize = sizeof( dm );

		dm.dmPelsWidth  = width;
		dm.dmPelsHeight = height;
		dm.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT;

		if ( gl_bitdepth->value != 0 )
		{
			dm.dmBitsPerPel = gl_bitdepth->value;
			dm.dmFields |= DM_BITSPERPEL;
			Msg("...using gl_bitdepth of %d\n", ( int ) gl_bitdepth->value );
		}
		else
		{
			HDC hdc = GetDC( NULL );
			int bitspixel = GetDeviceCaps( hdc, BITSPIXEL );

			Msg("...using desktop display depth of %d\n", bitspixel );

			ReleaseDC( 0, hdc );
		}

		Msg("...calling CDS: " );
		if ( ChangeDisplaySettings( &dm, CDS_FULLSCREEN ) == DISP_CHANGE_SUCCESSFUL )
		{
			*pwidth = width;
			*pheight = height;

			gl_state.fullscreen = true;

			Msg("ok\n" );

			if ( !VID_CreateWindow (width, height, true) )
				return rserr_invalid_mode;

			return rserr_ok;
		}
		else
		{
			*pwidth = width;
			*pheight = height;

			Msg("failed\n" );

			Msg("...calling CDS assuming dual monitors:" );

			dm.dmPelsWidth = width * 2;
			dm.dmPelsHeight = height;
			dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

			if ( gl_bitdepth->value != 0 )
			{
				dm.dmBitsPerPel = gl_bitdepth->value;
				dm.dmFields |= DM_BITSPERPEL;
			}

			/*
			** our first CDS failed, so maybe we're running on some weird dual monitor
			** system 
			*/
			if ( ChangeDisplaySettings( &dm, CDS_FULLSCREEN ) != DISP_CHANGE_SUCCESSFUL )
			{
				Msg(" failed\n" );

				Msg("...setting windowed mode\n" );

				ChangeDisplaySettings( 0, 0 );

				*pwidth = width;
				*pheight = height;
				gl_state.fullscreen = false;
				if ( !VID_CreateWindow (width, height, false) )
					return rserr_invalid_mode;
				return rserr_invalid_fullscreen;
			}
			else
			{
				Msg(" ok\n" );
				if ( !VID_CreateWindow (width, height, true) )
					return rserr_invalid_mode;

				gl_state.fullscreen = true;
				return rserr_ok;
			}
		}
	}
	else
	{
		Msg("...setting windowed mode\n" );

		ChangeDisplaySettings( 0, 0 );

		*pwidth = width;
		*pheight = height;
		gl_state.fullscreen = false;
		if ( !VID_CreateWindow (width, height, false) )
			return rserr_invalid_mode;
	}

	return rserr_ok;
}

/*
** GLimp_Shutdown
**
** This routine does all OS specific shutdown procedures for the OpenGL
** subsystem.  Under OpenGL this means NULLing out the current DC and
** HGLRC, deleting the rendering context, and releasing the DC acquired
** for the window.  The state structure is also nulled out.
**
*/
void GLimp_Shutdown( void )
{
	if ( qwglMakeCurrent && !qwglMakeCurrent( NULL, NULL ) )
		Msg("R_Shutdown() - wglMakeCurrent failed\n");
	if ( glw_state.hGLRC )
	{
		if (  qwglDeleteContext && !qwglDeleteContext( glw_state.hGLRC ) )
			Msg("R_Shutdown() - wglDeleteContext failed\n");
		glw_state.hGLRC = NULL;
	}
	if (glw_state.hDC)
	{
		if ( !ReleaseDC( glw_state.hWnd, glw_state.hDC ) )
			Msg("R_Shutdown() - ReleaseDC failed\n" );
		glw_state.hDC   = NULL;
	}
	if (glw_state.hWnd)
	{
		DestroyWindow ( glw_state.hWnd );
		glw_state.hWnd = NULL;
	}

	if ( glw_state.log_fp )
	{
		fclose( glw_state.log_fp );
		glw_state.log_fp = 0;
	}

	UnregisterClass (WINDOW_CLASS_NAME, glw_state.hInstance);

	if ( gl_state.fullscreen )
	{
		ChangeDisplaySettings( 0, 0 );
		gl_state.fullscreen = false;
	}
}


/*
** GLimp_Init
**
** This routine is responsible for initializing the OS specific portions
** of OpenGL.  Under Win32 this means dealing with the pixelformats and
** doing the wgl interface stuff.
*/
bool GLimp_Init( void *hinstance, void *wndproc )
{
#define OSR2_BUILD_NUMBER 1111

	OSVERSIONINFO	vinfo;

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	glw_state.allowdisplaydepthchange = false;

	if ( GetVersionEx( &vinfo) )
	{
		if ( vinfo.dwMajorVersion > 4 )
		{
			glw_state.allowdisplaydepthchange = true;
		}
		else if ( vinfo.dwMajorVersion == 4 )
		{
			if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT )
			{
				glw_state.allowdisplaydepthchange = true;
			}
			else if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS )
			{
				if ( LOWORD( vinfo.dwBuildNumber ) >= OSR2_BUILD_NUMBER )
				{
					glw_state.allowdisplaydepthchange = true;
				}
			}
		}
	}
	else
	{
		Msg("GLimp_Init() - GetVersionEx failed\n" );
		return false;
	}

	glw_state.hInstance = ( HINSTANCE ) hinstance;
	glw_state.wndproc = wndproc;

	return true;
}

bool GLimp_InitGL (void)
{
    PIXELFORMATDESCRIPTOR pfd = 
	{
		sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
		1,				// version number
		PFD_DRAW_TO_WINDOW |		// support window
		PFD_SUPPORT_OPENGL |		// support OpenGL
		PFD_GENERIC_ACCELERATED |		// accelerated
		PFD_DOUBLEBUFFER,			// double buffered
		PFD_TYPE_RGBA,			// RGBA type
		24,				// 24-bit color depth
		0, 0, 0, 0, 0, 0,			// color bits ignored
		0,				// no alpha buffer
		0,				// shift bit ignored
		0,				// no accumulation buffer
		0, 0, 0, 0, 			// accum bits ignored
		32,				// 32-bit z-buffer	
		0,				// no stencil buffer
		0,				// no auxiliary buffer
		PFD_MAIN_PLANE,			// main layer
		0,				// reserved
		0, 0, 0				// layer masks ignored
    };
    int  pixelformat;
	cvar_t *stereo;
	
	stereo = ri.Cvar_Get( "cl_stereo", "0", 0 );

	/*
	** set PFD_STEREO if necessary
	*/
	if ( stereo->value != 0 )
	{
		Msg("...attempting to use stereo\n" );
		pfd.dwFlags |= PFD_STEREO;
		gl_state.stereo_enabled = true;
	}
	else
	{
		gl_state.stereo_enabled = false;
	}

	glw_state.minidriver = false;
	/*
	** Get a DC for the specified window
	*/
	if ( glw_state.hDC != NULL )
		Msg("GLimp_Init() - non-NULL DC exists\n" );

    	if ( ( glw_state.hDC = GetDC( glw_state.hWnd ) ) == NULL )
	{
		Msg("GLimp_Init() - GetDC failed\n" );
		return false;
	}

	if ( glw_state.minidriver )
	{
		if ( (pixelformat = qwglChoosePixelFormat( glw_state.hDC, &pfd)) == 0 )
		{
			Msg("GLimp_Init() - qwglChoosePixelFormat failed\n");
			return false;
		}
		if ( qwglSetPixelFormat( glw_state.hDC, pixelformat, &pfd) == FALSE )
		{
			Msg("GLimp_Init() - qwglSetPixelFormat failed\n");
			return false;
		}
		qwglDescribePixelFormat( glw_state.hDC, pixelformat, sizeof( pfd ), &pfd );
	}
	else
	{
		if ( ( pixelformat = ChoosePixelFormat( glw_state.hDC, &pfd)) == 0 )
		{
			Msg("GLimp_Init() - ChoosePixelFormat failed\n");
			return false;
		}
		if ( SetPixelFormat( glw_state.hDC, pixelformat, &pfd) == FALSE )
		{
			Msg("GLimp_Init() - SetPixelFormat failed\n");
			return false;
		}
		DescribePixelFormat( glw_state.hDC, pixelformat, sizeof( pfd ), &pfd );

		if ( !( pfd.dwFlags & PFD_GENERIC_ACCELERATED ) )
		{
			extern cvar_t *gl_allow_software;

			if ( gl_allow_software->value )
				glw_state.mcd_accelerated = true;
			else
				glw_state.mcd_accelerated = false;
		}
		else
		{
			glw_state.mcd_accelerated = true;
		}
	}

	/*
	** report if stereo is desired but unavailable
	*/
	if ( !( pfd.dwFlags & PFD_STEREO ) && ( stereo->value != 0 ) ) 
	{
		Msg("...failed to select stereo pixel format\n" );
		ri.Cvar_SetValue( "cl_stereo", 0 );
		gl_state.stereo_enabled = false;
	}

	/*
	** startup the OpenGL subsystem by creating a context and making
	** it current
	*/
	if ( ( glw_state.hGLRC = qwglCreateContext( glw_state.hDC ) ) == 0 )
	{
		Msg("GLimp_Init() - qwglCreateContext failed\n");
		goto fail;
	}

	if ( !qwglMakeCurrent( glw_state.hDC, glw_state.hGLRC ) )
	{
		Msg("GLimp_Init() - qwglMakeCurrent failed\n");
		goto fail;
	}

	if ( !VerifyDriver() )
	{
		Msg("GLimp_Init() - no hardware acceleration detected\n" );
		goto fail;
	}

	// Vertex arrays
	qglEnableClientState (GL_VERTEX_ARRAY);
	qglEnableClientState (GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer (2, GL_FLOAT, sizeof(tex_array[0]), tex_array[0]);
	qglVertexPointer (3, GL_FLOAT, sizeof(vert_array[0]),vert_array[0]);
	qglColorPointer (4, GL_FLOAT, sizeof(col_array[0]), col_array[0]);

	/*
	** print out PFD specifics 
	*/
	Msg("GL PFD: color(%d-bits) Z(%d-bit)\n", ( int ) pfd.cColorBits, ( int ) pfd.cDepthBits );
	return true;
fail:
	if ( glw_state.hGLRC )
	{
		qwglDeleteContext( glw_state.hGLRC );
		glw_state.hGLRC = NULL;
	}

	if ( glw_state.hDC )
	{
		ReleaseDC( glw_state.hWnd, glw_state.hDC );
		glw_state.hDC = NULL;
	}
	return false;
}

/*
** GLimp_BeginFrame
*/
void GLimp_BeginFrame( float camera_separation )
{
	if ( gl_bitdepth->modified )
	{
		if ( gl_bitdepth->value != 0 && !glw_state.allowdisplaydepthchange )
		{
			ri.Cvar_SetValue( "gl_bitdepth", 0 );
			Msg("gl_bitdepth requires Win95 OSR2.x or WinNT 4.x\n" );
		}
		gl_bitdepth->modified = false;
	}

	if ( camera_separation < 0 && gl_state.stereo_enabled )
	{
		qglDrawBuffer( GL_BACK_LEFT );
	}
	else if ( camera_separation > 0 && gl_state.stereo_enabled )
	{
		qglDrawBuffer( GL_BACK_RIGHT );
	}
	else
	{
		qglDrawBuffer( GL_BACK );
	}
}

/*
** GLimp_EndFrame
** 
** Responsible for doing a swapbuffers and possibly for other stuff
** as yet to be determined.  Probably better not to make this a GLimp
** function and instead do a call to GLimp_SwapBuffers.
*/
void GLimp_EndFrame (void)
{
	int err;

	err = qglGetError();
	assert( err == GL_NO_ERROR );

	if ( stricmp( gl_drawbuffer->string, "GL_BACK" ) == 0 )
	{
		if ( !qwglSwapBuffers( glw_state.hDC ) )
			Sys_Error("GLimp_EndFrame() - SwapBuffers() failed!\n" );
	}
}

/*
** GLimp_AppActivate
*/
void GLimp_AppActivate( bool active )
{
	if ( active )
	{
		SetForegroundWindow( glw_state.hWnd );
		ShowWindow( glw_state.hWnd, SW_RESTORE );
	}
	else
	{
		if ( vid_fullscreen->value )
			ShowWindow( glw_state.hWnd, SW_MINIMIZE );
	}
}
     