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

#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "TSocket.h"
#include "TServerSocket.h"
#include <boost/shared_ptr.hpp>

#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif

namespace apache { namespace thrift { namespace transport {

using namespace std;
using boost::shared_ptr;

#ifdef _WIN32

extern std::string winapi_strerror(DWORD err);

#define SOCKETERRNO ::WSAGetLastError()
#define ERRNO ::GetLastError() 
#define STRERROR( e ) winapi_strerror(e)

#ifndef SHUT_RDWR
#define SHUT_RDWR SD_BOTH
#endif

inline int setsockopt( socket_t s, int level, int optname, void* optval, int optlen ) {
	return ::setsockopt( s, level, optname, (const char *)optval, optlen );
}

HANDLE TServerSocket::hIOCP_ = 0;

#else

#define SOCKETERRNO errno
#define ERRNO errno
#define STRERROR( e ) std::string(strerror)

#endif

#define THROW( ex, msg, e ) \
{ \
	const int __err = e; \
	std::string strerr( msg + std::string(", ") + STRERROR(__err) ); \
    GlobalOutput.perror(strerr, __err); \
    throw TTransportException(ex, strerr, __err); \
}

TServerSocket::TServerSocket(int port) :
  port_(port),
  serverSocket_(-1),
  acceptBacklog_(1024),
  sendTimeout_(0),
  recvTimeout_(0),
  retryLimit_(0),
  retryDelay_(0),
  tcpSendBuffer_(0),
  tcpRecvBuffer_(0)
#ifndef _WIN32
, intSock1_(-1),
  intSock2_(-1)
#endif
  {  
	init();
  }

TServerSocket::TServerSocket(int port, int sendTimeout, int recvTimeout) :
  port_(port),
  serverSocket_(-1),
  acceptBacklog_(1024),
  sendTimeout_(sendTimeout),
  recvTimeout_(recvTimeout),
  retryLimit_(0),
  retryDelay_(0),
  tcpSendBuffer_(0),
  tcpRecvBuffer_(0)
#ifndef _WIN32
, intSock1_(-1),
  intSock2_(-1)
#endif
  {
	init();
  }

#ifndef _WIN32

TServerSocket::TServerSocket(string path) :
  port_(0),
  path_(path),
  serverSocket_(-1),
  acceptBacklog_(1024),
  sendTimeout_(0),
  recvTimeout_(0),
  retryLimit_(0),
  retryDelay_(0),
  tcpSendBuffer_(0),
  tcpRecvBuffer_(0),
  intSock1_(-1),
  intSock2_(-1)
  {  
	 init();
  }

#endif

TServerSocket::~TServerSocket() {
  close();

#ifdef _WIN32

  WSACleanup();

#endif
}

void TServerSocket::setSendTimeout(int sendTimeout) {
  sendTimeout_ = sendTimeout;
}

void TServerSocket::setRecvTimeout(int recvTimeout) {
  recvTimeout_ = recvTimeout;
}

void TServerSocket::setRetryLimit(int retryLimit) {
  retryLimit_ = retryLimit;
}

void TServerSocket::setRetryDelay(int retryDelay) {
  retryDelay_ = retryDelay;
}

void TServerSocket::setTcpSendBuffer(int tcpSendBuffer) {
  tcpSendBuffer_ = tcpSendBuffer;
}

void TServerSocket::setTcpRecvBuffer(int tcpRecvBuffer) {
  tcpRecvBuffer_ = tcpRecvBuffer;
}

void TServerSocket::listen() {

#ifndef _WIN32

  int sv[2];
  if (-1 == socketpair(AF_LOCAL, SOCK_STREAM, 0, sv)) {
    GlobalOutput.perror("TServerSocket::listen() socketpair() ", errno);
    intSock1_ = -1;
    intSock2_ = -1;
  } else {
    intSock1_ = sv[1];
    intSock2_ = sv[0];
  }

#endif

  struct addrinfo hints, *res, *res0;
  int error;
  char port[sizeof("65536") + 1];
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
  sprintf(port, "%d", port_);

  // Wildcard address
  error = getaddrinfo(NULL, port, &hints, &res0);
  if (error) {
    GlobalOutput.printf("getaddrinfo %d: %s", error, gai_strerror(error));
    close();
    throw TTransportException(TTransportException::NOT_OPEN, "Could not resolve host for server socket.");
  }

  // Pick the ipv6 address first since ipv4 addresses can be mapped
  // into ipv6 space.
  for (res = res0; res; res = res->ai_next) {
    if (res->ai_family == AF_INET6 || res->ai_next == NULL)
      break;
  }

#ifndef _WIN32

  if (! path_.empty()) {
    serverSocket_ = socket(PF_UNIX, SOCK_STREAM, IPPROTO_IP);
  } else
#endif
  {
    serverSocket_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	server_addr_info_ = *res;
  }

  if (serverSocket_ == -1) {
    int errno_copy = SOCKETERRNO;
	close();
	THROW( TTransportException::NOT_OPEN, "Could not create server socket, TServerSocket::listen() socket()",  errno_copy )
  }

  // Set reusaddress to prevent 2MSL delay on accept
  int one = 1;
  if (-1 == setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR,
                       &one, sizeof(one))) {
    int errno_copy = SOCKETERRNO;
	close();
	THROW( TTransportException::NOT_OPEN, "Could not set SO_REUSEADDR, TServerSocket::listen() setsockopt() SO_REUSEADDR",  errno_copy )
  }

