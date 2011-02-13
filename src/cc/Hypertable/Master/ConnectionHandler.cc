/** -*- c++ -*-
 * Copyright (C) 2008 Doug Judd (Zvents, Inc.)
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of the
 * License, or any later version.
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
#include "Common/Config.h"
#include "Common/Error.h"
#include "Common/StringExt.h"
#include "Common/Serialization.h"

#include "AsyncComm/ApplicationQueue.h"

#include "Hypertable/Lib/MasterProtocol.h"

#include "ConnectionHandler.h"
#include "EventHandlerServerLeft.h"
#include "RequestHandlerClose.h"
#include "RequestHandlerCreateTable.h"
#include "RequestHandlerDoMaintenance.h"
#include "RequestHandlerAlterTable.h"
#include "RequestHandlerDropTable.h"
#include "RequestHandlerRenameTable.h"
#include "RequestHandlerGetSchema.h"
#include "RequestHandlerStatus.h"
#include "RequestHandlerRegisterServer.h"
#include "RequestHandlerRelinquishAcknowledge.h"
#include "RequestHandlerMoveRange.h"
#include "RequestHandlerShutdown.h"
#include "RequestHandlerCreateNamespace.h"
#include "RequestHandlerDropNamespace.h"


using namespace Hypertable;
using namespace Serialization;
using namespace Error;


/**
 *
 */
ConnectionHandler::ConnectionHandler(Comm *comm, ApplicationQueuePtr &app_queue,
                                     MasterPtr &master) : m_comm(comm), m_app_queue(app_queue),
                                     m_master(master) {
  int error;
  if ((error = m_comm->set_timer(m_master->get_maintenance_interval(), this)) != Error::OK)
    HT_FATALF("Problem setting timer - %s", Error::get_text(error));
}


/**
 *
 */
void ConnectionHandler::handle(EventPtr &event) {
  ApplicationHandler *hp = 0;

  if (event->type == Event::MESSAGE) {

    //event->display()

    try {

      // sanity check command code
      if (event->header.command < 0
          || event->header.command >= MasterProtocol::COMMAND_MAX)
        HT_THROWF(PROTOCOL_ERROR, "Invalid command (%llu)",
                  (Llu)event->header.command);

      switch (event->header.command) {
      case MasterProtocol::COMMAND_CREATE_TABLE:
        hp = new RequestHandlerCreateTable(m_comm, m_master.get(), event);
        break;
      case MasterProtocol::COMMAND_DROP_TABLE:
        hp = new RequestHandlerDropTable(m_comm, m_master.get(), event);
        break;
      case MasterProtocol::COMMAND_ALTER_TABLE:
        hp = new RequestHandlerAlterTable(m_comm, m_master.get(), event);
        break;
      case MasterProtocol::COMMAND_RENAME_TABLE:
        hp = new RequestHandlerRenameTable(m_comm, m_master.get(), event);
        break;
      case MasterProtocol::COMMAND_GET_SCHEMA:
        hp = new RequestHandlerGetSchema(m_comm, m_master.get(), event);
        break;
      case MasterProtocol::COMMAND_STATUS:
        hp = new RequestHandlerStatus(m_comm, m_master.get(), event);
        break;
      case MasterProtocol::COMMAND_REGISTER_SERVER:
        hp = new RequestHandlerRegisterServer(m_comm, m_master.get(),
                                              event);
        break;
      case MasterProtocol::COMMAND_MOVE_RANGE:
        hp = new RequestHandlerMoveRange(m_comm, m_master.get(), event);
        break;
      case MasterProtocol::COMMAND_RELINQUISH_ACKNOWLEDGE:
        hp = new RequestHandlerRelinquishAcknowledge(m_comm, m_master.get(), event);
        break;
      case MasterProtocol::COMMAND_CLOSE:
        hp = new RequestHandlerClose(m_comm, m_master.get(), event);
        break;
      case MasterProtocol::COMMAND_SHUTDOWN:
        hp = new RequestHandlerShutdown(m_comm, m_master.get(), event);
        break;
      case MasterProtocol::COMMAND_CREATE_NAMESPACE:
        hp = new RequestHandlerCreateNamespace(m_comm, m_master.get(), event);
        break;
      case MasterProtocol::COMMAND_DROP_NAMESPACE:
        hp = new RequestHandlerDropNamespace(m_comm, m_master.get(), event);
        break;

      default:
        HT_THROWF(PROTOCOL_ERROR, "Unimplemented command (%llu)",
                  (Llu)event->header.command);
      }
      m_app_queue->add(hp);
    }
    catch (Exception &e) {
      ResponseCallback cb(m_comm, event);
      HT_ERROR_OUT << e << HT_END;
      std::string errmsg = format("%s - %s", e.what(), get_text(e.code()));
      cb.error(Error::PROTOCOL_ERROR, errmsg);
    }
  }
  else if (event->type == Event::DISCONNECT) {
    String location;
    if (m_master->handle_disconnect(event->addr, location)) {
      hp = new EventHandlerServerLeft(m_master, location, event);
      m_app_queue->add(hp);
    }
    HT_INFOF("%s", event->to_str().c_str());
  }
  else if (event->type == Hypertable::Event::TIMER) {
    int error;

    m_app_queue->add( new RequestHandlerDoMaintenance(m_comm, m_master.get(), event) );

    if ((error = m_comm->set_timer(m_master->get_maintenance_interval(), this)) != Error::OK)
      HT_FATALF("Problem setting timer - %s", Error::get_text(error));

  }
  else {
    HT_INFOF("%s", event->to_str().c_str());
  }

}
