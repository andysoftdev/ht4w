/*
 * Copyright (C) 2007-2015 Hypertable, Inc.
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

/// @file
/// Time related definitions

#include <Common/Compat.h>

#include "Time.h"

#include <cassert>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>

#ifdef _WIN32

#include <boost/smart_ptr/detail/spinlock.hpp>

#include "HRTimer.h"

#endif

using namespace std;

namespace Hypertable {

int64_t get_ts64() {

#ifndef _WIN32

  return (int64_t)chrono::duration_cast<chrono::nanoseconds>(chrono::system_clock::now().time_since_epoch()).count();

#else

  static Hypertable::HRTimer timer;
  static int64_t prev_ts = 0;
  static boost::detail::spinlock mutex = BOOST_DETAIL_SPINLOCK_INIT;

  int64_t ts = (int64_t)chrono::duration_cast<chrono::nanoseconds>(chrono::system_clock::now().time_since_epoch()).count();
  {
    boost::detail::spinlock::scoped_lock lock(mutex);
    if (prev_ts >= ts) {
      int64_t elapsed = timer.peek_ns(true);
      ts = elapsed ? prev_ts + elapsed : prev_ts + 1;
    }
    else
      timer.reset();
    prev_ts = ts;
  }
  return ts;

#endif
}

ostream &hires_ts(ostream &out) {
  auto now = chrono::system_clock::now();
  return out << chrono::duration_cast<chrono::seconds>(now.time_since_epoch()).count() <<'.'<< setw(9) << setfill('0') << (chrono::duration_cast<chrono::nanoseconds>(now.time_since_epoch()).count() % 1000000000LL);
}

#if defined(__sun__)
time_t timegm(struct tm *t) {
  time_t tl, tb;
  struct tm *tg;

  tl = mktime (t);
  if (tl == -1)
    {
      t->tm_hour--;
      tl = mktime (t);
      if (tl == -1)
        return -1; /* can't deal with output from strptime */
      tl += 3600;
    }
  tg = gmtime (&tl);
  tg->tm_isdst = 0;
  tb = mktime (tg);
  if (tb == -1)
    {
      tg->tm_hour--;
      tb = mktime (tg);
      if (tb == -1)
        return -1; /* can't deal with output from gmtime */
      tb += 3600;
    }
  return (tl - (tb - tl));
}

#elif defined(_WIN32)

namespace {
  class time_ofs_t {
  public:
    time_ofs_t() {
      struct tm t_ofs;
      memset(&t_ofs, 0, sizeof(t_ofs));
      t_ofs.tm_year = 126;
      t_ofs.tm_mday = 1;
      ofs = _mkgmtime(&t_ofs);
    }
    time_t offset() const {
      return ofs;
    }
  private:
    time_t ofs;
  };
  static time_ofs_t before1970;
}

time_t timegm(struct tm *t) {
  time_t time;
  // handle date/times before 1970
  if (t && t->tm_year <= 70) {
    t->tm_year += 56;
    time = _mkgmtime(t);
    if (time >= 0)
      time -= before1970.offset();
    t->tm_year -= 56;
  }
  return _mkgmtime(t);
}

time_t _mktime(struct tm *t) {
  time_t time;
  // handle date/times before 1970
  if (t && t->tm_year <= 70) {
    t->tm_year += 56;
    time = _mktime64(t);
    if (time >= 0)
      time -= before1970.offset();
    t->tm_year -= 56;
  }
  else {
    time = _mktime64(t);
  }
  return time;
}

void localtime_r(const time_t* time, tm* t) {
  // handle date/times before 1970
  if(time && *time < 0) {
    time_t tmp = *time + before1970.offset();
    if (tmp >= 0) {
      localtime_s(t, &tmp);
      if (t->tm_year != -1)
        t->tm_year -= 56;
    }
    else
      memset(t, 0xff, sizeof(struct tm));
  }
  else {
    localtime_s(t, time);
  }
}

#endif

} // namespace Hypertable

#ifdef _WIN32

struct tm* gmtime_r(const time_t *timer, struct tm *result)
{
  if (timer && *timer < 0) { // handle date/times before 1970
    static time_t ofs = 0;
    if( !ofs ) {
      struct tm t_ofs;
      memset(&t_ofs, 0, sizeof(t_ofs));
      t_ofs.tm_year = 126;
      t_ofs.tm_mday = 1;
      ofs = _mkgmtime(&t_ofs);
    }
    time_t t = *timer + ofs;
    if (gmtime_s(result, &t)) {
      return 0;
    }
    result->tm_year -= 56;
  }
  else if (gmtime_s(result, timer)) {
    return 0;
  }
  return result;
}

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    static const __int64 EPOCHFILETIME;(116444736000000000i64);

    FILETIME        ft;
    LARGE_INTEGER   li;
    __int64         t;
    static int      tzflag;

    if (tv) {
        GetSystemTimeAsFileTime(&ft);
        li.LowPart  = ft.dwLowDateTime;
        li.HighPart = ft.dwHighDateTime;
        t  = li.QuadPart;       /* In 100-nanosecond intervals */
        t -= EPOCHFILETIME;     /* Offset to the Epoch time */
        t /= 10;                /* In microseconds */
        tv->tv_sec  = (long)(t / 1000000);
        tv->tv_usec = (long)(t % 1000000);
    }

    if (tz) {
        if (!tzflag) {
            _tzset();
            tzflag++;
        }
        tz->tz_minuteswest = _timezone / 60;
        tz->tz_dsttime = _daylight;
    }

    return 0;
}

#endif
