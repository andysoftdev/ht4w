/** -*- C++ -*-
 * Copyright (C) 2010-2013 Thalmann Software & Consulting, http://www.softdev.ch
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

#ifndef HYPERTABLE_SERVICEUTILS_H
#define HYPERTABLE_SERVICEUTILS_H

#ifndef _WIN32
#error Platform isn't supported
#endif

#include "Common/String.h"
#include "Common/ServerLaunchEvent.h"

namespace Hypertable {

  class ServiceUtils {
  public:
    typedef ServerLaunchEvent ShutdownEvent;

    static void init_service();
    static void install_service();
    static void uninstall_service();
    static void start_service();
    static void stop_service();
    static void stop_all_services();

    static ShutdownEvent* create_shutdown_event();
    static ShutdownEvent* create_shutdown_event(DWORD pid);

  private:
    enum manage_service_t {
      uninstall,
      start,
      stop
    };

    static void service_main(DWORD argc, char** argv);
    static void service_ctrl_handler(DWORD ctrl);
    static void find_services(std::vector<DWORD>& pids);
    static bool service_exists(const String& service_name);
    static bool service_status(const String& service_name, SERVICE_STATUS_PROCESS& status);
    static bool install_service(const String& service_name, const String& service_display_name, const String& exe_name, const String& service_desc, bool& access_denied);
    static bool manage_service(const String& service_name, manage_service_t ms, bool& access_denied);
    static bool self_elevate();
    static bool exec(const String& exe_name, const String& args);
  };

}

#endif // HYPERTABLE_SERVICEUTILS_H
