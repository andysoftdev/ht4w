/** -*- C++ -*-
 * Copyright (C) 2010-2016 Thalmann Software & Consulting, http://www.softdev.ch
 *
 * This file is part of ht4w.
 *
 * ht4w is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
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

#ifndef HYPERTABLE_SECURITYUTILS_H
#define HYPERTABLE_SECURITYUTILS_H

#ifndef _WIN32
#error Platform isn't supported
#endif

#include <AccCtrl.h>

namespace Hypertable {

  class SecurityUtils {
  public:
    static bool set_file_security_info(const char* objName, WELL_KNOWN_SID_TYPE wellKnownSid, DWORD accessRights, ACCESS_MODE accessMode = SET_ACCESS, DWORD inheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT);
    
    static bool set_security_info(const char* objName, SE_OBJECT_TYPE objType, WELL_KNOWN_SID_TYPE wellKnownSid, DWORD accessRights, ACCESS_MODE accessMode = SET_ACCESS, DWORD inheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT);
    static bool set_security_info(const char* objName, SE_OBJECT_TYPE objType, PSID sid, DWORD accessRights, ACCESS_MODE accessMode = SET_ACCESS, DWORD inheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT);
    
    static bool set_security_info(HANDLE handle, WELL_KNOWN_SID_TYPE wellKnownSid, DWORD accessRights);
    static bool set_security_info(HANDLE handle, PSID sid, DWORD accessRights);
  };

}

#endif // HYPERTABLE_SECURITYUTILS_H
