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

#include "Common/Compat.h"

#ifndef _WIN32
#error Platform isn't supported
#endif

#include <boost/algorithm/string.hpp>

#include "Common/Error.h"
#include "Common/Filesystem.h"
#include "Common/Logger.h"
#include "Common/FileUtils.h"
#include "Common/Path.h"
#include "Common/String.h"
#include "Common/Serialization.h"
#include "Common/SystemInfo.h"

#include "AsyncComm/Protocol.h"

#include "EmbeddedFilesystem.h"
#include "ClientBufferedReaderHandler.h"

#include "Request/Handler/Factory.h"
#include "Request/Parameters/Append.h"
#include "Request/Parameters/Close.h"
#include "Request/Parameters/Create.h"
#include "Request/Parameters/Debug.h"
#include "Request/Parameters/Exists.h"
#include "Request/Parameters/Flush.h"
#include "Request/Parameters/Length.h"
#include "Request/Parameters/Mkdirs.h"
#include "Request/Parameters/Open.h"
#include "Request/Parameters/Pread.h"
#include "Request/Parameters/Readdir.h"
#include "Request/Parameters/Read.h"
#include "Request/Parameters/Remove.h"
#include "Request/Parameters/Rename.h"
#include "Request/Parameters/Rmdir.h"
#include "Request/Parameters/Seek.h"
#include "Request/Parameters/Shutdown.h"
#include "Response/Parameters/Append.h"
#include "Response/Parameters/Exists.h"
#include "Response/Parameters/Length.h"
#include "Response/Parameters/Open.h"
#include "Response/Parameters/Read.h"
#include "Response/Parameters/Readdir.h"
#include "Response/Parameters/Status.h"

#define IO_ERROR \
    winapi_strerror(::GetLastError())

namespace {

  inline uint64_t SetFilePointer(HANDLE hf, __int64 distance, DWORD dwMoveMethod) {
    LARGE_INTEGER li;
    li.QuadPart = distance;
    li.LowPart = ::SetFilePointer(hf, li.LowPart, &li.HighPart, dwMoveMethod);
    if (li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
      li.QuadPart = -1;
    return li.QuadPart;
  }

}

using namespace Hypertable;
using namespace Serialization;
using namespace Hypertable::FsBroker;
using namespace Hypertable::FsBroker::Lib;

atomic_t EmbeddedFilesystem::ms_next_fd = ATOMIC_INIT(0);

EmbeddedFilesystem::EmbeddedFilesystem(PropertiesPtr &cfg)
  : m_directio(false), m_asyncio(true), m_request_queue(m_mutex) {
  m_directio = cfg->get_bool(cfg->has("FsBroker.Local.DirectIO") ? "FsBroker.Local.DirectIO" : "DfsBroker.Local.DirectIO");
  m_no_removal = cfg->get_bool("FsBroker.DisableFileRemoval");
  m_asyncio = cfg->get_bool("FsBroker.Local.Embedded.AsyncIO");

  Path root = cfg->get_str(cfg->has("FsBroker.Local.Root") ? "FsBroker.Local.Root" : "DfsBroker.Local.Root", "fs/local");
  if (!root.is_complete()) {
    Path data_dir = cfg->get_str("Hypertable.DataDirectory");
    root = data_dir / root;
  }

  m_rootdir = root.string();
  boost::trim_right_if(m_rootdir, boost::is_any_of("/\\"));

  // ensure that root directory exists
  if (!FileUtils::mkdirs(m_rootdir))
    HT_FATALF("mkdirs %s (%s)", m_rootdir.c_str(), winapi_strerror(::GetLastError()));

  // create async i/o threads
  if (m_asyncio) {
    int workers = std::max(2, System::cpu_info().total_cores);
    for (int i = 0; i < workers; ++i)
      m_asyncio_thread.create_thread(WorkerAsyncIO(this));
  }
}

EmbeddedFilesystem::~EmbeddedFilesystem() {
  if (m_asyncio) {
    m_request_queue.shutdown(true);
    m_asyncio_thread.join_all();

    ScopedRecLock lock(m_mutex);
    for (BufferedReaderMap::iterator iter = m_buffered_reader_map.begin(); iter != m_buffered_reader_map.end(); ++iter)
      delete iter->second;
  }
}

void EmbeddedFilesystem::open(const String &name, uint32_t flags, DispatchHandler *handler) {
  try {
    CommHeader header(Lib::Request::Handler::Factory::FUNCTION_OPEN);
    Lib::Request::Parameters::Open params(name, flags, 0);
    CommBufPtr cbuf( new CommBuf(header, params.encoded_length()) );
    params.encode(cbuf->get_data_ptr_address());

    enqueue_message(Request::fdRead, cbuf, handler);
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error opening DFS file: %s", name.c_str());
  }
}

int EmbeddedFilesystem::open(const String &name, uint32_t flags) {
  return open(name, flags, m_asyncio);
}

int EmbeddedFilesystem::open_buffered(const String &name, uint32_t flags, uint32_t buf_size,
                      uint32_t outstanding, uint64_t start_offset,
                      uint64_t end_offset) {
  try {
    HT_ASSERT((flags & Filesystem::OPEN_FLAG_DIRECTIO) == 0 ||
              (HT_IO_ALIGNED(buf_size) &&
               HT_IO_ALIGNED(start_offset) &&
               HT_IO_ALIGNED(end_offset)));

    int fd = open(name, flags, false);

    if (m_asyncio) {
      ScopedRecLock lock(m_mutex);
      HT_ASSERT(m_buffered_reader_map.find(fd) == m_buffered_reader_map.end());
      m_buffered_reader_map[fd] =
          new FsBroker::Lib::ClientBufferedReaderHandler(this, fd, buf_size, outstanding,
                                          start_offset, end_offset);
    }

    return fd;
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error opening buffered DFS file=%s buf_size=%u "
        "outstanding=%u start_offset=%llu end_offset=%llu", name.c_str(),
        buf_size, outstanding, (Llu)start_offset, (Llu)end_offset);
  }
}

