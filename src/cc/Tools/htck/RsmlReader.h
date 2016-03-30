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

#ifndef Tools_htck_RsmlReader_h
#define Tools_htck_RsmlReader_h

#include <Tools/htck/RangeInfo.h>
#include <Tools/htck/TableIdentifierMap.h>

#include <Hypertable/RangeServer/MetaLogEntityRange.h>

#include <Common/String.h>
#include <Common/Filesystem.h>
#include <Common/FlyweightString.h>

#include <cstdio>
#include <map>
#include <vector>

namespace Hypertable {

  typedef std::vector<MetaLog::EntityPtr> EntityVectorT;
  typedef std::map<String, EntityVectorT *> LocationEntitiesMapT;

  class RsmlReader {
  public:
    RsmlReader(FilesystemPtr &fs, StringSet &valid_locations, StringSet &valid_tids,
               TableIdentifierMap &tidmap, RangeInfoSet &riset, 
	       LocationEntitiesMapT &entities_map, size_t *entry_countp);

    void swap_dangling_riset(RangeInfoSet &riset) { riset.swap(m_dangling_riset); }

  private:
    RangeInfo *create_rinfo_from_entity(const String &location, MetaLogEntityRangePtr &range_entity);

    RangeInfoSet m_dangling_riset;
    FlyweightString m_flyweight;
  };

}


#endif // Tools_htck_RsmlReader_h
