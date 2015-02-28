@echo off
setlocal EnableDelayedExpansion

set platform=Win32
set visualstudioversion=12.0
set configuration=Release
set params=

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
							set params=!params! %%p
						)
					)
				)
			)
		)
	)
)

set bin=%~dp0..\dist\%visualstudioversion%\%platform%\%configuration%\

if not exist %bin%\conf md %bin%\conf > nul
if not exist %bin%\conf\hypertable.cfg xcopy %~dp0..\conf\hypertable.cfg %bin%\conf\ /Q /R /Y /D > nul
xcopy %~dp0..\conf\METADATA.xml %bin%\conf\ /Q /R /Y /D > nul
xcopy %~dp0..\conf\RS_METRICS.xml %bin%\conf\ /Q /R /Y /D > nul

if not exist %bin%\hypertable.service.exe goto :missing_exe
@%bin%\hypertable.service.exe --stop-all-services --stop-servers %params%

goto done

:missing_exe
echo hypertable.service.exe does not exists
goto done:

:done
