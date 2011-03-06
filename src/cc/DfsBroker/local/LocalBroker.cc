/**
 * Copyright (C) 2007 Doug Judd (Zvents, Inc.)
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

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#ifdef _WIN32
#include <boost/algorithm/string.hpp>
#endif

extern "C" {
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <sys/types.h>
#if defined(__sun__)
#include <sys/fcntl.h>
#endif
#include <sys/uio.h>
#include <unistd.h>
}

#include "AsyncComm/ReactorFactory.h"

#include "Common/FileUtils.h"
#include "Common/Filesystem.h"
#include "Common/Path.h"
#include "Common/String.h"
#include "Common/System.h"
#include "Common/SystemInfo.h"

#include "LocalBroker.h"

#ifdef _WIN32

#define IO_ERROR \
    winapi_strerror(::GetLastError())

#define CRT_IO_ERROR \
    _strerror(0)

inline int fsync( int fd ) {
    if( !::FlushFileBuffers((HANDLE) _get_osfhandle(fd)) ) {
        return ::GetLastError();
    }
    return 0;
}

#else

#define IO_ERROR \
    strerror(errno)

#define CRT_IO_ERROR \
    strerror(errno)

#endif

using namespace Hypertable;
using namespace std;

atomic_t LocalBroker::ms_next_fd = ATOMIC_INIT(0);

LocalBroker::LocalBroker(PropertiesPtr &cfg) {
  m_verbose = cfg->get_bool("verbose");
  m_directio = cfg->get_bool("DfsBroker.Local.DirectIO");

#if defined(__linux__)
  // disable direct i/o for kernels < 2.6
  if (m_directio) {
    if (System::os_info().version_major == 2 &&
        System::os_info().version_minor < 6)
      m_directio = false;
  }
#endif

  /**
   * Determine root directory
   */
  Path root = cfg->get_str("root", "");

  if (!root.is_complete()) {
    Path data_dir = cfg->get_str("Hypertable.DataDirectory");
    root = data_dir / root;
  }

  m_rootdir = root.directory_string();
#ifdef _WIN32
  boost::trim_right_if(m_rootdir, boost::is_any_of("/\\"));
#endif


  // ensure that root directory exists
  if (!FileUtils::mkdirs(m_rootdir))
    exit(1);
}



LocalBroker::~LocalBroker() {
}


void
LocalBroker::open(ResponseCallbackOpen *cb, const char *fname, 
                  uint32_t flags, uint32_t bufsz) {
  int fd, local_fd;
  String abspath;

  HT_DEBUGF("open file='%s' flags=%u bufsz=%d", fname, flags, bufsz);

  if (fname[0] == '/')
    abspath = m_rootdir + fname;
  else
    abspath = m_rootdir + "/" + fname;

  fd = atomic_inc_return(&ms_next_fd);

  int oflags = O_RDONLY;

  if (m_directio && flags & Filesystem::OPEN_FLAG_DIRECTIO) {
#ifdef O_DIRECT
    oflags |= O_DIRECT;
#endif
  }

  /**
   * Open the file
   */
  if ((local_fd = ::open(abspath.c_str(), oflags)) == -1) {
    report_error(cb);
    HT_ERRORF("open failed: file='%s' - %s", abspath.c_str(), CRT_IO_ERROR);
    return;
  }

#if defined(__APPLE__)
#ifdef F_NOCACHE
  //fcntl(local_fd, F_NOCACHE, 1);
#endif  
#elif defined(__sun__)
  if (m_directio)
    directio(local_fd, DIRECTIO_ON);
#endif

  HT_INFOF("open( %s ) = %d (local=%d)", fname, (int)fd, local_fd);

  {
    struct sockaddr_in addr;
    OpenFileDataLocalPtr fdata(new OpenFileDataLocal(fname, local_fd, O_RDONLY));

    cb->get_address(addr);

    m_open_file_map.create(fd, addr, fdata);

    cb->response(fd);
  }
}


