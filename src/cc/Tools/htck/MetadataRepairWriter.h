/* -*- c++ -*-
 * HYPERTABLE, INC. CONFIDENTIAL
 * __________________
 * 
 * Copyright (C) 2007-2014 Hypertable, Inc.
 * All Rights Reserved.
 * 
 * NOTICE:  All information contained herein is, and remains
 * the property of Hypertable Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Hypertable Incorporated and its
 * suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or
 * copyright law.  Dissemination of this information or reproduction
 * of this material is strictly forbidden unless prior written
 * permission is obtained from Hypertable Incorporated.
 */

#ifndef Tools_htck_MetadataRepairWriter_h
#define Tools_htck_MetadataRepairWriter_h

#include <Hypertable/Lib/LoadDataEscape.h>

#include <Tools/htck/RangeInfo.h>

#include <cstdio>
#include <string>

namespace Hypertable {

  class MetadataRepairWriter {

  public:

    MetadataRepairWriter() {
      if ((m_fp = fopen("metadata.repair.tsv", "w+")) == 0) {
	perror("metadata.repair.tsv");
	exit(1);
      }
    }

    ~MetadataRepairWriter() { fclose(m_fp); }

    void write_location(RangeInfo *ri) {
      write_tsv_line(ri->table_id, ri->end_row, ri->end_row_len, "Location", 
		     ri->location, strlen(ri->location));
    }

    void write_start_row(RangeInfo *ri) {
      write_tsv_line(ri->table_id, ri->end_row, ri->end_row_len, "StartRow", 
		     ri->start_row, ri->start_row_len);
    }

    void delete_row(const std::string &tid, const std::string &end_row,
		    bool escape) {
      write_tsv_line(tid.c_str(), end_row.c_str(), end_row.length(),
		     "", "	DELETE", strlen("	DELETE"), escape);
    }

    size_t lines_written() { return m_lines_written; }

  private:

    void write_tsv_line(const char *tid, const char *end_row, size_t end_row_len,
			const char *column, const char *value, size_t value_len,
			bool escape=true) {
      LoadDataEscape escaper;
      const char *escaped_str;
      size_t escaped_len;

      if (!m_header_written)
	write_header();

      fwrite(tid, strlen(tid), 1, m_fp);
      fwrite(":", 1, 1, m_fp);
      if (escape)
	escaper.escape(end_row, end_row_len, &escaped_str, &escaped_len);
      else {
	escaped_str = end_row;
	escaped_len = end_row_len;
      }
      fwrite(escaped_str, escaped_len, 1, m_fp);
      fwrite("\t", 1, 1, m_fp);
      fwrite(column, strlen(column), 1, m_fp);
      fwrite("\t", 1, 1, m_fp);
      if (escape)
	escaper.escape(value, value_len, &escaped_str, &escaped_len);
      else {
	escaped_str = value;
	escaped_len = value_len;
      }
      fwrite(escaped_str, escaped_len, 1, m_fp);
      fwrite("\n", 1, 1, m_fp);
      m_lines_written++;
    }

    void write_header() {
      fwrite("#row\tcolumn\tvalue\n", strlen("#row\tcolumn\tvalue\n"), 1, m_fp);
      m_header_written = true;
    }

    FILE *m_fp;
    bool m_header_written {};
    size_t m_lines_written {};

  };

}

#endif // Tools_htck_MetadataRepairWriter_h
