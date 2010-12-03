//=======================================================================
//			Copyright XashXT Group 2010 ©
//		        gl_vidnt.c - NT GL vid component
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"
#include "cm_local.h"
#include "input.h"

#define MAX_PFDS		256
#define VID_DEFAULTMODE	"3"
#define num_vidmodes	((int)(sizeof(vidmode) / sizeof(vidmode[0])) - 1)
#define WINDOW_STYLE	(WS_OVERLAPPED|WS_BORDER|WS_SYSMENU|WS_CAPTION|WS_VISIBLE)
#define WINDOW_EX_STYLE	(0)
#define GL_DRIVER_OPENGL	"OpenGL32"

convar_t	*gl_allow_software;
convar_t	*gl_extensions;
convar_t	*gl_colorbits;
convar_t	*gl_depthbits;
convar_t	*gl_stencilbits;
convar_t	*gl_texturebits;
convar_t	*gl_ignorehwgamma;
convar_t	*gl_texture_anisotropy;
convar_t	*gl_compress_textures;
convar_t	*gl_texture_lodbias;
convar_t	*gl_showtextures;
convar_t	*gl_swapInterval;
convar_t	*gl_check_errors;
convar_t	*gl_texturemode;
convar_t	*gl_round_down;
convar_t	*gl_picmip;
convar_t	*gl_skymip;
convar_t	*gl_nobind;
convar_t	*gl_finish;
convar_t	*gl_delayfinish;
convar_t	*gl_frontbuffer;
convar_t	*gl_clear;

convar_t	*r_width;
convar_t	*r_height;
convar_t	*r_speeds;
convar_t	*r_fullbright;

convar_t	*vid_displayfrequency;
convar_t	*vid_fullscreen;
convar_t	*vid_gamma;
convar_t	*vid_mode;

byte	*r_temppool;

ref_globals_t	tr;
glconfig_t	glConfig;
glstate_t		glState;
glwstate_t	glw_state;

typedef enum
{
	rserr_ok,
	rserr_invalid_fullscreen,
	rserr_invalid_mode,
	rserr_unknown
} rserr_t;

typedef struct vidmode_s
{
	const char	*desc;
	int		width; 
	int		height;
	qboolean		wideScreen;
} vidmode_t;

vidmode_t vidmode[] =
{
{ "Mode  0: 4x3",	400,	300,	false	},
{ "Mode  1: 4x3",	512,	384,	false	},
{ "Mode  2: 4x3",	640,	480,	false	},
{ "Mode  3: 4x3",	800,	600,	false	},
{ "Mode  4: 4x3",	960,	720,	false	},
{ "Mode  5: 4x3",	1024,	768,	false	},
{ "Mode  6: 4x3",	1152,	864,	false	},
{ "Mode  7: 4x3",	1280,	800,	false	},
{ "Mode  8: 4x3",	1280,	960,	false	},
{ "Mode  9: 4x3",	1280,	1024,	false	},
{ "Mode 10: 4x3",	1600,	1200,	false	},
{ "Mode 11: 4x3",	2048,	1536,	false	},
{ "Mode 12: 16x9",	856,	480,	true	},
{ "Mode 13: 16x9",	1024,	576,	true	},
{ "Mode 14: 16x9",	1280,	720,	true	},
{ "Mode 15: 16x9",	1360,	768,	true	},
{ "Mode 16: 16x9",	1440,	900,	true	},
{ "Mode 17: 16x9",	1680,	1050,	true	},
{ "Mode 18: 16x9",	1920,	1200,	true	},
{ "Mode 19: 16x9",	2560,	1600,	true	},
{ NULL,		0,	0,	0	},
};

static dllfunc_t opengl_110funcs[] =
{
{ "glClearColor"         , (void **)&pglClearColor },
{ "glClear"              , (void **)&pglClear },
{ "glAlphaFunc"          , (void **)&pglAlphaFunc },
{ "glBlendFunc"          , (void **)&pglBlendFunc },
{ "glCullFace"           , (void **)&pglCullFace },
{ "glDrawBuffer"         , (void **)&pglDrawBuffer },
{ "glReadBuffer"         , (void **)&pglReadBuffer },
{ "glEnable"             , (void **)&pglEnable },
{ "glDisable"            , (void **)&pglDisable },
{ "glEnableClientState"  , (void **)&pglEnableClientState },
{ "glDisableClientState" , (void **)&pglDisableClientState },
{ "glGetBooleanv"        , (void **)&pglGetBooleanv },
{ "glGetDoublev"         , (void **)&pglGetDoublev },
{ "glGetFloatv"          , (void **)&pglGetFloatv },
{ "glGetIntegerv"        , (void **)&pglGetIntegerv },
{ "glGetError"           , (void **)&pglGetError },
{ "glGetString"          , (void **)&pglGetString },
{ "glFinish"             , (void **)&pglFinish },
{ "glFlush"              , (void **)&pglFlush },
{ "glClearDepth"         , (void **)&pglClearDepth },
{ "glDepthFunc"          , (void **)&pglDepthFunc },
{ "glDepthMask"          , (void **)&pglDepthMask },
{ "glDepthRange"         , (void **)&pglDepthRange },
{ "glFrontFace"          , (void **)&pglFrontFace },
{ "glDrawElements"       , (void **)&pglDrawElements },
{ "glColorMask"          , (void **)&pglColorMask },
{ "glIndexPointer"       , (void **)&pglIndexPointer },
{ "glVertexPointer"      , (void **)&pglVertexPointer },
{ "glNormalPointer"      , (void **)&pglNormalPointer },
{ "glColorPointer"       , (void **)&pglColorPointer },
{ "glTexCoordPointer"    , (void **)&pglTexCoordPointer },
{ "glArrayElement"       , (void **)&pglArrayElement },
{ "glColor3f"            , (void **)&pglColor3f },
{ "glColor3fv"           , (void **)&pglColor3fv },
{ "glColor4f"            , (void **)&pglColor4f },
{ "glColor4fv"           , (void **)&pglColor4fv },
{ "glColor4ub"           , (void **)&pglColor4ub },
{ "glColor4ubv"          , (void **)&pglColor4ubv },
{ "glTexCoord1f"         , (void **)&pglTexCoord1f },
{ "glTexCoord2f"         , (void **)&pglTexCoord2f },
{ "glTexCoord3f"         , (void **)&pglTexCoord3f },
{ "glTexCoord4f"         , (void **)&pglTexCoord4f },
{ "glTexGenf"            , (void **)&pglTexGenf },
{ "glTexGenfv"           , (void **)&pglTexGenfv },
{ "glTexGeni"            , (void **)&pglTexGeni },
{ "glVertex2f"           , (void **)&pglVertex2f },
{ "glVertex3f"           , (void **)&pglVertex3f },
{ "glVertex3fv"          , (void **)&pglVertex3fv },
{ "glNormal3f"           , (void **)&pglNormal3f },
{ "glNormal3fv"          , (void **)&pglNormal3fv },
{ "glBegin"              , (void **)&pglBegin },
{ "glEnd"                , (void **)&pglEnd },
{ "glLineWidth"          , (void**)&pglLineWidth },
{ "glPointSize"          , (void**)&pglPointSize },
{ "glMatrixMode"         , (void **)&pglMatrixMode },
{ "glOrtho"              , (void **)&pglOrtho },
{ "glRasterPos2f"        , (void **) &pglRasterPos2f },
{ "glFrustum"            , (void **)&pglFrustum },
{ "glViewport"           , (void **)&pglViewport },
{ "glPushMatrix"         , (void **)&pglPushMatrix },
{ "glPopMatrix"          , (void **)&pglPopMatrix },
{ "glLoadIdentity"       , (void **)&pglLoadIdentity },
{ "glLoadMatrixd"        , (void **)&pglLoadMatrixd },
{ "glLoadMatrixf"        , (void **)&pglLoadMatrixf },
{ "glMultMatrixd"        , (void **)&pglMultMatrixd },
{ "glMultMatrixf"        , (void **)&pglMultMatrixf },
{ "glRotated"            , (void **)&pglRotated },
{ "glRotatef"            , (void **)&pglRotatef },
{ "glScaled"             , (void **)&pglScaled },
{ "glScalef"             , (void **)&pglScalef },
{ "glTranslated"         , (void **)&pglTranslated },
{ "glTranslatef"         , (void **)&pglTranslatef },
{ "glReadPixels"         , (void **)&pglReadPixels },
{ "glDrawPixels"         , (void **)&pglDrawPixels },
{ "glStencilFunc"        , (void **)&pglStencilFunc },
{ "glStencilMask"        , (void **)&pglStencilMask },
{ "glStencilOp"          , (void **)&pglStencilOp },
{ "glClearStencil"       , (void **)&pglClearStencil },
{ "glTexEnvf"            , (void **)&pglTexEnvf },
{ "glTexEnvfv"           , (void **)&pglTexEnvfv },
{ "glTexEnvi"            , (void **)&pglTexEnvi },
{ "glTexParameterf"      , (void **)&pglTexParameterf },
{ "glTexParameterfv"     , (void **)&pglTexParameterfv },
{ "glTexParameteri"      , (void **)&pglTexParameteri },
{ "glHint"               , (void **)&pglHint },
{ "glPixelStoref"        , (void **)&pglPixelStoref },
{ "glPixelStorei"        , (void **)&pglPixelStorei },
{ "glGenTextures"        , (void **)&pglGenTextures },
{ "glDeleteTextures"     , (void **)&pglDeleteTextures },
{ "glBindTexture"        , (void **)&pglBindTexture },
{ "glTexImage1D"         , (void **)&pglTexImage1D },
{ "glTexImage2D"         , (void **)&pglTexImage2D },
{ "glTexSubImage1D"      , (void **)&pglTexSubImage1D },
{ "glTexSubImage2D"      , (void **)&pglTexSubImage2D },
{ "glCopyTexImage1D"     , (void **)&pglCopyTexImage1D },
{ "glCopyTexImage2D"     , (void **)&pglCopyTexImage2D },
{ "glCopyTexSubImage1D"  , (void **)&pglCopyTexSubImage1D },
{ "glCopyTexSubImage2D"  , (void **)&pglCopyTexSubImage2D },
{ "glScissor"            , (void **) &pglScissor },
{ "glPolygonOffset"      , (void **)&pglPolygonOffset },
{ "glPolygonMode"        , (void **)&pglPolygonMode },
{ "glPolygonStipple"     , (void **)&pglPolygonStipple },
{ "glClipPlane"          , (void **)&pglClipPlane },
{ "glGetClipPlane"       , (void **)&pglGetClipPlane },
{ "glShadeModel"         , (void **)&pglShadeModel },
{ "glFogfv"              , (void **)&pglFogfv },
{ "glFogf"               , (void **)&pglFogf },
{ "glFogi"               , (void **)&pglFogi },
{ NULL, NULL }
};

