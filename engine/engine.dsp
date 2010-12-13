# Microsoft Developer Studio Project File - Name="engine" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=engine - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "engine.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "engine.mak" CFG="engine - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "engine - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "engine - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "engine - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\engine\!release"
# PROP Intermediate_Dir "..\temp\engine\!release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "./" /I "common" /I "server" /I "client" /I "client/sound" /I "../public" /I "../common" /I "../game_shared" /I "../pm_shared" /I "../utils/vgui/include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386 /opt:nowin98
# ADD LINK32 msvcrt.lib user32.lib vfw32.lib msacm32.lib gdi32.lib winmm.lib ../utils/vgui/lib/win32_vc6/vgui.lib /nologo /subsystem:windows /dll /pdb:none /machine:I386 /nodefaultlib:"libc.lib" /opt:nowin98
# SUBTRACT LINK32 /debug /nodefaultlib
# Begin Custom Build
TargetDir=\Xash3D\src_main\temp\engine\!release
InputPath=\Xash3D\src_main\temp\engine\!release\engine.dll
SOURCE="$(InputPath)"

"D:\Xash3D\engine.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\engine.dll "D:\Xash3D\engine.dll"

# End Custom Build

!ELSEIF  "$(CFG)" == "engine - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\temp\engine\!debug"
# PROP Intermediate_Dir "..\temp\engine\!debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "./" /I "common" /I "server" /I "client" /I "client/sound" /I "../public" /I "../common" /I "../game_shared" /I "../pm_shared" /I "../utils/vgui/include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 user32.lib msvcrtd.lib vfw32.lib msacm32.lib gdi32.lib winmm.lib ../utils/vgui/lib/win32_vc6/vgui.lib /nologo /subsystem:windows /dll /debug /machine:I386 /nodefaultlib:"msvcrt.lib" /pdbtype:sept
# SUBTRACT LINK32 /incremental:no /map /nodefaultlib
# Begin Custom Build
TargetDir=\Xash3D\src_main\temp\engine\!debug
InputPath=\Xash3D\src_main\temp\engine\!debug\engine.dll
SOURCE="$(InputPath)"

"D:\Xash3D\engine.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\engine.dll "D:\Xash3D\engine.dll"

# End Custom Build

!ENDIF 

# Begin Target

# Name "engine - Win32 Release"
# Name "engine - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\common\avikit.c
# End Source File
# Begin Source File

SOURCE=.\common\build.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_cmds.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_demo.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_frame.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_game.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_main.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_menu.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_parse.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_pmove.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_pred.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_scrn.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_tent.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_video.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_view.c
# End Source File
# Begin Source File

SOURCE=.\common\con_utils.c
# End Source File
# Begin Source File

SOURCE=.\common\console.c
# End Source File
# Begin Source File

SOURCE=.\common\engfuncs.c
# End Source File
# Begin Source File

SOURCE=.\client\gl_backend.c
# End Source File
# Begin Source File

SOURCE=.\client\gl_beams.c
# End Source File
# Begin Source File

SOURCE=.\client\gl_cull.c
# End Source File
# Begin Source File

SOURCE=.\client\gl_decals.c
# End Source File
# Begin Source File

SOURCE=.\client\gl_draw.c
# End Source File
# Begin Source File

SOURCE=.\client\gl_image.c
# End Source File
# Begin Source File

SOURCE=.\client\gl_refrag.c
# End Source File
# Begin Source File

SOURCE=.\client\gl_rlight.c
# End Source File
# Begin Source File

SOURCE=.\client\gl_rmain.c
# End Source File
# Begin Source File

SOURCE=.\client\gl_rmath.c
# End Source File
# Begin Source File

SOURCE=.\client\gl_rmisc.c
# End Source File
# Begin Source File

SOURCE=.\client\gl_rpart.c
# End Source File
# Begin Source File

SOURCE=.\client\gl_rsurf.c
# End Source File
# Begin Source File

SOURCE=.\client\gl_sprite.c
# End Source File
# Begin Source File

SOURCE=.\client\gl_studio.c
# End Source File
# Begin Source File

SOURCE=.\client\gl_vidnt.c
# End Source File
# Begin Source File

SOURCE=.\client\gl_warp.c
# End Source File
# Begin Source File

SOURCE=.\common\host.c
# End Source File
# Begin Source File

SOURCE=.\common\infostring.c
# End Source File
# Begin Source File

SOURCE=.\common\input.c
# End Source File
# Begin Source File

SOURCE=.\common\keys.c
# End Source File
# Begin Source File

SOURCE=.\common\mathlib.c
# End Source File
# Begin Source File

SOURCE=.\common\model.c
# End Source File
# Begin Source File

SOURCE=.\common\net_buffer.c
# End Source File
# Begin Source File

SOURCE=.\common\net_chan.c
# End Source File
# Begin Source File

SOURCE=.\common\net_encode.c
# End Source File
# Begin Source File

SOURCE=.\common\net_huff.c
# End Source File
# Begin Source File

SOURCE=.\common\pm_studio.c
# End Source File
# Begin Source File

SOURCE=.\common\pm_surface.c
# End Source File
# Begin Source File

SOURCE=.\common\pm_trace.c
# End Source File
# Begin Source File

SOURCE=.\client\sound\s_backend.c
# End Source File
# Begin Source File

SOURCE=.\client\sound\s_dsp.c
# End Source File
# Begin Source File

SOURCE=.\client\sound\s_load.c
# End Source File
# Begin Source File

SOURCE=.\client\sound\s_main.c
# End Source File
# Begin Source File

SOURCE=.\client\sound\s_mix.c
# End Source File
# Begin Source File

SOURCE=.\client\sound\s_mouth.c
# End Source File
# Begin Source File

SOURCE=.\client\sound\s_stream.c
# End Source File
# Begin Source File

SOURCE=.\client\sound\s_utils.c
# End Source File
# Begin Source File

SOURCE=.\client\sound\s_vox.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_client.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_cmds.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_custom.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_frame.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_game.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_init.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_main.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_move.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_phys.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_pmove.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_save.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_studio.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_world.c
# End Source File
# Begin Source File

SOURCE=.\common\titles.c
# End Source File
# Begin Source File

SOURCE=.\common\vgui_int.cpp
# End Source File
# Begin Source File

SOURCE=.\common\world.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\client\cl_tent.h
# End Source File
# Begin Source File

SOURCE=.\client\client.h
# End Source File
# Begin Source File

SOURCE=.\common\cm_local.h
# End Source File
# Begin Source File

SOURCE=.\common\common.h
# End Source File
# Begin Source File

SOURCE=.\client\gl_local.h
# End Source File
# Begin Source File

SOURCE=.\common\mathlib.h
# End Source File
# Begin Source File

SOURCE=.\common\net_buffer.h
# End Source File
# Begin Source File

SOURCE=.\common\net_encode.h
# End Source File
# Begin Source File

SOURCE=.\common\net_msg.h
# End Source File
# Begin Source File

SOURCE=.\common\protocol.h
# End Source File
# Begin Source File

SOURCE=.\server\server.h
# End Source File
# Begin Source File

SOURCE=.\client\sound\sound.h
# End Source File
# Begin Source File

SOURCE=.\client\sound\vox.h
# End Source File
# Begin Source File

SOURCE=.\common\world.h
# End Source File
# End Group
# End Target
# End Project
