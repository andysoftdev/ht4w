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

#ifndef Tools_htck_MetadataDumpReader_h
#define Tools_htck_MetadataDumpReader_h

#include "MetadataRepairWriter.h"
#include "RangeInfo.h"

#include <Common/String.h>
#include <Common/FlyweightString.h>

#include <cstdio>


namespace Hypertable {

  class MetadataDumpReader {
  public:
    MetadataDumpReader(const String &fname, StringSet &valid_locations,
		       StringSet &valid_tids, FlyweightString &flyweight,
		       MetadataRepairWriter &repair_writer);
    void swap_riset(RangeInfoSet &riset) { riset.swap(m_riset); }

  private:
    void parse_line(char *line, const char **tablep, const char **endrowp,
		    const char **columnp, const char **valuep);

    FILE *m_fp;
    RangeInfoSet m_riset;
    FlyweightString &m_flyweight;
    size_t m_lineno;
  };

}


#endif // Tools_htck_MetadataDumpReader_h
