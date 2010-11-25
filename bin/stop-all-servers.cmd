@echo off
setlocal EnableDelayedExpansion

set platform=Win32
set configuration=Release
set params=
set no_thriftbroker=
set wait=5

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
								set params=!params! %%p		
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

if not exist %bin%\hypertable.exe goto :missing_exe
if not exist %bin%\dfsclient.exe goto :missing_exe

if "%no_thriftbroker%" == "" (
	echo shutdown Hypertable.ThriftBroker...
	@taskkill /F /IM "Hypertable.ThriftBroker.exe" /T > nul
)

echo shutdown Hypertable.RangeServer/Hypertable.Master...
@%bin%\hypertable.exe --batch --execute shutdown;quit; > nul
@ping 127.0.0.1 -n %wait% -w 1000 > nul

echo shutdown Hyperspace.Master...
@taskkill /F /IM "Hyperspace.Master.exe" /T > nul

echo shutdown Hypertable.LocalBroker...
@%bin%\dfsclient.exe --silent --eval shutdown

goto done

:missing_exe
echo Hypertable executable does not exists
goto done:

:done
