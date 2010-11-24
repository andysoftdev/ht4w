/** -*- c++ -*-
 * Copyright (C) 2009 Doug Judd (Zvents, Inc.17 *
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

#include "Common/Compat.h"
#include <cassert>
#include <string>
#include <vector>

extern "C" {
#include <poll.h>
#include <string.h>
}

#include <boost/algorithm/string.hpp>

#include "Common/Config.h"
#include "Common/Error.h"
#include "Common/FailureInducer.h"
#include "Common/FileUtils.h"
#include "Common/md5.h"

#include "Hypertable/Lib/CommitLog.h"
#include "Hypertable/Lib/CommitLogReader.h"

#include "CellStoreFactory.h"
#include "Global.h"
#include "Location.h"
#include "MergeScanner.h"
#include "MetadataNormal.h"
#include "MetadataRoot.h"
#include "Range.h"

using namespace Hypertable;
using namespace std;


Range::Range(MasterClientPtr &master_client,
             const TableIdentifier *identifier, SchemaPtr &schema,
             const RangeSpec *range, RangeSet *range_set,
             const RangeState *state)
    : m_bytes_read(0), m_cells_read(0), m_scans(0), m_bytes_written(0), m_cells_written(0),
      m_master_client(master_client), m_identifier(*identifier), m_schema(schema),
      m_revision(TIMESTAMP_MIN), m_latest_revision(TIMESTAMP_MIN), m_split_off_high(false),
      m_added_inserts(0), m_range_set(range_set), m_state(*state),
      m_error(Error::OK), m_dropped(false), m_capacity_exceeded_throttle(false),
      m_maintenance_generation(0) {
  AccessGroup *ag;

  memset(m_added_deletes, 0, 3*sizeof(int64_t));

  if (m_state.soft_limit == 0 || m_state.soft_limit > (uint64_t)Global::range_split_size)
    m_state.soft_limit = Global::range_split_size;

  m_start_row = range->start_row;
  m_end_row = range->end_row;

  // set the range id
  m_start_end_id.set(m_start_row.c_str(), m_end_row.c_str());

  /**
   * Determine split side
   */
  if (m_state.state == RangeState::SPLIT_LOG_INSTALLED ||
      m_state.state == RangeState::SPLIT_SHRUNK) {
    int cmp = strcmp(m_state.split_point, m_state.old_boundary_row);
    if (cmp < 0)
      m_split_off_high = true;
    else
      HT_ASSERT(cmp > 0);
  }
  else {
    String split_off = Config::get_str("Hypertable.RangeServer.Range.SplitOff");
    if (split_off == "high")
      m_split_off_high = true;
    else
      HT_ASSERT(split_off == "low");
  }

  if (m_state.state == RangeState::SPLIT_LOG_INSTALLED)
    split_install_log_rollback_metadata();

  m_name = format("%s[%s..%s]", identifier->id, range->start_row,
                  range->end_row);

  m_is_root = (m_identifier.is_metadata() && *range->start_row == 0
      && !strcmp(range->end_row, Key::END_ROOT_ROW));

  m_column_family_vector.resize(m_schema->get_max_column_family_id() + 1);

  foreach(Schema::AccessGroup *sag, m_schema->get_access_groups()) {
    ag = new AccessGroup(identifier, m_schema, sag, range);
    m_access_group_map[sag->name] = ag;
    m_access_group_vector.push_back(ag);

    foreach(Schema::ColumnFamily *scf, sag->columns)
      m_column_family_vector[scf->id] = ag;
  }

  if (m_is_root) {
    MetadataRoot metadata(m_schema);
    load_cell_stores(&metadata);
  }
  else {
    MetadataNormal metadata(&m_identifier, m_end_row);
    load_cell_stores(&metadata);
  }

  HT_DEBUG_OUT << "Range object for " << m_name << " constructed\n"
               << *state << HT_END;
}


