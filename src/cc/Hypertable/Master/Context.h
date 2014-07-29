/* -*- c++ -*-
 * Copyright (C) 2007-2013 Hypertable, Inc.
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

/** @file
 * Declarations for Context.
 * This file contains declarations for Context, a class that provides execution
 * context for the Master.
 */

#ifndef HYPERTABLE_CONTEXT_H
#define HYPERTABLE_CONTEXT_H

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/thread/condition.hpp>

#include "Common/Filesystem.h"
#include "Common/Properties.h"
#include "Common/ReferenceCount.h"
#include "Common/StringExt.h"

#include "AsyncComm/Comm.h"
#include "AsyncComm/ConnectionManager.h"

#include "Hyperspace/Session.h"

#include "Hypertable/Lib/NameIdMapper.h"
#include "Hypertable/Lib/MetaLogDefinition.h"
#include "Hypertable/Lib/MetaLogWriter.h"
#include "Hypertable/Lib/Table.h"

#include "Monitoring.h"
#include "RangeServerConnection.h"
#include "RangeServerConnectionManager.h"
#include "RecoveryStepFuture.h"
#include "SystemState.h"

#include <set>
#include <unordered_map>

namespace Hypertable {

  /// @addtogroup Master
  /// @{

  class LoadBalancer;
  class Operation;
  class OperationTimedBarrier;
  class OperationProcessor;
  class ResponseManager;
  class ReferenceManager;
  class BalancePlanAuthority;

  /// Execution context for the Master.
  class Context : public ReferenceCount {

    class RecoveryState {
      public:
        void install_replay_future(int64_t id, RecoveryStepFuturePtr &future);
        RecoveryStepFuturePtr get_replay_future(int64_t id);
        void erase_replay_future(int64_t id);

        void install_prepare_future(int64_t id, RecoveryStepFuturePtr &future);
        RecoveryStepFuturePtr get_prepare_future(int64_t id);
        void erase_prepare_future(int64_t id);

        void install_commit_future(int64_t id, RecoveryStepFuturePtr &future);
        RecoveryStepFuturePtr get_commit_future(int64_t id);
        void erase_commit_future(int64_t id);

      private:
        friend class Context;

        typedef std::map<int64_t, RecoveryStepFuturePtr> FutureMap;

        Mutex m_mutex;
        FutureMap m_replay_map;
        FutureMap m_prepare_map;
        FutureMap m_commit_map;
    };

  public:

    /** Context.
     * @param p Reference to properties object
     */
    Context(PropertiesPtr &p);

    /** Destructor. */
    ~Context();

    Mutex mutex;
    boost::condition cond;
    Comm *comm;
    SystemStatePtr system_state;       //!< System state entity
    RangeServerConnectionManagerPtr rsc_manager;
    StringSet available_servers;
    PropertiesPtr props;
    ConnectionManagerPtr conn_manager;
    Hyperspace::SessionPtr hyperspace;
    uint64_t master_file_handle;
    FilesystemPtr dfs;
    String toplevel_dir;
    NameIdMapperPtr namemap;
    MetaLog::DefinitionPtr mml_definition;
    MetaLog::WriterPtr mml_writer;
    LoadBalancer *balancer;
    MonitoringPtr monitoring;
    ResponseManager *response_manager;
    ReferenceManager *reference_manager;
    TablePtr metadata_table;
    TablePtr rs_metrics_table;
    time_t request_timeout;
    uint32_t timer_interval;
    uint32_t monitoring_interval;
    uint32_t gc_interval;
    time_t next_monitoring_time;
    time_t next_gc_time;
    OperationProcessor *op;
    OperationTimedBarrier *recovery_barrier_op;
    String cluster_name;               //!< Name of cluster
    String location_hash;
    int32_t disk_threshold;            //!< Disk use threshold percentage
    int32_t max_allowable_skew;
    bool test_mode;
    bool quorum_reached;

    /// Adds operation to active <i>move range</i> operation map.
    /// This method adds a mapping for <code>operation</code> to the
    /// #m_outstanding_move_ops map.  This map holds references to outstanding
    /// OperationMoveRange operations, mapping the operation's hash_code to it's
    /// ID.  The actual reference to the operation is held in #reference_manager
    /// and the #m_outstanding_move_ops map is used to map the operation's hash
    /// code to it's ID which is used as the key to #reference_manager.  This
    /// map is used to prevent multiple OperationMoveRange operations to get
    /// created for the same range.
    /// @param operation Move range operation to add to map
    /// @return <i>true</i> if operation was successfully added to map,
    /// <i>false</i> if operation was not added to the map because an entry
    /// already exists in the map for the same operation hash code
    bool add_move_operation(Operation *operation);

    /// Removes operation from active <i>move range</i> operation map.
    /// Removes entry from #m_outstanding_move_ops map correspoding to
    /// <code>operation</code>.
    /// @param operation Move range operation to remove from map
    /// @see add_move_operation().
    void remove_move_operation(Operation *operation);

    /// Gets operation from active <i>move range</i> operation map.
    /// Gets operation corresponding with <code>hash_code</code> by consulting
    /// #m_outstanding_move_ops to determine the operation ID of the outstanding
    /// move range operation and fetching it from #reference_manager.
    /// @param hash_code Hash code of move range operation to get.
    /// @return Pointer to outstanding move range operation corresponding with
    /// <code>hash_code</code>, or nullptr if no mapping exists.
    /// @see add_move_operation().
    Operation *get_move_operation(int64_t hash_code);

    void add_available_server(const String &location);
    void remove_available_server(const String &location);
    size_t available_server_count();
    void get_available_servers(StringSet &servers);

    bool can_accept_ranges(const RangeServerStatistics &stats);
    void replay_status(EventPtr &event);
    void replay_complete(EventPtr &event);
    void prepare_complete(EventPtr &event);
    void commit_complete(EventPtr &event);

    /** Invoke notification hook. */
    void notification_hook(const String &subject, const String &message);

    /** set the BalancePlanAuthority. */
    void set_balance_plan_authority(BalancePlanAuthority *bpa);

    // get the BalancePlanAuthority; this creates a new instance when
    // called for the very first time 
    BalancePlanAuthority *get_balance_plan_authority();

    RecoveryState &recovery_state() { return m_recovery_state; }

  private:

    RecoveryState m_recovery_state;
    BalancePlanAuthority *m_balance_plan_authority;
    /// %Mutex for serializing access to #m_outstanding_move_ops
    Mutex m_outstanding_move_ops_mutex;
    /// Map of outstanding <i>move range</i> operations
    std::unordered_map<int64_t, int64_t> m_outstanding_move_ops;
  };

  /// Smart pointer to Context
  typedef intrusive_ptr<Context> ContextPtr;

  /// @}

} // namespace Hypertable

#endif // HYPERTABLE_CONTEXT_H
