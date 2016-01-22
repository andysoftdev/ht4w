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
#include "Common/String.h"
#include "IOOP.h"

using namespace Hypertable;

String IOOP::to_str() const {
  switch(op) {
  case CONNECT:
    if(err!=NOERROR)
      return format("CONNECT error - %s", winapi_strerror(err));
    return "CONNECT";

  case ACCEPT:
    if(err!=NOERROR)
      return format("ACCEPT error - %s", winapi_strerror(err));
    return "ACCEPT";

  case RECV:
    if(err!=NOERROR)
      return format("RECV error - %s", winapi_strerror(err));
    return format("RECV %d bytes", numberOfBytes);

  case SEND:
    if(err!=NOERROR)
      return format("SEND error - %s", winapi_strerror(err));
    return format("SEND %d bytes", numberOfBytes);

  case RECVFROM:
    if(err!=NOERROR)
      return format("RECVFROM error - %s", winapi_strerror(err));
    return format("RECVFROM %d bytes", numberOfBytes);

  case SENDTO:
    if(err!=NOERROR)
      return format("SENDTO error - %s", winapi_strerror(err));
    return format("SENDTO %d bytes", numberOfBytes);

  default:
    HT_ASSERT(false);
  }
}
