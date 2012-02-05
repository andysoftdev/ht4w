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

#ifndef HYPERTABLE_MAINTENANCEFLAG_H
#define HYPERTABLE_MAINTENANCEFLAG_H

#include "Common/HashMap.h"

namespace Hypertable {
  namespace MaintenanceFlag {

    enum {
      SPLIT                     = 0x0100,
      COMPACT                   = 0x0200,
      COMPACT_MINOR             = 0x0201,
      COMPACT_MAJOR             = 0x0202,
      COMPACT_MERGING           = 0x0204,
      COMPACT_GC                = 0x0208,
      COMPACT_MOVE              = 0x0210,
      MEMORY_PURGE              = 0x0400,
      MEMORY_PURGE_SHADOW_CACHE = 0x0401,
      MEMORY_PURGE_CELLSTORE    = 0x0402,
      RELINQUISH                = 0x0800
    };

    inline bool split(int flags) {
      return (flags & SPLIT) == SPLIT;
    }

    inline bool compaction(int flags) {
      return (flags & COMPACT) == COMPACT;
    }

    inline bool minor_compaction(int flags) {
      return (flags & COMPACT_MINOR) == COMPACT_MINOR;
    }

    inline bool merging_compaction(int flags) {
      return (flags & COMPACT_MERGING) == COMPACT_MERGING;
    }

    inline bool major_compaction(int flags) {
      return (flags & COMPACT_MAJOR) == COMPACT_MAJOR;
    }

    inline bool gc_compaction(int flags) {
      return (flags & COMPACT_GC) == COMPACT_GC;
    }

    inline bool move_compaction(int flags) {
      return (flags & COMPACT_MOVE) == COMPACT_MOVE;
    }

    inline bool purge_shadow_cache(int flags) {
      return (flags & MEMORY_PURGE_SHADOW_CACHE) == MEMORY_PURGE_SHADOW_CACHE;
    }

    inline bool purge_cellstore(int flags) {
      return (flags & MEMORY_PURGE_CELLSTORE) == MEMORY_PURGE_CELLSTORE;
    }

#ifndef _WIN32

    class Hash {
    public:
      size_t operator () (const void *obj) const {
	return (size_t)obj;
      }
    };

    struct Equal {
      bool operator()(const void *obj1, const void *obj2) const {
	return obj1 == obj2;
      }
    };

class Map : public hash_map<const void *, int, Hash, Equal> {

#else

    class HashCompare {
    public:
      enum {   // parameters for hash table
        bucket_size = 4,    // 0 < bucket_size
        min_buckets = 8};   // min_buckets = 2 ^^ N, 0 < N
      size_t operator () (const void *obj) const {
        return (size_t)obj;
      }
      bool operator()(const void *obj1, const void *obj2) const {
        return obj1 < obj2;
      }
    };

    class Map : public hash_map<const void *, int, HashCompare> {

#endif

    public:
      int flags(const void *key) {
	iterator iter = this->find(key);
	if (iter != this->end())
	  return (*iter).second;
	return 0;
      }
      bool compaction(const void *key) {
	iterator iter = this->find(key);
	if (iter != this->end())
	  return ((*iter).second & COMPACT) == COMPACT;
	return false;
      }
      bool minor_compaction(const void *key) {
	iterator iter = this->find(key);
	if (iter != this->end())
	  return ((*iter).second & COMPACT_MINOR) == COMPACT_MINOR;
	return false;
      }
      bool memory_purge(const void *key) {
	iterator iter = this->find(key);
	if (iter != this->end())
	  return ((*iter).second & MEMORY_PURGE) == MEMORY_PURGE;
	return false;
      }
    };
  }
}

#endif // HYPERTABLE_MAINTENANCEFLAG_H
