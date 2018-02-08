# Microsoft Developer Studio Project File - Name="vgui" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=vgui - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "vgui.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "vgui.mak" CFG="vgui - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "vgui - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "vgui - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "vgui - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\temp\vgui\!release"
# PROP Intermediate_Dir "..\..\..\temp\vgui\!release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "VGUI_DLL_EXPORTS" /YX /FD /c
# ADD CPP /nologo /W3 /GR /GX /O2 /I "..\include" /I "..\..\..\engine" /I "..\..\..\common" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 msvcrt.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /dll /pdb:none /machine:I386 /nodefaultlib:"libc.lib" /opt:nowin98
# Begin Custom Build
TargetDir=\Xash3D\src_main\temp\vgui\!release
InputPath=\Xash3D\src_main\temp\vgui\!release\vgui.dll
SOURCE="$(InputPath)"

BuildCmds= \
	copy $(TargetDir)\vgui.dll "D:\Xash3D\vgui.dll" \
	copy $(TargetDir)\vgui.lib "D:\Xash3D\src_main\utils\vgui\lib\win32_vc6\vgui.lib" \
	

"D:\Xash3D\vgui.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"D:\Xash3D\src_main\utils\vgui\lib\win32_vc6\vgui.lib" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "vgui - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\temp\vgui\!debug"
# PROP Intermediate_Dir "..\..\..\temp\vgui\!debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "VGUI_DLL_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /Gi /GR /GX /ZI /Od /I "..\include" /I "..\..\..\engine" /I "..\..\..\common" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /FAs /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 msvcrtd.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /dll /debug /machine:I386 /nodefaultlib:"LIBCMTD" /pdbtype:sept
# Begin Custom Build
TargetDir=\Xash3D\src_main\temp\vgui\!debug
InputPath=\Xash3D\src_main\temp\vgui\!debug\vgui.dll
SOURCE="$(InputPath)"

"D:\Xash3D\src_main\utils\vgui\lib\win32_vc6\vgui.lib" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\vgui.dll "D:\Xash3D\vgui.dll" 
	copy $(TargetDir)\vgui.lib "D:\Xash3D\src_main\utils\vgui\lib\win32_vc6\vgui.lib" 
	
# End Custom Build

!ENDIF 

# Begin Target

# Name "vgui - Win32 Release"
# Name "vgui - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "win32"

# PROP Default_Filter ""
# Begin Source File

SOURCE="win32_vc6\App.cpp"

!IF  "$(CFG)" == "vgui - Win32 Release"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!release\win32"

!ELSEIF  "$(CFG)" == "vgui - Win32 Debug"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!debug\win32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE="win32_vc6\Cursor.cpp"

!IF  "$(CFG)" == "vgui - Win32 Release"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!release\win32"

!ELSEIF  "$(CFG)" == "vgui - Win32 Debug"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!debug\win32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE="win32_vc6\fileimage.cpp"

!IF  "$(CFG)" == "vgui - Win32 Release"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!release\win32"

!ELSEIF  "$(CFG)" == "vgui - Win32 Debug"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!debug\win32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE="win32_vc6\fileimage.h"

!IF  "$(CFG)" == "vgui - Win32 Release"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!release\win32"

!ELSEIF  "$(CFG)" == "vgui - Win32 Debug"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!debug\win32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE="win32_vc6\Font.cpp"

!IF  "$(CFG)" == "vgui - Win32 Release"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!release\win32"

!ELSEIF  "$(CFG)" == "vgui - Win32 Debug"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!debug\win32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE="win32_vc6\Surface.cpp"

!IF  "$(CFG)" == "vgui - Win32 Release"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!release\win32"

!ELSEIF  "$(CFG)" == "vgui - Win32 Debug"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!debug\win32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE="win32_vc6\vfontdata.cpp"

!IF  "$(CFG)" == "vgui - Win32 Release"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!release\win32"

!ELSEIF  "$(CFG)" == "vgui - Win32 Debug"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!debug\win32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE="win32_vc6\vfontdata.h"

!IF  "$(CFG)" == "vgui - Win32 Release"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!release\win32"

!ELSEIF  "$(CFG)" == "vgui - Win32 Debug"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!debug\win32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE="win32_vc6\vgui.cpp"

!IF  "$(CFG)" == "vgui - Win32 Release"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!release\win32"

!ELSEIF  "$(CFG)" == "vgui - Win32 Debug"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!debug\win32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE="win32_vc6\vgui_win32.h"

!IF  "$(CFG)" == "vgui - Win32 Release"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!release\win32"

!ELSEIF  "$(CFG)" == "vgui - Win32 Debug"

# PROP Intermediate_Dir "..\..\..\temp\vgui\!debug\win32"

!ENDIF 

# End Source File
# End Group
# Begin Source File

SOURCE="App.cpp"
# End Source File
# Begin Source File

SOURCE="Bitmap.cpp"
# End Source File
# Begin Source File

SOURCE="BitmapTGA.cpp"
# End Source File
# Begin Source File

SOURCE="Border.cpp"
# End Source File
# Begin Source File

SOURCE="BorderLayout.cpp"
# End Source File
# Begin Source File

SOURCE="BorderPair.cpp"
# End Source File
# Begin Source File

SOURCE="BuildGroup.cpp"
# End Source File
# Begin Source File

SOURCE="Button.cpp"
# End Source File
# Begin Source File

SOURCE="ButtonGroup.cpp"
# End Source File
# Begin Source File

SOURCE="CheckButton.cpp"
# End Source File
# Begin Source File

