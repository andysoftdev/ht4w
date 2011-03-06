HOW TO BUILD HYPERTABLE FOR WINDOWS
===================================

Building hypertable from source for Windows requires Microsoft Visual Studio 2010,
in order to build and run all regression tests the option maximum number of parallel
project builds should be set to one (the option is available under Tools/Options/Project
and Solutions/Build and Run).

###Browse or get the source###

* Browse or download the source at [github](http://github.com/andysoftdev/hypertable).
  Download the latest sources by pressing the Downloads button and choosing
  Download.tar.gz or Download.zip.
  
* Or get the source from the repository, create a projects folder (the path must not
  contain any spaces) and use:

  git clone git://github.com/andysoftdev/hypertable.git


###Download and install boost###

* Download the latest boost version from [boost](http://www.boost.org/users/download/)
  or [sourceforge](http://sourceforge.net/projects/boost/files/boost/).

* Unpack the package content (*e.g.* below boost\_1\_44\_0/...) into ...\hypertable\deps\boost\.

* Go to ...\hypertable\deps\boost\ and run build-win32.bat or build-x64.bat (Be patient with the build). After successful build the intermediate files located in ...\hypertable\build\deps\boost could be deleted.


###Download and install berkeley db###

* Download the latest berkeley db from [Oracle](http://www.oracle.com/technetwork/database/berkeleydb/downloads/index.html).

* Unpack the package content (*e.g.* below db-5.1.19/...) into ...\hypertable\deps\db\.

* Run ...\hypertable\deps\db\libdb.js. (Note: libdb will be built within the regular hypertable solution.)


###Build hypertable servers, clients and tools###

* Open solution ...\hypertable\hypertable.sln with Microsoft Visual Studio 2010 and build the required configuration(s).


MANAGE HYPERTABLE SERVERS
=========================

Use Hypertable.Service.exe (...\hypertable\dist\\[Win32|x64]\\[Release|Debug]) to manage hypertable servers. Run Hypertable.Service.exe --help (or --help-config) for a complete list of available options and configuration settings.

Copy or update the required config files (hypertable.cfg, METADATA.xml and RS_METRICS.xml) from ...\hypertable\conf to ...\hypertable\dist\\[Win32|x64]\\[Release|Debug]\conf.

Using the default configuration the database data root directory (Hypertable.DataDirectory configuration paramter) will be ...\ProgrammData\Hypertable or ...\All Users\Application Data\Hypertable


###Run hypertable servers as Windows service###

Hypertable.Service.exe will ask for elevation if running on Windows Vista or Windows 7 with UAC enabled:

* Install hypertable service (the additional arguments will be passed to the servers on service startup):

  Hypertable.Service --install-service [--service-name] [additional arguments]

* Start hypertable service:

  Hypertable.Service --start-service [--service-name]

* Stop hypertable service:

  Hypertable.Service --stop-service [--service-name]

* Uninstall hypertable service:

  Hypertable.Service --uninstall-service [--service-name]


###Run hypertable servers in foreground###

The servers will run under the logged on windows account.

* Start servers in command shell (use ctrl+c to stop the servers):

  Hypertable.Service --join-servers [additional arguments]

* Start servers and terminate:

  Hypertable.Service --start-servers [--create-console] [additional arguments]

* Stop running servers:

  Hypertable.Service --stop-servers

* Kill running servers:

  Hypertable.Service --kill-servers


###Run hypertable servers in the build environment###

* Start servers:

  ...\hypertable\bin\start-all-servers [x86|x64] [debug] [additional arguments]

* Stop servers:

  ...\hypertable\bin\stop-all-servers [x86|x64] [debug] [additional arguments]


REDISTRIBUTE HYPERTABLE FOR WINDOWS
===================================

* Install the Microsoft Visual C++ 2010 Redistributable Package [x86|x64] on the target machine.

* Install the required config files (hypertable.cfg, METADATA.xml and RS_METRICS.xml) to [hypertable install directory]\conf.