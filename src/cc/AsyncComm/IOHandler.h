/* -*- c++ -*-
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

/** @file
 * Declarations for IOHandler.
 * This file contains type declarations for IOHandler, an abstract base class
 * from which I/O handlers are derived.
 */

#ifndef AsyncComm_IOHandler_h
#define AsyncComm_IOHandler_h

#include "Clock.h"
#include "DispatchHandler.h"
#include "PollEvent.h"
#include "ReactorFactory.h"
#include "ExpireTimer.h"

#include <Common/Logger.h>
#include <Common/Time.h>

#include <mutex>
#include <atomic>

extern "C" {
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/event.h>
#elif defined(__linux__)
#include <sys/epoll.h>
#if !defined(POLLRDHUP)
#define POLLRDHUP 0x2000
#endif
#elif defined(__sun__)
#include <port.h>
#include <sys/port_impl.h>
#include <unistd.h>
#endif
}

namespace Hypertable {

#ifdef _WIN32

  struct IOOP;
  class HandlerMap;

#endif

  /** @addtogroup AsyncComm
   *  @{
   */

  /** Base class for socket descriptor I/O handlers.
   * When a socket is created, an I/O handler object is allocated to handle
   * events that occur on the socket.  Events are encapsulated in the Event
   * class and are delivered to the application through a DispatchHandler.  For
   * example, a TCP socket will have an associated IOHandlerData object that
   * reads messages off the socket and sends then to the application via the
   * installed DispatchHandler object.
   */
  class IOHandler {

  public:

    /** Constructor.
     * Initializes the I/O handler, assigns it a Reactor, and sets #m_local_addr
     * to the locally bound address (IPv4:port) of <code>sd</code> (see
     * <code>getsockname</code>).
     * @param sd Socket descriptor
     * @param dhp Dispatch handler
     */
    IOHandler(socket_t sd, const DispatchHandlerPtr &dhp,
              Reactor::Priority rp = Reactor::Priority::NORMAL)
      : m_free_flag(0), m_error(Error::OK),
        m_sd(sd), m_dispatch_handler(dhp), m_decomissioned(false) {
      ReactorFactory::get_reactor(m_reactor, rp);
      socklen_t namelen = sizeof(m_local_addr);
      getsockname(m_sd, (sockaddr *)&m_local_addr, &namelen);
      memset(&m_alias, 0, sizeof(m_alias));
    }

    /// Constructor.
    /// Initializes handler for raw I/O.  Assigns it a Reactor and
    /// sets #m_local_addr to the locally bound address (IPv4:port) of
    /// <code>sd</code>.
    /// @param sd Socket descriptor
    IOHandler(socket_t sd) : m_free_flag(0),
                        m_error(Error::OK), m_sd(sd),
                        m_decomissioned(false) {
      ReactorFactory::get_reactor(m_reactor);
      socklen_t namelen = sizeof(m_local_addr);
      getsockname(m_sd, (sockaddr *)&m_local_addr, &namelen);
      memset(&m_alias, 0, sizeof(m_alias));
    }

#ifndef _WIN32

    /** Event handler method for Unix <i>poll</i> interface.
     * @param event Pointer to pollfd structure describing event
     * @param arrival_time Arrival time of event
     * @return <i>true</i> if socket should be closed, <i>false</i> otherwise
     */
    virtual bool handle_event(struct pollfd *event,
                              ClockT::time_point arrival_time) = 0;
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
    /** Event handler method for <i>kqueue</i> interface (OSX, FreeBSD).
     * @param event Pointer to <code>kevent</code> structure describing event
     * @param arrival_time Arrival time of event
     * @return <i>true</i> if socket should be closed, <i>false</i> otherwise
     */
    virtual bool handle_event(struct kevent *event,
                              ClockT::time_point arrival_time) = 0;
#elif defined(__linux__)
    /** Event handler method for Linux <i>epoll</i> interface.
     * @param event Pointer to <code>epoll_event</code> structure describing event
     * @param arrival_time Arrival time of event
     * @return <i>true</i> if socket should be closed, <i>false</i> otherwise
     */
    virtual bool handle_event(struct epoll_event *event,
                              ClockT::time_point arrival_time) = 0;
#elif defined(__sun__)
    /** Event handler method for <i>port_associate</i> interface (Solaris).
     * @param event Pointer to <code>port_event_t</code> structure describing event
     * @param arrival_time Arrival time of event
     * @return <i>true</i> if socket should be closed, <i>false</i> otherwise
     */
    virtual bool handle_event(port_event_t *event,
                              ClockT::time_point arrival_time) = 0;
#elif defined(_WIN32)
    virtual bool handle_event(IOOP *event,
                              ClockT::time_point arival_time) = 0;
#else
    // Implement me!!!
#endif

