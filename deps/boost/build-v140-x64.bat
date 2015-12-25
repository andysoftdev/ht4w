@echo off
rem notice THIS_PATH must not include spaces
set THIS_PATH=%~dp0

set PLATFORM=x64
set VISUALSTUDIOVERSION=14.0
set ZLIB_SOURCE=%THIS_PATH%\..\zlib
set BZIP2_SOURCE=%THIS_PATH%\..\bzip2
set VCVARSALL="c:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"
if not exist %VCVARSALL% goto vcvarsall  
call %VCVARSALL% x64
goto :bootstrap

:vcvarsall
set VCVARSALL="c:\Program Files\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"
if not exist %VCVARSALL% goto missing_vcvarsall  
call %VCVARSALL% x86_amd64

:bootstrap
if exist bjam.exe del bjam.exe
if exist project-config.jam del project-config.jam
if not exist "%THIS_PATH%\bootstrap.bat" goto missing_bootstrap  
echo build bjam
call "%THIS_PATH%\bootstrap.bat"

:build_boost
if not exist "%THIS_PATH%\stage" md "%THIS_PATH%\stage"
if not exist "%THIS_PATH%\stage\%VISUALSTUDIOVERSION%" md "%THIS_PATH%\stage\%VISUALSTUDIOVERSION%"
if not exist "%THIS_PATH%\stage\%VISUALSTUDIOVERSION%\%PLATFORM%" md "%THIS_PATH%\stage\%VISUALSTUDIOVERSION%\%PLATFORM%"

bjam --toolset=msvc-14.0 --build-dir="%THIS_PATH%\..\..\build\%VISUALSTUDIOVERSION%\deps\boost\%PLATFORM%" --stagedir="%THIS_PATH%\stage\%VISUALSTUDIOVERSION%\%PLATFORM%" --without-python --without-mpi -sNO_COMPRESSION=0 -sNO_ZLIB=0 -sZLIB_SOURCE="%ZLIB_SOURCE%" -sNO_BZIP2=0 -sBZIP2_SOURCE="%BZIP2_SOURCE%" address-model=64 debug link=static runtime-link=shared cflags=/GF cxxflags=/GF stage
bjam --toolset=msvc-14.0 --build-dir="%THIS_PATH%\..\..\build\%VISUALSTUDIOVERSION%\deps\boost\%PLATFORM%" --stagedir="%THIS_PATH%\stage\%VISUALSTUDIOVERSION%\%PLATFORM%" --without-python --without-mpi -sNO_COMPRESSION=0 -sNO_ZLIB=0 -sZLIB_SOURCE="%ZLIB_SOURCE%" -sNO_BZIP2=0 -sBZIP2_SOURCE="%BZIP2_SOURCE%" address-model=64 release link=static runtime-link=shared cflags="/GL /GF /fp:precise" cxxflags="/GL /GF /fp:precise" linkflags=/LTCG stage
goto done

:missing_vcvarsall
echo missing vcvarsall.bat, install VC2015 or adjust the VCVARSALL variable
goto done

:missing_bootstrap
echo missing bootstrap.bat, unpack boost content into this folder
goto done

:done
