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

#include <Common/Compat.h>

#include "MetadataDumpReader.h"

#include <Hypertable/Lib/LoadDataEscape.h>

#include <Common/StringExt.h>

using namespace Hypertable;
using namespace std;

MetadataDumpReader::MetadataDumpReader(const String &fname,
				       StringSet &valid_locations,
				       StringSet &valid_tids,
				       FlyweightString &flyweight,
				       MetadataRepairWriter &repair_writer)
  : m_flyweight(flyweight), m_lineno(0) {
  RangeInfo rkey, *rinfo;
  char *buffer = new char [ 10*1024*1024 ];
  const char *table_id, *end_row, *column, *value;
  RangeInfoSetEndRow::iterator iter;
  RangeInfoSetEndRow riset_endrow;
  LoadDataEscape escaper;
  const char *unescaped_str;
  size_t unescaped_len;
  std::map<String, size_t> skipped_tid_map;
  StringSet deleted_rows;

  if ((m_fp = fopen(fname.c_str(), "r")) == 0) {
    String msg = format("Unable to open '%s' for reading", fname.c_str());
    perror(msg.c_str());
    exit(1);
  }

  m_lineno = 1;
  while (fgets(buffer, 10*1024*1024, m_fp)) {
    parse_line(buffer, &table_id, &end_row, &column, &value);

    if (strcmp(column, "StartRow") && strcmp(column, "Location") && strncmp(column, "Files", 5)) {
      //cout << "Skipping line " << m_lineno << " because column '" << column << "' not needed" << endl;
      m_lineno++;
      continue;
    }

    if (valid_tids.count(table_id) == 0) {
      std::map<String, size_t>::iterator iter = skipped_tid_map.find(table_id);
      if (iter == skipped_tid_map.end())
	skipped_tid_map[table_id] = 1;
      else {
	size_t count = iter->second + 1;
	skipped_tid_map.erase(iter);
	skipped_tid_map[table_id] = count;
      }
      String delete_row = String(table_id) + ":" + end_row;
      if (deleted_rows.count(delete_row) == 0) {
	repair_writer.delete_row(table_id, end_row, false);
	deleted_rows.insert(delete_row);
      }
      m_lineno++;
      continue;
    }

    escaper.unescape(end_row, strlen(end_row), &unescaped_str, &unescaped_len);

    rkey.end_row = (char *)unescaped_str;
    rkey.end_row_len = unescaped_len;
    rkey.table_id = m_flyweight.get(table_id);

    if ((iter = riset_endrow.find(&rkey)) == riset_endrow.end()) {
      rinfo = new RangeInfo();
      rinfo->table_id = m_flyweight.get(table_id);
      rinfo->end_row_len = rkey.end_row_len;
      rinfo->end_row = new char [ rkey.end_row_len + 1 ];
      memcpy(rinfo->end_row, rkey.end_row, rkey.end_row_len);
      rinfo->end_row[rkey.end_row_len] = 0;
      riset_endrow.insert(rinfo);
    }
    else
      rinfo = *iter;

    if (!strcmp(column, "StartRow")) {
      if (rinfo->start_row != 0) {
        cout << "error: duplicate StartRow encountered for " << table_id 
             << ":" << end_row << endl;
        cout << "       METADATA should be dumped with 'REVS 1'" << endl;
        exit(1);
      }
      escaper.unescape(value, strlen(value), &unescaped_str, &unescaped_len);
      rinfo->start_row_len = unescaped_len;
      rinfo->start_row = new char [ unescaped_len + 1 ];
      memcpy(rinfo->start_row, unescaped_str, unescaped_len);
      rinfo->start_row[unescaped_len] = 0;
      rinfo->got_start_row = true;
    }
    else if (!strcmp(column, "Location")) {
      if (rinfo->location != 0) {
        cout << "error: duplicate Location encountered for " << table_id 
             << ":" << end_row << endl;
        cout << "       METADATA should be dumped with 'REVS 1'" << endl;
        exit(1);
      }
      if (valid_locations.count(value) > 0) {
        rinfo->location = m_flyweight.get(value);
        rinfo->got_location = true;
      }
    }
    else if (!strncmp(column, "Files", 5)) {
      rinfo->got_files = true;
      String tmp = m_flyweight.get(value);
      char *p = (char *)tmp.c_str();
      size_t len = strlen(p);
      while (p < tmp.c_str() + len) {
        char *s = p;
        while (*p != ';') {
          if (*p == '\0') {
            cout << "error: invalid cellstore File encountered for "
                 << table_id << ":" << end_row << endl;
            exit(1);
          }
          p++;
        }
        if (*(p + 1) != '\\' && *(p + 2) != '\n') {
          if (*p == '\0') {
            cout << "error: invalid cellstore File encountered for "
                 << table_id << ":" << end_row << endl;
            exit(1);
          }
        }
        *p = '\0';
        rinfo->files.push_back(s);
        p += 3;
      }
    }

    m_lineno++;
  }

  for (std::map<String, size_t>::iterator iter = skipped_tid_map.begin();
       iter != skipped_tid_map.end(); ++iter) {
    cout << "Skipped " << iter->second << " lines of METADATA dump file because they're for table '"
	 << iter->first << "' which does not exist." << endl;
  }

  for (iter = riset_endrow.begin(); iter != riset_endrow.end(); ++iter)
    m_riset.insert(*iter);

  rkey.end_row = 0;
}