void EmbeddedFilesystem::decode_response_open(EventPtr &event, int32_t *fd) {
  int error = Protocol::response_code(event);
  if (error != Error::OK)
    HT_THROW(error, Protocol::string_format_message(event));

  const uint8_t *ptr = event->payload + 4;
  size_t remain = event->payload_len - 4;

  Lib::Response::Parameters::Open params;
  params.decode(&ptr, &remain);
  *fd = params.get_fd();
}

void EmbeddedFilesystem::create(const String &name, uint32_t flags, int32_t bufsz,
               int32_t replication, int64_t blksz,
               DispatchHandler *handler) {
  try {
    CommHeader header(Lib::Request::Handler::Factory::FUNCTION_CREATE);
    Lib::Request::Parameters::Create params(name, flags, bufsz, replication, blksz);
    CommBufPtr cbuf( new CommBuf(header, params.encoded_length()) );
    params.encode(cbuf->get_data_ptr_address());

    enqueue_message(Request::fdWrite, cbuf, handler);
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error creating DFS file: %s:", name.c_str());
  }
}

int EmbeddedFilesystem::create(const String &name, uint32_t flags, int32_t bufsz,
               int32_t replication, int64_t blksz) {
  return create(name, flags, bufsz, replication, blksz, m_asyncio);
}

void EmbeddedFilesystem::decode_response_create(EventPtr &event, int32_t *fd) {
  decode_response_open(event, fd);
}

void EmbeddedFilesystem::close(int fd, DispatchHandler *handler) {
  try {
    if (!handler)
      close(fd);
    else {
      CommHeader header(Lib::Request::Handler::Factory::FUNCTION_CLOSE);
      header.gid = fd;
      Lib::Request::Parameters::Close params(fd);
      CommBufPtr cbuf( new CommBuf(header, params.encoded_length()) );

      enqueue_message(fd, cbuf, handler);
    }
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error closing DFS fd: %d", fd);
  }
}

void EmbeddedFilesystem::close(int fd) {
  close(fd, m_asyncio);
}

void EmbeddedFilesystem::read(int fd, size_t len, DispatchHandler *handler) {
  try {
    CommHeader header(Lib::Request::Handler::Factory::FUNCTION_READ);
    header.gid = fd;
    Lib::Request::Parameters::Read params(fd, len);
    CommBufPtr cbuf( new CommBuf(header, params.encoded_length()) );

    enqueue_message(fd, cbuf, handler);
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error sending read request for %u bytes "
               "from DFS fd: %d", (unsigned)len, fd);
  }
}

size_t EmbeddedFilesystem::read(int fd, void *dst, size_t len) {
  if (m_asyncio) {
    FsBroker::Lib::ClientBufferedReaderHandler *reader_handler = 0;
    {
      ScopedRecLock lock(m_mutex);
      BufferedReaderMap::iterator iter = m_buffered_reader_map.find(fd);
      if (iter != m_buffered_reader_map.end())
        reader_handler = (*iter).second;
    }
    if (reader_handler) {
      try {
        return reader_handler->read(dst, len);
      }
      catch (Exception &e) {
        HT_THROW2F(e.code(), e, "Error reading %u bytes from buffered DFS fd %d",
                   (unsigned)len, fd);
      }
    }
  }
  return read(fd, dst, len, 0, m_asyncio);
}

void EmbeddedFilesystem::decode_response_read(EventPtr &event, const void **buffer,
                                      uint64_t *offset, uint32_t *length) {
  int error = Protocol::response_code(event);
  if (error != Error::OK)
    HT_THROW(error, Protocol::string_format_message(event));

  const uint8_t *ptr = event->payload + 4;
  size_t remain = event->payload_len - 4;

  Lib::Response::Parameters::Read params;
  params.decode(&ptr, &remain);
  *offset = params.get_offset();
  *length = params.get_amount();

  if (*length == (uint32_t)-1) {
    *length = 0;
    return;
  }

  if (remain < (size_t)*length)
    HT_THROWF(Error::RESPONSE_TRUNCATED, "%lu < %lu", (Lu)remain, (Lu)*length);

  *buffer = ptr;
}

void EmbeddedFilesystem::append(int fd, StaticBuffer &buffer, uint32_t flags,
               DispatchHandler *handler) {
  try {
    if (!handler)
      append(fd, buffer, flags);
    else {
      CommHeader header(Lib::Request::Handler::Factory::FUNCTION_APPEND);
      header.gid = fd;
      header.alignment = HT_DIRECT_IO_ALIGNMENT;
      CommBuf *cbuf = new CommBuf(header, HT_DIRECT_IO_ALIGNMENT, buffer);
      Lib::Request::Parameters::Append params(fd, buffer.size, (bool)(flags & O_FLUSH));
      uint8_t *base = (uint8_t *)cbuf->get_data_ptr();
      params.encode(cbuf->get_data_ptr_address());
      size_t padding = HT_DIRECT_IO_ALIGNMENT -
        (((uint8_t *)cbuf->get_data_ptr()) - base);
      memset(cbuf->get_data_ptr(), 0, padding);
      cbuf->advance_data_ptr(padding);

      CommBufPtr cbp(cbuf);
      enqueue_message(fd, cbp, handler);
    }
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error appending %u bytes to DFS fd %d",
               (unsigned)buffer.size, fd);
  }
}

size_t EmbeddedFilesystem::append(int fd, StaticBuffer &buffer, uint32_t flags) {
  return append(fd, buffer, flags, 0, m_asyncio);
}

