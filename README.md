HOW TO BUILD HYPERTABLE FOR WINDOWS
===================================

Building Hypertable for Windows from source requires at least Microsoft Visual Studio 2010 Professional.

###Browse through or download the ht4w source###

* Browse through or download the ht4w source from [github](http://github.com/andysoftdev/ht4w).
  To download the latest sources press the 'Downloads' button and choose one of the following files, either the
  Download.tar.gz or the Download.zip.
  
* Alternatively, get the source from the repository, create a projects folder (the path must not contain any spaces) and use:

		mkdir hypertable
		cd hypertable
		git clone git://github.com/andysoftdev/ht4w.git


###Download and install boost###

* Download the latest boost version either from [boost](http://www.boost.org/users/download/)
  or from [sourceforge](http://sourceforge.net/projects/boost/files/boost/).

* Unpack the content of the package (i.e. the content of the root folder, e.g. of the boost_1_44_0\) into ht4w\deps\boost\ and run:

		cd ht4w\deps\boost
		build-win32.bat
		build-x64.bat
  When the build is successfully completed the intermediate files located in ht4w\build\deps\boost can be deleted.


###Download and install Berkeley DB###

* Download the latest Berkeley DB from [Oracle](http://www.oracle.com/technetwork/database/berkeleydb/downloads/index.html).

* Unpack the content of the package (i.e. the content of the root folder, e.g. of the db-5.2.28\) into ht4w\deps\db\ and run:

		cd ht4w\deps\db
		cscript libdb.js


###Build Hypertable servers, clients and tools###

* Open the ht4w solution (ht4w\ht4w.sln) with Microsoft Visual Studio 2010 and build its configurations. Alternatively, open the Visual Studio command prompt and type:

		cd ht4w
		msbuild ht4w.buildproj
  or, for a complete rebuild, type

		cd ht4w
		msbuild ht4w.buildproj /t:Clean;Make


MANAGE HYPERTABLE SERVERS
=========================

Use Hypertable.Service.exe (ht4w\\dist\\\[Win32|x64]\\\[Release|Debug]) to manage the Hypertable servers. For a complete list
of all available options and configuration settings run:

		Hypertable.Service.exe [--help|--help-config]

Copy or update the required config files (hypertable.cfg, METADATA.xml and RS_METRICS.xml) from ht4w\\conf to ht4w\\dist\\\[Win32|x64]\\\[Release|Debug]\\conf.


###Run Hypertable servers as a Windows service###

Hypertable.Service.exe will ask for elevation if running on Windows Vista or Windows 7 with UAC enabled:

* Install Hypertable as a Windows service (the additional arguments will be passed to the Hypertable servers on service startup):

		Hypertable.Service --install-service [--service-name] [additional arguments]

* Start Hypertable service:

		Hypertable.Service --start-service [--service-name]

* Stop Hypertable service:

		Hypertable.Service --stop-service [--service-name]

* Uninstall Hypertable service:

		Hypertable.Service --uninstall-service [--service-name]


###Run Hypertable servers in foreground###

The servers will run under the logged-on user account.

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

* Install the Microsoft Visual C++ 2010 redistributable package [x86|x64] on the target machine.

* Install the required config files (hypertable.cfg, METADATA.xml and RS_METRICS.xml) to [hypertable install directory]\\conf.