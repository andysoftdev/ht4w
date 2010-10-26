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

//#define HT_DISABLE_LOG_DEBUG

#include "Common/Compat.h"

#include <cassert>
#include <iostream>

extern "C" {
#if defined(__APPLE__) || defined(__sun__) || defined(__FreeBSD__)
#include <arpa/inet.h>
#include <netinet/ip.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
}

#include "Common/Config.h"
#include "Common/Error.h"
#include "Common/InetAddr.h"
#include "Common/FileUtils.h"
#include "Common/SystemInfo.h"
#include "Common/Time.h"

#include "ReactorFactory.h"
#include "ReactorRunner.h"
#include "Comm.h"
#include "IOHandlerAccept.h"
#include "IOHandlerData.h"

using namespace Hypertable;
using namespace std;

atomic_t Comm::ms_next_request_id = ATOMIC_INIT(1);

Comm *Comm::ms_instance = NULL;
Mutex Comm::ms_mutex;

#ifdef _WIN32

static void* get_extension_function(socket_t s, const GUID *which_fn)
{
  void *ptr = NULL;
  DWORD bytes=0;
  WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER,
      (GUID*)which_fn, sizeof(*which_fn),
      &ptr, sizeof(ptr),
      &bytes, NULL, NULL);
  return ptr;
}

LPFN_ACCEPTEX Comm::pfnAcceptEx = 0;
LPFN_CONNECTEX Comm::pfnConnectEx = 0;
LPFN_GETACCEPTEXSOCKADDRS Comm::pfnGetAcceptExSockaddrs = 0;

#endif

Comm::Comm() {
  if (ReactorFactory::ms_reactors.size() == 0) {
    HT_ERROR("ReactorFactory::initialize must be called before creating "
             "AsyncComm::comm object");
    HT_ABORT;
  }

  #ifdef _WIN32
    
  socket_t s = socket(AF_INET, SOCK_STREAM, 0);
  if(s != INVALID_SOCKET) {
	static const GUID acceptex = WSAID_ACCEPTEX;
	static const GUID connectex = WSAID_CONNECTEX;
	static const GUID getacceptexsockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
    pfnAcceptEx            = (LPFN_ACCEPTEX)get_extension_function(s, &acceptex);
    pfnConnectEx           = (LPFN_CONNECTEX)get_extension_function(s, &connectex);
    pfnGetAcceptExSockaddrs= (LPFN_GETACCEPTEXSOCKADDRS)get_extension_function(s, &getacceptexsockaddrs);
    closesocket(s);

	if( pfnAcceptEx == 0 || pfnConnectEx == 0 || pfnGetAcceptExSockaddrs == 0 ) {
		HT_ERROR("ReactorFactory::initialize unable to query socket extension functions");
		HT_ABORT;
	}
  }
  else {
	  HT_ERROR("ReactorFactory::initialize create socket failed");
	  HT_ABORT;
  }

#endif

  ReactorFactory::get_reactor(m_timer_reactor);
  m_handler_map = ReactorRunner::handler_map;
}


Comm::~Comm() {
  set<IOHandler *> handlers;
  m_handler_map->decomission_all(handlers);

  foreach(IOHandler *handler, handlers)
    handler->shutdown();

  // Since Comm is a singleton, this is OK
  ReactorFactory::destroy();

  // wait for all decomissioned handlers to get purged by Reactor
  m_handler_map->wait_for_empty();
}


void Comm::destroy() {
  if (ms_instance) {
    delete ms_instance;
    ms_instance = 0;
  }
}


