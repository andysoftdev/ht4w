/** -*- C++ -*-
 * Copyright (C) 2011 Andy Thalmann
 *
 * This file is part of ht4w.
 *
 * ht4w is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
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

#ifndef _WIN32
#error Platform isn't supported
#endif

#include "Common/Compat.h"
#include "ServerLaunchEvent.h"
#include "String.h"
#include "Logger.h"
#include "SecurityUtils.h"

using namespace Hypertable;

#define WINAPI_ERROR( msg ) \
{ \
  if (Logger::logger) { \
    DWORD err = GetLastError(); \
    HT_ERRORF(msg, winapi_strerror(err)); \
    SetLastError(err); \
  } \
}

ServerLaunchEvent::ServerLaunchEvent()
  : evt(create_event(0, GetCurrentProcessId(), already_exists)) {
}

ServerLaunchEvent::ServerLaunchEvent(const char* preffix)
  : evt(create_event(preffix, GetCurrentProcessId(), already_exists)) {
}

ServerLaunchEvent::ServerLaunchEvent(DWORD pid)
  : evt(create_event(0, pid, already_exists)) {
}

ServerLaunchEvent::ServerLaunchEvent(const char* preffix, DWORD pid)
  : evt(create_event(preffix, pid, already_exists)) {
}

ServerLaunchEvent::~ServerLaunchEvent() {
  if (evt) {
    if (!ResetEvent(evt))
      WINAPI_ERROR("ResetEvent failed - %s");
    if (!CloseHandle(evt))
      WINAPI_ERROR("CloseHandle failed - %s");
  }
}

void ServerLaunchEvent::set_event() {
  if (evt) {
    if (!SetEvent(evt))
      WINAPI_ERROR("SetEvent failed - %s");
  }
  else
    HT_ERRORF("ServerLaunchEvent::set_event - invalid event handle");
}

bool ServerLaunchEvent::wait(int timeout_ms) {
  if (evt) {
    switch (WaitForSingleObject(evt, timeout_ms)) {
      case WAIT_OBJECT_0:
        return true;
      case WAIT_ABANDONED: {
        HT_ERRORF("ServerLaunchEvent::wait - abandoned");
        break;
      }
      case WAIT_FAILED:
        WINAPI_ERROR("WaitForSingleObject failed - %s");
        break;
      default:
        break;
    }
  }
  else
    HT_ERRORF("ServerLaunchEvent::wait - invalid event handle");
  return false;
}

HANDLE ServerLaunchEvent::create_event(const char* preffix, DWORD pid, bool& already_exists) {
  HANDLE evt = CreateEvent(0, TRUE, FALSE, format("Global\\%s_%d", preffix && *preffix ? preffix : "Hypertable", pid).c_str());
  already_exists = GetLastError() == ERROR_ALREADY_EXISTS;
  if (!evt) {
    WINAPI_ERROR("CreateEvent failed - %s");
    return 0;
  }
  else if (!already_exists) {
    if (!SecurityUtils::set_security_info(evt, WinBuiltinUsersSid, EVENT_ALL_ACCESS))
      if (Logger::logger)
        HT_ERROR("set_security_info failed");
  }
  return evt;
}
