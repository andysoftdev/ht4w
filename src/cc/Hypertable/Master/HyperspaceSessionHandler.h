/** -*- c++ -*-
 * Copyright (C) 2008 Doug Judd (Zvents, Inc.)
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of the
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

#ifndef HYPERSPACE_SESSIONHANDLER_H
#define HYPERSPACE_SESSIONHANDLER_H

#include "Common/Logger.h"

#include "Hyperspace/Session.h"

namespace Hypertable {

  class HyperspaceSessionHandler : public Hyperspace::SessionCallback {
  public:
    virtual void safe() {
      HT_NOTICE("Hyperspace session state = SAFE");
    }
    virtual void expired() {
      HT_NOTICE("Hyperspace session state = EXPIRED");
    }
    virtual void jeopardy() {
      HT_NOTICE("Hyperspace session state = JEOPARDY");
    }
    virtual void disconnected() {
      HT_NOTICE("Hyperspace session state = RECONNECTING");
    }
    virtual void reconnected() {
      HT_NOTICE("Hyperspace session state = RECONNECTED");
    }

  };

}

#endif // HYPERSPACE_SESSIONHANDLER_H

