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

#ifndef _WIN32
#error Platform isn't supported
#endif

#include "Common/Compat.h"
#include <Aclapi.h>
#pragma comment( lib, "Advapi32.lib" )

#include "SecurityUtils.h"
#include "Logger.h"

using namespace Hypertable;

#define WINAPI_ERROR( msg ) \
{ \
  if (Logger::logger) { \
    DWORD err = GetLastError(); \
    HT_ERRORF(msg, winapi_strerror(err)); \
    SetLastError(err); \
  } \
}

bool SecurityUtils::set_file_security_info(const char* objName, WELL_KNOWN_SID_TYPE wellKnownSid, DWORD accessRights, ACCESS_MODE accessMode, DWORD inheritance) {
  return set_security_info(objName, SE_FILE_OBJECT, wellKnownSid, accessRights, accessMode, inheritance);
}

bool SecurityUtils::set_security_info(const char* objName, SE_OBJECT_TYPE objType, WELL_KNOWN_SID_TYPE wellKnownSid, DWORD accessRights, ACCESS_MODE accessMode, DWORD inheritance) {
  bool success = false;
  DWORD sidSize = SECURITY_MAX_SID_SIZE;
  PSID sid = (PSID)LocalAlloc(LMEM_FIXED, sidSize);
  if (CreateWellKnownSid(wellKnownSid, 0, sid, &sidSize)) {
    success = set_security_info(objName, objType, sid, accessRights, accessMode, inheritance);
  }
  else
    WINAPI_ERROR("CreateWellKnownSid failed - %s");
  LocalFree(sid);
  return success;
}

bool SecurityUtils::set_security_info(const char* objName, SE_OBJECT_TYPE objType, PSID sid, DWORD accessRights, ACCESS_MODE accessMode, DWORD inheritance) {
  bool success = false;
  if (objName && *objName) {
    PSECURITY_DESCRIPTOR sd = 0;
    PACL dacl = 0;
    if (GetNamedSecurityInfo(objName, objType, DACL_SECURITY_INFORMATION, 0, 0, &dacl, 0, &sd) == ERROR_SUCCESS) {
      EXPLICIT_ACCESS ea;
      ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));
      ea.grfAccessPermissions = accessRights;
      ea.grfAccessMode = accessMode;
      ea.grfInheritance = inheritance;
      BuildTrusteeWithSid(&ea.Trustee, sid);
      PACL daclNew = 0;
      if (SetEntriesInAcl(1, &ea, dacl, &daclNew) == ERROR_SUCCESS) {
        if (SetNamedSecurityInfo(const_cast<char*>(objName), objType, DACL_SECURITY_INFORMATION, 0, 0, daclNew, 0) == ERROR_SUCCESS)
          success = true;
        else
          WINAPI_ERROR("SetNamedSecurityInfo failed - %s");
        LocalFree(daclNew);
      }
      else
        WINAPI_ERROR("SetEntriesInAcl failed - %s");
      LocalFree(sd);
    }
    else
      WINAPI_ERROR("GetNamedSecurityInfo failed - %s");
  }
  return success;
}

bool SecurityUtils::set_security_info(HANDLE handle, WELL_KNOWN_SID_TYPE wellKnownSid, DWORD accessRights) {
  bool success = false;
  DWORD sidSize = SECURITY_MAX_SID_SIZE;
  PSID sid = (PSID)LocalAlloc(LMEM_FIXED, sidSize);
  if (CreateWellKnownSid(wellKnownSid, 0, sid, &sidSize)) {
    success = set_security_info(handle, sid, accessRights);
  }
  else
    WINAPI_ERROR("CreateWellKnownSid failed - %s");
  LocalFree(sid);
  return success;
}