SOURCE="Color.cpp"
# End Source File
# Begin Source File

SOURCE="ConfigWizard.cpp"
# End Source File
# Begin Source File

SOURCE="Cursor.cpp"
# End Source File
# Begin Source File

SOURCE="DataInputStream.cpp"
# End Source File
# Begin Source File

SOURCE="Desktop.cpp"
# End Source File
# Begin Source File

SOURCE="DesktopIcon.cpp"
# End Source File
# Begin Source File

SOURCE="EditPanel.cpp"
# End Source File
# Begin Source File

SOURCE="EtchedBorder.cpp"
# End Source File
# Begin Source File

SOURCE="FileInputStream.cpp"
# End Source File
# Begin Source File

SOURCE="FlowLayout.cpp"
# End Source File
# Begin Source File

SOURCE="FocusNavGroup.cpp"
# End Source File
# Begin Source File

SOURCE="Font.cpp"
# End Source File
# Begin Source File

SOURCE="Frame.cpp"
# End Source File
# Begin Source File

SOURCE="GridLayout.cpp"
# End Source File
# Begin Source File

SOURCE="HeaderPanel.cpp"
# End Source File
# Begin Source File

SOURCE="Image.cpp"
# End Source File
# Begin Source File

SOURCE="ImagePanel.cpp"
# End Source File
# Begin Source File

SOURCE="IntLabel.cpp"
# End Source File
# Begin Source File

SOURCE="Label.cpp"
# End Source File
# Begin Source File

SOURCE="Layout.cpp"
# End Source File
# Begin Source File

SOURCE="LineBorder.cpp"
# End Source File
# Begin Source File

SOURCE="ListPanel.cpp"
# End Source File
# Begin Source File

SOURCE="LoweredBorder.cpp"
# End Source File
# Begin Source File

SOURCE="Menu.cpp"
# End Source File
# Begin Source File

SOURCE="MenuItem.cpp"
# End Source File
# Begin Source File

SOURCE="MenuSeparator.cpp"
# End Source File
# Begin Source File

SOURCE="MessageBox.cpp"
# End Source File
# Begin Source File

SOURCE="MiniApp.cpp"
# End Source File
# Begin Source File

SOURCE="Panel.cpp"
# End Source File
# Begin Source File

SOURCE="PopupMenu.cpp"
# End Source File
# Begin Source File

SOURCE="ProgressBar.cpp"
# End Source File
# Begin Source File

SOURCE="RadioButton.cpp"
# End Source File
# Begin Source File

SOURCE="RaisedBorder.cpp"
# End Source File
# Begin Source File

SOURCE="Scheme.cpp"
# End Source File
# Begin Source File

SOURCE="ScrollBar.cpp"
# End Source File
# Begin Source File

SOURCE="ScrollPanel.cpp"
# End Source File
# Begin Source File

SOURCE="Slider.cpp"
# End Source File
# Begin Source File

SOURCE="StackLayout.cpp"
# End Source File
# Begin Source File

SOURCE="String.cpp"
# End Source File
# Begin Source File

SOURCE="Surface.cpp"
# End Source File
# Begin Source File

SOURCE="SurfaceBase.cpp"
# End Source File
# Begin Source File

SOURCE="TablePanel.cpp"
# End Source File
# Begin Source File

SOURCE="TabPanel.cpp"
# End Source File
# Begin Source File

SOURCE="TaskBar.cpp"
# End Source File
# Begin Source File

SOURCE="TextEntry.cpp"
# End Source File
# Begin Source File

SOURCE="TextGrid.cpp"
# End Source File
# Begin Source File

SOURCE="TextImage.cpp"
# End Source File
# Begin Source File

SOURCE="TextPanel.cpp"
# End Source File
# Begin Source File

SOURCE="ToggleButton.cpp"
# End Source File
# Begin Source File

SOURCE="TreeFolder.cpp"
# End Source File
# Begin Source File

SOURCE="vgui.cpp"
# End Source File
# Begin Source File

SOURCE="WizardPanel.cpp"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\include\VGUI.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_ActionSignal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_App.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_Bitmap.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_BitmapTGA.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_Border.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_BorderLayout.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_BorderPair.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_BuildGroup.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_Button.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_ButtonController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_ButtonGroup.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_ChangeSignal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_CheckButton.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_Color.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_ComboKey.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_ConfigWizard.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_Cursor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_Dar.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_DataInputStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_Desktop.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_DesktopIcon.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_EditPanel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_EtchedBorder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_FileInputStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_FlowLayout.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_FocusChangeSignal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_FocusNavGroup.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_Font.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_Frame.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_FrameSignal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_GridLayout.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_HeaderPanel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_Image.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_ImagePanel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_InputSignal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_InputStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_IntChangeSignal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_IntLabel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_KeyCode.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_Label.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_Layout.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_LayoutInfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_LineBorder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_ListPanel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_LoweredBorder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_Menu.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_MenuItem.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_MenuSeparator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_MessageBox.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_MiniApp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_MouseCode.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_Panel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_Point.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_PopupMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_ProgressBar.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_RadioButton.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_RaisedBorder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_RepaintSignal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_Scheme.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_ScrollBar.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_ScrollPanel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_Slider.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_StackLayout.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_String.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_Surface.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_SurfaceBase.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_SurfaceGL.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_TablePanel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_TabPanel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_TaskBar.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_TextEntry.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_TextGrid.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_TextImage.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_TextPanel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_TickSignal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_ToggleButton.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_TreeFolder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\VGUI_WizardPanel.h
# End Source File
# End Group
# End Target
# End Project
