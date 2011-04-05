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

#ifndef HYPERTABLE_SERVERUTILS_H
#define HYPERTABLE_SERVERUTILS_H

#ifndef _WIN32
#error Platform isn't supported
#endif

#include "Common/String.h"

namespace Hypertable {

  class ServerUtils {
  public:
    enum server_t {
      dfsBroker = 0,
      hyperspaceMaster,
      hypertableMaster,
      rangeServer,
      thriftBroker
    };

    class Notify {
    public:
      virtual void servers_joined() = 0;
      virtual void server_start_pending(server_t server) = 0;
      virtual void server_started(server_t server) = 0;
      virtual void server_stop_pending(server_t server) = 0;
      virtual void server_stopped(server_t server) = 0;
      virtual void servers_shutdown_pending() = 0;
      virtual void servers_shutdown() = 0;
    };

    static void start_servers();
    static void join_servers();
    static bool join_servers(HANDLE shutdown_event, Notify* notify = 0);
    static void stop_servers();
    static void kill_servers();

  private:
    enum {
      firstServer = dfsBroker,
      lastServer = thriftBroker
    };
    typedef std::set<server_t> servers_t;

    struct launched_server_t {
      server_t server;
      String args;
      PROCESS_INFORMATION pi;
      HANDLE logfile;

      launched_server_t() : logfile(INVALID_HANDLE_VALUE) {}
    };

    typedef std::vector<launched_server_t> launched_servers_t;

    static void get_servers(servers_t& servers);
    static bool start(const servers_t& servers, const char* args, launched_servers_t& launched_servers, Notify* notify);
    static bool start(server_t server, const char* args, launched_server_t& launched_server, Notify* notify);
    static void stop(launched_servers_t& launched_servers, Notify* notify);
    static void stop(servers_t servers, Notify* notify);
    static void stop(server_t server, DWORD pid, bool& killed, Notify* notify);
    static void kill(const servers_t& servers);
    static void kill(server_t server);
    static void find(server_t server, std::vector<DWORD>& pids);
    static String server_exe_name(server_t server);
    static String server_log_name(server_t server);
    static const String& server_name(server_t server);

    static bool shutdown_dfsbroker(DWORD pid);
    static bool shutdown_master(DWORD pid);
    static bool shutdown_rangeserver(DWORD pid);
    static void close_handles(launched_servers_t& launched_servers);
    static void close_handles(launched_server_t& launched_server);
  };

}

#endif // HYPERTABLE_SERVERUTILS_H
