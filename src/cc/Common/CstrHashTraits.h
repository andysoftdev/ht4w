/**
 * Copyright (C) 2008 Luke Lu (Zvents, Inc.)
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
 * along with Hypertable. If not, see <http://www.gnu.org/licenses/>
 */

#ifndef HYPERTABLE_CSTR_HASH_TRAITS_H
#define HYPERTABLE_CSTR_HASH_TRAITS_H

#include "PageArena.h"
#include "TclHash.h"

namespace Hypertable {

/**
 * Traits for CstrHashMap/Set
 */
template <class HashT = TclHash2>
struct CstrHashTraits {
  typedef CharArena key_allocator;

 #ifndef _WIN32

  struct hasher {
    HashT hasher;

    size_t operator()(const char *s) const { return hasher(s); }
  };

  struct key_equal {
    bool
    operator()(const char *a, const char *b) const {
      return std::strcmp(a, b) == 0;
    }
  };

#else

  struct hash_compare {
    enum {   // parameters for hash table
        bucket_size = 4,    // 0 < bucket_size
        min_buckets = 8};   // min_buckets = 2 ^^ N, 0 < N
    HashT hash_fun;
    size_t operator()(const char *s) const { return hash_fun(s); }
    bool operator()(const char *a, const char *b) const {
      return std::strcmp(a, b) < 0;
    }
  };

#endif

  struct key_equal {
    bool
    operator()(const char *a, const char *b) const {
      return std::strcmp(a, b) == 0;
    }
  };
};

inline size_t
hash_case_cstr(const char *s) {
  register size_t ret = 0;

  for (; *s; ++s)
    ret += (ret << 3) + tolower((unsigned)*s);

  return ret;
}

struct CstrCaseHashTraits {
  typedef CharArena key_allocator;

  struct hasher {
    size_t
    operator()(const char *s) const { return hash_case_cstr(s); }
  };

  struct key_equal {
    bool
    operator()(const char *a, const char *b) const {
      for (; tolower((unsigned)*a) == tolower((unsigned)*b); ++a, ++b)
        if (!*a)
          return true;

      return false;
    }
  };
};

} // namespace Hypertable

#endif // !HYPERTABLE_CSTR_HASH_TRAITS_H
