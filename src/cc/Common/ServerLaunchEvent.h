/** -*- C++ -*-
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

#ifndef HYPERTABLE_SERVERLAUNCHEVENT_H
#define HYPERTABLE_SERVERLAUNCHEVENT_H

#ifndef _WIN32
#error Platform isn't supported
#endif

#include "Common/String.h"
#include "Common/Logger.h"

namespace Hypertable {

  class ServerLaunchEvent {
  public:
    ServerLaunchEvent() : evt(create_event(GetCurrentProcessId())) {
    }
    ServerLaunchEvent(DWORD pid) : evt(create_event(pid)) {
    }

    ~ServerLaunchEvent() {
      if (evt) {
        ResetEvent(evt);
        CloseHandle(evt);
      }
    }

    void set_event() {
      if (evt) {
        if (!SetEvent(evt)) {
          DWORD err = GetLastError();
          HT_ERRORF("SetEvent failed - %s", winapi_strerror(err));
          SetLastError(err);
        }
      }
      else {
        HT_ERRORF("set_event - invalid event handle");
      }
    }

    bool wait(int timeout_ms) {
      if (evt) {
        switch (WaitForSingleObject(evt, timeout_ms)) {
          case WAIT_OBJECT_0:
            return true;
          case WAIT_ABANDONED: {
            HT_ERRORF("wait - abandoned");
            break;
          }
          case WAIT_FAILED: {
            DWORD err = GetLastError();
            HT_ERRORF("WaitForSingleObject failed - %s", winapi_strerror(err));
            SetLastError(err);
            break;
          }
          default:
            break;
        }
      }
      else {
        HT_ERRORF("wait - invalid event handle");
      }
      return false;
    }

  private:
    HANDLE evt;

    static HANDLE create_event(DWORD pid) {
      HANDLE evt = CreateEvent(0, TRUE, FALSE, format("Global\\Hypertable_%d", pid).c_str());
      if (!evt) {
        DWORD err = GetLastError();
        HT_ERRORF("CreateEvent failed - %s", winapi_strerror(err));
        SetLastError(err);
        return 0;
      }
      return evt;
    }
  };

}

#endif // HYPERTABLE_SERVERLAUNCHEVENT_H
