@echo off

@call "%ANT_HOME%\bin\ant" clean -Dversion=%1 -Dvisualstudioversion=%2 > nul

cd java

for /f "delims=" %%i in ('dir /s /b /a-d "pom.xml.in"') do (
   ..\sed.exe -e s/@VERSION@/%1/ig "%%i" > "%%~pipom.xml"
)

@call "%MAVEN_HOME%\bin\mvn.bat" -Pcdh3 clean > nul
@call "%MAVEN_HOME%\bin\mvn.bat" -Papache1 clean > nul
@call "%MAVEN_HOME%\bin\mvn.bat" -Papache2 clean > nul

del /Q /s "pom.xml" > nul
del /Q /s "hypertable\target" > nul
del /Q /s "hypertable-common\target" > nul
del /Q /s "hypertable-common\src\main\java\org\hypertable\thriftgen" > nul
del /Q /s "hypertable-cdh3\target" > nul
del /Q /s "hypertable-apache1\target" > nul
del /Q /s "hypertable-apache2\target" > nul
del /Q /s "hypertable-examples\target" > nul

cd ..