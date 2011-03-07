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
#include "ServerUtils.h"
#include "ServiceUtils.h"
#include "Config.h"

#include "Common/Logger.h"
#include "Common/Config.h"
#include "Common/ProcessUtils.h"
#include "Common/ServerLaunchEvent.h"
#include "DfsBroker/Lib/Client.h"
#include "Hypertable/Lib/RangeServerClient.h"
#include "Hypertable/Lib/MasterClient.h"

using namespace Hypertable;

#define WINAPI_ERROR( msg ) \
  { \
    DWORD err = GetLastError(); \
    HT_ERRORF(msg, winapi_strerror(err)); \
    SetLastError(err); \
  }

namespace {

  static ServiceUtils::ShutdownEvent* shutdown_event = 0;

  static BOOL console_ctrl_handler(DWORD) {
    if (shutdown_event)
      shutdown_event->set_event();
    else
      HT_ERRORF("console_ctrl_handler - invalid event handle");
    return TRUE;
  }

}

void ServerUtils::start_servers() {
  HT_NOTICE("Start servers");
  servers_t servers;
  get_servers(servers);
  launched_servers_t launched_servers;
  if (start(servers, Config::server_args().c_str(), Config::wait(), launched_servers, 0))
    close_handles(launched_servers);
}

void ServerUtils::join_servers() {
  HT_NOTICE("Join servers");
  HT_EXPECT(!shutdown_event, Error::FAILED_EXPECTATION);
  shutdown_event = ServiceUtils::create_shutdown_event();
  if (shutdown_event) {
    if (SetConsoleCtrlHandler((PHANDLER_ROUTINE) console_ctrl_handler, TRUE)) {
      join_servers(shutdown_event->handle(), Config::wait());
      HT_NOTICE("Servers stopped");
    }
    else
      WINAPI_ERROR("SetConsoleCtrlHandler failed - %s");
    delete shutdown_event;
    shutdown_event = 0;
  }
}

bool ServerUtils::join_servers(HANDLE shutdown_event, DWORD timeout_ms, Notify* notify) {
  bool joined = false;
  servers_t servers;
  get_servers(servers);
  if (!servers.empty()) {
    bool shutdown = false;

    while (!shutdown) {
      launched_servers_t launched_servers;
      if (start(servers, Config::server_args().c_str(), timeout_ms, launched_servers, notify)) {
        HT_NOTICE("Servers started");
        joined = true;
        if (notify)
          notify->servers_joined();
        DWORD num_wait_handles = launched_servers.size() + 1;
        HANDLE* wait_handles = new HANDLE[num_wait_handles];
        int n = 0;
        foreach(const launched_server_t& launched_server, launched_servers)
          wait_handles[n++] = launched_server.pi.hProcess;
        wait_handles[n] = shutdown_event;

        while (!shutdown) {
          DWORD signaled = WaitForMultipleObjects(num_wait_handles, wait_handles, FALSE, INFINITE);
          if (signaled - WAIT_OBJECT_0 == num_wait_handles - 1 || signaled == WAIT_FAILED) {
            if (signaled == WAIT_FAILED)
              WINAPI_ERROR("WaitForMultipleObjects failed - %s")
            // shutdown
            HT_NOTICE("Shutdown servers");
            if (notify)
              notify->servers_shutdown_pending();
            stop(launched_servers, timeout_ms, notify);
            shutdown = true;
          }
          else {
            // restart server
            launched_server_t& launched_server = launched_servers[signaled - WAIT_OBJECT_0];
            close_handles(launched_server);
            HT_NOTICEF("Restart %s", server_exe_name(launched_server.server).c_str());
            if (start(launched_server.server, launched_server.args.c_str(), timeout_ms, launched_server, notify))
              wait_handles[signaled - WAIT_OBJECT_0] = launched_server.pi.hProcess;
            else {
              // shutdown and restart all servers
              stop(launched_servers, timeout_ms, notify);
              HT_NOTICE("Restart all servers");
              break;
            }
          }
        }
        delete [] wait_handles;
      }
    }
    if (joined && notify)
      notify->servers_shutdown();
  }
  return joined;
}

