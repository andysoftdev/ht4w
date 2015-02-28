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
 * Logging routines and macros.
 * The LogWriter provides facilities to write debug, log, error- and other
 * messages to stdout. The Logging namespaces provides std::ostream-
 * and printf-like macros and convenience functions.
 */

#include "Common/Compat.h"

#include <iostream>
#include <stdio.h>
#include <stdarg.h>

#include "String.h"
#include "Logger.h"
#include "Mutex.h"

namespace Hypertable { namespace Logger {

static String logger_name;
static LogWriter *logger_obj = 0;
static Mutex mutex;

void initialize(const String &name) {
  logger_name = name;
}

LogWriter *get() {
  if (!logger_obj)
    logger_obj = new LogWriter(logger_name);
  return logger_obj;
}

void LogWriter::log_string(int priority, const char *message) {
  static const char *priority_name[] = {
    "FATAL",
    "ALERT",
    "CRIT",
    "ERROR",
    "WARN",
    "", // NOTICE
    "INFO",
    "DEBUG",
    "NOTSET"
  };

  ScopedLock lock(mutex);
  if (m_test_mode) {
    fprintf(m_file, "%s %s : %s\n", priority_name[priority], m_name.c_str(),
            message);
  }
  else {
    time_t t = ::time(0);

#ifdef _WIN32

    struct tm lt;
    localtime_s(&lt, &t);
    char dateTime[64];
    strftime(dateTime, sizeof(dateTime), "%Y-%m-%d %H:%M:%S", &lt);

    String entry = format("%s %6s %s", 
                          dateTime,
                          priority_name[priority],
                          message);
    fprintf(m_file, "%s\n", entry.c_str());

#else

    fprintf(m_file, "%u %s %s : %s\n", (unsigned)t, priority_name[priority],
            m_name.c_str(), message);

#endif

    for each( const LogSink* ls in logSinks )
      ls->log( priority, message, entry );

  }

#ifndef _WIN32

  flush();

#else

  _fflush_nolock(m_file);

#endif
}

void LogWriter::log_varargs(int priority, const char *format, va_list ap) {
  char buffer[1024 * 16];
  vsnprintf(buffer, sizeof(buffer), format, ap);
  log_string(priority, buffer);
}

void LogWriter::debug(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  log_varargs(Priority::DEBUG, format, ap);
  va_end(ap);
}

void LogWriter::log(int priority, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  log_varargs(priority, format, ap);
  va_end(ap);
}

void LogWriter::set_file(FILE *file) {
  ScopedLock lock(mutex);
  m_file = file;
}

void LogWriter::add_sink(const LogSink* ls) {
  ScopedLock lock(mutex);
  if (ls)
    logSinks.insert(ls);
}

void LogWriter::remove_sink(const LogSink* ls) {
  ScopedLock lock(mutex);
  if (ls)
    logSinks.erase(ls);
}

void LogWriter::set_test_mode(int fd) {
  if (fd != -1) {

#ifndef _WIN32

    m_file = fdopen(fd, "wt");

#else

    m_file = fdopen(fd, "w+tc");

#endif

  }
  m_show_line_numbers = false;
  m_test_mode = true;
}

void LogWriter::flush() {
  ScopedLock lock(mutex);

#ifndef _WIN32

  fflush(m_file);

#else

  _fflush_nolock(m_file);

#endif
}

void LogWriter::close() {
  ScopedLock lock(mutex);

  if (m_file != stdout) {
    fclose(m_file);
    m_file = stdout;
  }
}

}} // namespace Hypertable::
