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
#include "Common/Config.h"

#include <boost/algorithm/string.hpp>

#include "Hypertable/Lib/MetaLogDefinition.h"
#include "Hypertable/Lib/MetaLogReader.h"

#include "Hypertable/RangeServer/MetaLogDefinitionRangeServer.h"
#include "Hypertable/RangeServer/MetaLogEntityRange.h"

#include "RsmlReader.h"

using namespace Hypertable;
using namespace std;

RsmlReader::RsmlReader(FilesystemPtr &fs, StringSet &valid_locations, StringSet &valid_tids,
                       TableIdentifierMap &tidmap, RangeInfoSet &riset,
		       LocationEntitiesMapT &entities_map, size_t *entry_countp) {
  MetaLog::ReaderPtr rsml_reader;
  MetaLog::DefinitionPtr rsml_definition;
  vector<Filesystem::Dirent> listing;  
  std::vector<MetaLog::EntityPtr> entities;
  MetaLogEntityRangePtr range_entity;
  RangeInfo rkey, *rinfo;
  RangeInfoSet::iterator iter;
  TableIdentifier table;
  String start_row, end_row;

  String toplevel = Config::properties->get_str("Hypertable.Directory");
  boost::trim_if(toplevel, boost::is_any_of("/"));
  toplevel = String("/") + toplevel;

  fs->readdir(toplevel + "/servers", listing);

  *entry_countp = 0;

  for (size_t i=0; i<listing.size(); i++) {

    if (listing[i].name == "master" ||
        valid_locations.count(listing[i].name) == 0)
      continue;

    rsml_definition = make_shared<MetaLog::DefinitionRangeServer>( listing[i].name.c_str() );

    rsml_reader = make_shared<MetaLog::Reader>(fs, rsml_definition,
				      toplevel + "/servers/" + listing[i].name + "/log/" + rsml_definition->name());

    entities.clear();
    rsml_reader->get_entities(entities);

    entities_map[listing[i].name] = new EntityVectorT();

    for (size_t j=0; j<entities.size(); j++) {

      if ((range_entity = dynamic_pointer_cast<MetaLogEntityRange>(entities[j]))) {

	(*entry_countp)++;

	range_entity->get_table_identifier(table);
	range_entity->get_boundary_rows(start_row, end_row);

	if (valid_tids.count(table.id) == 0) {
	  cout << "Skipping range entity " << table.id << "[" << start_row << ".."
	       << end_row << "] in " << listing[i].name 
	       << " RSML because table does not exist" << endl;
	  continue;
	}

	tidmap.update(listing[i].name, table);

	rkey.table_id = table.id;
	rkey.end_row = (char *)end_row.c_str();
	rkey.end_row_len = end_row.length();
	rkey.start_row = (char *)start_row.c_str();
	rkey.start_row_len = start_row.length();
	rkey.location = listing[i].name.c_str();

	if ((iter = riset.find(&rkey)) == riset.end()) {
	  rinfo = create_rinfo_from_entity(listing[i].name, range_entity);
	  rinfo->entity = range_entity;
	  m_dangling_riset.insert(rinfo);
	}
	else {
	  // This should probably be a vector of entities
	  HT_ASSERT((*iter)->entity == 0);
	  (*iter)->entity = range_entity;
	}
      }
      else
	entities_map[listing[i].name]->push_back(entities[j]);
    }
  }

  rkey.start_row = 0;
  rkey.end_row = 0;

}



RangeInfo *RsmlReader::create_rinfo_from_entity(const String &location, MetaLogEntityRangePtr &range_entity) {
  TableIdentifier table;
  String start_row, end_row;
  RangeInfo *rinfo = new RangeInfo();

  range_entity->get_table_identifier(table);
  range_entity->get_boundary_rows(start_row, end_row);

  rinfo->table_id = m_flyweight.get(table.id);

  rinfo->end_row_len = end_row.length();
  rinfo->end_row = new char [ rinfo->end_row_len + 1 ];
  memcpy(rinfo->end_row, end_row.c_str(), rinfo->end_row_len);
  rinfo->end_row[rinfo->end_row_len] = 0;

  rinfo->start_row_len = start_row.length();
  rinfo->start_row = new char [ rinfo->start_row_len + 1 ];
  memcpy(rinfo->start_row, start_row.c_str(), rinfo->start_row_len);
  rinfo->start_row[rinfo->start_row_len] = 0;

  rinfo->location = m_flyweight.get(location.c_str());
  return rinfo;
}
