/*
 * Copyright (C) 2007-2015 Hypertable, Inc.
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

/** @file
 * Definitions for IOHandlerAccept.
 * This file contains method definitions for IOHandlerAccept, a class for
 * processing I/O events for accept (listen) sockets.
 */

#include <Common/Compat.h>

#define HT_DISABLE_LOG_DEBUG 1

#include "HandlerMap.h"
#include "IOHandlerAccept.h"
#include "IOHandlerData.h"
#include "ReactorFactory.h"
#include "ReactorRunner.h"

#include <Common/Error.h>
#include <Common/FileUtils.h>
#include <Common/Logger.h>

#include <iostream>

extern "C" {
#include <errno.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
}

#ifdef _WIN32

#include "Comm.h"
#include "IOOP.h"

#endif

using namespace Hypertable;
using namespace std;

#ifndef _WIN32

bool
IOHandlerAccept::handle_event(struct pollfd *event,
                              ClockT::time_point) {
  if (event->revents & POLLIN)
    return handle_incoming_connection();
  ReactorRunner::handler_map->decomission_handler(this);
  return true;
}

#endif

/**
 *
 */
#if defined(__APPLE__) || defined(__FreeBSD__)
bool IOHandlerAccept::handle_event(struct kevent *event,
                                   ClockT::time_point) {
  //DisplayEvent(event);
  if (event->filter == EVFILT_READ)
    return handle_incoming_connection();
  ReactorRunner::handler_map->decomission_handler(this);
  return true;
}
#elif defined(__linux__)
bool IOHandlerAccept::handle_event(struct epoll_event *event,
                                   ClockT::time_point) {
  //DisplayEvent(event);
  return handle_incoming_connection();
}
#elif defined(__sun__)
bool IOHandlerAccept::handle_event(port_event_t *event,
                                   ClockT::time_point) {
  if (event->portev_events == POLLIN)
    return handle_incoming_connection();
  ReactorRunner::handler_map->decomission_handler(this);
  return true;
}
#elif defined(_WIN32)

bool IOHandlerAccept::handle_event(IOOP *ioop, ClockT::time_point arival_time) {
  const socket_t sd = ioop->sd;
  const int one = 1;

  HT_DEBUGF("IOHandlerAccept::handle_event(%d)", pol);

  struct sockaddr_in *sa_local=NULL, *sa_remote=NULL;
  int socklen_local=0, socklen_remote=0;

  Comm::pfnGetAcceptExSockaddrs(&ioop->addresses, 0,
                        sizeof(struct sockaddr_in) + 16,
                        sizeof(struct sockaddr_in) + 16,
                        (LPSOCKADDR*)&sa_local, &socklen_local,
                        (LPSOCKADDR*)&sa_remote, &socklen_remote);

  HT_DEBUGF("Just accepted incoming connection, fd=%d (%s:%d)",
              sd, inet_ntoa(sa_remote->sin_addr), ntohs(sa_remote->sin_port));

  u_long arg_one = 1;
  if (ioctlsocket(sd, FIONBIO, &arg_one) == SOCKET_ERROR)
    HT_ERRORF("ioctlsocket(FIONBIO) failed - %s", winapi_strerror(WSAGetLastError()));

  if (setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one)) == SOCKET_ERROR)
    HT_ERRORF("setsockopt(TCP_NODELAY) failure: %s", winapi_strerror(WSAGetLastError()));

  if (setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, (char *)&arg_one, sizeof(arg_one)) == SOCKET_ERROR)
        HT_ERRORF("setsockopt(SO_KEEPALIVE) failed - %s", winapi_strerror(WSAGetLastError()));

  const int bufsize = 4*32768;

  if (setsockopt(sd, SOL_SOCKET, SO_SNDBUF, (const char *)&bufsize, sizeof(bufsize)) == SOCKET_ERROR)
    HT_ERRORF("setsockopt(SO_SNDBUF) failed - %s", winapi_strerror(WSAGetLastError()));
  if (setsockopt(sd, SOL_SOCKET, SO_RCVBUF, (const char *)&bufsize, sizeof(bufsize)) == SOCKET_ERROR)
    HT_ERRORF("setsockopt(SO_RCVBUF) failed - %s", winapi_strerror(WSAGetLastError()));

  DispatchHandlerPtr dhp;
  m_handler_factory->get_instance(dhp);
  IOHandlerData *data_handler = new IOHandlerData(ioop->sd, *sa_remote, dhp, true);
  m_handler_map->insert_handler(data_handler);
  IOHandlerPtr handler(data_handler);
  data_handler->start_polling();

  if (ReactorFactory::proxy_master) {
    int32_t error;
    if ((error = m_handler_map->propagate_proxy_map(data_handler))
        != Error::OK) {
      HT_ERRORF("Problem sending proxy map to %s - %s",
                m_addr.format().c_str(), Error::get_text(error));
      return true;
    }
  }

  data_handler->async_recv_header();
  deliver_event(make_shared<Event>(Event::CONNECTION_ESTABLISHED, *sa_remote, Error::OK));

  // accept next connection
  async_accept();

  return false;
}

