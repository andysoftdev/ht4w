@echo off
setlocal EnableDelayedExpansion

set platform=Win32
set configuration=Release
set params=
set no_rangeserver=
set no_thriftbroker=
set wait=6

for %%p in (%*) do (
	if /i "%%~p" == "x86" (
		set platform=Win32
	) else (
		if /i "%%~p" == "win32" (
			set platform=Win32
		) else (
			if /i "%%~p" == "x64" (
				set platform=x64
			) else (
				if /i "%%~p" == "win64" (
					set platform=x64					
				) else (
					if /i "%%~p" == "release" (
						set configuration=Release
					) else (
						if /i "%%~p" == "debug" (
							set configuration=Debug
						) else (							
							if /i "%%~p" == "no-thriftbroker" (
								set no_thriftbroker=1
							) else (
								if /i "%%~p" == "no-rangeserver" (
									set no_rangeserver=1
								) else (							
									set params=!params! %%p
								)								
							)
						)
					)
				)
			)
		)
	)
)

set bin=%~dp0..\dist\%platform%\%configuration%\

if "%params%" == "" (
	set params=-l error
)

if not exist %bin%\conf md %bin%\conf
if not exist %bin%\conf\hypertable.cfg xcopy %~dp0..\conf\hypertable.cfg %bin%\conf\
if not exist %bin%\conf\METADATA.xml xcopy %~dp0..\conf\METADATA.xml %bin%\conf\

if not exist %bin%\serverup.exe goto :missing_exe

echo Starting Hypertable.LocalBroker...
if not exist %bin%\Hypertable.LocalBroker.exe goto :missing_exe
@start "Hypertable.LocalBroker" /min cmd /c "%bin%\Hypertable.LocalBroker.exe %params%"
@ping 127.0.0.1 -n %wait% -w 1000 > nul
@%bin%\serverup.exe --silent --wait 5000 dfsbroker
if "%ERRORLEVEL%" NEQ "0" goto failed

echo Starting Hyperspace.Master...
if not exist %bin%\Hyperspace.Master.exe goto :missing_exe
@start "Hyperspace.Master" /min cmd /c "%bin%\Hyperspace.Master.exe %params%"
@ping 127.0.0.1 -n %wait% -w 1000 > nul
@%bin%\serverup.exe --silent --wait 5000 hyperspace
if "%ERRORLEVEL%" NEQ "0" goto failed

echo Starting Hypertable.Master...
if not exist %bin%\Hypertable.Master.exe goto :missing_exe
@start "Hypertable.Master" /min cmd /c "%bin%\Hypertable.Master.exe %params%"
@ping 127.0.0.1 -n %wait% -w 1000 > nul
@%bin%\serverup.exe --silent --wait 5000 master
if "%ERRORLEVEL%" NEQ "0" goto failed

if "%no_rangeserver%" == "" (
	echo Starting Hypertable.RangeServer...
	if not exist %bin%\Hypertable.RangeServer.exe goto :missing_exe
	@start "Hypertable.RangeServer" /min cmd /c "%bin%\Hypertable.RangeServer.exe %params%"
	@ping 127.0.0.1 -n %wait% -w 1000 > nul
	@%bin%\serverup.exe --silent --wait 5000 rangeserver
	if "%ERRORLEVEL%" NEQ "0" goto failed
)

if "%no_thriftbroker%" == "" (
	echo Starting Hypertable.ThriftBroker...
	if not exist %bin%\Hypertable.ThriftBroker.exe goto :missing_exe
	@start "Hypertable.ThriftBroker" /min cmd /c "%bin%\Hypertable.ThriftBroker.exe %params%"
	@ping 127.0.0.1 -n %wait% -w 1000 > nul
	@%bin%\serverup.exe --silent --wait 5000 thriftbroker
	if "%ERRORLEVEL%" NEQ "0" goto failed
)

goto done

:missing_exe
echo Hypertable executable does not exists
goto done:

:failed
echo Unable to start servers
goto done:

:done