static dllfunc_t pointparametersfunc[] =
{
{ "glPointParameterfEXT"  , (void **)&pglPointParameterfEXT },
{ "glPointParameterfvEXT" , (void **)&pglPointParameterfvEXT },
{ NULL, NULL }
};

static dllfunc_t drawrangeelementsfuncs[] =
{
{ "glDrawRangeElements" , (void **)&pglDrawRangeElements },
{ NULL, NULL }
};

static dllfunc_t drawrangeelementsextfuncs[] =
{
{ "glDrawRangeElementsEXT" , (void **)&pglDrawRangeElementsEXT },
{ NULL, NULL }
};

static dllfunc_t sgis_multitexturefuncs[] =
{
{ "glSelectTextureSGIS" , (void **)&pglSelectTextureSGIS },
{ "glMTexCoord2fSGIS"   , (void **)&pglMTexCoord2fSGIS },
{ NULL, NULL }
};

static dllfunc_t multitexturefuncs[] =
{
{ "glMultiTexCoord1fARB"     , (void **)&pglMultiTexCoord1f },
{ "glMultiTexCoord2fARB"     , (void **)&pglMultiTexCoord2f },
{ "glMultiTexCoord3fARB"     , (void **)&pglMultiTexCoord3f },
{ "glMultiTexCoord4fARB"     , (void **)&pglMultiTexCoord4f },
{ "glActiveTextureARB"       , (void **)&pglActiveTextureARB },
{ "glClientActiveTextureARB" , (void **)&pglClientActiveTexture },
{ "glClientActiveTextureARB" , (void **)&pglClientActiveTextureARB },
{ NULL, NULL }
};

static dllfunc_t compiledvertexarrayfuncs[] =
{
{ "glLockArraysEXT"   , (void **)&pglLockArraysEXT },
{ "glUnlockArraysEXT" , (void **)&pglUnlockArraysEXT },
{ "glDrawArrays"      , (void **)&pglDrawArrays },
{ NULL, NULL }
};

static dllfunc_t texture3dextfuncs[] =
{
{ "glTexImage3DEXT"        , (void **)&pglTexImage3D },
{ "glTexSubImage3DEXT"     , (void **)&pglTexSubImage3D },
{ "glCopyTexSubImage3DEXT" , (void **)&pglCopyTexSubImage3D },
{ NULL, NULL }
};

static dllfunc_t atiseparatestencilfuncs[] =
{
{ "glStencilOpSeparateATI"   , (void **)&pglStencilOpSeparate },
{ "glStencilFuncSeparateATI" , (void **)&pglStencilFuncSeparate },
{ NULL, NULL }
};

static dllfunc_t gl2separatestencilfuncs[] =
{
{ "glStencilOpSeparate"   , (void **)&pglStencilOpSeparate },
{ "glStencilFuncSeparate" , (void **)&pglStencilFuncSeparate },
{ NULL, NULL }
};

static dllfunc_t stenciltwosidefuncs[] =
{
{ "glActiveStencilFaceEXT" , (void **)&pglActiveStencilFaceEXT },
{ NULL, NULL }
};

static dllfunc_t blendequationfuncs[] =
{
{ "glBlendEquationEXT" , (void **)&pglBlendEquationEXT },
{ NULL, NULL }
};

static dllfunc_t shaderobjectsfuncs[] =
{
{ "glDeleteObjectARB"             , (void **)&pglDeleteObjectARB },
{ "glGetHandleARB"                , (void **)&pglGetHandleARB },
{ "glDetachObjectARB"             , (void **)&pglDetachObjectARB },
{ "glCreateShaderObjectARB"       , (void **)&pglCreateShaderObjectARB },
{ "glShaderSourceARB"             , (void **)&pglShaderSourceARB },
{ "glCompileShaderARB"            , (void **)&pglCompileShaderARB },
{ "glCreateProgramObjectARB"      , (void **)&pglCreateProgramObjectARB },
{ "glAttachObjectARB"             , (void **)&pglAttachObjectARB },
{ "glLinkProgramARB"              , (void **)&pglLinkProgramARB },
{ "glUseProgramObjectARB"         , (void **)&pglUseProgramObjectARB },
{ "glValidateProgramARB"          , (void **)&pglValidateProgramARB },
{ "glUniform1fARB"                , (void **)&pglUniform1fARB },
{ "glUniform2fARB"                , (void **)&pglUniform2fARB },
{ "glUniform3fARB"                , (void **)&pglUniform3fARB },
{ "glUniform4fARB"                , (void **)&pglUniform4fARB },
{ "glUniform1iARB"                , (void **)&pglUniform1iARB },
{ "glUniform2iARB"                , (void **)&pglUniform2iARB },
{ "glUniform3iARB"                , (void **)&pglUniform3iARB },
{ "glUniform4iARB"                , (void **)&pglUniform4iARB },
{ "glUniform1fvARB"               , (void **)&pglUniform1fvARB },
{ "glUniform2fvARB"               , (void **)&pglUniform2fvARB },
{ "glUniform3fvARB"               , (void **)&pglUniform3fvARB },
{ "glUniform4fvARB"               , (void **)&pglUniform4fvARB },
{ "glUniform1ivARB"               , (void **)&pglUniform1ivARB },
{ "glUniform2ivARB"               , (void **)&pglUniform2ivARB },
{ "glUniform3ivARB"               , (void **)&pglUniform3ivARB },
{ "glUniform4ivARB"               , (void **)&pglUniform4ivARB },
{ "glUniformMatrix2fvARB"         , (void **)&pglUniformMatrix2fvARB },
{ "glUniformMatrix3fvARB"         , (void **)&pglUniformMatrix3fvARB },
{ "glUniformMatrix4fvARB"         , (void **)&pglUniformMatrix4fvARB },
{ "glGetObjectParameterfvARB"     , (void **)&pglGetObjectParameterfvARB },
{ "glGetObjectParameterivARB"     , (void **)&pglGetObjectParameterivARB },
{ "glGetInfoLogARB"               , (void **)&pglGetInfoLogARB },
{ "glGetAttachedObjectsARB"       , (void **)&pglGetAttachedObjectsARB },
{ "glGetUniformLocationARB"       , (void **)&pglGetUniformLocationARB },
{ "glGetActiveUniformARB"         , (void **) &pglGetActiveUniformARB },
{ "glGetUniformfvARB"             , (void **)&pglGetUniformfvARB },
{ "glGetUniformivARB"             , (void **)&pglGetUniformivARB },
{ "glGetShaderSourceARB"          , (void **)&pglGetShaderSourceARB },
{ "glVertexAttribPointerARB"      , (void **)&pglVertexAttribPointerARB },
{ "glEnableVertexAttribArrayARB"  , (void **)&pglEnableVertexAttribArrayARB },
{ "glDisableVertexAttribArrayARB" , (void **)&pglDisableVertexAttribArrayARB },
{ "glBindAttribLocationARB"       , (void **)&pglBindAttribLocationARB },
{ "glGetActiveAttribARB"          , (void **)&pglGetActiveAttribARB },
{ "glGetAttribLocationARB"        , (void **)&pglGetAttribLocationARB },
{ NULL, NULL }
};