  // Set TCP buffer sizes
  if (tcpSendBuffer_ > 0) {
    if (-1 == setsockopt(serverSocket_, SOL_SOCKET, SO_SNDBUF,
                         &tcpSendBuffer_, sizeof(tcpSendBuffer_))) {
      int errno_copy = SOCKETERRNO;
	  close();
	  THROW( TTransportException::NOT_OPEN, "Could not set SO_SNDBUF, TServerSocket::listen() setsockopt() SO_SNDBUF",  errno_copy )
    }
  }

  if (tcpRecvBuffer_ > 0) {
    if (-1 == setsockopt(serverSocket_, SOL_SOCKET, SO_RCVBUF,
                         &tcpRecvBuffer_, sizeof(tcpRecvBuffer_))) {
      int errno_copy = SOCKETERRNO;
	  close();
	  THROW( TTransportException::NOT_OPEN, "Could not set SO_RCVBUF, TServerSocket::listen() setsockopt() SO_RCVBUF",  errno_copy )
    }
  }

  // Defer accept
  #ifdef TCP_DEFER_ACCEPT
  if (-1 == setsockopt(serverSocket_, SOL_SOCKET, TCP_DEFER_ACCEPT,
                       &one, sizeof(one))) {
    int errno_copy = SOCKETERRNO;
	close();
	THROW( TTransportException::NOT_OPEN, "Could not set TCP_DEFER_ACCEPT, TServerSocket::listen() setsockopt() TCP_DEFER_ACCEPT",  errno_copy )
  }
  #endif // #ifdef TCP_DEFER_ACCEPT

  #ifdef IPV6_V6ONLY
  if (res->ai_family == AF_INET6) {
    int zero = 0;
    if (-1 == setsockopt(serverSocket_, IPPROTO_IPV6, IPV6_V6ONLY, 
          &zero, sizeof(zero))) {
      int errno_copy = SOCKETERRNO;
	  GlobalOutput.perror("TServerSocket::listen() IPV6_V6ONLY, " + STRERROR(errno_copy), errno_copy);
    }
  }
  #endif // #ifdef IPV6_V6ONLY

  // Turn linger off, don't want to block on calls to close
  struct linger ling = {0, 0};
  if (-1 == setsockopt(serverSocket_, SOL_SOCKET, SO_LINGER,
                       &ling, sizeof(ling))) {
    int errno_copy = SOCKETERRNO;
	close();
	THROW( TTransportException::NOT_OPEN, "Could not set SO_LINGER, TServerSocket::listen() setsockopt() SO_LINGER",  errno_copy )
  }

