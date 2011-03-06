/**
 * Copyright (C) 2011 Andy Thalmann
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

#ifndef _WIN32
#error Platform isn't supported
#endif

#include "Common/Compat.h"
#include "Config.h"
#include "ServerUtils.h"
#include "ServiceUtils.h"

#include "Common/Logger.h"
#include "AsyncComm/Comm.h"

using namespace Hypertable;

int main(int argc, char **argv) {
  int exit = 0;
  try {
    Config::init(argc, argv);
    if (Config::silent())
      Logger::set_level(Logger::Priority::FATAL);

    if (Config::stop_all_services())
      ServiceUtils::stop_all_services();
    else if (Config::stop_service())
      ServiceUtils::stop_service();

    if (Config::stop_servers())
      ServerUtils::stop_servers();

    if (Config::kill_servers())
      ServerUtils::kill_servers();

    if (Config::install_service())
      ServiceUtils::install_service();
    else if (Config::uninstall_service())
      ServiceUtils::uninstall_service();

    if (Config::start_service())
      ServiceUtils::start_service();
    else if (Config::start_servers())
      ServerUtils::start_servers();
    else if (Config::join_servers())
      ServerUtils::join_servers();
    else if (!Config::install_service() &&
             !Config::uninstall_service() &&
             !Config::stop_all_services() &&
             !Config::stop_service() &&
             !Config::stop_servers() &&
             !Config::kill_servers())
      ServiceUtils::init_service();
  }
  catch (Exception &e) {
    HT_ERROR_OUT << e << HT_END;
    exit = 1;
  }
  Comm::destroy();
  Config::cleanup();
  return exit;
}
