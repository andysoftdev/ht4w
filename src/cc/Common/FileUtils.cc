/**
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "Common/Compat.h"
#include <iomanip>
#include <iostream>
#include <sstream>

extern "C" {
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#ifdef HT_XATTR_ENABLED
# if defined(__FreeBSD__)
#   include <sys/extattr.h>
# else
#   include <sys/xattr.h>
# endif
# if defined(__linux__)
#   include <attr/xattr.h>
# endif
#endif
}

#include <boost/shared_array.hpp>
#include <boost/shared_ptr.hpp>

#include <re2/re2.h>

#include "FileUtils.h"
#include "Logger.h"
#ifdef _WIN32
#include "Path.h"
#include <Shlobj.h>
#include <boost/algorithm/string/replace.hpp>
#endif

using namespace Hypertable;
using namespace std;

ssize_t FileUtils::read(const String &fname, String &contents) {
  size_t len = 0;
  String str;
  char *buf = file_to_buffer(fname, &len);
  if (buf != 0) {
    contents.append(buf, len);
    delete [] buf;
  }
  return (ssize_t)len;
}

String FileUtils::file_to_string(const String &fname) {
  String str;
  read(fname, str);
  return str;
}

#ifdef _WIN32

#define HT_WIN32_LASTERROR( msg ) \
  { \
    DWORD err = GetLastError(); \
    HT_ERRORF( msg" failed - %s", winapi_strerror(err)); \
    SetLastError(err); \
  }

ssize_t FileUtils::read(HANDLE fd, void *vptr, size_t n) {
  DWORD nread;
  if( !::ReadFile(fd, vptr, n, &nread, 0) ) {
    HT_WIN32_LASTERROR("ReadFile");
    return -1;
  }
  return nread;
}

ssize_t FileUtils::read(int fd, void *vptr, size_t n) {
  return read((HANDLE) _get_osfhandle(fd), vptr, n);
}

ssize_t FileUtils::pread(HANDLE fd, void *vptr, size_t n, uint64_t offset) {
  OVERLAPPED ov = { 0 };
  ov.Offset = (DWORD)offset;
  ov.OffsetHigh = offset >> 32;
  DWORD nread;
  if(!::ReadFile(fd, vptr, n, &nread, &ov)) {
    if( GetLastError() != ERROR_IO_PENDING ) {
      HT_WIN32_LASTERROR("ReadFile");
      return -1;
    }
    if(!::GetOverlappedResult(fd, &ov, &nread, TRUE)) {
      HT_WIN32_LASTERROR("GetOverlappedResult");
      return -1;
    }
  }
  return nread;
}

ssize_t FileUtils::pread(int fd, void *vptr, size_t n, uint64_t offset) {
  return pread((HANDLE) _get_osfhandle(fd), vptr, n, offset);
}

ssize_t FileUtils::write(const String &fname, String &contents) {
  HANDLE fd = ::CreateFile(fname.c_str(), GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, NULL);
  if (fd == INVALID_HANDLE_VALUE) {
    DWORD err = GetLastError();
    HT_ERRORF("CreateFile %s failed - %s", fname.c_str(), winapi_strerror(err));
    SetLastError(err);
  }
  ssize_t rval = write(fd, contents.c_str(), contents.length());
  ::CloseHandle(fd);
  return rval;
}

ssize_t FileUtils::write(HANDLE fd, const void *vptr, size_t n) {
  DWORD nwritten;
  if (!::WriteFile(fd, vptr, n, &nwritten, 0)) {
    HT_WIN32_LASTERROR("WriteFile");
    return -1; /* error */
  }
  return nwritten;
}

ssize_t FileUtils::write(int fd, const void *vptr, size_t n) {
  return write((HANDLE) _get_osfhandle(fd), vptr, n);
}