void MetadataDumpReader::parse_line(char *line, const char **tablep, const char **endrowp,
				   const char **columnp, const char **valuep) {
  char *ptr = strchr(line, ':');

  *tablep = line;
  if (ptr == 0) {
    cout << "Mal-formed METADATA line " << m_lineno << ", no ':' character found" << endl;
    exit(1);
  }
  *ptr++ = 0;

  *endrowp = ptr;
  ptr = strchr(ptr, '\t');
  if (ptr == 0) {
    cout << "Mal-formed METADATA line " << m_lineno << ", too few tab characters found" << endl;
    exit(1);
  }
  *ptr++ = 0;

  *columnp = ptr;
  ptr = strchr(ptr, '\t');
  if (ptr == 0) {
    cout << "Mal-formed METADATA line " << m_lineno << ", too few tab characters found" << endl;
    exit(1);
  }
  *ptr++ = 0;

  *valuep = ptr;
  ptr = ptr + strlen(ptr);
  while (ptr > *valuep && *(ptr-1) == '\n')
    ptr--;
  *ptr = 0;
}


#if 0

RangeInfo *MetadataDumpReader::next() {
  if (m_eof)
    return 0;
  RangeInfo *rinfo = new RangeInfo();
  while (m_next_ptr && process_current_line(rinfo)) {
    m_next_ptr = fgetln(m_fp, &m_next_len);
    m_lineno++;
  }
  if (m_next_ptr == 0) {
    if (feof(m_fp))
      m_eof = true;
    else {
      cout << "Error occurred on i/o stream" << endl;
      exit(1);
    }
  }
  return rinfo;
}


bool MetadataDumpReader::process_current_line(RangeInfo *rinfo) {
  char *endptr = m_next_ptr + m_next_len;
  char *base, *ptr;
  while (endptr > m_next_ptr && *(endptr-1) == '\n')
    endptr--;

  for (base = m_next_ptr; base<endptr && *base != ':'; base++)
    ;
  if (base == endptr) {
    cout << "Malformed metadata line " << m_lineno << ", no table ID found" << endl;
    exit(1);
  }
  if (rinfo->table_id == 0)
    rinfo->table_id = m_flyweight.get(m_next_ptr, base-m_next_ptr);
  else {
    const char *loc = m_flyweight.get(m_next_ptr, base-m_next_ptr);
    if (strcmp(loc, rinfo->table_id))
      return false;
  }
  base++;

  for (ptr = base; ptr<endptr && *ptr != '\t'; ptr++)
    ;
  if (ptr == endptr) {
    cout << "Malformed metadata line " << m_lineno << ", no tab found" << endl;
    exit(1);
  }
  if (rinfo->end_row) {
    if (rinfo->end_row_len != (size_t)(ptr-base) ||
        memcmp(rinfo->end_row, base, rinfo->end_row_len))
      return false;
  }
  else {
    rinfo->end_row_len = ptr - base;
    rinfo->end_row = new char [ rinfo->end_row_len+1 ];
    memcpy(rinfo->end_row, base, rinfo->end_row_len);
    rinfo->end_row[rinfo->end_row_len] = 0;
  }
  ptr++;
  if (!strncmp(ptr, "StartRow", 8)) {
    if (rinfo->start_row) {
      cout << "Malformed metadata line " << m_lineno << ", multiple StartRow lines encountered" << endl;
      exit(1);
    }
    ptr += 8;
    if (*ptr != '\t') {
      cout << "Malformed metadata line " << m_lineno << ", no second tab found" << endl;
      exit(1);
    }
    ptr++;
    rinfo->start_row_len = endptr - ptr;
    rinfo->start_row = new char [ rinfo->start_row_len+1 ];
    memcpy(rinfo->start_row, ptr, rinfo->start_row_len);
    rinfo->start_row[rinfo->start_row_len] = 0;
  }
  else if (!strncmp(ptr, "Location", 8)) {
    if (rinfo->location) {
      cout << "Malformed metadata line " << m_lineno << ", multiple Location lines encountered" << endl;
      exit(1);
    }
    ptr += 8;
    if (*ptr != '\t') {
      cout << "Malformed metadata line " << m_lineno << ", no second tab found" << endl;
      exit(1);
    }
    ptr++;
    rinfo->location = m_flyweight.get(ptr, endptr - ptr);
  }
  return true;
}

#endif
