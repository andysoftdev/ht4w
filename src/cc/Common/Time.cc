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

/** @file
 * High resolution time handling based on boost::xtime.
 */

#include "Common/Compat.h"

#include <time.h>
#include <iomanip>

#include "Time.h"
#include "Mutex.h"

#ifdef _WIN32

#include "HRTimer.h"

#endif

using namespace std;

namespace Hypertable {

int64_t get_ts64() {
#ifndef _WIN32
  HiResTime now;
  return ((int64_t)now.sec * 1000000000LL) + (int64_t)now.nsec;
#else
  static Hypertable::HRTimer timer;
  static int64_t prev_ts = 0;
  static boost::detail::spinlock mutex = BOOST_DETAIL_SPINLOCK_INIT;

  HiResTime now;
  int64_t ts = ((int64_t)now.sec * 1000000000LL) + (int64_t)now.nsec;
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

boost::xtime get_xtime() {
  int64_t ts = get_ts64();
  boost::xtime now;
  now.sec = ts / 1000000000LL;
  now.nsec = ts % 1000000000LL;
  return now;
}

bool xtime_add_millis(boost::xtime &xt, uint32_t millis) {
  uint64_t nsec = (uint64_t)xt.nsec + ((uint64_t)millis * 1000000LL);
  if (nsec > 1000000000LL) {
    uint32_t new_secs = xt.sec + (uint32_t)(nsec / 1000000000LL);
    if (new_secs < xt.sec)
      return false;
    xt.sec = new_secs;
    xt.nsec = (uint32_t)(nsec % 1000000000LL);
  }
  else
    xt.nsec = nsec;
  return true;
}

bool xtime_sub_millis(boost::xtime &xt, uint32_t millis) {
  uint64_t nsec = (uint64_t)millis * 1000000LL;

  if (nsec <= (uint64_t)xt.nsec)
    xt.nsec -= (uint32_t)nsec;
  else {
    uint32_t secs = millis / 1000;
    uint32_t rem = (uint32_t)(nsec % 1000000000LL);
    if (rem <= (uint32_t)xt.nsec) {
      if (secs < xt.sec)
        return false;
      xt.sec -= secs;
      xt.nsec -= rem;
    }
    else {
      secs++;
      if (secs < xt.sec)
        return false;
      xt.sec -= secs;
      xt.nsec = 1000000000LL - (rem % xt.nsec);
    }
  }
  return true;
}

int64_t xtime_diff_millis(boost::xtime &early_xt, boost::xtime &late_xt) {
  int64_t total_millis = 0;

  if (early_xt.sec > late_xt.sec)
    return 0;

  if (early_xt.sec < late_xt.sec) {
    total_millis = (late_xt.sec - (early_xt.sec+1)) * 1000;
    total_millis += 1000 - (early_xt.nsec / 1000000);
    total_millis += late_xt.nsec / 1000000;
  }
  else if (early_xt.nsec > late_xt.nsec)
    return 0;
  else
    total_millis = (late_xt.nsec - early_xt.nsec) / 1000000;

  return total_millis;
}

std::ostream &hires_ts(std::ostream &out) {
  HiResTime now;
  return out << now.sec << '.' << setw(9) << setfill('0') << now.nsec;
}

std::ostream &hires_ts_date(std::ostream &out) {
  tm tv;
  HiResTime now;
  time_t s = now.sec; // using const time_t * is not convenient
  gmtime_r(&s, &tv);
  return out << tv.tm_year + 1900 << '-'
             << right << setw(2) << setfill('0') << tv.tm_mon + 1 << '-'
             << right << setw(2) << setfill('0') << tv.tm_mday
             << ' '
             << right << setw(2) << setfill('0') << tv.tm_hour << ':'
             << right << setw(2) << setfill('0') << tv.tm_min << ':'
             << right << setw(2) << setfill('0') << tv.tm_sec << '.'
             << right << setw(9) << setfill('0') << now.nsec;
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

#endif