void EmbeddedFilesystem::decode_response_append(EventPtr &event, uint64_t *offset,
                                        uint32_t *length) {
  int error = Protocol::response_code(event);
  if (error != Error::OK)
    HT_THROW(error, Protocol::string_format_message(event));

  const uint8_t *ptr = event->payload + 4;
  size_t remain = event->payload_len - 4;

  Lib::Response::Parameters::Append params;
  params.decode(&ptr, &remain);
  *offset = params.get_offset();
  *length = params.get_amount();
}

void EmbeddedFilesystem::seek(int fd, uint64_t offset, DispatchHandler *handler) {
  try {
    if (!handler)
      seek(fd, offset);
    else {
      CommHeader header(Lib::Request::Handler::Factory::FUNCTION_SEEK);
      header.gid = fd;
      Lib::Request::Parameters::Seek params(fd, offset);
      CommBufPtr cbuf( new CommBuf(header, params.encoded_length()) );
      params.encode(cbuf->get_data_ptr_address());

      enqueue_message(fd, cbuf, handler);
    }
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error seeking to %llu on DFS fd %d",
               (Llu)offset, fd);
  }
}

void EmbeddedFilesystem::seek(int fd, uint64_t offset) {
  seek(fd, offset, m_asyncio);
}

void EmbeddedFilesystem::remove(const String &name, DispatchHandler *handler) {
  try {
    if (!handler)
      remove(name);
    else {
      CommHeader header(Lib::Request::Handler::Factory::FUNCTION_REMOVE);
      Lib::Request::Parameters::Remove params(name);
      CommBufPtr cbuf( new CommBuf(header, params.encoded_length()) );
      params.encode(cbuf->get_data_ptr_address());
      enqueue_message(Request::fdWrite, cbuf, handler);
    }
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error removing DFS file: %s", name.c_str());
  }
}

void EmbeddedFilesystem::remove(const String &name, bool force) {
  remove(name, force, m_asyncio);
}

void EmbeddedFilesystem::length(const String &name, bool accurate, DispatchHandler *handler) {
  try {
    CommHeader header(Lib::Request::Handler::Factory::FUNCTION_LENGTH);
    Lib::Request::Parameters::Length params(name, accurate);
    CommBufPtr cbuf( new CommBuf(header, params.encoded_length()) );
    params.encode(cbuf->get_data_ptr_address());

    enqueue_message(Request::fdRead, cbuf, handler);
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error sending length request for DFS file: %s",
               name.c_str());
  }
}

int64_t EmbeddedFilesystem::length(const String &name) {
  return length(name, m_asyncio);
}

int64_t EmbeddedFilesystem::decode_response_length(EventPtr &event) {
  int error = Protocol::response_code(event);
  if (error != Error::OK)
    HT_THROW(error, Protocol::string_format_message(event));

  const uint8_t *ptr = event->payload + 4;
  size_t remain = event->payload_len - 4;

  Lib::Response::Parameters::Length params;
  params.decode(&ptr, &remain);
  return params.get_length();
}

void EmbeddedFilesystem::pread(int fd, size_t len, uint64_t offset,
              DispatchHandler *handler) {
  try {
    CommHeader header(Lib::Request::Handler::Factory::FUNCTION_PREAD);
    header.gid = fd;
    Lib::Request::Parameters::Pread params(fd, offset, len, true);
    CommBufPtr cbuf( new CommBuf(header, params.encoded_length()) );
    params.encode(cbuf->get_data_ptr_address());

    enqueue_message(fd, cbuf, handler);
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error sending pread request at byte %llu "
               "on DFS fd %d", (Llu)offset, fd);
  }
}

size_t EmbeddedFilesystem::pread(int fd, void *dst, size_t len, uint64_t offset, bool verify_checksum) {
  return pread(fd, dst, len, offset, verify_checksum, m_asyncio);
}

void EmbeddedFilesystem::decode_response_pread(EventPtr &event, const void **buffer,
                                       uint64_t *offset, uint32_t *length) {
  decode_response_read(event, buffer, offset, length);
}

void EmbeddedFilesystem::mkdirs(const String &name, DispatchHandler *handler) {
  try {
    if (!handler)
      mkdirs(name);
    else {
      CommHeader header(Lib::Request::Handler::Factory::FUNCTION_MKDIRS);
      Lib::Request::Parameters::Mkdirs params(name);
      CommBufPtr cbuf( new CommBuf(header, params.encoded_length()) );
      params.encode(cbuf->get_data_ptr_address());

      enqueue_message(Request::fdWrite, cbuf, handler);
    }
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error sending mkdirs request for DFS "
               "directory: %s", name.c_str());
  }
}

void EmbeddedFilesystem::mkdirs(const String &name) {
  mkdirs(name, m_asyncio);
}

void EmbeddedFilesystem::flush(int fd, DispatchHandler *handler) {
  try {
    if (!handler)
      flush(fd);
    else {
      CommHeader header(Lib::Request::Handler::Factory::FUNCTION_FLUSH);
      header.gid = fd;
      Lib::Request::Parameters::Flush params(fd);
      CommBufPtr cbuf( new CommBuf(header, params.encoded_length()) );
      params.encode(cbuf->get_data_ptr_address());

      enqueue_message(fd, cbuf, handler);
    }
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error flushing DFS fd %d", fd);
  }
}

void EmbeddedFilesystem::flush(int fd) {
  flush(fd, m_asyncio);
}

void EmbeddedFilesystem::rmdir(const String &name, DispatchHandler *handler) {
  try {
    if (!handler)
      rmdir(name);
    else {
      CommHeader header(Lib::Request::Handler::Factory::FUNCTION_RMDIR);
      Lib::Request::Parameters::Rmdir params(name);
      CommBufPtr cbuf( new CommBuf(header, params.encoded_length()) );
      params.encode(cbuf->get_data_ptr_address());

      enqueue_message(Request::fdWrite, cbuf, handler);
    }
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error sending rmdir request for DFS directory: "
               "%s", name.c_str());
  }
}