static dllfunc_t vertexshaderfuncs[] =
{
{ "glVertexAttribPointerARB"      , (void **)&pglVertexAttribPointerARB },
{ "glEnableVertexAttribArrayARB"  , (void **)&pglEnableVertexAttribArrayARB },
{ "glDisableVertexAttribArrayARB" , (void **)&pglDisableVertexAttribArrayARB },
{ "glBindAttribLocationARB"       , (void **)&pglBindAttribLocationARB },
{ "glGetActiveAttribARB"          , (void **)&pglGetActiveAttribARB },
{ "glGetAttribLocationARB"        , (void **)&pglGetAttribLocationARB },
{ NULL, NULL }
};

static dllfunc_t vbofuncs[] =
{
{ "glBindBufferARB"    , (void **)&pglBindBufferARB },
{ "glDeleteBuffersARB" , (void **)&pglDeleteBuffersARB },
{ "glGenBuffersARB"    , (void **)&pglGenBuffersARB },
{ "glIsBufferARB"      , (void **)&pglIsBufferARB },
{ "glMapBufferARB"     , (void **)&pglMapBufferARB },
{ "glUnmapBufferARB"   , (void **)&pglUnmapBufferARB },
{ "glBufferDataARB"    , (void **)&pglBufferDataARB },
{ "glBufferSubDataARB" , (void **)&pglBufferSubDataARB },
{ NULL, NULL}
};

static dllfunc_t occlusionfunc[] =
{
{ "glGenQueriesARB"        , (void **)&pglGenQueriesARB },
{ "glDeleteQueriesARB"     , (void **)&pglDeleteQueriesARB },
{ "glIsQueryARB"           , (void **)&pglIsQueryARB },
{ "glBeginQueryARB"        , (void **)&pglBeginQueryARB },
{ "glEndQueryARB"          , (void **)&pglEndQueryARB },
{ "glGetQueryivARB"        , (void **)&pglGetQueryivARB },
{ "glGetQueryObjectivARB"  , (void **)&pglGetQueryObjectivARB },
{ "glGetQueryObjectuivARB" , (void **)&pglGetQueryObjectuivARB },
{ NULL, NULL }
};

static dllfunc_t texturecompressionfuncs[] =
{
{ "glCompressedTexImage3DARB"    , (void **)&pglCompressedTexImage3DARB },
{ "glCompressedTexImage2DARB"    , (void **)&pglCompressedTexImage2DARB },
{ "glCompressedTexImage1DARB"    , (void **)&pglCompressedTexImage1DARB },
{ "glCompressedTexSubImage3DARB" , (void **)&pglCompressedTexSubImage3DARB },
{ "glCompressedTexSubImage2DARB" , (void **)&pglCompressedTexSubImage2DARB },
{ "glCompressedTexSubImage1DARB" , (void **)&pglCompressedTexSubImage1DARB },
{ "glGetCompressedTexImageARB"   , (void **)&pglGetCompressedTexImage },
{ NULL, NULL }
};

static dllfunc_t wgl_funcs[] =
{
{ "wglChoosePixelFormat"   , (void **)&pwglChoosePixelFormat },
{ "wglDescribePixelFormat" , (void **)&pwglDescribePixelFormat },
{ "wglSetPixelFormat"      , (void **)&pwglSetPixelFormat },
{ "wglSwapBuffers"         , (void **)&pwglSwapBuffers },
{ "wglCreateContext"       , (void **)&pwglCreateContext },
{ "wglDeleteContext"       , (void **)&pwglDeleteContext },
{ "wglGetProcAddress"      , (void **)&pwglGetProcAddress },
{ "wglMakeCurrent"         , (void **)&pwglMakeCurrent },
{ "wglGetCurrentContext"   , (void **)&pwglGetCurrentContext },
{ "wglGetCurrentDC"        , (void **)&pwglGetCurrentDC },
{ NULL, NULL }
};

static dllfunc_t wglswapintervalfuncs[] =
{
{ "wglSwapIntervalEXT" , (void **)&pwglSwapIntervalEXT },
{ NULL, NULL }
};

static dllfunc_t wgl3DFXgammacontrolfuncs[] =
{
{ "wglGetDeviceGammaRamp3DFX" , (void **)&pwglGetDeviceGammaRamp3DFX },
{ "wglSetDeviceGammaRamp3DFX" , (void **)&pwglSetDeviceGammaRamp3DFX },
{ NULL, NULL }
};

dll_info_t opengl_dll = { "opengl32.dll", wgl_funcs, NULL, NULL, NULL, true };

/*
=================
GL_SetExtension
=================
*/
void GL_SetExtension( int r_ext, int enable )
{
	if( r_ext >= 0 && r_ext < GL_EXTCOUNT )
		glConfig.extension[r_ext] = enable ? GL_TRUE : GL_FALSE;
	else MsgDev( D_ERROR, "GL_SetExtension: invalid extension %d\n", r_ext );
}

/*
=================
GL_Support
=================
*/
qboolean GL_Support( int r_ext )
{
	if( r_ext >= 0 && r_ext < GL_EXTCOUNT )
		return glConfig.extension[r_ext] ? true : false;
	MsgDev( D_ERROR, "GL_Support: invalid extension %d\n", r_ext );
	return false;		
}

/*
=================
GL_GetProcAddress
=================
*/
void *GL_GetProcAddress( const char *name )
{
	void	*p = NULL;

	if( pwglGetProcAddress != NULL )
		p = (void *)pwglGetProcAddress( name );
	if( !p ) p = (void *)Sys_GetProcAddress( &opengl_dll, name );

	return p;
}

/*
=================
GL_CheckExtension
=================
*/
void GL_CheckExtension( const char *name, const dllfunc_t *funcs, const char *cvarname, int r_ext )
{
	const dllfunc_t	*func;
	convar_t		*parm;

	MsgDev( D_NOTE, "GL_CheckExtension: %s ", name );

	if( gl_extensions->integer == 0 && r_ext != GL_OPENGL_110 )
	{
		GL_SetExtension( r_ext, 0 );	// update render info
		return;
	}

	if( cvarname )
	{
		// system config disable extensions
		parm = Cvar_Get( cvarname, "1", CVAR_RENDERINFO, va( "enable or disable %s", name ));
		GL_SetExtension( r_ext, parm->integer );	// update render info
		if( parm->integer == 0 )
		{
			MsgDev( D_NOTE, "- disabled\n");
			return; // nothing to process at
		}
	}

	if(( name[2] == '_' || name[3] == '_' ) && !com.strstr( glConfig.extensions_string, name ))
	{
		GL_SetExtension( r_ext, false );	// update render info
		MsgDev( D_NOTE, "- failed\n");
		return;
	}

	// clear exports
	for( func = funcs; func && func->name; func++ )
		*func->func = NULL;

	GL_SetExtension( r_ext, true ); // predict extension state
	for( func = funcs; func && func->name != NULL; func++ )
	{
		// functions are cleared before all the extensions are evaluated
		if(!(*func->func = (void *)GL_GetProcAddress( func->name )))
			GL_SetExtension( r_ext, false ); // one or more functions are invalid, extension will be disabled
	}

	if( GL_Support( r_ext ))
		MsgDev( D_NOTE, "- enabled\n");
}

/*
===============
GL_BuildGammaTable
===============
*/
void GL_BuildGammaTable( void )
{
	int	i, v;
	double	invGamma, div;

	invGamma = 1.0 / bound( 0.5, vid_gamma->value, 3.0 );
	div = (double) 1.0 / 255.5;

	Mem_Copy( glState.gammaRamp, glState.stateRamp, sizeof( glState.gammaRamp ));
	
	for( i = 0; i < 256; i++ )
	{
		v = (int)(65535.0 * pow(((double)i + 0.5 ) * div, invGamma ) + 0.5 );
		glState.gammaRamp[i+0]   = ((word)bound( 0, v, 65535 ));
		glState.gammaRamp[i+256] = ((word)bound( 0, v, 65535 ));
		glState.gammaRamp[i+512] = ((word)bound( 0, v, 65535 ));
	}
}