int
Comm::connect(const CommAddress &addr, DispatchHandlerPtr &default_handler) {
  socket_t sd;
  int error = m_handler_map->contains_data_handler(addr);

  if (error == Error::OK)
    return Error::COMM_ALREADY_CONNECTED;
  else if (error != Error::COMM_NOT_CONNECTED)
    return error;

#ifdef _WIN32
  
  sd = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
  if (sd == INVALID_SOCKET) {
    HT_ERRORF("WSASocket: %s", winapi_strerror(WSAGetLastError()));
    return Error::COMM_SOCKET_ERROR;
  }

#else
  
  if ((sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    HT_ERRORF("socket: %s", strerror(errno));
    return Error::COMM_SOCKET_ERROR;
  }

#endif

  return connect_socket(sd, addr, default_handler);
}



int
Comm::connect(const CommAddress &addr, const CommAddress &local_addr,
              DispatchHandlerPtr &default_handler) {
  socket_t sd;
  int error = m_handler_map->contains_data_handler(addr);

  HT_ASSERT(local_addr.is_inet());

  if (error == Error::OK)
    return Error::COMM_ALREADY_CONNECTED;
  else if (error != Error::COMM_NOT_CONNECTED)
    return error;

#ifdef _WIN32

  sd = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
  if (sd == INVALID_SOCKET) {
    HT_ERRORF("WSASocket: %s", winapi_strerror(WSAGetLastError()));
    return Error::COMM_SOCKET_ERROR;
  }

  if ((::bind(sd, (const sockaddr *)&local_addr.inet, sizeof(sockaddr_in))) == SOCKET_ERROR) {
    HT_ERRORF( "bind: %s: %s", local_addr.to_str().c_str(), winapi_strerror(WSAGetLastError()));
    return Error::COMM_BIND_ERROR;
  }

#else

  if ((sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    HT_ERRORF("socket: %s", strerror(errno));
    return Error::COMM_SOCKET_ERROR;
  }

  // bind socket to local address
  if ((bind(sd, (const sockaddr *)&local_addr.inet, sizeof(sockaddr_in))) < 0) {
    HT_ERRORF( "bind: %s: %s", local_addr.to_str().c_str(), strerror(errno));
    return Error::COMM_BIND_ERROR;
  }

 #endif

  return connect_socket(sd, addr, default_handler);
}


int Comm::set_alias(const InetAddr &addr, const InetAddr &alias) {
  ScopedLock lock(ms_mutex);
  return m_handler_map->set_alias(addr, alias);
}


void Comm::listen(const CommAddress &addr, ConnectionHandlerFactoryPtr &chf) {
  DispatchHandlerPtr null_handler(0);
  listen(addr, chf, null_handler);
}


int Comm::add_proxy(const String &proxy, const InetAddr &addr) {
  HT_ASSERT(ReactorFactory::proxy_master);
  return m_handler_map->add_proxy(proxy, addr);
}

void Comm::get_proxy_map(ProxyMapT &proxy_map) {
  m_handler_map->get_proxy_map(proxy_map);
}

bool Comm::wait_for_proxy_load(Timer &timer) {
  return m_handler_map->wait_for_proxy_load(timer);
}


void
Comm::listen(const CommAddress &addr, ConnectionHandlerFactoryPtr &chf,
             DispatchHandlerPtr &default_handler) {
  IOHandlerPtr handler;
  IOHandlerAccept *accept_handler;
  int one = 1;
  socket_t sd;

  HT_ASSERT(addr.is_inet());

#ifdef _WIN32
  
  sd = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
  if( sd == INVALID_SOCKET ) {
    HT_THROWF(Error::COMM_SOCKET_ERROR, "WSASocket: %s", winapi_strerror(WSAGetLastError()));
  }

  u_long one_arg = 1;
  if( ioctlsocket(sd, FIONBIO, &one_arg) == SOCKET_ERROR ) {
    HT_ERRORF("ioctlsocket(FIONBIO) failed : %s", winapi_strerror(WSAGetLastError()));
  }

#else

  if ((sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    HT_THROW(Error::COMM_SOCKET_ERROR, strerror(errno));

  // Set to non-blocking
  FileUtils::set_flags(sd, O_NONBLOCK);

#endif

#if defined(__linux__)
  if (setsockopt(sd, SOL_TCP, TCP_NODELAY, &one, sizeof(one)) < 0)
    HT_ERRORF("setting TCP_NODELAY: %s", strerror(errno));
#elif defined(__sun__)
  if (setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, (char*)&one, sizeof(one)) < 0)
    HT_ERRORF("setting TCP_NODELAY: %s", strerror(errno));
#elif defined(__APPLE__) || defined(__FreeBSD__)
  if (setsockopt(sd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one)) < 0)
    HT_WARNF("setsockopt(SO_NOSIGPIPE) failure: %s", strerror(errno));
#elif defined(_WIN32)
  if (setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one)) == SOCKET_ERROR)
    HT_ERRORF("setsockopt(TCP_NODELAY) failure: %s", winapi_strerror(WSAGetLastError()));
#endif

#ifdef _WIN32

  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one)) == SOCKET_ERROR)
    HT_ERRORF("setting SO_REUSEADDR: %s", winapi_strerror(WSAGetLastError()));
  if ((::bind(sd, (const sockaddr *)&addr.inet, sizeof(sockaddr_in))) == SOCKET_ERROR)
    HT_THROWF(Error::COMM_BIND_ERROR, "binding to %s: %s",
              addr.to_str().c_str(), winapi_strerror(WSAGetLastError()));
   if (::listen(sd, 1000) == SOCKET_ERROR)
    HT_THROWF(Error::COMM_LISTEN_ERROR, "listening: %s", winapi_strerror(WSAGetLastError()));