void EmbeddedFilesystem::rmdir(const String &name, bool force) {
  rmdir(name, force, m_asyncio);
}

void EmbeddedFilesystem::readdir(const String &name, DispatchHandler *handler) {
  try {
    CommHeader header(Lib::Request::Handler::Factory::FUNCTION_READDIR);
    Lib::Request::Parameters::Readdir params(name);
    CommBufPtr cbuf( new CommBuf(header, params.encoded_length()) );
    params.encode(cbuf->get_data_ptr_address());

    enqueue_message(Request::fdRead, cbuf, handler);
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error sending readdir request for DFS directory"
               ": %s", name.c_str());
  }
}

void EmbeddedFilesystem::readdir(const String &name, std::vector<Dirent> &listing) {
  readdir(name, listing, m_asyncio);
}

void EmbeddedFilesystem::decode_response_readdir(EventPtr &event,
                                         std::vector<Dirent> &listing) {
  int error = Protocol::response_code(event);
  if (error != Error::OK)
    HT_THROW(error, Protocol::string_format_message(event));

  const uint8_t *ptr = event->payload + 4;
  size_t remain = event->payload_len - 4;

  Lib::Response::Parameters::Readdir params;
  params.decode(&ptr, &remain);
  params.get_listing(listing);
}

void EmbeddedFilesystem::exists(const String &name, DispatchHandler *handler) {
  try {
    CommHeader header(Lib::Request::Handler::Factory::FUNCTION_EXISTS);
    Lib::Request::Parameters::Exists params(name);
    CommBufPtr cbuf( new CommBuf(header, params.encoded_length()) );
    params.encode(cbuf->get_data_ptr_address());

    enqueue_message(Request::fdRead, cbuf, handler);
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "sending 'exists' request for DFS path: %s",
               name.c_str());
  }
}

bool EmbeddedFilesystem::exists(const String &name) {
  return exists(name, m_asyncio);
}

bool EmbeddedFilesystem::decode_response_exists(EventPtr &event) {
  int error = Protocol::response_code(event);
  if (error != Error::OK)
    HT_THROW(error, Protocol::string_format_message(event));

  const uint8_t *ptr = event->payload + 4;
  size_t remain = event->payload_len - 4;

  Lib::Response::Parameters::Exists params;
  params.decode(&ptr, &remain);
  return params.get_exists();
}

void EmbeddedFilesystem::rename(const String &src, const String &dst, DispatchHandler *handler) {
  try {
    if (!handler)
      rename(src, dst);
    else {
      CommHeader header(Lib::Request::Handler::Factory::FUNCTION_RENAME);
      Lib::Request::Parameters::Rename params(src, dst);
      CommBufPtr cbuf( new CommBuf(header, params.encoded_length()) );
      params.encode(cbuf->get_data_ptr_address());

      enqueue_message(Request::fdWrite, cbuf, handler);
    }
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error sending 'rename' request for DFS "
               "path: %s -> %s", src.c_str(), dst.c_str());
  }
}

void EmbeddedFilesystem::rename(const String &src, const String &dst) {
  rename(src, dst, m_asyncio);
}

void EmbeddedFilesystem::status(Status &status, Timer *timer) {
}

void EmbeddedFilesystem::decode_response_status(EventPtr &event, Status &status) {
  int error = Protocol::response_code(event);
  if (error != Error::OK)
    HT_THROW(error, Protocol::string_format_message(event));

  const uint8_t *ptr = event->payload + 4;
  size_t remain = event->payload_len - 4;

  Lib::Response::Parameters::Status params;
  params.decode(&ptr, &remain);
  status = params.status();
}

void EmbeddedFilesystem::debug(int32_t command, StaticBuffer &serialized_parameters,
              DispatchHandler *handler) {
  HT_FATAL("Not implemented");
}

void EmbeddedFilesystem::debug(int32_t command, StaticBuffer &serialized_parameters) {
  HT_FATAL("Not implemented");
}

void EmbeddedFilesystem::worker_async_io() {
  Request request;
  while (m_request_queue.dequeue(request)) {
    try {
      process_message(request.cbp, request.handler);
    }
    catch (...) {
      HT_ERRORF("Unexpected");
    }

    m_request_queue.completed(request.fd);
  }
}

void EmbeddedFilesystem::enqueue_message(int fd, CommBufPtr &cbp_request, DispatchHandler *handler) {
  if (m_asyncio) {
    m_request_queue.enqueue(Request(fd, cbp_request, handler));
  }
  else
    process_message(cbp_request, handler);
}

