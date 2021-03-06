/** -*- C++ -*-
 * Copyright (C) 2010-2016 Thalmann Software & Consulting, http://www.softdev.ch
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
#include <boost/algorithm/string.hpp>
#include "Config.h"

#include "Common/Init.h"
#include "Common/InetAddr.h"
#include "Common/System.h"
#include "Common/SystemInfo.h"
#include "Common/FileUtils.h"
#include "Common/Path.h"
#include "AsyncComm/Config.h"

namespace Hypertable { namespace Config {

const char* usage =
    "Usage: Hypertable.Service [options]\n\n"
    "Description:\n"
    "  This program is used to manage the Hypertable servers.\n\n"
    "Options";

const char* cmdline_install_service      = "install-service";
const char* cmdline_uninstall_service    = "uninstall-service";
const char* cmdline_start_service        = "start-service";
const char* cmdline_stop_service         = "stop-service";
const char* cmdline_stop_all_services    = "stop-all-services";
const char* cmdline_service_name         = "service-name";
const char* cmdline_service_display_name = "service-display-name";
const char* cmdline_start_servers        = "start-servers";
const char* cmdline_join_servers         = "join-servers";
const char* cmdline_stop_servers         = "stop-servers";
const char* cmdline_kill_servers         = "kill-servers";
const char* cmdline_no_fsbroker          = "no-fsbroker";
const char* cmdline_no_hyperspace        = "no-hyperspace";
const char* cmdline_no_hypertable        = "no-hypertable";
const char* cmdline_no_rangeserver       = "no-rangeserver";
const char* cmdline_no_thriftbroker      = "no-thriftbroker";
const char* cmdline_create_console       = "create-console";
const char* cmdline_logging_dir          = "logging-dir";
const char* cmdline_rangeserver          = "range-server";

const char* cfg_service_name             = "Hypertable.Service.Name";
const char* cfg_service_display_name     = "Hypertable.Service.DisplayName";
const char* cfg_fsbroker                 = "Hypertable.Service.FsBroker";
const char* cfg_hyperspace               = "Hypertable.Service.HyperspaceMaster";
const char* cfg_hypertable               = "Hypertable.Service.HypertableMaster";
const char* cfg_rangeserver              = "Hypertable.Service.RangeServer";
const char* cfg_thriftbroker             = "Hypertable.Service.ThriftBroker";
const char* cfg_logging_dir              = "Hypertable.Service.Logging.Directory";
const char* cfg_start_service_timeout    = "Hypertable.Service.Timeout.StartService";
const char* cfg_stop_service_timeout     = "Hypertable.Service.Timeout.StopService";
const char* cfg_start_server_timeout     = "Hypertable.Service.Timeout.StartServer";
const char* cfg_stop_server_timeout      = "Hypertable.Service.Timeout.StopServer";
const char* cfg_kill_server_timeout      = "Hypertable.Service.Timeout.KillServer";
const char* cfg_connection_timeout       = "Hypertable.Service.Timeout.Connection";
const char* cfg_minuptime_before_restart = "Hypertable.Service.MinimumUptimeBeforeRestart";
const char* cfg_priority_class           = "Hypertable.Service.Priority";
const char* cfg_max_sys_file_cache       = "Hypertable.Service.MaxSystemFileCache";
const char* cfg_max_sys_file_cache_pct   = "Hypertable.Service.MaxSystemFileCache.Percentage";

void init(int argc, char **argv) {
  typedef Meta::list<ServicePolicy, DefaultServerPolicy > Policies;
  cleanup();
  init_with_policies<Policies>(argc, argv);
}

void init_service_options() {
  // command line options
  cmdline_desc(usage).add_options()
    (cmdline_install_service, boo()->zero_tokens()->default_value(false), "Install hypertable service")
    (cmdline_uninstall_service, boo()->zero_tokens()->default_value(false), "Uninstall hypertable service")
    (cmdline_start_service, boo()->zero_tokens()->default_value(false), "Start hypertable service")
    (cmdline_stop_service, boo()->zero_tokens()->default_value(false), "Stop hypertable service")
    (cmdline_stop_all_services, boo()->zero_tokens()->default_value(false), "Stop all hypertable services")
    (cmdline_service_name, str()->default_value("Hypertable"), "Service name")
    (cmdline_service_display_name, str()->default_value("Hypertable Database Service"), "Service display name")
    (cmdline_start_servers, boo()->zero_tokens()->default_value(false), "Start servers")
    (cmdline_join_servers, boo()->zero_tokens()->default_value(false), "Start servers and join")
    (cmdline_stop_servers, boo()->zero_tokens()->default_value(false), "Stop servers")
    (cmdline_kill_servers, boo()->zero_tokens()->default_value(false), "Kill servers")
    (cmdline_no_fsbroker, boo()->zero_tokens()->default_value(false), "Exclude FS broker")
    (cmdline_no_hyperspace, boo()->zero_tokens()->default_value(false), "Exclude hyperspace master")
    (cmdline_no_hypertable, boo()->zero_tokens()->default_value(false), "Exclude hypertable master")
    (cmdline_no_rangeserver, boo()->zero_tokens()->default_value(false), "Exclude range server")
    (cmdline_no_thriftbroker, boo()->zero_tokens()->default_value(false), "Exclude thrift broker")
    (cmdline_create_console, boo()->zero_tokens()->default_value(false), "Create a console for each server")
    (cmdline_logging_dir, str()->default_value("log"), "Logging directory (if relative, it's relative to the Hypertable data directory root)")
    (cmdline_rangeserver, str()->default_value("localhost:15865"), "Range server to connect in <host:port> format");

  // aliases
  alias(cmdline_service_name, cfg_service_name);
  alias(cmdline_service_display_name, cfg_service_display_name);
  alias(cmdline_logging_dir, cfg_logging_dir);
  alias("rs-port", "Hypertable.RangeServer.Port");
}

void init_service() {
  HT_EXPECT(Config::properties, Error::FAILED_EXPECTATION);

  #define EXCLUDE_SERVER(cmd_desc, file_desc) \
    if (properties->get_bool(cmd_desc)) { \
      bool defaulted = properties->defaulted(file_desc); \
      properties->set(file_desc, false, defaulted); \
    }

  EXCLUDE_SERVER(cmdline_no_fsbroker    , cfg_fsbroker)
  EXCLUDE_SERVER(cmdline_no_hyperspace  , cfg_hyperspace)
  EXCLUDE_SERVER(cmdline_no_hypertable  , cfg_hypertable)
  EXCLUDE_SERVER(cmdline_no_rangeserver , cfg_rangeserver)
  EXCLUDE_SERVER(cmdline_no_thriftbroker, cfg_thriftbroker)

  if (!properties->get_bool(cfg_rangeserver))
    properties->set(cfg_thriftbroker, false, true);

  Endpoint e = InetAddr::parse_endpoint(get_str(cmdline_rangeserver));
  bool defaulted = properties->defaulted(cmdline_rangeserver);
  properties->set("rs-host", e.host, defaulted);
  properties->set("rs-port", e.port, !e.port || defaulted);

  if (properties->defaulted("logging-level"))
    properties->set("logging-level", String("notice"), true);

  #undef EXCLUDE_SERVER
}

bool install_service() {
  return get_bool(cmdline_install_service);
}

bool uninstall_service() {
  return get_bool(cmdline_uninstall_service);
}

bool start_service() {
  return get_bool(cmdline_start_service);
}

bool stop_service() {
  return get_bool(cmdline_stop_service);
}

bool stop_all_services() {
  return get_bool(cmdline_stop_all_services);
}

String service_name() {
  return get_str(cmdline_service_name);
}

String service_display_name() {
  String name = get_str(cmdline_service_display_name);
  if (name.empty())
    return service_name();
  return name;
}

String service_desc() {
  return "Hypertable is a high performance distributed data storage system designed to support applications requiring maximum performance, scalability, and reliability.";
}

bool start_servers() {
  return get_bool(cmdline_start_servers);
}

bool join_servers() {
  return get_bool(cmdline_join_servers);
}

bool stop_servers() {
  return get_bool(cmdline_stop_servers);
}

bool kill_servers() {
  return get_bool(cmdline_kill_servers);
}

bool create_console() {
  return get_bool(cmdline_create_console);
}

int32_t start_service_timeout() {
  return get_i32(cfg_start_service_timeout);
}

int32_t stop_service_timeout() {
  return get_i32(cfg_stop_service_timeout);
}

int32_t start_server_timeout() {
  return get_i32(cfg_start_server_timeout);
}

int32_t stop_server_timeout() {
  return get_i32(cfg_stop_server_timeout);
}

int32_t kill_server_timeout() {
  return get_i32(cfg_kill_server_timeout);
}

int32_t connection_timeout() {
  return get_i32(cfg_connection_timeout);
}

int32_t minuptime_before_restart() {
  return get_i32(cfg_minuptime_before_restart);
}

uint32_t priority_class() {
  String priority_class = get_str(cfg_priority_class);
  if (stricmp(priority_class.c_str(), "AboveNormal") == 0) {
    return ABOVE_NORMAL_PRIORITY_CLASS;
  }
  else if (stricmp(priority_class.c_str(), "High") == 0) {
    return HIGH_PRIORITY_CLASS;
  }
  return NORMAL_PRIORITY_CLASS;
}

uint64_t max_system_file_cache(bool& isDefaulted) {
  if (has(cfg_max_sys_file_cache)) {
    isDefaulted = false;
    return get_i32(cfg_max_sys_file_cache) * Property::MiB;
  }
  isDefaulted = defaulted(cfg_max_sys_file_cache_pct);
  double pct = std::max(1.0, std::min((double)get_i32(cfg_max_sys_file_cache_pct), 99.0)) / 100.0;
  return (uint64_t)(System::mem_stat().ram * Property::MiB * pct);
}

bool silent() {
  return get_bool("silent");
}

String logging_directory() {
  String logging_dir = get_str(cmdline_logging_dir);
  if (!logging_dir.empty()) {
    Path logging_path = get_str(cmdline_logging_dir);
    if (!logging_path.is_complete()) {
      Path data_path = get_str("Hypertable.DataDirectory");
      logging_path = data_path / logging_path;
    }
    logging_dir = logging_path.directory_string();
    boost::trim_right_if(logging_dir, boost::is_any_of("/\\"));

    // ensure that logging directory exists
    if (!FileUtils::mkdirs(logging_dir)) {
      logging_dir.clear();
    }
  }
  return logging_dir;
}

String server_args() {
  static const char* all_service_args[] = {
    cmdline_install_service,
    cmdline_uninstall_service,
    cmdline_start_service,
    cmdline_stop_service,
    cmdline_stop_all_services,
    cmdline_service_name,
    cmdline_service_display_name,
    cmdline_start_servers,
    cmdline_join_servers,
    cmdline_stop_servers,
    cmdline_kill_servers,
    cmdline_no_fsbroker,
    cmdline_no_hyperspace,
    cmdline_no_hypertable,
    cmdline_no_rangeserver,
    cmdline_no_thriftbroker,
    cmdline_create_console,
    cmdline_logging_dir,
    cmdline_rangeserver,

    cfg_service_name,
    cfg_service_display_name,
    cfg_fsbroker,
    cfg_hyperspace,
    cfg_hypertable,
    cfg_rangeserver,
    cfg_thriftbroker,
    cfg_logging_dir,
    cfg_start_service_timeout,
    cfg_stop_service_timeout,
    cfg_start_server_timeout,
    cfg_stop_server_timeout,
    cfg_kill_server_timeout,
    cfg_connection_timeout,
    cfg_minuptime_before_restart,
    cfg_priority_class,
    0
  };

  static String args;
  if (args.empty() ) {
    const ProcInfo& proc_info = System::proc_info();
    for (uint32_t i = 1; i < proc_info.args.size(); ++i) {
      const String& arg = proc_info.args[i];
      int skip_tokens = -1;
      for (uint32_t n = 0; all_service_args[n] && skip_tokens < 0; ++n) {
        const char* tok;
        if ((tok = strstr(arg.c_str(), all_service_args[n])) != 0 ) {
          const boost::program_options::option_description* od = cmdline_desc().find_nothrow(all_service_args[n], false);
          if (!od) {
            od = file_desc().find_nothrow(all_service_args[n], false);
          }
          if (od) {
            skip_tokens = od->semantic()->min_tokens() == 1 && *(tok + strlen(all_service_args[n])) != '=' ? 1 : 0;
          }
        }
      }
      if (skip_tokens < 0) {
        if (!args.empty())
          args.append(" ");
        args.append(arg);
      }
      else
        i += skip_tokens;
    }
  }
  return args;
}

String service_args() {
  static const char* all_service_args[] = {
    cmdline_install_service,
    cmdline_uninstall_service,
    cmdline_start_service,
    cmdline_stop_service,
    cmdline_stop_all_services,
    cmdline_service_name,
    cmdline_service_display_name,
    cmdline_start_servers,
    cmdline_join_servers,
    cmdline_stop_servers,
    cmdline_kill_servers,
    cmdline_create_console,
    cmdline_rangeserver,

    cfg_service_name,
    cfg_service_display_name,
    0
  };

  static String args;
  if (args.empty() ) {
    const ProcInfo& proc_info = System::proc_info();
    for (uint32_t i = 1; i < proc_info.args.size(); ++i) {
      const String& arg = proc_info.args[i];
      int skip_tokens = -1;
      for (uint32_t n = 0; all_service_args[n] && skip_tokens < 0; ++n) {
        const char* tok;
        if ((tok = strstr(arg.c_str(), all_service_args[n])) != 0 ) {
          const boost::program_options::option_description* od = cmdline_desc().find_nothrow(all_service_args[n], false);
          if (!od) {
            od = file_desc().find_nothrow(all_service_args[n], false);
          }
          if (od) {
            skip_tokens = od->semantic()->min_tokens() == 1 && *(tok + strlen(all_service_args[n])) != '=' ? 1 : 0;
          }
        }
      }
      if (skip_tokens < 0) {
        if (!args.empty())
          args.append(" ");
        args.append(arg);
      }
      else
        i += skip_tokens;
    }
  }
  return args;
}

} }

