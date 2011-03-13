/** -*- c++ -*-
 * Copyright (C) 2009 Doug Judd (Zvents, Inc.)
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

#ifndef HYPERTABLE_MEMORYTRACKER_H
#define HYPERTABLE_MEMORYTRACKER_H

#include <boost/thread/mutex.hpp>

#include "FileBlockCache.h"

namespace Hypertable {

  class MemoryTracker {
  public:
    enum {
      query_cache = 0,
      cell_cache,
      cell_store,
      block_cache,
      consumer_count
    };

    MemoryTracker(FileBlockCache *block_cache) 
      : m_block_cache(block_cache) { memset(m_memory_used_by, 0, sizeof(m_memory_used_by)); }

    void add(int consumer, int64_t amount) {
      ScopedLock lock(m_mutex);
      m_memory_used_by[consumer] += amount;
    }

    void subtract(int consumer, int64_t amount) {
      ScopedLock lock(m_mutex);
      m_memory_used_by[consumer] -= amount;
    }

    int64_t balance() {
      int64_t memory_used_by[consumer_count];
      memory_used(memory_used_by);
      int64_t memory_used = 0;
      for (int i = 0; i < consumer_count; ++i)
        memory_used += memory_used_by[i];
      return memory_used;
    }

    int64_t memory_used(int consumer) {
      HT_ASSERT(consumer >= query_cache && consumer <= block_cache);
      ScopedLock lock(m_mutex);
      return consumer != block_cache ? m_memory_used_by[consumer] : m_block_cache->memory_used();
    }

    template <size_t size> inline
    void memory_used(int64_t (&memory_used_by)[size]) {
      ScopedLock lock(m_mutex);
      memcpy(memory_used_by, m_memory_used_by, sizeof(int64_t) * std::min(size, (size_t)block_cache));
      if (size > block_cache)
        memory_used_by[block_cache] = m_block_cache->memory_used();
    }

  private:
    Mutex m_mutex;
    int64_t m_memory_used_by[consumer_count-1]; // block_cache tracked by m_block_cache
    FileBlockCache *m_block_cache;
  };

}

#endif // HYPERTABLE_MEMORYTRACKER_H
