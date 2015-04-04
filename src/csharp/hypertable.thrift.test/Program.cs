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

namespace Hypertable.Thrift.Test
{
    using System;
    using System.Collections.Generic;
    using System.Text;
    using System.Threading;

    using Hypertable.ThriftGen;

    internal static class Program
    {
        #region Methods

        private static int Main()
        {
            ThriftClient client = null;
            long ns = -1;
            try
            {
                client = ThriftClient.Create("localhost");
                client.Open();

                if (!client.namespace_exists("test"))
                {
                    client.create_namespace("test");
                }

                ns = client.namespace_open("test");
                client.hql_query(ns, "drop table if exists thrift_test");
                client.hql_query(ns, "create table thrift_test ( col )");

                // HQL examples
                show(client.hql_query(ns, "show tables").ToString());
                show(client.hql_query(ns, "select * from thrift_test").ToString());

                // Schema example
                var schema = new Schema();
                schema = client.table_get_schema(ns, "thrift_test");

                show("Access groups:");
                foreach (var name in schema.Access_groups.Keys)
                {
                    show("\t" + name);
                }

                show("Column families:");
                foreach (var name in schema.Column_families.Keys)
                {
                    show("\t" + name);
                }

                // mutator examples
                var mutator = client.mutator_open(ns, "thrift_test", 0, 0);

                try
                {
                    var cell = new Cell();
                    var key = new Key { Row = "java-k1", Column_family = "col" };
                    cell.Key = key;
                    var vtmp = "java-v1";
                    cell.Value = Encoding.UTF8.GetBytes(vtmp);
                    client.mutator_set_cell(mutator, cell);
                }
                finally
                {
                    client.mutator_close(mutator);
                }

                // shared mutator example
                {
                    var mutate_spec = new MutateSpec { Appname = "test-java", Flush_interval = 1000 };
                    var cell = new Cell();

                    var key = new Key { Row = "java-put1", Column_family = "col" };
                    cell.Key = key;
                    var vtmp = "java-put-v1";
                    cell.Value = Encoding.UTF8.GetBytes(vtmp);
                    client.offer_cell(ns, "thrift_test", mutate_spec, cell);

                    key = new Key { Row = "java-put2", Column_family = "col" };
                    cell.Key = key;
                    vtmp = "java-put-v2";
                    cell.Value = Encoding.UTF8.GetBytes(vtmp);
                    client.shared_mutator_refresh(ns, "thrift_test", mutate_spec);
                    client.shared_mutator_set_cell(ns, "thrift_test", mutate_spec, cell);
                    Thread.Sleep(2000);
                }

                // scanner examples
                show("Full scan");
                var scanSpec = new ScanSpec(); // empty scan spec select all
                var scanner = client.scanner_open(ns, "thrift_test", scanSpec);

                try
                {
                    var cells = client.scanner_get_cells(scanner);

                    while (cells.Count > 0)
                    {
                        foreach (var cell in cells)
                        {
                            var s = Encoding.UTF8.GetString(cell.Value);
                            show(s);
                        }
                        cells = client.scanner_get_cells(scanner);
                    }
                }
                finally
                {
                    client.scanner_close(scanner);
                }

                show("Full scan serialized cells");
                scanSpec = new ScanSpec(); // empty scan spec select all
                scanner = client.scanner_open(ns, "thrift_test", scanSpec);

                try
                {
                    var cells = client.scanner_get_cells_serialized(scanner);
                    using (var reader = new SerializedCellsReader(cells))
                    {
                        foreach (var cell in reader)
                        {
                            var s = Encoding.UTF8.GetString(cell.Value);
                            show(s);
                        }
                    }
                }
                finally
                {
                    client.scanner_close(scanner);
                }

                // restricted scanspec
                scanSpec.Columns = new List<string> { "col:/^.*$/" };
                scanSpec.Row_regexp = "java.*";
                scanSpec.Value_regexp = "v2";
                scanner = client.scanner_open(ns, "thrift_test", scanSpec);
                show("Restricted scan");
                try
                {
                    var cells = client.scanner_get_cells(scanner);

                    while (cells.Count > 0)
                    {
                        foreach (var cell in cells)
                        {
                            var s = Encoding.UTF8.GetString(cell.Value);
                            show(s);
                        }
                        cells = client.scanner_get_cells(scanner);
                    }
                }
                finally
                {
                    client.scanner_close(scanner);
                }

                // asynchronous api
                long future = 0;
                long mutator_async_1 = 0;
                long mutator_async_2 = 0;
                long color_scanner = 0;
                long location_scanner = 0;
                long energy_scanner = 0;
                const int expected_cells = 6;
                var num_cells = 0;

                try
                {
                    show("Asynchronous mutator");
                    future = client.future_open(0);

                    mutator_async_1 = client.async_mutator_open(ns, "thrift_test", future, 0);
                    mutator_async_2 = client.async_mutator_open(ns, "thrift_test", future, 0);

                    var cell = new Cell();

                    var key = new Key { Row = "java-put1", Column_family = "col" };
                    cell.Key = key;
                    var vtmp = "java-async-put-v1";
                    cell.Value = Encoding.UTF8.GetBytes(vtmp);
                    client.async_mutator_set_cell(mutator_async_1, cell);

                    key = new Key { Row = "java-put2", Column_family = "col" };
                    cell.Key = key;
                    vtmp = "java-async-put-v2";
                    cell.Value = Encoding.UTF8.GetBytes(vtmp);
                    client.async_mutator_set_cell(mutator_async_2, cell);

                    client.async_mutator_flush(mutator_async_1);
                    client.async_mutator_flush(mutator_async_2);

                    var num_flushes = 0;
                    while (true)
                    {
                        var result = client.future_get_result(future, 0);
                        if (result.Is_empty || result.Is_error || result.Is_scan)
                        {
                            break;
                        }

                        num_flushes++;
                    }

                    if (num_flushes > 2)
                    {
                        show("ERROR: Expected 2 flushes, received " + num_flushes);
                        return 1;
                    }

                    if (client.future_is_cancelled(future) || client.future_is_full(future) || !client.future_is_empty(future) || client.future_has_outstanding(future))
                    {
                        show("ERROR: Future object in unexpected state");
                        return 1;
                    }
                }
                finally
                {
                    client.async_mutator_close(mutator_async_1);
                    client.async_mutator_close(mutator_async_2);
                }

                client.hql_query(ns, "drop table if exists FruitColor");
                client.hql_query(ns, "drop table if exists FruitLocation");
                client.hql_query(ns, "drop table if exists FruitEnergy");

                client.hql_query(ns, "create table FruitColor(data)");
                client.hql_query(ns, "create table FruitLocation(data)");
                client.hql_query(ns, "create table FruitEnergy(data)");

                //  writes
                var color_mutator = client.async_mutator_open(ns, "FruitColor", future, 0);
                var location_mutator = client.async_mutator_open(ns, "FruitLocation", future, 0);
                var energy_mutator = client.async_mutator_open(ns, "FruitEnergy", future, 0);

                try
                {
                    using (var color = new SerializedCellsWriter())
                    {
                        color.Add("apple", "data", null, Encoding.UTF8.GetBytes("red"));
                        color.Add("kiwi", "data", null, Encoding.UTF8.GetBytes("brown"));
                        color.Add("pomegranate", "data", null, Encoding.UTF8.GetBytes("pink"));
                        client.async_mutator_set_cells_serialized(color_mutator, color.ToArray(), false);
                    }

                    using (var location = new SerializedCellsWriter())
                    {
                        location.Add("apple", "data", null, Encoding.UTF8.GetBytes("Western Asia"));
                        location.Add("kiwi", "data", null, Encoding.UTF8.GetBytes("Southern China"));
                        location.Add("pomegranate", "data", null, Encoding.UTF8.GetBytes("Iran"));
                        client.async_mutator_set_cells_serialized(energy_mutator, location.ToArray(), false);
                    }


                    using (var energy = new SerializedCellsWriter())
                    {
                        energy.Add("apple", "data", null, Encoding.UTF8.GetBytes("2.18kJ/g"));
                        energy.Add("kiwi", "data", null, Encoding.UTF8.GetBytes("0.61Cal/g"));
                        energy.Add("pomegranate", "data", null, Encoding.UTF8.GetBytes("0.53Cal/g"));
                        client.async_mutator_set_cells_serialized(location_mutator, energy.ToArray(), false);
                    }
                }
                finally
                {
                    client.async_mutator_close(color_mutator);
                    client.async_mutator_close(location_mutator);
                    client.async_mutator_close(energy_mutator);
                }

                try
                {
                    show("Asynchronous scan");
                    var ss = new ScanSpec();
                    color_scanner = client.async_scanner_open(ns, "FruitColor", future, ss);
                    location_scanner = client.async_scanner_open(ns, "FruitLocation", future, ss);
                    energy_scanner = client.async_scanner_open(ns, "FruitEnergy", future, ss);
                    while (true)
                    {
                        var result = client.future_get_result(future, 250);

                        if (result.Is_empty || result.Is_error)
                        {
                            break;
                        }

                        if (result.Is_scan)
                        {
                            foreach (var cell in result.Cells)
                            {
                                var s = Encoding.UTF8.GetString(cell.Value);
                                show(s);
                                num_cells++;
                            }

                            if (num_cells >= 6)
                            {
                                client.future_cancel(future);
                                break;
                            }
                        }
                    }

                    if (!client.future_is_cancelled(future))
                    {
                        show("ERROR: Expected future object to be cancelled");
                        return 1;
                    }
                }
                finally
                {
                    client.async_scanner_close(color_scanner);
                    client.async_scanner_close(location_scanner);
                    client.async_scanner_close(energy_scanner);
                }

                if (num_cells != 6)
                {
                    show("ERROR: Expected " + expected_cells + " cells got " + num_cells);
                    return 1;
                }

                try
                {
                    show("Asynchronous scan with serialized results");
                    var ss = new ScanSpec();
                    color_scanner = client.async_scanner_open(ns, "FruitColor", future, ss);
                    location_scanner = client.async_scanner_open(ns, "FruitLocation", future, ss);
                    energy_scanner = client.async_scanner_open(ns, "FruitEnergy", future, ss);
                    while (true)
                    {
                        var result = client.future_get_result_serialized(future, 250);

                        if (result.Is_empty || result.Is_error)
                        {
                            break;
                        }

                        if (result.Is_scan)
                        {
                            using (var reader = new SerializedCellsReader(result.Cells))
                            {
                                foreach (var cell in reader)
                                {
                                    var s = Encoding.UTF8.GetString(cell.Value);
                                    show(s);
                                }
                            }
                        }
                    }
                }
                finally
                {
                    client.async_scanner_close(color_scanner);
                    client.async_scanner_close(location_scanner);
                    client.async_scanner_close(energy_scanner);
                }

                client.future_close(future);

                client.hql_query(ns, "drop table if exists FruitColor");
                client.hql_query(ns, "drop table if exists FruitLocation");
                client.hql_query(ns, "drop table if exists FruitEnergy");

                // issue 497
                try
                {
                    client.hql_query(ns, "drop table if exists java_thrift_test");
                    client.hql_query(ns, "create table java_thrift_test ( c1, c2, c3 )");

                    mutator = client.mutator_open(ns, "java_thrift_test", 0, 0);

                    var cell = new Cell();
                    var key = new Key { Row = "000", Column_family = "c1", Column_qualifier = "test" };
                    cell.Key = key;
                    var str = "foo";
                    cell.Value = Encoding.UTF8.GetBytes(str);
                    client.mutator_set_cell(mutator, cell);

                    cell = new Cell();
                    key = new Key { Row = "000", Column_family = "c1" };
                    cell.Key = key;
                    str = "bar";
                    cell.Value = Encoding.UTF8.GetBytes(str);
                    client.mutator_set_cell(mutator, cell);

                    client.mutator_close(mutator);

                    var result = client.hql_query(ns, "select * from java_thrift_test");
                    var cells = result.Cells;
                    var qualifier_count = 0;
                    foreach (var c in cells)
                    {
                        if (c.Key.__isset.column_qualifier && c.Key.Column_qualifier.Length == 0)
                        {
                            qualifier_count++;
                        }
                    }

                    if (qualifier_count != 1)
                    {
                        show("ERROR: Expected qualifier_count of 1, got " + qualifier_count);
                        client.namespace_close(ns);
                        return 1;
                    }
                }
                finally
                {
                    client.hql_query(ns, "drop table if exists java_thrift_test");
                }
            }
            catch (Exception e)
            {
                show(e.ToString());
                try
                {
                    if (client != null && ns != -1)
                    {
                        client.namespace_close(ns);
                    }
                }
                catch (Exception inner)
                {
                    show("ERROR: Problen closing namespace \"test\" - " + inner.Message);
                }
                return 1;
            }
            finally
            {
                if (client != null)
                {
                    if (ns >= 0)
                    {
                        client.hql_query(ns, "drop table if exists thrift_test");
                        client.namespace_close(ns);
                    }

                    client.Close();
                }
            }

            return 0;
        }

        private static void show(String line)
        {
            Console.WriteLine(line);
        }

        #endregion
    }
}