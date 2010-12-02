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

#ifndef HYPERTABLE_SERVERLAUNCHER_H
#define HYPERTABLE_SERVERLAUNCHER_H

#include <iostream>

extern "C" {
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <poll.h>
}

namespace Hypertable {

#ifdef _WIN32

  class ServerLauncher {
  public:
    ServerLauncher(const char *path, char *const argv[],
                   const char *outfile = 0, bool append_output = false) {

      if( !path ) {
          HT_FATALF("Invalid argument");
      }
      m_path = path;
      m_write_fd = -1;
      m_outfd = INVALID_HANDLE_VALUE;
      HANDLE hWritePipe;
      SECURITY_ATTRIBUTES sa;
      sa.nLength = sizeof(SECURITY_ATTRIBUTES);
      sa.bInheritHandle = TRUE;
      sa.lpSecurityDescriptor = NULL;
      if (!::CreatePipe(&m_hReadPipe, &hWritePipe, &sa, 0)) {
        HT_FATALF("CreatePipe error: %s", winapi_strerror(::GetLastError()));
      }
      // Ensure the write handle to the pipe for STDIN is not inherited.
      if (!::SetHandleInformation(hWritePipe, HANDLE_FLAG_INHERIT, 0) ) {
        ::CloseHandle(hWritePipe);
        HT_FATALF("stdin pipe SetHandleInformation error: %s", winapi_strerror(::GetLastError()));
      }

      ZeroMemory( &pi, sizeof(pi) );
      STARTUPINFO si;
      ZeroMemory( &si, sizeof(si) );
      si.cb = sizeof(STARTUPINFO);
      si.dwFlags |= STARTF_USESTDHANDLES;
      si.hStdInput = m_hReadPipe;
      si.hStdError  = GetStdHandle( STD_ERROR_HANDLE );
      si.hStdOutput = GetStdHandle( STD_OUTPUT_HANDLE );

      if (outfile) {
        if (append_output) {
          m_outfd = ::CreateFile(outfile, GENERIC_READ|GENERIC_WRITE, 0, &sa, OPEN_ALWAYS, 0, 0);
          if (m_outfd != INVALID_HANDLE_VALUE) {
              ::SetFilePointer(m_outfd, 0, 0, FILE_END);
          }
        }
        else
          m_outfd = ::CreateFile(outfile, GENERIC_READ|GENERIC_WRITE, 0, &sa, CREATE_ALWAYS, 0, 0);
        if (m_outfd == INVALID_HANDLE_VALUE) {
          ::CloseHandle(hWritePipe);
          HT_FATALF("stdout/err file creation error");
        }
        else {
          si.hStdError  = m_outfd;
          si.hStdOutput = m_outfd;
        }
      }

      m_write_fd = _open_osfhandle((intptr_t)hWritePipe, 0);
      if( m_write_fd == -1 ) {
        DWORD err = ::GetLastError();
        if( m_outfd != INVALID_HANDLE_VALUE ) {
          ::CloseHandle(m_outfd);
        }
        ::CloseHandle(hWritePipe);
        HT_FATALF("_open_osfhandle failed: %s", winapi_strerror(err));
      }

      std::string cmdline = m_path;
      if(argv)
        for(const char* const* pp=argv+1; *pp; pp++) {
          cmdline = format("%s \"%s\"", cmdline.c_str(), *pp);
        }
      if (!::CreateProcessA(0, (LPSTR)cmdline.c_str(), 0, 0, TRUE, 0, 0, 0, &si, &pi)) {
        HT_ERRORF("CreateProcess error: %s", winapi_strerror(::GetLastError()));
        if( m_outfd != INVALID_HANDLE_VALUE ) {
          ::CloseHandle(m_outfd);
        }
        ::CloseHandle(hWritePipe);
        exit(1);
      }
      if (!::CloseHandle(pi.hProcess)) {
         HT_ERRORF("CloseHandle error: %s", winapi_strerror(::GetLastError()));
      }
      if (!::CloseHandle(pi.hThread)) {
         HT_ERRORF("CloseHandle error: %s", winapi_strerror(::GetLastError()));
      }     
      ::Sleep(5000);
    }

    ~ServerLauncher() {
      if( m_outfd != INVALID_HANDLE_VALUE ) {
        if (!::CloseHandle(m_outfd)) {
           HT_ERRORF("CloseHandle error: %s", winapi_strerror(::GetLastError()));
        }
      }      
      _close(m_write_fd);
      if (!::CloseHandle(m_hReadPipe)) {
           HT_ERRORF("CloseHandle error: %s", winapi_strerror(::GetLastError()));
        }
      kill(pi.dwProcessId);
    }

    int get_write_descriptor() { return m_write_fd; }

    pid_t get_pid() { return pi.dwProcessId; }
    
    static void kill(pid_t pid) {
      HANDLE handle = ::OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE, FALSE, pid);
      if (handle) {
        std::cerr << "Killing pid=" << pid
                  << std::endl << std::flush;

        if (!::TerminateProcess(handle, -1)) {
			DWORD dwLastError = ::GetLastError();
            if (::WaitForSingleObject(handle, 1000) != WAIT_OBJECT_0) {
              HT_ERRORF("TerminateProcess pid=%d error: %s", pid, winapi_strerror(dwLastError));
            }
        }
        else if (::WaitForSingleObject(handle, 5000) != WAIT_OBJECT_0) {
            HT_ERRORF("TerminateProcess pid=%d time out", pid);
        }
        if (!::CloseHandle(handle)) {
          HT_ERRORF("CloseHandle error: %s", winapi_strerror(::GetLastError()));
        }
      }
      else if( ::GetLastError() != ERROR_INVALID_PARAMETER ) {
        HT_ERRORF("OpenProcess pid=%d error: %s", pid, winapi_strerror(::GetLastError()));
      }
    }

  private:
    std::string m_path;
    HANDLE m_hReadPipe;
    PROCESS_INFORMATION pi;
    int m_write_fd;
    HANDLE m_outfd;
  };

#else

  class ServerLauncher {
  public:
    ServerLauncher(const char *path, char *const argv[],
                   const char *outfile = 0, bool append_output = false) {
      int fd[2];
      m_path = path;
      if (pipe(fd) < 0) {
        perror("pipe");
        exit(1);
      }
      if ((m_child_pid = fork()) == 0) {
        if (outfile) {
          int open_flags;
          int outfd = -1;

          if (append_output)
            open_flags = O_CREAT|O_APPEND|O_RDWR;
          else
            open_flags = O_CREAT|O_TRUNC|O_WRONLY,

          outfd = open(outfile, open_flags, 0644);
          if (outfd < 0) {
            perror("open");
            exit(1);
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
      poll(0,0,2000);
    }

    ~ServerLauncher() {
      std::cerr << "Killing '" << m_path << "' pid=" << m_child_pid
                << std::endl << std::flush;
      close(m_write_fd);
      if (kill(m_child_pid, 9) == -1)
        perror("kill");
    }

    int get_write_descriptor() { return m_write_fd; }

    pid_t get_pid() { return m_child_pid; }

  private:
    const char *m_path;
    pid_t m_child_pid;
    int   m_write_fd;
  };

#endif

}

#endif // HYPERTABLE_SERVERLAUNCHER_H