void Range::split_install_log_rollback_metadata() {

  try {
    String metadata_key_str;
    KeySpec key;

    TableMutatorPtr mutator = Global::metadata_table->create_mutator();

    // Reset start row
    metadata_key_str = String("") + m_identifier.id + ":" + m_end_row;
    key.row = metadata_key_str.c_str();
    key.row_len = metadata_key_str.length();
    key.column_qualifier = 0;
    key.column_qualifier_len = 0;
    key.column_family = "StartRow";
    mutator->set(key, (uint8_t *)m_start_row.c_str(), m_start_row.length());

    // Get rid of new range
    metadata_key_str = format("%s:%s", m_identifier.id, m_state.split_point);
    key.row = metadata_key_str.c_str();
    key.row_len = metadata_key_str.length();
    key.column_qualifier = 0;
    key.column_qualifier_len = 0;
    key.column_family = 0;
    mutator->set_delete(key);

    mutator->flush();

  }
  catch (Hypertable::Exception &e) {
    // TODO: propagate exception
    HT_ERROR_OUT << "Problem rolling back Range from SPLIT_LOG_INSTALLED state " << e << HT_END;
    HT_ABORT;
  }

}


/**
 *
 */
void Range::update_schema(SchemaPtr &schema) {
  ScopedLock lock(m_schema_mutex);

  vector<Schema::AccessGroup*> new_access_groups;
  AccessGroup *ag;
  AccessGroupMap::iterator ag_iter;
  size_t max_column_family_id = schema->get_max_column_family_id();

  // only update schema if there is more recent version
  if(schema->get_generation() <= m_schema->get_generation())
    return;

  // resize column family vector if needed
  if (max_column_family_id > m_column_family_vector.size()-1)
    m_column_family_vector.resize(max_column_family_id+1);

  // update all existing access groups & create new ones as needed
  foreach(Schema::AccessGroup *s_ag, schema->get_access_groups()) {
    if( (ag_iter = m_access_group_map.find(s_ag->name)) !=
        m_access_group_map.end()) {
      ag_iter->second->update_schema(schema, s_ag);
      foreach(Schema::ColumnFamily *s_cf, s_ag->columns) {
        if (s_cf->deleted == false)
          m_column_family_vector[s_cf->id] = ag_iter->second;
      }
    }
    else {
      new_access_groups.push_back(s_ag);
    }
  }

  // create new access groups
  RangeSpec range_spec(m_start_row.c_str(), m_end_row.c_str());
  foreach(Schema::AccessGroup *s_ag, new_access_groups) {
    ag = new AccessGroup(&m_identifier, schema, s_ag, &range_spec);
    m_access_group_map[s_ag->name] = ag;
    m_access_group_vector.push_back(ag);

    foreach(Schema::ColumnFamily *s_cf, s_ag->columns) {
      if (s_cf->deleted == false)
        m_column_family_vector[s_cf->id] = ag;
    }
  }

  // TODO: remove deleted access groups
  m_schema = schema;
  return;
}

/**
 *
 */
void Range::load_cell_stores(Metadata *metadata) {
  AccessGroup *ag;
  CellStorePtr cellstore;
  uint32_t csid;
  const char *base, *ptr, *end;
  std::vector<String> csvec;
  String ag_name;
  String files;
  String file_str;
  bool need_update;

  metadata->reset_files_scan();

  while (metadata->get_next_files(ag_name, files)) {
    csvec.clear();
    need_update = false;

    if ((ag = m_access_group_map[ag_name]) == 0) {
      HT_ERRORF("Unrecognized access group name '%s' found in METADATA for "
                "table '%s'", ag_name.c_str(), m_identifier.id);
      HT_ABORT;
    }

    ptr = base = (const char *)files.c_str();
    end = base + strlen(base);
    while (ptr < end) {

      while (*ptr != ';' && ptr < end)
        ptr++;

      file_str = String(base, ptr-base);
      boost::trim(file_str);

      if (!file_str.empty()) {
        if (file_str[0] == '#') {
          ++ptr;
          base = ptr;
          need_update = true;
          continue;
        }

        csvec.push_back(file_str);
      }
      ++ptr;
      base = ptr;
    }

    files = "";

    String file_basename = Global::toplevel_dir + "/tables/";

    for (size_t i=0; i<csvec.size(); i++) {

      files += csvec[i] + ";\n";

      HT_INFOF("Loading CellStore %s", csvec[i].c_str());

      if (!extract_csid_from_path(csvec[i], &csid)) {
        HT_THROWF(Error::RANGESERVER_BAD_CELLSTORE_FILENAME,
                  "Unable to extract cell store ID from path '%s'",
                  csvec[i].c_str());
      }

      cellstore = CellStoreFactory::open(file_basename + csvec[i], m_start_row.c_str(), m_end_row.c_str());

      int64_t revision = boost::any_cast<int64_t>
        (cellstore->get_trailer()->get("revision"));
      if (revision > m_latest_revision)
        m_latest_revision = revision;

      ag->add_cell_store(cellstore, csid);
    }

    /** this causes startup deadlock (and is not needed) ..
    if (need_update)
      metadata->write_files(ag_name, files);
    */

  }

}


