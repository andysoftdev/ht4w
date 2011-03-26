/** -*- C++ -*-
 * Copyright (C) 2011 Andy Thalmann
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
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

#include "Common/Compat.h"

#ifndef _LIB

#pragma comment( lib, "sigar.lib" )
#pragma comment( lib, "log4cpp.lib" )
#pragma comment( lib, "zlib.lib" )
#pragma comment( lib, "advapi32.lib" )
#pragma comment( lib, "shell32.lib" )
#pragma comment( lib, "ws2_32.lib" )
#pragma comment( lib, "netapi32.lib" )
#pragma comment( lib, "version.lib" )


#if defined(_USE_TCMALLOC)
#pragma comment( lib, "tcmalloc.lib" )
#pragma comment( linker, "/INCLUDE:__tcmalloc")
#elif defined(_USE_HOARD)
#pragma comment( lib, "hoard.lib" )
#pragma comment( linker, "/INCLUDE:__hoardmalloc")
#endif

#endif
