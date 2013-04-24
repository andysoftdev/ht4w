/*
 * Copyright (C) 2007-2013 Hypertable, Inc.
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
 * Definitions for IOHandlerDatagram.
 * This file contains method definitions for IOHandlerDatagram, a class for
 * processing I/O events for datagram sockets.
 */

#include "Common/Compat.h"

#include <cassert>
#include <iostream>

extern "C" {
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/event.h>
#endif
}

#define HT_DISABLE_LOG_DEBUG 1

#include "Common/Error.h"
#include "Common/FileUtils.h"

#include "IOHandlerDatagram.h"
#include "ReactorRunner.h"
#ifdef _WIN32
#include "IOOP.h"
#endif

using namespace Hypertable;
using namespace std;

#ifndef _WIN32

bool 
IOHandlerDatagram::handle_event(struct pollfd *event, time_t arrival_time) {
  int error;

  //DisplayEvent(event);

  if (event->revents & POLLOUT) {
    if ((error = handle_write_readiness()) != Error::OK) {
      test_and_set_error(error);
      ReactorRunner::handler_map->decomission_handler(this);
      deliver_event(new Event(Event::ERROR, m_addr, error));
      return true;
    }
  }

  if (event->revents & POLLIN) {
    ssize_t nread, payload_len;
    struct sockaddr_in addr;
    socklen_t fromlen = sizeof(struct sockaddr_in);

    while ((nread = FileUtils::recvfrom(m_sd, m_message, 65536,
            (struct sockaddr *)&addr, &fromlen)) != (ssize_t)-1) {

      Event *event = new Event(Event::MESSAGE, addr, Error::OK);

      event->load_message_header(m_message, (size_t)m_message[1]);

      payload_len = nread - (ssize_t)event->header.header_len;
      event->payload_len = payload_len;
      event->payload = new uint8_t [payload_len];
      event->arrival_time = arrival_time;
      memcpy((void *)event->payload, m_message + event->header.header_len,
             payload_len);
      deliver_event( event );
      fromlen = sizeof(struct sockaddr_in);
    }

    if (errno != EAGAIN) {
      HT_ERRORF("FileUtils::recvfrom(%d) failure : %s", m_sd, strerror(errno));
      deliver_event(new Event(Event::ERROR, addr,
                              Error::COMM_RECEIVE_ERROR));
      ReactorRunner::handler_map->decomission_handler(this);
    }

    return false;
  }

  if (event->events & POLLERR) {
    HT_WARN_OUT << "Received EPOLLERR on descriptor " << m_sd << " ("
                << m_addr.format() << ")" << HT_END;
    deliver_event(new Event(Event::ERROR, m_addr, Error::COMM_POLL_ERROR));
    ReactorRunner::handler_map->decomission_handler(this);
    return true;
  }

  HT_ASSERT((event->revents & POLLNVAL) == 0);

  return false;
}

#endif

#if defined(__linux__)

