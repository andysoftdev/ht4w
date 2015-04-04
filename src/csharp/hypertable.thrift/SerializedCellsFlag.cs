/** -*- C# -*-
 * Copyright (C) 2010-2015 Thalmann Software & Consulting, http://www.softdev.ch
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

namespace Hypertable.Thrift
{
    using System;

    [Flags]
    internal enum SerializedCellsFlag : byte {
      EOB                       = 0x01,
      EOS                       = 0x02,
      FLUSH                     = 0x04,
      REV_IS_TS                 = 0x10,
      AUTO_TIMESTAMP            = 0x20,
      HAVE_TIMESTAMP            = 0x40,
      HAVE_REVISION             = 0x80
    }

}