#else

  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0)
    HT_ERRORF("setting SO_REUSEADDR: %s", strerror(errno));

  if ((bind(sd, (const sockaddr *)&addr.inet, sizeof(sockaddr_in))) < 0)
    HT_THROWF(Error::COMM_BIND_ERROR, "binding to %s: %s",
              addr.to_str().c_str(), strerror(errno));

  if (::listen(sd, 1000) < 0)
    HT_THROWF(Error::COMM_LISTEN_ERROR, "listening: %s", strerror(errno));

#endif

  handler = accept_handler = new IOHandlerAccept(sd, addr.inet, default_handler,
                                                 m_handler_map, chf);
  int32_t error = m_handler_map->insert_handler(accept_handler);
  if (error != Error::OK)
    HT_THROWF(error, "Error inserting accept handler for %s into handler map",
              addr.to_str().c_str());
  accept_handler->start_polling();

#ifdef _WIN32

  accept_handler->async_accept();

#endif

}



int
Comm::send_request(const CommAddress &addr, uint32_t timeout_ms,
                   CommBufPtr &cbuf, DispatchHandler *resp_handler) {
  ScopedLock lock(ms_mutex);
  IOHandlerDataPtr data_handler;
  int error;

  if ((error = m_handler_map->lookup_data_handler(addr, data_handler)) != Error::OK) {
    HT_WARNF("No connection for %s - %s", addr.to_str().c_str(), Error::get_text(error));
    return error;
  }

  return send_request(data_handler, timeout_ms, cbuf, resp_handler);
}



int Comm::send_request(IOHandlerDataPtr &data_handler, uint32_t timeout_ms,
		       CommBufPtr &cbuf, DispatchHandler *resp_handler) {
  int error;

  cbuf->header.flags |= CommHeader::FLAGS_BIT_REQUEST;
  if (resp_handler == 0) {
    cbuf->header.flags |= CommHeader::FLAGS_BIT_IGNORE_RESPONSE;
    cbuf->header.id = 0;
  }
  else {
    cbuf->header.id = atomic_inc_return(&ms_next_request_id);
    if (cbuf->header.id == 0)
      cbuf->header.id = atomic_inc_return(&ms_next_request_id);
  }

  cbuf->header.timeout_ms = timeout_ms;
  cbuf->write_header_and_reset();

  if ((error = data_handler->send_message(cbuf, timeout_ms, resp_handler))
      != Error::OK)
    data_handler->shutdown();

  return error;
}



int Comm::send_response(const CommAddress &addr, CommBufPtr &cbuf) {
  ScopedLock lock(ms_mutex);
  IOHandlerDataPtr data_handler;
  int error;

  if ((error = m_handler_map->lookup_data_handler(addr, data_handler)) != Error::OK) {
    HT_ERRORF("No connection for %s - %s", addr.to_str().c_str(), Error::get_text(error));
    return error;
  }

  cbuf->header.flags &= CommHeader::FLAGS_MASK_REQUEST;

  cbuf->write_header_and_reset();

  if ((error = data_handler->send_message(cbuf)) != Error::OK)
    data_handler->shutdown();

  return error;
}


void
Comm::create_datagram_receive_socket(CommAddress &addr, int tos,
                                     DispatchHandlerPtr &dhp) {
  IOHandlerPtr handler;
  IOHandlerDatagram *dg_handler;
  int one = 1;
  socket_t sd;

  HT_ASSERT(addr.is_inet());
  int bufsize = 4*32768;

#ifdef _WIN32

  sd = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
  if (sd  == SOCKET_ERROR)
    HT_THROWF(Error::COMM_SOCKET_ERROR, "WSASocket: %s", winapi_strerror(WSAGetLastError()));

  // Set to non-blocking
  u_long one_arg = 1;
  if( ioctlsocket(sd, FIONBIO, &one_arg) == SOCKET_ERROR ) {
    HT_ERRORF("ioctlsocket(FIONBIO) failed : %s", winapi_strerror(WSAGetLastError()));
  }

  if (setsockopt(sd, SOL_SOCKET, SO_SNDBUF, (char *)&bufsize, sizeof(bufsize)) == SOCKET_ERROR)
    HT_ERRORF("setsockopt(SO_SNDBUF) failed - %s", winapi_strerror(WSAGetLastError()));
  if (setsockopt(sd, SOL_SOCKET, SO_RCVBUF, (char *)&bufsize, sizeof(bufsize)) == SOCKET_ERROR)
    HT_ERRORF("setsockopt(SO_RCVBUF) failed - %s", winapi_strerror(WSAGetLastError()));
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one)) == SOCKET_ERROR)
    HT_WARNF("setsockopt(SO_REUSEADDR) failure: %s", winapi_strerror(WSAGetLastError()));

