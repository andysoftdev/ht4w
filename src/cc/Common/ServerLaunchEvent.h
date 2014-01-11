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

#ifndef HYPERTABLE_SERVERLAUNCHEVENT_H
#define HYPERTABLE_SERVERLAUNCHEVENT_H

#ifndef _WIN32
#error Platform isn't supported
#endif

namespace Hypertable {

  class ServerLaunchEvent {
  public:
    ServerLaunchEvent();
    ServerLaunchEvent(const char* preffix);
    ServerLaunchEvent(DWORD pid);
    ServerLaunchEvent(const char* preffix, DWORD pid);
    virtual ~ServerLaunchEvent();

    void set_event();
    bool wait(int timeout_ms, bool& timed_out);
    inline bool is_existing_event() const { 
      return already_exists;
    }
    inline ::HANDLE handle() const { 
      return evt;
    }

  private:
    ::HANDLE evt;
    DWORD pid;
    bool already_exists;

    static ::HANDLE create_event(const char* preffix, DWORD pid, bool& already_exists);
  };

}

#endif // HYPERTABLE_SERVERLAUNCHEVENT_H
