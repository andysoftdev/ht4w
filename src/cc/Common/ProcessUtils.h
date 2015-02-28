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

#ifndef HYPERTABLE_PROCESSUTILS_H
#define HYPERTABLE_PROCESSUTILS_H

#ifndef _WIN32
#error Platform isn't supported
#endif

#include <vector>

namespace Hypertable {

  class ProcessUtils {
  public:

    static bool create(const char* cmd_line, DWORD flags, STARTUPINFO& si, const char* outfile, bool append_output, PROCESS_INFORMATION& pi, HANDLE& houtfile);
    static bool create(const char* cmd_line, DWORD flags, const STARTUPINFO& si, PROCESS_INFORMATION& pi);
    static void find(const char* exe_name, std::vector<DWORD>& pids);
    static bool exists(DWORD pid);
    static bool join(DWORD pid, DWORD timeout_ms);
    static bool join(const std::vector<DWORD>& pids, bool joinAll, DWORD timeout_ms);
    static void kill(DWORD pid, DWORD timeout_ms);
  };

}

#endif // HYPERTABLE_PROCESSUTILS_H
