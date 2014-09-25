@echo off

deps\thrift\thrift-0.9.1.exe -r -gen java -out java/hypertable-common/src/main/java src\cc\ThriftBroker\client.thrift
deps\thrift\thrift-0.9.1.exe -r -gen java -out java/hypertable-common/src/main/java src\cc\ThriftBroker\hql.thrift

@call "%ANT_HOME%\bin\ant" dist -Dversion=%1 -Dvisualstudioversion=%2

cd java

for /f "delims=" %%i in ('dir /s /b /a-d "pom.xml.in"') do (
   ..\sed.exe -e s/@VERSION@/%1/ig "%%i" > "%%~pipom.xml"
)

@set MAVEN_OPTS=-Xmx512m -XX:MaxPermSize=128m
@call "%MAVEN_HOME%\bin\mvn" -U -Dmaven.test.skip=true -Pcdh3 package
xcopy /Y hypertable\target\hypertable-%1-cdh3.jar ..\dist\%2\libs\java\

@call "%MAVEN_HOME%\bin\mvn" -U -Dmaven.test.skip=true -Papache1 package
xcopy /Y hypertable\target\hypertable-%1-apache1.jar ..\dist\%2\libs\java\

@call "%MAVEN_HOME%\bin\mvn" -U -Dmaven.test.skip=true -Papache2 package
xcopy /Y hypertable\target\hypertable-%1-apache2.jar ..\dist\%2\libs\java\

del /Q /s "pom.xml" > nul
del /Q /s "hypertable\target" > nul
del /Q /s "hypertable-common\target" > nul
del /Q /s "hypertable-common\src\main\java\org\hypertable\thriftgen" > nul
del /Q /s "hypertable-cdh3\target" > nul
del /Q /s "hypertable-apache1\target" > nul
del /Q /s "hypertable-apache2\target" > nul
del /Q /s "hypertable-examples\target" > nul

cd ..

