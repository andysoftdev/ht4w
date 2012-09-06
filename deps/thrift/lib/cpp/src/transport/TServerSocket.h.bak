/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef _THRIFT_TRANSPORT_TSERVERSOCKET_H_
#define _THRIFT_TRANSPORT_TSERVERSOCKET_H_ 1

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>

typedef int socket_t; // should be SOCKET, but SOCKET is unsigned 32/64

#else 

typedef int socket_t;
#define INVALID_SOCKET -1

#endif

#include "TServerTransport.h"
#include <boost/shared_ptr.hpp>

namespace apache { namespace thrift { namespace transport {

class TSocket;

/**
 * Server socket implementation of TServerTransport. Wrapper around a unix
 * socket listen and accept calls.
 *
 */
class TServerSocket : public TServerTransport {
 public:
  TServerSocket(int port);
  TServerSocket(int port, int sendTimeout, int recvTimeout);

#ifndef _WIN32

  TServerSocket(std::string path);

#endif

  ~TServerSocket();

  void setSendTimeout(int sendTimeout);
  void setRecvTimeout(int recvTimeout);

  void setRetryLimit(int retryLimit);
  void setRetryDelay(int retryDelay);

  void setTcpSendBuffer(int tcpSendBuffer);
  void setTcpRecvBuffer(int tcpRecvBuffer);

  void listen();
  void close();

  void interrupt();

 protected:
  boost::shared_ptr<TTransport> acceptImpl();

 private:
  int port_;  
#ifndef _WIN32
  std::string path_;
#endif
  socket_t serverSocket_;

  int acceptBacklog_;
  int sendTimeout_;
  int recvTimeout_;
  int retryLimit_;
  int retryDelay_;
  int tcpSendBuffer_;
  int tcpRecvBuffer_;  

  #ifdef _WIN32

  struct addrinfo server_addr_info_;
  LPFN_ACCEPTEX lpfnAcceptEx_;
  static HANDLE hIOCP_;

  void init();

  #else

  int intSock1_;
  int intSock2_;

  void init() {}

  #endif
};

}}} // apache::thrift::transport

#endif // #ifndef _THRIFT_TRANSPORT_TSERVERSOCKET_H_
