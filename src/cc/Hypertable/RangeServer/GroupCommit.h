/** -*- c++ -*-
 * Copyright (C) 2010 Doug Judd (Hypertable, Inc.)
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

#ifndef HYPERSPACE_GROUPCOMMIT_H
#define HYPERSPACE_GROUPCOMMIT_H

#include "Common/HashMap.h"
#include "Common/Mutex.h"
#include "Common/FlyweightString.h"

#include "GroupCommitInterface.h"
#include "RangeServer.h"

namespace Hypertable {

  /**
   */
  class GroupCommit : public GroupCommitInterface {

  public:
    GroupCommit(RangeServer *range_server);
    virtual void add(EventPtr &event, SchemaPtr &schema, const TableIdentifier *table,
                     uint32_t count, StaticBuffer &buffer, uint32_t flags);
    virtual void trigger();

  private:
    Mutex         m_mutex;
    RangeServer  *m_range_server;
    uint32_t      m_commit_interval;
    int           m_counter;
    FlyweightString m_flyweight_strings;

#ifndef _WIN32

    typedef hash_map<TableIdentifier, TableUpdate *, __gnu_cxx::hash<TableIdentifier>, eqtid> TableUpdateMap;

#else

    struct TableIdentifierComparer {
      enum { bucket_size = 4 };
      size_t operator()(const TableIdentifier& tid) const {
        hash<const char*> H;
        return (size_t)H(tid.id) ^ tid.generation;
      }
      bool operator()(const TableIdentifier& a, const TableIdentifier& b) const { // a < b
        if (a.id == b.id)
          return false;
        if (!a.id)
          return true;
        if (!b.id)
          return false;
        int cmp;
        if ((cmp = strcmp(a.id, b.id)) != 0)
          return cmp < 0;
        return a.generation < b.generation;
      }
    };

    typedef hash_map<TableIdentifier, TableUpdate *, TableIdentifierComparer> TableUpdateMap;

#endif

    TableUpdateMap m_table_map;
  };
}

#endif // HYPERSPACE_GROUPCOMMIT_H

