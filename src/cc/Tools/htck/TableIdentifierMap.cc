/*
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

#include "Common/Compat.h"

#include "TableIdentifierMap.h"

using namespace Hypertable;


void TableIdentifierMap::update(const String &location, const TableIdentifier &table) {
  struct key key;
  key.location = location.c_str();
  key.tid = table.id;
  TidMapT::iterator iter = m_map.find(key);
  if (iter == m_map.end()) {
    key.location = m_flyweight.get(key.location);
    key.tid = m_flyweight.get(key.tid);
    TableIdentifier *t = new TableIdentifier();
    t->id = key.tid;
    t->generation = table.generation;
    m_map[key] = t;
  }
  else if (iter->second->generation < table.generation)
    iter->second->generation = table.generation;
}


const TableIdentifier *TableIdentifierMap::get(const char *location, const char *tid) {
  struct key key;
  key.location = location;
  key.tid = tid;
  TidMapT::iterator iter = m_map.find(key);
  if (iter != m_map.end())
    return iter->second;
  return 0;
}

const TableIdentifier *TableIdentifierMap::get_latest(const char *tid) {
  const TableIdentifier *rtid = 0;
  uint32_t latest_generation = 0;
  for (TidMapT::iterator iter = m_map.begin(); iter != m_map.end(); ++iter) {
    if (!strcmp(tid, (*iter).first.tid) &&
	(rtid == 0 || (*iter).second->generation > latest_generation)) {
      latest_generation = (*iter).second->generation;
      rtid = (*iter).second;
    }
  }
  return rtid;
}
