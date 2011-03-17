/** -*- C++ -*-
* Copyright (C) 2011 Andy Thalmann
*
* This file is part of Hypertable.
*
* Hypertable is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
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

#define WINAPI_ERROR( msg ) \
{ \
  DWORD err = GetLastError(); \
  HT_ERRORF(msg, winapi_strerror(err)); \
  SetLastError(err); \
}

namespace {

  class Service : private ServerUtils::Notify {
  public:
    Service() : shutdown_event(0), service_launch_event(0), ssh(0) {
      ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
      ss.dwServiceSpecificExitCode = 0;
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
      ServerUtils::join_servers(shutdown_event->handle(), static_cast<ServerUtils::Notify*>(this));
      set_status(SERVICE_STOPPED);
      HT_INFO("Service stopped");
      delete shutdown_event;
      shutdown_event = 0;
      delete service_launch_event;
      service_launch_event = 0;
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

    void set_status(DWORD dwCurrentState, DWORD dwWaitHint = 0) {
      static DWORD dwCheckPoint = 1;
      ss.dwCurrentState = dwCurrentState;
      ss.dwWin32ExitCode = NO_ERROR;
      ss.dwWaitHint = dwWaitHint;

      if (dwCurrentState == SERVICE_START_PENDING)
        ss.dwControlsAccepted = 0;
      else
        ss.dwControlsAccepted = SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_SHUTDOWN;

      if (dwCurrentState == SERVICE_RUNNING || dwCurrentState == SERVICE_STOPPED)
        ss.dwCheckPoint = 0;
      else
        ss.dwCheckPoint = ++dwCheckPoint;

      SetServiceStatus( ssh, &ss );
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

    std::ofstream* lf = 0;
    String logfile = Config::logging_directory();
    if (!logfile.empty()) {
      logfile += "\\Hypertable.Service.log";
      lf = new std::ofstream(logfile.c_str(), std::ios::out|std::ios::app);
      Logger::redirect(*lf, true);
      HT_INFOF("Redirect to %s", logfile.c_str());
    }
    if (!StartServiceCtrlDispatcher(st))
      WINAPI_ERROR("StartServiceCtrlDispatcher failed - %s");
    if (lf) {
      Logger::logger->removeAllAppenders();
      delete lf;
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
      if (GetModuleFileName(0, moduleFilename, MAX_PATH) > 0 && GetLastError() == ERROR_SUCCESS) {
        String exe_name = format("%s %s", moduleFilename, Config::server_args().c_str());
        boost::trim_right_if(exe_name, boost::is_any_of(" "));
        bool access_denied;
        if (!install_service(service_name, Config::service_display_name(), exe_name, Config::service_desc(), access_denied) && access_denied)
          self_elevate();
        Sleep(250);
        if (!service_exists(service_name))
          HT_NOTICEF("Installing service '%s' failed", service_name.c_str());
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
      Sleep(250);
      if (service_exists(service_name))
        HT_NOTICEF("Uninstalling service '%s' failed", service_name.c_str());
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
        Sleep(250);
        if (service_status(service_name, status)) {
          ServerLaunchEvent service_launch_event(status.dwProcessId);
          if (!service_launch_event.wait(Config::start_service_timeout()))
            HT_ERRORF("Launching service '%s' has been timed out", service_name.c_str());
        }
        else
          HT_INFO("New service process has not been found");
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
        Sleep(250);
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
                                  0,0,0,0,0);
    if (scv) {
      success = true;
      if (!service_desc.empty()) {
        SERVICE_DESCRIPTION desc;
        desc.lpDescription = const_cast<char*>(service_desc.c_str());
        if (!ChangeServiceConfig2(scv, SERVICE_CONFIG_DESCRIPTION, &desc))
          WINAPI_ERROR("ChangeServiceConfig2 failed - %s");
      }
      if (!CloseServiceHandle(scm))
        WINAPI_ERROR("CloseServiceHandle failed - %s");
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
        if (!DeleteService(scv))
          WINAPI_ERROR("DeleteService failed - %s");
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