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

#include "Common/Compat.h"

#ifdef _WIN32

#include "Logger.h"

using namespace Hypertable;

const char* winapi_strerror(DWORD err) {
	static DWORD idx = TLS_OUT_OF_INDEXES;
	if( idx == TLS_OUT_OF_INDEXES ) {
		idx = ::TlsAlloc();
	}
	HT_ASSERT(idx != TLS_OUT_OF_INDEXES);
	char* lpBuffer = (char*)TlsGetValue(idx);
	if( lpBuffer ) {
		::LocalFree(lpBuffer);
	}
	lpBuffer = 0;

	DWORD len = ::FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			0, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR) &lpBuffer, 0, NULL );

	if( len == 0 ) {
		lpBuffer = (char*)::LocalAlloc(LMEM_FIXED, 0x100);
		sprintf( lpBuffer, "unknown error %d", err );
	}
	else {
		while( len > 0 && lpBuffer[len-1] < ' ' ) {
			lpBuffer[--len] = '\0';
		}
	}
	::TlsSetValue( idx, lpBuffer );
	return lpBuffer;
}

#endif