/*
 * Copyright (C) 2007-2015 Hypertable, Inc.
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or any later version.
 *
 * Hypertable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/** @file
 * Retrieves system information (hardware, installation directory, etc)
 */

#include <Common/Compat.h>
#include <Common/FileUtils.h>
#include <Common/Logger.h>
#include <Common/Path.h>
#include <Common/SystemInfo.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <iostream>
#include <vector>

#ifdef _WIN32
#include <winioctl.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#endif

using namespace Hypertable;
using namespace std;
using namespace boost::filesystem;

string System::install_dir;
string System::exe_name;

#ifndef _WIN32

long System::tm_gmtoff;
string System::tm_zone;

#endif

bool   System::ms_initialized = false;
mutex  System::ms_mutex;

String System::locate_install_dir(const char *argv0) {
  lock_guard<mutex> lock(ms_mutex);
  return _locate_install_dir(argv0);
}

String System::_locate_install_dir(const char *argv0) {

  if (!install_dir.empty())
    return install_dir;

  exe_name = Path(argv0).filename().generic_string();

  Path exepath(proc_info().exe);

  // Detect install_dir/bin/exe_name: assumed install layout
#ifndef _WIN32
  if (exepath.parent_path().filename() == "bin")
    install_dir = exepath.parent_path().parent_path().string();
  else
#endif
    install_dir = exepath.parent_path().string();

#ifdef _WIN32
  boost::trim_right_if(install_dir, boost::is_any_of("/\\"));
#endif
  return install_dir;
}

void System::_init(const String &install_directory) {

  if (install_directory.empty()) {
    install_dir = _locate_install_dir(proc_info().exe.c_str());
  }
  else {
    // set installation directory
    install_dir = install_directory;
    while (boost::ends_with(install_dir, "/"))
    install_dir = install_dir.substr(0, install_dir.length()-1);
  }

  if (exe_name.empty())
    exe_name = Path(proc_info().args[0]).filename().generic_string();

  // initialize logging system
  Logger::initialize(exe_name);
}

void System::initialize_tm(struct tm *tmval) {
  memset(tmval, 0, sizeof(struct tm));
  tmval->tm_isdst = -1;
#if !defined(__sun__) && !defined(_WIN32)
  tmval->tm_gmtoff = System::tm_gmtoff;
  tmval->tm_zone = (char *)System::tm_zone.c_str();
#endif
}

int32_t System::get_processor_count() {
  return cpu_info().total_cores;
}

namespace {
  int32_t drive_count = 0;
}

int32_t System::get_drive_count() {
  if (drive_count > 0)
    return drive_count;

#ifndef _WIN32

#if defined(__linux__)
  String device_regex = "sd[a-z][a-z]?|hd[a-z][a-z]?";
#elif defined(__APPLE__)
  String device_regex = "disk[0-9][0-9]?";
#else
  ImplementMe;
#endif

  vector<struct dirent> listing;

  FileUtils::readdir("/dev", device_regex, listing);

  drive_count = listing.size();

#else

  char deviceName[MAX_PATH];
  HANDLE dev;
  DISK_GEOMETRY driveInfo;
  DWORD dwResult;
  for (int i = 0; i < 64; i++) {
    _snprintf(deviceName, sizeof(deviceName), "\\\\.\\PhysicalDrive%d", i);
    dev = CreateFileA(deviceName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (dev != INVALID_HANDLE_VALUE) {
      if (DeviceIoControl(dev, IOCTL_DISK_GET_DRIVE_GEOMETRY, 0, 0, &driveInfo, sizeof(driveInfo), &dwResult, 0)) {
        if (driveInfo.MediaType == FixedMedia)
          ++drive_count;
      }
      CloseHandle(dev);
    }
  }

  // check volumes if there are no physical drives
  if (drive_count == 0) {
    char volumeName[MAX_PATH];
    HANDLE fh = FindFirstVolumeA(volumeName, ARRAYSIZE(volumeName));
    if (fh != INVALID_HANDLE_VALUE) {
      do {
        if (GetDriveTypeA(volumeName) == DRIVE_FIXED) {
          PathRemoveBackslashA(volumeName);
          // is USB device
          dev = CreateFileA(volumeName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
          if (dev != INVALID_HANDLE_VALUE) {
            DWORD dwOutBytes = 0;
            STORAGE_PROPERTY_QUERY query;
            query.PropertyId = StorageDeviceProperty;
            query.QueryType = PropertyStandardQuery;

            char outBuf[1024];
            PSTORAGE_DEVICE_DESCRIPTOR pDevDesc = (PSTORAGE_DEVICE_DESCRIPTOR)outBuf;
            pDevDesc->Size = sizeof(outBuf);
            if (DeviceIoControl(dev, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), pDevDesc, pDevDesc->Size, &dwOutBytes, 0)) {
              if (pDevDesc->BusType != BusTypeUsb) 
                ++drive_count;
            }
            else {
              ++drive_count; // assume none USB
            }
          }
          CloseHandle(dev);
        }
      }
      while (FindNextVolumeA(fh, volumeName, ARRAYSIZE(volumeName)));
      FindVolumeClose(fh);
    }
  }

#endif

  return drive_count;
}

int32_t System::get_pid() {
  return proc_info().pid;
}
