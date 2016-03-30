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

#include "Common/Compat.h"

#include <Hyperspace/Config.h>
#include <Hyperspace/ServerConnectionHandler.h>
#include <Hyperspace/ServerKeepaliveHandler.h>
#include <Hyperspace/Master.h>

#include <AsyncComm/ApplicationQueue.h>
#include <AsyncComm/Comm.h>
#include <AsyncComm/ConnectionHandlerFactory.h>

#include <Common/Init.h>
#include <Common/Error.h>
#include <Common/InetAddr.h>
#include <Common/SleepWakeNotifier.h>
#include <Common/System.h>
#include <Common/Usage.h>

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

using namespace Hyperspace;
using namespace Hypertable;
using namespace Config;
using namespace std;

/*
 * Handler factory for Hyperspace master
 */
class HandlerFactory : public ConnectionHandlerFactory {

public:
  HandlerFactory(ApplicationQueuePtr &app_queue, MasterPtr &master)
    : m_app_queue(app_queue), m_master(master) { }

  virtual void get_instance(DispatchHandlerPtr &dhp) {
    dhp = make_shared<ServerConnectionHandler>(m_app_queue, m_master);
  }

private:
  ApplicationQueuePtr m_app_queue;
  MasterPtr m_master;
};


int main(int argc, char **argv) {
  #ifdef _WIN32
  ServerLaunchEvent server_launch_event;
  #endif

  typedef Cons<HyperspaceMasterPolicy, DefaultServerPolicy> AppPolicy;

  try {
    init_with_policy<AppPolicy>(argc, argv);
    HT_NOTICE("Starting hyperspace");

    Comm *comm = Comm::instance();
    ConnectionManagerPtr conn_mgr = make_shared<ConnectionManager>(comm);
    ServerKeepaliveHandlerPtr keepalive_handler;
    ApplicationQueuePtr app_queue;
    MasterPtr master =
      make_shared<Master>(conn_mgr, properties, keepalive_handler, app_queue);
    function<void()> sleep_callback = [master]() -> void {master->handle_sleep();};
    function<void()> wakeup_callback = [master]() -> void {master->handle_wakeup();};
    SleepWakeNotifier sleep_wake_notifier(sleep_callback, wakeup_callback);
    uint16_t port = has("port") ? get_i16("port") : get_i16("Hyperspace.Replica.Port");
    CommAddress local_addr = InetAddr(INADDR_ANY, port);
    ConnectionHandlerFactoryPtr hf(new HandlerFactory(app_queue, master));
    comm->listen(local_addr, hf);

    // give hyperspace message higher priority if possible
    comm->create_datagram_receive_socket(local_addr, 0x10, keepalive_handler);

    // set up maintenance timer
    uint32_t maintenance_interval = get_i32("Hyperspace.Maintenance.Interval");
    DispatchHandlerPtr maintenance_dhp;
    int error;

    hf->get_instance(maintenance_dhp);
    if ((error = comm->set_timer(maintenance_interval, maintenance_dhp)) != Error::OK)
      HT_FATALF("Problem setting timer - %s", Error::get_text(error));

    #ifdef _WIN32
    server_launch_event.set_event();
    #endif

    app_queue->join();

    HT_NOTICE("Exiting hyperspace");
    Logger::get()->set_level(Logger::Priority::FATAL);
  }
  catch (Exception &e) {
    HT_ERROR_OUT << e << HT_END;
    return 1;
  }
  return 0;
}
