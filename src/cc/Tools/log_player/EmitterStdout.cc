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

/** @file
 * Definitions for EmitterStdout.
 * This file contains definitions for EmitterStdout, an class for emitting
 * key/value pairs to stdout.
 */

#include "Common/Compat.h"
#include "Common/Config.h"

#include "AsyncComm/Comm.h"

#include "Hypertable/Lib/LoadDataEscape.h"

#include "EmitterStdout.h"

#include <iostream>
#include <unordered_map>

using namespace Hypertable;
using namespace Config;
using namespace std;

EmitterStdout::EmitterStdout() {
  Comm *comm = Comm::instance();
  m_toplevel_dir = String("/") + properties->get_str("Hypertable.Directory");

  m_hyperspace = make_shared<Hyperspace::Session>(comm, properties);

  m_name_id_mapper = make_shared<NameIdMapper>(m_hyperspace, m_toplevel_dir);
}

EmitterStdout::~EmitterStdout() {
  cout << flush;
}

void EmitterStdout::add(TableIdentifier &table, Key &key,
                        const uint8_t *value, size_t value_len) {
  LoadDataEscape row_escaper;
  LoadDataEscape escaper;
  const char *unescaped_buf, *row_unescaped_buf;
  size_t unescaped_len, row_unescaped_len;
  String name;
  unordered_map<String, TableInfo>::iterator table_id_map_iter;
  Schema *schema;

  if ((table_id_map_iter = m_table_id_map.find(table.id)) == m_table_id_map.end()) {

    if (!m_name_id_mapper->id_to_name(table.id, name))
      return;

    try {
      String tablefile = m_toplevel_dir + "/tables/" + table.id;
      DynamicBuffer value_buf(0);
      m_hyperspace->attr_get(tablefile, "schema", value_buf);
      schema = Schema::new_instance((const char *)value_buf.base);
    }
    catch (Exception &e) {
      return;
    }
    m_table_id_map[table.id] = TableInfo(name, schema);
    table_id_map_iter = m_table_id_map.find(table.id);
  }

  cout << table_id_map_iter->second.name << "\t" << key.timestamp << "\t";

  row_escaper.escape(key.row, key.row_len,
                     &row_unescaped_buf, &row_unescaped_len);
  cout << String(row_unescaped_buf, row_unescaped_len) << "\t";

  ColumnFamilySpec *cf = table_id_map_iter->second.schema->get_column_family(key.column_family_code);

  cout << cf->get_name();
  if (*key.column_qualifier)
    cout << ":" << key.column_qualifier;

  escaper.escape((const char *)value, value_len,
                 &unescaped_buf, &unescaped_len);

  cout << "\t" << String(unescaped_buf, unescaped_len);

  if (key.flag == FLAG_INSERT)
    cout << "\n";
  else if (key.flag == FLAG_DELETE_ROW)
    cout << "\tDELETE_ROW\n";
  else if (key.flag == FLAG_DELETE_COLUMN_FAMILY)
    cout << "\tDELETE_COLUMN_FAMILY\n";
  else if (key.flag == FLAG_DELETE_CELL)
    cout << "\tDELETE_CELL\n";
  else if (key.flag == FLAG_DELETE_CELL_VERSION)
    cout << "\tDELETE_CELL_VERSION\n";
  else
    HT_ASSERT(!"Invalid flag byte encountered");

}

