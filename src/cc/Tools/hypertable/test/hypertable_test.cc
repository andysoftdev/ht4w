/**
 * Copyright (C) 2007 Doug Judd (Zvents, Inc.)
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
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
#include <cstdlib>
#include <iostream>
#include <string>

extern "C" {
#include <sys/types.h>
#include <unistd.h>
}

#include "Common/FileUtils.h"
#include "Common/Logger.h"
#include "Common/System.h"

using namespace Hypertable;
using namespace std;


namespace {
  const char *required_files[] = {
#ifndef _WIN32
    "./hypertable_test",
#endif
    "./hypertable.cfg",
    "./hypertable_test.hql",
    "./hypertable_test.golden",
    "./hypertable_select_gz_test.golden",
    "./hypertable_test.tsv",
    0
  };
}

int main(int argc, char **argv) {
  std::string cmd_str;

  System::initialize(argv[0]);

  for (int i=0; required_files[i]; i++) {
    if (!FileUtils::exists(required_files[i])) {
      HT_ERRORF("Unable to find '%s'", required_files[i]);
      return 1;
    }
  }

  /**
   *  hypertable_test
   */
#ifndef _WIN32
  cmd_str = "./hypertable --test-mode --config hypertable.cfg "
      "< hypertable_test.hql > hypertable_test.output 2>&1";
  if (system(cmd_str.c_str()) != 0)
    return 1;

  cmd_str = "diff hypertable_test.output hypertable_test.golden";
#else
  cmd_str = "..\\hypertable.exe --test-mode --config hypertable.cfg "
      "< hypertable_test.hql > hypertable_test.output 2>&1";
  if (system(cmd_str.c_str()) != 0)
    return 1;

  cmd_str = "sed.exe -e s/hypertable.exe/hypertable/g hypertable_test.output > hypertable_test.sed.output";
  if (system(cmd_str.c_str()) != 0)
    return 1;

  cmd_str = "fc hypertable_test.sed.output hypertable_test.golden";
#endif
  if (system(cmd_str.c_str()) != 0)
    return 1;

#ifndef _WIN32
  cmd_str = "gunzip -f hypertable_select_gz_test.output.gz";
#else
  cmd_str = "gzip -d -f hypertable_select_gz_test.output.gz";
#endif
  if (system(cmd_str.c_str()) != 0)
    return 1;

#ifndef _WIN32
  cmd_str = "diff hypertable_select_gz_test.output hypertable_select_gz_test.golden";
#else
  cmd_str = "fc hypertable_select_gz_test.output hypertable_select_gz_test.golden";
#endif
  if (system(cmd_str.c_str()) != 0)
    return 1;

  return 0;
}
