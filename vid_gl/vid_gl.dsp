# Microsoft Developer Studio Project File - Name="vid_gl" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=vid_gl - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "vid_gl.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "vid_gl.mak" CFG="vid_gl - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "vid_gl - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "vid_gl - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "vid_gl - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir "."
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\vid_gl\!release"
# PROP Intermediate_Dir "..\temp\vid_gl\!release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir "."
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MD /W3 /GX /O2 /Ob2 /I "../public" /I "../common" /I "../game_shared" /I "../engine" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 msvcrt.lib user32.lib gdi32.lib /nologo /subsystem:windows /dll /pdb:none /machine:I386 /nodefaultlib:"libc.lib" /libpath:"../public/libs/"
# SUBTRACT LINK32 /debug
# Begin Custom Build
TargetDir=\Xash3D\src_main\temp\vid_gl\!release
InputPath=\Xash3D\src_main\temp\vid_gl\!release\vid_gl.dll
SOURCE="$(InputPath)"

"D:\Xash3D\bin\vid_gl.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\vid_gl.dll "D:\Xash3D\bin\vid_gl.dll"

# End Custom Build

!ELSEIF  "$(CFG)" == "vid_gl - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir "."
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\temp\vid_gl\!debug"
# PROP Intermediate_Dir "..\temp\vid_gl\!debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir "."
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "../public" /I "../common" /I "../game_shared" /I "../engine" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 msvcrtd.lib user32.lib gdi32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /nodefaultlib:"msvcrt.lib" /pdbtype:sept
# SUBTRACT LINK32 /profile /incremental:no /map
# Begin Custom Build
TargetDir=\Xash3D\src_main\temp\vid_gl\!debug
InputPath=\Xash3D\src_main\temp\vid_gl\!debug\vid_gl.dll
SOURCE="$(InputPath)"

"D:\Xash3D\bin\vid_gl.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\vid_gl.dll "D:\Xash3D\bin\vid_gl.dll"

# End Custom Build

!ENDIF 

# Begin Target

# Name "vid_gl - Win32 Release"
# Name "vid_gl - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\r_backend.c
# End Source File
# Begin Source File

SOURCE=.\r_bloom.c
# End Source File
# Begin Source File

SOURCE=.\r_cull.c
# End Source File
# Begin Source File

SOURCE=.\r_decals.c
# End Source File
# Begin Source File

SOURCE=.\r_draw.c
# End Source File
# Begin Source File

SOURCE=.\r_image.c
# End Source File
# Begin Source File

SOURCE=.\r_light.c
# End Source File
# Begin Source File

SOURCE=.\r_main.c
# End Source File
# Begin Source File

SOURCE=.\r_math.c
# End Source File
# Begin Source File

SOURCE=.\r_mesh.c
# End Source File
# Begin Source File

SOURCE=.\r_model.c
# End Source File
# Begin Source File

SOURCE=.\r_opengl.c
# End Source File
# Begin Source File

SOURCE=.\r_poly.c
# End Source File
# Begin Source File

SOURCE=.\r_program.c
# End Source File
# Begin Source File

SOURCE=.\r_register.c
# End Source File
# Begin Source File

SOURCE=.\r_shader.c
# End Source File
# Begin Source File

SOURCE=.\r_shadow.c
# End Source File
# Begin Source File

SOURCE=.\r_sky.c
# End Source File
# Begin Source File

SOURCE=.\r_sprite.c
# End Source File
# Begin Source File

SOURCE=.\r_studio.c
# End Source File
# Begin Source File

SOURCE=.\r_surf.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\r_backend.h
# End Source File
# Begin Source File

SOURCE=.\r_local.h
# End Source File
# Begin Source File

SOURCE=.\r_math.h
# End Source File
# Begin Source File

SOURCE=.\r_mesh.h
# End Source File
# Begin Source File

SOURCE=.\r_model.h
# End Source File
# Begin Source File

SOURCE=.\r_opengl.h
# End Source File
# Begin Source File

SOURCE=.\r_public.h
# End Source File
# Begin Source File

SOURCE=.\r_shader.h
# End Source File
# Begin Source File

SOURCE=.\r_shadow.h
# End Source File
# End Group
# End Target
# End Project