void ServerUtils::stop_servers() {
  HT_NOTICE("Shutdown servers");
  servers_t servers;
  get_servers(servers);
  stop(servers, Config::wait(), 0);
}

void ServerUtils::kill_servers() {
  HT_NOTICE("Kill servers");
  servers_t servers;
  get_servers(servers);
  kill(servers, Config::wait());
}

void ServerUtils::get_servers(servers_t& servers) {
  HT_EXPECT(Config::properties, Error::FAILED_EXPECTATION);
  servers.clear();

  #define INSERT_SERVER(server, prop) \
    if (Config::properties->get_bool(prop)) \
      servers.insert(server);

  INSERT_SERVER(dfsBroker       , Config::cfg_dfsbroker)
  INSERT_SERVER(hyperspaceMaster, Config::cfg_hyperspace)
  INSERT_SERVER(hypertableMaster, Config::cfg_hypertable)
  INSERT_SERVER(rangeServer     , Config::cfg_rangeserver)
  INSERT_SERVER(thriftBroker    , Config::cfg_thriftbroker)

  #undef INSERT_SERVER
}

bool ServerUtils::start(const servers_t& servers, const char* args, DWORD timeout_ms, launched_servers_t& launched_servers, Notify* notify) {
  launched_servers.clear();
  for (servers_t::const_iterator it = servers.begin(); it != servers.end(); ++it ) {
    launched_server_t launched_server;
    if (!start(*it, args, timeout_ms, launched_server, notify)) {
      // shutdown already launched servers
      stop(launched_servers, timeout_ms, notify);
      return false;
    }
    launched_servers.push_back(launched_server);
    Sleep(200);
  }
  return true;
}

bool ServerUtils::start(server_t server, const char* args, DWORD timeout_ms, launched_server_t& launched_server, Notify* notify) {
  HT_EXPECT(!System::install_dir.empty(), Error::FAILED_EXPECTATION);
  HT_EXPECT(Config::properties, Error::FAILED_EXPECTATION);
  launched_server.server = server;
  launched_server.args = args ? args : "";

  String exe_name = server_exe_name(server);
  if (args && *args) {
    exe_name.append(" ");
    exe_name.append(args);
  }
  if (Config::properties->defaulted("logging-level"))
    exe_name.append(" -l notice");

  HT_NOTICEF("Starting %s", exe_name.c_str());
  if (notify)
    notify->server_start_pending(server);
  bool launched = false;
  String cmd = System::install_dir + "\\" + exe_name.c_str();

  STARTUPINFO si;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(STARTUPINFO);

  String logfile;
  if (!Config::create_console()) {
    si.dwFlags |= STARTF_USESTDHANDLES;
    logfile = Config::logging_directory();
    if (!logfile.empty()) {
      logfile += "\\" + server_log_name(server);
      HT_INFOF("Redirect to %s", logfile.c_str());
    }
  }
  else
    HT_INFO("Creating new console");

  if (ProcessUtils::create(cmd.c_str(), Config::create_console() ? CREATE_NEW_CONSOLE : CREATE_NEW_PROCESS_GROUP,
                           si, logfile.c_str(), true, launched_server.pi, launched_server.logfile)) {
    ServerLaunchEvent server_launch_event(launched_server.pi.dwProcessId);
    if (!(launched = server_launch_event.wait(timeout_ms << 1))) { // give enough time to start
      HT_ERRORF("Launching %s has been timed out", exe_name.c_str());
      HT_NOTICEF("Killing %s (%d)", server_exe_name(server).c_str(), launched_server.pi.dwProcessId);
      ProcessUtils::kill(launched_server.pi.dwProcessId, timeout_ms);
    }
    else if (notify)
      notify->server_started(server);
  }
  else {
    HT_ERRORF("Launching %s failed", exe_name.c_str());
  }
  return launched;
}