bool Range::extract_csid_from_path(String &path, uint32_t *csidp) {
  const char *base;

  if ((base = strrchr(path.c_str(), '/')) == 0 || strncmp(base, "/cs", 3))
    *csidp = 0;
  else
    *csidp = atoi(base+3);

  return true;
}


/**
 * This method must not fail.  The caller assumes that it will succeed.
 */
void Range::add(const Key &key, const ByteString value) {
  HT_DEBUG_OUT <<"key="<< key <<" value='";
  const uint8_t *p;
  size_t len = value.decode_length(&p);
  _out_ << format_bytes(20, p, len) << HT_END;

  if (key.flag == FLAG_DELETE_ROW) {
    for (size_t i=0; i<m_access_group_vector.size(); ++i)
      m_access_group_vector[i]->add(key, value);
  }
  else
    m_column_family_vector[key.column_family_code]->add(key, value);

  if (key.flag == FLAG_INSERT)
    m_added_inserts++;
  else
    m_added_deletes[key.flag]++;

  if (key.revision > m_revision)
    m_revision = key.revision;
}


CellListScanner *Range::create_scanner(ScanContextPtr &scan_ctx) {
  bool return_deletes = scan_ctx->spec ? scan_ctx->spec->return_deletes : false;
  MergeScanner *mscanner = new MergeScanner(scan_ctx, return_deletes);
  AccessGroupVector  ag_vector(0);

  {
    ScopedLock lock(m_schema_mutex);
    ag_vector = m_access_group_vector;
    m_scans++;
  }

  try {
    for (size_t i=0; i<ag_vector.size(); ++i) {
      if (ag_vector[i]->include_in_scan(scan_ctx))
        mscanner->add_scanner(ag_vector[i]->create_scanner(scan_ctx));
    }
  }
  catch (Exception &e) {
    delete mscanner;
    HT_THROW2(e.code(), e, "");
  }

  // increment #scanners
  return mscanner;
}


uint64_t Range::disk_usage() {
  ScopedLock lock(m_schema_mutex);
  uint64_t usage = 0;
  for (size_t i=0; i<m_access_group_vector.size(); i++)
    usage += m_access_group_vector[i]->disk_usage();
  return usage;
}

bool Range::need_maintenance() {
  ScopedLock lock(m_schema_mutex);
  bool needed = false;
  int64_t mem, disk, disk_total = 0;
  for (size_t i=0; i<m_access_group_vector.size(); ++i) {
    m_access_group_vector[i]->space_usage(&mem, &disk);
    disk_total += disk;
    if (mem >= Global::access_group_max_mem)
      needed = true;
  }
  if (m_identifier.is_metadata()) {
    if (Global::range_metadata_split_size != 0 &&
        disk_total >= (int64_t)Global::range_metadata_split_size)
      needed = true;
  }
  else if (disk_total >= Global::range_split_size)
    needed = true;
  return needed;
}


bool Range::cancel_maintenance() {
  return m_dropped ? true : false;
}


