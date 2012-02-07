/** -*- C++ -*-
 * Copyright (C) 2007-2012 Hypertable, Inc.
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
 * along with Hypertable. If not, see <http://www.gnu.org/licenses/>
 */

#ifndef HYPERTABLE_THRIFT_CLIENT_H
#define HYPERTABLE_THRIFT_CLIENT_H

#ifdef _WIN32
#pragma warning( push, 3 )
#pragma warning( disable : 4250 ) // inherits via dominance
#endif

// Note: do NOT add any hypertable dependencies in this file
#include <protocol/TBinaryProtocol.h>
#include <transport/TSocket.h>
#include <transport/TTransportUtils.h>

#include "gen-cpp/HqlService.h"

namespace Hypertable { namespace Thrift {

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

// helper to initialize base class of Client
struct ClientHelper {
  boost::shared_ptr<TSocket> socket;
  boost::shared_ptr<TTransport> transport;
  boost::shared_ptr<TProtocol> protocol;

  ClientHelper(const std::string &host, int port, int conn_timeout_ms, int timeout_ms)
    : socket(new TSocket(host, port)),
      transport(new TFramedTransport(socket)),
      protocol(new TBinaryProtocol(transport)) {

    socket->setConnTimeout(conn_timeout_ms);
    socket->setSendTimeout(timeout_ms);
    socket->setRecvTimeout(timeout_ms);
  }
};

/**
 * A client for the ThriftBroker
 */
class Client : protected ClientHelper, public ThriftGen::HqlServiceClient {
public:
  Client(const std::string &host, int port, int conn_timeout_ms = 30000, int timeout_ms = 300000,
         bool open = true)
    : ClientHelper(host, port, conn_timeout_ms, timeout_ms), HqlServiceClient(protocol),
      m_do_close(false) {

    if (open) {
      transport->open();
      m_do_close = true;
    }
  }

  virtual ~Client() {
    if (m_do_close) {
      transport->close();
      m_do_close = false;
    }
  }

private:
  bool m_do_close;
};

#ifdef _WIN32

class ThriftClient : public Client, public ReferenceCount {
  public:
    ThriftClient(const std::string &_host, int _port, int _conn_timeout_ms = 30000, int _timeout_ms = 300000, bool open = true) 
    : Client(_host, _port, _timeout_ms, open)
    , host(_host), port(_port), conn_timeout_ms(_conn_timeout_ms), timeout_ms(_timeout_ms), locked(0), next_client(0) {
      ::InitializeCriticalSection(&cs);
    }

    virtual ~ThriftClient() {
      client_pool.clear();
      ::DeleteCriticalSection(&cs);
    }

    ThriftClient* clone() {
      Lock lock( this );
      return new ThriftClient(host, port, conn_timeout_ms, timeout_ms);
    }

    ThriftClient* get_pooled() {
      Lock lock( this );
      if (client_pool.size() < maxConnectionPoolSize) {
        ClientPtr client = new ThriftClient(host, port, conn_timeout_ms, timeout_ms);
        client_pool.push_back(client);
        return client.get();
      }
      while (client_pool.size() > maxConnectionPoolSize)
        client_pool.erase(client_pool.begin());
      next_client = next_client % client_pool.size();
      for (int i = 0; i < (int)client_pool.size(); ++i) {
        ClientPtr client = client_pool[next_client];
        next_client = (++next_client) % client_pool.size();
        if (!client->is_locked())
          return client.get();
      }
      ClientPtr client = new ThriftClient(host, port, conn_timeout_ms, timeout_ms);
      client_pool.push_back(client);
      return client.get();
    }

    bool is_open() {
      Lock lock( this );
      return transport && transport->isOpen();
    }

    inline bool is_locked() const {
      return _InterlockedOr( const_cast<volatile long*>(&locked), 0 ) > 0;
    }

    void renew_nothrow() {
      Lock lock( this );
      try {
        boost::shared_ptr<TSocket> _socket(new TSocket(host, port));
        socket = _socket;

        boost::shared_ptr<TTransport> _transport(new TFramedTransport(socket));
        transport = _transport;

        boost::shared_ptr<TProtocol> _protocol(new TBinaryProtocol(transport));
        protocol = piprot_ = poprot_ =  _protocol;
        iprot_ = oprot_ = protocol.get();

        socket->setConnTimeout(conn_timeout_ms);
        socket->setSendTimeout(timeout_ms);
        socket->setRecvTimeout(timeout_ms);

        if (transport) {
          for (int retry = 0; true; ++retry) {
            try {
              transport->open();
              break;
            }
            catch (apache::thrift::TException&) {
              if (retry >= maxRetryConnect)
                break;
              Sleep(250);
            }
          }
        }
      }
      catch (...) {
      }
    }

    class Lock {
    public:

      inline Lock(ThriftClient* _client)
      : client(_client) {
        client->lock();
      }

      inline ~Lock() {
        client->unlock();
      }

    private:

      ThriftClient* client;
    };
    friend class Lock;

  private:

    enum {
      maxRetryConnect = 4,
      maxConnectionPoolSize = 5
    };

    typedef ::boost::intrusive_ptr<ThriftClient> ClientPtr;
    typedef std::vector<ClientPtr> client_pool_t;

    inline void lock() {
      ::EnterCriticalSection(&cs);
      _InterlockedIncrement( &locked );
    }

    inline void unlock( ) {
       _InterlockedDecrement( &locked );
      ::LeaveCriticalSection(&cs);
    }

    CRITICAL_SECTION cs;
    volatile long locked;
    std::string host;
    int port;
    int conn_timeout_ms;
    int timeout_ms;
    client_pool_t client_pool;
    int next_client;
};

typedef ::boost::intrusive_ptr<ThriftClient> ClientPtr;

#endif

}} // namespace Hypertable::Thrift

#ifdef _WIN32
#pragma warning( pop )
#endif

#endif /* HYPERTABLE_THRIFT_CLIENT_H */
