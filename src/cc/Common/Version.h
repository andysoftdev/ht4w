/**
 * Copyright (C) 2007-2012 Hypertable, Inc.
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or any later version.
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

#ifndef HYPERTABLE_VERSION_H
#define HYPERTABLE_VERSION_H

#include <cstdio>

// version macros for detecting header/library mismatch
#define HT_VERSION_MAJOR        0
#define HT_VERSION_MINOR        9
#define HT_VERSION_MICRO        7
#define HT_VERSION_PATCH        6
#define HT_VERSION_MISC_SUFFIX  ""
#define HT_VERSION_STRING       "0.9.7.6"

namespace Hypertable {
  extern const int version_major;
  extern const int version_minor;
  extern const int version_micro;
  extern const int version_patch;
  extern const std::string version_misc_suffix;
  extern const char *version_string();

  // must be inlined for version check
  inline void check_version() {
    if (version_major != HT_VERSION_MAJOR ||
        version_minor != HT_VERSION_MINOR ||
        version_micro != HT_VERSION_MICRO ||
        version_patch != HT_VERSION_PATCH ||
        version_misc_suffix != HT_VERSION_MISC_SUFFIX) {
      std::fprintf(stderr, "Hypertable header/library version mismatch:\n"
                   " header: %s\nlibrary: %s\n", HT_VERSION_STRING, version_string());
      exit(1);
    }
  }
}

#endif // HYPERTABLE_VERSION_H