Range::MaintenanceData *Range::get_maintenance_data(ByteArena &arena, time_t now) {
  MaintenanceData *mdata = (MaintenanceData *)arena.alloc( sizeof(MaintenanceData) );
  AccessGroup::MaintenanceData **tailp = 0;
  AccessGroupVector  ag_vector(0);
  uint64_t size=0;
  int64_t starting_maintenance_generation;

  memset(mdata, 0, sizeof(MaintenanceData));

  {
    ScopedLock lock(m_schema_mutex);
    ag_vector = m_access_group_vector;
    mdata->scans = m_scans;
    mdata->bytes_written = m_bytes_written;
    mdata->cells_written = m_cells_written;
  }

  mdata->range = this;
  mdata->table_id = m_identifier.id;
  mdata->is_metadata = m_identifier.is_metadata();
  mdata->schema_generation = m_identifier.generation;

  // record starting maintenance generation
  {
    ScopedLock lock(m_mutex);
    starting_maintenance_generation = m_maintenance_generation;
    mdata->bytes_read = m_bytes_read;
    mdata->cells_read = m_cells_read;
    mdata->state = m_state.state;
    mdata->range_id = m_start_end_id;
  }

  for (size_t i=0; i<ag_vector.size(); i++) {
    if (mdata->agdata == 0) {
      mdata->agdata = ag_vector[i]->get_maintenance_data(arena, now);
      tailp = &mdata->agdata;
    }
    else {
      (*tailp)->next = ag_vector[i]->get_maintenance_data(arena, now);
      tailp = &(*tailp)->next;
    }
    size += (*tailp)->disk_used;
    mdata->disk_used += (*tailp)->disk_used;
    mdata->memory_used += (*tailp)->mem_used;
    mdata->memory_allocated += (*tailp)->mem_allocated;
    mdata->block_index_memory += (*tailp)->block_index_memory;
    mdata->bloom_filter_memory += (*tailp)->bloom_filter_memory;
    mdata->bloom_filter_accesses += (*tailp)->bloom_filter_accesses;
    mdata->bloom_filter_maybes += (*tailp)->bloom_filter_maybes;
    mdata->bloom_filter_fps += (*tailp)->bloom_filter_fps;
    mdata->shadow_cache_memory += (*tailp)->shadow_cache_memory;
  }

  if (tailp)
    (*tailp)->next = 0;

  if (size > (uint64_t)Global::range_maximum_size) {
    ScopedLock lock(m_mutex);
    if (starting_maintenance_generation == m_maintenance_generation)
      m_capacity_exceeded_throttle = true;
  }

  mdata->busy = m_maintenance_guard.in_progress();

  return mdata;
}


void Range::split() {
  RangeMaintenanceGuard::Activator activator(m_maintenance_guard);
  String old_start_row;

  HT_ASSERT(!m_is_root);

  try {

    switch (m_state.state) {

    case (RangeState::STEADY):
      split_install_log();

    case (RangeState::SPLIT_LOG_INSTALLED):
      split_compact_and_shrink();

    case (RangeState::SPLIT_SHRUNK):
      split_notify_master();

    }

  }
  catch (Exception &e) {
    if (e.code() == Error::CANCELLED || cancel_maintenance())
      return;
    throw;
  }

  {
    ScopedLock lock(m_mutex);
    m_capacity_exceeded_throttle = false;
    m_maintenance_generation++;
  }

  HT_INFOF("Split Complete.  New Range end_row=%s", m_start_row.c_str());
}



/**
 */
