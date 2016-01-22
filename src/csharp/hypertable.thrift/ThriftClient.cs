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

    using global::Thrift.Protocol;
    using global::Thrift.Transport;

    using Hypertable.ThriftGen;

    public sealed class ThriftClient : HqlService.Client
    {
        #region Static Fields

        private static int defaultPort = 15867;

        private static TimeSpan defaultTimeout = TimeSpan.FromMilliseconds(10000);

        #endregion

        #region Fields

        private readonly object syncRoot = new object();

        private readonly TTransport transport;

        private bool opened;

        #endregion

        #region Constructors and Destructors

        private ThriftClient(TTransport transport, TProtocol protocol)
            : base(protocol)
        {
            if (transport == null)
            {
                throw new ArgumentNullException("transport");
            }

            if (protocol == null)
            {
                throw new ArgumentNullException("protocol");
            }

            this.transport = transport;
        }

        #endregion

        #region Public Properties

        public static int DefaultPort
        {
            get
            {
                return defaultPort;
            }
            set
            {
                defaultPort = value;
            }
        }

        public static TimeSpan DefaultTimeout
        {
            get
            {
                return defaultTimeout;
            }
            set
            {
                defaultTimeout = value;
            }
        }

        public bool IsOpen
        {
            get
            {
                lock (this.syncRoot)
                {
                    return this.opened;
                }
            }
        }

        #endregion

        #region Public Methods and Operators

        public static ThriftClient Create(String host)
        {
            return Create(host, defaultPort, defaultTimeout);
        }

        public static ThriftClient Create(String host, int port)
        {
            return Create(host, port, defaultTimeout);
        }

        public static ThriftClient Create(String host, int port, TimeSpan timeout)
        {
            var transport = new TFramedTransport(new TSocket(host, port, timeout.Milliseconds));
            return new ThriftClient(transport, new TBinaryProtocol(transport));
        }

        public void Close()
        {
            lock (this.syncRoot)
            {
                if (this.opened)
                {
                    this.transport.Close();
                    this.opened = false;
                }
            }
        }

        public void Open()
        {
            lock (this.syncRoot)
            {
                if (!this.opened)
                {
                    this.transport.Open();
                    this.opened = true;
                }
            }
        }

        #endregion
    }
}