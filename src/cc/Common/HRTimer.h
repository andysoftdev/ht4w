/** -*- C++ -*-
 * Copyright (C) 2011 Andy Thalmann
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
    inline HRTimer(const char* _msg) {
      if (_msg)
        msg = _msg;
      LARGE_INTEGER _freq;
      if (!::QueryPerformanceFrequency(&_freq))
        HT_THROWF(Error::EXTERNAL, "QueryPerformanceFrequency failed, %s", winapi_strerror(::GetLastError()));
      freq = (double)_freq.QuadPart;
      if (!::QueryPerformanceCounter(&start))
        HT_THROWF(Error::EXTERNAL, "QueryPerformanceCounter failed, %s", winapi_strerror(::GetLastError()));
    }

    inline ~HRTimer() {
      if (!::QueryPerformanceCounter(&stop))
        HT_THROWF(Error::EXTERNAL, "QueryPerformanceCounter failed, %s", winapi_strerror(::GetLastError()));
      double delta = (stop.QuadPart - start.QuadPart) / (freq / 1000.0);
      HT_INFO_OUT << std::flush << std::setw(6) << ::GetCurrentThreadId() << " " << msg << " " << std::setprecision(3) << delta << "ms" << std::endl << HT_END;
    }

private:

    LARGE_INTEGER start;
    LARGE_INTEGER stop;
    double freq;
    std::string msg;
};


}

#endif // HYPERTABLE_HRTIMER_H