void Range::split_install_log() {
  std::vector<String> split_rows;
  char md5DigestStr[33];
  AccessGroupVector  ag_vector(0);

  {
    ScopedLock lock(m_schema_mutex);
    ag_vector = m_access_group_vector;
  }

  if (cancel_maintenance())
    HT_THROW(Error::CANCELLED, "");

  for (size_t i=0; i<ag_vector.size(); i++)
    ag_vector[i]->get_split_rows(split_rows, false);

  /**
   * If we didn't get at least one row from each Access Group, then try again
   * the hard way (scans CellCache for middle row)
   */
  if (split_rows.size() < ag_vector.size()) {
    for (size_t i=0; i<ag_vector.size(); i++)
      ag_vector[i]->get_split_rows(split_rows, true);
  }
  sort(split_rows.begin(), split_rows.end());

  /**
  cout << flush;
  cout << "thelma Dumping split rows for " << m_name << "\n";
  for (size_t i=0; i<split_rows.size(); i++)
    cout << "thelma Range::get_split_row [" << i << "] = " << split_rows[i]
         << "\n";
  cout << flush;
  */

  /**
   * If we still didn't get a good split row, try again the *really* hard way
   * by collecting all of the cached rows, sorting them and then taking the
   * middle.
   */
  if (split_rows.size() > 0) {
    ScopedLock lock(m_mutex);
    m_split_row = split_rows[split_rows.size()/2];
    if (m_split_row < m_start_row || m_split_row >= m_end_row) {
      split_rows.clear();
      for (size_t i=0; i<ag_vector.size(); i++)
        ag_vector[i]->get_cached_rows(split_rows);
      if (split_rows.size() > 0) {
        sort(split_rows.begin(), split_rows.end());
        m_split_row = split_rows[split_rows.size()/2];
        if (m_split_row < m_start_row || m_split_row >= m_end_row) {
          m_error = Error::RANGESERVER_ROW_OVERFLOW;
          HT_THROWF(Error::RANGESERVER_ROW_OVERFLOW,
                    "(a) Unable to determine split row for range %s[%s..%s]",
                    m_identifier.id, m_start_row.c_str(), m_end_row.c_str());
        }
      }
      else {
        m_error = Error::RANGESERVER_ROW_OVERFLOW;
        HT_THROWF(Error::RANGESERVER_ROW_OVERFLOW,
                  "(b) Unable to determine split row for range %s[%s..%s]",
                   m_identifier.id, m_start_row.c_str(), m_end_row.c_str());
      }
    }
  }
  else {
    m_error = Error::RANGESERVER_ROW_OVERFLOW;
    HT_THROWF(Error::RANGESERVER_ROW_OVERFLOW,
              "(c) Unable to determine split row for range %s[%s..%s]",
              m_identifier.id, m_start_row.c_str(), m_end_row.c_str());
  }

  m_state.set_split_point(m_split_row);

  /**
   * Create split (transfer) log
   */
  md5_trunc_modified_base64(m_state.split_point, md5DigestStr);
  md5DigestStr[16] = 0;
  m_state.set_transfer_log(Global::log_dir + "/" + md5DigestStr);

  // Create transfer log dir
  try {
    Global::log_dfs->rmdir(m_state.transfer_log);
    Global::log_dfs->mkdirs(m_state.transfer_log);
  }
  catch (Exception &e) {
    HT_ERROR_OUT << "Problem creating log directory '%s' - " << e << HT_END;
    HT_ABORT;
  }

  /**
   * Create and install the split log
   */
  {
    Barrier::ScopedActivator block_updates(m_update_barrier);
    ScopedLock lock(m_mutex);
    for (size_t i=0; i<ag_vector.size(); i++)
      ag_vector[i]->stage_compaction();
    m_split_log = new CommitLog(Global::dfs, m_state.transfer_log);
  }

  if (m_split_off_high)
    m_state.set_old_boundary_row(m_end_row);
  else
    m_state.set_old_boundary_row(m_start_row);

  /**
   * Write SPLIT_START MetaLog entry
   */
  m_state.state = RangeState::SPLIT_LOG_INSTALLED;
  for (int i=0; true; i++) {
    try {
      Global::range_log->log_split_start(m_identifier,
          RangeSpec(m_start_row.c_str(), m_end_row.c_str()), m_state);
      break;
    }
    catch (Exception &e) {
      if (i<3) {
        HT_WARNF("%s - %s", Error::get_text(e.code()), e.what());
        poll(0, 0, 5000);
        continue;
      }
      HT_ERRORF("Problem writing SPLIT_LOG_INSTALLED meta log entry for %s "
                "split-point='%s'", m_name.c_str(), m_state.split_point);
      HT_FATAL_OUT << e << HT_END;
    }
  }

  HT_MAYBE_FAIL("split-1");
  HT_MAYBE_FAIL_X("metadata-split-1", m_identifier.is_metadata());

}


