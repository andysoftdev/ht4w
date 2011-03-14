/** -*- c++ -*-
 * Copyright (C) 2011 Hypertable, Inc.
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

#include "AsyncComm/ResponseCallback.h"

#include "Hypertable/Lib/MasterProtocol.h"

#include "ConnectionHandler.h"

#include "OperationAlterTable.h"
#include "OperationCollectGarbage.h"
#include "OperationCreateNamespace.h"
#include "OperationCreateTable.h"
#include "OperationDropNamespace.h"
#include "OperationDropTable.h"
#include "OperationGatherStatistics.h"
#include "OperationProcessor.h"
#include "OperationMoveRange.h"
#include "OperationRecoverServer.h"
#include "OperationRegisterServer.h"
#include "OperationRelinquishAcknowledge.h"
#include "OperationRenameTable.h"
#include "OperationStatus.h"
#include "RangeServerConnection.h"


using namespace Hypertable;
using namespace Serialization;
using namespace Error;


/**
 *
 */
ConnectionHandler2::ConnectionHandler2(ContextPtr &context) : m_context(context) {
  int error;
  if ((error = m_context->comm->set_timer(context->timer_interval, this)) != Error::OK)
    HT_FATALF("Problem setting timer - %s", Error::get_text(error));
}


/**
 *
 */
void ConnectionHandler2::handle(EventPtr &event) {
  OperationPtr operation;

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
        operation = new OperationCreateTable(m_context, event);
        break;
      case MasterProtocol::COMMAND_DROP_TABLE:
        operation = new OperationDropTable(m_context, event);
        break;
      case MasterProtocol::COMMAND_ALTER_TABLE:
        operation = new OperationAlterTable(m_context, event);
        break;
      case MasterProtocol::COMMAND_RENAME_TABLE:
        operation = new OperationRenameTable(m_context, event);
        break;
      case MasterProtocol::COMMAND_STATUS:
        operation = new OperationStatus(m_context, event);
        break;
      case MasterProtocol::COMMAND_REGISTER_SERVER:
        operation = new OperationRegisterServer(m_context, event);
        break;
      case MasterProtocol::COMMAND_MOVE_RANGE:
        operation = new OperationMoveRange(m_context, event);
        break;
      case MasterProtocol::COMMAND_RELINQUISH_ACKNOWLEDGE:
        operation = new OperationRelinquishAcknowledge(m_context, event);
        break;
      case MasterProtocol::COMMAND_CLOSE:
        // TBD
        break;
      case MasterProtocol::COMMAND_SHUTDOWN:
        // TBD
        break;
      case MasterProtocol::COMMAND_CREATE_NAMESPACE:
        operation = new OperationCreateNamespace(m_context, event);
        break;
      case MasterProtocol::COMMAND_DROP_NAMESPACE:
        operation = new OperationDropNamespace(m_context, event);
        break;

      default:
        HT_THROWF(PROTOCOL_ERROR, "Unimplemented command (%llu)",
                  (Llu)event->header.command);
      }
      if (operation)
        m_context->op->add_operation(operation);
      else
        HT_THROWF(PROTOCOL_ERROR, "Unimplemented command (%llu)",
                  (Llu)event->header.command);
    }
    catch (Exception &e) {
      ResponseCallback cb(m_context->comm, event);
      HT_ERROR_OUT << e << HT_END;
      cb.error(Error::PROTOCOL_ERROR, 
               format("%s - %s", e.what(), get_text(e.code())));
    }
  }
  else if (event->type == Event::DISCONNECT) {
    RangeServerConnectionPtr rsc;
    if (m_context->find_server_by_local_addr(event->addr, rsc)) {
      if (m_context->disconnect_server(rsc)) {
        OperationPtr operation = new OperationRecoverServer(m_context, rsc);
        m_context->op->add_operation(operation);
      }
    }
    HT_INFOF("%s", event->to_str().c_str());
  }
  else if (event->type == Hypertable::Event::TIMER) {
    OperationPtr operation;
    int error;
    time_t now = time(0);

    if (m_context->next_monitoring_time <= now) {
      operation = new OperationGatherStatistics(m_context);
      m_context->op->add_operation(operation);
      m_context->next_monitoring_time = now + (m_context->monitoring_interval/1000) - 1;
    }

    if (m_context->next_gc_time <= now) {
      operation = new OperationCollectGarbage(m_context);
      m_context->op->add_operation(operation);
      m_context->next_gc_time = now + (m_context->gc_interval/1000) - 1;
    }

    if ((error = m_context->comm->set_timer(m_context->timer_interval, this)) != Error::OK)
      HT_FATALF("Problem setting timer - %s", Error::get_text(error));

  }
  else {
    HT_INFOF("%s", event->to_str().c_str());
  }

}
