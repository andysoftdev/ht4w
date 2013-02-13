/** -*- C++ -*-
 * Copyright (C) 2010-2013 Thalmann Software & Consulting, http://www.softdev.ch
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

#ifndef HYPERTABLE_HRTIMER_H
#define HYPERTABLE_HRTIMER_H

#ifndef _WIN32
#error Platform isn't supported
#endif

#include "Common/Error.h"
#include "Common/Logger.h"

namespace Hypertable {

class HRTimer {
  public:
    inline explicit HRTimer(const char* _msg = 0) {
      if (_msg)
        msg = _msg;
      LARGE_INTEGER _freq;
      if (!::QueryPerformanceFrequency(&_freq))
        HT_THROWF(Error::EXTERNAL, "QueryPerformanceFrequency failed, %s", winapi_strerror(::GetLastError()));
      freq = (double)_freq.QuadPart;
      if (!::QueryPerformanceCounter(&start))
        HT_THROWF(Error::EXTERNAL, "QueryPerformanceCounter failed, %s", winapi_strerror(::GetLastError()));
    }

    inline void reset() {
      if (!::QueryPerformanceCounter(&start))
          HT_THROWF(Error::EXTERNAL, "QueryPerformanceCounter failed, %s", winapi_strerror(::GetLastError()));
    }

    inline double peek_ms(bool reset = false) {
      if (!::QueryPerformanceCounter(&peek))
          HT_THROWF(Error::EXTERNAL, "QueryPerformanceCounter failed, %s", winapi_strerror(::GetLastError()));
      double ms = (peek.QuadPart - start.QuadPart) / (freq / 1e3);
      if (reset)
        start = peek;
      return ms;
    }

    inline int64_t peek_ns(bool reset = false) {
      if (!::QueryPerformanceCounter(&peek))
          HT_THROWF(Error::EXTERNAL, "QueryPerformanceCounter failed, %s", winapi_strerror(::GetLastError()));
      int64_t ns = (int64_t)((peek.QuadPart - start.QuadPart) / (freq / 1e9));
      if (reset)
        start = peek;
      return ns;
    }

    inline ~HRTimer() {
      if (!msg.empty())
        HT_NOTICE_OUT << "[" << ::GetCurrentThreadId() << "] " << msg.c_str() << " " << peek_ms() << "ms" << HT_END;
    }

private:

    LARGE_INTEGER start;
    LARGE_INTEGER peek;
    double freq;
    std::string msg;
};


}

#endif // HYPERTABLE_HRTIMER_H
