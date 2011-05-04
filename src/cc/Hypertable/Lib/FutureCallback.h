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

#ifndef HYPERTABLE_FUTURECALLBACK_H
#define HYPERTABLE_FUTURECALLBACK_H

#include <boost/thread/condition.hpp>
#include <set>

#include "ResultCallback.h"

namespace Hypertable {

  class TableScannerAsync;
  class ScanSpec;

  class FutureCallback : public ResultCallback {
  public:

    /**
     * FutureCallback objects are used to access results from asynchronous scanners/mutators
     * @param capacity number of Result objects to enqueue
     */
    FutureCallback() : m_cancelled(false) { }
    virtual ~FutureCallback();

    /**
     * Cancels outstanding scanners/mutators. Callers responsibility to make sure that
     * this method gets called before async scanner/mutator destruction when the application
     * abruptly stops processing async results before all operations are complete
     */
    void cancel();

    bool is_cancelled() {
      ScopedRecLock lock(m_outstanding_mutex);
      return _is_cancelled();
    }

    void register_scanner(TableScannerAsync *scanner);
    void deregister_scanner(TableScannerAsync *scanner);

    const ScanSpec &get_scan_spec(TableScannerAsync *scanner);

  protected:

    virtual void scan_error(TableScannerAsync *scanner, int error, const String &error_msg, bool eos);
    virtual void update_error(TableMutator *mutator, int error, const String &error_msg);

  private:
    friend class TableScannerAsync;
    friend class TableMutator;

    // without locks
    bool _is_cancelled() const { return m_cancelled; }

    bool m_cancelled;
    typedef std::set<TableScannerAsync*> ScannerSet;
    ScannerSet m_scanner_set;
    ScannerSet m_scanners_owned;
  };
  typedef intrusive_ptr<FutureCallback> FutureCallbackPtr;
}

#endif // HYPERTABLE_FUTURECALLBACK_H