bool IOHandlerDatagram::handle_event(struct epoll_event *event,
                                     time_t arrival_time) {
  int error;

  //DisplayEvent(event);

  if (event->events & EPOLLOUT) {
    if ((error = handle_write_readiness()) != Error::OK) {
      deliver_event(new Event(Event::ERROR, m_addr, error));
      ReactorRunner::handler_map->decomission_handler(this);
      return true;
    }
  }

  if (event->events & EPOLLIN) {
    ssize_t nread, payload_len;
    InetAddr addr;
    socklen_t fromlen = sizeof(struct sockaddr_in);

    while ((nread = FileUtils::recvfrom(m_sd, m_message, 65536,
            (struct sockaddr *)&addr, &fromlen)) != (ssize_t)-1) {

      Event *event = new Event(Event::MESSAGE, addr, Error::OK);
      
      try {
        event->load_message_header(m_message, (size_t)m_message[1]);
      }
      catch (Hypertable::Exception &e) {
        HT_ERROR_OUT << e << " - from " << addr.format() << HT_END;
        delete event;
        continue;
      }

      payload_len = nread - (ssize_t)event->header.header_len;
      event->payload_len = payload_len;
      event->payload = new uint8_t [payload_len];
      event->arrival_time = arrival_time;
      memcpy((void *)event->payload, m_message + event->header.header_len,
             payload_len);
      deliver_event( event );
      fromlen = sizeof(struct sockaddr_in);
    }

    if (errno != EAGAIN) {
      HT_ERRORF("FileUtils::recvfrom(%d) failure : %s", m_sd, strerror(errno));
      deliver_event(new Event(Event::ERROR, addr,
                              Error::COMM_RECEIVE_ERROR));
      ReactorRunner::handler_map->decomission_handler(this);
      return true;
    }

    return false;
  }

  if (event->events & EPOLLERR) {
    HT_WARN_OUT << "Received EPOLLERR on descriptor " << m_sd << " ("
                << m_addr.format() << ")" << HT_END;
    deliver_event(new Event(Event::ERROR, m_addr, Error::COMM_POLL_ERROR));
    ReactorRunner::handler_map->decomission_handler(this);
    return true;
  }

  return false;
}

#elif defined(__sun__)
bool
IOHandlerDatagram::handle_event(port_event_t *event, time_t arrival_time) {
  int error;

  //DisplayEvent(event);

  try {

    if (event->portev_events == POLLOUT) {
      if ((error = handle_write_readiness()) != Error::OK) {
	deliver_event(new Event(Event::ERROR, m_addr, error));
        ReactorRunner::handler_map->decomission_handler(this);
	return true;
      }
    }

    if (event->portev_events == POLLIN) {
      ssize_t nread, payload_len;
      struct sockaddr_in addr;
      socklen_t fromlen = sizeof(struct sockaddr_in);

      while ((nread = FileUtils::recvfrom(m_sd, m_message, 65536,
					  (struct sockaddr *)&addr, &fromlen)) != (ssize_t)-1) {

	Event *event = new Event(Event::MESSAGE, addr, Error::OK);

	event->load_message_header(m_message, (size_t)m_message[1]);

	payload_len = nread - (ssize_t)event->header.header_len;
	event->payload_len = payload_len;
	event->payload = new uint8_t [payload_len];
        event->arrival_time = arrival_time;
	memcpy((void *)event->payload, m_message + event->header.header_len,
	       payload_len);
	deliver_event( event );
	fromlen = sizeof(struct sockaddr_in);
      }

      if (errno != EAGAIN) {
	HT_ERRORF("FileUtils::recvfrom(%d) failure : %s", m_sd, strerror(errno));
	deliver_event(new Event(Event::ERROR, addr,
				Error::COMM_RECEIVE_ERROR));
        ReactorRunner::handler_map->decomission_handler(this);
	return true;
      }

      return false;
    }
    
    if (event->portev_events == POLLERR) {
      HT_WARN_OUT << "Received EPOLLERR on descriptor " << m_sd << " ("
		  << m_addr.format() << ")" << HT_END;
      deliver_event(new Event(Event::ERROR, m_addr, Error::COMM_POLL_ERROR));
      ReactorRunner::handler_map->decomission_handler(this);
      return true;
    }

    if (event->portev_events == POLLREMOVE) {
      HT_DEBUGF("Received POLLREMOVE on descriptor %d (%s:%d)", m_sd,
                inet_ntoa(m_addr.sin_addr), ntohs(m_addr.sin_port));
      ReactorRunner::handler_map->decomission_handler(this);
      return true;
    }
    
  }
  catch (Hypertable::Exception &e) {
    HT_ERROR_OUT << e << HT_END;
    ReactorRunner::handler_map->decomission_handler(this);
    return true;
  }

  return false;
  
}

#elif defined(__APPLE__) || defined(__FreeBSD__)

