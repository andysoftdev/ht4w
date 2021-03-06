/** -*- c++ -*-
 * Copyright (C) 2007-2012 Hypertable, Inc.
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 3 of the
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

#include "FutureCallback.h"
#include "TableScannerAsync.h"
#include "TableMutatorAsync.h"

using namespace Hypertable;

FutureCallback::~FutureCallback() {
  cancel();
  wait_for_completion();
}

void FutureCallback::cancel() {
  std::lock_guard<std::mutex> lock(m_outstanding_mutex);
  m_cancelled = true;
  for (TableScannerAsync *scanner : m_scanner_set)
    scanner->cancel();
  m_outstanding_cond.notify_all();
}

void FutureCallback::register_scanner(TableScannerAsync *scanner) {
  std::lock_guard<std::mutex> lock(m_outstanding_mutex);
  m_scanner_set.insert(scanner);
  m_cancelled = false;
}

void FutureCallback::deregister_scanner(TableScannerAsync *scanner) {
  std::lock_guard<std::mutex> lock(m_outstanding_mutex);
  ScannerSet::iterator it = m_scanner_set.find(scanner);
  HT_ASSERT(it != m_scanner_set.end());
  m_scanner_set.erase(it);
}

void FutureCallback::register_mutator(TableMutatorAsync *mutator) {
  std::lock_guard<std::mutex> lock(m_outstanding_mutex);
  m_mutator_set.insert(mutator);
  m_cancelled = false;
}

void FutureCallback::deregister_mutator(TableMutatorAsync *mutator) {
  std::lock_guard<std::mutex> lock(m_outstanding_mutex);
  MutatorSet::iterator it = m_mutator_set.find(mutator);
  HT_ASSERT(it != m_mutator_set.end());
  m_mutator_set.erase(it);
}

void FutureCallback::wait_for_completion() {
  {
    std::lock_guard<std::mutex> lock(m_outstanding_mutex);
    for (TableMutatorAsync *mutator : m_mutator_set)
      mutator->flush();
  }
  ResultCallback::wait_for_completion();
}
