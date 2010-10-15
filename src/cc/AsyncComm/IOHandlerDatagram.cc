/**
 * Copyright (C) 2008 Doug Judd (Zvents, Inc.)
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
#include "HandlerMap.h"

using namespace Hypertable;
using namespace std;

#ifndef _WIN32

bool 
IOHandlerDatagram::handle_event(struct pollfd *event, clock_t, time_t) {
  int error;

  //DisplayEvent(event);

  if (event->revents & POLLOUT) {
    if ((error = handle_write_readiness()) != Error::OK) {
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

      event->load_header(m_sd, m_message, (size_t)m_message[1]);

      payload_len = nread - (ssize_t)event->header.header_len;
      event->payload_len = payload_len;
      event->payload = new uint8_t [payload_len];
      memcpy((void *)event->payload, m_message + event->header.header_len,
             payload_len);
      deliver_event( event );
      fromlen = sizeof(struct sockaddr_in);
    }

    if (errno != EAGAIN) {
      HT_ERRORF("FileUtils::recvfrom(%d) failure : %s", m_sd, strerror(errno));
      deliver_event(new Event(Event::ERROR, addr,
                              Error::COMM_RECEIVE_ERROR));
      return true;
    }

    return false;
  }

  if (event->events & POLLERR) {
    HT_WARN_OUT << "Received EPOLLERR on descriptor " << m_sd << " ("
                << m_addr.format() << ")" << HT_END;
    deliver_event(new Event(Event::ERROR, m_addr, Error::COMM_POLL_ERROR));
    return true;
  }

  HT_ASSERT((event->revents & POLLNVAL) == 0);

  return false;
}

#endif

#if defined(__linux__)

bool IOHandlerDatagram::handle_event(struct epoll_event *event, clock_t, time_t) {
  int error;

  //DisplayEvent(event);

  if (event->events & EPOLLOUT) {
    if ((error = handle_write_readiness()) != Error::OK) {
      deliver_event(new Event(Event::ERROR, m_addr, error));
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
        event->load_header(m_sd, m_message, (size_t)m_message[1]);
      }
      catch (Hypertable::Exception &e) {
        HT_ERROR_OUT << e << " - from " << addr.format() << HT_END;
        delete event;
        continue;
      }

      payload_len = nread - (ssize_t)event->header.header_len;
      event->payload_len = payload_len;
      event->payload = new uint8_t [payload_len];
      memcpy((void *)event->payload, m_message + event->header.header_len,
             payload_len);
      deliver_event( event );
      fromlen = sizeof(struct sockaddr_in);
    }

    if (errno != EAGAIN) {
      HT_ERRORF("FileUtils::recvfrom(%d) failure : %s", m_sd, strerror(errno));
      deliver_event(new Event(Event::ERROR, addr,
                              Error::COMM_RECEIVE_ERROR));
      return true;
    }

    return false;
  }

  if (event->events & EPOLLERR) {
    HT_WARN_OUT << "Received EPOLLERR on descriptor " << m_sd << " ("
                << m_addr.format() << ")" << HT_END;
    deliver_event(new Event(Event::ERROR, m_addr, Error::COMM_POLL_ERROR));
    return true;
  }

  return false;
}

#elif defined(__sun__)
bool IOHandlerDatagram::handle_event(port_event_t *event, clock_t, time_t) {
  int error;

  //DisplayEvent(event);

  try {

    if (event->portev_events == POLLOUT) {
      if ((error = handle_write_readiness()) != Error::OK) {
	deliver_event(new Event(Event::ERROR, m_addr, error));
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

	event->load_header(m_sd, m_message, (size_t)m_message[1]);

	payload_len = nread - (ssize_t)event->header.header_len;
	event->payload_len = payload_len;
	event->payload = new uint8_t [payload_len];
	memcpy((void *)event->payload, m_message + event->header.header_len,
	       payload_len);
	deliver_event( event );
	fromlen = sizeof(struct sockaddr_in);
      }

      if (errno != EAGAIN) {
	HT_ERRORF("FileUtils::recvfrom(%d) failure : %s", m_sd, strerror(errno));
	deliver_event(new Event(Event::ERROR, addr,
				Error::COMM_RECEIVE_ERROR));
	return true;
      }

      return false;
    }
    
    if (event->portev_events == POLLERR) {
      HT_WARN_OUT << "Received EPOLLERR on descriptor " << m_sd << " ("
		  << m_addr.format() << ")" << HT_END;
      deliver_event(new Event(Event::ERROR, m_addr, Error::COMM_POLL_ERROR));
      return true;
    }

    if (event->portev_events == POLLREMOVE) {
      HT_DEBUGF("Received POLLREMOVE on descriptor %d (%s:%d)", m_sd,
                inet_ntoa(m_addr.sin_addr), ntohs(m_addr.sin_port));
      return true;
    }
    
  }
  catch (Hypertable::Exception &e) {
    HT_ERROR_OUT << e << HT_END;
    return true;
  }

  return false;
  
}
#elif defined(__APPLE__) || defined(__FreeBSD__)

/**
 *
 */