void Range::split_compact_and_shrink() {
  int error;
  String old_start_row = m_start_row;
  String old_end_row = m_end_row;
  AccessGroupVector  ag_vector(0);

  {
    ScopedLock lock(m_schema_mutex);
    ag_vector = m_access_group_vector;
  }

  if (cancel_maintenance())
    HT_THROW(Error::CANCELLED, "");

  /**
   * Perform major compactions
   */
  for (size_t i=0; i<ag_vector.size(); i++)
    ag_vector[i]->run_compaction(MaintenanceFlag::COMPACT_MAJOR);

  try {
    String files;
    String metadata_key_str;
    KeySpec key;

    TableMutatorPtr mutator = Global::metadata_table->create_mutator();

    // For new range with existing end row, update METADATA entry with new
    // 'StartRow' column.
    metadata_key_str = String("") + m_identifier.id + ":" + m_end_row;
    key.row = metadata_key_str.c_str();
    key.row_len = metadata_key_str.length();
    key.column_qualifier = 0;
    key.column_qualifier_len = 0;
    key.column_family = "StartRow";
    mutator->set(key, (uint8_t *)m_state.split_point,
                 strlen(m_state.split_point));
    if (m_split_off_high) {
      key.column_family = "Files";
      for (size_t i=0; i<ag_vector.size(); i++) {
        key.column_qualifier = ag_vector[i]->get_name();
        key.column_qualifier_len = strlen(ag_vector[i]->get_name());
        ag_vector[i]->get_file_list(files, false);
        if (files != "")
          mutator->set(key, (uint8_t *)files.c_str(), files.length());
      }
    }

    // For new range whose end row is the split point, create a new METADATA
    // entry
    metadata_key_str = format("%s:%s", m_identifier.id, m_state.split_point);
    key.row = metadata_key_str.c_str();
    key.row_len = metadata_key_str.length();
    key.column_qualifier = 0;
    key.column_qualifier_len = 0;

    key.column_family = "StartRow";
    mutator->set(key, old_start_row.c_str(), old_start_row.length());

    key.column_family = "Files";
    for (size_t i=0; i<ag_vector.size(); i++) {
      key.column_qualifier = ag_vector[i]->get_name();
      key.column_qualifier_len = strlen(ag_vector[i]->get_name());
      ag_vector[i]->get_file_list(files, m_split_off_high);
      if (files != "")
        mutator->set(key, (uint8_t *)files.c_str(), files.length());
    }
    if (m_split_off_high) {
      key.column_qualifier = 0;
      key.column_qualifier_len = 0;
      key.column_family = "Location";
      String location = Location::get();
      mutator->set(key, location.c_str(), location.length());
    }

    mutator->flush();

  }
  catch (Hypertable::Exception &e) {
    // TODO: propagate exception
    HT_ERROR_OUT <<"Problem updating METADATA after split (new_end="
        << m_state.split_point <<", old_end="<< m_end_row <<") "<< e << HT_END;
    // need to unblock updates and then return error
    HT_ABORT;
  }

  /**
   *  Shrink the range
   */
  {
    Barrier::ScopedActivator block_updates(m_update_barrier);
    Barrier::ScopedActivator block_scans(m_scan_barrier);

    // Shrink access groups
    if (m_split_off_high) {
      if (!m_range_set->change_end_row(m_end_row, m_state.split_point)) {
        HT_ERROR_OUT << "Problem changing end row of range " << m_name
                     << " to " << m_state.split_point << HT_END;
        HT_ABORT;
      }
    }
    {
      ScopedLock lock(m_mutex);
      String split_row = m_state.split_point;

      // Shrink access groups
      if (m_split_off_high)
        m_end_row = m_state.split_point;
      else
        m_start_row = m_state.split_point;

      // set the range id
      m_start_end_id.set(m_start_row.c_str(), m_end_row.c_str());

      m_name = String(m_identifier.id) + "[" + m_start_row + ".." + m_end_row
        + "]";
      m_split_row = "";
      for (size_t i=0; i<ag_vector.size(); i++)
        ag_vector[i]->shrink(split_row, m_split_off_high);

      // Close and uninstall split log
      if ((error = m_split_log->close()) != Error::OK) {
        HT_ERRORF("Problem closing split log '%s' - %s",
                  m_split_log->get_log_dir().c_str(), Error::get_text(error));
      }
      m_split_log = 0;
    }
  }

  /**
   * Write SPLIT_SHRUNK MetaLog entry
   */
  m_state.state = RangeState::SPLIT_SHRUNK;
  if (m_split_off_high) {
    /** Create DFS directories for this range **/
    {
      char md5DigestStr[33];
      String table_dir, range_dir;

      md5_trunc_modified_base64(m_end_row.c_str(), md5DigestStr);
      md5DigestStr[16] = 0;
      table_dir = Global::toplevel_dir + "/tables/" + m_identifier.id;

      {
        ScopedLock lock(m_schema_mutex);
        foreach(Schema::AccessGroup *ag, m_schema->get_access_groups()) {
          // notice the below variables are different "range" vs. "table"
          range_dir = table_dir + "/" + ag->name + "/" + md5DigestStr;
          Global::dfs->mkdirs(range_dir);
        }
      }
    }

  }

  for (int i=0; true; i++) {
    try {
      Global::range_log->log_split_shrunk(m_identifier,
          RangeSpec(m_start_row.c_str(), m_end_row.c_str()), m_state);
      break;
    }
    catch (Exception &e) {
      if (i<3) {
        HT_ERRORF("%s - %s", Error::get_text(e.code()), e.what());
        poll(0, 0, 5000);
        continue;
      }
      HT_ERRORF("Problem writing SPLIT_SHRUNK meta log entry for %s "
                "split-point='%s'", m_name.c_str(), m_state.split_point);
      HT_FATAL_OUT << e << HT_END;
    }
  }

  HT_MAYBE_FAIL("split-2");
  HT_MAYBE_FAIL_X("metadata-split-2", m_identifier.is_metadata());

}