#else

  if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    HT_THROWF(Error::COMM_SOCKET_ERROR, "%s", strerror(errno));

  // Set to non-blocking
  FileUtils::set_flags(sd, O_NONBLOCK);  

  if (setsockopt(sd, SOL_SOCKET, SO_SNDBUF, (char *)&bufsize, sizeof(bufsize))
      < 0) {
    HT_ERRORF("setsockopt(SO_SNDBUF) failed - %s", strerror(errno));
  }
  if (setsockopt(sd, SOL_SOCKET, SO_RCVBUF, (char *)&bufsize, sizeof(bufsize))
      < 0) {
    HT_ERRORF("setsockopt(SO_RCVBUF) failed - %s", strerror(errno));
  }

  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
    HT_WARNF("setsockopt(SO_REUSEADDR) failure: %s", strerror(errno));
  }

#endif

  if (tos) {

#ifndef _WIN32

    int opt;

#endif

#if defined(__linux__)
    opt = tos;
    setsockopt(sd, SOL_IP, IP_TOS, &opt, sizeof(opt));
    opt = tos;
    setsockopt(sd, SOL_SOCKET, SO_PRIORITY, &opt, sizeof(opt));
#elif defined(__APPLE__) || defined(__sun__) || defined(__FreeBSD__)
    opt = IPTOS_LOWDELAY;       /* see <netinet/in.h> */
    setsockopt(sd, IPPROTO_IP, IP_TOS, &opt, sizeof(opt));
#endif
  }

  // bind socket

#ifdef _WIN32

  if ((::bind(sd, (const sockaddr *)&addr.inet, sizeof(sockaddr_in))) == SOCKET_ERROR)
    HT_THROWF(Error::COMM_BIND_ERROR, "binding to %s: %s",
              addr.to_str().c_str(), winapi_strerror(WSAGetLastError()));

#else

  if ((bind(sd, (const sockaddr *)&addr.inet, sizeof(sockaddr_in))) < 0)
    HT_THROWF(Error::COMM_BIND_ERROR, "binding to %s: %s",
              addr.to_str().c_str(), strerror(errno));

#endif

  handler = dg_handler = new IOHandlerDatagram(sd, addr.inet, dhp);

  addr.set_inet( handler->get_local_address() );

  int32_t error = m_handler_map->insert_datagram_handler(dg_handler);
  if (error != Error::OK)
    HT_THROWF(error, "Error inserting datagram handler for %s into handler map",
              addr.to_str().c_str());
  dg_handler->start_polling();
}


int
Comm::send_datagram(const CommAddress &addr, const CommAddress &send_addr,
                    CommBufPtr &cbuf) {
  ScopedLock lock(ms_mutex);
  IOHandlerDatagramPtr dg_handler;
  int error;

  HT_ASSERT(addr.is_inet());

  if ((error = m_handler_map->lookup_datagram_handler(send_addr, dg_handler)) != Error::OK) {
    HT_ERRORF("Datagram send/local address %s not registered",
              send_addr.to_str().c_str());
    return error;
  }

  cbuf->header.flags |= (CommHeader::FLAGS_BIT_REQUEST |
			 CommHeader::FLAGS_BIT_IGNORE_RESPONSE);

  cbuf->write_header_and_reset();

  if ((error = dg_handler->send_message(addr.inet, cbuf)) != Error::OK)
    dg_handler->shutdown();

  return error;
}


int Comm::set_timer(uint32_t duration_millis, DispatchHandler *handler) {
  ExpireTimer timer;
  boost::xtime_get(&timer.expire_time, boost::TIME_UTC);
  xtime_add_millis(timer.expire_time, duration_millis);
  timer.handler = handler;
  m_timer_reactor->add_timer(timer);
  return Error::OK;
}


int
Comm::set_timer_absolute(boost::xtime expire_time, DispatchHandler *handler) {
  ExpireTimer timer;
  memcpy(&timer.expire_time, &expire_time, sizeof(boost::xtime));
  timer.handler = handler;
  m_timer_reactor->add_timer(timer);
  return Error::OK;
}


int
Comm::get_local_address(const CommAddress &addr,
                        CommAddress &local_addr) {
  ScopedLock lock(ms_mutex);
  IOHandlerDataPtr data_handler;
  int error;

  if ((error = m_handler_map->lookup_data_handler(addr, data_handler)) != Error::OK) {
    HT_ERRORF("No connection for %s - %s", addr.to_str().c_str(), Error::get_text(error));
    return error;
  }

  local_addr.set_inet( data_handler->get_local_address() );

  return Error::OK;
}