bool IOHandlerDatagram::handle_event(struct kevent *event, clock_t, time_t) {
  int error;

  //DisplayEvent(event);

  assert(m_sd == (int)event->ident);

  assert((event->flags & EV_EOF) == 0);

  if (event->filter == EVFILT_WRITE) {
    if ((error = handle_write_readiness()) != Error::OK) {
      deliver_event(new Event(Event::ERROR, m_addr, error));
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
      return true;
    }

    Event *event = new Event(Event::MESSAGE, addr, Error::OK);

    event->load_header(m_sd, m_message, (size_t)m_message[1]);

    payload_len = nread - (ssize_t)event->header.header_len;
    event->payload_len = payload_len;
    event->payload = new uint8_t [payload_len];
    memcpy((void *)event->payload, m_message + event->header.header_len,
           payload_len);

    deliver_event( event );

    return false;
  }

  return false;

}

#elif defined(_WIN32)

bool IOHandlerDatagram::async_recvfrom() {
  // try to receive message, result will be in ReactorRunner,
  // then in IOHandlerDatagram::handle_event
  DWORD flags = 0;
  OverlappedEx* pol = new OverlappedEx(m_sd, OverlappedEx::RECVFROM, this);

  WSABUF wsabuf;
  wsabuf.buf = (char*)m_message;
  wsabuf.len = 65536;

  m_whencelen = sizeof(struct sockaddr_in);

//  HT_INFOF("WSARecvFrom(%d)", m_sd);
  if (WSARecvFrom(m_sd, &wsabuf, 1, 0, &flags, (struct sockaddr *)&m_whence,
                  &m_whencelen, pol, NULL) == SOCKET_ERROR) {
    int err = WSAGetLastError();
    if (err != WSA_IO_PENDING) {
      HT_ERRORF("WSARecvFrom err is %d\n", winapi_strerror(err));
      delete pol;
      return false;
    }
  }
  return true;
}

/**
 *
 */
bool IOHandlerDatagram::handle_event(OverlappedEx *pol, clock_t, time_t) {

  //HT_DEBUGF("IOHandlerDatagram::handle_event(%d)", pol->m_type);

  if (pol->m_type == OverlappedEx::RECVFROM) {
    if (pol->m_err != NOERROR) {
      deliver_event(new Event(Event::ERROR, m_addr, Error::COMM_RECEIVE_ERROR));
      return true;
    }

    Event *event = new Event(Event::MESSAGE, m_whence, Error::OK);

    event->load_header(m_sd, m_message, (size_t)m_message[1]);

    event->payload_len = pol->m_numberOfBytes - (ssize_t)event->header.header_len;
    event->payload = new uint8_t [event->payload_len];
    memcpy((void *)event->payload, m_message + event->header.header_len,
           event->payload_len);

    deliver_event( event );

    async_recvfrom();

    return false;
  }
  else if (pol->m_type == OverlappedEx::SENDTO) {
    if (pol->m_err != NOERROR) {
      deliver_event(new Event(Event::ERROR, m_addr, Error::COMM_SEND_ERROR));
      return true;
    }
  }
  return false;
}
#else
ImplementMe;
#endif

#ifdef _WIN32

int IOHandlerDatagram::send_message(const InetAddr &addr, CommBufPtr &cbp) {
  WSABUF wsabuf;
  wsabuf.buf = (char*)cbp->data_ptr;
  wsabuf.len = cbp->data.size - (cbp->data_ptr - cbp->data.base);

  OverlappedEx* pol = new OverlappedEx(m_sd, OverlappedEx::SENDTO, this);
  pol->m_commbuf = cbp; // keep it until completion

//  HT_INFOF("WSASendTo(%d)", m_sd);
  int rc = WSASendTo(m_sd, &wsabuf, 1, 0, 0, (sockaddr *)&addr,
                    sizeof(struct sockaddr_in), pol, 0);
  if (rc == SOCKET_ERROR) {
    int err = WSAGetLastError();
    if(err != WSA_IO_PENDING) {
      HT_WARNF("WSASendTo(%d, len=%d, addr=%s:%d) failed : %s",
          m_sd, wsabuf.len, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port),
          winapi_strerror(err));
      delete pol;
      return Error::COMM_BROKEN_CONNECTION;
    }
  }
  return Error::OK;
}

#else

int IOHandlerDatagram::handle_write_readiness() {
  ScopedLock lock(m_mutex);
  int error;

  if ((error = flush_send_queue()) != Error::OK)
    return error;

  // is this necessary?
  if (m_send_queue.empty())
    remove_poll_interest(Reactor::WRITE_READY);

  return Error::OK;
}



int IOHandlerDatagram::send_message(const InetAddr &addr, CommBufPtr &cbp) {
  ScopedLock lock(m_mutex);
  int error;
  bool initially_empty = m_send_queue.empty() ? true : false;

  HT_LOG_ENTER;

  //HT_INFOF("Pushing message destined for %s:%d onto send queue",
  //inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

  m_send_queue.push_back(SendRec(addr, cbp));

  if ((error = flush_send_queue()) != Error::OK)
    return error;

  if (initially_empty && !m_send_queue.empty()) {
    add_poll_interest(Reactor::WRITE_READY);
    //HT_INFO("Adding Write interest");
  }
  else if (!initially_empty && m_send_queue.empty()) {
    remove_poll_interest(Reactor::WRITE_READY);
    //HT_INFO("Removing Write interest");
  }

  return Error::OK;
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