    /// Destructor.
    /// If #m_socket_internally_created is set to <i>true</i>, closes the socket
    /// descriptor #m_sd.
	virtual ~IOHandler() throw(...);

    /** Convenience method for delivering event to application.
     * This method will deliver <code>event</code> to the application via the
     * event handler <code>dh</code> if supplied, otherwise the event will be
     * delivered via the default event handler, or no default event handler
     * exists, it will just log the event.  This method is (and should always)
     * by called from a reactor thread.
     * @param event pointer to Event (deleted by this method)
     * @param dh Event handler via which to deliver event
     */
    void deliver_event(EventPtr &event, DispatchHandler *dh=0) {
      memcpy(&event->local_addr, &m_local_addr, sizeof(m_local_addr));
      if (dh)
        dh->handle(event);
      else {
        if (!m_dispatch_handler)
          HT_INFOF("%s", event->to_str().c_str());
        else
          m_dispatch_handler->handle(event);
      }
    }

#ifdef _WIN32

    int start_polling();

    bool is_closed() const {
      return m_sd == INVALID_SOCKET;
    }

    void close();

#else

    /** Start polling on the handler with the poll interest specified in
     * <code>mode</code>.
     * This method registers the poll interest, specified in <code>mode</code>,
     * with the polling interface and sets #m_poll_interest to
     * <code>mode</code>.  If an error is encountered, #m_error is
     * set to the approprate error code.
     * @return Error::OK on success, or one of Error::COMM_POLL_ERROR,
     * Error::COMM_SEND_ERROR, or Error::COMM_RECEIVE_ERROR on error
     */
    int start_polling(int mode=PollEvent::READ);

    /** Adds the poll interest specified in <code>mode</code> to the polling
     * interface for this handler.
     * This method adds the poll interest, specified in <code>mode</code>,
     * to the polling interface for this handler and merges <code>mode</code>
     * into #m_poll_interest using bitwise OR (|).  If an error is encountered,
     * #m_error is set to the approprate error code.
     * @return Error::OK on success, or one of Error::COMM_POLL_ERROR,
     * Error::COMM_SEND_ERROR, or Error::COMM_RECEIVE_ERROR on error
     */
    int add_poll_interest(int mode);

    /** Removes the poll interest specified in <code>mode</code> to the polling
     * interface for this handler.
     * This method removes the poll interest, specified in <code>mode</code>,
     * from the polling interface for this handler and strips <code>mode</code>
     * from #m_poll_interest using boolean operations.  If an error is
     * encountered, #m_error is set to the approprate error code.
     * @return Error::OK on success, or one of Error::COMM_POLL_ERROR,
     * Error::COMM_SEND_ERROR, or Error::COMM_RECEIVE_ERROR on error
     */
    int remove_poll_interest(int mode);

    /** Resets poll interest by adding #m_poll_interest to the polling interface
     * for this handler.  If an error is encountered, #m_error is set to the
     * approprate error code.
     * @return Error::OK on success, or one of Error::COMM_POLL_ERROR,
     * Error::COMM_SEND_ERROR, or Error::COMM_RECEIVE_ERROR on error
     */
    int reset_poll_interest() {
      return add_poll_interest(m_poll_interest);
    }

#endif

    /** Gets the handler socket address.  The socket address is the
     * address of the remote end of the connection for data (TCP) handlers,
     * and the local socket address for datagram and accept handlers.
     * @return Handler socket address
     */
    InetAddr get_address() { return m_addr; }

    /** Get local socket address for connection.
     * @return Local socket address for connection.
     */
    InetAddr get_local_address() { return m_local_addr; }

