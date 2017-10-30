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

#include <Common/Compat.h>

#include "Event.h"

#define HT_BDBTXN_EVT_BEGIN(parent_txn) \
  do { \
    BDbTxn txn; \
    ms_bdb_fs->start_transaction(txn); \
    try

#define HT_BDBTXN_EVT_END_CB(_cb_) \
    catch (Exception &e) { \
      if (e.code() != Error::HYPERSPACE_BERKELEYDB_DEADLOCK) { \
        if (e.code() == Error::HYPERSPACE_BERKELEYDB_ERROR) \
          HT_ERROR_OUT << e << HT_END; \
        else \
          HT_WARNF("%s - %s", Error::get_text(e.code()), e.what()); \
        txn.abort(); \
        _cb_->error(e.code(), e.what()); \
        return; \
      } \
      HT_WARN_OUT << "Berkeley DB deadlock encountered in txn "<< txn << HT_END; \
      txn.abort(); \
      std::this_thread::sleep_for(Random::duration_millis(3000)); \
      continue; \
    } \
    break; \
  } while (true)

#define HT_BDBTXN_EVT_END(...) \
    catch (Exception &e) { \
      if (e.code() != Error::HYPERSPACE_BERKELEYDB_DEADLOCK) { \
        if (e.code() == Error::HYPERSPACE_BERKELEYDB_ERROR) \
          HT_ERROR_OUT << e << HT_END; \
        else \
          HT_WARNF("%s - %s", Error::get_text(e.code()), e.what()); \
        txn.abort(); \
        return __VA_ARGS__; \
      } \
      HT_WARN_OUT << "Berkeley DB deadlock encountered in txn "<< txn << HT_END; \
      txn.abort(); \
      std::this_thread::sleep_for(Random::duration_millis(3000)); \
      continue; \
    } \
    break; \
  } while (true)

namespace Hyperspace {

BerkeleyDbFilesystem *Event::ms_bdb_fs=0;

void Event::increment_notification_count() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_notification_count++;
}

void Event::decrement_notification_count() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_notification_count--;
	if (m_notification_count == 0) {
		// all notifications received, so delete event from BDB
		HT_BDBTXN_EVT_BEGIN() {
			ms_bdb_fs->delete_event(txn, m_id);
			txn.commit();
		}
		HT_BDBTXN_EVT_END(BOOST_PP_EMPTY());
		m_cond.notify_all();
	}
}

}
