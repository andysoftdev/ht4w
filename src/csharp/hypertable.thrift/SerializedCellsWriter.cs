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
    using System.Collections.Generic;
    using System.IO;
    using System.Linq;
    using System.Text;

    using Hypertable.ThriftGen;

    public sealed class SerializedCellsWriter : IDisposable
    {
        #region Constants

        public const long AUTO_ASSIGN = long.MinValue + 2;

        public const long NULL = long.MinValue + 1;

        public const int VERSION = 0x01;

        #endregion

        #region Static Fields

        private static readonly Encoding UTF8 = Encoding.UTF8;

        #endregion

        #region Fields

        private readonly MemoryStream buffer;

        private readonly BinaryWriter writer;

        private bool finalized;

        private string recentRow = string.Empty;

        #endregion

        #region Constructors and Destructors

        public SerializedCellsWriter()
        {
            this.buffer = new MemoryStream(512);
            this.writer = new BinaryWriter(this.buffer);
            this.writer.Write(VERSION);
        }

        public SerializedCellsWriter(int size)
        {
            if (size < 0)
            {
                throw new ArgumentException("Invalid size", "size");
            }

            this.buffer = new MemoryStream(size + 5);
            this.writer = new BinaryWriter(this.buffer);
            this.writer.Write(VERSION);
        }

        #endregion

        #region Public Properties

        public int Capacity
        {
            get
            {
                return this.buffer.Capacity;
            }

            set
            {
                this.buffer.Capacity = value;
            }
        }

        public bool IsEmpty
        {
            get
            {
                return this.buffer.Position == 0;
            }
        }

        #endregion

        #region Public Methods and Operators

        public bool Add(Cell cell)
        {
            if (cell == null)
            {
                throw new ArgumentNullException("cell");
            }

            return this.Add(cell.Key, cell.Value);
        }

        public int Add(IEnumerable<Cell> cells)
        {
            if (cells == null)
            {
                throw new ArgumentNullException("cells");
            }

            return cells.Count(cell => cell != null && this.Add(cell));
        }

        public bool Add(Key key, byte[] value)
        {
            if (key == null)
            {
                throw new ArgumentNullException("key");
            }

            return this.Add(
                key.Row,
                key.Column_family,
                key.Column_qualifier,
                key.__isset.timestamp ? key.Timestamp : AUTO_ASSIGN,
                value,
                key.__isset.flag ? key.Flag : KeyFlag.INSERT);
        }

        public bool Add(string row, string columnFamily, string columnQualifier, byte[] value)
        {
            return this.Add(row, columnFamily, columnQualifier, AUTO_ASSIGN, value);
        }

        public bool Add(string row, string columnFamily, string columnQualifier, long timestamp, byte[] value)
        {
            return this.Add(row, columnFamily, columnQualifier, timestamp, value, KeyFlag.INSERT);
        }

        public bool Add(string row, string columnFamily, string columnQualifier, long timestamp, byte[] value, KeyFlag flag)
        {
            if (row == null)
            {
                throw new ArgumentNullException("row");
            }

            var rowBytes = UTF8.GetBytes(row);
            var columnFamilyBytes = columnFamily != null ? UTF8.GetBytes(columnFamily) : null;
            var columnQualifierBytes = columnQualifier != null ? UTF8.GetBytes(columnQualifier) : null;

            return this.Add(
                rowBytes,
                0,
                rowBytes.Length,
                columnFamilyBytes,
                0,
                columnFamilyBytes != null ? columnFamilyBytes.Length : 0,
                columnQualifierBytes,
                0,
                columnQualifierBytes != null ? columnQualifierBytes.Length : 0,
                timestamp,
                value,
                0,
                value != null ? value.Length : 0,
                flag);
        }

        public bool Add(byte[] row, byte[] columnFamily, byte[] columnQualifier, byte[] value)
        {
            return this.Add(row, columnFamily, columnQualifier, AUTO_ASSIGN, value);
        }

        public bool Add(byte[] row, byte[] columnFamily, byte[] columnQualifier, long timestamp, byte[] value)
        {
            return this.Add(
                row,
                0,
                row.Length,
                columnFamily,
                0,
                columnFamily != null ? columnFamily.Length : 0,
                columnQualifier,
                0,
                columnQualifier != null ? columnQualifier.Length : 0,
                timestamp,
                value,
                0,
                value != null ? value.Length : 0,
                KeyFlag.INSERT);
        }

        public bool Add(
            byte[] row,
            int rowOffset,
            int rowLength,
            byte[] columnFamily,
            int columnFamilyOffset,
            int columnFamilyLength,
            byte[] columnQualifier,
            int columnQualifierOffset,
            int columnQualifierLength,
            long timestamp,
            byte[] value,
            int valueOffset,
            int valueLength,
            KeyFlag flag)
        {
            if (row == null)
            {
                throw new ArgumentNullException("row");
            }

            this.TrowIfFinalized();

            var length = 9 + rowLength + columnFamilyLength + columnQualifierLength + valueLength;
            byte control = 0;

            if (timestamp == AUTO_ASSIGN)
            {
                control |= (byte)SerializedCellsFlag.AUTO_TIMESTAMP;
            }
            else if (timestamp != NULL)
            {
                control |= (byte)SerializedCellsFlag.HAVE_TIMESTAMP;
                length += 8;
            }

            // need to leave room for the termination byte
            if (length >= this.buffer.Remaining())
            {
                if (this.buffer.Position > 0)
                {
                    this.buffer.Capacity = (this.buffer.Capacity + length) * 3 / 2;
                }
                else
                {
                    this.buffer.Capacity = length + 5;
                    this.writer.Write(VERSION);
                }
            }

            // control byte
            this.writer.Write(control);

            // timestamp
            if ((control & (byte)SerializedCellsFlag.HAVE_TIMESTAMP) != 0)
            {
                this.writer.Write(timestamp);
            }

            if ((control & (byte)SerializedCellsFlag.HAVE_REVISION) != 0 && (control & (byte)SerializedCellsFlag.REV_IS_TS) == 0)
            {
                this.writer.Write((long)0);
            }

            var newRow = UTF8.GetString(row, rowOffset, rowLength);

            if (!this.recentRow.Equals(newRow))
            {
                this.recentRow = newRow;
                this.writer.Write(row, rowOffset, rowLength);
            }
            this.writer.Write((byte)0);

            // column family
            if (columnFamilyLength > 0)
            {
                this.writer.Write(columnFamily, columnFamilyOffset, columnFamilyLength);
            }

            this.writer.Write((byte)0);

            // column qualifier
            if (columnQualifierLength > 0)
            {
                this.writer.Write(columnQualifier, columnQualifierOffset, columnQualifierLength);
            }

            this.writer.Write((byte)0);

            if (value == null)
            {
                this.writer.Write(0);
            }
            else
            {
                this.writer.Write(valueLength); // fix me: should be zero-compressed
                this.writer.Write(value, valueOffset, valueLength);
            }

            this.writer.Write((byte)flag);

            return true;
        }

        public bool Add(byte[] serializedCells)
        {
            if (serializedCells == null)
            {
                throw new ArgumentNullException("serializedCells");
            }

            var length = serializedCells.Length - 5; // skip 4-byte version and 1-byte terminator

            // need to leave room for the termination byte
            if (length >= this.buffer.Remaining())
            {
                if (this.buffer.Position > 0)
                {
                    this.buffer.Capacity = (this.buffer.Capacity + length) * 3 / 2;
                }
                else
                {
                    this.buffer.Capacity = length + 5;
                }
            }

            this.writer.Write(serializedCells, 4, length);
            return true;
        }

        public bool AddDelete(string row, long timestamp)
        {
            if (row == null)
            {
                throw new ArgumentNullException("row");
            }

            var rowBytes = UTF8.GetBytes(row);
            return this.Add(rowBytes, 0, rowBytes.Length, null, 0, 0, null, 0, 0, timestamp, null, 0, 0, KeyFlag.DELETE_ROW);
        }

        public bool AddDelete(string row, string columnFamily, long timestamp)
        {
            if (row == null)
            {
                throw new ArgumentNullException("row");
            }

            var rowBytes = UTF8.GetBytes(row);
            var columnFamilyBytes = columnFamily != null ? UTF8.GetBytes(columnFamily) : null;

            return this.Add(
                rowBytes,
                0,
                rowBytes.Length,
                columnFamilyBytes,
                0,
                columnFamily != null ? columnFamilyBytes.Length : 0,
                null,
                0,
                0,
                timestamp,
                null,
                0,
                0,
                KeyFlag.DELETE_CF);
        }

        public bool AddDelete(string row, string columnFamily, string columnQualifier, long timestamp)
        {
            if (row == null)
            {
                throw new ArgumentNullException("row");
            }

            var rowBytes = UTF8.GetBytes(row);
            var columnFamilyBytes = columnFamily != null ? UTF8.GetBytes(columnFamily) : null;
            var columnQualifierBytes = columnQualifier != null ? UTF8.GetBytes(columnQualifier) : null;

            return this.Add(
                rowBytes,
                0,
                rowBytes.Length,
                columnFamilyBytes,
                0,
                columnFamily != null ? columnFamilyBytes.Length : 0,
                columnQualifierBytes,
                0,
                columnQualifierBytes != null ? columnQualifierBytes.Length : 0,
                timestamp,
                null,
                0,
                0,
                KeyFlag.DELETE_CELL);
        }

        public bool AddDelete(byte[] row, long timestamp)
        {
            if (row == null)
            {
                throw new ArgumentNullException("row");
            }

            return this.Add(row, 0, row.Length, null, 0, 0, null, 0, 0, timestamp, null, 0, 0, KeyFlag.DELETE_ROW);
        }

        public bool AddDelete(byte[] row, byte[] columnFamily, long timestamp)
        {
            if (row == null)
            {
                throw new ArgumentNullException("row");
            }

            return this.Add(row, 0, row.Length, columnFamily, 0, columnFamily != null ? columnFamily.Length : 0, null, 0, 0, timestamp, null, 0, 0, KeyFlag.DELETE_CF);
        }

        public bool AddDelete(byte[] row, byte[] columnFamily, byte[] columnQualifier, long timestamp)
        {
            return this.Add(
                row,
                0,
                row.Length,
                columnFamily,
                0,
                columnFamily != null ? columnFamily.Length : 0,
                columnQualifier,
                0,
                columnQualifier != null ? columnQualifier.Length : 0,
                timestamp,
                null,
                0,
                0,
                KeyFlag.DELETE_CELL);
        }

        public bool AddDelete(byte[] row, int rowOffset, int rowLength, long timestamp)
        {
            return this.Add(row, rowOffset, rowLength, null, 0, 0, null, 0, 0, timestamp, null, 0, 0, KeyFlag.DELETE_ROW);
        }

        public bool AddDelete(byte[] row, int rowOffset, int rowLength, byte[] columnFamily, int columnFamilyOffset, int columnFamilyLength, long timestamp)
        {
            return this.Add(row, rowOffset, rowLength, columnFamily, columnFamilyOffset, columnFamilyLength, null, 0, 0, timestamp, null, 0, 0, KeyFlag.DELETE_CF);
        }

        public bool AddDelete(
            byte[] row,
            int rowOffset,
            int rowLength,
            byte[] columnFamily,
            int columnFamilyOffset,
            int columnFamilyLength,
            byte[] columnQualifier,
            int columnQualifierOffset,
            int columnQualifierLength,
            long timestamp)
        {
            return this.Add(
                row,
                rowOffset,
                rowLength,
                columnFamily,
                columnFamilyOffset,
                columnFamilyLength,
                columnQualifier,
                columnQualifierOffset,
                columnQualifierLength,
                timestamp,
                null,
                0,
                0,
                KeyFlag.DELETE_CELL);
        }

        public void Clear()
        {
            this.buffer.SetLength(0);
            this.writer.Write(VERSION);
            this.finalized = false;
            this.recentRow = string.Empty;
        }

        public void Dispose()
        {
            this.writer.Dispose();
        }

        public void Finalize(byte flag)
        {
            this.writer.Write((byte)SerializedCellsFlag.EOB | flag);
            this.finalized = true;
        }

        public byte[] ToArray()
        {
            if (!this.finalized)
            {
                this.Finalize((byte)SerializedCellsFlag.EOB);
            }

            this.writer.Flush();
            var array = this.buffer.ToArray();
            this.Clear();
            return array;
        }

        #endregion

        #region Methods

        private void TrowIfFinalized()
        {
            if (this.finalized)
            {
                throw new InvalidOperationException("SerializedCellsWriter have been already finalized");
            }
        }

        #endregion
    }
}