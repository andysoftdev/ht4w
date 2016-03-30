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

/// @file
/// Definitions for Open response callback.
/// This file contains definitions for Open, a response callback class used
/// to deliver results of the <i>open</i> function call back to the client.

#include <Common/Compat.h>

#include "Open.h"

#include <FsBroker/Lib/Response/Parameters/Open.h>

#include <AsyncComm/CommBuf.h>

#include <Common/Error.h>

using namespace Hypertable;
using namespace FsBroker::Lib::Response;

int Callback::Open::response(int32_t fd) {
  CommHeader header;
  header.initialize_from_request_header(m_event->header);
  Parameters::Open params(fd);
  CommBufPtr cbuf( new CommBuf(header, 4 + params.encoded_length()) );
  cbuf->append_i32(Error::OK);
  params.encode(cbuf->get_data_ptr_address());
  return m_comm->send_response(m_event->addr, cbuf);
}
