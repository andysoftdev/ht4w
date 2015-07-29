/*
 * Copyright (C) 2007-2015 Hypertable, Inc.
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
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

#include <Common/Compat.h>
#include <Common/FileUtils.h>
#include <Common/Logger.h>
#include <Common/System.h>

#include <cstdlib>
#include <iostream>
#include <string>

extern "C" {
#include <sys/types.h>
#include <unistd.h>
}

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
    "./offset_test.hql",
    "./offset_test.golden",
    "./timeorder_test.hql",
    "./timeorder_test.golden",
    "./indices_test.hql",
    "./indices_test.golden",
    0
  };
}

int main(int argc, char **argv) {
  std::string cmd_str;

  System::initialize(argv[0]);

  for (int i=0; required_files[i]; i++) {
    if (!FileUtils::exists(required_files[i])) {
      HT_ERRORF("Unable to find '%s'", required_files[i]);
      quick_exit(EXIT_FAILURE);
    }
  }

  /**
   *  secondary INDEX tests
   */
#ifndef _WIN32
  cmd_str = "./ht_hypertable --test-mode --config hypertable.cfg "
      "< indices_test.hql > indices_test.output 2>&1";
  if (system(cmd_str.c_str()) != 0)
    quick_exit(EXIT_FAILURE);

  cmd_str = "diff indices_test.output indices_test.golden";
#else
  cmd_str = "..\\hypertable.exe --test-mode --config hypertable.cfg "
      "< indices_test.hql > indices_test.output 2>&1";
  if (system(cmd_str.c_str()) != 0)
    _exit(1);

  cmd_str = "sed.exe -e s/hypertable.exe/hypertable/ig indices_test.output > indices_test.sed.output";
  if (system(cmd_str.c_str()) != 0)
    _exit(1);

  cmd_str = "fc indices_test.sed.output indices_test.golden";
#endif
  if (system(cmd_str.c_str()) != 0)
    quick_exit(EXIT_FAILURE);

  // Check value index before and after REBUILD
#ifndef _WIN32
  cmd_str = "diff products-value-index-before.tsv products-value-index-after.tsv";
#else
  cmd_str = "fc products-value-index-before.tsv products-value-index-after.tsv";
#endif
  if (system(cmd_str.c_str()) != 0)
    quick_exit(EXIT_FAILURE);

  // Check qualifier index before and after REBUILD
#ifndef _WIN32
  cmd_str = "diff products-qualifier-index-before.tsv products-qualifier-index-after.tsv";
#else
  cmd_str = "fc products-qualifier-index-before.tsv products-qualifier-index-after.tsv";
#endif
  if (system(cmd_str.c_str()) != 0)
    quick_exit(EXIT_FAILURE);

  /**
   *  offset-test
   */
#ifndef _WIN32
  cmd_str = "./ht_hypertable --test-mode --config hypertable.cfg "
      "< offset_test.hql > offset_test.output 2>&1";
  if (system(cmd_str.c_str()) != 0)
    quick_exit(EXIT_FAILURE);

  cmd_str = "diff offset_test.output offset_test.golden";
#else
  cmd_str = "..\\hypertable.exe --test-mode --config hypertable.cfg "
      "< offset_test.hql > offset_test.output 2>&1";
  if (system(cmd_str.c_str()) != 0)
    _exit(1);

  cmd_str = "sed.exe -e s/hypertable.exe/hypertable/ig offset_test.output > offset_test.sed.output";
  if (system(cmd_str.c_str()) != 0)
    _exit(1);

  cmd_str = "fc offset_test.sed.output offset_test.golden";
#endif
  if (system(cmd_str.c_str()) != 0)
    quick_exit(EXIT_FAILURE);

  /**
   *  TIME_ORDER tests
   */
#ifndef _WIN32
  cmd_str = "./ht_hypertable --test-mode --config hypertable.cfg "
      "< timeorder_test.hql > timeorder_test.output 2>&1";
  if (system(cmd_str.c_str()) != 0)
    quick_exit(EXIT_FAILURE);

  cmd_str = "diff timeorder_test.output timeorder_test.golden";
#else
  cmd_str = "..\\hypertable.exe --test-mode --config hypertable.cfg "
      "< timeorder_test.hql > timeorder_test.output 2>&1";
  if (system(cmd_str.c_str()) != 0)
    _exit(1);

  cmd_str = "sed.exe -e s/hypertable.exe/hypertable/ig timeorder_test.output > timeorder_test.sed.output";
  if (system(cmd_str.c_str()) != 0)
    _exit(1);

  cmd_str = "fc timeorder_test.sed.output timeorder_test.golden";
#endif
  if (system(cmd_str.c_str()) != 0)
    quick_exit(EXIT_FAILURE);

  /**
   *  hypertable_test
   */
#ifndef _WIN32
  cmd_str = "./ht_hypertable --test-mode --config hypertable.cfg "
      "< hypertable_test.hql > hypertable_test.output 2>&1";
  if (system(cmd_str.c_str()) != 0)
    quick_exit(EXIT_FAILURE);

  cmd_str = "diff hypertable_test.output hypertable_test.golden";
#else
  cmd_str = "..\\hypertable.exe --test-mode --config hypertable.cfg "
      "< hypertable_test.hql > hypertable_test.output 2>&1";
  if (system(cmd_str.c_str()) != 0)
    quick_exit(EXIT_FAILURE);

  cmd_str = "sed.exe -e s/hypertable.exe/hypertable/ig hypertable_test.output > hypertable_test.sed.output";
  if (system(cmd_str.c_str()) != 0)
    quick_exit(EXIT_FAILURE);

  cmd_str = "fc hypertable_test.sed.output hypertable_test.golden";
#endif
  if (system(cmd_str.c_str()) != 0)
    quick_exit(EXIT_FAILURE);

  quick_exit(EXIT_SUCCESS);
}
