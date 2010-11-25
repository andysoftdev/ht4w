/**
 * Copyright (C) 2007 Luke Lu (Zvents, Inc.)
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
 * along with Hypertable. If not, see <http://www.gnu.org/licenses/>
 */

#ifndef HYPERTABLE_COMPAT_C_H
#define HYPERTABLE_COMPAT_C_H

/** Portability macros for C code. */

/* Name mangling */
#ifdef __cplusplus
#  define HT_EXTERN_C  extern "C"
#else
#  define HT_EXTERN_C  extern
#endif

/* Calling convention */
#ifdef _MSC_VER
#  define HT_CDECL      __cdecl
#else
#  define HT_CDECL
#endif

#define HT_PUBLIC(ret_type)     ret_type HT_CDECL
#define HT_EXTERN(ret_type)     HT_EXTERN_C HT_PUBLIC(ret_type)

#ifdef __GNUC__
#  define HT_NORETURN __attribute__((__noreturn__))
#  define HT_FORMAT(x) __attribute__((format x))
#  define HT_FUNC __PRETTY_FUNCTION__
#  define HT_COND(x, _prob_) __builtin_expect(x, _prob_)
#elif _MSC_VER
#  define HT_NORETURN //__declspec(noreturn)
#  define HT_FORMAT(x)
#  ifndef __attribute__
#    define __attribute__(x)
#  endif
#  define __func__ __FUNCTION__
#  define HT_FUNC __FUNCTION__
#  define HT_COND(x, _prob_) (__assume((_prob_) ? (x) : (!(x))), (x))
#else
#  define HT_NORETURN
#  define HT_FORMAT(x)
#  ifndef __attribute__
#    define __attribute__(x)
#  endif
#  define HT_FUNC __func__
#  define HT_COND(x, _prob_) (x)
#endif

#define HT_LIKELY(x) HT_COND(x, 1)
#define HT_UNLIKELY(x) HT_COND(x, 0)

/* We want C limit macros, even when using C++ compilers */
#ifndef __STDC_LIMIT_MACROS
#  define __STDC_LIMIT_MACROS
#endif


#ifdef _MSC_VER
#define inline __inline
#endif

#ifdef _WIN32

#pragma warning( disable : 4355 ) // 'this' : used in base member initializer list
#pragma warning( disable : 4003 ) // not enough actual parameters for macro
#pragma warning( disable : 4996 ) // the POSIX name for this item is deprecated. Instead, use the ISO C++ conformant name
#pragma warning( disable : 4244 ) // conversion from 'x' to 'y', possible loss of data

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <mswsock.h>
#include <ctype.h>
#include <fcntl.h>
#include <intrin.h>
#include <io.h>
#include <math.h>
#include <process.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#ifdef __cplusplus

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <functional>
#include <hash_map>
#include <hash_set>
#include <iomanip>
#include <iosfwd>
#include <iostream>
#include <istream>
#include <iterator>
#include <limits.h>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <new>
#include <ostream>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <streambuf>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#endif

#define HT_USE_ABORT
#ifndef _DEBUG
#define HT_DISABLE_LOG_DEBUG 1
#endif

typedef int socket_t; // should be SOCKET, but SOCKET is unsigned 32/64
typedef int socklen_t;

typedef signed __int64 _off64_t;
typedef signed __int64 off64_t;

#define _off_t _off64_t
#define off_t off64_t

typedef int pid_t;
extern struct tm * gmtime_r(const time_t *timer, struct tm *result);

#define HAVE_STRUCT_TIMESPEC
typedef struct timespec {
    time_t tv_sec;	// Seconds since 00:00:00 GMT, 1 January 1970
    long tv_nsec;	// Additional nanoseconds since tv_sec
} timespec_t;

const char* winapi_strerror( DWORD err );

#pragma deprecated(_errno)
#pragma deprecated(perror)
#pragma deprecated(strerror)

#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strtok_r strtok_s
#define strtoll _strtoi64
#define strtoull _strtoui64
#define snprintf _snprintf
#define getpid _getpid
#define atoll _atoi64
#define SLEEP Sleep

#undef ERROR

#ifdef _WIN64
typedef int64_t ssize_t;
#else
typedef int32_t ssize_t;
#endif

#else

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

typedef int socket_t;
#define INVALID_SOCKET -1
#define off_t size_t

#endif 

#endif /* HYPERTABLE_COMPAT_C_H */
