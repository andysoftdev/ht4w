/** -*- C++ -*-
 * Copyright (C) 2010-2014 Thalmann Software & Consulting, http://www.softdev.ch
 *
 * This file is part of ht4w.
 *
 * ht4w is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or any later version.
 *
 * Hypertable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef _WIN32
#error Platform isn't supported
#endif

#include "Common/Compat.h"
#include <winsvc.h>
#include <shellapi.h>
#include <boost/algorithm/string.hpp>
#include "ServiceUtils.h"
#include "ServerUtils.h"
#include "Config.h"

#include "Common/Logger.h"
#include "Common/System.h"
#include "Common/SystemInfo.h"
#include "Common/ProcessUtils.h"
#include "Common/SecurityUtils.h"

using namespace Hypertable;

#ifndef FILE_CACHE_FLAGS_DEFINED

#define FILE_CACHE_MAX_HARD_ENABLE      0x00000001
#define FILE_CACHE_MAX_HARD_DISABLE     0x00000002
#define FILE_CACHE_MIN_HARD_ENABLE      0x00000004
#define FILE_CACHE_MIN_HARD_DISABLE     0x00000008

#endif

#ifndef MAXSIZE_T
#define MAXSIZE_T ((SIZE_T)~((SIZE_T)0))
#endif

#define WINAPI_ERROR( msg ) \
{ \
  DWORD err = GetLastError(); \
  HT_ERRORF(msg, winapi_strerror(err)); \
  SetLastError(err); \
}

namespace {

  class Service : private ServerUtils::Notify {
  public:
    Service()
      : shutdown_event(0)
      , service_launch_event(0)
      , ssh(0)
      , waitHint(0)
      , getSystemFileCacheSize(0)
      , setSystemFileCacheSize(0)
      , min_fcs(0)
      , max_fcs(0)
      , flags_fcs(0)
    {
      ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
      ss.dwServiceSpecificExitCode = 0;

      HMODULE kernel32 = LoadLibrary("kernel32.dll");
      getSystemFileCacheSize = (fpGetSystemFileCacheSize)GetProcAddress(kernel32, "GetSystemFileCacheSize");
      setSystemFileCacheSize = (fpSetSystemFileCacheSize)GetProcAddress(kernel32, "SetSystemFileCacheSize");
    }
    ~Service() {
      if (shutdown_event)
        delete shutdown_event;
      if (service_launch_event)
        delete service_launch_event;
    }

    void run(SERVICE_STATUS_HANDLE _ssh) {
      HT_EXPECT(!shutdown_event, Error::FAILED_EXPECTATION);
      HT_EXPECT(!ssh, Error::FAILED_EXPECTATION);
      ssh = _ssh;
      shutdown_event = ServiceUtils::create_shutdown_event();
      service_launch_event = new ServerLaunchEvent();
      waitHint = Config::start_service_timeout();
      set_status(SERVICE_START_PENDING, waitHint);
      HT_INFO("Service start pending");
      adjust_max_system_cache();
      ServerUtils::join_servers(shutdown_event->handle(), static_cast<ServerUtils::Notify*>(this));
      restore_max_system_cache();
      HT_INFO("Service stopped");
      delete shutdown_event;
      shutdown_event = 0;
      delete service_launch_event;
      service_launch_event = 0;
      set_status(SERVICE_STOPPED);
      ssh = 0;
    }

    void stop() {
      if (shutdown_event) {
        set_status(SERVICE_STOP_PENDING, waitHint);
        HT_INFO("Service stop pending");
        shutdown_event->set_event();
        refresh_status();
      }
    }

  private:
    ServiceUtils::ShutdownEvent* shutdown_event;
    ServerLaunchEvent* service_launch_event;
    SERVICE_STATUS_HANDLE ssh;
    SERVICE_STATUS ss;
    DWORD waitHint;

    typedef BOOL (WINAPI *fpGetSystemFileCacheSize)(PSIZE_T lpMinimumFileCacheSize, PSIZE_T lpMaximumFileCacheSize, PDWORD lpFlags);
    typedef BOOL (WINAPI *fpSetSystemFileCacheSize)(SIZE_T MinimumFileCacheSize, SIZE_T MaximumFileCacheSize, DWORD Flags);

    fpGetSystemFileCacheSize getSystemFileCacheSize;
    fpSetSystemFileCacheSize setSystemFileCacheSize;
    SIZE_T min_fcs;
    SIZE_T max_fcs;
    DWORD flags_fcs;

    // handle notifications
    virtual void servers_joined() {
      set_status(SERVICE_RUNNING);
      HT_INFO("Service running");
      if (service_launch_event)
        service_launch_event->set_event();
    }
    virtual void server_start_pending(ServerUtils::server_t server) {
      if (ss.dwCurrentState != SERVICE_RUNNING)
        refresh_status();
    }
    virtual void server_started(ServerUtils::server_t server) {
      if (ss.dwCurrentState != SERVICE_RUNNING)
        refresh_status();
    }
    virtual void server_stop_pending(ServerUtils::server_t server) {
      if (ss.dwCurrentState != SERVICE_RUNNING)
        refresh_status();
    }
    virtual void server_stopped(ServerUtils::server_t server) {
      if (ss.dwCurrentState != SERVICE_RUNNING)
        refresh_status();
    }
    virtual void servers_shutdown_pending() {
      if (ss.dwCurrentState != SERVICE_RUNNING)
        refresh_status();
      else {
        set_status(SERVICE_STOP_PENDING, waitHint);
        HT_INFO("Service stop pending");
      }
    }
    virtual void servers_shutdown() {
      if (ss.dwCurrentState != SERVICE_RUNNING)
        refresh_status();
    }

    void refresh_status() {
      set_status(ss.dwCurrentState, waitHint);
    }

    void set_status(DWORD dwState, DWORD dwWaitHint = 0) {
      static DWORD dwCheckPoint = 1;
      ss.dwCurrentState = dwState;
      ss.dwWin32ExitCode = NO_ERROR;
      ss.dwWaitHint = dwWaitHint;

      if (dwState == SERVICE_START_PENDING)
        ss.dwControlsAccepted = 0;
      else
        ss.dwControlsAccepted = SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_SHUTDOWN;

      if (dwState == SERVICE_RUNNING || dwState == SERVICE_STOPPED)
        ss.dwCheckPoint = 0;
      else
        ss.dwCheckPoint = ++dwCheckPoint;

      SetServiceStatus( ssh, &ss );
    }

    void adjust_max_system_cache() {
      if (getSystemFileCacheSize && setSystemFileCacheSize) {
        // enable the privilege to set system file cache size
        HANDLE hToken = 0;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
          if (set_privilege(hToken, SE_INCREASE_QUOTA_NAME)) {
            // get current settings
            if ((*getSystemFileCacheSize)(&min_fcs, &max_fcs, &flags_fcs)) {
              if (!(flags_fcs & FILE_CACHE_MIN_HARD_ENABLE))
                flags_fcs |= FILE_CACHE_MIN_HARD_DISABLE;
              if (!(flags_fcs & FILE_CACHE_MAX_HARD_ENABLE))
                flags_fcs |= FILE_CACHE_MAX_HARD_DISABLE;
            }
            else if (GetLastError() != ERROR_ARITHMETIC_OVERFLOW)
              WINAPI_ERROR("GetSystemFileCacheSize failed - %s");

            bool isDefaulted;
            uint64_t max_system_file_cache = std::max(128 * Property::MiB, Config::max_system_file_cache(isDefaulted));
            if (max_system_file_cache < MAXSIZE_T) {
              // ensure the granularity to 1 MB
              SIZE_T new_max_fcs = max_system_file_cache >> 20;
              new_max_fcs <<= 20;
              if ((*setSystemFileCacheSize)(min_fcs, new_max_fcs, FILE_CACHE_MAX_HARD_ENABLE)) {
                HT_INFOF("Adjust system file cache size %dMB", (int)(new_max_fcs >> 20));
              }
              else
                WINAPI_ERROR("SetSystemFileCacheSize failed - %s");
            }
            else if (!isDefaulted)
              HT_WARNF("Unable to adjust system file cache size to %dMB (max. cache size %dMB)", (int)(max_system_file_cache >> 20), (int)(MAXSIZE_T >> 20));
          }
          CloseHandle (hToken);
        }
        else
          WINAPI_ERROR("OpenProcessToken failed - %s");
      }
    }

    void restore_max_system_cache() {
      if (getSystemFileCacheSize && setSystemFileCacheSize) {
        if (min_fcs || max_fcs || flags_fcs) {
          if ((*setSystemFileCacheSize)(min_fcs, max_fcs, flags_fcs))
            HT_INFOF("Restore system file cache size (%dMB, %dMB, flags=%d)", (int)(min_fcs >> 20), (int)(max_fcs >> 20), flags_fcs);
          else
            WINAPI_ERROR("SetSystemFileCacheSize failed - %s");
        }
      }
    }

    void purge_system_cache() {
      if (getSystemFileCacheSize && setSystemFileCacheSize) {
        if (!(*setSystemFileCacheSize)(-1, -1, 0))
          WINAPI_ERROR("SetSystemFileCacheSize failed - %s");
      }
    }

    bool set_privilege(HANDLE hToken, LPCTSTR lpszPrivilege, bool enablePrivilege = true) {
        LUID luid;
        if (!LookupPrivilegeValue(0, lpszPrivilege, &luid)) {
            WINAPI_ERROR("LookupPrivilegeValue failed - %s");
            return false;
        }
        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = enablePrivilege ? SE_PRIVILEGE_ENABLED : 0;
        // enable the privilege or disable all privileges
        if (!AdjustTokenPrivileges(hToken, 0, &tp, sizeof(TOKEN_PRIVILEGES), 0, 0)) {
            WINAPI_ERROR("AdjustTokenPrivileges failed - %s");
            return false; 
        }
        if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
            WINAPI_ERROR("The token does not have the specified privilege - %s");
            return false;
        }
        return true;
    }

  };
  static Service service;
}

void ServiceUtils::init_service() {
  String service_name = Config::service_name();
  if (!service_name.empty()) {
    SERVICE_TABLE_ENTRY st[]= {
      { const_cast<char*>(service_name.c_str()), (LPSERVICE_MAIN_FUNCTION)ServiceUtils::service_main },
      { 0, 0 }
    };

    FILE* lf = 0;
    String logfile = Config::logging_directory();
    if (!logfile.empty()) {
      logfile += "\\Hypertable.Service.log";
      lf = freopen(logfile.c_str(), "a", stdout);
      HT_INFOF("Redirect to %s", logfile.c_str());
    }
    if (!StartServiceCtrlDispatcher(st))
      WINAPI_ERROR("StartServiceCtrlDispatcher failed - %s");
    if (lf) {
      fclose(lf);
    }
  }
  else
    HT_ERROR("Invalid service name");
}

void ServiceUtils::install_service() {
  HT_NOTICE("Install service");
  String service_name = Config::service_name();
  if (!service_name.empty()) {
    if (!service_exists(service_name)) {
      char moduleFilename[MAX_PATH];
      if (GetModuleFileName(0, moduleFilename, MAX_PATH)) {
        String exe_name = format("%s %s", moduleFilename, Config::service_args().c_str());
        boost::trim_right_if(exe_name, boost::is_any_of(" "));
        bool access_denied;
        if (!install_service(service_name, Config::service_display_name(), exe_name, Config::service_desc(), access_denied) && access_denied)
          self_elevate();
        Sleep(500);
      }
      else
        WINAPI_ERROR("GetModuleFileName failed - %s");
    }
    else
      HT_NOTICEF("Service '%s' already exists", service_name.c_str());
  }
  else
    HT_ERROR("Invalid service name");
}

void ServiceUtils::uninstall_service() {
  HT_NOTICE("Uninstall service");
  String service_name = Config::service_name();
  if (!service_name.empty()) {
    SERVICE_STATUS_PROCESS status;
    if (service_status(service_name, status)) {
      if (status.dwCurrentState != SERVICE_STOPPED) {
        ShutdownEvent* shutdownEvent = create_shutdown_event(status.dwProcessId);
        if (shutdownEvent->is_existing_event()) {
          shutdownEvent->set_event();
          if (!ProcessUtils::join(status.dwProcessId, Config::stop_service_timeout()))
            HT_WARNF("Stop service '%s' has been timed out", service_name.c_str());
        }
        delete shutdownEvent;
      }
      bool access_denied;
      if (!manage_service(service_name, uninstall, access_denied) && access_denied)
        self_elevate();
      Sleep(500);
    }
    else
      HT_NOTICEF("Service '%s' does not exists", service_name.c_str());
  }
  else
    HT_ERROR("Invalid service name");
}

void ServiceUtils::start_service() {
  HT_NOTICE("Start service");
  String service_name = Config::service_name();
  if (!service_name.empty()) {
    SERVICE_STATUS_PROCESS status;
    if (service_status(service_name, status)) {
      if (status.dwCurrentState != SERVICE_RUNNING) {
        bool access_denied;
        if (!manage_service(service_name, start, access_denied) && access_denied)
          self_elevate();
        Sleep(500);
        if (service_status(service_name, status)) {
          if (status.dwCurrentState != SERVICE_STOPPED && status.dwCurrentState != SERVICE_STOP_PENDING) {
            ServerLaunchEvent service_launch_event(status.dwProcessId);
            bool timed_out;
            if (!service_launch_event.wait(Config::start_service_timeout(), timed_out)) {
              if (timed_out)
                HT_ERRORF("Launching service '%s' has been timed out", service_name.c_str());
              else
                HT_ERRORF("Launching service '%s' failed", service_name.c_str());
            }
            else
              HT_INFO("New service process has not been found");
          }
          else
            HT_ERRORF("Launching service '%s' failed", service_name.c_str());
        }
        else
          HT_NOTICEF("Service '%s' does not exists", service_name.c_str());
      }
      else
        HT_NOTICEF("Service '%s' is already running", service_name.c_str());
    }
    else
      HT_NOTICEF("Service '%s' does not exists", service_name.c_str());
  }
  else
    HT_ERROR("Invalid service name");
}

void ServiceUtils::stop_service() {
  HT_NOTICE("Stop service");
  String service_name = Config::service_name();
  if (!service_name.empty()) {
    SERVICE_STATUS_PROCESS status;
    if (service_status(service_name, status)) {
      if (status.dwCurrentState != SERVICE_STOPPED) {
        bool access_denied;
        if (!manage_service(service_name, stop, access_denied) && access_denied)
          self_elevate();
        Sleep(500);
        if (!ProcessUtils::join(status.dwProcessId, Config::stop_service_timeout()))
          HT_WARNF("Stop service '%s' has been timed out", service_name.c_str());
      }
      else
        HT_NOTICEF("Service '%s' has been already stopped", service_name.c_str());
    }
    else
      HT_NOTICEF("Service '%s' does not exists", service_name.c_str());
  }
  else
    HT_ERROR("Invalid service name");
}

void ServiceUtils::stop_all_services() {
  HT_NOTICE("Stop all services");
  std::vector<DWORD> pids;
  find_services(pids);
  for (std::vector<DWORD>::iterator it = pids.begin(); it != pids.end(); ) {
    ShutdownEvent* shutdownEvent = create_shutdown_event(*it);
    if (!shutdownEvent || !shutdownEvent->is_existing_event()) {
      delete shutdownEvent;
      it = pids.erase(it);
    }
    else {
      shutdownEvent->set_event();
      delete shutdownEvent;
      ++it;
    }
  }
  if (pids.size()) {
    if (!ProcessUtils::join(pids, true, Config::stop_service_timeout() * pids.size()))
      HT_WARN("Stop all services has been timed out");
  }
}

ServiceUtils::ShutdownEvent* ServiceUtils::create_shutdown_event() {
  return new ServerLaunchEvent("HypertableServiceShutdown");
}

ServiceUtils::ShutdownEvent* ServiceUtils::create_shutdown_event(DWORD pid) {
  return new ServerLaunchEvent("HypertableServiceShutdown", pid);
}

void ServiceUtils::service_main(DWORD argc, char** argv) {
  try {
   HT_NOTICE("Start service");

    // change DACL of the current process in order to get synchronize access rights
    if (!SecurityUtils::set_security_info(GetCurrentProcess(), WinBuiltinUsersSid, SYNCHRONIZE))
      HT_ERROR("set_security_info failed");

    // register control handler
    SERVICE_STATUS_HANDLE ssh;
    if ((ssh = RegisterServiceCtrlHandler(Config::service_name().c_str(), (LPHANDLER_FUNCTION)service_ctrl_handler)) ) {
      service.run(ssh);
      HT_NOTICE("Service stopped");
    }
    else 
      WINAPI_ERROR("RegisterServiceCtrlHandler failed - %s");
  }
  catch (Exception &e) {
    HT_ERROR_OUT << e << HT_END;
  }
}

void ServiceUtils::service_ctrl_handler(DWORD ctrl) {
  switch (ctrl) {
  case SERVICE_CONTROL_SHUTDOWN:
  case SERVICE_CONTROL_STOP:
    service.stop();
    break;
  case SERVICE_CONTROL_INTERROGATE:
    break;
  default:
    break;
  } 
}

void ServiceUtils::find_services(std::vector<DWORD>& pids) {
  ProcessUtils::find("Hypertable.Service.exe", pids);
  DWORD current_pid = GetCurrentProcessId();
  for (std::vector<DWORD>::iterator it = pids.begin(); it != pids.end(); ++it) {
    if (*it == current_pid) {
      pids.erase(it);
      break;
    }
  }
}

bool ServiceUtils::service_exists(const String& service_name) {
  SERVICE_STATUS_PROCESS status;
  return service_status(service_name, status);
}

bool ServiceUtils::service_status(const String& service_name, SERVICE_STATUS_PROCESS& status) {
  ZeroMemory(&status, sizeof(status));
  bool success = false;
  SC_HANDLE scm = OpenSCManager(0, 0, GENERIC_READ);
  if (scm) {
    SC_HANDLE scv = OpenService(scm, service_name.c_str(), GENERIC_READ);
    if (scv) {
      DWORD cb;
      if (QueryServiceStatusEx(scv, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(SERVICE_STATUS_PROCESS), &cb))
        success = true;
      else
        WINAPI_ERROR("QueryServiceStatusEx failed - %s");
      if (!CloseServiceHandle(scv))
        WINAPI_ERROR("CloseServiceHandle failed - %s");
    }
    else if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
      HT_INFOF("Service '%s' does not exists", service_name.c_str());
    else
      WINAPI_ERROR("OpenService failed - %s");
    if (!CloseServiceHandle(scm))
      WINAPI_ERROR("CloseServiceHandle failed - %s");
  }
  else
    WINAPI_ERROR("OpenSCManager failed - %s");
  return success;
}

bool ServiceUtils::install_service(const String& service_name, const String& service_display_name, const String& exe_name, const String& service_desc, bool& access_denied) {
  access_denied = false;
  bool success = false;
  SC_HANDLE scm = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE);
  if (scm) {
    SC_HANDLE scv = CreateService(scm,
                                  service_name.c_str(),
                                  service_display_name.c_str(),
                                  SERVICE_ALL_ACCESS,
                                  SERVICE_WIN32_OWN_PROCESS,
                                  SERVICE_AUTO_START,
                                  SERVICE_ERROR_NORMAL,
                                  exe_name.c_str(),
                                  0,
                                  0,
                                  "LanmanWorstation\0LanmanServer\0",
                                  0,
                                  0);
    if (scv) {
      success = true;
      if (!service_desc.empty()) {
        SERVICE_DESCRIPTION desc;
        desc.lpDescription = const_cast<char*>(service_desc.c_str());
        if (!ChangeServiceConfig2(scv, SERVICE_CONFIG_DESCRIPTION, &desc))
          WINAPI_ERROR("ChangeServiceConfig2 failed - %s");
      }
    }
    else if (GetLastError() == ERROR_ACCESS_DENIED)
      access_denied = true;
    else
      WINAPI_ERROR("CreateService failed - %s");
    if (!CloseServiceHandle(scm))
      WINAPI_ERROR("CloseServiceHandle failed - %s");
  }
  else if (GetLastError() == ERROR_ACCESS_DENIED)
    access_denied = true;
  else
    WINAPI_ERROR("OpenSCManager failed - %s");
  return success;
}

bool ServiceUtils::manage_service(const String& service_name, manage_service_t ms, bool& access_denied) {
  access_denied = false;
  bool success = false;
  SC_HANDLE scm = OpenSCManager(0, 0, GENERIC_ALL);
  if (scm) {
    SC_HANDLE scv = OpenService(scm, service_name.c_str(), GENERIC_ALL);
    if (scv) {
      switch (ms) {
      case uninstall:
        if (!DeleteService(scv)) {
          if (GetLastError() == ERROR_SERVICE_MARKED_FOR_DELETE)
            HT_NOTICEF("Service '%s' has been marked for delete", service_name.c_str());
          else
            WINAPI_ERROR("DeleteService failed - %s");
        }
        break;
      case start:
        if (!StartService(scv, 0, 0))
          WINAPI_ERROR("StartService failed - %s");
        break;
      case stop: {
        SERVICE_STATUS status;
        if (!ControlService(scv, SERVICE_CONTROL_STOP, &status))
          WINAPI_ERROR("ControlService failed - %s");
        }
        break;
      default:
        HT_ERRORF("Invalid manage service parameter (ms=%d)", ms);
        break;
      }
      if (!CloseServiceHandle(scv))
        WINAPI_ERROR("CloseServiceHandle failed - %s");
    }
    else if (GetLastError() == ERROR_ACCESS_DENIED)
      access_denied = true;
    else if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
      HT_INFOF("Service '%s' does not exists", service_name.c_str());
    else
      WINAPI_ERROR("OpenService failed - %s");
    if (!CloseServiceHandle(scm))
      WINAPI_ERROR("CloseServiceHandle failed - %s");
  }
  else if (GetLastError() == ERROR_ACCESS_DENIED)
    access_denied = true;
  else
    WINAPI_ERROR("OpenSCManager failed - %s");
  return success;
}

bool ServiceUtils::self_elevate() {
  String args;
  const ProcInfo& proc_info = System::proc_info();
  for (uint32_t i = 1; i < proc_info.args.size(); ++i) {
    args.append(proc_info.args[i]);
    args.append(" ");
  }
  boost::trim_right_if(args, boost::is_any_of(" "));
  return exec(proc_info.exe, args);
}

bool ServiceUtils::exec(const String& exe_name, const String& args) {
  SHELLEXECUTEINFO sei;
  ZeroMemory(&sei, sizeof(sei));
  sei.cbSize = sizeof(sei);
  sei.fMask = SEE_MASK_NOASYNC|SEE_MASK_FLAG_NO_UI;
  sei.lpVerb = "runas";
  sei.lpFile = exe_name.c_str();
  sei.lpParameters = args.c_str();
  sei.nShow = SW_HIDE;
  if (ShellExecuteEx(&sei))
    return true;
  else
    WINAPI_ERROR("ShellExecuteEx failed - %s");
  return false;
}