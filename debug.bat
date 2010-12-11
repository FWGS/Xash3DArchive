@echo off

set MSDEV=BuildConsole
set CONFIG=/ShowTime /ShowAgent /nologo /cfg=
set MSDEV=msdev
set CONFIG=/make 
set build_type=debug
set BUILD_ERROR=
call vcvars32

%MSDEV% dlls/hl.dsp %CONFIG%"hl - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% cl_dll/cl_dll.dsp %CONFIG%"cl_dll - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% engine/engine.dsp %CONFIG%"engine - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% mainui/mainui.dsp %CONFIG%"mainui - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% launch/launch.dsp %CONFIG%"launch - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

if "%BUILD_ERROR%"=="" goto build_ok

echo *********************
echo *********************
echo *** Build Errors! ***
echo *********************
echo *********************
echo press any key to exit
echo *********************
pause>nul
goto done


@rem
@rem Successful build
@rem
:build_ok

rem //delete log files
if exist dlls\hl.plg del /f /q dlls\hl.plg
if exist cl_dll\cl_dll.plg del /f /q cl_dll\cl_dll.plg
if exist engine\engine.plg del /f /q engine\engine.plg
if exist mainui\mainui.plg del /f /q mainui\mainui.plg
if exist launch\launch.plg del /f /q launch\launch.plg
if exist vid_gl\vid_gl.plg del /f /q vid_gl\vid_gl.plg
if exist viewer\viewer.plg del /f /q viewer\viewer.plg

echo
echo 	     Build succeeded!
echo
:done