/*
===============
GL_UpdateGammaRamp
===============
*/
void GL_UpdateGammaRamp( void )
{
	if( gl_ignorehwgamma->integer ) return;
	if( !glConfig.deviceSupportsGamma ) return;

	GL_BuildGammaTable();

	if( pwglGetDeviceGammaRamp3DFX )
		pwglSetDeviceGammaRamp3DFX( glw_state.hDC, glState.gammaRamp );
	else SetDeviceGammaRamp( glw_state.hDC, glState.gammaRamp );
}

/*
===============
GL_UpdateSwapInterval
===============
*/
void GL_UpdateSwapInterval( void )
{
	if( gl_swapInterval->modified )
	{
		gl_swapInterval->modified = false;

		if( pwglSwapIntervalEXT )
			pwglSwapIntervalEXT( gl_swapInterval->integer );
	}
}

/*
===============
GL_SetDefaultTexState
===============
*/
static void GL_SetDefaultTexState( void )
{
	Mem_Set( glState.currentTextures, -1, MAX_TEXTURE_UNITS * sizeof( *glState.currentTextures ));
	Mem_Set( glState.currentEnvModes, -1, MAX_TEXTURE_UNITS * sizeof( *glState.currentEnvModes ));
	Mem_Set( glState.texIdentityMatrix, 0, MAX_TEXTURE_UNITS * sizeof( *glState.texIdentityMatrix ));
	Mem_Set( glState.genSTEnabled, 0, MAX_TEXTURE_UNITS * sizeof( *glState.genSTEnabled ));
	Mem_Set( glState.texCoordArrayMode, 0, MAX_TEXTURE_UNITS * sizeof( *glState.texCoordArrayMode ));
}

/*
===============
GL_SetDefaultState
===============
*/
static void GL_SetDefaultState( void )
{
	Mem_Set( &glState, 0, sizeof( glState ));
	GL_SetDefaultTexState ();

	glState.initializedMedia = false;
}

/*
=================
GL_DeleteContext
=================
*/
qboolean GL_DeleteContext( void )
{
	if( glw_state.hGLRC )
	{
		pwglDeleteContext( glw_state.hGLRC );
		glw_state.hGLRC = NULL;
	}

	if( glw_state.hDC )
	{
		ReleaseDC( host.hWnd, glw_state.hDC );
		glw_state.hDC = NULL;
	}

	return false;
}

/*
=================
GL_ChoosePFD
=================
*/
static int GL_ChoosePFD( int colorBits, int depthBits, int stencilBits )
{
	PIXELFORMATDESCRIPTOR	PFDs[MAX_PFDS], *current, *selected;
	uint			flags = PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
	int			i, numPFDs, pixelFormat = 0;

	MsgDev( D_NOTE, "GL_ChoosePFD( color %i, depth %i, stencil %i )\n", colorBits, depthBits, stencilBits );

	// Count PFDs
	if( glw_state.minidriver )
		numPFDs = pwglDescribePixelFormat( glw_state.hDC, 0, 0, NULL );
	else numPFDs = DescribePixelFormat( glw_state.hDC, 0, 0, NULL );

	if( numPFDs > MAX_PFDS )
	{
		MsgDev( D_NOTE, "too many PFDs returned (%i > %i), reduce it\n", numPFDs, MAX_PFDS );
		numPFDs = MAX_PFDS;
	}
	else if( numPFDs < 1 )
	{
		MsgDev( D_ERROR, "R_ChoosePFD failed\n" );
		return 0;
	}

	// run through all the PFDs, looking for the best match
	for( i = 1, current = PFDs; i <= numPFDs; i++, current++ )
	{
		if( glw_state.minidriver )
			pwglDescribePixelFormat( glw_state.hDC, i, sizeof( PIXELFORMATDESCRIPTOR ), current );
		else DescribePixelFormat( glw_state.hDC, i, sizeof( PIXELFORMATDESCRIPTOR ), current );

		// check acceleration
		if(( current->dwFlags & PFD_GENERIC_FORMAT ) && !gl_allow_software->integer )
			continue;

		// check flags
		if(( current->dwFlags & flags ) != flags )
			continue;

		// check pixel type
		if( current->iPixelType != PFD_TYPE_RGBA )
			continue;

		// check color bits
		if( current->cColorBits < colorBits )
			continue;

		// check depth bits
		if( current->cDepthBits < depthBits )
			continue;

		// check stencil bits
		if( current->cStencilBits < stencilBits )
			continue;

		// if we don't have a selected PFD yet, then use it
		if( !pixelFormat )
		{
			selected = current;
			pixelFormat = i;
			continue;
		}

		if( colorBits != selected->cColorBits )
		{
			if( colorBits == current->cColorBits || current->cColorBits > selected->cColorBits )
			{
				selected = current;
				pixelFormat = i;
				continue;
			}
		}

		if( depthBits != selected->cDepthBits )
		{
			if( depthBits == current->cDepthBits || current->cDepthBits > selected->cDepthBits )
			{
				selected = current;
				pixelFormat = i;
				continue;
			}
		}

		if( stencilBits != selected->cStencilBits )
		{
			if( stencilBits == current->cStencilBits || current->cStencilBits > selected->cStencilBits )
			{
				selected = current;
				pixelFormat = i;
				continue;
			}
		}
	}

	if( !pixelFormat )
	{
		MsgDev( D_ERROR, "GL_ChoosePFD: no hardware acceleration found\n" );
		return 0;
	}

	if( selected->dwFlags & PFD_GENERIC_FORMAT )
	{
		if( selected->dwFlags & PFD_GENERIC_ACCELERATED )
		{
			MsgDev( D_NOTE, "R_ChoosePFD: usign Generic MCD acceleration\n" );
			glw_state.software = false;
		}
		else
		{
			MsgDev( D_NOTE, "R_ChoosePFD: using software emulation\n" );
			glw_state.software = true;
		}
	}
	else
	{
		MsgDev( D_NOTE, "R_ChoosePFD: using hardware acceleration\n");
		glw_state.software = false;
	}

	return pixelFormat;
}

