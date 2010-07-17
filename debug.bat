@echo off

set MSDEV=BuildConsole
set CONFIG=/ShowTime /ShowAgent /nologo /cfg=
set MSDEV=msdev
set CONFIG=/make 
set build_type=debug
set BUILD_ERROR=
call vcvars32

%MSDEV% baserc/baserc.dsp %CONFIG%"baserc - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% bshift/bshift.dsp %CONFIG%"bshift - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% client/client.dsp %CONFIG%"client - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% engine/engine.dsp %CONFIG%"engine - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% gameui/gameui.dsp %CONFIG%"gameui - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% launch/launch.dsp %CONFIG%"launch - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% physic/physic.dsp %CONFIG%"physic - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% vid_gl/vid_gl.dsp %CONFIG%"vid_gl - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% spirit/spirit.dsp %CONFIG%"spirit - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% snd_dx/snd_dx.dsp %CONFIG%"snd_dx - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% xtools/xtools.dsp %CONFIG%"xtools - Win32 Debug" %build_target%
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
if exist baserc\baserc.plg del /f /q baserc\baserc.plg
if exist bshift\bshift.plg del /f /q bshift\bshift.plg
if exist client\client.plg del /f /q client\client.plg
if exist engine\engine.plg del /f /q engine\engine.plg
if exist gameui\gameui.plg del /f /q gameui\gameui.plg
if exist launch\launch.plg del /f /q launch\launch.plg
if exist physic\physic.plg del /f /q physic\physic.plg
if exist spirit\spirit.plg del /f /q spirit\spirit.plg
if exist vid_gl\vid_gl.plg del /f /q vid_gl\vid_gl.plg
if exist viewer\viewer.plg del /f /q viewer\viewer.plg
if exist snd_dx\snd_dx.plg del /f /q snd_dx\snd_dx.plg
if exist xtools\xtools.plg del /f /q xtools\xtools.plg

echo 	     Build succeeded!
echo Please wait. Xash is now loading
cd D:\Xash3D\
hl.exe -log -dev 3 +loadquick
:done