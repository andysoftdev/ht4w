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

#include <config.h>
#include <cstring>
#include <sstream>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "concurrency/Monitor.h"
#include "concurrency/Util.h"
#include "TSocket.h"
#include "TTransportException.h"

namespace apache { namespace thrift { namespace transport {

using namespace std;

#ifdef _WIN32

std::string winapi_strerror(DWORD err) {
	std::string strerror;
	if( err ) {
		char* lpBuffer = 0;

		DWORD len = ::FormatMessageA(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				0, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPSTR) &lpBuffer, 0, NULL );

		if( len == 0 ) {
			lpBuffer = (char*)::LocalAlloc(LMEM_FIXED, 0x100);
			sprintf( lpBuffer, "unknown error %d", err );
		}
		else {
			while( len > 0 && lpBuffer[len-1] < ' ' ) {
				lpBuffer[--len] = '\0';
			}
		}
		if( lpBuffer ) {
			if( *lpBuffer ) {
				strerror = lpBuffer;
			}
			::LocalFree(lpBuffer);
		}		
	}
	return strerror;
}

#define SOCKETERRNO ::WSAGetLastError()
#define ERRNO ::GetLastError() 
#define STRERROR( e ) winapi_strerror(e)

#ifndef SHUT_RDWR
#define SHUT_RDWR SD_BOTH
#endif

inline int setsockopt( socket_t s, int level, int optname, void* optval, int optlen ) {
	return ::setsockopt( s, level, optname, (const char *)optval, optlen );
}

inline void usleep( int ms ) {
	::Sleep( ms );
}

#else

#define SOCKETERRNO errno
#define ERRNO errno
#define STRERROR( e ) std::string(strerror)

#endif

#define THROW( ex, msg, e ) \
{ \
	const int __err = e; \
	std::string strerr( msg + getSocketInfo() + ", " + STRERROR(__err) ); \
    GlobalOutput.perror(strerr, __err); \
    throw TTransportException(ex, strerr, __err); \
}

// Global var to track total socket sys calls
uint32_t g_socket_syscalls = 0;

/**
 * TSocket implementation.
 *
 */

TSocket::TSocket(string host, int port) :
  host_(host),
  port_(port),
  path_(""),
  socket_(INVALID_SOCKET),
  connTimeout_(0),
  sendTimeout_(0),
  recvTimeout_(0),
  lingerOn_(1),
  lingerVal_(0),
  noDelay_(1),
  maxRecvRetries_(5) {
  recvTimeval_.tv_sec = (int)(recvTimeout_/1000);
  recvTimeval_.tv_usec = (int)((recvTimeout_%1000)*1000);
}

TSocket::TSocket(string path) :
  host_(""),
  port_(0),
  path_(path),
  socket_(INVALID_SOCKET),
  connTimeout_(0),
  sendTimeout_(0),
  recvTimeout_(0),
  lingerOn_(1),
  lingerVal_(0),
  noDelay_(1),
  maxRecvRetries_(5) {
  recvTimeval_.tv_sec = (int)(recvTimeout_/1000);
  recvTimeval_.tv_usec = (int)((recvTimeout_%1000)*1000);
}

TSocket::TSocket() :
  host_(""),
  port_(0),
  path_(""),
  socket_(INVALID_SOCKET),
  connTimeout_(0),
  sendTimeout_(0),
  recvTimeout_(0),
  lingerOn_(1),
  lingerVal_(0),
  noDelay_(1),
  maxRecvRetries_(5) {
  recvTimeval_.tv_sec = (int)(recvTimeout_/1000);
  recvTimeval_.tv_usec = (int)((recvTimeout_%1000)*1000);
}

TSocket::TSocket(socket_t socket) :
  host_(""),
  port_(0),
  path_(""),
  socket_(socket),
  connTimeout_(0),
  sendTimeout_(0),
  recvTimeout_(0),
  lingerOn_(1),
  lingerVal_(0),
  noDelay_(1),
  maxRecvRetries_(5) {
  recvTimeval_.tv_sec = (int)(recvTimeout_/1000);
  recvTimeval_.tv_usec = (int)((recvTimeout_%1000)*1000);
}

TSocket::~TSocket() {
  close();
}

bool TSocket::isOpen() {
  return (socket_ != INVALID_SOCKET);
}

bool TSocket::peek() {
  if (!isOpen()) {
    return false;
  }
  uint8_t buf;
#ifndef _WIN32
  int r = recv(socket_, &buf, 1, MSG_PEEK);
#else
  int r = recv(socket_, (char*)&buf, 1, MSG_PEEK);
#endif
  if (r == -1) {
    int errno_copy = SOCKETERRNO;
    #if defined __FreeBSD__ || defined __MACH__
    /* shigin:
     * freebsd returns -1 and ECONNRESET if socket was closed by 
     * the other side
     */
    if (errno_copy == ECONNRESET)
    {
      close();
      return false;
    }
    #endif
	THROW( TTransportException::UNKNOWN, "TSocket::peek() recv()",  errno_copy )
  }
  return (r > 0);
}

void TSocket::openConnection(struct addrinfo *res) {
  if (isOpen()) {
    return;
  }

#ifndef _WIN32
  if (! path_.empty()) {
    socket_ = socket(PF_UNIX, SOCK_STREAM, IPPROTO_IP);
  } else 
#endif
  {
    socket_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  }

  if (socket_ == INVALID_SOCKET) {
   THROW( TTransportException::NOT_OPEN, "TSocket::open() socket()",  SOCKETERRNO )
  }

  // Send timeout
  if (sendTimeout_ > 0) {
    setSendTimeout(sendTimeout_);
  }

  // Recv timeout
  if (recvTimeout_ > 0) {
    setRecvTimeout(recvTimeout_);
  }

  // Linger
  setLinger(lingerOn_, lingerVal_);

  // No delay
  setNoDelay(noDelay_);

  // Uses a low min RTO if asked to.
#ifdef TCP_LOW_MIN_RTO
  if (getUseLowMinRto()) {
    int one = 1;
    setsockopt(socket_, IPPROTO_TCP, TCP_LOW_MIN_RTO, &one, sizeof(one));
  }
#endif


#ifndef _WIN32

  // Set the socket to be non blocking for connect if a timeout exists
  int flags = fcntl(socket_, F_GETFL, 0);
  if (connTimeout_ > 0) {
    if (-1 == fcntl(socket_, F_SETFL, flags | O_NONBLOCK)) {
      THROW( TTransportException::NOT_OPEN, "TSocket::open() fcntl()",  SOCKETERRNO )
    }
  } else {
    if (-1 == fcntl(socket_, F_SETFL, flags & ~O_NONBLOCK)) {
      THROW( TTransportException::NOT_OPEN, "TSocket::open() fcntl()",  SOCKETERRNO )
    }
  }

#else
    
  u_long arg = connTimeout_ > 0 ? 1 : 0;
  if( ioctlsocket(socket_, FIONBIO, &arg) == SOCKET_ERROR ) {
	 THROW( TTransportException::NOT_OPEN, "TSocket::open() ioctlsocket() FIONBIO",  SOCKETERRNO )
  }

#endif

  // Connect the socket
  int ret;

#ifndef _WIN32

  if (! path_.empty()) {
    struct sockaddr_un address;
    socklen_t len;

    if (path_.length() > sizeof(address.sun_path)) {
	  THROW( TTransportException::NOT_OPEN, "TSocket::open() Unix Domain socket path too long", SOCKETERRNO )
    }

    address.sun_family = AF_UNIX;
    sprintf(address.sun_path, path_.c_str());
    len = sizeof(address);
    ret = connect(socket_, (struct sockaddr *) &address, len);
  } else 

#endif

  {
    ret = connect(socket_, res->ai_addr, res->ai_addrlen);
  }

  // success case
  if (ret == 0) {
    goto done;
  }

#ifndef _WIN32
  if (SOCKETERRNO != EINPROGRESS) {
#else
   if (SOCKETERRNO != WSAEINPROGRESS && SOCKETERRNO != WSAEWOULDBLOCK) {
#endif
    THROW( TTransportException::NOT_OPEN, "TSocket::open() connect()", SOCKETERRNO )
  }

#ifndef _WIN32

  struct pollfd fds[1];
  std::memset(fds, 0 , sizeof(fds));
  fds[0].fd = socket_;
  fds[0].events = POLLOUT;
  ret = poll(fds, 1, connTimeout_);

#else

	WSAEVENT wait = WSACreateEvent();  
	if( (ret = WSAEventSelect(socket_, wait, FD_CONNECT)) == 0 ) {
		switch( WaitForSingleObject(wait, connTimeout_) ) {
			case WAIT_OBJECT_0:
				ret = 1;
				break;
			case WAIT_TIMEOUT:
				ret = 0;
				break;
			default:
				WSACloseEvent( wait );  
				THROW( TTransportException::NOT_OPEN, "TSocket::open() WaitForSingleObject()", GetLastError() )
				break;
		}	
		WSAEventSelect( socket_, 0, 0 );
	}
	else {
		WSACloseEvent( wait );  
		THROW( TTransportException::NOT_OPEN, "TSocket::open() WSAEventSelect()", SOCKETERRNO )
	}
    WSACloseEvent( wait );
 
#endif

  if (ret > 0) {
    // Ensure the socket is connected and that there are no errors set
    int val;
    socklen_t lon;
    lon = sizeof(int);
#ifndef _WIN32
    int ret2 = getsockopt(socket_, SOL_SOCKET, SO_ERROR, (void *)&val, &lon);
#else
	int ret2 = getsockopt(socket_, SOL_SOCKET, SO_ERROR, (char *)&val, &lon);
#endif
    if (ret2 == -1) {
	  THROW( TTransportException::NOT_OPEN, "TSocket::open() getsockopt()", SOCKETERRNO )
    }
    // no errors on socket, go to town
    if (val == 0) {
      goto done;
    }
	THROW( TTransportException::NOT_OPEN, "TSocket::open() error on socket (after poll)", val )
  } else if (ret == 0) {
    // socket timed out
    string errStr = "TSocket::open() timed out " + getSocketInfo();
    GlobalOutput(errStr.c_str());
    throw TTransportException(TTransportException::NOT_OPEN, errStr);
  } else {
    // error on poll()
    THROW( TTransportException::NOT_OPEN, "TSocket::open() poll()", SOCKETERRNO )
  }

done:

#ifndef _WIN32

  // Set socket back to normal mode (blocking)
  fcntl(socket_, F_SETFL, flags);

#else

  arg = 0;
  if( ioctlsocket(socket_, FIONBIO, &arg) == SOCKET_ERROR ) {
	 THROW( TTransportException::NOT_OPEN, "TSocket::open() ioctlsocket() FIONBIO", SOCKETERRNO )
  }

#endif
}

void TSocket::open() {
  if (isOpen()) {
    return;
  }
#ifndef _WIN32
  if (! path_.empty()) {
    unix_open();
  } else
#endif
  {
    local_open();
  }
}

void TSocket::unix_open(){
  if (! path_.empty()) {
    // Unix Domain SOcket does not need addrinfo struct, so we pass NULL
    openConnection(NULL);
  }
}

void TSocket::local_open(){
  if (isOpen()) {
    return;
  }

  // Validate port number
  if (port_ < 0 || port_ > 0xFFFF) {
    throw TTransportException(TTransportException::NOT_OPEN, "Specified port is invalid");
  }

  struct addrinfo hints, *res, *res0;
  res = NULL;
  res0 = NULL;
  int error;
  char port[sizeof("65535")];
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
  sprintf(port, "%d", port_);

  error = getaddrinfo(host_.c_str(), port, &hints, &res0);

  if (error) {
    string errStr = "TSocket::open() getaddrinfo() " + getSocketInfo() + string(gai_strerror(error));
    GlobalOutput(errStr.c_str());
    close();
    throw TTransportException(TTransportException::NOT_OPEN, "Could not resolve host for client socket.");
  }

  // Cycle through all the returned addresses until one
  // connects or push the exception up.
  for (res = res0; res; res = res->ai_next) {
    try {
      openConnection(res);
      break;
    } catch (TTransportException& ) {
      if (res->ai_next) {
        close();
      } else {
        close();
        freeaddrinfo(res0); // cleanup on failure
        throw;
      }
    }
  }

  // Free address structure memory
  freeaddrinfo(res0);
}

void TSocket::close() {
  if (socket_ != INVALID_SOCKET) {
    shutdown(socket_, SHUT_RDWR);
#ifndef _WIN32
    ::close(socket_);
#else
	::closesocket(socket_);
#endif
  }
  socket_ = INVALID_SOCKET;
}

uint32_t TSocket::read(uint8_t* buf, uint32_t len) {
  if (socket_ == INVALID_SOCKET) {
    throw TTransportException(TTransportException::NOT_OPEN, "Called read on non-open socket");
  }

  int32_t retries = 0;

  // EAGAIN can be signalled both when a timeout has occurred and when
  // the system is out of resources (an awesome undocumented feature).
  // The following is an approximation of the time interval under which
  // EAGAIN is taken to indicate an out of resources error.
  uint32_t eagainThresholdMicros = 0;
  if (recvTimeout_) {
    // if a readTimeout is specified along with a max number of recv retries, then
    // the threshold will ensure that the read timeout is not exceeded even in the
    // case of resource errors
    eagainThresholdMicros = (recvTimeout_*1000)/ ((maxRecvRetries_>0) ? maxRecvRetries_ : 2);
  }

 try_again:
  // Read from the socket
  int64_t begin = concurrency::Util::currentTimeTicks(1000);
#ifndef _WIN32
  int got = recv(socket_, buf, len, 0);
#else
  int got = recv(socket_, (char*)buf, len, 0);
#endif
  int errno_copy = SOCKETERRNO;
  ++g_socket_syscalls;

  // Check for error on read
  if (got < 0) {

#ifndef _WIN32

    if (errno_copy == EAGAIN) {
      // check if this is the lack of resources or timeout case
      uint32_t readElapsedMicros = concurrency::Util::currentTimeTicks(1000) - begin;

      if (!eagainThresholdMicros || (readElapsedMicros < eagainThresholdMicros)) {
        if (retries++ < maxRecvRetries_) {
          usleep(50);
          goto try_again;
        } else {
          throw TTransportException(TTransportException::TIMED_OUT,
                                    "EAGAIN (unavailable resources)");
        }
      } else {
        // infer that timeout has been hit
        throw TTransportException(TTransportException::TIMED_OUT,
                                  "EAGAIN (timed out)");
      }
    }

#endif

    // If interrupted, try again
#ifndef _WIN32
    if (errno_copy == EINTR && retries++ < maxRecvRetries_) {
#else
	if (errno_copy == WSAEINTR && retries++ < maxRecvRetries_) {
#endif
      goto try_again;
    }

    #if defined __FreeBSD__ || defined __MACH__
    if (errno_copy == ECONNRESET) {
      /* shigin: freebsd doesn't follow POSIX semantic of recv and fails with
       * ECONNRESET if peer performed shutdown 
       */
      close();
      return 0;
    }
    #endif

    // Now it's not a try again case, but a real probblem
    GlobalOutput.perror("TSocket::read() recv() " + getSocketInfo(), errno_copy);

#ifndef _WIN32

    // If we disconnect with no linger time
    if (errno_copy == ECONNRESET) {
      throw TTransportException(TTransportException::NOT_OPEN, "ECONNRESET");
    }

    // This ish isn't open
    if (errno_copy == ENOTCONN) {
      throw TTransportException(TTransportException::NOT_OPEN, "ENOTCONN");
    }

    // Timed out!
    if (errno_copy == ETIMEDOUT) {
      throw TTransportException(TTransportException::TIMED_OUT, "ETIMEDOUT");
    }

#else

	// If we disconnect with no linger time
    if (errno_copy == WSAECONNRESET) {
      throw TTransportException(TTransportException::NOT_OPEN, "ECONNRESET");
    }

    // This ish isn't open
    if (errno_copy == WSAENOTCONN) {
      throw TTransportException(TTransportException::NOT_OPEN, "ENOTCONN");
    }

    // Timed out!
    if (errno_copy == WSAETIMEDOUT) {
      throw TTransportException(TTransportException::TIMED_OUT, "ETIMEDOUT");
    }

#endif

    // Some other error, whatevz
    throw TTransportException(TTransportException::UNKNOWN, "Unknown", errno_copy);
  }

  // The remote host has closed the socket
  if (got == 0) {
    close();
    return 0;
  }

  // Pack data into string
  return got;
}

void TSocket::write(const uint8_t* buf, uint32_t len) {
  if (socket_ == INVALID_SOCKET) {
    throw TTransportException(TTransportException::NOT_OPEN, "Called write on non-open socket");
  }

  uint32_t sent = 0;

  while (sent < len) {

    int flags = 0;
    #ifdef MSG_NOSIGNAL
    // Note the use of MSG_NOSIGNAL to suppress SIGPIPE errors, instead we
    // check for the EPIPE return condition and close the socket in that case
    flags |= MSG_NOSIGNAL;
    #endif // ifdef MSG_NOSIGNAL

#ifndef _WIN32
    int b = send(socket_, buf + sent, len - sent, flags);
#else
	int b = send(socket_, (char*)(buf + sent), len - sent, flags);
#endif
    ++g_socket_syscalls;

    // Fail on a send error
    if (b < 0) {
      int errno_copy = SOCKETERRNO;
      GlobalOutput.perror("TSocket::write() send() " + getSocketInfo(), errno_copy);

#ifndef _WIN32
      if (errno_copy == EPIPE || errno_copy == ECONNRESET || errno_copy == ENOTCONN) {
#else
	  if (errno_copy == WSAECONNRESET || errno_copy == WSAENOTCONN) {
#endif
        close();
        throw TTransportException(TTransportException::NOT_OPEN, "write() send()", errno_copy);
      }

      throw TTransportException(TTransportException::UNKNOWN, "write() send()", errno_copy);
    }

    // Fail on blocked send
    if (b == 0) {
      throw TTransportException(TTransportException::NOT_OPEN, "Socket send returned 0.");
    }
    sent += b;
  }
}

std::string TSocket::getHost() {
  return host_;
}

int TSocket::getPort() {
  return port_;
}

void TSocket::setHost(string host) {
  host_ = host;
}

void TSocket::setPort(int port) {
  port_ = port;
}

void TSocket::setLinger(bool on, int linger) {
  lingerOn_ = on;
  lingerVal_ = linger;
  if (socket_ == INVALID_SOCKET) {
    return;
  }

  struct linger l = {(lingerOn_ ? 1 : 0), lingerVal_};
  int ret = setsockopt(socket_, SOL_SOCKET, SO_LINGER, &l, sizeof(l));
  if (ret == -1) {
    int errno_copy = SOCKETERRNO;  // Copy errno because we're allocating memory.
    GlobalOutput.perror("TSocket::setLinger() setsockopt() " + getSocketInfo(), errno_copy);
  }
}

void TSocket::setNoDelay(bool noDelay) {
  noDelay_ = noDelay;
  if (socket_ == INVALID_SOCKET) {
    return;
  }

  // Set socket to NODELAY
  int v = noDelay_ ? 1 : 0;
  int ret = setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v));
  if (ret == -1) {
    int errno_copy = SOCKETERRNO;  // Copy errno because we're allocating memory.
    GlobalOutput.perror("TSocket::setNoDelay() setsockopt() " + getSocketInfo(), errno_copy);
  }
}

void TSocket::setConnTimeout(int ms) {
  connTimeout_ = ms;
}

void TSocket::setRecvTimeout(int ms) {
  if (ms < 0) {
    char errBuf[512];
    sprintf(errBuf, "TSocket::setRecvTimeout with negative input: %d\n", ms);
    GlobalOutput(errBuf);
    return;
  }
  recvTimeout_ = ms;

  if (socket_ == INVALID_SOCKET) {
    return;
  }

  recvTimeval_.tv_sec = (int)(recvTimeout_/1000);
  recvTimeval_.tv_usec = (int)((recvTimeout_%1000)*1000);

  // Copy because poll may modify
#ifndef _WIN32
  struct timeval r = recvTimeval_;
  int ret = setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &r, sizeof(r));
#else
  DWORD recvTimeout( recvTimeout_ );
  int ret = setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &recvTimeout, sizeof(recvTimeout));
#endif
  if (ret == -1) {
    int errno_copy = SOCKETERRNO;  // Copy errno because we're allocating memory.
    GlobalOutput.perror("TSocket::setRecvTimeout() setsockopt() " + getSocketInfo(), errno_copy);
  }
}

void TSocket::setSendTimeout(int ms) {
  if (ms < 0) {
    char errBuf[512];
    sprintf(errBuf, "TSocket::setSendTimeout with negative input: %d\n", ms);
    GlobalOutput(errBuf);
    return;
  }
  sendTimeout_ = ms;

  if (socket_ == INVALID_SOCKET) {
    return;
  }

#ifndef _WIN32
  struct timeval s = {(int)(sendTimeout_/1000),
                      (int)((sendTimeout_%1000)*1000)};
  int ret = setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, &s, sizeof(s));
#else
  DWORD sendTimeout( sendTimeout_ );
  int ret = setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, &sendTimeout, sizeof(sendTimeout));
