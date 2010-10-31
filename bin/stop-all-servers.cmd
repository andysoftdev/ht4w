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
goto stop

:x86_debug
set params=%3 %4 %5 %6 %7 %8 %9
set bin=%~dp0\..\dist\Win32\Debug\
goto stop

:x64
if "%2" NEQ "" (
	if /i "%2" == "debug" goto x64_debug	
)
set params=%2 %3 %4 %5 %6 %7 %8 %9
if "%2" == "" (
	set params=-l error
)
set bin=%~dp0\..\dist\x64\Release\
goto stop

:x64_debug
set params=%3 %4 %5 %6 %7 %8 %9
set bin=%~dp0\..\dist\x64\Debug\
goto stop

:default
set params=-l error
set bin=%~dp0\..\dist\Win32\Release\
goto stop

:default_debug
set params=-l error
set bin=%~dp0\..\dist\Win32\Debug\
goto stop

:stop
if "%bin%" == "" goto usage

if not exist %bin%\conf md %bin%\conf
if not exist %bin%\conf\hypertable.cfg xcopy ..\conf\hypertable.cfg %bin%\conf\
if not exist %bin%\conf\METADATA.xml xcopy ..\conf\METADATA.xml %bin%\conf\

if not exist %bin%\hypertable.exe goto :missing_exe
if not exist %bin%\dfsclient.exe goto :missing_exe

echo shutdown Hypertable.ThriftBroker...
taskkill /F /IM "Hypertable.ThriftBroker.exe" /T > nul

echo shutdown Hypertable.RangeServer/Hypertable.Master...
%bin%\hypertable.exe --batch --execute shutdown;quit; > nul
@ping 127.0.0.1 -n 3 -w 1000 > nul

echo shutdown Hyperspace.Master...
taskkill /F /IM "Hyperspace.Master.exe" /T > nul

echo shutdown Hypertable.LocalBroker...
%bin%\dfsclient.exe --silent --eval shutdown

echo completed.

goto done

:usage
echo Invalid argument, use "stop-all-servers [x86|x64] [debug]"
goto done:

:missing_exe
echo Hypertable executable does not exists
goto done:

:done