void EmbeddedFilesystem::process_message(CommBufPtr &cbp_request, DispatchHandler *handler) {
  DynamicBuffer response(0, false);
  try {
    size_t header_len = cbp_request->header.encoded_length();
    const uint8_t *decode_ptr = cbp_request->data.base + header_len;
    size_t decode_remain = cbp_request->data.size - header_len;

    switch (cbp_request->header.command) {
    case Lib::Request::Handler::Factory::FUNCTION_OPEN:
      {
        uint32_t flags = decode_i32(&decode_ptr, &decode_remain);
        uint32_t bufsz = decode_i32(&decode_ptr, &decode_remain);
        const char *fname = decode_str16(&decode_ptr, &decode_remain);
        // validate filename
        if (fname[strlen(fname)-1] == '/')
          HT_THROWF(Error::FSBROKER_BAD_FILENAME, "bad filename: %s", fname);
        int fd = open(fname, flags, false);

        response.ensure(8);
        encode_i32(&response.ptr, Error::OK);
        encode_i32(&response.ptr, fd);
      }
      break;
    case Lib::Request::Handler::Factory::FUNCTION_CREATE:
      {
        uint32_t flags = decode_i32(&decode_ptr, &decode_remain);
        int32_t bufsz = decode_i32(&decode_ptr, &decode_remain);
        int32_t replication = decode_i32(&decode_ptr, &decode_remain);
        int64_t blksz = decode_i64(&decode_ptr, &decode_remain);
        const char *fname = decode_str16(&decode_ptr, &decode_remain);
        // validate filename
        if (fname[strlen(fname)-1] == '/')
          HT_THROWF(Error::FSBROKER_BAD_FILENAME, "bad filename: %s", fname);
        int fd = create(fname, flags, bufsz, replication, blksz, false);

        response.ensure(8);
        encode_i32(&response.ptr, Error::OK);
        encode_i32(&response.ptr, fd);
      }
      break;
    case Lib::Request::Handler::Factory::FUNCTION_CLOSE:
      {
        int fd = Serialization::decode_i32(&decode_ptr, &decode_remain);
        close(fd, false);

        response.ensure(4);
        encode_i32(&response.ptr, Error::OK);
      }
      break;
    case Lib::Request::Handler::Factory::FUNCTION_READ:
      {
        int fd = decode_i32(&decode_ptr, &decode_remain);
        uint32_t amount = decode_i32(&decode_ptr, &decode_remain);
        response.ensure(16 + amount);
        uint64_t offset;
        size_t nread = read(fd, response.base + 16, amount, &offset, false);

        encode_i32(&response.ptr, Error::OK);
        encode_i64(&response.ptr, offset);
        encode_i32(&response.ptr, amount);
      }
      break;
    case Lib::Request::Handler::Factory::FUNCTION_APPEND:
      {
        int fd = decode_i32(&decode_ptr, &decode_remain);
        uint32_t amount = decode_i32(&decode_ptr, &decode_remain);
        bool flush = decode_bool(&decode_ptr, &decode_remain);
        uint64_t offset;
        append(fd, cbp_request->ext, flush ? O_FLUSH : 0, &offset, false);

        response.ensure(16);
        encode_i32(&response.ptr, Error::OK);
        encode_i64(&response.ptr, offset);
        encode_i32(&response.ptr, amount);
      }
      break;
    case Lib::Request::Handler::Factory::FUNCTION_SEEK:
      {
        int fd = decode_i32(&decode_ptr, &decode_remain);
        uint64_t offset = decode_i64(&decode_ptr, &decode_remain);
        seek(fd, offset, false);

        response.ensure(4);
        encode_i32(&response.ptr, Error::OK);
      }
      break;
    case Lib::Request::Handler::Factory::FUNCTION_REMOVE:
      {
        const char *fname = decode_str16(&decode_ptr, &decode_remain);
        remove(fname, true, false);

        response.ensure(4);
        encode_i32(&response.ptr, Error::OK);
      }
      break;
    case Lib::Request::Handler::Factory::FUNCTION_LENGTH:
      {
        const char *fname = decode_str16(&decode_ptr, &decode_remain);
        bool accurate = decode_bool(&decode_ptr, &decode_remain);
        int64_t len = length(fname, false);

        response.ensure(12);
        encode_i32(&response.ptr, Error::OK);
        encode_i64(&response.ptr, len);
      }
      break;
    case Lib::Request::Handler::Factory::FUNCTION_PREAD:
      {
        int fd = decode_i32(&decode_ptr, &decode_remain);
        uint64_t offset = decode_i64(&decode_ptr, &decode_remain);
        uint32_t amount = decode_i32(&decode_ptr, &decode_remain);
        bool verify_checksum = decode_bool(&decode_ptr, &decode_remain);
        response.ensure(16 + amount);
        size_t nread = pread(fd, response.base + 16, offset, amount, verify_checksum, false);

        encode_i32(&response.ptr, Error::OK);
        encode_i64(&response.ptr, offset);
        encode_i32(&response.ptr, amount);
      }
      break;
    case Lib::Request::Handler::Factory::FUNCTION_MKDIRS:
      {
        const char *dname = decode_str16(&decode_ptr, &decode_remain);
        mkdirs(dname, false);

        response.ensure(4);
        encode_i32(&response.ptr, Error::OK);
      }
      break;
    case Lib::Request::Handler::Factory::FUNCTION_FLUSH:
      {
        int fd = decode_i32(&decode_ptr, &decode_remain);
        flush(fd, false);

        response.ensure(4);
        encode_i32(&response.ptr, Error::OK);
      }
      break;
    case Lib::Request::Handler::Factory::FUNCTION_RMDIR:
      {
        const char *dname = decode_str16(&decode_ptr, &decode_remain);
        rmdir(dname, true, false);

        response.ensure(4);
        encode_i32(&response.ptr, Error::OK);
      }
      break;
    case Lib::Request::Handler::Factory::FUNCTION_READDIR:
      {
        const char *dname = decode_str16(&decode_ptr, &decode_remain);
        std::vector<Dirent> listing;
        readdir(dname, listing, false);

        uint32_t len = 8;
        for each (const Dirent& d in listing)
          len += d.encoded_length();
        response.ensure(len);
        encode_i32(&response.ptr, Error::OK);
        encode_i32(&response.ptr, listing.size());
        for each (const Dirent& d in listing)
          d.encode(&response.ptr);
      }
      break;
    case Lib::Request::Handler::Factory::FUNCTION_EXISTS:
      {
        const char *fname = decode_str16(&decode_ptr, &decode_remain);
        bool exist = exists(fname, false);

        response.ensure(5);
        encode_i32(&response.ptr, Error::OK);
        encode_bool(&response.ptr, exist);
      }
      break;
    case Lib::Request::Handler::Factory::FUNCTION_RENAME:
      {
        const char *src = decode_str16(&decode_ptr, &decode_remain);
        const char *dst = decode_str16(&decode_ptr, &decode_remain);
        rename(src, dst, false);

        response.ensure(4);
        encode_i32(&response.ptr, Error::OK);
      }
      break;
    default:
      HT_THROWF(Error::PROTOCOL_ERROR, "Unimplemented command (%llx)",
                (Llu)cbp_request->header.command);
    }
  }
  catch (Exception &e) {
    HT_ERROR_OUT << e << HT_END;
    String errmsg = format("%s - %s", e.what(), Error::get_text(e.code()));
    size_t max_msg_size = std::numeric_limits<int16_t>::max();
    if (errmsg.length() >= max_msg_size)
      errmsg = errmsg.substr(0, max_msg_size);

    response.ensure(4 + encoded_length_str16(errmsg));
    encode_i32(&response.ptr, e.code());
    encode_str16(&response.ptr, errmsg);
  }

  EventPtr event = make_shared<Event>(Event::MESSAGE);
  event->header.initialize_from_request_header(cbp_request->header);
  event->payload = response.base;
  event->payload_len = response.size;
  handler->handle(event);
}

