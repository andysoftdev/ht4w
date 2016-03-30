/**
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

#include <Common/Compat.h>

#include <Hyperspace/Session.h>
#include <Hyperspace/Config.h>

#include <Hypertable/RangeServer/CellStore.h>
#include <Hypertable/RangeServer/CellStoreFactory.h>
#include <Hypertable/RangeServer/CellStoreTrailerV7.h>
#include <Hypertable/RangeServer/Config.h>
#include <Hypertable/RangeServer/Global.h>
#include <Hypertable/RangeServer/KeyDecompressorPrefix.h>

#include <Hypertable/Lib/BlockHeaderCellStore.h>
#include <Hypertable/Lib/CompressorFactory.h>
#include <Hypertable/Lib/Key.h>
#include <Hypertable/Lib/LoadDataEscape.h>
#include <Hypertable/Lib/CommitLog.h>
#include <Hypertable/Lib/CommitLogReader.h>
#include <Hypertable/Lib/LegacyDecoder.h>
#include <Hypertable/Lib/TableIdentifier.h>

#include <FsBroker/Lib/Client.h>

#include <AsyncComm/Comm.h>
#include <AsyncComm/ConnectionManager.h>
#include <AsyncComm/ReactorFactory.h>

#include <Common/BloomFilterWithChecksum.h>
#include <Common/ByteString.h>
#include <Common/Checksum.h>
#include <Common/InetAddr.h>
#include <Common/Init.h>
#include <Common/Logger.h>
#include <Common/Serialization.h>
#include <Common/System.h>
#include <Common/Usage.h>

#include <Hypertable/Master/MetaLogDefinitionMaster.h>

#include <Hypertable/RangeServer/MetaLogDefinitionRangeServer.h>
#include <Hypertable/RangeServer/MetaLogEntityRange.h>

#include <Hypertable/Lib/MetaLog.h>
#include <Hypertable/Lib/MetaLogReader.h>

#include <boost/algorithm/string.hpp>
#include <boost/any.hpp>

#include <re2/re2.h>

#include <iostream>
#include <string>
#include <vector>

using namespace Hypertable;
using namespace Config;
using namespace std;

namespace {

  struct AppPolicy : Config::Policy {
    static void init_options() {
      cmdline_desc("Usage: %s [options]\n\nRequires connection to the fs broker and hyperspace.\n\nOptions")
      .add_options()
        ("dump-namemap", "Dumps the name mapping")
        ("dump-all-cellstores", "Dumps all the cell stores")
        ("dump-cellstores", str(), "Dumps the cell stores matching the namespace/table preffix or /regexp/")
        ("dump-all-logs", "Dumps all the logs")
        ("dump-logs", str(), "Dumps the logs matching the namespace/table preffix or /regexp/")
        ("recover", str(), "Creates a recovery script matching the namespace/table preffix or /regexp/")
        ("include-row", str()->default_value(""), "include table/row filter, preffix or /regexp/")
        ("exclude-row", str()->default_value(""), "exclude include table/row filter, preffix or /regexp/")
        ("include-cf", str()->default_value(""), "include table/column family filter, preffix or /regexp/")
        ("exclude-cf", str()->default_value(""), "exclude table/column family filter, preffix or /regexp/")
        ("include-cq", str()->default_value(""), "include table/column qualifier, preffix or /regexp/")
        ("exclude-cq", str()->default_value(""), "exclude table/column qualifier, preffix or /regexp/")
        ("include-rs", str()->default_value(""), "include range server for dump logs, preffix or /regexp/")
        ("exclude-rs", str()->default_value(""), "exclude range server for dump logs, preffix or /regexp/")
        ("skip-empty-cells", "skips cells with no value")
        ("skip-deletes", "skips deletes")
        ("drop-table-if-exists", "adds a drop table statements into the recovery script")
        ("legacy-create-namespace", "not using the CREATE NAMESPACE IF NOT EXISTS syntax for the recovery script")
        ("cellstore-dir", str()->default_value("hypertable"), "The top level cell store directory, relative to FsBroker.Local.Root");
    }
    static void init() {
    }
  };

  typedef Meta::list<AppPolicy, FsClientPolicy, HyperspaceClientPolicy, DefaultCommPolicy> Policies;

  typedef std::map<String, String> StringMap;
  StringMap id_to_name_cache;

  String get_table_name(String id) {
    if (id[0] != '/')
      id = "/" + id;
    StringMap::const_iterator it = id_to_name_cache.find(id);
    if (it == id_to_name_cache.end()) {
        NameIdMapper mapper(Global::hyperspace, "/hypertable");
        String name;
        mapper.id_to_name(id, name, 0);
        id_to_name_cache.insert(StringMap::value_type(id, name));
        return name;
    }
    return it->second;
  }

  typedef std::shared_ptr<std::ofstream> OfstreamPtr;
  typedef std::map<String, OfstreamPtr> OfstreamMap;

  void replace_all_r(String& str, const String& what, const String& rewrite) {
    String s;
    do {
      s = str; 
      boost::replace_all(str, what, rewrite);
    }
    while (s != str);
  }

  void make_fname(String& fname) {
    String search = " ";
    const char invalid[] = "[\\~#%&*{}/:<>?|\"-].,;'";
    for( int i = 0; i < sizeof(invalid); ++i ) {
      search[0] = invalid[i];
      boost::replace_all(fname, search, "_");
    }
    replace_all_r(fname, "__", "_");
    boost::trim_if(fname, boost::is_any_of("_"));
  }

  OfstreamPtr get(OfstreamMap& map, const String& name) {
    OfstreamMap::const_iterator it = map.find(name);
    if (it == map.end()) {
        OfstreamPtr p = std::make_shared<std::ofstream>(name, ios::out|ios::trunc|ios::binary);
        *p << "#timestamp\trow\tcolumn\tvalue\n";
        map.insert(OfstreamMap::value_type(name, p));
        return p;
    }
    return it->second;
  }

  template<class Predicates, class Operator>
  void walk_filesystem(String name, Predicates& pr, Operator& op) {
    boost::trim_if(name, boost::is_any_of("/\\"));
    name = "/" + name;
    std::vector<Filesystem::Dirent> listing;
    Global::dfs->readdir(name, listing);
    for (const Filesystem::Dirent item : listing) {
      if (item.is_dir)
        walk_filesystem(name + "/" + item.name, pr, op);
      else if (pr(name, item.name))
        if (!op(name, item.name))
          break;
    }
  }

  void get_table_schema(const String &id, SchemaPtr& schema) {
    String tablefile = "/hypertable/tables" + id;
    DynamicBuffer value_buf;
    Global::hyperspace->attr_get(tablefile, "schema", value_buf);
    schema.reset( Schema::new_instance((const char *)value_buf.base));
  }

  void get_table_schema(const String &id, const String &table_name, bool xml, String& result) {
    SchemaPtr schema;
    get_table_schema(id, schema);
    result = xml ? schema->render_xml(true) : schema->render_hql(table_name);
    boost::replace_all(result, "\r", "");
    boost::replace_all(result, "\n", "");
    replace_all_r(result, "  ", " ");
    boost::replace_all(result, "( ", "(");
    boost::replace_all(result, "> <", "><");
  }

  struct Predicates {
    String include_row;
    String include_cf;
    String include_cq;
    String include_rs;
    String exclude_row;
    String exclude_cf;
    String exclude_cq;
    String exclude_rs;
  };

  struct Namemap {
    Namemap(std::vector<String>& _lines)
      : lines(_lines) { }
    void operator()(const String &path, const String &name, const String &id, const NamespaceListing& item) {
      if (!item.is_namespace) {
        String schema;
        get_table_schema(id, item.name, false, schema);
        lines.push_back(name + "\t" + id + "\t" + schema);
      }
    }
  private:
    std::vector<String>& lines;
  };

  struct PreffixFilter {
    PreffixFilter(const String& _preffix)
      : preffix(_preffix) {

      if (preffix.length() > 2 && preffix.front() == '/' && preffix.back() == '/') {
        preffix.erase(preffix.begin());
        preffix.pop_back();
        regex = std::make_shared<RE2>(preffix);
      }
    }
    bool match(const String& value) const {
      return regex
        ? RE2::PartialMatch(value, *regex)
        : preffix.empty() || boost::algorithm::starts_with(value, preffix);
    }
    bool no_match(const String& value) const {
      return regex
        ? !RE2::PartialMatch(value, *regex)
        : preffix.empty() || !boost::algorithm::starts_with(value, preffix);
    }
  private:
    std::shared_ptr<RE2> regex;
    String preffix;
  };

  struct RebuildTables : PreffixFilter {
    RebuildTables(std::ostream& _os, const String& preffix)
      : PreffixFilter(preffix), os(_os), drop_table_if_exists(has("drop-table-if-exists"))
      , legacy_create_namespace(has("legacy-create-namespace")){ }
    void operator()(const String &path, const String &name, const String &id, const NamespaceListing& item) {
      if(match(name)) {
        if (!item.is_namespace) {
          String schema;
          get_table_schema(id, item.name, false, schema);
          if (!(path.empty() || path == "/"))
            os << "USE '" + path + "';" << endl;
          if (drop_table_if_exists)
            os << "DROP TABLE IF EXISTS '" + item.name + "';" << endl;
          os << schema << ";" << endl;
          if (!(path.empty() || path == "/"))
            os << "USE '/';" << endl;
        }
        else {
          os << "CREATE NAMESPACE '" + name + "'";
          if(!legacy_create_namespace)
            os << " IF NOT EXIST";
          os << ";" << endl;
        }
      }
    }
  private:
    std::ostream& os;
    bool drop_table_if_exists;
    bool legacy_create_namespace;

  };

  template<class Operator>
  void walk_listing(Operator& op, const String &name, const String &id, const std::vector<NamespaceListing>& listing) {
    for (const NamespaceListing& item : listing) {
      String _name = name + "/" + item.name;

      if(_name == "/sys" || _name == "/tmp")
        continue;

      String _id = id + "/" + item.id;
      op(name, _name, _id, item);
      walk_listing<Operator>(op, _name, _id, item.sub_entries);
    }
  }

  void dump_listing(std::ostream& os, const String &name, const String &id, const std::vector<NamespaceListing>& listing) {
    std::vector<String> lines;
    Namemap nm(lines);
    walk_listing(nm, name, id, listing);
    std::sort(lines.begin(), lines.end());
    for (const String& line : lines) {
      os << line << endl;
    }
  }

  int get_namemap(std::vector<String>& lines) {
    try {
      NameIdMapper mapper(Global::hyperspace, "/hypertable");
      std::vector<NamespaceListing> listing;
      mapper.id_to_sublisting("/", true, listing);

      Namemap nm(lines);
      walk_listing(nm, "", "", listing);
    }
    catch (Exception &e) {
      HT_ERROR_OUT << e << HT_END;
      return 1;
    }
    return 0;
  }

  int dump_namemap() {
    try {
      NameIdMapper mapper(Global::hyperspace, "/hypertable");
      std::vector<NamespaceListing> listing;
      mapper.id_to_sublisting("/", true, listing);

      std::ofstream of("namemap.txt", ios::out|ios::trunc);
      dump_listing(of, "", "", listing);
    }
    catch (Exception &e) {
      HT_ERROR_OUT << e << HT_END;
      return 1;
    }
    return 0;
  }

  int rebuild_namemap(std::ostream& os, const String& preffix) {
    try {
      NameIdMapper mapper(Global::hyperspace, "/hypertable");
      std::vector<NamespaceListing> listing;
      mapper.id_to_sublisting("/", true, listing);

      RebuildTables rt(os, preffix);
      walk_listing(rt, "", "", listing);
    }
    catch (Exception &e) {
      HT_ERROR_OUT << e << HT_END;
      return 1;
    }
    return 0;
  }

  struct IsCellStore {
    bool operator()(const String &path, const String &fname) {
      return boost::algorithm::starts_with(fname, "cs");
    }
  };

  struct IsLog {
    IsLog(const Predicates& predicates)
      : include_rs(predicates.include_rs), exclude_rs(predicates.exclude_rs) { }
    bool operator()(const String &path, const String &fname) {
      if (!isdigit(fname[0]))
        return false;
      String name = path + fname;
      return include_rs.match(name)
          && exclude_rs.no_match(name);
    }
  private:
    PreffixFilter include_rs;
    PreffixFilter exclude_rs;
  };

  struct KeyFilter {
    KeyFilter(const Predicates& predicates)
      : include_row(predicates.include_row), include_cf(predicates.include_cf), include_cq(predicates.include_cq)
      , exclude_row(predicates.exclude_row), exclude_cf(predicates.exclude_cf), exclude_cq(predicates.exclude_cq)
      , skip_empty_cells(has("skip-empty-cells")), skip_deletes(has("skip-deletes")) { }
    bool match(const String& table, const Key& key, const ColumnFamilySpec* _cf, size_t value_len) const {
      if (skip_empty_cells && key.flag == FLAG_INSERT && value_len == 0)
        return false;
      if (skip_deletes && key.flag != FLAG_INSERT)
        return false;
      String table_row = table + "/" + key.row;
      String table_cf = table + "/" + _cf->get_name();
      return include_row.match(table_row)
          && include_cf.match(table_cf)
          && (key.column_qualifier == 0 || key.column_qualifier_len == 0 || include_cq.match(table + "/" + key.column_qualifier))
          && exclude_row.no_match(table_row)
          && exclude_cf.no_match(table_cf)
          && (key.column_qualifier == 0 || key.column_qualifier_len == 0 || exclude_cq.no_match(table + "/" + key.column_qualifier));
    }
  private:
    PreffixFilter include_row;
    PreffixFilter include_cf;
    PreffixFilter include_cq;
    PreffixFilter exclude_row;
    PreffixFilter exclude_cf;
    PreffixFilter exclude_cq;
    bool skip_empty_cells;
    bool skip_deletes;
  };

  struct DumpCellStore : KeyFilter {
    DumpCellStore(const Predicates& predicates)
      : KeyFilter(predicates), os(0) { }
    DumpCellStore(std::ostream* _os, const Predicates& predicates)
      : KeyFilter(predicates)
      , os(_os) { }
    bool operator()(const String &path, const String &fname) {
      dump_cellstore(path, fname);
      return true;
    }
  private:
    int dump_cellstore(const String &path, const String &fname) {
      String name = path + "/" + fname;

      uint32_t cells = 0;
      try {

        String id;
        String _path(path);
        char *end_nl;
        for (char *part = strtok_r((char*)_path.c_str(), "/", &end_nl); part; part = strtok_r(0, "/", &end_nl)) {
          if( isdigit(*part)) {
            id += "/";
            id += part;
          }
          else if (!id.empty())
            break;
        }

        String table_name = get_table_name(id);
        if (boost::algorithm::starts_with(table_name, "sys/") ||
            boost::algorithm::starts_with(table_name, "tmp/"))
          return 0;

        cout << "Dumping " << name << " (" << table_name << ") ... " << flush;

        SchemaPtr schema;
        get_table_schema(id, schema);

        CellStorePtr cellstore = CellStoreFactory::open(name, 0, 0);

        uint64_t key_count = 0;
        ByteString key, value;
        uint8_t *bsptr;
        size_t bslen;
        size_t buf_len = 1024 * 1024;
        char *buf = new char [ buf_len ];
        Key key_comps;

        LoadDataEscape row_escaper;
        LoadDataEscape escaper;
        const char *unescaped_buf, *row_unescaped_buf;
        size_t unescaped_len, row_unescaped_len;

        make_fname(name);
        std::ofstream of(name + ".tsv", ios::out|ios::trunc|ios::binary);

        of << "#timestamp\trow\tcolumn\tvalue\n";

        ScanContextPtr scan_ctx(new ScanContext(schema));
        CellListScannerPtr scanner = cellstore->create_scanner(scan_ctx.get());
        while (scanner->get(key_comps, value)) {
          ColumnFamilySpec* cf = schema->get_column_family(key_comps.column_family_code, false);
          /*if (key_comps.flag != FLAG_INSERT) {
            cout << "DELETES ignored on table " << table_name 
                 << " row=" << key_comps.row 
                 << "cf=" << cf->get_name();
            if(key_comps.column_qualifier != 0 && key_comps.column_qualifier_len != 0)
               cout << " cq=" << key_comps.column_qualifier;
            cout << endl;

            continue;
          }*/
          if (cf) {
            bslen = value.decode_length((const uint8_t **)&bsptr);
            if (match(table_name, key_comps, cf, bslen)) {
              row_escaper.escape(key_comps.row, key_comps.row_len,
                                  &row_unescaped_buf, &row_unescaped_len);

              of << key_comps.timestamp << "\t" << row_unescaped_buf << "\t" << cf->get_name();
              if (key_comps.column_qualifier && *key_comps.column_qualifier) {
                escaper.escape(key_comps.column_qualifier, key_comps.column_qualifier_len,
                                &unescaped_buf, &unescaped_len);
                of << ":";
                of.write(unescaped_buf, unescaped_len);
              }

              if (bslen >= buf_len) {
                delete [] buf;
                buf_len = bslen + 256;
                buf = new char [ buf_len ];
              }
              memcpy(buf, bsptr, bslen);
              buf[bslen] = 0;
              of << "\t";
              if (key_comps.flag == FLAG_INSERT) {
                escaper.escape(buf, bslen, &unescaped_buf, &unescaped_len);
                of.write(unescaped_buf, unescaped_len);
              }
              else {
                switch(key_comps.flag) {
                  case FLAG_DELETE_ROW:
                    of << "DELETE_ROW";
                    break;
                  case FLAG_DELETE_COLUMN_FAMILY:
                    of << "DELETE_COLUMN_FAMILY";
                    break;
                  case FLAG_DELETE_CELL:
                    of << "DELETE_CELL";
                    break;
                  case FLAG_DELETE_CELL_VERSION:
                    of << "DELETE_CELL_VERSION";
                    break;
                  default:
                    cout << "Unexpected key flag 0x" << std::hex << key_comps.flag << std::endl;
                }
              }
              of << "\n";

              ++cells;
            }
          }
          scanner->forward();
        }

        if (!cells) {
          of.close();
          DeleteFile((name + ".tsv").c_str());
        }
        else if (os)
          *os << "LOAD DATA INFILE '" << name << ".tsv' INTO TABLE '/" << table_name << "';" << endl;

        delete buf;
      }
      catch (Exception &e) {
        cout << cells << "cells - aborting:" << endl;
        HT_ERROR_OUT << e << HT_END;
        return 1;
      }

      cout << cells << "cells" << endl;
      return 0;
    }
    private:
      std::ostream* os;
  };

  struct DumpLog : KeyFilter, PreffixFilter {
    DumpLog(const Predicates& predicates)
      : KeyFilter(predicates), PreffixFilter(""), os(0) { }
    DumpLog(const String& preffix, const Predicates& predicates)
      : KeyFilter(predicates), PreffixFilter(preffix), os(0) { }
    DumpLog(std::ostream* _os, const String& preffix, const Predicates& predicates)
      : KeyFilter(predicates), PreffixFilter(preffix), os(_os) { }
    bool operator()(const String &path, const String &fname) {
      if (!boost::algorithm::ends_with(path, "/user") && path.find("/user/") == String::npos) {
        return true;
      }

      dump_log(path);
      return false;
    }
  private:
    int dump_log(String path) {
      cout << "Dumping " << path << "/ ... " << flush;

      uint32_t cells = 0;
      try {
        OfstreamMap ostreams;
        CommitLogReader clr(Global::dfs, path);

        make_fname(path);

        BlockHeaderCommitLog header;
        const uint8_t *base;
        size_t len;
        const uint8_t *ptr, *end;
        TableIdentifier table_id;
        ByteString bs;
        Key key;
        String value;
        uint32_t blockno=0;

        LoadDataEscape row_escaper;
        LoadDataEscape escaper;
        const char *unescaped_buf, *row_unescaped_buf;
        size_t unescaped_len, row_unescaped_len;
 
        uint8_t *bsptr;
        size_t bslen;
        size_t buf_len = 1024 * 1024;
        char *buf = new char [ buf_len ];

        while (clr.next(&base, &len, &header)) {
          if (!header.check_magic(CommitLog::MAGIC_DATA)) {
            cout << cells << "cells - aborting, got invalid header" << endl;
            return 1;
          }

          ptr = base;
          end = base + len;

          size_t len_saved = len;
          try {
            table_id.decode(&ptr, &len);
          }
          catch (Exception &e) {
            if (e.code() == Error::PROTOCOL_ERROR) {
              len = len_saved;
              ptr = base;
              legacy_decode(&ptr, &len, &table_id);
            }
            else {
              cout << cells << "cells - aborting:" << endl;
              HT_ERROR_OUT << e << HT_END;
              return 1;
            }
          }

          String table_name = get_table_name(table_id.id);
          if (!boost::algorithm::starts_with(table_name, "sys/") &&
              !boost::algorithm::starts_with(table_name, "tmp/") &&
               PreffixFilter::match("/" + table_name)) {

            String fname(path);
            fname += "_";
            fname += table_id.id;
            fname +=  + ".tsv";
            make_fname(fname);

            SchemaPtr schema;
            try {
              get_table_schema(String("/") + table_id.id, schema);
            }
            catch (Exception &) {
              cout << "\nMissing table schema for " << String("/") + table_id.id << ", continue..";
              continue;
            }

            OfstreamPtr of;

            while (ptr < end) {
              bs.ptr = ptr;
              key.load(bs);
              bs.next();
              ColumnFamilySpec* cf = schema->get_column_family(key.column_family_code, false);
              if (cf) {
                bslen = bs.decode_length((const uint8_t **)&bsptr);
                if (KeyFilter::match(table_name, key, cf, bslen)) {
                  if(!of) {
                    size_t count = ostreams.size();
                    of = get(ostreams, fname);
                    if (os && count < ostreams.size())
                      *os << "LOAD DATA INFILE '" << fname << "' INTO TABLE '/" << table_name << "';" << endl;
                  }

                  row_escaper.escape(key.row, key.row_len,
                                      &row_unescaped_buf, &row_unescaped_len);

                  *of << key.timestamp << "\t" << row_unescaped_buf << "\t" << cf->get_name();
                  if (key.column_qualifier && *key.column_qualifier) {
                    escaper.escape(key.column_qualifier, key.column_qualifier_len,
                                    &unescaped_buf, &unescaped_len);
                    *of << ":";
                    of->write(unescaped_buf, unescaped_len);
                  }
                  *of << "\t";
                  if (bslen >= buf_len) {
                    delete [] buf;
                    buf_len = bslen + 256;
                    buf = new char [ buf_len ];
                  }
                  memcpy(buf, bsptr, bslen);
                  buf[bslen] = 0;

                  if (key.flag == FLAG_INSERT) {
                    escaper.escape(buf, bslen, &unescaped_buf, &unescaped_len);
                    of->write(unescaped_buf, unescaped_len);
                  }
                  else {
                    switch(key.flag) {
                      case FLAG_DELETE_ROW:
                        *of << "DELETE_ROW";
                        break;
                      case FLAG_DELETE_COLUMN_FAMILY:
                        *of << "DELETE_COLUMN_FAMILY";
                        break;
                      case FLAG_DELETE_CELL:
                        *of << "DELETE_CELL";
                        break;
                      case FLAG_DELETE_CELL_VERSION:
                        *of << "DELETE_CELL_VERSION";
                        break;
                      default:
                        cout << "Unexpected key flag 0x" << std::hex << key.flag << std::endl;
                    }
                  }
                  *of << "\n";
                }
              }

              bs.next();
              ptr = bs.ptr;

              if (ptr > end) {
                HT_THROW(Error::REQUEST_TRUNCATED, "Problem decoding value");
              }
              blockno++;
            }
          }
        }
        delete buf;

        cout << cells << "cells" << endl;
        return 0;
      }
      catch (Exception &e) {
        cout << cells << "cells - aborting:" << endl;
        HT_ERROR_OUT << e << HT_END;
        return 1;
      }
    }
    std::ostream* os;
  };

  /*
  struct ValidateCellStore {
    bool operator()(const String &path, const String &fname) {
      String name = path + "/" + fname;
      cout << "Checking " << name << " ... " << flush;
      try {
        CellStorePtr cellstore = CellStoreFactory::open(name, 0, 0);
        ScanContextPtr scan_ctx(new ScanContext());
        CellListScannerPtr scanner = cellstore->create_scanner(scan_ctx.get());

        ByteString value;
        Key key_comps;
        while (scanner->get(key_comps, value)) {
          scanner->forward();
        }
      }
      catch (Exception &e) {
        HT_ERROR_OUT << e << HT_END;
        return true;
      }
      cout << "valid" << endl;
      return true;
    }
  };

  struct ValidateLog {
    ValidateLog() {
      MetaLog::DefinitionPtr def = make_shared<MetaLog::DefinitionRangeServer>("");
      defmap[def->name()] = def;
      def = make_shared<MetaLog::DefinitionMaster>("");
      defmap[def->name()] = def;
    }
    bool operator()(const String &path, const String &fname) {
      if (boost::algorithm::ends_with(path, "log/mml") || 
          boost::algorithm::ends_with(path, "log/rsml")) {

        try {
          String name = path + "/" + fname;
          cout << "Checking " << name << " ... " << flush;

          String type;
          determine_log_type(name, type);

          auto iter = defmap.find(type);
          if (iter == defmap.end()) {
            cout << "No definition for log type '" << type << "'" << endl;
            return true;
          }
          MetaLog::DefinitionPtr def = iter->second;

          MetaLog::ReaderPtr rsml_reader = 
            make_shared<MetaLog::Reader>(Global::dfs, def, MetaLog::Reader::LOAD_ALL_ENTITIES);
          rsml_reader->load_file(name);
          std::vector<MetaLog::EntityPtr> entities;
          rsml_reader->get_all_entities(entities);

          cout << "valid" << endl;
        }
        catch (Exception &e) {
          HT_ERROR_OUT << e << HT_END;
          return true;
        }
        return true;
      }

      cout << "Checking " << path << "/ ... " << flush;
      try {
        CommitLogReader clr(Global::dfs, path);
        CommitLogBlockInfo binfo;
        BlockHeaderCommitLog header;
        while (clr.next_raw_block(&binfo, &header)) {
          if(!header.check_magic(CommitLog::MAGIC_DATA)) {
            cout << "invalid block header" << endl;
          }

          if (binfo.error != Error::OK) {
            cout << "block error " << Error::get_text(binfo.error) << endl;
          }
        }
        cout << "valid" << endl;
      }
      catch (Exception &e) {
        HT_ERROR_OUT << e << HT_END;
        return false;
      }
      return false;
    }

  private:

    int determine_log_type(const String &fname, String &type) {
      int fd = Global::dfs->open(fname, Filesystem::OPEN_FLAG_VERIFY_CHECKSUM);
      MetaLog::Header header;
      uint8_t buf[MetaLog::Header::LENGTH];
      const uint8_t *ptr = buf;
      size_t remaining = MetaLog::Header::LENGTH;

      size_t nread = Global::dfs->read(fd, buf, MetaLog::Header::LENGTH);

      if (nread != MetaLog::Header::LENGTH) {
        cout << format("Short read of header for '%s' (expected %d, got %d)",
                  fname.c_str(), (int)MetaLog::Header::LENGTH, (int)nread) << endl;

        return 1;
      }

      header.decode(&ptr, &remaining);
      type = header.name;
      Global::dfs->close(fd);
      return 0;
    }

    std::unordered_map<String, MetaLog::DefinitionPtr> defmap;
  };
  */

}

