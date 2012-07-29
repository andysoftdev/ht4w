/** -*- C++ -*-
 * Copyright (C) 2011 Andy Thalmann
 *
 * This file is part of ht4w.
 *
 * ht4w is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 *
 * Hypertable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

// stub for <readline/readline.h>

#ifdef _MSC_VER
#pragma once
#endif

#ifdef _WIN32

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

inline char *readline(const char *prompt) {
  const int buflen = 0x1000;
  printf("%s", prompt);
  char *p = (char*)malloc(buflen);
  return gets_s(p, buflen);
}

#ifdef __cplusplus
}
#endif

#endif