void Range::split_notify_master() {
  RangeSpec range;
  int64_t soft_limit = (int64_t)m_state.soft_limit;

  if (cancel_maintenance())
    HT_THROW(Error::CANCELLED, "");

  if (m_split_off_high) {
    range.start_row = m_end_row.c_str();
    range.end_row = m_state.old_boundary_row;
  }
  else {
    range.start_row = m_state.old_boundary_row;
    range.end_row = m_start_row.c_str();
  }

  // update the latest generation, this should probably be protected
  {
    ScopedLock lock(m_schema_mutex);
    m_identifier.generation = m_schema->get_generation();
  }

  HT_INFOF("Reporting newly split off range %s[%s..%s] to Master",
           m_identifier.id, range.start_row, range.end_row);

  if (soft_limit < Global::range_split_size) {
    soft_limit *= 2;
    if (soft_limit > Global::range_split_size)
      soft_limit = Global::range_split_size;
  }

  m_master_client->report_split(&m_identifier, range,
                                m_state.transfer_log, soft_limit);

  /**
   * NOTE: try the following crash and make sure that the master does
   * not try to load the range twice.
   */

  HT_MAYBE_FAIL("split-3");
  HT_MAYBE_FAIL_X("metadata-split-3", m_identifier.is_metadata());

  m_state.soft_limit = soft_limit;

  /**
   * Write SPLIT_DONE MetaLog entry
   */
  for (int i=0; true; i++) {
    try {
      Global::range_log->log_split_done(m_identifier,
          RangeSpec(m_start_row.c_str(), m_end_row.c_str()), m_state);
      break;
    }
    catch (Exception &e) {
      if (i<2) {
        HT_ERRORF("%s - %s", Error::get_text(e.code()), e.what());
        poll(0, 0, 5000);
        continue;
      }
      HT_ERRORF("Problem writing SPLIT_DONE meta log entry for %s "
                "split-point='%s'", m_name.c_str(), m_state.split_point);
      HT_FATAL_OUT << e << HT_END;
    }
  }

  m_state.clear();

  HT_MAYBE_FAIL("split-4");
  HT_MAYBE_FAIL_X("metadata-split-4", m_identifier.is_metadata());

}


void Range::compact(MaintenanceFlag::Map &subtask_map) {
  RangeMaintenanceGuard::Activator activator(m_maintenance_guard);
  AccessGroupVector ag_vector(0);
  int flags;

  {
    ScopedLock lock(m_schema_mutex);
    ag_vector = m_access_group_vector;
  }

  try {

    // Initiate minor compactions (freeze cell cache)
    {
      Barrier::ScopedActivator block_updates(m_update_barrier);
      ScopedLock lock(m_mutex);
      for (size_t i=0; i<ag_vector.size(); i++) {
    if (subtask_map.minor_compaction(ag_vector[i].get()))
      ag_vector[i]->stage_compaction();
      }
    }

    // do compactions
    for (size_t i=0; i<ag_vector.size(); i++) {
      flags = subtask_map.flags(ag_vector[i].get());
      if (flags & MaintenanceFlag::COMPACT) {
    try {
      ag_vector[i]->run_compaction(flags);
    }
    catch (Exception&) {
      ag_vector[i]->unstage_compaction();
    }
      }
    }

  }
  catch (Exception &e) {
    if (e.code() == Error::CANCELLED || cancel_maintenance())
      return;
    throw;
  }

  {
    ScopedLock lock(m_mutex);
    m_capacity_exceeded_throttle = false;
    m_maintenance_generation++;
  }
}



