/*
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
 * Compatibility Macros for C/C++.
 * This file contains compatibility macros for C/C++. Always include this
 * file before including any other file!
 */

#ifndef HYPERTABLE_COMPAT_H
#define HYPERTABLE_COMPAT_H

/** @addtogroup Common
 *  @{
 */

// The same stuff for C code
#include "compat-c.h"

#include <cstddef> // for std::size_t and std::ptrdiff_t
#include <memory>

#if _MSC_VER <= 1800

// the chrono nightmare starts here
#define _CHRONO_
#include <boost/chrono/chrono.hpp>
#include <thr/xtimec.h>

namespace std {
  namespace chrono {

    typedef ::boost::chrono::steady_clock steady_clock;
    typedef ::boost::chrono::system_clock system_clock;

    typedef ::boost::chrono::hours hours;
    typedef ::boost::chrono::minutes minutes;
    typedef ::boost::chrono::seconds seconds;
    typedef ::boost::chrono::milliseconds milliseconds;
    typedef ::boost::chrono::microseconds microseconds;
    typedef ::boost::chrono::nanoseconds nanoseconds;

    template<typename Clock, typename Duration = typename Clock::duration>
    using time_point = ::boost::chrono::time_point<Clock, Duration>;

    template<typename Rep, typename Period>
    using duration = ::boost::chrono::duration< Rep, Period>;

    template <typename ToDuration, typename Rep, typename Period>
    inline BOOST_CONSTEXPR
    typename ::boost::enable_if<::boost::chrono::detail::is_duration<ToDuration>, ToDuration>::type
    duration_cast(const duration<Rep, Period>& fd) {
      return ::boost::chrono::detail::duration_cast<
        duration<Rep, Period>, ToDuration>()(fd);
    }

    #define CHRONO_OPERATOR \
      ::boost::chrono::operator

  }

  template<typename _Rep, typename _Period> inline
  ::xtime _To_xtime(const chrono::duration<_Rep, _Period>& _Rel_time){
    // convert duration to xtime
    ::xtime _Xt;
    if (_Rel_time <= chrono::duration<_Rep, _Period>::zero()) {
      // negative or zero relative time, return zero
      _Xt.sec = 0;
      _Xt.nsec = 0;
    }
    else { // positive relative time, convert
      chrono::nanoseconds _T0 =
        chrono::system_clock::now().time_since_epoch();
      _T0 += _Rel_time;
      _Xt.sec = chrono::duration_cast<chrono::seconds>(_T0).count();
      _T0 -= chrono::seconds(_Xt.sec);
      _Xt.nsec = (long)_T0.count();
    }
    return (_Xt);
  }

}

#endif

// C++ specific stuff
#ifndef BOOST_SPIRIT_THREADSAFE
#  define BOOST_SPIRIT_THREADSAFE
#endif

#define BOOST_IOSTREAMS_USE_DEPRECATED
#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_FILESYSTEM_DEPRECATED     1

#define HT_UNUSED(x) static_cast<void>(x)

template<typename T, typename... Ts>
std::unique_ptr<T> make_unique(Ts&&... params) {
  return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}

#if defined(__APPLE__) || !defined(_GLIBCXX_HAVE_QUICK_EXIT)
namespace std {
  inline void quick_exit(int status) { _exit(status); }
}
#endif

/** @}*/

#endif // HYPERTABLE_COMPAT_H

