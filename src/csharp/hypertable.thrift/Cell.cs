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

namespace Hypertable.ThriftGen
{
    using System;

    public partial class Cell
    {
        #region Static Fields

        private static readonly DateTime timestampOrigin = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc);

        #endregion

        #region Public Properties

        public static DateTime TimestampOrigin
        {
            get
            {
                return timestampOrigin;
            }
        }

        #endregion

        #region Public Methods and Operators

        public static DateTime ToDateTime(long timestamp)
        {
            if (timestamp <= 0)
            {
                throw new ArgumentException("Invalid timestamp");
            }

            return timestampOrigin + TimeSpan.FromTicks(timestamp / 100);
        }

        public static long ToTimestamp(DateTime dateTime)
        {
            if (dateTime < timestampOrigin)
            {
                throw new ArgumentException("Invalid DateTime");
            }

            if (dateTime.Kind == DateTimeKind.Unspecified)
            {
                throw new ArgumentException("Unspecified DateTime Kind");
            }

            return (dateTime.ToUniversalTime() - timestampOrigin).Ticks * 100;
        }

        #endregion
    }
}