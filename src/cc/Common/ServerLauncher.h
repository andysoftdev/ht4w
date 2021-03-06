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
 * Launches an external command, redirects its output to a file. Used for
 * testing.
 */

#ifndef Common_ServerLauncher_h
#define Common_ServerLauncher_h

#include <chrono>
#include <iostream>
#include <thread>

extern "C" {
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
}

#ifdef _WIN32
#include "Common/ServerLaunchEvent.h"
#include "Common/ProcessUtils.h"
#endif

namespace Hypertable {

#ifndef _WIN32

 /** @addtogroup Common
   *  @{
   */

  /**
   * Launches external commands and redirects their output to a file; kills
   * the external process when going out of scope.
   * This class is used for testing purposes.
   */
  class ServerLauncher {
  public:
    /** Constructor; launches the external command, optionally redirects the
     * output.
     *
     * @param path The path of the external program
     * @param argv The argument list of the program
     * @param outfile Path of the redirected output file (for stdout AND
     *      stderr); optional. If NULL then will use stdout/stderr of the
     *      host program
     * @param append_output If true, output will be appended. Otherwise output
     *      file will be overwritten
     */
    ServerLauncher(const char *path, char *const argv[],
            const char *outfile = 0, bool append_output = false) {
      int fd[2];
      m_path = path;
      if (pipe(fd) < 0) {
        perror("pipe");
        exit(EXIT_FAILURE);
      }
      if ((m_child_pid = fork()) == 0) {
        if (outfile) {
          int open_flags;
          int outfd = -1;

          if (append_output)
            open_flags = O_CREAT | O_APPEND | O_RDWR;
          else
            open_flags = O_CREAT | O_TRUNC | O_WRONLY,

          outfd = open(outfile, open_flags, 0644);
          if (outfd < 0) {
            perror("open");
            exit(EXIT_FAILURE);
          }
          dup2(outfd, 1);
          dup2(outfd, 2);
        }
        close(fd[1]);
        dup2(fd[0], 0);
        close(fd[0]);
        execv(path, argv);
      }
      close(fd[0]);
      m_write_fd = fd[1];
      std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }

    /** Destructor; kills the external program */
    ~ServerLauncher() {
      std::cerr << "Killing '" << m_path << "' pid=" << m_child_pid
          << std::endl << std::flush;
      close(m_write_fd);
      if (kill(m_child_pid, 9) == -1)
        perror("kill");
    }

    /** Returns stdin of the external program */
    int get_write_descriptor() const {
      return m_write_fd;
    }

    /** Returns the pid of the external program */
    pid_t get_pid() const {
      return m_child_pid;
    }

  private:
    /** The path of the external program */
    const char *m_path;

    /** The pid of the external program */
    pid_t m_child_pid;

    /** The file descriptor writing to stdin of the external program */
    int m_write_fd;
  };

  /** @} */

#else

  class ServerLauncher {
  public:
    ServerLauncher(const char *path, char *const argv[],
                   const char *outfile = 0, bool append_output = false)
     : m_write_fd(-1), m_outfd(INVALID_HANDLE_VALUE), m_hReadPipe(INVALID_HANDLE_VALUE) {

      if( !path || !*path) {
          HT_FATALF("Invalid argument");
      }

      std::string cmdline = path;
      if (argv) {
        for (const char* const* pp=argv+1; *pp; pp++) {
          cmdline = format("%s \"%s\"", cmdline.c_str(), *pp);
        }
      }

      // create pipe
      HANDLE hWritePipe;
      SECURITY_ATTRIBUTES sa;
      ZeroMemory(&sa, sizeof(sa));
      sa.nLength = sizeof(SECURITY_ATTRIBUTES);
      sa.bInheritHandle = TRUE;

      if (!CreatePipe(&m_hReadPipe, &hWritePipe, &sa, 0)) {
        HT_FATALF("CreatePipe error: %s", winapi_strerror(GetLastError()));
      }
      // Ensure the write handle to the pipe for STDIN is not inherited.
      if (!SetHandleInformation(hWritePipe, HANDLE_FLAG_INHERIT, 0) ) {
        CloseHandle(hWritePipe);
        CloseHandle(m_hReadPipe);
        HT_FATALF("stdin pipe SetHandleInformation error: %s", winapi_strerror(GetLastError()));
      }

      m_write_fd = _open_osfhandle((intptr_t)hWritePipe, 0);
      if( m_write_fd == -1 ) {
        DWORD err = GetLastError();
        CloseHandle(hWritePipe);
        CloseHandle(m_hReadPipe);
        HT_FATALF("_open_osfhandle failed: %s", winapi_strerror(err));
      }

      STARTUPINFO si;
      ZeroMemory(&si, sizeof(si));
      si.cb = sizeof(STARTUPINFO);
      si.dwFlags |= STARTF_USESTDHANDLES;
      si.hStdInput = m_hReadPipe;
      si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
      si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

      // create process
      if (!ProcessUtils::create(cmdline.c_str(), 0, si, outfile, append_output, m_pi, m_outfd)) {
        if( m_outfd != INVALID_HANDLE_VALUE ) {
          CloseHandle(m_outfd);
        }
        CloseHandle(hWritePipe);
        CloseHandle(m_hReadPipe);
        exit(1);
      }

      // wait
      ServerLaunchEvent server_launch_event(m_pi.dwProcessId);
      bool timed_out;
      server_launch_event.wait(10000, timed_out);

      if (!CloseHandle(m_pi.hThread)) {
        HT_ERRORF("CloseHandle error: %s", winapi_strerror(GetLastError()));
      }
      if (!CloseHandle(m_pi.hProcess)) {
        HT_ERRORF("CloseHandle error: %s", winapi_strerror(GetLastError()));
      }
      Sleep(200);
    }

    ~ServerLauncher() {
      if (m_outfd != INVALID_HANDLE_VALUE) {
        if (!CloseHandle(m_outfd)) {
          HT_ERRORF("CloseHandle error: %s", winapi_strerror(GetLastError()));
        }
      }
      _close(m_write_fd);
      if (!CloseHandle(m_hReadPipe)) {
        HT_ERRORF("CloseHandle error: %s", winapi_strerror(GetLastError()));
      }
      std::cerr << "Killing pid=" << m_pi.dwProcessId << std::endl << std::flush;
      ProcessUtils::kill(m_pi.dwProcessId, 5000);
    }

    int get_write_descriptor() { return m_write_fd; }
    pid_t get_pid() { return m_pi.dwProcessId; }

  private:
    int m_write_fd;
    HANDLE m_outfd;
    HANDLE m_hReadPipe;
    PROCESS_INFORMATION m_pi;
  };

#endif

}

#endif // Common_ServerLauncher_h