int main(int argc, char **argv) {
  cout << "ht4w - Validating and recovery utility\nCopyright (C) 2010-2016 Thalmann Software & Consulting\nhttp://www.softdev.ch\n\n";

  try {
    init_with_policies<Policies>(argc, argv);

    String toplevelCellStoreDirectory = get_str("cellstore-dir");
    boost::trim_if(toplevelCellStoreDirectory, boost::is_any_of("/\\"));

    Comm *comm = Comm::instance();
    Global::hyperspace = make_shared<Hyperspace::Session>(comm, properties);

    if (has("dump-namemap"))
      if (dump_namemap())
        quick_exit(EXIT_FAILURE);

    if (has("dump-all-cellstores") || 
        has("dump-cellstores") || 
        has("dump-all-logs") || 
        has("dump-logs") ||
        has("recover")) {

      int timeout = get_i32("timeout");
      ConnectionManagerPtr conn_mgr = make_shared<ConnectionManager>();
      FsBroker::Lib::ClientPtr dfs = std::make_shared<FsBroker::Lib::Client>(conn_mgr, properties);
      if (!dfs->wait_for_connection(timeout)) {
        cout << "timed out waiting for FS broker" << endl;
        quick_exit(EXIT_FAILURE);
      }

      Global::dfs = dfs;
      Global::memory_tracker = new MemoryTracker(0, 0);

      Predicates predicates;
      predicates.include_row = get_str("include-row");
      predicates.exclude_row = get_str("exclude-row");

      predicates.include_cf = get_str("include-cf");
      predicates.exclude_cf = get_str("exclude-cf");

      predicates.include_cq = get_str("include-cq");
      predicates.exclude_cq = get_str("exclude-cq");

      predicates.include_rs = get_str("include-rs");
      predicates.exclude_rs = get_str("exclude-rs");

      if (has("dump-all-cellstores") ||
          has("dump-cellstores")) {

        if (has("dump-cellstores")) {
          String preffix = get_str("dump-cellstores");
          if (!preffix.empty()) {
            std::vector<String> lines;
            if (get_namemap(lines))
              quick_exit(EXIT_FAILURE);
            PreffixFilter filter(preffix);
            for (const String& line : lines) {
              String _line(line);
              char *end_nl;
              strtok_r((char*)_line.c_str(), "\t", &end_nl); 
              if (filter.match(_line)) {
                String id = strtok_r(0, "\t", &end_nl);
                boost::trim_if(id, boost::is_any_of("/"));
                walk_filesystem(
                  toplevelCellStoreDirectory + "/tables/" + id, 
                  IsCellStore(),
                  DumpCellStore(predicates));
              }
            }
          }
        }
        else
          walk_filesystem(
          toplevelCellStoreDirectory + "/tables", 
          IsCellStore(), 
          DumpCellStore(predicates));
      }

      if (has("dump-all-logs") ||
          has("dump-logs")) {

        if (has("dump-logs")) {
          String preffix = get_str("dump-logs");
          if (!preffix.empty()) {
            walk_filesystem(
              toplevelCellStoreDirectory + "/servers", 
              IsLog(predicates), 
              DumpLog(preffix, predicates));
          }
        }
        else
          walk_filesystem(
            toplevelCellStoreDirectory + "/servers", 
            IsLog(predicates), 
            DumpLog(predicates));
      }

      if (has("recover")) {
        String preffix = get_str("recover");
        String fname = "recover" + preffix;
        make_fname(fname);
        fname += ".hql";
        std::ofstream of(fname, ios::out|ios::trunc);
        of << "USE '/';" << endl;

        rebuild_namemap(of, preffix);

        std::vector<String> lines;
        if (get_namemap(lines))
          quick_exit(EXIT_FAILURE);
        PreffixFilter filter(preffix);
        for (const String& line : lines) {
          String _line(line);
          char *end_nl;
          strtok_r((char*)_line.c_str(), "\t", &end_nl); 
          if (filter.match(_line)) {
            String id = strtok_r(0, "\t", &end_nl);
            boost::trim_if(id, boost::is_any_of("/"));
            walk_filesystem(
              toplevelCellStoreDirectory + "/tables/" + id, 
              IsCellStore(), 
              DumpCellStore(&of, predicates));
          }
        }

        walk_filesystem(
          toplevelCellStoreDirectory + "/servers", 
          IsLog(predicates), 
          DumpLog(&of, preffix, predicates));
      }
    }
  }
  catch (Exception &e) {
    HT_ERROR_OUT << e << HT_END;
    quick_exit(EXIT_FAILURE);
  }

  quick_exit(0);
}
