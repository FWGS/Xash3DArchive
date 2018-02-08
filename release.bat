@echo off

set MSDEV=BuildConsole
set CONFIG=/ShowTime /ShowAgent /nologo /cfg=
set MSDEV=msdev
set CONFIG=/make 
set build_type=release
set BUILD_ERROR=
call vcvars32

%MSDEV% engine/engine.dsp %CONFIG%"engine - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% mainui/mainui.dsp %CONFIG%"mainui - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% utils/vgui/lib/vgui.dsp %CONFIG%"vgui - Win32 Release" %build_target%
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
if exist engine\engine.plg del /f /q engine\engine.plg
if exist mainui\mainui.plg del /f /q mainui\mainui.plg
if exist utils\vgui\lib\vgui.plg del /f /q utils\vgui\lib\vgui.plg

echo
echo 	     Build succeeded!
echo
:done