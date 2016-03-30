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

/** @file
 * Declarations for EmitterStdout.
 * This file contains declarations for EmitterStdout, an class for emitting
 * key/value pairs to stdout.
 */

#ifndef HYPERTABLE_EMITTERSTDOUT_H
#define HYPERTABLE_EMITTERSTDOUT_H

#include "Hypertable/Lib/NameIdMapper.h"
#include "Hypertable/Lib/Schema.h"

#include "Hyperspace/Session.h"

#include "Emitter.h"

#include <unordered_map>

namespace Hypertable {

  class EmitterStdout : public Emitter {
  public:
    EmitterStdout();
    virtual ~EmitterStdout();
    virtual void add(TableIdentifier &table, Key &key,
                     const uint8_t *value, size_t value_len);
  private:
    String m_toplevel_dir;
    Hyperspace::SessionPtr m_hyperspace;
    NameIdMapperPtr m_name_id_mapper;

    class TableInfo {
    public:
      TableInfo() { }
      TableInfo(const String &n, Schema *s)
        : name(n), schema(s) { }
      String name;
      Schema *schema;
    };

    std::unordered_map<String, TableInfo> m_table_id_map;
  };

} // namespace Hypertable

#endif // HYPERTABLE_EMITTERSTDOUT_H