void
LocalBroker::create(ResponseCallbackOpen *cb, const char *fname, uint32_t flags,
                    int32_t bufsz, int16_t replication, int64_t blksz) {
  int fd, local_fd;
  int oflags = O_WRONLY | O_CREAT;
  String abspath;

  HT_DEBUGF("create file='%s' flags=%u bufsz=%d replication=%d blksz=%lld",
            fname, flags, bufsz, (int)replication, (Lld)blksz);

  if (fname[0] == '/')
    abspath = m_rootdir + fname;
  else
    abspath = m_rootdir + "/" + fname;

  fd = atomic_inc_return(&ms_next_fd);

  if (flags & Filesystem::OPEN_FLAG_OVERWRITE)
    oflags |= O_TRUNC;
  else
    oflags |= O_APPEND;

  if (m_directio && flags & Filesystem::OPEN_FLAG_DIRECTIO) {
#ifdef O_DIRECT
    oflags |= O_DIRECT;
#endif
  }

  /**
   * Open the file
   */
  if ((local_fd = ::open(abspath.c_str(), oflags, 0644)) == -1) {
    report_error(cb);
    HT_ERRORF("open failed: file='%s' - %s", abspath.c_str(), CRT_IO_ERROR);
    return;
  }

#if defined(__APPLE__)
#ifdef F_NOCACHE
    fcntl(local_fd, F_NOCACHE, 1);
#endif  
#elif defined(__sun__)
    if (m_directio)
      directio(local_fd, DIRECTIO_ON);
#endif

  //HT_DEBUGF("created file='%s' fd=%d local_fd=%d", fname, fd, local_fd);

  HT_INFOF("create( %s ) = %d (local=%d)", fname, (int)fd, local_fd);

  {
    struct sockaddr_in addr;
    OpenFileDataLocalPtr fdata(new OpenFileDataLocal(fname, local_fd, O_WRONLY));

    cb->get_address(addr);

    m_open_file_map.create(fd, addr, fdata);

    cb->response(fd);
  }
}


void LocalBroker::close(ResponseCallback *cb, uint32_t fd) {
  int error;
  HT_DEBUGF("close fd=%d", fd);
  m_open_file_map.remove(fd);
  if ((error = cb->response_ok()) != Error::OK)
    HT_ERRORF("Problem sending response for close(%u) - %s", (unsigned)fd, Error::get_text(error));
}


