/** -*- C++ -*-
 * Copyright (C) 2010-2014 Thalmann Software & Consulting, http://www.softdev.ch
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

#ifndef HYPERTABLE_DFSBROKER_LOCALFILESYSTEM_H
#define HYPERTABLE_DFSBROKER_LOCALFILESYSTEM_H

#ifndef _WIN32
#error Platform isn't supported
#endif

#include <unordered_map>
#include <set>
#include <deque>
#include <boost/thread/condition.hpp>

#include "Common/Mutex.h"
#include "Common/Properties.h"
#include "Common/Filesystem.h"
#include "Common/atomic.h"
#include "Common/Thread.h"
#include "AsyncComm/CommBuf.h"

#include "Protocol.h"

namespace Hypertable { 

  class ClientBufferedReaderHandler;

  namespace FsBroker {

    /** Ebedded filesystem class for single node usage.
     */
    class EmbeddedFilesystem : public Filesystem {
    public:

      virtual ~EmbeddedFilesystem();

      EmbeddedFilesystem(PropertiesPtr &cfg);

      virtual void open(const String &name, uint32_t flags, DispatchHandler *handler);
      virtual int open(const String &name, uint32_t flag);
      virtual int open_buffered(const String &name, uint32_t flags, uint32_t buf_size,
                                uint32_t outstanding, uint64_t start_offset=0,
                                uint64_t end_offset=0);

      virtual void create(const String &name, uint32_t flags,
                          int32_t bufsz, int32_t replication,
                          int64_t blksz, DispatchHandler *handler);
      virtual int create(const String &name, uint32_t flags, int32_t bufsz,
                         int32_t replication, int64_t blksz);

      virtual void close(int fd, DispatchHandler *handler);
      virtual void close(int fd);

      virtual void read(int fd, size_t amount, DispatchHandler *handler);
      virtual size_t read(int fd, void *dst, size_t amount);

      virtual void append(int fd, StaticBuffer &buffer, uint32_t flags,
                          DispatchHandler *handler);
      virtual size_t append(int fd, StaticBuffer &buffer,
                            uint32_t flags = 0);

      virtual void seek(int fd, uint64_t offset, DispatchHandler *handler);
      virtual void seek(int fd, uint64_t offset);

      virtual void remove(const String &name, DispatchHandler *handler);
      virtual void remove(const String &name, bool force = true);

      virtual void length(const String &name, bool accurate, DispatchHandler *handler);
      virtual int64_t length(const String &name);

      virtual void pread(int fd, size_t len, uint64_t offset,
                         DispatchHandler *handler);
      virtual size_t pread(int fd, void *dst, size_t len, uint64_t offset,
                           bool verify_checksum=true);

      virtual void mkdirs(const String &name, DispatchHandler *handler);
      virtual void mkdirs(const String &name);

      virtual void flush(int fd, DispatchHandler *handler);
      virtual void flush(int fd);

      virtual void rmdir(const String &name, DispatchHandler *handler);
      virtual void rmdir(const String &name, bool force = true);

      virtual void readdir(const String &name, DispatchHandler *handler);
      virtual void readdir(const String &name, std::vector<Dirent> &listing);

      virtual void posix_readdir(const String &name, std::vector<DirectoryEntry> &listing);

      virtual void exists(const String &name, DispatchHandler *handler);
      virtual bool exists(const String &name);

      virtual void rename(const String &src, const String &dst,
                          DispatchHandler *handler);
      virtual void rename(const String &src, const String &dst);

      virtual void debug(int32_t command, StaticBuffer &serialized_parameters);
      virtual void debug(int32_t command, StaticBuffer &serialized_parameters,
                         DispatchHandler *handler);

    private:

      struct WorkerAsyncIO {
        EmbeddedFilesystem* fs;
        void operator()() {
          fs->worker_async_io();
        }
        WorkerAsyncIO(EmbeddedFilesystem *_fs) : fs(_fs) {}
      };
      friend struct WorkerAsyncIO;

      struct Request {
        enum { fdWrite = 0, fdRead = -1 };
        int fd;
        Hypertable::CommBufPtr cbp;
        DispatchHandler *handler;

        Request() : fd(0), handler(0) {}
        Request(int _fd, CommBufPtr &_cbp, DispatchHandler *_handler)
        : fd(_fd), cbp(_cbp), handler(_handler) {}
      };

      class RequestQueue {
      public:
        RequestQueue(RecMutex& mutex);

        void enqueue(const Request &request);
        bool dequeue(Request &request);
        void sync(int fd);
        void completed(int fd);
        void shutdown(bool shutdown);

      private:
        bool can_dequeue() const;
        bool dequeue_request(Request &request);
        bool empty(int fd) const;

        typedef std::deque<Request> Queue;
        typedef std::map<int, Queue> FdQueueMap;
        typedef std::set<int> FdSet;

        FdQueueMap m_fd_queue_map;
        FdSet m_fd_in_progress;
        RecMutex& m_mutex;
        boost::condition m_fd_cond;
        boost::condition m_fd_completed_cond;
        bool m_shutdown;
      };

      class FdSyncGuard {
      public:
        FdSyncGuard(EmbeddedFilesystem* fs, int fd, bool sync);
        ~FdSyncGuard();
      private:
        FdSyncGuard(const FdSyncGuard&);
        FdSyncGuard& operator =(const FdSyncGuard&);

        EmbeddedFilesystem* m_fs;
        int m_fd;
        bool m_sync;
      };

      EmbeddedFilesystem(const EmbeddedFilesystem&);
      EmbeddedFilesystem& operator =(const EmbeddedFilesystem&);

      void worker_async_io();
      void enqueue_message(int fd, Hypertable::CommBufPtr &cbp_request, DispatchHandler *handler);
      void process_message(Hypertable::CommBufPtr &cbp_request, DispatchHandler *handler);

      int open(const String &name, uint32_t flags, bool sync);
      int create(const String &name, uint32_t flags, int32_t bufsz,
                         int32_t replication, int64_t blksz, bool sync);
      void close(int fd, bool sync);
      void seek(int fd, uint64_t offset, bool sync);
      size_t read(int fd, void *dst, size_t amount, uint64_t* offset, bool sync);
      size_t append(int fd, StaticBuffer &buffer, uint32_t flags, uint64_t* offset, bool sync);
      void remove(const String &name, bool force, bool sync);
      int64_t length(const String &name, bool sync);
      size_t pread(int fd, void *dst, size_t len, uint64_t offset, bool verify_checksum, bool sync);
      void mkdirs(const String &name, bool sync);
      void flush(int fd, bool sync);
      void rmdir(const String &name, bool force, bool sync);
      void readdir(const String &name, std::vector<Dirent> &listing, bool sync);
      void posix_readdir(const String &name, std::vector<DirectoryEntry> &listing, bool sync);
      bool exists(const String &name, bool sync);
      void rename(const String &src, const String &dst, bool sync);

      static bool remove_dir(const String& absdir);
      void throw_error();

      inline void set_handle(int fd, ::HANDLE h, uint32_t flags) {
        ScopedRecLock lock(m_mutex);
        m_handles[fd] = std::make_pair(h, flags);
      }

      inline ::HANDLE get_handle(int fd) {
        ScopedRecLock lock(m_mutex);
        return m_handles[fd].first;
      }

      inline ::HANDLE get_handle(int fd, uint32_t& flags) {
        ScopedRecLock lock(m_mutex);
        std::pair<::HANDLE, uint32_t> item = m_handles[fd];
        flags = item.second;
        return item.first;
      }

      inline void close_handle(int fd) {
        ::HANDLE h;
        {
          ScopedRecLock lock(m_mutex);
          std::map<int, std::pair<::HANDLE, uint32_t>>::iterator it = m_handles.find(fd);
          h = it->second.first;
          m_handles.erase(it);
        }
        ::CloseHandle(h);
      }

      static atomic_t ms_next_fd;
      RecMutex m_mutex;
      std::map<int, std::pair<::HANDLE, uint32_t>> m_handles;

      String m_rootdir;
      bool m_directio;
      bool m_no_removal;
      bool m_asyncio;
      Protocol m_protocol;
      ThreadGroup m_asyncio_thread;
      RequestQueue m_request_queue;

      typedef std::unordered_map<int, ClientBufferedReaderHandler *>
          BufferedReaderMap;
      BufferedReaderMap m_buffered_reader_map;
    };

    typedef intrusive_ptr<EmbeddedFilesystem> EmbeddedFilesystemPtr;

}} // namespace Hypertable::DfsBroker


#endif // HYPERTABLE_DFSBROKER_LOCALFILESYSTEM_H