void Range::purge_memory(MaintenanceFlag::Map &subtask_map) {
  RangeMaintenanceGuard::Activator activator(m_maintenance_guard);
  AccessGroupVector ag_vector(0);
  uint64_t memory_purged = 0;

  {
    ScopedLock lock(m_schema_mutex);
    ag_vector = m_access_group_vector;
  }

  try {
    for (size_t i=0; i<ag_vector.size(); i++) {
      if ( subtask_map.memory_purge(ag_vector[i].get()) )
    memory_purged += ag_vector[i]->purge_memory(subtask_map);
    }
  }
  catch (Exception &e) {
    if (e.code() == Error::CANCELLED || cancel_maintenance())
      return;
    throw;
  }

  {
    ScopedLock lock(m_mutex);
    m_maintenance_generation++;
  }

  HT_INFOF("Memory Purge complete for range %s.  Purged %llu bytes of memory",
       m_name.c_str(), (Llu)memory_purged);

}


/**
 * This method is called when the range is offline so no locking is needed
 */
void Range::recovery_finalize() {

  if (m_state.state == RangeState::SPLIT_LOG_INSTALLED) {
    CommitLogReaderPtr commit_log_reader =
        new CommitLogReader(Global::dfs, m_state.transfer_log);
    replay_transfer_log(commit_log_reader.get());
    commit_log_reader = 0;

    // re-initiate compaction
    for (size_t i=0; i<m_access_group_vector.size(); i++)
      m_access_group_vector[i]->stage_compaction();

    m_split_log = new CommitLog(Global::dfs, m_state.transfer_log);
    m_split_row = m_state.split_point;
    HT_INFOF("Restored range state to SPLIT_LOG_INSTALLED (split point='%s' "
             "split log='%s')", m_state.split_point, m_state.transfer_log);
  }

  for (size_t i=0; i<m_access_group_vector.size(); i++)
    m_access_group_vector[i]->recovery_finalize();
}


void Range::lock() {
  m_schema_mutex.lock();
  for (size_t i=0; i<m_access_group_vector.size(); ++i)
    m_access_group_vector[i]->lock();
  m_revision = TIMESTAMP_MIN;
}


void Range::unlock() {

  // this needs to happen before the subsequent m_mutex lock
  // because the lock ordering is assumed to be Range::m_mutex -> AccessGroup::lock
  for (size_t i=0; i<m_access_group_vector.size(); ++i)
    m_access_group_vector[i]->unlock();

  {
    ScopedLock lock(m_mutex);
    if (m_revision > m_latest_revision)
      m_latest_revision = m_revision;
  }

  m_schema_mutex.unlock();
}


void Range::replay_transfer_log(CommitLogReader *commit_log_reader) {
  BlockCompressionHeaderCommitLog header;
  const uint8_t *base, *ptr, *end;
  size_t len;
  ByteString key, value;
  Key key_comps;
  size_t nblocks = 0;
  size_t count = 0;
  TableIdentifier table_id;

  m_revision = TIMESTAMP_MIN;

  try {

    while (commit_log_reader->next(&base, &len, &header)) {

      ptr = base;
      end = base + len;

      table_id.decode(&ptr, &len);

      if (strcmp(m_identifier.id, table_id.id))
        HT_THROWF(Error::RANGESERVER_CORRUPT_COMMIT_LOG,
                  "Table name mis-match in split log replay \"%s\" != \"%s\"",
                  m_identifier.id, table_id.id);

      while (ptr < end) {
        key.ptr = (uint8_t *)ptr;
        key_comps.load(key);
        ptr += key_comps.length;
        value.ptr = (uint8_t *)ptr;
        ptr += value.length();
        add(key_comps, value);
        count++;
      }
      nblocks++;
    }

    if (m_revision > m_latest_revision)
      m_latest_revision = m_revision;

    {
      ScopedLock lock(m_mutex);
      HT_INFOF("Replayed %d updates (%d blocks) from split log '%s' into "
               "%s[%s..%s]", (int)count, (int)nblocks,
               commit_log_reader->get_log_dir().c_str(),
               m_identifier.id, m_start_row.c_str(), m_end_row.c_str());
    }

    m_added_inserts = 0;
    memset(m_added_deletes, 0, 3*sizeof(int64_t));

  }
  catch (Hypertable::Exception &e) {
    HT_ERROR_OUT << "Problem replaying split log - " << e << HT_END;
    if (m_revision > m_latest_revision)
      m_latest_revision = m_revision;
    throw;
  }
}


int64_t Range::get_scan_revision() {
  ScopedLock lock(m_mutex);
  return m_latest_revision;
}

