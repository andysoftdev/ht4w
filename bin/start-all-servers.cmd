@echo off
if "%1" == "" goto default
if /i "%1" == "x86" goto x86
if /i "%1" == "x64" goto x64
if /i "%1" == "debug" goto default_debug

goto default

:x86
if "%2" NEQ "" (
	if /i "%2" == "debug" goto x86_debug	
)
set params=%2 %3 %4 %5 %6 %7 %8 %9
if "%2" == "" (
	set params=-l error
)
set bin=%~dp0\..\dist\Win32\Release\
goto start

:x86_debug
set params=%3 %4 %5 %6 %7 %8 %9
set bin=%~dp0\..\dist\Win32\Debug\
goto start

:x64
if "%2" NEQ "" (
	if /i "%2" == "debug" goto x64_debug	
)
set params=%2 %3 %4 %5 %6 %7 %8 %9
if "%2" == "" (
	set params=-l error
)
set bin=%~dp0\..\dist\x64\Release\
goto start

:x64_debug
set params=%3 %4 %5 %6 %7 %8 %9
set bin=%~dp0\..\dist\x64\Debug\
goto start

:default
set params=-l error
set bin=%~dp0\..\dist\Win32\Release\
goto start

:default_debug
set params=-l error
set bin=%~dp0\..\dist\Win32\Debug\
goto start

:start
if "%bin%" == "" goto usage

if not exist %bin%\conf md %bin%\conf
if not exist %bin%\conf\hypertable.cfg xcopy ..\conf\hypertable.cfg %bin%\conf\
if not exist %bin%\conf\METADATA.xml xcopy ..\conf\METADATA.xml %bin%\conf\

echo Starting Hypertable.LocalBroker...
if not exist %bin%\Hypertable.LocalBroker.exe goto :missing_exe
@start cmd /c "%bin%\Hypertable.LocalBroker.exe %params%"
@ping 127.0.0.1 -n 3 -w 1000 > nul

echo Starting Hyperspace.Master...
if not exist %bin%\Hyperspace.Master.exe goto :missing_exe
@start cmd /c "%bin%\Hyperspace.Master.exe %params%"
@ping 127.0.0.1 -n 5 -w 1000 > nul

echo Starting Hypertable.Master...
if not exist %bin%\Hypertable.Master.exe goto :missing_exe
@start cmd /c "%bin%\Hypertable.Master.exe %params%"
@ping 127.0.0.1 -n 5 -w 1000 > nul

echo Starting Hypertable.RangeServer...
if not exist %bin%\Hypertable.RangeServer.exe goto :missing_exe
@start cmd /c "%bin%\Hypertable.RangeServer.exe %params%"
@ping 127.0.0.1 -n 5 -w 1000 > nul

echo completed.

goto done

:usage
echo Invalid argument, use "start-all-servers [x86|x64] [debug]"
goto done:

:missing_exe
echo Hypertable executable does not exists
goto done:

:done