void ServerUtils::stop(servers_t servers, DWORD timeout_ms, Notify* notify) {
  // hypertable master will shutdown the range servers as well,
  // let's skip range server shutdown if the master is available
  bool kill_rangeservers_on_failure = false;
  if (servers.count(hypertableMaster) && servers.count(rangeServer)) {
    if (has_master(timeout_ms)) {
      servers.erase(rangeServer);
      kill_rangeservers_on_failure = true;
    }
  }
  bool killed;
  for (servers_t::const_reverse_iterator it = servers.rbegin(); it != servers.rend(); ++it ) {
    stop(*it, 0, timeout_ms, killed, notify);
    // usually hypertable master will shutdown the range servers,
    // on failure let's kill all range servers on this node
    if (kill_rangeservers_on_failure && killed && *it == hypertableMaster)
      kill(rangeServer, timeout_ms);
  }
}

void ServerUtils::stop(launched_servers_t& launched_servers, DWORD timeout_ms, Notify* notify) {
  // hypertable master will shutdown the range servers as well,
  // let's skip range server shutdown if the master is available
  bool kill_rangeservers_on_failure = false;
  int range_server = launched_servers.size() - 1;
  for (; range_server >= 0; --range_server)
    if (launched_servers[range_server].server == rangeServer)
      break;
  if (range_server >= 0) {
    for (launched_servers_t::reverse_iterator it = launched_servers.rbegin(); it != launched_servers.rend(); ++it) {
      if ((*it).server == hypertableMaster) {
        if (has_master(timeout_ms)) {
          close_handles(launched_servers[range_server]);
          kill_rangeservers_on_failure = true;
          break;
        }
      }
    }
  }
  bool killed;
  for (launched_servers_t::reverse_iterator it = launched_servers.rbegin(); it != launched_servers.rend(); ++it) {
    if ((*it).pi.hProcess != INVALID_HANDLE_VALUE) {
      stop((*it).server, (*it).pi.dwProcessId, timeout_ms, killed, notify);
      close_handles(*it);
      // usually hypertable master will shutdown the range servers,
      // on failure let's kill all range servers on this node
      if (kill_rangeservers_on_failure && killed && (*it).server == hypertableMaster)
        kill(rangeServer, timeout_ms);
    }
  }
}

void ServerUtils::stop(server_t server, DWORD pid, DWORD timeout_ms, bool& killed, Notify* notify) {
  killed = false;
  if (notify)
    notify->server_stop_pending(server);
  bool shutdown_done = false;
  switch (server) {
    case dfsBroker:
      shutdown_done = shutdown_dfsbroker(pid, timeout_ms);
      break;
    case hypertableMaster:
      shutdown_done = shutdown_master(pid, timeout_ms);
      break;
    case rangeServer:
      shutdown_done = shutdown_rangeserver(pid, timeout_ms);
      break;
    default:
      break;
  }
  if (!shutdown_done) {
    if (pid) {
      HT_NOTICEF("Killing %s (%d)", server_exe_name(server).c_str(), pid);
      ProcessUtils::kill(pid, timeout_ms);
    }
    else
      kill(server, timeout_ms);
    killed = true;
  }
  if (notify)
    notify->server_stopped(server);
}

void ServerUtils::kill(const servers_t& servers, DWORD timeout_ms) {
  for (servers_t::const_reverse_iterator it = servers.rbegin(); it != servers.rend(); ++it )
    kill(*it, timeout_ms);
}

void ServerUtils::kill(server_t server, DWORD timeout_ms) {
  std::vector<DWORD> pids;
  find(server, pids);
  const String& exe_name = server_exe_name(server).c_str();
  foreach(DWORD pid, pids) {
    HT_NOTICEF("Killing %s (%d)", exe_name.c_str(), pid);
    ProcessUtils::kill(pid, timeout_ms);
  }
}