char *FileUtils::file_to_buffer(const String &fname, size_t *lenp) {
  *lenp = 0;

  HANDLE fd = ::CreateFile(fname.c_str(), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, NULL);
  if (fd == INVALID_HANDLE_VALUE) {
    DWORD err = ::GetLastError();
    HT_ERRORF("CreateFile %s failed - %s", fname.c_str(), winapi_strerror(err));
    ::SetLastError(err);
    return 0;
  }

  LARGE_INTEGER fs;
  if (!::GetFileSizeEx(fd, &fs)) {
    DWORD err = ::GetLastError();
    HT_ERRORF("GetFileSizeEx %s failed - %s", fname.c_str(), winapi_strerror(err));
    ::CloseHandle(fd);
    ::SetLastError(err);
    return 0;
  }

  *lenp = (size_t)fs.QuadPart;
  char *rbuf = new char [*lenp + 1];
  ssize_t nread = FileUtils::read(fd, rbuf, *lenp);
  ::CloseHandle(fd);

  if (nread == (ssize_t)-1) {
    delete [] rbuf;
    *lenp = 0;
    return 0;
  }

  if (nread < (ssize_t)*lenp) {
    HT_WARNF("short read (%d of %d bytes)", (int)nread, (int)*lenp);
    *lenp = nread;
  }

  rbuf[nread] = 0;
  return rbuf;
}

bool FileUtils::mkdirs(const String &dirname) {
  Path dir(dirname);
  if (!dir.is_complete()) {
    dir = boost::filesystem::current_path();
    dir /= dirname;
  }
  String native_dirname(dir.native_directory_string());
  int err;
  if ((err = SHCreateDirectoryExA(0, native_dirname.c_str(), 0)) != ERROR_SUCCESS) {
    if (err != ERROR_FILE_EXISTS && err != ERROR_ALREADY_EXISTS) {
      HT_ERRORF("SHCreateDirectoryExA %s failed - %s", native_dirname.c_str(), winapi_strerror(err));
      SetLastError(err);
      return false;
    }
  }
  return true;
}

bool FileUtils::exists(const String &fname) {
  if( ::GetFileAttributes(fname.c_str()) != INVALID_FILE_ATTRIBUTES )
    return true;
  DWORD err = ::GetLastError();
  if (err != ERROR_FILE_NOT_FOUND &&
    err != ERROR_PATH_NOT_FOUND) {
      HT_ERRORF("GetFileAttributes %s failed - %s", fname.c_str(), winapi_strerror(err));
      ::SetLastError(err);
  }
  return false;
}

