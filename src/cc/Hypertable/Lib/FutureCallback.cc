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

#include "FutureCallback.h"
#include "TableScannerAsync.h"
#include "TableMutatorAsync.h"

using namespace Hypertable;

FutureCallback::~FutureCallback() {
  cancel();
  wait_for_completion();
  foreach (TableScannerAsync *scanner, m_scanners_owned)
    intrusive_ptr_release(scanner);
  foreach (TableMutatorAsync *mutator, m_mutators_owned)
    intrusive_ptr_release(mutator);
}

void FutureCallback::cancel() {
  ScopedRecLock lock(m_outstanding_mutex);
  m_cancelled = true;
  foreach (TableScannerAsync *scanner, m_scanner_set)
    scanner->cancel();
  m_outstanding_cond.notify_all();
}

void FutureCallback::register_scanner(TableScannerAsync *scanner) {
  ScopedRecLock lock(m_outstanding_mutex);
  m_scanner_set.insert(scanner);
  if (m_scanners_owned.insert(scanner).second)
    intrusive_ptr_add_ref(scanner);
}

void FutureCallback::deregister_scanner(TableScannerAsync *scanner) {
  ScopedRecLock lock(m_outstanding_mutex);
  ScannerSet::iterator it = m_scanner_set.find(scanner);
  HT_ASSERT(it != m_scanner_set.end());
  m_scanner_set.erase(it);
}

void FutureCallback::register_mutator(TableMutatorAsync *mutator) {
  ScopedRecLock lock(m_outstanding_mutex);
  m_mutator_set.insert(mutator);
  if (m_mutators_owned.insert(mutator).second)
    intrusive_ptr_add_ref(mutator);
}

void FutureCallback::deregister_mutator(TableMutatorAsync *mutator) {
  ScopedRecLock lock(m_outstanding_mutex);
  MutatorSet::iterator it = m_mutator_set.find(mutator);
  HT_ASSERT(it != m_mutator_set.end());
  m_mutator_set.erase(it);
}

const ScanSpec &FutureCallback::get_scan_spec(TableScannerAsync *scanner) {
  return scanner->get_scan_spec();
}

void FutureCallback::scan_error(TableScannerAsync *scanner, int error, const String &error_msg, bool eos) {
  cancel();
}

void FutureCallback::update_error(TableMutatorAsync *mutator, int error, FailedMutations &failedMutations) {
  cancel();
}
