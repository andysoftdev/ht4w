HOW TO BUILD HYPERTABLE FOR WINDOWS
===================================

Building Hypertable for Windows from source requires Microsoft Visual Studio 2010 Professional or better.

###Browse or get the source###

* Browse or download the source at [github](http://github.com/andysoftdev/ht4w).
  Download the latest sources by pressing the Downloads button and choosing
  Download.tar.gz or Download.zip.
  
* Or get the source from the repository, create a projects folder (the path must not
  contain any spaces) and use:

		mkdir hypertable
		cd hypertable
		git clone git://github.com/andysoftdev/ht4w.git


###Download and install boost###

* Download the latest boost version from [boost](http://www.boost.org/users/download/)
  or [sourceforge](http://sourceforge.net/projects/boost/files/boost/).

* Unpack the package content (*e.g.* below boost\_1\_44\_0\\) into ht4w\\deps\\boost\\ and run:

		cd ht4w\deps\boost
		build-win32.bat
		build-x64.bat
  After successful build the intermediate files located in ht4w\\build\\deps\\boost could be deleted.


###Download and install berkeley db###

* Download the latest berkeley db from [Oracle](http://www.oracle.com/technetwork/database/berkeleydb/downloads/index.html).

* Unpack the package content (*e.g.* below db-5.1.25\\) into ht4w\\deps\\db\\ and run:

		cd ht4w\deps\db
		cscript libdb.js
  libdb will be built within the regular ht4w solution or msbuild script.


###Build Hypertable servers, clients and tools###

* Open the ht4w solution (ht4w\\ht4w.sln) with Microsoft Visual Studio 2010 and build the solution configuration(s) or
  run Visual Studio Command Prompt and run:

		cd ht4w
		msbuild ht4w.buildproj
  or for a complete rebuild

		cd ht4w
		msbuild ht4w.buildproj /t:Clean;Make


MANAGE HYPERTABLE SERVERS
=========================

Use Hypertable.Service.exe (ht4w\\dist\\\[Win32|x64]\\\[Release|Debug]) to manage Hypertable servers. For a complete list
of available options and configuration settings run:

		Hypertable.Service.exe [--help|--help-config]

Copy or update the required config files (hypertable.cfg, METADATA.xml and RS_METRICS.xml) from ht4w\\conf to ht4w\\dist\\\[Win32|x64]\\\[Release|Debug]\\conf.

Using the default configuration the database data root directory (Hypertable.DataDirectory configuration parameter) will be ProgrammData\\Hypertable
or All Users\\Application Data\\Hypertable


###Run Hypertable servers as Windows service###

Hypertable.Service.exe will ask for elevation if running on Windows Vista or Windows 7 with UAC enabled:

* Install Hypertable service (the additional arguments will be passed to the servers on service startup):

		Hypertable.Service --install-service [--service-name] [additional arguments]

* Start Hypertable service:

		Hypertable.Service --start-service [--service-name]

* Stop Hypertable service:

		Hypertable.Service --stop-service [--service-name]

* Uninstall Hypertable service:

		Hypertable.Service --uninstall-service [--service-name]


###Run Hypertable servers in foreground###

The servers will run under the logged on windows account.

* Start servers in command shell (use ctrl+c to stop the servers):

		Hypertable.Service --join-servers [additional arguments]

* Start servers and terminate:

		Hypertable.Service --start-servers [--create-console] [additional arguments]

* Stop running servers:

		Hypertable.Service --stop-servers

* Kill running servers:

		Hypertable.Service --kill-servers


###Run Hypertable servers in the build environment###

* Start servers:

		cd ht4w\bin
		start-all-servers [x86|x64] [debug] [additional arguments]

* Stop servers:

		cd ht4w\bin
		stop-all-servers [x86|x64] [debug] [additional arguments]


REDISTRIBUTE HYPERTABLE FOR WINDOWS
===================================

* Install the Microsoft Visual C++ 2010 Redistributable Package [x86|x64] on the target machine.

* Install the required config files (hypertable.cfg, METADATA.xml and RS_METRICS.xml) to [hypertable install directory]\\conf.