int EmbeddedFilesystem::open(const String &name, uint32_t flags, bool sync) {
  try {
    FdSyncGuard guard(this, Request::fdWrite, sync);

    String abspath;
    if (name[0] == '/')
      abspath = m_rootdir + name;
    else
      abspath = m_rootdir + "/" + name;

    HANDLE h;
    int fd = atomic_inc_return(&ms_next_fd);

    //DWORD flagsAndAttributes = m_directio && (flags & Filesystem::OPEN_FLAG_DIRECTIO) ? FILE_FLAG_NO_BUFFERING : FILE_FLAG_RANDOM_ACCESS;
    if ((h = CreateFile(abspath.c_str(), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, 0)) == INVALID_HANDLE_VALUE)
      throw_error();
    set_handle(fd, h, flags);
    return fd;
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error opening DFS file: %s", name.c_str());
  }
}

int EmbeddedFilesystem::create(const String &name, uint32_t flags, int32_t bufsz,
               int32_t replication, int64_t blksz, bool sync) {
  try {
    FdSyncGuard guard(this, Request::fdWrite, sync);

    String abspath;
    if (name[0] == '/')
      abspath = m_rootdir + name;
    else
      abspath = m_rootdir + "/" + name;

    HANDLE h;
    int fd = atomic_inc_return(&ms_next_fd);

    DWORD creationDisposition = flags & Filesystem::OPEN_FLAG_OVERWRITE ? CREATE_ALWAYS : OPEN_ALWAYS;
    DWORD flagsAndAttributes = m_directio && (flags & Filesystem::OPEN_FLAG_DIRECTIO) ? FILE_FLAG_WRITE_THROUGH/*|FILE_FLAG_NO_BUFFERING*/ : 0;
    if ((h = CreateFile(abspath.c_str(), GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_DELETE, 0, creationDisposition, flagsAndAttributes, 0)) == INVALID_HANDLE_VALUE)
      throw_error();
    set_handle(fd, h, flags);
    return fd;
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error creating DFS file: %s", name.c_str());
  }
}

void EmbeddedFilesystem::close(int fd, bool sync) {
  try {
    FdSyncGuard guard(this, fd, sync);

    if (m_asyncio) {
      FsBroker::Lib::ClientBufferedReaderHandler *reader_handler = 0;
      {
        ScopedRecLock lock(m_mutex);
        BufferedReaderMap::iterator iter = m_buffered_reader_map.find(fd);
        if (iter != m_buffered_reader_map.end()) {
          reader_handler = (*iter).second;
          m_buffered_reader_map.erase(iter);
        }
      }
      if (reader_handler)
        delete reader_handler;
    }

    close_handle(fd);
  }
  catch(Exception &e) {
    HT_THROW2F(e.code(), e, "Error closing DFS fd: %d", fd);
  }
}

size_t EmbeddedFilesystem::append(int fd, StaticBuffer &buffer, uint32_t flags, uint64_t* offset, bool sync) {
  try {
    FdSyncGuard guard(this, fd, sync);

    uint32_t hflags;
    HANDLE h = get_handle(fd, hflags);
    if (offset) {
      if ((*offset = SetFilePointer(h, 0, FILE_CURRENT)) == (uint64_t)-1)
        throw_error();
    }
    DWORD nwritten;
    if (!WriteFile(h, buffer.base, buffer.size, &nwritten, 0))
      throw_error();
    if (flags && !(m_directio && (hflags & Filesystem::OPEN_FLAG_DIRECTIO))) { // no sync because handle has FILE_FLAG_WRITE_THROUGH
      if (!::FlushFileBuffers(h))
        throw_error();
    }
    return nwritten;
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error appending %u bytes to DFS fd %d",
               (unsigned)buffer.size, fd);
  }
}

size_t EmbeddedFilesystem::read(int fd, void *dst, size_t len, uint64_t* offset, bool sync) {
  try {
    FdSyncGuard guard(this, fd, sync);

    HANDLE h = get_handle(fd);
    DWORD nread;
    if (!ReadFile(h, dst, len, &nread, 0))
      throw_error();
    if (offset) {
      if ((*offset = SetFilePointer(h, 0, FILE_CURRENT)) == (uint64_t)-1)
        throw_error();
    }
    return nread;
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error reading %u bytes from DFS fd %d",
               (unsigned)len, fd);
  }
}

void EmbeddedFilesystem::seek(int fd, uint64_t offset, bool sync) {
  try {
    FdSyncGuard guard(this, fd, sync);

    if (SetFilePointer(get_handle(fd), offset, FILE_BEGIN) == (uint64_t)-1)
      throw_error();
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error seeking to %llu on DFS fd %d",
               (Llu)offset, fd);
  }
}

