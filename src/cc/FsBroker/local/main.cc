/*
 * Copyright (C) 2007-2016 Hypertable, Inc.
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

#include <Common/Compat.h>

#include "LocalBroker.h"

#include <FsBroker/Lib/Config.h>
#include <FsBroker/Lib/ConnectionHandlerFactory.h>

#include <AsyncComm/ApplicationQueue.h>
#include <AsyncComm/Comm.h>
#include <AsyncComm/DispatchHandler.h>

#include <Common/Config.h>
#include <Common/Error.h>
#include <Common/FileUtils.h>
#include <Common/InetAddr.h>
#include <Common/Init.h>
#include <Common/Usage.h>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>

extern "C" {
#include <poll.h>
#include <sys/types.h>
#include <unistd.h>
}

#ifdef _WIN32
#include "Common/ServerLaunchEvent.h"
#endif

using namespace Hypertable;
using namespace Hypertable::FsBroker;
using namespace Config;
using namespace std;

namespace {

  struct AppPolicy : Policy {
    static void init_options() {
      cmdline_desc().add_options()
        ("root", str()->default_value("fs/local"), "root directory for local "
         "broker (if relative, it's relative to the Hypertable data directory")
        ;
      alias("port", "FsBroker.Local.Port");
      alias("root", "FsBroker.Local.Root");
      alias("workers", "FsBroker.Local.Workers");
      alias("reactors", "FsBroker.Local.Reactors");
    }
  };

  typedef Meta::list<AppPolicy, FsBrokerPolicy, DefaultCommPolicy> Policies;

} // local namespace


int main(int argc, char **argv) {
  #ifdef _WIN32
  ServerLaunchEvent server_launch_event;
  #endif

  try {
    init_with_policies<Policies>(argc, argv);
    HT_NOTICE("Starting local broker");

    int port;

    int worker_count = get_i32("workers");

    if (has("port"))
      port = get_i16("port");
    else
      port = get_i16("FsBroker.Port");

    // Backward compatibility
    if (has("DfsBroker.Local.Port"))
      port = get_i16("DfsBroker.Local.Port");
    else if (has("DfsBroker.Port"))
      port = get_i16("DfsBroker.Port");
    if (has("DfsBroker.Local.Workers"))
      worker_count = get_i32("DfsBroker.Local.Workers");

    Comm *comm = Comm::instance();

    ApplicationQueuePtr app_queue = make_shared<ApplicationQueue>(worker_count);
    BrokerPtr broker = make_shared<LocalBroker>(properties);
    ConnectionHandlerFactoryPtr handler_factory =
      make_shared<FsBroker::Lib::ConnectionHandlerFactory>(comm, app_queue, broker);
    InetAddr listen_addr(INADDR_ANY, port);

    comm->listen(listen_addr, handler_factory);

    #ifdef _WIN32
    server_launch_event.set_event();
    #endif

    app_queue->join();

    HT_NOTICE("Exiting local broker");
    Logger::get()->set_level(Logger::Priority::FATAL);
  }
  catch (Exception &e) {
    HT_ERROR_OUT << e << HT_END;
    return 1;
  }

  if (has("pidfile"))
    FileUtils::unlink(get_str("pidfile"));

  return 0;
}