    /** Sets the proxy name for this connection.
     * @param proxy Proxy name to set for this connection.
     */
    void set_proxy(const String &proxy) {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_proxy = proxy;
    }

    /** Gets the proxy name for this connection.
     * @return Proxy name for this connection.
     */
    const String& get_proxy() {
      std::lock_guard<std::mutex> lock(m_mutex);
      return m_proxy;
    }

    /** Gets the socket descriptor for this connection.
     * @return Socket descriptor for this connection.
     */
    socket_t get_sd() { return m_sd; }

    /** Get the reactor that this handler is assigned to.
     * @param reactor Reference to returned reactor pointer
     */
    void get_reactor(ReactorPtr &reactor) { reactor = m_reactor; }

#ifndef _WIN32

    /** Display polling event from <code>poll()</code> interface to
     * <i>stderr</i>.
     * @param event Pointer to <code>pollfd</code> structure describing
     * <code>poll()</code> event.
     */
    void display_event(struct pollfd *event);

#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
    /** Display polling event from <code>kqueue</code> interface to
     * <i>stderr</i>.
     * @param event Pointer to <code>kevent</code> structure describing
     * <code>kqueue()</code> event.
     */
    void display_event(struct kevent *event);
#elif defined(__linux__)
    /** Display polling event from <code>epoll()</code> interface to
     * <i>stderr</i>.
     * @param event Pointer to <code>epoll_event</code> structure describing
     * <code>epoll()</code> event.
     */
    void display_event(struct epoll_event *event);
#elif defined(__sun__)
    /** Display polling event from <code>port_associate()</code> interface to
     * <i>stderr</i>.
     * @param event Pointer to <code>port_event_t</code> structure describing
     * <code>port_associate()</code> event.
     */
    void display_event(port_event_t *event);
#elif defined(_WIN32)
    void display_event(IOOP *event);
#endif

    friend class HandlerMap;

  protected:

    /** Sets #m_error to <code>error</code> if it has not already been set.
     * This method checks to see if #m_error is set to Error::OK and if so, it
     * sets #m_error to <code>error</code> and returns <i>true</i>.  Otherwise
     * it does nothing and returns false.
     * @return <i>true</i> if #m_error was set to <code>error</code>,
     * <i>false</i> otherwise.
     */
    bool test_and_set_error(int32_t error) {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (m_error == Error::OK) {
        m_error = error;
        return true;
      }
      return false;
    }

    /** Returns first error code encountered by handler.
     * When an error is encountered during handler methods, the first error code
     * that is encountered is recorded in #m_error.  This method returns that
     * error or Error::OK if no error has been encountered.
     * @return First error code encountered by this handler, or Error::OK if
     * no error has been encountered
     */
    int32_t get_error() {
      std::lock_guard<std::mutex> lock(m_mutex);
      return m_error;
    }

    /** Get alias address for this connection.
     * @return Alias address for this connection.
     */
    InetAddr get_alias() {
      return m_alias;
    }

    /** Set alias address for this connection.
     * @param alias Reference to return alias address.
     */
    void set_alias(const InetAddr &alias) {
      m_alias = alias;
    }

    /** Increment reference count.
     * @note This method assumes the caller is serializing access to this and
     * related methods with a mutex lock.
     * @see #decrement_reference_count, #reference_count, and #decomission
     */
#ifndef _WIN32

    void increment_reference_count() {
      m_reference_count++;
    }

#else

    void increment_reference_count(HandlerMap* handler_map) {
      m_handler_map = handler_map;
      m_reference_count++;
    }

#endif

    /** Decrement reference count.
     * If reference count drops to 0 and the handler is decomissioned
     * then it is scheduled for removal with a call to
     * <code>m_reactor->schedule_removal(this)</code>.
     * @note This method assumes the caller is serializing access to this and
     * related methods with a mutex lock.
     * @see #increment_reference_count, #reference_count, and #decomission
     */
    void decrement_reference_count() {
      if (m_reference_count.fetch_sub(1) == 1 && m_decomissioned)
        m_reactor->schedule_removal(this);
    }