void EmbeddedFilesystem::remove(const String &name, bool force, bool sync) {
  try {
    FdSyncGuard guard(this, Request::fdWrite, sync);

    String abspath;
    if (name[0] == '/')
      abspath = m_rootdir + name;
    else
      abspath = m_rootdir + "/" + name;

    if (!DeleteFile(abspath.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
      throw_error();
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error removing DFS file: %s", name.c_str());
  }
}

int64_t EmbeddedFilesystem::length(const String &name, bool sync) {
  try {
    FdSyncGuard guard(this, Request::fdWrite, sync);

    String abspath;
    if (name[0] == '/')
      abspath = m_rootdir + name;
    else
      abspath = m_rootdir + "/" + name;

    uint64_t length;
    if ((length = FileUtils::length(abspath)) == (uint64_t)-1)
      throw_error();
    return length;
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error getting length of DFS file: %s",
               name.c_str());
  }
}

size_t EmbeddedFilesystem::pread(int fd, void *dst, size_t len, uint64_t offset, bool /*verify_checksum*/, bool sync) {
  try {
    FdSyncGuard guard(this, fd, sync);

    ssize_t nread;
    if ((nread = FileUtils::pread(get_handle(fd), dst, len, offset)) != (ssize_t)len)
      throw_error();
    return nread;
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error preading at byte %llu on DFS fd %d",
               (Llu)offset, fd);
  }
}

void EmbeddedFilesystem::mkdirs(const String &name, bool sync) {
  try {
    FdSyncGuard guard(this, Request::fdWrite, sync);

    String absdir;
    if (name[0] == '/')
      absdir = m_rootdir + name;
    else
      absdir = m_rootdir + "/" + name;

    if (!FileUtils::mkdirs(absdir))
      throw_error();
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error mkdirs DFS directory %s", name.c_str());
  }
}

void EmbeddedFilesystem::flush(int fd, bool sync) {
  try {
    FdSyncGuard guard(this, fd, sync);

    if (!::FlushFileBuffers(get_handle(fd)))
      throw_error();
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error flushing DFS fd %d", fd);
  }
}

void EmbeddedFilesystem::rmdir(const String &name, bool force, bool sync) {
  try {
    FdSyncGuard guard(this, Request::fdWrite, sync);

    String absdir;
    if (name[0] == '/')
      absdir = m_rootdir + name;
    else
      absdir = m_rootdir + "/" + name;

    if (FileUtils::exists(absdir)) {
      if (!remove_dir(absdir))
        throw_error();
    }
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error removing DFS directory: %s", name.c_str());
  }
}

void EmbeddedFilesystem::readdir(const String &name, std::vector<Dirent> &listing, bool sync) {
  try {
    FdSyncGuard guard(this, Request::fdWrite, sync);

    String absdir;
    if (name[0] == '/')
      absdir = m_rootdir + name;
    else
      absdir = m_rootdir + "/" + name;

    WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile((absdir + "\\*").c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
      throw_error();

    Dirent entry;
    do {
      if (*ffd.cFileName) {
        if (ffd.cFileName[0] != '.' ||
            (ffd.cFileName[1] != 0 && (ffd.cFileName[1] != '.' || ffd.cFileName[2] != 0))) {
          entry.name = ffd.cFileName;
          entry.is_dir = is_directory(ffd);
          entry.length = get_file_length(ffd);
          entry.last_modification_time = get_last_access_time(ffd);
          if (m_no_removal) {
            size_t len = strlen(ffd.cFileName);
            if (len <= 8 || strcmp(&ffd.cFileName[len-8], ".deleted"))
              listing.push_back(entry);
          }
          else
            listing.push_back(entry);
        }
      }
    } while (FindNextFile(hFind, &ffd) != 0);
    if (!FindClose(hFind))
      HT_ERRORF("FindClose failed: %s", IO_ERROR);
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error reading directory entries for DFS "
               "directory: %s", name.c_str());
  }
}

bool EmbeddedFilesystem::exists(const String &name, bool sync) {
  try {
    FdSyncGuard guard(this, Request::fdWrite, sync);

    String abspath;
    if (name[0] == '/')
      abspath = m_rootdir + name;
    else
      abspath = m_rootdir + "/" + name;

    return FileUtils::exists(abspath);
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error checking existence of DFS path: %s",
               name.c_str());
  }
}

void EmbeddedFilesystem::rename(const String &src, const String &dst, bool sync) {
  try {
    FdSyncGuard guard(this, Request::fdWrite, sync);

    String asrc = m_rootdir + "/" + src;
    String adst = m_rootdir + "/" + dst;
    if (!::MoveFileEx(asrc.c_str(), adst.c_str(), MOVEFILE_COPY_ALLOWED|MOVEFILE_REPLACE_EXISTING))
      throw_error();
  }
  catch (Exception &e) {
    HT_THROW2F(e.code(), e, "Error renaming of DFS path: %s -> %s",
               src.c_str(), dst.c_str());
  }
}

bool EmbeddedFilesystem::remove_dir(const String& absdir) {
  if (!absdir.empty()) {
    WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile( (absdir + "\\*").c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
      HT_ERRORF("FindFirstFile %s\\* failed: %s", absdir.c_str(), IO_ERROR);
      return false;
    }
    do {
      String _absdir = absdir + "\\";
      if (*ffd.cFileName) {
        if (ffd.cFileName[0] != '.' ||
            (ffd.cFileName[1] != 0 && (ffd.cFileName[1] != '.' || ffd.cFileName[2] != 0))) {
          String item = _absdir + ffd.cFileName;
          if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!remove_dir(item)) {
              HT_ERRORF("rmdir %s failed", item.c_str());
              if (!FindClose(hFind))
                HT_ERRORF("FindClose failed: %s", IO_ERROR);
              return false;
            }
          }
          else if (!DeleteFile(item.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND) {
            HT_ERRORF("DeleteFile %s failed: %s", item.c_str(), IO_ERROR);
            if (!FindClose(hFind))
              HT_ERRORF("FindClose failed: %s", IO_ERROR);
            return false;
          }
        }
      }
    } while (FindNextFile(hFind, &ffd) != 0);
    if (!FindClose(hFind))
      HT_ERRORF("FindClose failed: %s", IO_ERROR);
    if (!RemoveDirectory(absdir.c_str())) {
      HT_ERRORF("RemoveDirectory %s failed: %s", absdir.c_str(), IO_ERROR);
    }
  }
  return true;
}

