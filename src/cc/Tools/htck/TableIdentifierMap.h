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

#ifndef Tools_htck_TableIdentifierMap_h
#define Tools_htck_TableIdentifierMap_h

#include <Hypertable/Lib/TableIdentifier.h>

#include <Common/FlyweightString.h>
#include <Common/String.h>

#include <map>

namespace Hypertable {

  class TableIdentifierMap {
  public:
    void update(const String &location, const TableIdentifier &table);
    const TableIdentifier *get(const char *location, const char *tid);
    const TableIdentifier *get_latest(const char *tid);

  private:
    struct key {
      const char *location;
      const char *tid;
    };
    struct ltkey {
      bool operator()(const key &k1, const key &k2) const {
	int cmpval = strcmp(k1.location, k2.location);
	if (cmpval)
	  return cmpval < 0;
	return strcmp(k1.tid, k2.tid) < 0;
      }
    };
    typedef std::map<key, TableIdentifier *, ltkey> TidMapT;

    TidMapT m_map;
    FlyweightString m_flyweight;
  };

}

#endif // Tools_htck_TableIdentifierMap_h