  // Unix Sockets do not need that
#ifndef _WIN32
  if (path_.empty())
#endif
  {

    // TCP Nodelay, speed over bandwidth
    if (-1 == setsockopt(serverSocket_, IPPROTO_TCP, TCP_NODELAY,
                         &one, sizeof(one))) {
      int errno_copy = SOCKETERRNO;
	  close();
	  THROW( TTransportException::NOT_OPEN, "Could not set TCP_NODELAY, TServerSocket::listen() setsockopt() TCP_NODELAY",  errno_copy )
    }
  }

#ifndef _WIN32

  // Set NONBLOCK on the accept socket
  int flags = fcntl(serverSocket_, F_GETFL, 0);
  if (flags == -1) {
    int errno_copy = SOCKETERRNO;
	close();
	THROW( TTransportException::NOT_OPEN, "TServerSocket::listen() fcntl() F_GETFL",  errno_copy )
  }

  if (-1 == fcntl(serverSocket_, F_SETFL, flags | O_NONBLOCK)) {
    int errno_copy = SOCKETERRNO;
	close();
	THROW( TTransportException::NOT_OPEN, "TServerSocket::listen() fcntl() O_NONBLOCK",  errno_copy )
  }

#else

  u_long one_arg = 1;
  if( ioctlsocket(serverSocket_, FIONBIO, &one_arg) == SOCKET_ERROR ) {
	 int errno_copy = SOCKETERRNO;
	 close();
	 THROW( TTransportException::NOT_OPEN, "TServerSocket::listen() ioctlsocket() FIONBIO",  errno_copy )
  }

#endif

  // prepare the port information
  // we may want to try to bind more than once, since SO_REUSEADDR doesn't
  // always seem to work. The client can configure the retry variables.
  int retries = 0;

#ifndef _WIN32

  if (! path_.empty()) {
    // Unix Domain Socket
    struct sockaddr_un address;
    socklen_t len;

    if (path_.length() > sizeof(address.sun_path)) {
      int errno_copy = errno;
      GlobalOutput.perror("TSocket::listen() Unix Domain socket path too long", errno_copy);
	  close(); 
      throw TTransportException(TTransportException::NOT_OPEN, " Unix Domain socket path too long");
    }

    address.sun_family = AF_UNIX;
    sprintf(address.sun_path, path_.c_str());
    len = sizeof(address);

    do {
      if (0 == bind(serverSocket_, (struct sockaddr *) &address, len)) {
        break;
      }
      // use short circuit evaluation here to only sleep if we need to
    } while ((retries++ < retryLimit_) && (sleep(retryDelay_) == 0));
  } else {
	  do {
      if (0 == bind(serverSocket_, res->ai_addr, res->ai_addrlen)) {
        break;
      }
      // use short circuit evaluation here to only sleep if we need to
    } while ((retries++ < retryLimit_) && (sleep(retryDelay_) == 0));

    // free addrinfo
    freeaddrinfo(res0);
  }

  #else

  HANDLE hIOCP = CreateIoCompletionPort((HANDLE)serverSocket_, hIOCP_, 0, 0);
  if(hIOCP != hIOCP_) {
	 int errno_copy = ERRNO;
	 close();
	 THROW( TTransportException::NOT_OPEN, "TServerSocket::listen() CreateIoCompletionPort()",  errno_copy )
  }
  static GUID GuidAcceptEx = WSAID_ACCEPTEX;
  DWORD dwBytes;
  if( WSAIoctl(serverSocket_,  SIO_GET_EXTENSION_FUNCTION_POINTER,  &GuidAcceptEx,  sizeof(GuidAcceptEx), &lpfnAcceptEx_, sizeof(lpfnAcceptEx_), &dwBytes, NULL, NULL) == SOCKET_ERROR ) {
	  int errno_copy = SOCKETERRNO;
	  close();
	  THROW( TTransportException::NOT_OPEN, "TServerSocket::listen() WSAIoctl()",  errno_copy )
  }

  do {
		if (0 == ::bind(serverSocket_, res->ai_addr, res->ai_addrlen)) {
		   break;
		}
		if( retries < retryLimit_ ) {
			::Sleep(retryDelay_);
		}
		// use short circuit evaluation here to only sleep if we need to
	} while ( retries++ < retryLimit_ );

	// free addrinfo
	freeaddrinfo(res0);