void EmbeddedFilesystem::throw_error() {
  DWORD err = ::GetLastError();
  if (!err) {
    errno_t err;
    _get_errno(&err);
    char errbuf[128];
    errbuf[0] = 0;
    strcpy_s(errbuf, _strerror(0));

    if (err == ENOTDIR || err == ENAMETOOLONG || err == ENOENT)
      throw Exception(Error::FSBROKER_BAD_FILENAME, errbuf);
    else if (err == EACCES || err == EPERM)
      throw Exception(Error::FSBROKER_PERMISSION_DENIED, errbuf);
    else if (err == EBADF)
      throw Exception(Error::FSBROKER_BAD_FILE_HANDLE, errbuf);
    else if (err == EINVAL)
      throw Exception(Error::FSBROKER_INVALID_ARGUMENT, errbuf);
    else
      throw Exception(Error::FSBROKER_IO_ERROR, errbuf);

    _set_errno(err);
  }
  else {
    const char* errbuf = winapi_strerror(err);

    switch(err) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
      throw Exception(Error::FSBROKER_BAD_FILENAME, errbuf);
      break;
    case ERROR_ACCESS_DENIED:
    case ERROR_NETWORK_ACCESS_DENIED:
      throw Exception(Error::FSBROKER_PERMISSION_DENIED, errbuf);
      break;
    case ERROR_INVALID_HANDLE:
      throw Exception(Error::FSBROKER_BAD_FILE_HANDLE, errbuf);
      break;
    case ERROR_INVALID_ACCESS:
      throw Exception(Error::FSBROKER_INVALID_ARGUMENT, errbuf);
      break;
    default:
      throw Exception(Error::FSBROKER_IO_ERROR, errbuf);
      break;
    }
    ::SetLastError(err);
  }
}

EmbeddedFilesystem::RequestQueue::RequestQueue(RecMutex& mutex)
: m_mutex(mutex), m_shutdown(false)
{
}

void EmbeddedFilesystem::RequestQueue::enqueue(const Request& request) {
  ScopedRecLock lock(m_mutex);
  if (!m_shutdown) {
    FdQueueMap::iterator it = m_fd_queue_map.find(request.fd);
    if( it == m_fd_queue_map.end())
      it = m_fd_queue_map.insert(FdQueueMap::value_type(request.fd, Queue())).first;
    it->second.push_back(request);
    m_fd_cond.notify_one();
  }
}

bool EmbeddedFilesystem::RequestQueue::dequeue(Request &request) {
  while (true) {
    ScopedRecLock lock(m_mutex);
    while (!can_dequeue()) {
      if (m_shutdown)
        return false;
      m_fd_cond.wait(lock);
    }
    if (dequeue_request(request))
      return true;
  }
}

void EmbeddedFilesystem::RequestQueue::sync(int fd) {
  ScopedRecLock lock(m_mutex);
  while (!empty(fd) || m_fd_in_progress.find(fd) != m_fd_in_progress.end())
    m_fd_completed_cond.wait(lock);
  m_fd_in_progress.insert(fd);
}

void EmbeddedFilesystem::RequestQueue::completed(int fd) {
  ScopedRecLock lock(m_mutex);
  m_fd_in_progress.erase(fd);
  m_fd_completed_cond.notify_all();
  if (!m_fd_queue_map.empty()) {
    FdQueueMap::const_iterator it = m_fd_queue_map.find(fd);
    if (it != m_fd_queue_map.end() && it->second.empty())
      m_fd_queue_map.erase(fd);
    m_fd_cond.notify_one();
  }
}

void EmbeddedFilesystem::RequestQueue::shutdown(bool shutdown) {
  ScopedRecLock lock(m_mutex);
  m_shutdown = shutdown;
  m_fd_cond.notify_all();
}

bool EmbeddedFilesystem::RequestQueue::can_dequeue() const {
  for (FdQueueMap::const_iterator it = m_fd_queue_map.begin(); it != m_fd_queue_map.end(); ++it) {
    if (!it->second.empty() && m_fd_in_progress.find(it->first) == m_fd_in_progress.end())
      return true;
  }
  return false;
}

bool EmbeddedFilesystem::RequestQueue::dequeue_request(Request& request) {
  for (FdQueueMap::iterator it = m_fd_queue_map.begin(); it != m_fd_queue_map.end(); ++it) {
    if (!it->second.empty() && m_fd_in_progress.find(it->first) == m_fd_in_progress.end()) {
      m_fd_in_progress.insert(it->first);
      request = it->second.front();
      it->second.pop_front();
      return true;
    }
  }
  return false;
}

bool EmbeddedFilesystem::RequestQueue::empty(int fd) const {
  FdQueueMap::const_iterator it = m_fd_queue_map.find(fd);
  return it == m_fd_queue_map.end() || it->second.empty();
}

EmbeddedFilesystem::FdSyncGuard::FdSyncGuard(EmbeddedFilesystem* fs, int fd, bool sync)
: m_fs(fs), m_fd(fd), m_sync(sync) {
  if (m_sync) {
    m_fs->m_request_queue.sync(fd);
  }
}

EmbeddedFilesystem::FdSyncGuard::~FdSyncGuard() {
  if (m_sync) {
    m_fs->m_request_queue.completed(m_fd);
  }
}
