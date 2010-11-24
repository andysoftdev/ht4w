/** -*- c++ -*-
 * Copyright (C) 2008 Luke Lu (Zvents, Inc.)
 *
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
#include "MasterMetaLog.h"

using namespace std;
using namespace Hypertable;

MasterMetaLog::MasterMetaLog(Filesystem *fs, const String &path)
    : MetaLogDfsBase(fs, path) {
  // TODO
}

void
MasterMetaLog::write(MetaLogEntry *e) {
  // TODO
}

void
MasterMetaLog::close() {
  // TODO
}

void
MasterMetaLog::purge(const MasterStates &) {
}
