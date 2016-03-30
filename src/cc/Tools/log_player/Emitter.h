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
 * Declarations for Emitter.
 * This file contains declarations for Emitter, an abstract base class
 * for emitting key/value pairs read from a commit log.
 */

#ifndef HYPERTABLE_EMITTER_H
#define HYPERTABLE_EMITTER_H

#include <Hypertable/Lib/Key.h>
#include <Hypertable/Lib/TableIdentifier.h>

namespace Hypertable {

  class Emitter {
  public:
    virtual ~Emitter() { }
    virtual void add(TableIdentifier &table, Key &key,
                     const uint8_t *value, size_t value_len) = 0;
  };

} // namespace Hypertable

#endif // HYPERTABLE_EMITTER_H
