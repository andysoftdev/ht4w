/* -*- c++ -*-
 * Copyright (C) 2007-2014 Hypertable, Inc.
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 3 of the
 * License, or any later version.
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

#ifndef Tools_master_client_MasterCommandInterpreter_h
#define Tools_master_client_MasterCommandInterpreter_h

#include "Common/String.h"

#include "AsyncComm/Comm.h"

#include "Tools/Lib/CommandInterpreter.h"

#include "Hypertable/Lib/MasterClient.h"

namespace Hypertable {

  class MasterCommandInterpreter : public CommandInterpreter {
  public:
    MasterCommandInterpreter(MasterClientPtr &master);

    int execute_line(const String &line) override;

  private:
    MasterClientPtr m_master;
  };

  typedef intrusive_ptr<MasterCommandInterpreter>
          MasterCommandInterpreterPtr;

}

#endif // Tools_master_client_MasterCommandInterpreter_h
