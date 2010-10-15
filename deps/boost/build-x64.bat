@echo off
rem notice THIS_PATH must not include spaces
set THIS_PATH=%~dp0

set PLATFORM=x64
set ZLIB_SOURCE=%THIS_PATH%\..\zlib
set BZIP2_SOURCE=%THIS_PATH%\..\bzip2
set VCVARSALL="c:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat"
if not exist %VCVARSALL% goto missing_vcvarsall  
call %VCVARSALL% x64

:bootstrap
if exist bjam.exe goto build_boost  
if not exist "%THIS_PATH%\bootstrap.bat" goto missing_bootstrap  
echo build bjam
call "%THIS_PATH%\bootstrap.bat"

:build_boost
if not exist "%THIS_PATH%\stage" md "%THIS_PATH%\stage"
if not exist "%THIS_PATH%\stage\%PLATFORM%" md "%THIS_PATH%\stage\%PLATFORM%"


bjam --build-dir="%THIS_PATH%\..\..\build\deps\boost\%PLATFORM%" --stagedir="%THIS_PATH%\stage\%PLATFORM%" --without-python --without-mpi -sNO_COMPRESSION=0 -sNO_ZLIB=0 -sZLIB_SOURCE="%ZLIB_SOURCE%" -sNO_BZIP2=0 -sBZIP2_SOURCE="%BZIP2_SOURCE%" address-model=64 debug link=static runtime-link=shared cflags=/GF cxxflags=/GF stage
bjam --build-dir="%THIS_PATH%\..\..\build\deps\boost\%PLATFORM%" --stagedir="%THIS_PATH%\stage\%PLATFORM%" --without-python --without-mpi -sNO_COMPRESSION=0 -sNO_ZLIB=0 -sZLIB_SOURCE="%ZLIB_SOURCE%" -sNO_BZIP2=0 -sBZIP2_SOURCE="%BZIP2_SOURCE%" address-model=64 release link=static runtime-link=shared cflags="/GL /GF /fp:precise" cxxflags="/GL /GF /fp:precise" linkflags=/LTCG stage
goto done

:missing_vcvarsall
echo missing vcvarsall.bat, install VC2010 or adjust the VCVARSALL variable
goto done

:missing_bootstrap
echo missing bootstrap.bat, unpack boost content into this folder
goto done

:done
