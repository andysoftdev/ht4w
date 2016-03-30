/* -*- c++ -*-
 * Copyright (C) 2007-2016 Hypertable, Inc.
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

#ifndef Tools_htck_RangeInfo_h
#define Tools_htck_RangeInfo_h

#include <cstdio>
#include <iostream>

#include <set>
#include <vector>

#include "Hypertable/RangeServer/MetaLogEntityRange.h"

namespace Hypertable {

  class RangeInfo {
  public:
    RangeInfo() : got_start_row(false), got_location(false), got_files(false),
		  start_row(0), start_row_len(0), end_row(0),
		  end_row_len(0), table_id(0), location(0) { }
    ~RangeInfo() { delete [] start_row; delete [] end_row; }
    void set_start_row(const char *buf, size_t len) {
      start_row = new char [ len + 1 ];
      memcpy(start_row, buf, len);
      start_row[len] = 0;
      start_row_len = len;
      got_start_row = true;
    }
    void set_end_row(const char *buf, size_t len) {
      end_row = new char [ len + 1 ];
      memcpy(end_row, buf, len);
      end_row[len] = 0;
      end_row_len = len;
    }
    void println() {
      printf("%s[%s..%s] %s\n", table_id ? table_id : "UNKNOWN",
             start_row ? start_row : "UNKNOWN",
             end_row ? end_row : "UNKNOWN",
             location ? location : "UNKNOWN");
    }
    String to_str() {
      return format("%s[%s..%s] at %s", table_id ? table_id : "UNKNOWN",
		    start_row ? start_row : "UNKNOWN",
		    end_row ? end_row : "UNKNOWN",
		    location ? location : "UNKNOWN");
    }
    bool got_start_row;
    bool got_location;
    bool got_files;
    char *start_row;
    size_t start_row_len;
    char *end_row;
    size_t end_row_len;
    const char *table_id;
    const char *location;
    std::vector<String> files;
    MetaLogEntityRangePtr entity;
  };

  inline std::ostream &operator<<(std::ostream &os, const RangeInfo &rinfo) {
    os << rinfo.table_id << "[" << rinfo.start_row << ".." << rinfo.end_row << "] " << rinfo.location;
    return os;
  }

  class lt_rinfo_base {
  protected:
    int table_id_cmp(const RangeInfo* ri1, const RangeInfo* ri2) const {
      if (ri1->table_id && ri2->table_id)
	return strcmp(ri1->table_id, ri2->table_id);
      if (ri2->table_id)
	return -1;
      if (ri1->table_id)
	return 1;
      return 0;
    }
    int start_row_cmp(const RangeInfo* ri1, const RangeInfo* ri2) const {
      int cmpval;
      if (ri1->start_row_len == 0 || ri2->start_row_len == 0)
	return (int)ri1->start_row_len - (int)ri2->start_row_len;
      if (ri1->start_row_len != ri2->start_row_len) {
        cmpval = memcmp(ri1->start_row, ri2->start_row,
                        std::min(ri1->start_row_len, ri2->start_row_len));
	return cmpval ? cmpval : (ri1->start_row_len - ri2->start_row_len);
      }
      return memcmp(ri1->start_row, ri2->start_row, ri1->start_row_len);
    }
    int end_row_cmp(const RangeInfo* ri1, const RangeInfo* ri2) const {
      int cmpval;
      if (ri1->end_row_len == 0 || ri2->end_row_len == 0)
	return (int)ri1->end_row_len - (int)ri2->end_row_len;
      if (ri1->end_row_len != ri2->end_row_len) {
        cmpval = memcmp(ri1->end_row, ri2->end_row,
                        std::min(ri1->end_row_len, ri2->end_row_len));
	return cmpval ? cmpval : (ri1->end_row_len - ri2->end_row_len);
      }
      return memcmp(ri1->end_row, ri2->end_row, ri1->end_row_len);
    }
    int location_cmp(const RangeInfo* ri1, const RangeInfo* ri2) const {
      if (ri1->location && ri2->location)
	return strcmp(ri1->location, ri2->location);
      if (ri2->location)
	return -1;
      if (ri1->location)
	return 1;
      return 0;
    }

  };

  struct lt_rinfo_endrow : public lt_rinfo_base {
    bool operator()(const RangeInfo* ri1, const RangeInfo* ri2) const {
      int cmpval;
      // compare table ID
      if ((cmpval = table_id_cmp(ri1, ri2)) != 0)
        return cmpval < 0;

      // compare end row
      if ((cmpval = end_row_cmp(ri1, ri2)) != 0)
	return cmpval < 0;

      return false;
    }
  };

  struct lt_rinfo_range : public lt_rinfo_base {
    bool operator()(const RangeInfo* ri1, const RangeInfo* ri2) const {
      int cmpval;
      // compare table ID
      if ((cmpval = table_id_cmp(ri1, ri2)) != 0)
        return cmpval < 0;

      // compare end row
      if ((cmpval = end_row_cmp(ri1, ri2)) != 0)
	return cmpval < 0;

      // compare start row
      if ((cmpval = start_row_cmp(ri1, ri2)) != 0)
	return cmpval < 0;

      return false;
    }
  };

  struct lt_rinfo : public lt_rinfo_base {
    bool operator()(const RangeInfo* ri1, const RangeInfo* ri2) const {
      int cmpval;
      // compare table ID
      if ((cmpval = table_id_cmp(ri1, ri2)) != 0)
        return cmpval < 0;

      // compare end row
      if ((cmpval = end_row_cmp(ri1, ri2)) != 0)
	return cmpval < 0;

      // compare start row
      if ((cmpval = start_row_cmp(ri1, ri2)) != 0)
	return cmpval < 0;

      // compare location
      if ((cmpval = location_cmp(ri1, ri2)) != 0)
        return cmpval < 0;

      return false;
    }
  };

  typedef std::set<RangeInfo *, lt_rinfo_endrow> RangeInfoSetEndRow;
  typedef std::set<RangeInfo *, lt_rinfo_range> RangeInfoSetRange;
  typedef std::set<RangeInfo *, lt_rinfo> RangeInfoSet;
}

#endif // Tools_htck_RangeInfo_h
