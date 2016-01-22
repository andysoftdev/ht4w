/** -*- C# -*-
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

namespace Hypertable.Thrift
{
    using System;
    using System.Collections;
    using System.Collections.Generic;
    using System.IO;
    using System.Text;

    using Hypertable.ThriftGen;

    public sealed class SerializedCellsReader : IEnumerable<Cell>, IDisposable
    {
        #region Static Fields

        private static readonly Encoding UTF8 = Encoding.UTF8;

        #endregion

        #region Fields

        private readonly byte[] buffer;

        private readonly BinaryReader reader;

        #endregion

        #region Constructors and Destructors

        public SerializedCellsReader(byte[] buffer)
        {
            if (buffer == null)
            {
                throw new ArgumentNullException("buffer");
            }

            this.buffer = buffer;
            this.reader = new BinaryReader(new MemoryStream(buffer));

            var version = this.reader.ReadInt32();
            if (version != SerializedCellsWriter.VERSION)
            {
                throw new InvalidDataException("SerializedCells version mismatch, expected " + SerializedCellsWriter.VERSION + ", got " + version);
            }
        }

        #endregion

        #region Public Methods and Operators

        public void Dispose()
        {
            this.reader.Dispose();
        }

        public IEnumerator<Cell> GetEnumerator()
        {
            var recentRow = string.Empty;

            while (true)
            {
                var flag = this.reader.ReadByte();

                if ((flag & (byte)SerializedCellsFlag.EOB) != 0)
                {
                    yield break;
                }

                var key = new Key();
                var cell = new Cell { Key = key };

                if ((flag & (byte)SerializedCellsFlag.HAVE_TIMESTAMP) != 0)
                {
                    key.Timestamp = this.reader.ReadInt64();

                    if ((flag & (byte)SerializedCellsFlag.REV_IS_TS) != 0)
                    {
                        key.Revision = key.Timestamp;
                    }
                }

                if ((flag & (byte)SerializedCellsFlag.HAVE_REVISION) != 0 && (flag & (byte)SerializedCellsFlag.REV_IS_TS) == 0)
                {
                    key.Revision = this.reader.ReadInt64();
                }

                // row
                var baseOffset = (int)this.reader.BaseStream.Position;
                var offset = baseOffset;
                while (this.buffer[offset++] != 0);
                if (offset - baseOffset - 1 > 0)
                {
                    recentRow = UTF8.GetString(this.buffer, baseOffset, offset - baseOffset - 1);
                }

                key.Row = recentRow;

                // column family
                baseOffset = offset;
                while (this.buffer[offset++] != 0);
                key.Column_family = offset - baseOffset - 1 > 0 ? UTF8.GetString(this.buffer, baseOffset, offset - baseOffset - 1) : null;

                // column qualifier
                baseOffset = offset;
                while (this.buffer[offset++] != 0);
                key.Column_qualifier = offset - baseOffset - 1 > 0 ? UTF8.GetString(this.buffer, baseOffset, offset - baseOffset - 1) : null;

                this.reader.BaseStream.Position = offset;

                var valueLength = this.reader.ReadInt32();
                if (valueLength < 0)
                {
                    throw new InvalidDataException("Invalid value length");
                }

                cell.Value = valueLength > 0 ? this.reader.ReadBytes(valueLength) : null;

                key.Flag = (KeyFlag)this.reader.ReadByte();

                yield return cell;
            }
        }

        #endregion

        #region Explicit Interface Methods

        IEnumerator IEnumerable.GetEnumerator()
        {
            return this.GetEnumerator();
        }

        #endregion
    }
}