#else
  ImplementMe;
#endif


#ifndef _WIN32

bool IOHandlerAccept::handle_incoming_connection() {
  int sd;
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(sockaddr_in);
  int one = 1;
  IOHandlerData *handler;

  while (true) {

    if ((sd = accept(m_sd, (struct sockaddr *)&addr, &addr_len)) < 0) {
      if (errno == EAGAIN)
        break;
      HT_ERRORF("accept() failure: %s", strerror(errno));
      break;
    }

    HT_DEBUGF("Just accepted incoming connection, fd=%d (%s:%d)",
	      m_sd, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

    // Set to non-blocking
    FileUtils::set_flags(sd, O_NONBLOCK);

#if defined(__linux__)
    if (setsockopt(sd, SOL_TCP, TCP_NODELAY, &one, sizeof(one)) < 0)
      HT_WARNF("setsockopt(TCP_NODELAY) failure: %s", strerror(errno));
#elif defined(__sun__)
    if (setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, (char*)&one, sizeof(one)) < 0)
      HT_ERRORF("setting TCP_NODELAY: %s", strerror(errno));
#elif defined(__APPLE__) || defined(__FreeBSD__)
    if (setsockopt(sd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one)) < 0)
      HT_WARNF("setsockopt(SO_NOSIGPIPE) failure: %s", strerror(errno));
#endif

    if (setsockopt(m_sd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one)) < 0)
      HT_ERRORF("setsockopt(SO_KEEPALIVE) failure: %s", strerror(errno));

    int bufsize = 4*32768;

    if (setsockopt(sd, SOL_SOCKET, SO_SNDBUF, (char *)&bufsize, sizeof(bufsize)) < 0)
      HT_WARNF("setsockopt(SO_SNDBUF) failed - %s", strerror(errno));

    if (setsockopt(sd, SOL_SOCKET, SO_RCVBUF, (char *)&bufsize, sizeof(bufsize)) < 0)
      HT_WARNF("setsockopt(SO_RCVBUF) failed - %s", strerror(errno));

    DispatchHandlerPtr dhp;
    m_handler_factory->get_instance(dhp);

    handler = new IOHandlerData(sd, addr, dhp, true);

    m_handler_map->insert_handler(handler, true);

    int32_t error;
    if ((error = handler->start_polling(PollEvent::READ |
                                        PollEvent::WRITE)) != Error::OK) {
      HT_ERRORF("Problem starting polling on incoming connection - %s",
                Error::get_text(error));
      ReactorRunner::handler_map->decrement_reference_count(handler);
      ReactorRunner::handler_map->decomission_handler(handler);
      return false;
    }

    if (ReactorFactory::proxy_master) {
      if ((error = ReactorRunner::handler_map->propagate_proxy_map(handler))
          != Error::OK) {
        HT_ERRORF("Problem sending proxy map to %s - %s",
                  m_addr.format().c_str(), Error::get_text(error));
        ReactorRunner::handler_map->decrement_reference_count(handler);
        return false;
      }
    }

    ReactorRunner::handler_map->decrement_reference_count(handler);

    EventPtr event = make_shared<Event>(Event::CONNECTION_ESTABLISHED, addr, Error::OK);
    deliver_event(event);
  }

  return false;
 }

#else

bool IOHandlerAccept::async_accept() {
  socket_t sd2 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sd2 == INVALID_SOCKET) {
    HT_ERRORF("socket creation error: %s", winapi_strerror(WSAGetLastError()));
    return false;
  }

  DWORD bytesReceived = 0;
  IOOP* ioop = new IOOP(sd2, IOOP::ACCEPT, this);
  if (!Comm::pfnAcceptEx(m_sd, sd2, &ioop->addresses, 0,
                  sizeof(struct sockaddr_in) + 16,
                  sizeof(struct sockaddr_in) + 16,
                  &bytesReceived,
                  ioop)) {
    int err = WSAGetLastError();
    if (err != ERROR_IO_PENDING) {
      HT_ERRORF("AcceptEx failed - %s", winapi_strerror(err));
      return false;
    }
  }
  else {
    ClockT::time_point arrival_time;
    if (ReactorRunner::record_arrival_time)
      arrival_time = ClockT::now();
    handle_event(ioop, arrival_time);
    delete ioop;
  }
  return true;
}

#endif

