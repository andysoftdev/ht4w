/*
 * Copyright (C) 2007-2016 Hypertable, Inc.
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 3
 * of the License.
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
 * Definitions for CommAddress.
 * This file contains method definitions for CommAddress, an abstraction class
 * for holding an arbitrary address type.
 */

#include "Common/Compat.h"

#include "ApplicationQueue.h"

using namespace Hypertable;

ApplicationQueue::ApplicationQueue(int worker_count, bool dynamic_threads)
	: joined(false), m_dynamic_threads(dynamic_threads) {
	m_state.threads_total = worker_count;
	Worker Worker(m_state);
	assert(worker_count > 0);
	for (int i = 0; i<worker_count; ++i) {
		m_thread_ids.push_back(m_threads.create_thread(Worker)->get_id());
	}
	//threads
}

ApplicationQueue::~ApplicationQueue() {
	if (!joined) {
		shutdown();
		join();
	}
}

bool ApplicationQueue::wait_for_idle(const std::chrono::time_point<std::chrono::steady_clock>& deadline,
	int reserve_threads) {
	std::unique_lock<std::mutex> lock(m_state.mutex);
	return m_state.quiesce_cond.wait_until(lock, deadline,
		[this, reserve_threads]() { return m_state.threads_available >= (m_state.threads_total - reserve_threads); });
}

void ApplicationQueue::join() {
	if (!joined) {
		m_threads.join_all();
		joined = true;
	}
}

void ApplicationQueue::start() {
	std::lock_guard<std::mutex> lock(m_state.mutex);
	m_state.paused = false;
	m_state.cond.notify_all();
}

void ApplicationQueue::stop() {
	std::lock_guard<std::mutex> lock(m_state.mutex);
	m_state.paused = true;
}

void ApplicationQueue::add(ApplicationHandler *app_handler) {
	GroupStateMap::iterator uiter;
	uint64_t group_id = app_handler->get_group_id();
	RequestRec *rec = new RequestRec(app_handler);
	rec->group_state = 0;

	HT_ASSERT(app_handler);

	if (group_id != 0) {
		std::lock_guard<std::mutex> ulock(m_state.mutex);
		if ((uiter = m_state.group_state_map.find(group_id))
			!= m_state.group_state_map.end()) {
			rec->group_state = (*uiter).second;
			rec->group_state->outstanding++;
		}
		else {
			rec->group_state = new GroupState();
			rec->group_state->group_id = group_id;
			m_state.group_state_map[group_id] = rec->group_state;
		}
	}

	{
		std::lock_guard<std::mutex> lock(m_state.mutex);
		if (app_handler->is_urgent()) {
			m_state.urgent_queue.push_back(rec);
			if (m_dynamic_threads && m_state.threads_available == 0) {
				Worker worker(m_state, true);
				Thread t(worker);
			}
		}
		else
			m_state.queue.push_back(rec);
		m_state.cond.notify_one();
	}
}