/*
=================
GL_SetPixelformat
=================
*/
qboolean GL_SetPixelformat( void )
{
	PIXELFORMATDESCRIPTOR	PFD;
	int			colorBits, depthBits, stencilBits;
	int			pixelFormat;
	size_t			gamma_size;
	byte			*savedGamma;

	glw_state.minidriver = false;	// FIXME: allow 3dfx drivers too

	if( glw_state.minidriver )
	{
    		if(( glw_state.hDC = pwglGetCurrentDC()) == NULL )
			return false;
	}
	else
	{
    		if(( glw_state.hDC = GetDC( host.hWnd )) == NULL )
			return false;
	}

	// set color/depth/stencil
	colorBits = (gl_colorbits->integer) ? gl_colorbits->integer : 32;
	depthBits = (gl_depthbits->integer) ? gl_depthbits->integer : 24;
	stencilBits = (gl_stencilbits->integer) ? gl_stencilbits->integer : 0;

	// choose a pixel format
	pixelFormat = GL_ChoosePFD( colorBits, depthBits, stencilBits );

	if( !pixelFormat )
	{
		// try again with default color/depth/stencil
		if( colorBits > 16 || depthBits > 16 || stencilBits > 0 )
			pixelFormat = GL_ChoosePFD( 16, 16, 0 );
		else pixelFormat = GL_ChoosePFD( 32, 24, 0 );

		if( !pixelFormat )
		{
			MsgDev( D_ERROR, "GL_SetPixelformat: failed to find an appropriate PIXELFORMAT\n" );
			ReleaseDC( host.hWnd, glw_state.hDC );
			glw_state.hDC = NULL;
			return false;
		}
	}

	// set the pixel format
	if( glw_state.minidriver )
	{
		pwglDescribePixelFormat( glw_state.hDC, pixelFormat, sizeof( PIXELFORMATDESCRIPTOR ), &PFD );

		if( !pwglSetPixelFormat( glw_state.hDC, pixelFormat, &PFD ))
		{
			MsgDev( D_ERROR, "GL_SetPixelformat: failed\n" );
			return GL_DeleteContext();
		}
	}
	else
	{
		DescribePixelFormat( glw_state.hDC, pixelFormat, sizeof( PIXELFORMATDESCRIPTOR ), &PFD );

		if( !SetPixelFormat( glw_state.hDC, pixelFormat, &PFD ))
		{
			MsgDev( D_ERROR, "GL_SetPixelformat: failed\n" );
			return GL_DeleteContext();
		}
	}

	glConfig.color_bits = PFD.cColorBits;
	glConfig.depth_bits = PFD.cDepthBits;
	glConfig.stencil_bits = PFD.cStencilBits;

	if(!( glw_state.hGLRC = pwglCreateContext( glw_state.hDC )))
		return GL_DeleteContext();
	if(!( pwglMakeCurrent( glw_state.hDC, glw_state.hGLRC )))
		return GL_DeleteContext();

	if( PFD.cStencilBits != 0 )
		glState.stencilEnabled = true;
	else glState.stencilEnabled = false;

	// print out PFD specifics 
	MsgDev( D_NOTE, "GL PFD: color( %d-bits ) Z( %d-bit )\n", PFD.cColorBits, PFD.cDepthBits );

	// init gamma ramp
	Mem_Set( glState.stateRamp, 0, sizeof( glState.stateRamp ));

	if( pwglGetDeviceGammaRamp3DFX )
		glConfig.deviceSupportsGamma = pwglGetDeviceGammaRamp3DFX( glw_state.hDC, glState.stateRamp );
	else glConfig.deviceSupportsGamma = GetDeviceGammaRamp( glw_state.hDC, glState.stateRamp );

	// share this extension so engine can grab them
	GL_SetExtension( GL_HARDWARE_GAMMA_CONTROL, glConfig.deviceSupportsGamma );

	savedGamma = FS_LoadFile( "gamma.dat", &gamma_size );
	if( !savedGamma || gamma_size != sizeof( glState.stateRamp ))
	{
		// saved gamma not found or corrupted file
		FS_WriteFile( "gamma.dat", glState.stateRamp, sizeof(glState.stateRamp));
		MsgDev( D_NOTE, "gamma.dat initialized\n" );
		if( savedGamma ) Mem_Free( savedGamma );
	}
	else
	{
		GL_BuildGammaTable();

		// validate base gamma
		if( !memcmp( savedGamma, glState.stateRamp, sizeof( glState.stateRamp )))
		{
			// all ok, previous gamma is valid
			MsgDev( D_NOTE, "GL_SetPixelformat: validate screen gamma - ok\n" );
		}
		else if( !memcmp( glState.gammaRamp, glState.stateRamp, sizeof( glState.stateRamp )))
		{
			// screen gamma is equal to render gamma (probably previous instance crashed)
			// run additional check to make sure it
			if( memcmp( savedGamma, glState.stateRamp, sizeof( glState.stateRamp )))
			{
				// yes, current gamma it's totally wrong, restore it from gamma.dat
				MsgDev( D_NOTE, "GL_SetPixelformat: restore original gamma after crash\n" );
				Mem_Copy( glState.stateRamp, savedGamma, sizeof( glState.gammaRamp ));
			}
			else
			{
				// oops, savedGamma == glState.stateRamp == glState.gammaRamp
				// probably r_gamma set as default
				MsgDev( D_NOTE, "GL_SetPixelformat: validate screen gamma - disabled\n" ); 
			}
		}
		else if( !memcmp( glState.gammaRamp, savedGamma, sizeof( glState.stateRamp )))
		{
			// saved gamma is equal render gamma, probably gamma.dat writed after crash
			// run additional check to make sure it
			if( memcmp( savedGamma, glState.stateRamp, sizeof( glState.stateRamp )))
			{
				// yes, saved gamma it's totally wrong, get origianl gamma from screen
				MsgDev( D_NOTE, "GL_SetPixelformat: merge gamma.dat after crash\n" );
				FS_WriteFile( "gamma.dat", glState.stateRamp, sizeof( glState.stateRamp ));
			}
			else
			{
				// oops, savedGamma == glState.stateRamp == glState.gammaRamp
				// probably r_gamma set as default
				MsgDev( D_NOTE, "GL_SetPixelformat: validate screen gamma - disabled\n" ); 
			}
		}
		else
		{
			// current gamma unset by other application, we can restore it here
			MsgDev( D_NOTE, "GL_SetPixelformat: restore original gamma after crash\n" );
			Mem_Copy( glState.stateRamp, savedGamma, sizeof( glState.gammaRamp ));			
		}
		Mem_Free( savedGamma );
	}
	vid_gamma->modified = true;

	return true;
}

void VID_RestoreGamma( void )
{
	if( !glw_state.hDC ) return;
	SetDeviceGammaRamp( glw_state.hDC, glState.stateRamp );
}

void R_Free_OpenGL( void )
{
	VID_RestoreGamma ();

	if( pwglMakeCurrent )
		pwglMakeCurrent( NULL, NULL );

	if( glw_state.hGLRC )
	{
		if( pwglDeleteContext ) pwglDeleteContext( glw_state.hGLRC );
		glw_state.hGLRC = NULL;
	}

	if( glw_state.hDC )
	{
		ReleaseDC( host.hWnd, glw_state.hDC );
		glw_state.hDC   = NULL;
	}

	if( host.hWnd )
	{
		DestroyWindow ( host.hWnd );
		host.hWnd = NULL;
	}

	UnregisterClass( "Xash Window", host.hInst );

	if( glState.fullScreen )
	{
		ChangeDisplaySettings( 0, 0 );
		glState.fullScreen = false;
	}

	Sys_FreeLibrary( &opengl_dll );

	// now all extensions are disabled
	Mem_Set( glConfig.extension, 0, sizeof( glConfig.extension[0] ) * GL_EXTCOUNT );
	glw_state.initialized = false;
}

void R_SaveVideoMode( int vid_mode )
{
	int	mode = bound( 0, vid_mode, num_vidmodes ); // check range

	glState.width = vidmode[mode].width;
	glState.height = vidmode[mode].height;
	glState.wideScreen = vidmode[mode].wideScreen;

	Cvar_FullSet( "width", va( "%i", vidmode[mode].width ), CVAR_READ_ONLY );
	Cvar_FullSet( "height", va( "%i", vidmode[mode].height ), CVAR_READ_ONLY );
	Cvar_SetFloat( "r_mode", mode ); // merge if it out of bounds

	MsgDev( D_NOTE, "Set: %s [%dx%d]\n", vidmode[mode].desc, vidmode[mode].width, vidmode[mode].height );
}

qboolean VID_CreateWindow( int width, int height, qboolean fullscreen )
{
	WNDCLASS		wc;
	RECT		rect;
	convar_t		*r_xpos, *r_ypos;
	int		x = 0, y = 0, w, h;
	int		stylebits = WINDOW_STYLE;
	int		exstyle = WINDOW_EX_STYLE;
	static string	wndname;
	
	com.snprintf( wndname, sizeof( wndname ), "%s", GI->title );

	// register the frame class
	wc.style         = CS_OWNDC|CS_NOCLOSE;
	wc.lpfnWndProc   = (WNDPROC)IN_WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = host.hInst;
	wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (void *)COLOR_3DSHADOW;
	wc.lpszClassName = "Xash Window";
	wc.lpszMenuName  = 0;

	// find the icon file in the filesystem
	if( FS_FileExistsEx( "game.ico", true ))
	{
		char	localPath[MAX_PATH];

		com.snprintf( localPath, sizeof( localPath ), "%s/game.ico", GI->gamedir );
		wc.hIcon = LoadImage( NULL, localPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE|LR_DEFAULTSIZE );
		if( !wc.hIcon )
		{
			MsgDev( D_INFO, "Extract game.ico from pak if you want to see it.\n" );
			wc.hIcon = LoadIcon( host.hInst, MAKEINTRESOURCE( 101 ));
		}
	}
	else wc.hIcon = LoadIcon( host.hInst, MAKEINTRESOURCE( 101 ));

	if( !RegisterClass( &wc ))
	{ 
		MsgDev( D_ERROR, "VID_CreateWindow: couldn't register window class %s\n" "Xash Window" );
		return false;
	}

	if( fullscreen )
	{
		stylebits = WS_POPUP|WS_VISIBLE;
		exstyle = WS_EX_TOPMOST;
	}

	rect.left = 0;
	rect.top = 0;
	rect.right  = width;
	rect.bottom = height;

	AdjustWindowRect( &rect, stylebits, FALSE );
	w = rect.right - rect.left;
	h = rect.bottom - rect.top;

	if( !fullscreen )
	{
		r_xpos = Cvar_Get( "r_xpos", "130", CVAR_RENDERINFO, "window position by horizontal" );
		r_ypos = Cvar_Get( "r_ypos", "48", CVAR_RENDERINFO, "window position by vertical" );
		x = r_xpos->integer;
		y = r_ypos->integer;

		// adjust window coordinates if necessary 
		// so that the window is completely on screen
		if( x < 0 ) x = 0;
		if( y < 0 ) y = 0;

		if( Cvar_VariableInteger( "vid_mode" ) != glConfig.prev_mode )
		{
			if(( x + w > glw_state.desktopWidth ) || ( y + h > glw_state.desktopHeight ))
			{
				x = ( glw_state.desktopWidth - w ) / 2;
				y = ( glw_state.desktopHeight - h ) / 2;
			}
		}
	}

	CreateWindowEx( exstyle, "Xash Window", wndname, stylebits, x, y, w, h, NULL, NULL, host.hInst, NULL );

	// host.hWnd will be filled in IN_WndProc

	if( !host.hWnd ) 
	{
		MsgDev( D_ERROR, "VID_CreateWindow: couldn't create '%s'\n", wndname );
		return false;
	}
	
	ShowWindow( host.hWnd, SW_SHOW );
	UpdateWindow( host.hWnd );

	// init all the gl stuff for the window
	if( !GL_SetPixelformat( ))
	{
		ShowWindow( host.hWnd, SW_HIDE );
		DestroyWindow( host.hWnd );
		host.hWnd = NULL;

		UnregisterClass( "Xash Window", host.hInst );
		MsgDev( D_ERROR, "OpenGL driver not installed\n" );
		return false;
	}

	SetForegroundWindow( host.hWnd );
	SetFocus( host.hWnd );

	return true;
}