bool
IOHandlerDatagram::handle_event(struct kevent *event, time_t arrival_time) {
  int error;

  //DisplayEvent(event);

  assert(m_sd == (int)event->ident);

  assert((event->flags & EV_EOF) == 0);

  if (event->filter == EVFILT_WRITE) {
    if ((error = handle_write_readiness()) != Error::OK) {
      deliver_event(new Event(Event::ERROR, m_addr, error));
      ReactorRunner::handler_map->decomission_handler(this);
      return true;
    }
  }

  if (event->filter == EVFILT_READ) {
    size_t available = (size_t)event->data;
    ssize_t nread, payload_len;
    struct sockaddr_in addr;
    socklen_t fromlen = sizeof(struct sockaddr_in);

    if ((nread = FileUtils::recvfrom(m_sd, m_message, 65536,
        (struct sockaddr *)&addr, &fromlen)) == (ssize_t)-1) {
      HT_ERRORF("FileUtils::recvfrom(%d, len=%d) failure : %s", m_sd,
                (int)available, strerror(errno));
      deliver_event(new Event(Event::ERROR, addr,
                              Error::COMM_RECEIVE_ERROR));
      ReactorRunner::handler_map->decomission_handler(this);
      return true;
    }

    Event *event = new Event(Event::MESSAGE, addr, Error::OK);

    event->load_message_header(m_message, (size_t)m_message[1]);

    payload_len = nread - (ssize_t)event->header.header_len;
    event->payload_len = payload_len;
    event->payload = new uint8_t [payload_len];
    event->arrival_time = arrival_time;
    memcpy((void *)event->payload, m_message + event->header.header_len,
           payload_len);

    deliver_event( event );
  }

  return false;

}

#elif defined(_WIN32)

bool IOHandlerDatagram::async_recvfrom() {
  // try to receive message, result will be in ReactorRunner,
  // then in IOHandlerDatagram::handle_event
  DWORD flags = 0;
  IOOP* ioop = new IOOP(m_sd, IOOP::RECVFROM, this);

  WSABUF wsabuf;
  wsabuf.buf = (char*)m_message;
  wsabuf.len = 65536;

  m_whencelen = sizeof(struct sockaddr_in);

  if (WSARecvFrom(m_sd, &wsabuf, 1, 0, &flags, (struct sockaddr *)&m_whence,
                  &m_whencelen, ioop, 0) == SOCKET_ERROR) {
    int err = WSAGetLastError();
    if (err != WSA_IO_PENDING) {
      HT_ERRORF("WSARecvFrom err is %d\n", winapi_strerror(err));
      delete ioop;
      return false;
    }
  }
  return true;
}

/**
 *
 */
bool IOHandlerDatagram::handle_event(IOOP *ioop, time_t) {
  if (ioop->op == IOOP::RECVFROM) {
    if (ioop->err != NOERROR) {
      HT_INFOF("IOOP::RECVFROM - %s", winapi_strerror(ioop->err));
      deliver_event(new Event(Event::ERROR, m_addr, Error::COMM_RECEIVE_ERROR));
      return true;
    }

    Event *event = new Event(Event::MESSAGE, m_whence, Error::OK);
    event->load_message_header(m_message, (size_t)m_message[1]);
    event->payload_len = ioop->numberOfBytes - (ssize_t)event->header.header_len;
    event->payload = new uint8_t [event->payload_len];
    memcpy((void *)event->payload, m_message + event->header.header_len,
           event->payload_len);

    deliver_event(event);
    async_recvfrom();
  }
  else if (ioop->op == IOOP::SENDTO) {
    if (ioop->err != NOERROR) {
      HT_INFOF("IOOP::SENDTO - %s", winapi_strerror(ioop->err));
      deliver_event(new Event(Event::ERROR, m_addr, Error::COMM_SEND_ERROR));
      return true;
    }
  }
  else
    HT_ERRORF("Unhandled I/O operation - %d", ioop->op);

  return false;
}
#else
ImplementMe;
#endif

