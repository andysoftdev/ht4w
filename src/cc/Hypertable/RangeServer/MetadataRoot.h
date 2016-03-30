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

#ifndef Hypertable_RangeServer_MetadataRoot_h
#define Hypertable_RangeServer_MetadataRoot_h

#include "Metadata.h"

#include <Hypertable/Lib/Schema.h>
#include <Hypertable/Lib/TableScanner.h>

#include <vector>

namespace Hypertable {
  class MetadataRoot : public Metadata {

  public:
    MetadataRoot(SchemaPtr &schema_ptr);
    virtual ~MetadataRoot();
    virtual void reset_files_scan();
    virtual bool get_next_files(String &ag_name, String &files, uint32_t *nextcsidp);
    virtual void write_files(const String &ag_name, const String &files, int64_t total_blocks);
    virtual void write_files(const String &ag_name, const String &files, int64_t total_blocks, uint32_t nextcsid);

  private:
    std::vector<String> m_agnames;
    size_t  m_next;
    uint64_t m_handle;
  };
}

#endif // Hypertable_RangeServer_MetadataRoot_h