    /** Return reference count
     * @note This method assumes the caller is serializing access to this and
     * related methods with a mutex lock.
     * @see #increment_reference_count, #decrement_reference_count, and
     * #decomission
     */
    uint32_t reference_count() const {
      return m_reference_count;
    }

    /** Decomission handler.
     * This method decomissions the handler by setting the #m_decomissioned
     * flag to <i>true</i>.  If the reference count is 0, the handler is
     * also scheduled for removal with a call to
     * <code>m_reactor->schedule_removal(this)</code>.
     * @note This method assumes the caller is serializing access to this and
     * related methods with a mutex lock.
     * @see #increment_reference_count, #decrement_reference_count,
     * #reference_count, and #is_decomissioned
     */
    void decomission() {
      if (!m_decomissioned) {
        m_decomissioned = true;
        if (reference_count() == 0)
          m_reactor->schedule_removal(this);
      }
    }

    /** Checks to see if handler is decomissioned.
     * @return <i>true</i> if it is decomissioned, <i>false</i> otherwise.
     */
    bool is_decomissioned() {
      return m_decomissioned;
    }

    /** Disconnect connection.
     */
    virtual void disconnect() { }

#ifndef _WIN32

    /** Return <code>poll()</code> interface events corresponding to the
     * normalized polling interest in <code>mode</code>.  <code>mode</code>
     * is some bitwise combination of the flags PollEvent::READ and
     * PollEvent::WRITE.
     * @return <code>poll()</code> events correspond to polling interest
     * specified in <code>mode</code>.
     */
    short poll_events(int mode) {
      short events = 0;
      if (mode & PollEvent::READ)
        events |= POLLIN;
      if (mode & PollEvent::WRITE)
        events |= POLLOUT;
      return events;
    }

    /** Stops polling by removing socket from polling interface.
     * Clears #m_poll_interest.
     */
    void stop_polling() {
      if (ReactorFactory::use_poll) {
        m_poll_interest = 0;
        m_reactor->modify_poll_interest(m_sd, 0);
        return;
      }
#if defined(__APPLE__) || defined(__sun__) || defined(__FreeBSD__)
      remove_poll_interest(PollEvent::READ|PollEvent::WRITE);
#elif defined(__linux__)
      struct epoll_event event;  // this is necessary for < Linux 2.6.9
      if (epoll_ctl(m_reactor->poll_fd, EPOLL_CTL_DEL, m_sd, &event) < 0) {
        HT_ERRORF("epoll_ctl(%d, EPOLL_CTL_DEL, %d) failed : %s",
                     m_reactor->poll_fd, m_sd, strerror(errno));
        exit(EXIT_FAILURE);
      }
      m_poll_interest = 0;
#endif
    }

#endif

    /// %Mutex for serializing concurrent access
    std::mutex m_mutex;

    /** Reference count.  Calls to methods that reference this member
     * must be mutex protected by caller.
     */
    std::atomic<uint32_t> m_reference_count {0};

    /// Free flag (for testing)
    uint32_t m_free_flag;

    /// Error code
    int32_t m_error;

    /// Proxy name for this connection
    String m_proxy;

    /// Handler socket address
    InetAddr m_addr;

    /// Local address of connection
    InetAddr m_local_addr;

    /// Address alias for connection
    InetAddr m_alias;

    /// Socket descriptor
    socket_t m_sd;

    /// Default dispatch hander for connection
    DispatchHandlerPtr m_dispatch_handler;

    /// Reactor to which this handler is assigned
    ReactorPtr m_reactor;

#ifndef _WIN32

    /** Current polling interest.  The polling interest is some bitwise
     * combination of the flags PollEvent::READ and
     * PollEvent::WRITE.
     */
    int m_poll_interest {0};

#else

    HandlerMap* m_handler_map {0};

    friend void intrusive_ptr_add_ref(IOHandler *handler);
    friend void intrusive_ptr_release(IOHandler *handler);

#endif

    /** Decomissioned flag.  Calls to methods that reference this member
     * must be mutex protected by caller.
     */
    bool m_decomissioned;

    /// Socket was internally created and should be closed on destroy.
    bool m_socket_internally_created {true};
  };
  /** @}*/

}

#endif // AsyncComm_IOHandler_h