  #endif

  // throw an error if we failed to bind properly
  if (retries > retryLimit_) {
    char errbuf[1024];
#ifndef _WIN32
    if (! path_.empty()) {
      sprintf(errbuf, "TServerSocket::listen() PATH %s", path_.c_str());
    }
    else
#endif
	{
      sprintf(errbuf, "TServerSocket::listen() BIND %d", port_);
    }
    GlobalOutput(errbuf);
    close();
    throw TTransportException(TTransportException::NOT_OPEN, "Could not bind");
  }

  // Call listen
  if (-1 == ::listen(serverSocket_, acceptBacklog_)) {
	int errno_copy = SOCKETERRNO;
	close();
	THROW( TTransportException::NOT_OPEN, "Could not listen, TServerSocket::listen() listen()",  errno_copy )
  }

  // The socket is now listening!
}

shared_ptr<TTransport> TServerSocket::acceptImpl() {
  if (serverSocket_ < 0) {
    throw TTransportException(TTransportException::NOT_OPEN, "TServerSocket not listening");
  }

#ifndef _WIN32

  struct pollfd fds[2];

  int maxEintrs = 5;
  int numEintrs = 0;

  while (true) {
    std::memset(fds, 0 , sizeof(fds));
    fds[0].fd = serverSocket_;
    fds[0].events = POLLIN;
    if (intSock2_ >= 0) {
      fds[1].fd = intSock2_;
      fds[1].events = POLLIN;
    }
    int ret = poll(fds, 2, -1);

    if (ret < 0) {
      // error cases
      if (errno == EINTR && (numEintrs++ < maxEintrs)) {
        // EINTR needs to be handled manually and we can tolerate
        // a certain number
        continue;
      }
      int errno_copy = errno;
      GlobalOutput.perror("TServerSocket::acceptImpl() poll() ", errno_copy);
      throw TTransportException(TTransportException::UNKNOWN, "Unknown", errno_copy);
    } else if (ret > 0) {
      // Check for an interrupt signal
      if (intSock2_ >= 0 && (fds[1].revents & POLLIN)) {
        int8_t buf;
        if (-1 == recv(intSock2_, &buf, sizeof(int8_t), 0)) {
          GlobalOutput.perror("TServerSocket::acceptImpl() recv() interrupt ", errno);
        }
        throw TTransportException(TTransportException::INTERRUPTED);
      }

      // Check for the actual server socket being ready
      if (fds[0].revents & POLLIN) {
        break;
      }
    } else {
      GlobalOutput("TServerSocket::acceptImpl() poll 0");
      throw TTransportException(TTransportException::UNKNOWN);
    }
  }

  struct sockaddr_storage clientAddress;
  int size = sizeof(clientAddress);
  int clientSocket = ::accept(serverSocket_,
                              (struct sockaddr *) &clientAddress,
                              (socklen_t *) &size);

  if (clientSocket < 0) {
    int errno_copy = errno;
    GlobalOutput.perror("TServerSocket::acceptImpl() ::accept() ", errno_copy);
    throw TTransportException(TTransportException::UNKNOWN, "accept()", errno_copy);
  }

  // Make sure client socket is blocking
  int flags = fcntl(clientSocket, F_GETFL, 0);
  if (flags == -1) {
    int errno_copy = errno;
    GlobalOutput.perror("TServerSocket::acceptImpl() fcntl() F_GETFL ", errno_copy);
    throw TTransportException(TTransportException::UNKNOWN, "fcntl(F_GETFL)", errno_copy);
  }

  if (-1 == fcntl(clientSocket, F_SETFL, flags & ~O_NONBLOCK)) {
    int errno_copy = errno;
    GlobalOutput.perror("TServerSocket::acceptImpl() fcntl() F_SETFL ~O_NONBLOCK ", errno_copy);
    throw TTransportException(TTransportException::UNKNOWN, "fcntl(F_SETFL)", errno_copy);
  }

#else
   
  // Create an accepting socket (none overlapped!)
  socket_t clientSocket = WSASocket(server_addr_info_.ai_family, server_addr_info_.ai_socktype, server_addr_info_.ai_protocol, NULL, 0, 0);
  if (clientSocket == INVALID_SOCKET) {
	THROW( TTransportException::UNKNOWN, "Could not create accepting socket, TServerSocket::acceptImpl() WSASocket()",  WSAGetLastError() )
  }

  char adr[(sizeof(struct sockaddr_in6) + 16)*2];
  WSAOVERLAPPED ol;
  memset(&ol, 0, sizeof(ol));
  DWORD dwBytes;

  if( !lpfnAcceptEx_(serverSocket_, 
		clientSocket,
		adr, 
		0,
		sizeof(sockaddr_in6) + 16, 
		sizeof(sockaddr_in6) + 16, 
		&dwBytes, 
		&ol) && WSAGetLastError() != ERROR_IO_PENDING ) {

	 THROW( TTransportException::UNKNOWN, "TServerSocket::acceptImpl() AcceptEx()",  WSAGetLastError() )
  }

  while (true) {
    DWORD numberOfBytes;
    ULONG_PTR completionKey;
    OVERLAPPED* pol = 0;
    if(GetQueuedCompletionStatus(hIOCP_,
                                 &numberOfBytes,
                                 &completionKey,
                                 (LPOVERLAPPED*)&pol,
                                 10000)) {
      if (pol == 0) {
        throw TTransportException(TTransportException::INTERRUPTED);
      }
      else {
        // IO completed with no error
        break;
      }
    }
    else {
      DWORD err = GetLastError();
      if( err != WAIT_TIMEOUT ) {
		THROW( TTransportException::UNKNOWN, "TServerSocket::acceptImpl() GetQueuedCompletionStatus()",  err )
       }
    }
  }

  // Make sure client socket is blocking
  u_long zero_arg = 0;
  if( ioctlsocket(clientSocket, FIONBIO, &zero_arg) == SOCKET_ERROR ) {
	 THROW( TTransportException::UNKNOWN, "TServerSocket::acceptImpl() ioctlsocket() FIONBIO",  WSAGetLastError() )
  }

#endif  

  shared_ptr<TSocket> client(new TSocket(clientSocket));
  if (sendTimeout_ > 0) {
    client->setSendTimeout(sendTimeout_);
  }
  if (recvTimeout_ > 0) {
    client->setRecvTimeout(recvTimeout_);
  }

  return client;
}

void TServerSocket::interrupt() {

#ifndef _WIN32

  if (intSock1_ >= 0) {
    int8_t byte = 0;
    if (-1 == send(intSock1_, &byte, sizeof(int8_t), 0)) {
      GlobalOutput.perror("TServerSocket::interrupt() send() ", errno);
    }

#else

   if (TServerSocket::hIOCP_) {
	  if(!PostQueuedCompletionStatus(TServerSocket::hIOCP_, 0, 0, 0)) {
		  GlobalOutput.perror("TServerSocket::interrupt() PostQueuedCompletionStatus() ", GetLastError());
	  }

#endif

  }
}

void TServerSocket::close() {
  if (serverSocket_ >= 0) {
    shutdown(serverSocket_, SHUT_RDWR);
#ifndef _WIN32
    ::close(serverSocket_);
#else
	::closesocket(serverSocket_);
#endif
  }
#ifndef _WIN32
  if (intSock1_ >= 0) {
    ::close(intSock1_);
  }
  if (intSock2_ >= 0) {
    ::close(intSock2_);
  }  
  intSock1_ = -1;
  intSock2_ = -1;
#endif
  serverSocket_ = -1;
}


#ifdef _WIN32

void TServerSocket::init() {
	memset( &server_addr_info_, 0, sizeof(server_addr_info_) );
	lpfnAcceptEx_ = 0;

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		THROW( TTransportException::UNKNOWN, "TServerSocket::init() WSAStartup()",  WSAGetLastError() )
	}

	if( hIOCP_ == 0 ) {
		hIOCP_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
		if(hIOCP_ == 0) {
			THROW( TTransportException::UNKNOWN, "TServerSocket::init() CreateIoCompletionPort()",  WSAGetLastError() )
		}
	}
}

#endif

}}} // apache::thrift::transport