#ifdef _WIN32

int IOHandlerDatagram::send_message(const InetAddr &addr, CommBufPtr &cbp) {
  WSABUF wsabuf;
  IOOP* ioop = new IOOP(m_sd, IOOP::SENDTO, this, cbp);
  wsabuf.buf = (char*)cbp->data_ptr;
  wsabuf.len = cbp->data.size - (cbp->data_ptr - cbp->data.base);

  if (WSASendTo(ioop->sd, &wsabuf, 1, 0, 0
                , (const sockaddr*)&addr, sizeof(struct sockaddr_in), ioop, 0) == SOCKET_ERROR) {
    int err = WSAGetLastError();
    if (err != WSA_IO_PENDING) {
      HT_ERRORF("WSASendTo(len=%d, addr=%s:%d) failed - %s",
                wsabuf.len, inet_ntoa(addr.sin_addr),
                ntohs(addr.sin_port),
                winapi_strerror(err));
      delete ioop;
      return Error::COMM_BROKEN_CONNECTION;
    }
  }
  return Error::OK;
}

#else

int IOHandlerDatagram::handle_write_readiness() {
  ScopedLock lock(m_mutex);
  int error;

  if ((error = flush_send_queue()) != Error::OK) {
    if (m_error == Error::OK)
      m_error = error;
    return error;
  }

  // is this necessary?
  if (m_send_queue.empty())
    error = remove_poll_interest(Reactor::WRITE_READY);

  if (error != Error::OK && m_error == Error::OK)
    m_error = error;

  return error;
}



int IOHandlerDatagram::send_message(const InetAddr &addr, CommBufPtr &cbp) {
  ScopedLock lock(m_mutex);
  int error = Error::OK;
  bool initially_empty = m_send_queue.empty() ? true : false;

  HT_LOG_ENTER;

  //HT_INFOF("Pushing message destined for %s:%d onto send queue",
  //inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

  m_send_queue.push_back(SendRec(addr, cbp));

  if ((error = flush_send_queue()) != Error::OK) {
    if (m_error == Error::OK)
      m_error = error;
    return error;
  }

  if (initially_empty && !m_send_queue.empty()) {
    error = add_poll_interest(Reactor::WRITE_READY);
    //HT_INFO("Adding Write interest");
  }
  else if (!initially_empty && m_send_queue.empty()) {
    error = remove_poll_interest(Reactor::WRITE_READY);
    //HT_INFO("Removing Write interest");
  }

  if (error != Error::OK && m_error == Error::OK)
    m_error = error;

  return error;
}


int IOHandlerDatagram::flush_send_queue() {
  ssize_t nsent, tosend;

  while (!m_send_queue.empty()) {

    SendRec &send_rec = m_send_queue.front();

    tosend = send_rec.second->data.size - (send_rec.second->data_ptr
                                           - send_rec.second->data.base);
    assert(tosend > 0);
    assert(send_rec.second->ext.base == 0);

    nsent = FileUtils::sendto(m_sd, send_rec.second->data_ptr, tosend,
                              (sockaddr *)&send_rec.first,
                              sizeof(struct sockaddr_in));

    if (nsent == (ssize_t)-1) {
      HT_WARNF("FileUtils::sendto(%d, len=%d, addr=%s:%d) failed : %s", m_sd,
               (int)tosend, inet_ntoa(send_rec.first.sin_addr),
               ntohs(send_rec.first.sin_port), strerror(errno));
      return Error::COMM_SEND_ERROR;
    }
    else if (nsent < tosend) {
      HT_WARNF("Only sent %d bytes", (int)nsent);
      if (nsent == 0)
        break;
      send_rec.second->data_ptr += nsent;
      break;
    }

    // buffer written successfully, now remove from queue (destroys buffer)
    m_send_queue.pop_front();
  }

  return Error::OK;
}

#endif