#endif
  if (ret == -1) {
    int errno_copy = SOCKETERRNO;  // Copy errno because we're allocating memory.
    GlobalOutput.perror("TSocket::setSendTimeout() setsockopt() " + getSocketInfo(), errno_copy);
  }
}

void TSocket::setMaxRecvRetries(int maxRecvRetries) {
  maxRecvRetries_ = maxRecvRetries;
}

string TSocket::getSocketInfo() {
  std::ostringstream oss;
  oss << "<Host: " << host_ << " Port: " << port_ << ">";
  return oss.str();
}

std::string TSocket::getPeerHost() {
  if (peerHost_.empty()) {
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    if (socket_ == INVALID_SOCKET) {
      return host_;
    }

    int rv = getpeername(socket_, (sockaddr*) &addr, &addrLen);

    if (rv != 0) {
      return peerHost_;
    }

    char clienthost[NI_MAXHOST];
    char clientservice[NI_MAXSERV];

    getnameinfo((sockaddr*) &addr, addrLen,
                clienthost, sizeof(clienthost),
                clientservice, sizeof(clientservice), 0);

    peerHost_ = clienthost;
  }
  return peerHost_;
}

std::string TSocket::getPeerAddress() {
  if (peerAddress_.empty()) {
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    if (socket_ == INVALID_SOCKET) {
      return peerAddress_;
    }

    int rv = getpeername(socket_, (sockaddr*) &addr, &addrLen);

    if (rv != 0) {
      return peerAddress_;
    }

    char clienthost[NI_MAXHOST];
    char clientservice[NI_MAXSERV];

    getnameinfo((sockaddr*) &addr, addrLen,
                clienthost, sizeof(clienthost),
                clientservice, sizeof(clientservice),
                NI_NUMERICHOST|NI_NUMERICSERV);

    peerAddress_ = clienthost;
    peerPort_ = std::atoi(clientservice);
  }
  return peerAddress_;
}

int TSocket::getPeerPort() {
  getPeerAddress();
  return peerPort_;
}

bool TSocket::useLowMinRto_ = false;
void TSocket::setUseLowMinRto(bool useLowMinRto) {
  useLowMinRto_ = useLowMinRto;
}
bool TSocket::getUseLowMinRto() {
  return useLowMinRto_;
}

}}} // apache::thrift::transport