rserr_t R_ChangeDisplaySettings( int vid_mode, qboolean fullscreen )
{
	int	width, height;
	HDC	hDC;
	
	R_SaveVideoMode( vid_mode );

	width = r_width->integer;
	height = r_height->integer;

	// check our desktop attributes
	hDC = GetDC( GetDesktopWindow( ));
	glw_state.desktopBitsPixel = GetDeviceCaps( hDC, BITSPIXEL );
	glw_state.desktopWidth = GetDeviceCaps( hDC, HORZRES );
	glw_state.desktopHeight = GetDeviceCaps( hDC, VERTRES );
	ReleaseDC( GetDesktopWindow(), hDC );

	// destroy the existing window
	if( host.hWnd ) R_Free_OpenGL();

	// do a CDS if needed
	if( fullscreen )
	{
		DEVMODE	dm;

		Mem_Set( &dm, 0, sizeof( dm ));
		dm.dmSize = sizeof( dm );
		dm.dmPelsWidth = width;
		dm.dmPelsHeight = height;
		dm.dmFields = DM_PELSWIDTH|DM_PELSHEIGHT;

		if( vid_displayfrequency->integer > 0 )
		{
			dm.dmFields |= DM_DISPLAYFREQUENCY;
			dm.dmDisplayFrequency = vid_displayfrequency->integer;
		}
		
		if( ChangeDisplaySettings( &dm, CDS_FULLSCREEN ) == DISP_CHANGE_SUCCESSFUL )
		{
			glState.fullScreen = true;

			if( !VID_CreateWindow( width, height, true ))
				return rserr_invalid_mode;
			return rserr_ok;
		}
		else
		{
			dm.dmPelsWidth = width * 2;
			dm.dmPelsHeight = height;
			dm.dmFields = DM_PELSWIDTH|DM_PELSHEIGHT;
			if( gl_depthbits->integer != 0 )
			{
				dm.dmBitsPerPel = gl_depthbits->integer;
				dm.dmFields |= DM_BITSPERPEL;
			}

			// our first CDS failed, so maybe we're running on some weird dual monitor system 
			if( ChangeDisplaySettings( &dm, CDS_FULLSCREEN ) != DISP_CHANGE_SUCCESSFUL )
			{
				ChangeDisplaySettings( 0, 0 );
				glState.fullScreen = false;
				if( !VID_CreateWindow( width, height, false ))
					return rserr_invalid_mode;
				return rserr_invalid_fullscreen;
			}
			else
			{
				if( !VID_CreateWindow( width, height, true ))
					return rserr_invalid_mode;
				glState.fullScreen = true;
				return rserr_ok;
			}
		}
	}
	else
	{
		ChangeDisplaySettings( 0, 0 );
		glState.fullScreen = false;
		if( !VID_CreateWindow( width, height, false ))
			return rserr_invalid_mode;
	}
	return rserr_ok;
}

/*
==================
R_Init_OpenGL
==================
*/
qboolean R_Init_OpenGL( void )
{
	rserr_t	err;
	qboolean	fullscreen;

	fullscreen = vid_fullscreen->integer;
	vid_fullscreen->modified = false;
	vid_mode->modified = false;

	Sys_LoadLibrary( NULL, &opengl_dll );	// load opengl32.dll
	if( !opengl_dll.link ) return false;

	if(( err = R_ChangeDisplaySettings( vid_mode->integer, fullscreen )) == rserr_ok )
	{
		glConfig.prev_mode = vid_mode->integer;
	}
	else
	{
		if( err == rserr_invalid_fullscreen )
		{
			Cvar_SetFloat( "fullscreen", 0 );
			vid_fullscreen->modified = false;
			MsgDev( D_ERROR, "R_SetMode: fullscreen unavailable in this mode\n" );
			if(( err = R_ChangeDisplaySettings( vid_mode->integer, false )) == rserr_ok )
				return true;
		}
		else if( err == rserr_invalid_mode )
		{
			Cvar_SetFloat( "vid_mode", glConfig.prev_mode );
			vid_mode->modified = false;
			MsgDev( D_ERROR, "R_SetMode: invalid mode\n" );
		}

		// try setting it back to something safe
		if(( err = R_ChangeDisplaySettings( glConfig.prev_mode, false )) != rserr_ok )
		{
			MsgDev( D_ERROR, "R_SetMode: could not revert to safe mode\n" );
			return false;
		}
	}
	return true;
}

/*
===============
GL_SetDefaults
===============
*/
static void GL_SetDefaults( void )
{
	int	i;

	pglFinish();

	pglClearColor( 1, 0, 0.5, 0.5 );

	pglEnable( GL_DEPTH_TEST );
	pglDisable( GL_CULL_FACE );
	pglEnable( GL_SCISSOR_TEST );
	pglDepthFunc( GL_LEQUAL );
	pglDepthMask( GL_FALSE );

	pglColor4f( 1, 1, 1, 1 );

	if( glState.stencilEnabled )
	{
		pglDisable( GL_STENCIL_TEST );
		pglStencilMask( ( GLuint ) ~0 );
		pglStencilFunc( GL_EQUAL, 128, 0xFF );
		pglStencilOp( GL_KEEP, GL_KEEP, GL_INCR );
	}

	// enable gouraud shading
	pglShadeModel( GL_SMOOTH );

	pglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	pglPolygonOffset( -1, -2 );

	// properly disable multitexturing at startup
	for( i = glConfig.max_texture_units - 1; i > 0; i-- )
	{
		GL_SelectTexture( i );
		GL_TexEnv( GL_MODULATE );
		pglDisable( GL_BLEND );
		pglDisable( GL_TEXTURE_2D );
	}

	GL_SelectTexture( 0 );
	pglDisable( GL_BLEND );
	pglDisable( GL_ALPHA_TEST );
	pglDisable( GL_POLYGON_OFFSET_FILL );
	pglEnable( GL_TEXTURE_2D );

	GL_Cull( 0 );
	GL_FrontFace( 0 );

	GL_SetState( GLSTATE_DEPTHWRITE );
	GL_TexEnv( GL_MODULATE );

	R_SetTextureParameters();

	pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
}

/*
=================
R_RenderInfo_f
=================
*/
void R_RenderInfo_f( void )
{

	Msg( "\n" );
	Msg( "GL_VENDOR: %s\n", glConfig.vendor_string );
	Msg( "GL_RENDERER: %s\n", glConfig.renderer_string );
	Msg( "GL_VERSION: %s\n", glConfig.version_string );
	Msg( "GL_EXTENSIONS: %s\n", glConfig.extensions_string );

	Msg( "GL_MAX_TEXTURE_SIZE: %i\n", glConfig.max_2d_texture_size );
	
	if( GL_Support( GL_ARB_MULTITEXTURE ))
		Msg( "GL_MAX_TEXTURE_UNITS_ARB: %i\n", glConfig.max_texture_units );
	if( GL_Support( GL_TEXTURECUBEMAP_EXT ))
		Msg( "GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB: %i\n", glConfig.max_cubemap_size );
	if( GL_Support( GL_ANISOTROPY_EXT ))
		Msg( "GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT: %.1f\n", glConfig.max_texture_anisotropy );
	if( glConfig.texRectangle )
		Msg( "GL_MAX_RECTANGLE_TEXTURE_SIZE_NV: %i\n", glConfig.max_2d_rectangle_size );

	Msg( "\n" );
	Msg( "MODE: %i, %i x %i %s\n", vid_mode->integer, r_width->integer, r_height->integer );
	Msg( "GAMMA: %s\n", (glConfig.deviceSupportsGamma) ? "hardware" : "software" );
	Msg( "\n" );
	Msg( "PICMIP: %i\n", gl_picmip->integer );
	Msg( "SKYMIP: %i\n", gl_skymip->integer );
	Msg( "TEXTUREMODE: %s\n", gl_texturemode->string );
	Msg( "VERTICAL SYNC: %s\n", gl_swapInterval->integer ? "enabled" : "disabled" );
}