bool FileUtils::unlink(const String &fname) {
  if (!::DeleteFile(fname.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND) {
    DWORD err = ::GetLastError();
    HT_ERRORF("DeleteFile %s failed - %s", fname.c_str(), winapi_strerror(err));
    ::SetLastError(err);
    return false;
  }
  return true;
}

uint64_t FileUtils::size(const String &fname) {
  int64_t length = FileUtils::length(fname);
  return length==-1 ? 0 : length;
}

bool FileUtils::rename(const String &oldpath, const String &newpath) {
  if (!::MoveFileEx(oldpath.c_str(), newpath.c_str(), MOVEFILE_COPY_ALLOWED|MOVEFILE_REPLACE_EXISTING)) {
    DWORD err = ::GetLastError();
    HT_ERRORF("MoveFileEx %s, %s failed - %s", oldpath.c_str(), newpath.c_str(), winapi_strerror(err));
    ::SetLastError(err);
    return false;
  }
  return true;
}

int64_t FileUtils::length(const String &fname) {
  WIN32_FIND_DATA wfd;
  HANDLE fh = ::FindFirstFile(fname.c_str(), &wfd);
  if (fh == INVALID_HANDLE_VALUE) {
    DWORD err = ::GetLastError();
    HT_ERRORF("FindFirstFile %s failed - %s", fname.c_str(), winapi_strerror(err));
    ::SetLastError(err);
    return (int64_t)-1;
  }
  ::FindClose(fh);
  return ((int64_t)wfd.nFileSizeHigh << 32) | wfd.nFileSizeLow;
}

void FileUtils::add_trailing_slash(String &path) {
  if (path.find('/', path.length()-1) == string::npos)
    path += "/";
}

bool  FileUtils::expand_tilde(String&) {
  return false;
}

#else

/**
*/
ssize_t FileUtils::read(int fd, void *vptr, size_t n) {
  size_t nleft;
  ssize_t nread;
  char *ptr;

  ptr = (char *)vptr;
  nleft = n;
  while (nleft > 0) {
    if ((nread = ::read(fd, ptr, nleft)) < 0) {
      if (errno == EINTR)
        nread = 0;/* and call read() again */
      else if (errno == EAGAIN)
        break;
      else {
        return -1;
      }
    } else if (nread == 0)
      break;/* EOF */

    nleft -= nread;
    ptr   += nread;
  }
  return n - nleft;
}

/**
*/
ssize_t FileUtils::pread(int fd, void *vptr, size_t n, off_t offset) {
  size_t nleft;
  ssize_t nread;
  char *ptr;

  ptr = (char *)vptr;
  nleft = n;
  while (nleft > 0) {
    if ((nread = ::pread(fd, ptr, nleft, offset)) < 0) {
      if (errno == EINTR)
        nread = 0;/* and call read() again */
      else if (errno == EAGAIN)
        break;
      else {
        return -1;
      }
    } else if (nread == 0)
      break;/* EOF */

    nleft -= nread;
    ptr   += nread;
    offset += nread;
  }
  return n - nleft;
}


ssize_t FileUtils::write(const String &fname, String &contents) {
  int fd = open(fname.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);
  if (fd < 0) {
    int saved_errno = errno;
    HT_ERRORF("Unable to open file \"%s\" for writing - %s", fname.c_str(),
      strerror(saved_errno));
    errno = saved_errno;
    return -1;
  }
  ssize_t rval = write(fd, contents.c_str(), contents.length());
  ::close(fd);
  return rval;
}



/**
*/
ssize_t FileUtils::write(int fd, const void *vptr, size_t n) {
  size_t nleft;
  ssize_t nwritten;
  const char *ptr;

  ptr = (const char *)vptr;
  nleft = n;
  while (nleft > 0) {
    if ((nwritten = ::write(fd, ptr, nleft)) <= 0) {
      if (errno == EINTR)
        nwritten = 0; /* and call write() again */
      else if (errno == EAGAIN)
        break;
      else {
        return -1; /* error */
      }
    }

    nleft -= nwritten;
    ptr   += nwritten;
  }
  return n - nleft;
}

ssize_t FileUtils::writev(int fd, const struct iovec *vector, int count) {
  ssize_t nwritten;
  while ((nwritten = ::writev(fd, vector, count)) <= 0) {
    if (errno == EINTR)
      nwritten = 0; /* and call write() again */
    else if (errno == EAGAIN) {
      nwritten = 0;
      break;
    }
    else {
      return -1; /* error */
    }
  }
  return nwritten;
}


ssize_t
  FileUtils::sendto(int fd, const void *vptr, size_t n, const sockaddr *to,
  socklen_t tolen) {
    size_t nleft;
    ssize_t nsent;
    const char *ptr;

    ptr = (const char *)vptr;
    nleft = n;
    while (nleft > 0) {
      if ((nsent = ::sendto(fd, ptr, nleft, 0, to, tolen)) <= 0) {
        if (errno == EINTR)
          nsent = 0; /* and call sendto() again */
        else if (errno == EAGAIN || errno == ENOBUFS)
          break;
        else {
          return -1; /* error */
        }
      }

      nleft -= nsent;
      ptr   += nsent;
    }
    return n - nleft;
}



ssize_t FileUtils::send(int fd, const void *vptr, size_t n) {
  size_t nleft;
  ssize_t nsent;
  const char *ptr;

  ptr = (const char *)vptr;
  nleft = n;
  while (nleft > 0) {
    if ((nsent = ::send(fd, ptr, nleft, 0)) <= 0) {
      if (errno == EINTR)
        nsent = 0; /* and call sendto() again */
      else if (errno == EAGAIN || errno == ENOBUFS)
        break;
      else {
        return -1; /* error */
      }
    }

    nleft -= nsent;
    ptr   += nsent;
  }
  return n - nleft;
}



ssize_t
  FileUtils::recvfrom(int fd, void *vptr, size_t n, sockaddr *from,
  socklen_t *fromlen) {
    ssize_t nread;
    while (true) {
      if ((nread = ::recvfrom(fd, vptr, n, 0, from, fromlen)) < 0) {
        if (errno != EINTR)
          break;
      }
      else
        break;
    }
    return nread;
}


ssize_t FileUtils::recv(int fd, void *vptr, size_t n) {
  ssize_t nread;
  while (true) {
    if ((nread = ::recv(fd, vptr, n, 0)) < 0) {
      if (errno != EINTR)
        break;
    }
    else
      break;
  }
  return nread;
}


/* flags are file status flags to turn on */
void FileUtils::set_flags(int fd, int flags) {
  int val;

  if ((val = fcntl(fd, F_GETFL, 0)) < 0) {
    int saved_errno = errno;
    cerr << "fcnt(F_GETFL) failed : " << strerror(saved_errno) << endl;
    errno = saved_errno;
  }

  val |= flags;

  if (fcntl(fd, F_SETFL, val) < 0) {
    int saved_errno = errno;
    cerr << "fcnt(F_SETFL) failed : " << strerror(saved_errno) << endl;
    errno = saved_errno;
  }
}



/**
*/
char *FileUtils::file_to_buffer(const String &fname, off_t *lenp) {
  struct stat statbuf;
  int fd;

  *lenp = 0;

  if ((fd = open(fname.c_str(), O_RDONLY)) < 0) {
    int saved_errno = errno;
    HT_ERRORF("open(\"%s\") failed - %s", fname.c_str(),  strerror(saved_errno));
    errno = saved_errno;
    return 0;
  }

  if (fstat(fd, &statbuf) < 0) {
    int saved_errno = errno;
    HT_ERRORF("fstat(\"%s\") failed - %s", fname.c_str(),  strerror(saved_errno));
    errno = saved_errno;
    return 0;
  }

  *lenp = statbuf.st_size;

  char *rbuf = new char [*lenp + 1];

  ssize_t nread = FileUtils::read(fd, rbuf, *lenp);

  ::close(fd);

  if (nread == (ssize_t)-1) {
    int saved_errno = errno;
    HT_ERRORF("read(\"%s\") failed - %s", fname.c_str(),  strerror(saved_errno));
    errno = saved_errno;
    delete [] rbuf;
    *lenp = 0;
    return 0;
  }

  if (nread < *lenp) {
    HT_WARNF("short read (%d of %d bytes)", (int)nread, (int)*lenp);
    *lenp = nread;
  }

  rbuf[nread] = 0;

  return rbuf;
}

String FileUtils::file_to_string(const String &fname) {
  String str;
  off_t len;
  char *contents = file_to_buffer(fname, &len);
  str = (contents == 0) ? "" : contents;
  delete [] contents;
  return str;
}



void *FileUtils::mmap(const String &fname, off_t *lenp) {
  int fd;
  struct stat statbuf;
  void *map;

  if (::stat(fname.c_str(), &statbuf) != 0)
    HT_FATALF("Unable determine length of '%s' for memory mapping - %s", fname.c_str(), strerror(errno));
  *lenp = (off_t)statbuf.st_size;

  if ((fd = ::open(fname.c_str(), O_RDONLY)) == -1)
    HT_FATALF("Unable to open '%s' for memory mapping - %s", fname.c_str(), strerror(errno));
  
  if ((map = ::mmap(0, *lenp, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED)
    HT_FATALF("Unable to memory map file '%s' - %s", fname.c_str(), strerror(errno));

  close(fd);

  return map;
}



bool FileUtils::mkdirs(const String &dirname) {
  struct stat statbuf;
  boost::shared_array<char> tmp_dir(new char [dirname.length() + 1]);
  char *tmpdir = tmp_dir.get();
  char *ptr = tmpdir+1;

  strcpy(tmpdir, dirname.c_str());

  while ((ptr = strchr(ptr, '/')) != 0) {
    *ptr = 0;
    if (stat(tmpdir, &statbuf) != 0) {
      if (errno == ENOENT) {
        if (mkdir(tmpdir, 0755) != 0) {
          int saved_errno = errno;
          HT_ERRORF("Problem creating directory '%s' - %s",
            tmpdir, strerror(saved_errno));
          errno = saved_errno;
          return false;
        }
      }
      else {
        int saved_errno = errno;
        HT_ERRORF("Problem stat'ing directory '%s' - %s",
          tmpdir, strerror(saved_errno));
        errno = saved_errno;
        return false;
      }
    }
    *ptr++ = '/';
  }

  if (stat(tmpdir, &statbuf) != 0) {
    if (errno == ENOENT) {
      if (mkdir(tmpdir, 0755) != 0) {
        int saved_errno = errno;
        HT_ERRORF("Problem creating directory '%s' - %s",
          tmpdir, strerror(saved_errno));
        errno = saved_errno;
        return false;
      }
    }
    else {
      int saved_errno = errno;
      HT_ERRORF("Problem stat'ing directory '%s' - %s",
        tmpdir, strerror(saved_errno));
      errno = saved_errno;
      return false;
    }
  }

  return true;
}


bool FileUtils::exists(const String &fname) {
  struct stat statbuf;
  if (stat(fname.c_str(), &statbuf) != 0)
    return false;
  return true;
}

bool FileUtils::unlink(const String &fname) {
  if (::unlink(fname.c_str()) == -1) {
    int saved_errno = errno;
    HT_ERRORF("unlink(\"%s\") failed - %s", fname.c_str(), strerror(saved_errno));
    errno = saved_errno;
    return false;
  }
  return true;
}

bool FileUtils::rename(const String &oldpath, const String &newpath) {
  if (::rename(oldpath.c_str(), newpath.c_str()) == -1) {
    int saved_errno = errno;
    HT_ERRORF("rename(\"%s\", \"%s\") failed - %s",
      oldpath.c_str(), newpath.c_str(), strerror(saved_errno));
    errno = saved_errno;
    return false;
  }
  return true;
}

uint64_t FileUtils::size(const String &fname) {
  struct stat statbuf;
  if (stat(fname.c_str(), &statbuf) != 0)
    return 0;
  return statbuf.st_size;

}


off_t FileUtils::length(const String &fname) {
  struct stat statbuf;
  if (stat(fname.c_str(), &statbuf) != 0)
    return (off_t)-1;
  return statbuf.st_size;
}


void FileUtils::add_trailing_slash(String &path) {
  if (path.find('/', path.length()-1) == string::npos)
    path += "/";
}


bool FileUtils::expand_tilde(String &fname) {
  struct passwd pbuf;
  struct passwd *prbuf;
  char buf[256];

  if (fname[0] != '~')
    return false;

  if (fname[1] == '/') {
    if (getpwuid_r(getuid() , &pbuf, buf, 256, &prbuf) != 0 || prbuf == 0)
      return false;
    fname = (String)pbuf.pw_dir + fname.substr(1);
  }
  else {
    String name;
    size_t first_slash = fname.find_first_of('/');

    if (first_slash == string::npos)
      name = fname.substr(1);
    else
      name = fname.substr(1, first_slash-1);

    if (getpwnam_r(name.c_str() , &pbuf, buf, 256, &prbuf) != 0 || prbuf == 0)
      return false;

    if (first_slash == string::npos)
      fname = pbuf.pw_dir;
    else
      fname = (String)pbuf.pw_dir + fname.substr(first_slash);
  }

  return true;
}

using namespace re2;

void FileUtils::readdir(const String &dirname, const String &fname_regex,
			std::vector<struct dirent> &listing) {
  int ret;
  DIR *dirp = opendir(dirname.c_str());
  struct dirent de, *dep;
  boost::shared_ptr<RE2> regex(fname_regex.length() ? new RE2(fname_regex) : 0);

  do {

    if ((ret = readdir_r(dirp, &de, &dep)) != 0)
      HT_FATALF("Problem reading directory '%s' - %s", dirname.c_str(), strerror(errno));

    if (dep != 0 &&
	(!regex || RE2::FullMatch(de.d_name, *regex)))
      listing.push_back(de);

  } while (dep != 0);

  (void)closedir(dirp);
}

#endif

#ifdef HT_XATTR_ENABLED

int
  FileUtils::getxattr(const String &path, const String &name, void *value,
  size_t size) {
    String canonic = (String)"user." + name;
#if defined(__linux__)
    return ::getxattr(path.c_str(), canonic.c_str(), value, size);
#elif defined(__APPLE__)
    return ::getxattr(path.c_str(), canonic.c_str(), value, size, 0, 0);
#elif defined(__FreeBSD__)
    return ::extattr_get_file(path.c_str(), EXTATTR_NAMESPACE_USER, canonic.c_str(), value, size);
#else
    ImplementMe;
#endif
}


int
  FileUtils::setxattr(const String &path, const String &name, const void *value,
  size_t size, int flags) {
    String canonic = (String)"user." + name;
#if defined(__linux__)
    return ::setxattr(path.c_str(), canonic.c_str(), value, size, flags);
#elif defined(__APPLE__)
    return ::setxattr(path.c_str(), canonic.c_str(), value, size, 0, flags);
#elif defined(__FreeBSD__)
    return ::extattr_set_file(path.c_str(), EXTATTR_NAMESPACE_USER, canonic.c_str(), value, size);
#else
    ImplementMe;
#endif
}


int FileUtils::fgetxattr(int fd, const String &name, void *value, size_t size) {
  String canonic = (String)"user." + name;
#if defined(__linux__)
  return ::fgetxattr(fd, canonic.c_str(), value, size);
#elif defined(__APPLE__)
  return ::fgetxattr(fd, canonic.c_str(), value, size, 0, 0);
#elif defined(__FreeBSD__)
  return ::extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, canonic.c_str(), value, size);
#else
  ImplementMe;
#endif
}


int
  FileUtils::fsetxattr(int fd, const String &name, const void *value,
  size_t size, int flags) {
    String canonic = (String)"user." + name;
#if defined(__linux__)
    return ::fsetxattr(fd, canonic.c_str(), value, size, flags);
#elif defined(__APPLE__)
    return ::fsetxattr(fd, canonic.c_str(), value, size, 0, flags);
#elif defined(__FreeBSD__)
    return ::extattr_set_fd(fd, EXTATTR_NAMESPACE_USER, canonic.c_str(), value, size);
#else
    ImplementMe;
#endif
}


int FileUtils::removexattr(const String &path, const String &name) {
  String canonic = (String)"user." + name;
#if defined(__linux__)
  return ::removexattr(path.c_str(), canonic.c_str());
#elif defined(__APPLE__)
  return ::removexattr(path.c_str(), canonic.c_str(), 0);
#elif defined(__FreeBSD__)
  return ::extattr_delete_file(path.c_str(), EXTATTR_NAMESPACE_USER, canonic.c_str());
#else
  ImplementMe;
#endif
}

int FileUtils::fremovexattr(int fd, const String &name) {
  String canonic = (String)"user." + name;
#if defined(__linux__)
  return ::fremovexattr(fd, canonic.c_str());
#elif defined(__APPLE__)
  return ::fremovexattr(fd, canonic.c_str(), 0);
#elif defined(__FreeBSD__)
  return ::extattr_delete_fd(fd, EXTATTR_NAMESPACE_USER, canonic.c_str());
#else
  ImplementMe;
#endif

}

#endif // HT_XATTR_ENABLED