void ServerUtils::find(server_t server, std::vector<DWORD>& pids) {
  const String& exe_name = server_exe_name(server).c_str();
  ProcessUtils::find(exe_name.c_str(), pids);
}

String ServerUtils::server_exe_name(server_t server) {
  return server_name(server) + ".exe";
}

String ServerUtils::server_log_name(server_t server) {
  return server_name(server) + ".log";
}

const String& ServerUtils::server_name(server_t server) {
  static String names[] = {
    "Hypertable.LocalBroker",
    "Hyperspace.Master",
    "Hypertable.Master",
    "Hypertable.RangeServer",
    "Hypertable.ThriftBroker",
  };
  return names[server];
}

bool ServerUtils::shutdown_dfsbroker(DWORD pid, DWORD timeout_ms) {
  HT_EXPECT(Config::properties, Error::FAILED_EXPECTATION);
  String host = Config::properties->get_str("DfsBroker.Host");
  uint16_t port = Config::properties->get_i16("DfsBroker.Port");
  try {
    std::vector<DWORD> pids;
    if (!pid)
      find(dfsBroker, pids);
    {
      DfsBroker::ClientPtr client = new DfsBroker::Client(host, port, timeout_ms);
      HT_NOTICEF("Shutdown DFS broker (%s)", InetAddr(host, port).format().c_str());
      DispatchHandlerSynchronizer sync_handler;
      client->shutdown(0, &sync_handler);
      EventPtr event_ptr;
      sync_handler.wait_for_reply(event_ptr);
    }
    if (pid) {
      if (!ProcessUtils::join(pid, timeout_ms))
        Sleep(500);
    }
    else {
      if (!ProcessUtils::join(pids, false, timeout_ms))
        Sleep(500);
    }
    return true;
  }
  catch (Exception& e) {
    HT_ERROR_OUT << e << HT_END;
  }
  return false;
}

bool ServerUtils::shutdown_master(DWORD pid, DWORD timeout_ms) {
  HT_EXPECT(Config::properties, Error::FAILED_EXPECTATION);
  try {
    std::vector<DWORD> pids;
    if (!pid)
      find(hypertableMaster, pids);
    {
      Comm* comm = Comm::instance();
      ConnectionManagerPtr conn_mgr = new ConnectionManager(comm);
      Hyperspace::SessionPtr hyperspace = new Hyperspace::Session(comm, Config::properties);

      if (!hyperspace->wait_for_connection(timeout_ms)) {
        conn_mgr->remove_all();
        HT_THROW(Error::REQUEST_TIMEOUT, "Unable to connect to hyperspace");
      }
      ApplicationQueuePtr app_queue = new ApplicationQueue(1);
      String toplevel_dir = Config::properties->get_str("Hypertable.Directory");
      boost::trim_if(toplevel_dir, boost::is_any_of("/"));
      toplevel_dir = String("/") + toplevel_dir;

      MasterClientPtr master = new MasterClient(conn_mgr, hyperspace, toplevel_dir, timeout_ms, app_queue);
      master->set_verbose_flag(Config::properties->get_bool("verbose"));
      master->initiate_connection(0);
      if (!master->wait_for_connection(timeout_ms)) {
        conn_mgr->remove_all();
        HT_THROW(Error::REQUEST_TIMEOUT, "Unable to connect to hypertable master");
      }
      HT_NOTICEF("Shutdown hypertable master (%s)", hyperspace->get_master_addr().format().c_str());
      Timer timer(timeout_ms, true);
      master->shutdown(&timer);
      conn_mgr->remove_all();
    }
    if (pid) {
      if (!ProcessUtils::join(pid, timeout_ms))
        Sleep(500);
    }
    else {
      if (!ProcessUtils::join(pids, false, timeout_ms))
        Sleep(500);
    }
    return true;
  }
  catch (Exception& e) {
    HT_ERROR_OUT << e << HT_END;
  }
  return false;
}