void LocalBroker::read(ResponseCallbackRead *cb, uint32_t fd, uint32_t amount) {
  OpenFileDataLocalPtr fdata;
  ssize_t nread;
  uint64_t offset;
  uint8_t *readbuf;
  int error;

#if defined(__linux__)
  void *vptr = 0;
  HT_ASSERT(posix_memalign(&vptr, HT_DIRECT_IO_ALIGNMENT, amount) == 0);
  readbuf = (uint8_t *)vptr;
#else
  readbuf = new uint8_t [amount];
#endif

  StaticBuffer buf(readbuf, amount);

  HT_DEBUGF("read fd=%d amount=%d", fd, amount);

  if (!m_open_file_map.get(fd, fdata)) {
    char errbuf[32];
    sprintf(errbuf, "%d", fd);
    cb->error(Error::DFSBROKER_BAD_FILE_HANDLE, errbuf);
    HT_ERRORF("bad file handle: %d", fd);
    return;
  }

#ifndef _WIN32
  if ((offset = (uint64_t)lseek(fdata->fd, 0, SEEK_CUR)) == (uint64_t)-1) {
#else
  if ((offset = (uint64_t)_telli64(fdata->fd)) == (uint64_t)-1) {
#endif
    report_error(cb);
    HT_ERRORF("lseek failed: fd=%d offset=0 SEEK_CUR - %s", fdata->fd, CRT_IO_ERROR);
    return;
  }

  if ((nread = FileUtils::read(fdata->fd, buf.base, amount)) == -1) {
    report_error(cb);
    HT_ERRORF("read failed: fd=%d offset=%llu amount=%d - %s",
          fdata->fd, (Llu)offset, amount, IO_ERROR);
    return;
  }

  buf.size = nread;

  if ((error = cb->response(offset, buf)) != Error::OK)
    HT_ERRORF("Problem sending response for read(%u, %u) - %s",
              (unsigned)fd, (unsigned)amount, Error::get_text(error));

}


void LocalBroker::append(ResponseCallbackAppend *cb, uint32_t fd,
                         uint32_t amount, const void *data, bool sync) {
  OpenFileDataLocalPtr fdata;
  ssize_t nwritten;
  uint64_t offset;
  int error;

  HT_DEBUG_OUT <<"append fd="<< fd <<" amount="<< amount <<" data='"
      << format_bytes(20, data, amount) <<" sync="<< sync << HT_END;

  if (!m_open_file_map.get(fd, fdata)) {
    char errbuf[32];
    sprintf(errbuf, "%d", fd);
    cb->error(Error::DFSBROKER_BAD_FILE_HANDLE, errbuf);
    return;
  }

#ifndef _WIN32
  if ((offset = (uint64_t)lseek(fdata->fd, 0, SEEK_CUR)) == (uint64_t)-1) {
#else
  if ((offset = (uint64_t)_telli64(fdata->fd)) == (uint64_t)-1) {
#endif
    report_error(cb);
    HT_ERRORF("lseek failed: fd=%d offset=0 SEEK_CUR - %s", fdata->fd, CRT_IO_ERROR);
    return;
  }

  if ((nwritten = FileUtils::write(fdata->fd, data, amount)) == -1) {
    report_error(cb);
    HT_ERRORF("write failed: fd=%d offset=%llu amount=%d data=%p- %s",
    fdata->fd, (Llu)offset, amount, data, IO_ERROR);
    return;
  }

  if (sync && fsync(fdata->fd) != 0) {
    report_error(cb);
    HT_ERRORF("flush failed: fd=%d - %s", fdata->fd, IO_ERROR);
    return;
  }

  if ((error = cb->response(offset, nwritten)) != Error::OK)
    HT_ERRORF("Problem sending response for append(%u, localfd=%u, %u) - %s",
              (unsigned)fd, (unsigned)fdata->fd, (unsigned)amount, Error::get_text(error));

}


void LocalBroker::seek(ResponseCallback *cb, uint32_t fd, uint64_t offset) {
  OpenFileDataLocalPtr fdata;
  int error;

  HT_DEBUGF("seek fd=%lu offset=%llu", (Lu)fd, (Llu)offset);

  if (!m_open_file_map.get(fd, fdata)) {
    char errbuf[32];
    sprintf(errbuf, "%d", fd);
    cb->error(Error::DFSBROKER_BAD_FILE_HANDLE, errbuf);
    return;
  }

#ifndef _WIN32
  if ((offset = (uint64_t)lseek(fdata->fd, offset, SEEK_SET)) == (uint64_t)-1) {
#else
  if ((offset = (uint64_t)_lseeki64(fdata->fd, offset, SEEK_SET)) == (uint64_t)-1) {
#endif
    report_error(cb);
    HT_ERRORF("lseek failed: fd=%d offset=%llu - %s", fdata->fd, (Llu)offset, CRT_IO_ERROR);
    return;
  }

  if ((error = cb->response_ok()) != Error::OK)
    HT_ERRORF("Problem sending response for seek(%u, %llu) - %s",
              (unsigned)fd, (Llu)offset, Error::get_text(error));

}


void LocalBroker::remove(ResponseCallback *cb, const char *fname) {
  String abspath;
  int error;

  HT_DEBUGF("remove file='%s'", fname);

  if (fname[0] == '/')
    abspath = m_rootdir + fname;
  else
    abspath = m_rootdir + "/" + fname;

#ifndef _WIN32
  if (unlink(abspath.c_str()) == -1) {
#else
  if (!DeleteFile(abspath.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND) {
#endif
    report_error(cb);
    HT_ERRORF("unlink failed: file='%s' - %s", abspath.c_str(), IO_ERROR);
    return;
  }

  if ((error = cb->response_ok()) != Error::OK)
    HT_ERRORF("Problem sending response for remove(%s) - %s",
              fname, Error::get_text(error));
}


void LocalBroker::length(ResponseCallbackLength *cb, const char *fname) {
  String abspath;
  uint64_t length;
  int error;

  HT_DEBUGF("length file='%s'", fname);

  if (fname[0] == '/')
    abspath = m_rootdir + fname;
  else
    abspath = m_rootdir + "/" + fname;

  if ((length = FileUtils::length(abspath)) == (uint64_t)-1) {
    report_error(cb);
    HT_ERRORF("length (stat) failed: file='%s' - %s", abspath.c_str(), IO_ERROR);
    return;
  }
  
  if ((error = cb->response(length)) != Error::OK)
    HT_ERRORF("Problem sending response (%llu) for length(%s) - %s",
              (Llu)length, fname, Error::get_text(error));
}


void
LocalBroker::pread(ResponseCallbackRead *cb, uint32_t fd, uint64_t offset,
                   uint32_t amount) {
  OpenFileDataLocalPtr fdata;
  ssize_t nread;
  uint8_t *readbuf;
  int error;

  HT_DEBUGF("pread fd=%d offset=%llu amount=%d", fd, (Llu)offset, amount);

#if defined(__linux__)
  void *vptr = 0;
  HT_ASSERT(posix_memalign(&vptr, HT_DIRECT_IO_ALIGNMENT, amount) == 0);
  readbuf = (uint8_t *)vptr;
#else
  readbuf = new uint8_t [amount];
#endif

  StaticBuffer buf(readbuf, amount);

  if (!m_open_file_map.get(fd, fdata)) {
    char errbuf[32];
    sprintf(errbuf, "%d", fd);
    cb->error(Error::DFSBROKER_BAD_FILE_HANDLE, errbuf);
    return;
  }

  if ((nread = FileUtils::pread(fdata->fd, buf.base, amount, (off_t)offset)) != amount) {
    report_error(cb);
    HT_ERRORF("pread failed: fd=%d amount=%d offset=%llu - %s", fdata->fd,
              amount, (Llu)offset, IO_ERROR);
    return;
  }

  buf.size = nread;

  if ((error = cb->response(offset, buf)) != Error::OK)
    HT_ERRORF("Problem sending response for pread(%u, %llu, %u) - %s",
              (unsigned)fd, (Llu)offset, (unsigned)amount, Error::get_text(error));

}


void LocalBroker::mkdirs(ResponseCallback *cb, const char *dname) {
  String absdir;
  int error;

  HT_DEBUGF("mkdirs dir='%s'", dname);

  if (dname[0] == '/')
    absdir = m_rootdir + dname;
  else
    absdir = m_rootdir + "/" + dname;

  if (!FileUtils::mkdirs(absdir)) {
    report_error(cb);
    HT_ERRORF("mkdirs failed: dname='%s' - %s", absdir.c_str(), IO_ERROR);
    return;
  }

  if ((error = cb->response_ok()) != Error::OK)
    HT_ERRORF("Problem sending response for mkdirs(%s) - %s",
              dname, Error::get_text(error));
}


void LocalBroker::rmdir(ResponseCallback *cb, const char *dname) {
  String absdir;
  String cmd_str;
  int error;

  if (m_verbose) {
    HT_DEBUGF("rmdir dir='%s'", dname);
  }

  if (dname[0] == '/')
    absdir = m_rootdir + dname;
  else
    absdir = m_rootdir + "/" + dname;

  if (FileUtils::exists(absdir)) {
#ifndef _WIN32
    cmd_str = (String)"/bin/rm -rf " + absdir;
    if (system(cmd_str.c_str()) != 0) {
      HT_ERRORF("%s failed.", cmd_str.c_str());
      cb->error(Error::DFSBROKER_IO_ERROR, cmd_str);
#else
    if (!rmdir(absdir)) {
      HT_ERRORF("rmdir %s failed.", absdir.c_str());
      cb->error(Error::DFSBROKER_IO_ERROR, format("rmdir %s failed.", absdir.c_str()));
#endif
      return;
    }
  }

#if 0
  if (rmdir(absdir.c_str()) != 0) {
    report_error(cb);
    HT_ERRORF("rmdir failed: dname='%s' - %s", absdir.c_str(), IO_ERROR);
    return;
  }
#endif

  if ((error = cb->response_ok()) != Error::OK)
    HT_ERRORF("Problem sending response for rmdir(%s) - %s",
              dname, Error::get_text(error));

}

void LocalBroker::readdir(ResponseCallbackReaddir *cb, const char *dname) {
  std::vector<String> listing;
  String absdir;

  HT_DEBUGF("Readdir dir='%s'", dname);

  if (dname[0] == '/')
    absdir = m_rootdir + dname;
  else
    absdir = m_rootdir + "/" + dname;

#ifdef _WIN32

  WIN32_FIND_DATA ffd;
  HANDLE hFind = FindFirstFile( (absdir + "\\*").c_str(), &ffd);
  if (hFind == INVALID_HANDLE_VALUE) {
    report_error(cb);
    return;
  }
  do {
    if (*ffd.cFileName) {
      if (ffd.cFileName[0] != '.' ||
          (ffd.cFileName[1] != 0 && (ffd.cFileName[1] != '.' || ffd.cFileName[2] != 0)))
        listing.push_back((String)ffd.cFileName);
    }
  } while (FindNextFile(hFind, &ffd) != 0);
  if (!FindClose(hFind))
    HT_ERRORF("FindClose failed: %s", IO_ERROR);

#else

  DIR *dirp = opendir(absdir.c_str());
  if (dirp == 0) {
    report_error(cb);
    HT_ERRORF("opendir('%s') failed - %s", absdir.c_str(), IO_ERROR);
    return;
  }

  struct dirent *dp = (struct dirent *)new uint8_t [sizeof(struct dirent)+1025];
  struct dirent *result;

  if (readdir_r(dirp, dp, &result) != 0) {
    report_error(cb);
    HT_ERRORF("readdir('%s') failed - %s", absdir.c_str(), IO_ERROR);
    (void)closedir(dirp);
    delete [] (uint8_t *)dp;
    return;
  }

  while (result != 0) {
    if (result->d_name[0] != '.' && result->d_name[0] != 0) {
      listing.push_back((String)result->d_name);
      //HT_INFOF("readdir Adding listing '%s'", result->d_name);
    }

    if (readdir_r(dirp, dp, &result) != 0) {
      report_error(cb);
      HT_ERRORF("readdir('%s') failed - %s", absdir.c_str(), IO_ERROR);
      delete [] (uint8_t *)dp;
      return;
    }
  }
  (void)closedir(dirp);

  delete [] (uint8_t *)dp;

#endif

  HT_DEBUGF("Sending back %d listings", (int)listing.size());

  cb->response(listing);
}


void LocalBroker::flush(ResponseCallback *cb, uint32_t fd) {
  OpenFileDataLocalPtr fdata;

  HT_DEBUGF("flush fd=%d", fd);

  if (!m_open_file_map.get(fd, fdata)) {
    char errbuf[32];
    sprintf(errbuf, "%d", fd);
    cb->error(Error::DFSBROKER_BAD_FILE_HANDLE, errbuf);
    return;
  }

  if (fsync(fdata->fd) != 0) {
    report_error(cb);
    HT_ERRORF("flush failed: fd=%d - %s", fdata->fd, IO_ERROR);
    return;
  }

  cb->response_ok();
}


void LocalBroker::status(ResponseCallback *cb) {
  cb->response_ok();
}


void LocalBroker::shutdown(ResponseCallback *cb) {
  m_open_file_map.remove_all();
  cb->response_ok();
  poll(0, 0, 2000);
}


void LocalBroker::exists(ResponseCallbackExists *cb, const char *fname) {
  String abspath;

  HT_DEBUGF("exists file='%s'", fname);

  if (fname[0] == '/')
    abspath = m_rootdir + fname;
  else
    abspath = m_rootdir + "/" + fname;

  cb->response(FileUtils::exists(abspath));
}


void
LocalBroker::rename(ResponseCallback *cb, const char *src, const char *dst) {
  String asrc =
    format("%s%s%s", m_rootdir.c_str(), *src == '/' ? "" : "/", src);
  String adst =
    format("%s%s%s", m_rootdir.c_str(), *dst == '/' ? "" : "/", dst);

  HT_DEBUGF("rename %s -> %s", asrc.c_str(), adst.c_str());

#ifndef _WIN32
  if (std::rename(asrc.c_str(), adst.c_str()) != 0) {
#else
  if (!::MoveFileEx(asrc.c_str(), adst.c_str(), MOVEFILE_COPY_ALLOWED|MOVEFILE_REPLACE_EXISTING)) {
#endif
    report_error(cb);
    HT_ERRORF("rename failed: %s -> %s - %s", asrc.c_str(), adst.c_str(), IO_ERROR);
    return;
  }
  cb->response_ok();
}

void
LocalBroker::debug(ResponseCallback *cb, int32_t command,
                   StaticBuffer &serialized_parameters) {
  HT_ERRORF("debug command %d not implemented.", command);
  cb->error(Error::NOT_IMPLEMENTED, format("Unsupported debug command - %d",
            command));
}

#ifdef _WIN32

void LocalBroker::report_error(ResponseCallback *cb) {
  DWORD err = ::GetLastError();
  if( !err ) {
    errno_t err;
    _get_errno(&err);
    char errbuf[128];
    errbuf[0] = 0;
    strcpy_s(errbuf, _strerror(0));

    if (err == ENOTDIR || err == ENAMETOOLONG || err == ENOENT)
      cb->error(Error::DFSBROKER_BAD_FILENAME, errbuf);
    else if (err == EACCES || err == EPERM)
      cb->error(Error::DFSBROKER_PERMISSION_DENIED, errbuf);
    else if (err == EBADF)
      cb->error(Error::DFSBROKER_BAD_FILE_HANDLE, errbuf);
    else if (err == EINVAL)
      cb->error(Error::DFSBROKER_INVALID_ARGUMENT, errbuf);
    else
      cb->error(Error::DFSBROKER_IO_ERROR, errbuf);

    _set_errno(err);
  }
  else {
    const char* errbuf = winapi_strerror(err);

    switch(err) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
      cb->error(Error::DFSBROKER_BAD_FILENAME, errbuf);
      break;
    case ERROR_ACCESS_DENIED:
    case ERROR_NETWORK_ACCESS_DENIED:
      cb->error(Error::DFSBROKER_PERMISSION_DENIED, errbuf);
      break;
    case ERROR_INVALID_HANDLE:
      cb->error(Error::DFSBROKER_BAD_FILE_HANDLE, errbuf);
      break;
    case ERROR_INVALID_ACCESS:
      cb->error(Error::DFSBROKER_INVALID_ARGUMENT, errbuf);
      break;
    default:
      cb->error(Error::DFSBROKER_IO_ERROR, errbuf);
      break;
    }
    ::SetLastError(err);
  }
}

bool LocalBroker::rmdir(const String& absdir) {
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
            if (!rmdir(item)) {
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

#else

void LocalBroker::report_error(ResponseCallback *cb) {
  char errbuf[128];
  errbuf[0] = 0;

  strerror_r(errno, errbuf, 128);

  if (errno == ENOTDIR || errno == ENAMETOOLONG || errno == ENOENT)
    cb->error(Error::DFSBROKER_BAD_FILENAME, errbuf);
  else if (errno == EACCES || errno == EPERM)
    cb->error(Error::DFSBROKER_PERMISSION_DENIED, errbuf);
  else if (errno == EBADF)
    cb->error(Error::DFSBROKER_BAD_FILE_HANDLE, errbuf);
  else if (errno == EINVAL)
    cb->error(Error::DFSBROKER_INVALID_ARGUMENT, errbuf);
  else
    cb->error(Error::DFSBROKER_IO_ERROR, errbuf);
}

#endif
