/** -*- C++ -*-
 * Copyright (C) 2011 Andy Thalmann
 *
 * This file is part of ht4w.
 *
 * ht4w is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
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

#ifndef HYPERTABLE_SERVICE_CONFIG_H
#define HYPERTABLE_SERVICE_CONFIG_H

#ifndef _WIN32
#error Platform isn't supported
#endif

#include "Common/Config.h"

namespace Hypertable { namespace Config {

  // config options
  extern const char* cfg_dfsbroker;
  extern const char* cfg_hyperspace;
  extern const char* cfg_hypertable;
  extern const char* cfg_rangeserver;
  extern const char* cfg_thriftbroker;

  // init helpers
  void init(int argc, char **argv);
  void init_service_options();
  void init_service();

  struct ServicePolicy : Config::Policy {
    static void init_options() { init_service_options(); }
    static void init() { init_service(); }
  };

  // accessors
  bool install_service();
  bool uninstall_service();
  bool start_service();
  bool stop_service();
  bool stop_all_services();
  String service_name();
  String service_display_name();
  String service_desc();

  bool start_servers();
  bool join_servers();
  bool stop_servers();
  bool kill_servers();
  bool create_console();

  String logging_directory();

  int32_t start_service_timeout();
  int32_t stop_service_timeout();
  int32_t start_server_timeout();
  int32_t stop_server_timeout();
  int32_t kill_server_timeout();
  int32_t connection_timeout();

  bool silent();

  // server arguments, returns the arguments which will be passed to the servers
  String server_args();

  // service arguments, returns the arguments which will be passed to the service
  String service_args();

} }

#endif // HYPERTABLE_SERVICE_CONFIG_H