int Comm::close_socket(const CommAddress &addr) {
  IOHandlerPtr handler;

  if (!m_handler_map->decomission_handler(addr, handler))
    return Error::COMM_NOT_CONNECTED;

  handler->shutdown();

  return Error::OK;
}


/**
 *  ----- Private methods -----
 */

int
Comm::connect_socket(socket_t sd, const CommAddress &addr,
                     DispatchHandlerPtr &default_handler) {
  IOHandlerPtr handler;
  IOHandlerData *data_handler;
  int32_t error;
  int one = 1;
  CommAddress connectable_addr;

  if (addr.is_proxy()) {
    if (!m_handler_map->translate_proxy_address(addr, connectable_addr))
      return Error::COMM_INVALID_PROXY;
  }
  else
    connectable_addr = addr;

  // Set to non-blocking
#ifdef _WIN32

  u_long one_arg = 1;
  if( ioctlsocket(sd, FIONBIO, &one_arg) == SOCKET_ERROR ) {
    HT_ERRORF("ioctlsocket(FIONBIO) failed : %s", winapi_strerror(WSAGetLastError()));
  }

#else

  FileUtils::set_flags(sd, O_NONBLOCK);

#endif

#if defined(__linux__)
  if (setsockopt(sd, SOL_TCP, TCP_NODELAY, &one, sizeof(one)) < 0)
    HT_ERRORF("setsockopt(TCP_NODELAY) failure: %s", strerror(errno));
#elif defined(__sun__)
  if (setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) < 0)
    HT_ERRORF("setsockopt(TCP_NODELAY) failure: %s", strerror(errno));
#elif defined(__APPLE__) || defined(__FreeBSD__)
  if (setsockopt(sd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one)) < 0)
    HT_WARNF("setsockopt(SO_NOSIGPIPE) failure: %s", strerror(errno));
#elif defined(_WIN32)
  if (setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one)) == SOCKET_ERROR)
    HT_ERRORF("setsockopt(TCP_NODELAY) failure: %s", winapi_strerror(WSAGetLastError()));
#endif

  handler = data_handler = new IOHandlerData(sd, connectable_addr.inet, default_handler);
  if (addr.is_proxy())
    handler->set_proxy(addr.proxy);
  if ((error = m_handler_map->insert_handler(data_handler)) != Error::OK)
    return error;

#ifdef _WIN32

  // ConnectEx requires bind to be called before
  struct sockaddr_storage ss = {0};
  ((sockaddr_in *)&ss)->sin_family = AF_INET;
  ((sockaddr_in *)&ss)->sin_addr.s_addr = INADDR_ANY;
  if( ::bind(sd, (struct sockaddr *)&ss, sizeof(ss)) == SOCKET_ERROR )
    HT_ERRORF("bind failure: %s", winapi_strerror(WSAGetLastError()));

  if(!data_handler->start_polling())
    return Error::COMM_POLL_ERROR;

  // will be deleted in ReactorRunner::operator()
  OverlappedEx *pol = new OverlappedEx(sd, OverlappedEx::CONNECT, data_handler);

  if (!pfnConnectEx(sd, (struct sockaddr *)&connectable_addr.inet, sizeof(struct sockaddr_in),
          0, 0, 0, pol)) {
    int err = WSAGetLastError();
    if(err != ERROR_IO_PENDING) {
      HT_ERRORF("connecting to %s: %s", connectable_addr.to_str().c_str(),
                winapi_strerror(err));

      m_handler_map->remove_handler(connectable_addr, handler);

      delete pol;
      return Error::COMM_CONNECT_ERROR;
    }
  }
  else {
    HT_ASSERT(false);
  }

  return Error::OK;

#else

  while (::connect(sd, (struct sockaddr *)&connectable_addr.inet, sizeof(struct sockaddr_in))
          < 0) {
    if (errno == EINTR) {
      poll(0, 0, 1000);
      continue;
    }
    else if (errno == EINPROGRESS) {
      //HT_INFO("connect() in progress starting to poll");
      return data_handler->start_polling(Reactor::READ_READY|Reactor::WRITE_READY);
    }
    m_handler_map->remove_handler(connectable_addr, handler);
    HT_ERRORF("connecting to %s: %s", connectable_addr.to_str().c_str(),
              strerror(errno));
    return Error::COMM_CONNECT_ERROR;
  }

  return data_handler->start_polling(Reactor::READ_READY|Reactor::WRITE_READY);

#endif

}
