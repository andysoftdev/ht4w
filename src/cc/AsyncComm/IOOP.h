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

#ifndef HYPERTABLE_IOOP_H
#define HYPERTABLE_IOOP_H

#ifndef _WIN32
#error Platform isn't supported
#endif

#include "HandlerMap.h"

namespace Hypertable {

  struct IOOP : OVERLAPPED {

    enum OP {
      CONNECT,
      ACCEPT,
      RECV,
      SEND,
      RECVFROM,
      SENDTO
    };

    IOOP(socket_t _sd, OP _op, IOHandler* _handler) :
        sd(_sd),
        op(_op),
        handler(_handler),
        numberOfBytes(0),
        commbuf(0),
        err(NOERROR)
    {
      ZeroMemory(this, sizeof(OVERLAPPED));
    }

    IOOP(socket_t _sd, OP _op, IOHandler* _handler, CommBufPtr& _commbuf) :
        sd(_sd),
        op(_op),
        handler(_handler),
        numberOfBytes(0),
        commbuf(_commbuf),
        err(NOERROR)
    {
      ZeroMemory(this, sizeof(OVERLAPPED));
    }

    const socket_t sd;
    const OP op;
    IOHandlerPtr handler; // prevent Handler to be deleted until all the IOCP done
    DWORD numberOfBytes; // from GetQueuedCompletionStatus
    CommBufPtr commbuf; // buffer to be freed after WSASend (or WSASentTo) is complete
    BYTE addresses[(sizeof(struct sockaddr_in) + 16)*2]; // for AcceptEx
    DWORD err; // GetLastError just after GetQueuedCompletionStatus

    String to_str() const;
  };

}

#endif // HYPERTABLE_IOOP_H
