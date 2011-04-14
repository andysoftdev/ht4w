/** -*- c++ -*-
 * Copyright (C) 2011 Hypertable, Inc.
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of the
 * License, or any later version.
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

#include "Common/Compat.h"
#include <boost/thread/xtime.hpp>

#include "Future.h"
#include "TableScannerAsync.h"

using namespace Hypertable;

Future::~Future() {
  cancel();
  wait_for_completion();
  foreach (TableScannerAsync *scanner, m_scanners_owned)
    intrusive_ptr_release(scanner);
}

bool Future::get(ResultPtr &result) {
  ScopedRecLock lock(m_outstanding_mutex);

  // wait till we have results to serve
  while(_is_empty() && !is_done() && !_is_cancelled()) {
    m_outstanding_cond.wait(lock);
  }

  if (_is_cancelled())
    return false;
  if (_is_empty() && is_done())
    return false;
  result = m_queue.front();
  // wake a thread blocked on queue space
  m_queue.pop_front();
  m_outstanding_cond.notify_one();
  return true;
}

bool Future::get(ResultPtr &result, uint32_t timeout_ms, bool &timed_out) {
  ScopedRecLock lock(m_outstanding_mutex);

  timed_out = false;

  boost::xtime wait_time;
  boost::xtime_get(&wait_time, boost::TIME_UTC);
  xtime_add_millis(wait_time, timeout_ms);

  // wait till we have results to serve
  while(_is_empty() && !is_done() && !_is_cancelled()) {
    timed_out = m_outstanding_cond.timed_wait(lock, wait_time);
    if (timed_out)
      return is_done();
  }
  if (_is_cancelled())
    return false;
  if (_is_empty() && is_done())
    return false;
  result = m_queue.front();
  // wake a thread blocked on queue space
  m_queue.pop_front();
  m_outstanding_cond.notify_one();
  return true;
}

void Future::scan_ok(TableScannerAsync *scanner, ScanCellsPtr &cells) {
  ResultPtr result = new Result(scanner, cells);
  enqueue(result);
}

void Future::enqueue(ResultPtr &result) {
  ScopedRecLock lock(m_outstanding_mutex);
  while (_is_full() && !_is_cancelled()) {
    m_outstanding_cond.wait(lock);
  }
  if (!_is_cancelled()) {
    m_queue.push_back(result);
  }
  m_outstanding_cond.notify_one();
}

void Future::scan_error(TableScannerAsync *scanner, int error, const String &error_msg,
                        bool eos) {
  ResultPtr result = new Result(scanner, error, error_msg);
  enqueue(result);
}

void Future::update_ok(TableMutator *mutator, FailedMutations &failed_mutations) {
  ResultPtr result = new Result(mutator, failed_mutations);
  enqueue(result);
}

void Future::update_error(TableMutator *mutator, int error, const String &error_msg) {
  ResultPtr result = new Result(mutator, error, error_msg);
  enqueue(result);
}

void Future::cancel() {
  ScopedRecLock lock(m_outstanding_mutex);
  m_cancelled = true;
  foreach (TableScannerAsync *scanner, m_scanner_set)
    scanner->cancel();
  m_outstanding_cond.notify_all();
}

void Future::register_scanner(TableScannerAsync *scanner) {
  ScopedRecLock lock(m_outstanding_mutex);
  // XXX: TODO DON"T ASSERT if m_cancelled == true throw an exception
  HT_ASSERT(m_scanner_set.insert(scanner).second && !m_cancelled);
  if (m_scanners_owned.insert(scanner).second)
    intrusive_ptr_add_ref(scanner);
}

void Future::deregister_scanner(TableScannerAsync *scanner) {
  ScopedRecLock lock(m_outstanding_mutex);
  ScannerSet::iterator it = m_scanner_set.find(scanner);
  HT_ASSERT(it != m_scanner_set.end());
  m_scanner_set.erase(it);
}