bool SecurityUtils::set_security_info(HANDLE handle, PSID sid, DWORD accessRights) {
  bool success = false;
  // obtain security descriptor
  DWORD len = 0;
  if (!GetKernelObjectSecurity(handle, DACL_SECURITY_INFORMATION, 0, 0, &len) && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    PSECURITY_DESCRIPTOR psd = LocalAlloc(LMEM_FIXED, len);
    if (GetKernelObjectSecurity(handle, DACL_SECURITY_INFORMATION, psd, len, &len)) {
      DWORD sidSize = GetLengthSid(sid);
      // retrieve the DACL
      PACL dacl = 0;
      BOOL daclPresent = FALSE;
      BOOL daclDefaulted = TRUE;
      if (GetSecurityDescriptorDacl(psd, &daclPresent, &dacl, &daclDefaulted)) {
        PACL daclNew = 0;
        bool daclModified = false;
        if (dacl) {
          ACL_SIZE_INFORMATION aclsizeinfo;
          if (GetAclInformation(dacl, &aclsizeinfo, sizeof(aclsizeinfo), AclSizeInformation)) {
            for (DWORD cAce = 0; cAce < aclsizeinfo.AceCount && !daclModified; ++cAce) {
              ACCESS_ALLOWED_ACE* oldAce = 0;
              if (GetAce(dacl, cAce, (LPVOID*)&oldAce)) {
                if (EqualSid((PSID)&oldAce->SidStart, sid)) {
                  DWORD aceSize = sizeof(ACCESS_ALLOWED_ACE) + sidSize - sizeof(DWORD);
                  ACCESS_ALLOWED_ACE* newAce = (ACCESS_ALLOWED_ACE*) LocalAlloc(LMEM_FIXED, aceSize);
                  memset(newAce, 0, aceSize);
                  newAce->Header.AceType = ACCESS_ALLOWED_ACE_TYPE;
                  newAce->Header.AceSize = (WORD)aceSize;
                  newAce->Mask = oldAce->Mask | accessRights;
                  if (CopySid(sidSize, (PSID)&newAce->SidStart, (PSID)&oldAce->SidStart)) {
                    if (DeleteAce(dacl, cAce)) {
                      if (AddAce(dacl, ACL_REVISION, cAce, newAce, aceSize)) {
                        daclModified = true;
                      }
                      else
                        WINAPI_ERROR("AddAce failed - %s");
                    }
                    else
                      WINAPI_ERROR("DeleteAce failed - %s");
                  }
                  else
                    WINAPI_ERROR("CopySid failed - %s");
                  LocalFree(newAce);
                }
              }
              else
                WINAPI_ERROR("GetAce failed - %s");
            }
          }
          else
            WINAPI_ERROR("GetAclInformation failed - %s");
        }
        // if not modified add a new ACE
        if(!daclModified) {
          DWORD aceSize = sizeof(ACCESS_ALLOWED_ACE) + sidSize - sizeof(DWORD);
          daclNew = (PACL) LocalAlloc(LMEM_FIXED, (dacl ? dacl->AclSize : 0) + aceSize);
          if (dacl)
            memcpy(daclNew, dacl, dacl->AclSize);
          daclNew->AclSize += aceSize;
          if (AddAccessAllowedAce(daclNew, ACL_REVISION, accessRights, sid))
            dacl = daclNew;
          else
            WINAPI_ERROR("AddAccessAllowedAce failed - %s");
        }
        SECURITY_DESCRIPTOR sdNew;
        if (InitializeSecurityDescriptor(&sdNew, SECURITY_DESCRIPTOR_REVISION)) {
          if (SetSecurityDescriptorDacl(&sdNew, TRUE, dacl, FALSE)) {
            if (SetKernelObjectSecurity(handle, DACL_SECURITY_INFORMATION, &sdNew))
              success = true;
            else
              WINAPI_ERROR("SetKernelObjectSecurity failed - %s");
          }
          else
            WINAPI_ERROR("SetSecurityDescriptorDacl failed - %s");
        }
        else
          WINAPI_ERROR("InitializeSecurityDescriptor failed - %s");
        if (daclNew)
          LocalFree(daclNew);
      }
      else
        WINAPI_ERROR("GetSecurityDescriptorDacl failed - %s");
    }
    else
      WINAPI_ERROR("GetKernelObjectSecurity failed - %s");
  }
  else
    WINAPI_ERROR("GetKernelObjectSecurity failed - %s")
  return success;
}