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

#ifndef HYPERTABLE_DATASOURCE_H
#define HYPERTABLE_DATASOURCE_H

#include "KeySpec.h"


namespace Hypertable {

  class DataSource {
  public:
    virtual ~DataSource() { return; }
    virtual bool next(uint32_t *type_flagp, int64_t *timestampp, KeySpec *keyp,
                      uint8_t **valuep, uint32_t *value_lenp,
                      uint32_t *consumedp);
  };

}

#endif // HYPERTABLE_DATASOURCE_H
