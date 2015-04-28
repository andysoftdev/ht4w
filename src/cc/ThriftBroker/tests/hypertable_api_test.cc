/*
 * Copyright (C) 2007-2014 Hypertable, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither Hypertable, Inc. nor the names of its contributors may be used to
 *   endorse or promote products derived from this software without specific
 *   prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <Common/Compat.h>
#include <Common/Logger.h>
#include <Common/System.h>

#include <Hypertable/Lib/Key.h>
#include <Hypertable/Lib/KeySpec.h>

#include <ThriftBroker/Client.h>
#include <ThriftBroker/gen-cpp/Client_types.h>
#include <ThriftBroker/gen-cpp/HqlService.h>
#include <ThriftBroker/ThriftHelper.h>
#include <ThriftBroker/SerializedCellsReader.h>
#include <ThriftBroker/SerializedCellsWriter.h>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <direct.h>

using namespace Hypertable;
using namespace Hypertable::ThriftGen;
using namespace std;

namespace {

  const ThriftGen::Cell convert_cell_as_array(ThriftGen::CellAsArray &cell_as_array) {
    char *end;
    ThriftGen::Key key;
    ThriftGen::Cell cell;
    int len = cell_as_array.size();
    switch (len) {
    case 7: key.__set_flag((ThriftGen::KeyFlag::type)atoi(cell_as_array[6].c_str()));
    case 6: key.__set_revision((Lld)strtoll(cell_as_array[5].c_str(), &end, 0));
    case 5: key.__set_timestamp((Lld)strtoll(cell_as_array[4].c_str(), &end, 0));
    case 4: cell.__set_value(cell_as_array[3]);
    case 3: key.__set_column_qualifier(cell_as_array[2]);
    case 2: key.__set_column_family(cell_as_array[1]);
    case 1: key.__set_row(cell_as_array[0]);
      cell.__set_key(key);
      break;
    default:
      HT_THROWF(Error::BAD_KEY, "CellAsArray: bad size: %d", len);
    }
    return cell;
  }

  const ThriftGen::Cell convert_cell(Hypertable::Cell &hcell) {
    ThriftGen::Cell cell;
    ThriftGen::Key key;
    ThriftGen::Value value;

    key.__set_row(hcell.row_key);
    key.__set_column_family(hcell.column_family);
    if (hcell.column_qualifier && *hcell.column_qualifier)
      key.__set_column_qualifier(hcell.column_qualifier);
    key.__set_flag((ThriftGen::KeyFlag::type)hcell.flag);
    key.__set_revision(hcell.revision);
    key.__set_timestamp(hcell.timestamp);

    value.append((const char *)hcell.value, hcell.value_len);

    cell.__set_key(key);
    cell.__set_value(value);
    return cell;
  }
}

void test_basic(Thrift::Client *client) {

  cout << "[basic]" << endl;

  try {

    if (!client->namespace_exists("test"))
      client->namespace_create("test");

    ThriftGen::Namespace ns = client->namespace_open("test");

    bool if_exists = true;
    client->table_drop(ns, "Fruits", if_exists);

    ThriftGen::Schema schema;
    map<string, ThriftGen::ColumnFamilySpec> cf_specs;
    ThriftGen::ColumnFamilySpec cf;

    cf.__set_name("genus");
    cf_specs["genus"] = cf;
    cf.__set_name("description");
    cf_specs["description"] = cf;
    cf.__set_name("tag");
    cf_specs["tag"] = cf;
    schema.__set_column_families(cf_specs);

    client->table_create(ns, "Fruits", schema);

    client->namespace_create("/test/sub");

    vector<ThriftGen::NamespaceListing> listing;

    client->namespace_get_listing(listing, ns);

    for (auto entry : listing)
      cout << entry.name << (entry.is_namespace ? "\t(dir)" : "") << endl;

    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }
  
}

void test_convenience(Thrift::Client *client) {

  cout << "[set cells]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    vector<ThriftGen::Cell> cells;
    ThriftGen::Cell cell;

    cell.key.__set_timestamp(AUTO_ASSIGN);
    cell.key.__set_flag(ThriftGen::KeyFlag::INSERT);

    cell.key.__set_row("apple");
    cell.key.__set_column_family("genus");
    cell.__set_value("Malus");
    cells.push_back(cell);

    cell.key.__set_row("apple");
    cell.key.__set_column_family("description");
    cell.__set_value("The apple is the pomaceous fruit of the apple tree.");
    cells.push_back(cell);

    cell.key.__set_row("apple");
    cell.key.__set_column_family("tag");
    cell.key.__set_column_qualifier("crunchy");
    cells.push_back(cell);

    client->set_cells(ns, "Fruits", cells);

    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[get cells]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    ThriftGen::ScanSpec ss;
    vector<ThriftGen::Cell> cells;
    vector<string> columns;

    columns.push_back("description");
    ss.__set_columns(columns);

    client->get_cells(cells, ns, "Fruits", ss);

    for (auto & cell : cells)
      cout << cell << endl;

    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[set cells as arrays]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    vector<CellAsArray> cells_as_arrays;
    CellAsArray cell_as_array;

    cell_as_array.clear();
    cell_as_array.push_back("orange");
    cell_as_array.push_back("genus");
    cell_as_array.push_back("");
    cell_as_array.push_back("Citrus");
    cells_as_arrays.push_back(cell_as_array);

    cell_as_array.clear();
    cell_as_array.push_back("orange");
    cell_as_array.push_back("description");
    cell_as_array.push_back("");
    cell_as_array.push_back("The orange (specifically, the sweet orange) is the"
                            " fruit of the citrus species Citrus × sinensis in "
                            "the family Rutaceae.");
    cells_as_arrays.push_back(cell_as_array);

    cell_as_array.clear();
    cell_as_array.push_back("orange");
    cell_as_array.push_back("tag");
    cell_as_array.push_back("juicy");
    cells_as_arrays.push_back(cell_as_array);

    client->set_cells_as_arrays(ns, "Fruits", cells_as_arrays);

    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[get cells as arrays]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    ThriftGen::ScanSpec ss;
    vector<CellAsArray> cells_as_arrays;
    vector<string> columns;

    columns.push_back("description");
    ss.__set_columns(columns);

    client->get_cells_as_arrays(cells_as_arrays, ns, "Fruits", ss);

    for (auto & cell_as_array : cells_as_arrays)
      cout << convert_cell_as_array(cell_as_array) << endl;

    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }  

  cout << "[set cells serialized]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    SerializedCellsWriter writer(1024);
    ThriftGen::CellsSerialized cells;

    writer.add("canteloupe", "genus", "", AUTO_ASSIGN, "Cucumis", 7, FLAG_INSERT);

    writer.add("canteloupe", "description", "", AUTO_ASSIGN,
               "Canteloupe refers to a variety of Cucumis melo, a species in "
               "the family Cucurbitaceae.", 86, FLAG_INSERT);

    writer.add("canteloupe", "tag", "juicy", AUTO_ASSIGN, "", 0, FLAG_INSERT);

    writer.finalize(0);

    cells.append((const char *)writer.get_buffer(), writer.get_buffer_length());

    client->set_cells_serialized(ns, "Fruits", cells);
    
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[get cells serialized]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    ThriftGen::ScanSpec ss;
    ThriftGen::CellsSerialized cells;
    vector<string> columns;

    columns.push_back("description");
    ss.__set_columns(columns);

    client->get_cells_serialized(cells, ns, "Fruits", ss);

    SerializedCellsReader reader(cells.data(), cells.size());

    Hypertable::Cell hcell;
    while (reader.next()) {
      reader.get(hcell);
      cout << convert_cell(hcell) << endl;
    }

    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }
}

void test_create_table(Thrift::Client *client) {

  cout << "[create table]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    ThriftGen::Schema schema;

    map<string, ThriftGen::AccessGroupSpec> ag_specs;
    map<string, ThriftGen::ColumnFamilySpec> cf_specs;

    // Set table defaults
    {
      ThriftGen::AccessGroupOptions ag_options;
      ThriftGen::ColumnFamilyOptions cf_options;
      ag_options.__set_blocksize(65536);
      schema.__set_access_group_defaults(ag_options);
      cf_options.__set_max_versions(1);
      schema.__set_column_family_defaults(cf_options);
    }

    // Access group "ag_normal"
    {
      ThriftGen::AccessGroupSpec ag;
      ThriftGen::ColumnFamilyOptions cf_options;
      cf_options.__set_max_versions(2);
      ag.__set_name("ag_normal");
      ag.__set_defaults(cf_options);
      ag_specs["ag_normal"] = ag;
    }

    // Column "a"
    {
      ThriftGen::ColumnFamilySpec cf;
      cf.__set_name("a");
      cf.__set_access_group("ag_normal");
      cf.__set_value_index(true);
      cf.__set_qualifier_index(true);
      cf_specs["a"] = cf;
    }

    // Column "b"
    {
      ThriftGen::ColumnFamilySpec cf;
      ThriftGen::ColumnFamilyOptions cf_options;
      cf.__set_name("b");
      cf.__set_access_group("ag_normal");
      cf_options.__set_max_versions(3);
      cf.__set_options(cf_options);
      cf_specs["b"] = cf;
    }

    // Access group "ag_fast"
    {
      ThriftGen::AccessGroupSpec ag;
      ThriftGen::AccessGroupOptions ag_options;
      ag.__set_name("ag_fast");
      ag_options.__set_in_memory(true);
      ag_options.__set_blocksize(131072);
      ag.__set_options(ag_options);
      ag_specs["ag_fast"] = ag;
    }

    // Column "c"
    {
      ThriftGen::ColumnFamilySpec cf;
      cf.__set_name("c");
      cf.__set_access_group("ag_fast");
      cf_specs["c"] = cf;
    }

    // Access group "ag_secure"
    {
      ThriftGen::AccessGroupSpec ag;
      ThriftGen::AccessGroupOptions ag_options;
      ag.__set_name("ag_secure");
      ag_options.__set_replication(5);
      ag.__set_options(ag_options);
      ag_specs["ag_secure"] = ag;
    }

    // Column "d"
    {
      ThriftGen::ColumnFamilySpec cf;
      cf.__set_name("d");
      cf.__set_access_group("ag_secure");
      cf_specs["d"] = cf;
    }

    // Access group "ag_counter"
    {
      ThriftGen::AccessGroupSpec ag;
      ThriftGen::ColumnFamilyOptions cf_options;
      ag.__set_name("ag_counter");
      cf_options.__set_counter(true);
      cf_options.__set_max_versions(0);
      ag.__set_defaults(cf_options);
      ag_specs["ag_counter"] = ag;
    }

    // Column "e"
    {
      ThriftGen::ColumnFamilySpec cf;
      cf.__set_name("e");
      cf.__set_access_group("ag_counter");
      cf_specs["e"] = cf;
    }

    // Column "f"
    {
      ThriftGen::ColumnFamilySpec cf;
      ThriftGen::ColumnFamilyOptions cf_options;
      cf.__set_name("f");
      cf.__set_access_group("ag_counter");
      cf_options.__set_counter(false);
      cf.__set_options(cf_options);
      cf_specs["f"] = cf;
    }

    schema.__set_access_groups(ag_specs);
    schema.__set_column_families(cf_specs);

    client->table_create(ns, "TestTable", schema);

    HqlResult result;
    client->hql_query(result, ns, "SHOW CREATE TABLE TestTable");

    if (!result.results.empty())
      cout << result.results[0] << endl;

    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }
}

void test_alter_table(Thrift::Client *client) {

  cout << "[alter table]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    ThriftGen::Schema schema;

    client->get_schema(schema, ns, "TestTable");

    // Rename column "b" to "z"
    {
      auto iter = schema.column_families.find("b");
      assert(iter != schema.column_families.end());
      ThriftGen::ColumnFamilySpec cf_spec = iter->second;
      schema.column_families.erase(iter);
      cf_spec.__set_name("z");
      schema.column_families["z"] = cf_spec;
    }

    // Add column "g"
    {
      ThriftGen::ColumnFamilySpec cf_spec;
      cf_spec.__set_name("g");
      cf_spec.__set_access_group("ag_counter");
      schema.column_families["g"] = cf_spec;
    }

    client->table_alter(ns, "TestTable", schema);

    HqlResult result;
    client->hql_query(result, ns, "SHOW CREATE TABLE TestTable");

    if (!result.results.empty())
      cout << result.results[0] << endl;

    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }
}

using Hypertable::ThriftGen::make_cell;

void test_mutator(Thrift::Client *client) {

  cout << "[mutator]" << endl;

  try {
    vector<ThriftGen::Cell> cells;

    ThriftGen::Namespace ns = client->namespace_open("test");

    ThriftGen::Mutator mutator = client->mutator_open(ns, "Fruits", 0, 0);

    // Auto-assigned timestamps

    cells.clear();
    cells.push_back(make_cell("lemon", "genus", "", "Citrus"));
    cells.push_back(make_cell("lemon", "tag", "bitter", ""));
    cells.push_back(make_cell("lemon", "description", 0, "The lemon (Citrus × "
                              "limon) is a small evergreen tree native to Asia."));
    client->mutator_set_cells(mutator, cells);
    client->mutator_flush(mutator);

    // Explicitly-supplied timestamps

    cells.clear();
    cells.push_back(make_cell("mango", "genus", "", "Mangifera", "2014-06-06 16:27:15"));
    cells.push_back(make_cell("mango", "tag", "sweet", "", "2014-06-06 16:27:15"));
    cells.push_back(make_cell("mango", "description", 0, "Mango is one of the "
                              "delicious seasonal fruits grown in the tropics.",
                              "2014-06-06 16:27:15"));
    cells.push_back(make_cell("mango", "description", 0, "The mango is a juicy "
                              "stone fruit belonging to the genus Mangifera, "
                              "consisting of numerous tropical fruiting trees, "
                              "that are cultivated mostly for edible fruits. ",
                              "2014-06-06 16:27:16"));
    client->mutator_set_cells(mutator, cells);
    client->mutator_flush(mutator);
  
    // Delete cells

    cells.clear();
    cells.push_back(make_cell("apple", "", nullptr, "", nullptr, nullptr,
                              ThriftGen::KeyFlag::DELETE_ROW));
    cells.push_back(make_cell("mango", "description", nullptr, "",
                              "2014-06-06 16:27:15", nullptr,
                              ThriftGen::KeyFlag::DELETE_CELL));
    client->mutator_set_cells(mutator, cells);
    client->mutator_flush(mutator);
    client->mutator_close(mutator);

    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }
}

void test_scanner(Thrift::Client *client) {

  cout << "[scanner - full table scan]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    ThriftGen::Scanner scanner = client->scanner_open(ns, "Fruits", ThriftGen::ScanSpec());

    vector<ThriftGen::Cell> cells;
    do {
      client->scanner_get_cells(cells, scanner);
      for (auto & cell : cells)
        cout << cell << endl;
    } while (!cells.empty());

    client->scanner_close(scanner);

    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[scanner - restricted scan]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    ThriftGen::ScanSpec ss;

    // Return row range [lemon..orange)
    ThriftGen::RowInterval ri;
    vector<ThriftGen::RowInterval> row_intervals;
    ri.__set_start_row("lemon");
    ri.__set_start_inclusive(true);
    ri.__set_end_row("orange");
    ri.__set_end_inclusive(false);
    row_intervals.push_back(ri);
    ss.__set_row_intervals(row_intervals);

    // Return columns "genus", "tag:bitter", "tag:sweet"
    vector<string> columns;
    columns.push_back("genus");
    columns.push_back("tag:bitter");
    columns.push_back("tag:sweet");
    ss.__set_columns(columns);

    // Return only most recent version of each cell
    ss.__set_versions(1);
  
    ThriftGen::Scanner scanner = client->scanner_open(ns, "Fruits", ss);

    vector<ThriftGen::Cell> cells;
    do {
      client->scanner_get_cells(cells, scanner);
      for (auto & cell : cells)
        cout << cell << endl;
    } while (!cells.empty());

    client->scanner_close(scanner);

    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }
}

void test_hql(Thrift::Client *client) {

  cout << "[hql_query]" << endl;

  try {
    HqlResult result;

    ThriftGen::Namespace ns = client->namespace_open("test");

    client->hql_query(result, ns, "GET LISTING");

    for (auto & str: result.results)
      cout << str << endl;

    client->hql_query(result, ns, "SELECT * from Fruits WHERE ROW = 'mango'");

    for (auto & cell : result.cells)
      cout << cell << endl;

    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[hql_query_as_arrays]" << endl;

  try {
    HqlResultAsArrays result_as_arrays;

    ThriftGen::Namespace ns = client->namespace_open("test");

    client->hql_query_as_arrays(result_as_arrays, ns,
                                "SELECT * from Fruits WHERE ROW = 'lemon'");

    for (auto & cell_as_array : result_as_arrays.cells)
      cout << cell_as_array << endl;

    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[hql_exec mutator]" << endl;

  try {
    HqlResult result;
    vector<ThriftGen::Cell> cells;

    ThriftGen::Namespace ns = client->namespace_open("test");

    client->hql_exec(result, ns, "INSERT INTO Fruits VALUES ('strawberry', "
                     "'genus', 'Fragaria'), ('strawberry', 'tag:fibrous', ''),"
                     " ('strawberry', 'description', 'The garden strawberry is"
                     " a widely grown hybrid species of the genus Fragaria')",
                     true, false);
    cells.push_back(make_cell("pineapple", "genus", "", "Ananas"));
    cells.push_back(make_cell("pineapple", "tag", "acidic", ""));
    cells.push_back(make_cell("pineapple", "description", 0, "The pineapple "
                              "(Ananas comosus) is a tropical plant with "
                              "edible multiple fruit consisting of coalesced "
                              "berries."));
    client->mutator_set_cells(result.mutator, cells);
    client->mutator_flush(result.mutator);
    client->mutator_close(result.mutator);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[hql_exec scanner]" << endl;

  try {
    HqlResult result;

    ThriftGen::Namespace ns = client->namespace_open("test");

    client->hql_exec(result, ns, "SELECT * from Fruits", false, true);

    vector<ThriftGen::Cell> cells;
    do {
      client->scanner_get_cells(cells, result.scanner);
      for (auto & cell : cells)
        cout << cell << endl;
    } while (!cells.empty());

    client->scanner_close(result.scanner);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }
}

void test_secondary_indices(Thrift::Client *client) {
  ThriftGen::ColumnPredicate column_predicate;

  cout << "[secondary indices]" << endl;

  try {
    HqlResult result;

    ThriftGen::Namespace ns = client->namespace_open("test");

    string command = "CREATE TABLE products (title, section, info, category,"
      "INDEX section, INDEX info, QUALIFIER INDEX info,"
      "QUALIFIER INDEX category)";
    client->hql_query(result, ns, command);

    command = format("LOAD DATA INFILE '%s/indices_test_products.tsv' INTO TABLE products", _getcwd(0, 0));
    client->hql_query(result, ns, command);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[secondary index - SELECT title FROM products WHERE section = 'books']" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    ThriftGen::ScanSpec ss;

    vector<ThriftGen::ColumnPredicate> column_predicates;
    column_predicate.__set_column_family("section");
    column_predicate.__set_operation(ColumnPredicateOperation::EXACT_MATCH);
    column_predicate.__set_value("books");
    column_predicates.push_back(column_predicate);
    ss.__set_column_predicates(column_predicates);

    vector<string> columns;
    columns.push_back("title");
    ss.__set_columns(columns);

    ThriftGen::Scanner scanner = client->scanner_open(ns, "products", ss);

    vector<ThriftGen::Cell> cells;
    do {
      client->scanner_get_cells(cells, scanner);
      for (auto & cell : cells)
        cout << cell << endl;
    } while (!cells.empty());

    client->scanner_close(scanner);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[secondary index - SELECT title FROM products WHERE info:actor = 'Jack Nicholson']" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    ThriftGen::ScanSpec ss;
    vector<ThriftGen::ColumnPredicate> column_predicates;
    column_predicate.__set_column_family("info");
    column_predicate.__set_column_qualifier("actor");
    int operation = (int)ColumnPredicateOperation::EXACT_MATCH |
                    (int)ColumnPredicateOperation::QUALIFIER_EXACT_MATCH;
    column_predicate.__set_operation((ColumnPredicateOperation::type)operation);
    column_predicate.__set_value("Jack Nicholson");
    column_predicates.push_back(column_predicate);
    ss.__set_column_predicates(column_predicates);

    vector<string> columns;
    columns.push_back("title");
    ss.__set_columns(columns);

    ThriftGen::Scanner scanner = client->scanner_open(ns, "products", ss);

    vector<ThriftGen::Cell> cells;
    do {
      client->scanner_get_cells(cells, scanner);
      for (auto & cell : cells)
        cout << cell << endl;
    } while (!cells.empty());

    client->scanner_close(scanner);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[secondary index - SELECT title,info:publisher FROM products WHERE info:publisher =^ 'Addison-Wesley']" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    ThriftGen::ScanSpec ss;

    vector<ThriftGen::ColumnPredicate> column_predicates;
    column_predicate.__set_column_family("info");
    column_predicate.__set_column_qualifier("publisher");
    int operation = (int)ColumnPredicateOperation::PREFIX_MATCH |
                    (int)ColumnPredicateOperation::QUALIFIER_EXACT_MATCH;
    column_predicate.__set_operation((ColumnPredicateOperation::type)operation);
    column_predicate.__set_value("Addison-Wesley");
    column_predicates.push_back(column_predicate);
    ss.__set_column_predicates(column_predicates);

    vector<string> columns;
    columns.push_back("title");
    columns.push_back("info:publisher");
    ss.__set_columns(columns);

    ThriftGen::Scanner scanner = client->scanner_open(ns, "products", ss);

    vector<ThriftGen::Cell> cells;
    do {
      client->scanner_get_cells(cells, scanner);
      for (auto & cell : cells)
        cout << cell << endl;
    } while (!cells.empty());

    client->scanner_close(scanner);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[secondary index - SELECT title,info:publisher FROM products WHERE info:publisher =~ /^Addison-Wesley/]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    ThriftGen::ScanSpec ss;

    vector<ThriftGen::ColumnPredicate> column_predicates;
    column_predicate.__set_column_family("info");
    column_predicate.__set_column_qualifier("publisher");
    int operation = (int)ColumnPredicateOperation::REGEX_MATCH |
                    (int)ColumnPredicateOperation::QUALIFIER_EXACT_MATCH;
    column_predicate.__set_operation((ColumnPredicateOperation::type)operation);
    column_predicate.__set_value("^Addison-Wesley");
    column_predicates.push_back(column_predicate);
    ss.__set_column_predicates(column_predicates);

    vector<string> columns;
    columns.push_back("title");
    columns.push_back("info:publisher");
    ss.__set_columns(columns);

    ThriftGen::Scanner scanner = client->scanner_open(ns, "products", ss);

    vector<ThriftGen::Cell> cells;
    do {
      client->scanner_get_cells(cells, scanner);
      for (auto & cell : cells)
        cout << cell << endl;
    } while (!cells.empty());

    client->scanner_close(scanner);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[secondary index - SELECT title FROM products WHERE Exists(info:studio)]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    ThriftGen::ScanSpec ss;

    vector<ThriftGen::ColumnPredicate> column_predicates;
    column_predicate.__set_column_family("info");
    column_predicate.__set_column_qualifier("studio");
    column_predicate.__set_operation(ColumnPredicateOperation::QUALIFIER_EXACT_MATCH);
    column_predicates.push_back(column_predicate);
    ss.__set_column_predicates(column_predicates);

    vector<string> columns;
    columns.push_back("title");
    ss.__set_columns(columns);

    ThriftGen::Scanner scanner = client->scanner_open(ns, "products", ss);

    vector<ThriftGen::Cell> cells;
    do {
      client->scanner_get_cells(cells, scanner);
      for (auto & cell : cells)
        cout << cell << endl;
    } while (!cells.empty());

    client->scanner_close(scanner);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[secondary index - SELECT title FROM products WHERE Exists(category:/^\\/Movies/)]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    ThriftGen::ScanSpec ss;

    vector<ThriftGen::ColumnPredicate> column_predicates;
    column_predicate.__set_column_family("category");
    column_predicate.__set_column_qualifier("^/Movies");
    column_predicate.__set_operation(ColumnPredicateOperation::QUALIFIER_REGEX_MATCH);
    column_predicates.push_back(column_predicate);
    ss.__set_column_predicates(column_predicates);

    vector<string> columns;
    columns.push_back("title");
    ss.__set_columns(columns);

    ThriftGen::Scanner scanner = client->scanner_open(ns, "products", ss);

    vector<ThriftGen::Cell> cells;
    do {
      client->scanner_get_cells(cells, scanner);
      for (auto & cell : cells)
        cout << cell << endl;
    } while (!cells.empty());

    client->scanner_close(scanner);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[secondary index - SELECT title FROM products WHERE info:author =~ /^Stephen P/ OR info:publisher =^ 'Anchor']" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    ThriftGen::ScanSpec ss;

    vector<ThriftGen::ColumnPredicate> column_predicates;
    // info:author =~ /^Stephen P/
    column_predicate.__set_column_family("info");
    column_predicate.__set_column_qualifier("author");
    int operation = (int)ColumnPredicateOperation::REGEX_MATCH |
                    (int)ColumnPredicateOperation::QUALIFIER_EXACT_MATCH;
    column_predicate.__set_operation((ColumnPredicateOperation::type)operation);
    column_predicate.__set_value("^Stephen P");
    column_predicates.push_back(column_predicate);
    // info:publisher =^ "Anchor"
    column_predicate.__set_column_family("info");
    column_predicate.__set_column_qualifier("publisher");
    operation = (int)ColumnPredicateOperation::PREFIX_MATCH |
                (int)ColumnPredicateOperation::QUALIFIER_EXACT_MATCH;
    column_predicate.__set_operation((ColumnPredicateOperation::type)operation);
    column_predicate.__set_value("Anchor");
    column_predicates.push_back(column_predicate);

    ss.__set_column_predicates(column_predicates);

    vector<string> columns;
    columns.push_back("title");
    ss.__set_columns(columns);

    ThriftGen::Scanner scanner = client->scanner_open(ns, "products", ss);

    vector<ThriftGen::Cell> cells;
    do {
      client->scanner_get_cells(cells, scanner);
      for (auto & cell : cells)
        cout << cell << endl;
    } while (!cells.empty());

    client->scanner_close(scanner);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[secondary index - SELECT title FROM products WHERE info:author =~ /^Stephen [PK]/ AND info:publisher =^ 'Anchor']" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    ThriftGen::ScanSpec ss;

    vector<ThriftGen::ColumnPredicate> column_predicates;
    // info:author =~ /^Stephen [PK]/
    column_predicate.__set_column_family("info");
    column_predicate.__set_column_qualifier("author");
    int operation = (int)ColumnPredicateOperation::REGEX_MATCH |
                    (int)ColumnPredicateOperation::QUALIFIER_EXACT_MATCH;
    column_predicate.__set_operation((ColumnPredicateOperation::type)operation);
    column_predicate.__set_value("^Stephen [PK]");
    column_predicates.push_back(column_predicate);
    // info:publisher =^ "Anchor"
    column_predicate.__set_column_family("info");
    column_predicate.__set_column_qualifier("publisher");
    operation = (int)ColumnPredicateOperation::PREFIX_MATCH |
                (int)ColumnPredicateOperation::QUALIFIER_EXACT_MATCH;
    column_predicate.__set_operation((ColumnPredicateOperation::type)operation);
    column_predicate.__set_value("Anchor");
    column_predicates.push_back(column_predicate);

    ss.__set_column_predicates(column_predicates);

    // AND the predicates together
    ss.__set_and_column_predicates(true);

    vector<string> columns;
    columns.push_back("title");
    ss.__set_columns(columns);

    ThriftGen::Scanner scanner = client->scanner_open(ns, "products", ss);

    vector<ThriftGen::Cell> cells;
    do {
      client->scanner_get_cells(cells, scanner);
      for (auto & cell : cells)
        cout << cell << endl;
    } while (!cells.empty());

    client->scanner_close(scanner);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[secondary index - SELECT title FROM products WHERE ROW > 'B00002VWE0' AND info:actor = 'Jack Nicholson']" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    ThriftGen::ScanSpec ss;

    // ROW > 'B00002VWE0'
    vector<ThriftGen::RowInterval> row_intervals;
    ThriftGen::RowInterval ri;
    ri.__set_start_row("B00002VWE0");
    ri.__set_start_inclusive(false);
    row_intervals.push_back(ri);
    ss.__set_row_intervals(row_intervals);

    // info:actor = 'Jack Nicholson'
    vector<ThriftGen::ColumnPredicate> column_predicates;
    column_predicate.__set_column_family("info");
    column_predicate.__set_column_qualifier("actor");
    int operation = (int)ColumnPredicateOperation::EXACT_MATCH |
                    (int)ColumnPredicateOperation::QUALIFIER_EXACT_MATCH;
    column_predicate.__set_operation((ColumnPredicateOperation::type)operation);
    column_predicate.__set_value("Jack Nicholson");
    column_predicates.push_back(column_predicate);
    ss.__set_column_predicates(column_predicates);

    // AND the predicates together
    ss.__set_and_column_predicates(true);

    vector<string> columns;
    columns.push_back("title");
    ss.__set_columns(columns);

    ThriftGen::Scanner scanner = client->scanner_open(ns, "products", ss);

    vector<ThriftGen::Cell> cells;
    do {
      client->scanner_get_cells(cells, scanner);
      for (auto & cell : cells)
        cout << cell << endl;
    } while (!cells.empty());

    client->scanner_close(scanner);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[secondary index - SELECT title FROM products WHERE ROW =^ 'B' AND info:actor = 'Jack Nicholson']" << endl;  

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");

    ThriftGen::ScanSpec ss;

    // ROW =^ 'B'
    vector<ThriftGen::RowInterval> row_intervals;
    ThriftGen::RowInterval ri;
    ri.__set_start_row("B");
    ri.__set_start_inclusive(true);
    ri.__set_end_row("C");
    ri.__set_end_inclusive(false);
    row_intervals.push_back(ri);
    ss.__set_row_intervals(row_intervals);

    // info:actor = 'Jack Nicholson'
    vector<ThriftGen::ColumnPredicate> column_predicates;
    column_predicate.__set_column_family("info");
    column_predicate.__set_column_qualifier("actor");
    int operation = (int)ColumnPredicateOperation::EXACT_MATCH |
                    (int)ColumnPredicateOperation::QUALIFIER_EXACT_MATCH;
    column_predicate.__set_operation((ColumnPredicateOperation::type)operation);
    column_predicate.__set_value("Jack Nicholson");
    column_predicates.push_back(column_predicate);
    ss.__set_column_predicates(column_predicates);

    // AND the predicates together
    ss.__set_and_column_predicates(true);

    vector<string> columns;
    columns.push_back("title");
    ss.__set_columns(columns);

    ThriftGen::Scanner scanner = client->scanner_open(ns, "products", ss);

    vector<ThriftGen::Cell> cells;
    do {
      client->scanner_get_cells(cells, scanner);
      for (auto & cell : cells)
        cout << cell << endl;
    } while (!cells.empty());

    client->scanner_close(scanner);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }
}

void test_async(Thrift::Client *client) {

  cout << "[async]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");
    ThriftGen::HqlResult result;
    client->hql_query(result, ns, "CREATE TABLE Profile (info, last_access MAX_VERSIONS 1)");
    client->hql_query(result, ns, "CREATE TABLE Session (user_id, page_hit)");
    client->hql_query(result, ns, "INSERT INTO Profile VALUES ('1', 'info:name', 'Joe'), ('2', 'info:name', 'Sue')");
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[async mutator]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");
    Future ff = client->future_open(0);
    MutatorAsync profile_mutator = client->async_mutator_open(ns, "Profile", ff, 0);
    MutatorAsync session_mutator = client->async_mutator_open(ns, "Session", ff, 0);

    vector<ThriftGen::Cell> cells;
    
    cells.push_back(make_cell("1", "last_access", 0, "2014-06-13 16:06:09"));
    cells.push_back(make_cell("2", "last_access", 0, "2014-06-13 16:06:10"));
    client->async_mutator_set_cells(profile_mutator, cells);

    cells.clear();
    cells.push_back(make_cell("0001-200238", "user_id", "1", ""));
    cells.push_back(make_cell("0001-200238", "page_hit", 0, "/index.html"));
    cells.push_back(make_cell("0002-383049", "user_id", "2", ""));
    cells.push_back(make_cell("0002-383049", "page_hit", 0, "/foo/bar.html"));
    client->async_mutator_set_cells(session_mutator, cells);

    client->async_mutator_flush(profile_mutator);
    client->async_mutator_flush(session_mutator);

    ThriftGen::Result result;
    size_t result_count = 0;
    while (true) {
      client->future_get_result(result, ff, 0);
      if (result.is_empty)
        break;
      result_count++;
      if (result.is_error) {
        cout << "Async mutator error:  " << result.error_msg << endl;
        _exit(1);
      }
      if (result.id == profile_mutator)
        cout << "Result is from Profile mutation" << endl;
      else if (result.id == session_mutator)
        cout << "Result is from Session mutation" << endl;
    }

    cout << "result count = " << result_count << endl;

    client->async_mutator_close(profile_mutator);
    client->async_mutator_close(session_mutator);
    client->future_close(ff);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[async scanner]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");
    ScannerAsync profile_scanner;
    ScannerAsync session_scanner;

    ThriftGen::Future ff = client->future_open(0);

    {
      ThriftGen::ScanSpec ss;
      vector<ThriftGen::RowInterval> row_intervals;
      ThriftGen::RowInterval ri;
      ri.__set_start_row("1");
      ri.__set_start_inclusive(true);
      ri.__set_end_row("1");
      ri.__set_end_inclusive(true);
      row_intervals.push_back(ri);
      ss.__set_row_intervals(row_intervals);
      profile_scanner = client->async_scanner_open(ns, "Profile", ff, ss);
    }

    {
      ThriftGen::ScanSpec ss;
      vector<ThriftGen::RowInterval> row_intervals;
      ThriftGen::RowInterval ri;
      ri.__set_start_row("0001-200238");
      ri.__set_start_inclusive(true);
      ri.__set_end_row("0001-200238");
      ri.__set_end_inclusive(true);
      row_intervals.push_back(ri);
      ss.__set_row_intervals(row_intervals);
      session_scanner = client->async_scanner_open(ns, "Session", ff, ss);
    }

    ThriftGen::Result result;

    while (true) {

      client->future_get_result(result, ff, 0);

      if (result.is_empty)
        break;

      if (result.is_error) {
        cout << "Async scanner error:  " << result.error_msg << endl;
        _exit(1);
      }

      assert(result.is_scan);
      assert(result.id == profile_scanner || result.id == session_scanner);

      if (result.id == profile_scanner)
        cout << "Result is from Profile scan" << endl;
      else if (result.id == session_scanner)
        cout << "Result is from Session scan" << endl;

      for (auto & cell : result.cells)
        cout << cell << endl;

    }

    client->async_scanner_close(profile_scanner);
    client->async_scanner_close(session_scanner);
    client->future_close(ff);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[async scanner - result serialized]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");
    ScannerAsync profile_scanner;
    ScannerAsync session_scanner;

    ThriftGen::Future ff = client->future_open(0);

    {
      ThriftGen::ScanSpec ss;
      vector<ThriftGen::RowInterval> row_intervals;
      ThriftGen::RowInterval ri;
      ri.__set_start_row("1");
      ri.__set_start_inclusive(true);
      ri.__set_end_row("1");
      ri.__set_end_inclusive(true);
      row_intervals.push_back(ri);
      ss.__set_row_intervals(row_intervals);
      profile_scanner = client->async_scanner_open(ns, "Profile", ff, ss);
    }

    {
      ThriftGen::ScanSpec ss;
      vector<ThriftGen::RowInterval> row_intervals;
      ThriftGen::RowInterval ri;
      ri.__set_start_row("0001-200238");
      ri.__set_start_inclusive(true);
      ri.__set_end_row("0001-200238");
      ri.__set_end_inclusive(true);
      row_intervals.push_back(ri);
      ss.__set_row_intervals(row_intervals);
      session_scanner = client->async_scanner_open(ns, "Session", ff, ss);
    }

    ThriftGen::ResultSerialized result_serialized;

    while (true) {

      client->future_get_result_serialized(result_serialized, ff, 0);

      if (result_serialized.is_empty)
        break;

      if (result_serialized.is_error) {
        cout << "Async scanner error:  " << result_serialized.error_msg << endl;
        _exit(1);
      }

      assert(result_serialized.is_scan);
      assert(result_serialized.id == profile_scanner || result_serialized.id == session_scanner);

      if (result_serialized.id == profile_scanner)
        cout << "Result is from Profile scan" << endl;
      else if (result_serialized.id == session_scanner)
        cout << "Result is from Session scan" << endl;

      SerializedCellsReader reader(result_serialized.cells.data(),
                                   result_serialized.cells.size());

      Hypertable::Cell hcell;
      while (reader.next()) {
        reader.get(hcell);
        cout << convert_cell(hcell) << endl;
      }

    }

    client->async_scanner_close(profile_scanner);
    client->async_scanner_close(session_scanner);
    client->future_close(ff);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[async scanner - result as arrays]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");
    ScannerAsync profile_scanner;
    ScannerAsync session_scanner;

    ThriftGen::Future ff = client->future_open(0);

    {
      ThriftGen::ScanSpec ss;
      vector<ThriftGen::RowInterval> row_intervals;
      ThriftGen::RowInterval ri;
      ri.__set_start_row("1");
      ri.__set_start_inclusive(true);
      ri.__set_end_row("1");
      ri.__set_end_inclusive(true);
      row_intervals.push_back(ri);
      ss.__set_row_intervals(row_intervals);
      profile_scanner = client->async_scanner_open(ns, "Profile", ff, ss);
    }

    {
      ThriftGen::ScanSpec ss;
      vector<ThriftGen::RowInterval> row_intervals;
      ThriftGen::RowInterval ri;
      ri.__set_start_row("0001-200238");
      ri.__set_start_inclusive(true);
      ri.__set_end_row("0001-200238");
      ri.__set_end_inclusive(true);
      row_intervals.push_back(ri);
      ss.__set_row_intervals(row_intervals);
      session_scanner = client->async_scanner_open(ns, "Session", ff, ss);
    }

    ThriftGen::ResultAsArrays result_as_arrays;

    while (true) {

      client->future_get_result_as_arrays(result_as_arrays, ff, 0);

      if (result_as_arrays.is_empty)
        break;

      if (result_as_arrays.is_error) {
        cout << "Async scanner error:  " << result_as_arrays.error_msg << endl;
        _exit(1);
      }

      assert(result_as_arrays.is_scan);
      assert(result_as_arrays.id == profile_scanner || result_as_arrays.id == session_scanner);

      if (result_as_arrays.id == profile_scanner)
        cout << "Result is from Profile scan" << endl;
      else if (result_as_arrays.id == session_scanner)
        cout << "Result is from Session scan" << endl;

      for (auto & cell_as_array : result_as_arrays.cells)
        cout << cell_as_array << endl;

    }

    client->async_scanner_close(profile_scanner);
    client->async_scanner_close(session_scanner);
    client->future_close(ff);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }
}

void test_atomic_counter(Thrift::Client *client) {

  cout << "[atomic counter]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");
    ThriftGen::HqlResult result;
    client->hql_query(result, ns, "CREATE TABLE Hits (count COUNTER)");
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[atomic counter - increment]" << endl;

  try {
    vector<ThriftGen::Cell> cells;
    ThriftGen::Namespace ns = client->namespace_open("test");
    ThriftGen::Mutator mutator = client->mutator_open(ns, "Hits", 0, 0);

    cells.push_back(make_cell("/index.html", "count", "2014-06-14 07:31:18", "1"));
    cells.push_back(make_cell("/index.html", "count", "2014-06-14 07:31:18", "1"));
    cells.push_back(make_cell("/foo/bar.html", "count", "2014-06-14 07:31:18", "1"));
    cells.push_back(make_cell("/foo/bar.html", "count", "2014-06-14 07:31:18", "1"));
    cells.push_back(make_cell("/foo/bar.html", "count", "2014-06-14 07:31:18", "1"));
    cells.push_back(make_cell("/index.html", "count", "2014-06-14 07:31:19", "1"));
    cells.push_back(make_cell("/index.html", "count", "2014-06-14 07:31:19", "1"));
    cells.push_back(make_cell("/index.html", "count", "2014-06-14 07:31:19", "1"));
    cells.push_back(make_cell("/index.html", "count", "2014-06-14 07:31:19", "1"));
    cells.push_back(make_cell("/foo/bar.html", "count", "2014-06-14 07:31:19", "1"));
    client->mutator_set_cells(mutator, cells);
    client->mutator_flush(mutator);

    {
      ThriftGen::ScanSpec ss;
      ThriftGen::Scanner scanner = client->scanner_open(ns, "Hits", ss);
      do {
        client->scanner_get_cells(cells, scanner);
        for (auto & cell : cells)
          cout << cell << endl;
      } while (!cells.empty());
      client->scanner_close(scanner);
    }

    client->mutator_close(mutator);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  cout << "[atomic counter - reset and subtraction]" << endl;

  try {
    vector<ThriftGen::Cell> cells;
    ThriftGen::Namespace ns = client->namespace_open("test");
    ThriftGen::Mutator mutator = client->mutator_open(ns, "Hits", 0, 0);

    cells.push_back(make_cell("/index.html", "count", "2014-06-14 07:31:18", "=0"));
    cells.push_back(make_cell("/index.html", "count", "2014-06-14 07:31:18", "7"));
    cells.push_back(make_cell("/foo/bar.html", "count", "2014-06-14 07:31:18", "-1"));
    cells.push_back(make_cell("/index.html", "count", "2014-06-14 07:31:19", "-2"));
    cells.push_back(make_cell("/foo/bar.html", "count", "2014-06-14 07:31:19", "=19"));
    client->mutator_set_cells(mutator, cells);
    client->mutator_flush(mutator);

    {
      ThriftGen::ScanSpec ss;
      ThriftGen::Scanner scanner = client->scanner_open(ns, "Hits", ss);
      do {
        client->scanner_get_cells(cells, scanner);
        for (auto & cell : cells)
          cout << cell << endl;
      } while (!cells.empty());
      client->scanner_close(scanner);
    }
    
    client->mutator_close(mutator);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }
}

void test_unique(Thrift::Client *client) {
  
  cout << "[unique]" << endl;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");
    ThriftGen::HqlResult result;
    client->hql_query(result, ns, "CREATE TABLE User (info, id TIME_ORDER desc MAX_VERSIONS 1)");
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  ThriftGen::Key key;
  key.column_family="id";
  String ret;

  try {
    ThriftGen::Namespace ns = client->namespace_open("test");
    key.row="joe1987";
    client->create_cell_unique(ret, ns, "User", key, "");
    key.row="mary.bellweather";
    client->create_cell_unique(ret, ns, "User", key, "");
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  {
    ThriftGen::Namespace ns = client->namespace_open("test");
    try {
      key.row="joe1987";
      client->create_cell_unique(ret, ns, "User", key, "");
    }
    catch (ClientException &e) {
      if (e.code == Error::ALREADY_EXISTS)
        cout << "User name '" << key.row << "' is already taken" << endl;
      else
        cout << e.message << endl;
    }
    client->namespace_close(ns);
  }

  try {
    vector<ThriftGen::Cell> cells;
    ThriftGen::Namespace ns = client->namespace_open("test");
    ThriftGen::Scanner scanner = client->scanner_open(ns, "User", ThriftGen::ScanSpec());
    do {
      client->scanner_get_cells(cells, scanner);
      for (auto & cell : cells)
        cout << cell << endl;
    } while (!cells.empty());
    client->scanner_close(scanner);
    client->namespace_close(ns);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

}


int main() {
  FILE* f = freopen("hypertable_api_test.txt", "w", stdout);

  Thrift::Client *client = nullptr;

  try {
    client = new Thrift::Client("localhost", 15867);
  }
  catch (ClientException &e) {
    cout << e.message << endl;
    exit(1);
  }

  test_basic(client);
  test_convenience(client);
  test_create_table(client);
  test_alter_table(client);
  test_mutator(client);
  test_scanner(client);
  test_hql(client);
  test_secondary_indices(client);
  test_async(client);
  test_atomic_counter(client);
  test_unique(client);

  fclose(f);

#ifndef _WIN32
  String cmd_str = "diff hypertable_api_test.txt hypertable_api_test.golden";
#else
  String cmd_str = "sed.exe " \
    "-e s/ts=[-,0-9]*/ts=\\.\\.\\./ig " \
    "-e s/timestamp=[-0-9]*/timestamp=\\.\\.\\./ig " \
    "-e s/revision=[-0-9]*/revision=\\.\\.\\./ig " \
    "-e s/value=[a-z0-9]*-[a-z0-9]*-[a-z0-9]*-[a-z0-9]*-[a-z0-9]*/value=\\.\\.\\./ig " \
    "hypertable_api_test.txt > hypertable_api_test.sed.txt";

  if (system(cmd_str.c_str()) != 0)
    _exit(1);

  cmd_str = "fc hypertable_api_test.sed.txt hypertable_api_test.golden > hypertable_api_test.diff.txt";
#endif
  if (system(cmd_str.c_str()) != 0)
    _exit(1);

  _exit(0);
}