bool ServerUtils::has_master(DWORD timeout_ms) {
  HT_EXPECT(Config::properties, Error::FAILED_EXPECTATION);
  try {
    Comm* comm = Comm::instance();
    ConnectionManagerPtr conn_mgr = new ConnectionManager(comm);
    Hyperspace::SessionPtr hyperspace = new Hyperspace::Session(comm, Config::properties);

    if (!hyperspace->wait_for_connection(timeout_ms)) {
      conn_mgr->remove_all();
      return false;
    }
    ApplicationQueuePtr app_queue = new ApplicationQueue(1);
    String toplevel_dir = Config::properties->get_str("Hypertable.Directory");
    boost::trim_if(toplevel_dir, boost::is_any_of("/"));
    toplevel_dir = String("/") + toplevel_dir;

    MasterClientPtr master = new MasterClient(conn_mgr, hyperspace, toplevel_dir, timeout_ms, app_queue);
    master->set_verbose_flag(Config::properties->get_bool("verbose"));
    master->initiate_connection(0);
    if (!master->wait_for_connection(timeout_ms)) {
      conn_mgr->remove_all();
      return false;
    }
    Timer timer(timeout_ms, true);
    master->status(&timer);
    conn_mgr->remove_all();
    HT_INFOF("Hypertable master found at %s", hyperspace->get_master_addr().format().c_str());
    return true;
  }
  catch (Exception&) {
  }
  return false;
}

bool ServerUtils::shutdown_rangeserver(DWORD pid, DWORD timeout_ms) {
  HT_EXPECT(Config::properties, Error::FAILED_EXPECTATION);
  try {
    std::vector<DWORD> pids;
    if (!pid)
      find(rangeServer, pids);
    {
      Comm* comm = Comm::instance();
      ConnectionManagerPtr conn_mgr = new ConnectionManager(comm);
      InetAddr addr(Config::properties->get_str("rs-host"), Config::properties->get_i16("rs-port"));
      conn_mgr->add(addr, timeout_ms, "Range Server");
      if (!conn_mgr->wait_for_connection(addr, timeout_ms)) {
        conn_mgr->remove_all();
        HT_THROWF(Error::REQUEST_TIMEOUT, "Unable to connect to range server (%s)", addr.format().c_str());
      }
      RangeServerClientPtr client = new RangeServerClient(comm, timeout_ms);
      HT_NOTICEF("Shutdown range server (%s)", addr.format().c_str());
      client->shutdown(addr);
      conn_mgr->remove_all();
    }
    if (pid) {
      if (!ProcessUtils::join(pid, timeout_ms))
        Sleep(500);
    }
    else {
      if (!ProcessUtils::join(pids, false, timeout_ms))
        Sleep(500);
    }
    return true;
  }
  catch (Exception& e) {
    HT_ERROR_OUT << e << HT_END;
  }
  return false;
}

void ServerUtils::close_handles(launched_servers_t& launched_servers) {
  for (launched_servers_t::reverse_iterator it = launched_servers.rbegin(); it != launched_servers.rend(); ++it)
    close_handles(*it);
}

void ServerUtils::close_handles(launched_server_t& launched_server) {
  if (launched_server.pi.hThread != INVALID_HANDLE_VALUE) {
    if (!CloseHandle(launched_server.pi.hThread))
      WINAPI_ERROR("CloseHandle failed - %s")
    launched_server.pi.hThread = INVALID_HANDLE_VALUE;
  }
  if (launched_server.pi.hProcess != INVALID_HANDLE_VALUE) {
    if (!CloseHandle(launched_server.pi.hProcess))
      WINAPI_ERROR("CloseHandle failed - %s")
    launched_server.pi.hProcess = INVALID_HANDLE_VALUE;
  }
  if (launched_server.logfile != INVALID_HANDLE_VALUE) {
    if (!CloseHandle(launched_server.logfile))
      WINAPI_ERROR("CloseHandle failed - %s")
    launched_server.logfile = INVALID_HANDLE_VALUE;
  }
}