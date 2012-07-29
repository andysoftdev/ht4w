/** -*- C++ -*-
 * Copyright (C) 2010-2012 Thalmann Software & Consulting, http://www.softdev.ch
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

#ifndef _WIN32
#error Platform isn't supported
#endif

#include "Common/Compat.h"
#include "ProcessUtils.h"
#include "String.h"
#include "Logger.h"

#include <tlhelp32.h>

using namespace Hypertable;

#define WINAPI_ERROR( msg ) \
  { \
    DWORD err = GetLastError(); \
    HT_ERRORF(msg, winapi_strerror(err)); \
    SetLastError(err); \
  }

bool ProcessUtils::create(const char* cmd_line, DWORD flags, STARTUPINFO& si, const char* outfile, bool append_output, PROCESS_INFORMATION& pi, HANDLE& houtfile) {
  houtfile = INVALID_HANDLE_VALUE;
  si.cb = sizeof(STARTUPINFO);

  if (outfile && *outfile) {
    SECURITY_ATTRIBUTES sa;
    ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    houtfile = CreateFile(outfile, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, &sa, append_output ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_FLAG_WRITE_THROUGH, 0);
    if (houtfile != INVALID_HANDLE_VALUE) {
      if (append_output)
        if (SetFilePointer(houtfile, 0, 0, FILE_END) == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
          WINAPI_ERROR("SetFilePointer failed - %s")
      si.hStdOutput = si.hStdError = houtfile;
      si.dwFlags |= STARTF_USESTDHANDLES;
    }
    else 
      WINAPI_ERROR("CreateFile failed - %s")
  }
  return create(cmd_line, flags, si, pi);
}

bool ProcessUtils::create(const char* cmd_line, DWORD flags, const STARTUPINFO& si, PROCESS_INFORMATION& pi) {
  ZeroMemory(&pi, sizeof(pi));
  pi.hProcess = pi.hThread = INVALID_HANDLE_VALUE;

  bool succeeded = false;
  if (cmd_line && *cmd_line) {
    if (CreateProcess(0, (char*)cmd_line, 0, 0, TRUE, flags, 0, 0, const_cast<STARTUPINFO*>(&si), &pi))
      succeeded = true;
    else
      WINAPI_ERROR("CreateProcess failed - %s")
  }
  return succeeded;
}

void ProcessUtils::find(const char* exe_name, std::vector<DWORD>& pids) {
  pids.clear();
  if (exe_name && *exe_name) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
      PROCESSENTRY32 pe;
      ZeroMemory(&pe, sizeof(pe));
      pe.dwSize = sizeof(pe);
      for (BOOL success = Process32First(snapshot, &pe); success; success = Process32Next(snapshot, &pe)) {
        if (stricmp(exe_name, pe.szExeFile) == 0) {
          pids.push_back(pe.th32ProcessID);
        }
      }
      if (!CloseHandle(snapshot) )
        WINAPI_ERROR("CloseHandle failed - %s")
    }
    else
      WINAPI_ERROR("CreateToolhelp32Snapshot failed - %s")
  }
}

bool ProcessUtils::exists(DWORD pid) {
  bool exists = false;
  HANDLE p = OpenProcess(SYNCHRONIZE, FALSE, pid);
  if (p) {
    exists = true;
  }
  else if (GetLastError() != ERROR_INVALID_PARAMETER)
    WINAPI_ERROR("OpenProcess failed - %s")

  return exists;
}

bool ProcessUtils::join(DWORD pid, DWORD timeout_ms) {
  bool joined = false;
  HANDLE p = OpenProcess(SYNCHRONIZE, FALSE, pid);
  if (p) {
    switch (WaitForSingleObject(p, timeout_ms)) {
      case WAIT_OBJECT_0:
      case WAIT_ABANDONED:
        joined = true;
        break;
      case WAIT_TIMEOUT:
        break;
      default:
        WINAPI_ERROR("WaitForSingleObject failed - %s")
        break;
    }
    if (!CloseHandle(p))
      WINAPI_ERROR("CloseHandle failed - %s")
  }
  else if (GetLastError() == ERROR_INVALID_PARAMETER)
    joined = true; // process id does not exists
  else
    WINAPI_ERROR("OpenProcess failed - %s")

  return joined;
}

bool ProcessUtils::join(const std::vector<DWORD>& pids, bool joinAll, DWORD timeout_ms) {
  bool joined = false;
  if (pids.size()) {
    HANDLE* wait_handles = new HANDLE[pids.size()];
    if (wait_handles) {
      HANDLE p;
      DWORD num_wait_handles = 0;
      foreach(DWORD pid, pids) {
        if ((p = OpenProcess(SYNCHRONIZE, FALSE, pid))) {
          wait_handles[num_wait_handles++] = p;
        }
        else if (GetLastError() != ERROR_INVALID_PARAMETER)
          WINAPI_ERROR("OpenProcess failed - %s")
      }
      if (num_wait_handles) {
        switch (WaitForMultipleObjects(num_wait_handles, wait_handles, joinAll, timeout_ms)) {
          case WAIT_TIMEOUT:
            break;
          case WAIT_FAILED:
            WINAPI_ERROR("WaitForMultipleObjects failed - %s")
            break;
          default:
            joined = true;
            break;
        }
        for (DWORD n = 0; n < num_wait_handles; ++n)
          if (!CloseHandle(wait_handles[n]))
            WINAPI_ERROR("CloseHandle failed - %s")
      }
      delete [] wait_handles;
    }
  }
  return joined;
}

void ProcessUtils::kill(DWORD pid, DWORD timeout_ms) {
  bool killed = false;
  HANDLE p = OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE, FALSE, pid);
  if (p) {
    if (!TerminateProcess(p, -1) && GetLastError() != ERROR_ACCESS_DENIED)
      WINAPI_ERROR("TerminateProcess failed - %s")
    switch (WaitForSingleObject(p, timeout_ms)) {
      case WAIT_OBJECT_0:
      case WAIT_ABANDONED:
        killed = true;
        break;
      case WAIT_TIMEOUT:
        HT_ERRORF("TerminateProcess pid=%d time out", pid);
        break;
      default:
        WINAPI_ERROR("WaitForSingleObject failed - %s")
        break;
    }
    if (!CloseHandle(p))
      WINAPI_ERROR("CloseHandle failed - %s")
  }
  else if (GetLastError() == ERROR_INVALID_PARAMETER)
    return;
  else if (GetLastError() != ERROR_ACCESS_DENIED)
    WINAPI_ERROR("OpenProcess failed - %s")

  if (!killed) {
    system(format("taskkill.exe /PID %d /F >nul", pid).c_str());
  }
}