//=======================================================================

void GL_InitCommands( void )
{
	Cbuf_AddText( "vidlatch\n" );
	Cbuf_Execute();

	// system screen width and height (don't suppose for change from console at all)
	r_width = Cvar_Get( "width", "640", CVAR_READ_ONLY, "screen width" );
	r_height = Cvar_Get( "height", "480", CVAR_READ_ONLY, "screen height" );
	r_speeds = Cvar_Get( "r_speeds", "0", CVAR_ARCHIVE, "shows renderer speeds" );
	r_fullbright = Cvar_Get( "r_fullbright", "0", CVAR_CHEAT, "disable lightmaps, get fullbright for entities" );

	gl_picmip = Cvar_Get( "gl_picmip", "0", CVAR_RENDERINFO|CVAR_LATCH_VIDEO, "reduces resolution of textures by powers of 2" );
	gl_skymip = Cvar_Get( "gl_skymip", "0", CVAR_RENDERINFO|CVAR_LATCH_VIDEO, "reduces resolution of skybox textures by powers of 2" );
	gl_ignorehwgamma = Cvar_Get( "gl_ignorehwgamma", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "ignore hardware gamma (e.g. not support)" );
	gl_allow_software = Cvar_Get( "gl_allow_software", "0", CVAR_ARCHIVE, "allow OpenGL software emulation" );
	gl_colorbits = Cvar_Get( "gl_colorbits", "0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO, "pixelformat color bits (0 - auto)" );
	gl_depthbits = Cvar_Get( "gl_depthbits", "0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO, "pixelformat depth bits (0 - auto)" );
	gl_texturemode = Cvar_Get( "gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE, "texture filter" );
	gl_texturebits = Cvar_Get( "gl_texturebits", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "set texture upload format (0 - auto)" );
	gl_round_down = Cvar_Get( "gl_round_down", "0", CVAR_RENDERINFO, "down size non-power of two textures" );
	gl_stencilbits = Cvar_Get( "gl_stencilbits", "8", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "pixelformat stencil bits (0 - auto)" );
	gl_check_errors = Cvar_Get( "gl_check_errors", "1", CVAR_ARCHIVE, "ignore video engine errors" );
	gl_swapInterval = Cvar_Get( "gl_swapInterval", "0", CVAR_ARCHIVE,  "time beetween frames (in msec)" );
	gl_extensions = Cvar_Get( "gl_extensions", "1", CVAR_RENDERINFO, "allow gl_extensions" );
	gl_texture_anisotropy = Cvar_Get( "r_anisotropy", "2.0", CVAR_ARCHIVE, "textures anisotropic filter" );
	gl_nobind = Cvar_Get( "gl_nobind", "0", 0, "replace all textures with '*notexture' (perfomance test)" );
	gl_texture_lodbias =  Cvar_Get( "gl_texture_lodbias", "0.0", CVAR_ARCHIVE, "LOD bias for mipmapped textures" );
	gl_compress_textures = Cvar_Get( "gl_compress_textures", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "compress textures to safe video memory" ); 
	gl_showtextures = Cvar_Get( "gl_showtextures", "0", CVAR_CHEAT, "show all uploaded textures" );
	gl_finish = Cvar_Get( "gl_finish", "0", CVAR_ARCHIVE, "use glFinish instead of glFlush" );
	gl_delayfinish = Cvar_Get( "gl_delayfinish", "1", CVAR_ARCHIVE, "make delay before call of glFinish" );
	gl_clear = Cvar_Get( "gl_clear", "0", CVAR_ARCHIVE, "clearing screen after each frame" );
	gl_frontbuffer = Cvar_Get( "r_frontbuffer", "0", 0, "use back or front buffer" );

	// make sure r_swapinterval is checked after vid_restart
	gl_swapInterval->modified = true;

	vid_gamma = Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE, "gamma amount" );
	vid_mode = Cvar_Get( "vid_mode", VID_DEFAULTMODE, CVAR_RENDERINFO, "display resolution mode" );
	vid_fullscreen = Cvar_Get( "fullscreen", "0", CVAR_RENDERINFO|CVAR_LATCH_VIDEO, "set in 1 to enable fullscreen mode" );
	vid_displayfrequency = Cvar_Get ( "vid_displayfrequency", "0", CVAR_RENDERINFO|CVAR_LATCH_VIDEO, "fullscreen refresh rate" );

	Cmd_AddCommand( "r_info", R_RenderInfo_f, "display renderer info" );
	Cmd_AddCommand( "texturelist", R_TextureList_f, "display loaded textures list" );
}

void GL_RemoveCommands( void )
{
	Cmd_RemoveCommand( "r_info");
	Cmd_RemoveCommand( "texturelist" );
}

void GL_InitExtensions( void )
{
	int	flags = 0;

	// initialize gl extensions
	GL_CheckExtension( "OpenGL 1.1.0", opengl_110funcs, NULL, GL_OPENGL_110 );

	// get our various GL strings
	glConfig.vendor_string = pglGetString( GL_VENDOR );
	glConfig.renderer_string = pglGetString( GL_RENDERER );
	glConfig.version_string = pglGetString( GL_VERSION );
	glConfig.extensions_string = pglGetString( GL_EXTENSIONS );
	MsgDev( D_INFO, "Video: %s\n", glConfig.renderer_string );
	
	GL_CheckExtension( "WGL_3DFX_gamma_control", wgl3DFXgammacontrolfuncs, NULL, GL_WGL_3DFX_GAMMA_CONTROL );
	GL_CheckExtension( "WGL_EXT_swap_control", wglswapintervalfuncs, NULL, GL_WGL_SWAPCONTROL );
	GL_CheckExtension( "glDrawRangeElements", drawrangeelementsfuncs, "gl_drawrangeelments", GL_DRAW_RANGEELEMENTS_EXT );

	if( !GL_Support( GL_DRAW_RANGEELEMENTS_EXT ))
		GL_CheckExtension( "GL_EXT_draw_range_elements", drawrangeelementsextfuncs, "gl_drawrangeelments", GL_DRAW_RANGEELEMENTS_EXT );

	// multitexture
	glConfig.max_texture_units = 1;
	GL_CheckExtension( "GL_ARB_multitexture", multitexturefuncs, "gl_arb_multitexture", GL_ARB_MULTITEXTURE );

	if( GL_Support( GL_ARB_MULTITEXTURE ))
	{
		pglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &glConfig.max_texture_units );
		GL_CheckExtension( "GL_ARB_texture_env_combine", NULL, "gl_texture_env_combine", GL_ENV_COMBINE_EXT );

		if( !GL_Support( GL_ENV_COMBINE_EXT ))
			GL_CheckExtension( "GL_EXT_texture_env_combine", NULL, "gl_texture_env_combine", GL_ENV_COMBINE_EXT );

		if( GL_Support( GL_ENV_COMBINE_EXT ))
			GL_CheckExtension( "GL_ARB_texture_env_dot3", NULL, "gl_texture_env_dot3", GL_DOT3_ARB_EXT );
	}
	else
	{
		GL_CheckExtension( "GL_SGIS_multitexture", sgis_multitexturefuncs, "gl_sgis_multitexture", GL_ARB_MULTITEXTURE );
		if( GL_Support( GL_ARB_MULTITEXTURE )) glConfig.max_texture_units = 2;
	}

	if( glConfig.max_texture_units == 1 )
		GL_SetExtension( GL_ARB_MULTITEXTURE, false );

	// 3d texture support
	GL_CheckExtension( "GL_EXT_texture3D", texture3dextfuncs, "gl_texture_3d", GL_TEXTURE_3D_EXT );

	if( GL_Support( GL_TEXTURE_3D_EXT ))
	{
		pglGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &glConfig.max_3d_texture_size );

		if( glConfig.max_3d_texture_size < 32 )
		{
			GL_SetExtension( GL_TEXTURE_3D_EXT, false );
			MsgDev( D_ERROR, "GL_EXT_texture3D reported bogus GL_MAX_3D_TEXTURE_SIZE, disabled\n" );
		}
	}

	GL_CheckExtension( "GL_SGIS_generate_mipmap", NULL, "gl_sgis_generate_mipmaps", GL_SGIS_MIPMAPS_EXT );

	// hardware cubemaps
	GL_CheckExtension( "GL_ARB_texture_cube_map", NULL, "gl_texture_cubemap", GL_TEXTURECUBEMAP_EXT );

	if( GL_Support( GL_TEXTURECUBEMAP_EXT ))
		pglGetIntegerv( GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &glConfig.max_cubemap_size );

	// point particles extension
	GL_CheckExtension( "GL_EXT_point_parameters", pointparametersfunc, NULL, GL_EXT_POINTPARAMETERS );

	GL_CheckExtension( "GL_ARB_texture_non_power_of_two", NULL, "gl_texture_npot", GL_ARB_TEXTURE_NPOT_EXT );
	GL_CheckExtension( "GL_ARB_texture_compression", texturecompressionfuncs, "gl_dds_hardware_support", GL_TEXTURE_COMPRESSION_EXT );
	GL_CheckExtension( "GL_EXT_compiled_vertex_array", compiledvertexarrayfuncs, "gl_cva_support", GL_CUSTOM_VERTEX_ARRAY_EXT );

	if( !GL_Support( GL_CUSTOM_VERTEX_ARRAY_EXT ))
		GL_CheckExtension( "GL_SGI_compiled_vertex_array", compiledvertexarrayfuncs, "gl_cva_support", GL_CUSTOM_VERTEX_ARRAY_EXT );		

	GL_CheckExtension( "GL_EXT_texture_edge_clamp", NULL, "gl_clamp_to_edge", GL_CLAMPTOEDGE_EXT );

	if( !GL_Support( GL_CLAMPTOEDGE_EXT ))
		GL_CheckExtension("GL_SGIS_texture_edge_clamp", NULL, "gl_clamp_to_edge", GL_CLAMPTOEDGE_EXT );

	glConfig.max_texture_anisotropy = 0.0f;
	GL_CheckExtension( "GL_EXT_texture_filter_anisotropic", NULL, "gl_ext_anisotropic_filter", GL_ANISOTROPY_EXT );

	if( GL_Support( GL_ANISOTROPY_EXT ))
		pglGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glConfig.max_texture_anisotropy );

	GL_CheckExtension( "GL_EXT_texture_lod_bias", NULL, "gl_ext_texture_lodbias", GL_TEXTURE_LODBIAS );
	if( GL_Support( GL_TEXTURE_LODBIAS ))
		pglGetFloatv( GL_MAX_TEXTURE_LOD_BIAS_EXT, &glConfig.max_texture_lodbias );

	GL_CheckExtension( "GL_ARB_texture_border_clamp", NULL, "gl_ext_texborder_clamp", GL_CLAMP_TEXBORDER_EXT );

	GL_CheckExtension( "GL_EXT_blend_minmax", blendequationfuncs, "gl_ext_customblend", GL_BLEND_MINMAX_EXT );
	GL_CheckExtension( "GL_EXT_blend_subtract", blendequationfuncs, "gl_ext_customblend", GL_BLEND_SUBTRACT_EXT );

	GL_CheckExtension( "glStencilOpSeparate", gl2separatestencilfuncs, "gl_separate_stencil", GL_SEPARATESTENCIL_EXT );
	if( !GL_Support( GL_SEPARATESTENCIL_EXT ))
		GL_CheckExtension("GL_ATI_separate_stencil", atiseparatestencilfuncs, "gl_separate_stencil", GL_SEPARATESTENCIL_EXT );

	GL_CheckExtension( "GL_EXT_stencil_two_side", stenciltwosidefuncs, "gl_stenciltwoside", GL_STENCILTWOSIDE_EXT );
	GL_CheckExtension( "GL_ARB_vertex_buffer_object", vbofuncs, "gl_vertex_buffer_object", GL_ARB_VERTEX_BUFFER_OBJECT_EXT );

	// we don't care if it's an extension or not, they are identical functions, so keep it simple in the rendering code
	if( pglDrawRangeElementsEXT == NULL ) pglDrawRangeElementsEXT = pglDrawRangeElements;

	GL_CheckExtension( "GL_ARB_texture_env_add", NULL, "gl_texture_env_add", GL_TEXTURE_ENV_ADD_EXT );

	// vp and fp shaders
	GL_CheckExtension( "GL_ARB_shader_objects", shaderobjectsfuncs, "gl_shaderobjects", GL_SHADER_OBJECTS_EXT );
	GL_CheckExtension( "GL_ARB_shading_language_100", NULL, "gl_glslprogram", GL_SHADER_GLSL100_EXT );
	GL_CheckExtension( "GL_ARB_vertex_shader", vertexshaderfuncs, "gl_vertexshader", GL_VERTEX_SHADER_EXT );
	GL_CheckExtension( "GL_ARB_fragment_shader", NULL, "gl_pixelshader", GL_FRAGMENT_SHADER_EXT );

	GL_CheckExtension( "GL_ARB_depth_texture", NULL, "gl_depthtexture", GL_DEPTH_TEXTURE );
	GL_CheckExtension( "GL_ARB_shadow", NULL, "gl_arb_shadow", GL_SHADOW_EXT );

	// occlusion queries
	GL_CheckExtension( "GL_ARB_occlusion_query", occlusionfunc, "gl_occlusion_queries", GL_OCCLUSION_QUERIES_EXT );

	// rectangle textures support
	if( com.strstr( glConfig.extensions_string, "GL_NV_texture_rectangle" ))
	{
		glConfig.texRectangle = GL_TEXTURE_RECTANGLE_NV;
		pglGetIntegerv( GL_MAX_RECTANGLE_TEXTURE_SIZE_NV, &glConfig.max_2d_rectangle_size );
	}
	else if( com.strstr( glConfig.extensions_string, "GL_EXT_texture_rectangle" ))
	{
		glConfig.texRectangle = GL_TEXTURE_RECTANGLE_EXT;
		pglGetIntegerv( GL_MAX_RECTANGLE_TEXTURE_SIZE_EXT, &glConfig.max_2d_rectangle_size );
	}
	else glConfig.texRectangle = glConfig.max_2d_rectangle_size = 0; // no rectangle

	glConfig.max_2d_texture_size = 0;
	pglGetIntegerv( GL_MAX_TEXTURE_SIZE, &glConfig.max_2d_texture_size );
	if( glConfig.max_2d_texture_size <= 0 ) glConfig.max_2d_texture_size = 256;

	Cvar_Get( "gl_max_texture_size", "0", CVAR_INIT, "opengl texture max dims" );
	Cvar_Set( "gl_max_texture_size", va( "%i", glConfig.max_2d_texture_size ));

	// MCD has buffering issues
	if(com.strstr( glConfig.renderer_string, "gdi" ))
		Cvar_SetFloat( "gl_finish", 1 );

	Cvar_Set( "gl_anisotropy", va( "%f", bound( 0, gl_texture_anisotropy->value, glConfig.max_texture_anisotropy )));

	// software mipmap generator does wrong result with NPOT textures ...
	if( !GL_Support( GL_SGIS_MIPMAPS_EXT ))
		GL_SetExtension( GL_ARB_TEXTURE_NPOT_EXT, false );

	flags |= IL_USE_LERPING|IL_ALLOW_OVERWRITE;

	Image_Init( NULL, flags );
	glw_state.initialized = true;
}

/*
===============
R_Init
===============
*/
qboolean R_Init( void )
{
	if( glw_state.initialized ) return true;

	GL_InitCommands();
	GL_SetDefaultState();

	// create the window and set up the context
	if( !R_Init_OpenGL( ))
	{
		GL_RemoveCommands();
		R_Free_OpenGL();
		return false;
	}

	r_temppool = Mem_AllocPool( "Render Zone" );

	GL_InitExtensions();
	GL_SetDefaults();
	R_InitImages();
	R_ClearScene();

	GL_CheckForErrors();

	return true;
}

/*
===============
R_Shutdown
===============
*/
void R_Shutdown( void )
{
	if( !glw_state.initialized )
		return;

	GL_RemoveCommands();
	R_ShutdownImages();

	Mem_FreePool( &r_temppool );

	// shut down OS specific OpenGL stuff like contexts, etc.
	R_Free_OpenGL();
}


/*
=================
GL_CheckForErrors
=================
*/
void GL_CheckForErrors_( const char *filename, const int fileline )
{
	int	err;
	char	*str;

	if( !gl_check_errors->integer )
		return;

	if(( err = pglGetError( )) == GL_NO_ERROR )
		return;

	switch( err )
	{
	case GL_STACK_OVERFLOW:
		str = "GL_STACK_OVERFLOW";
		break;
	case GL_STACK_UNDERFLOW:
		str = "GL_STACK_UNDERFLOW";
		break;
	case GL_INVALID_ENUM:
		str = "GL_INVALID_ENUM";
		break;
	case GL_INVALID_VALUE:
		str = "GL_INVALID_VALUE";
		break;
	case GL_INVALID_OPERATION:
		str = "GL_INVALID_OPERATION";
		break;
	case GL_OUT_OF_MEMORY:
		str = "GL_OUT_OF_MEMORY";
		break;
	default:
		str = "UNKNOWN ERROR";
		break;
	}

	Host_Error( "GL_CheckForErrors: %s (called at %s:%i)\n", str, filename, fileline );
}