#!/usr/bin/env bash

HT_HOME=${INSTALL_DIR:-"$HOME/hypertable/current"}
HYPERTABLE_HOME=$HT_HOME
PIDFILE=$HT_HOME/run/Hypertable.RangeServer.pid
LAUNCHER_PIDFILE=$HT_HOME/run/Hypertable.RangeServerLauncher.pid
DUMP_METALOG=$HT_HOME/bin/dump_metalog
MY_IP=`$HT_HOME/bin/system_info --my-ip`
RS_PORT=38060
METALOG="/hypertable/servers/rs1/log/range_txn/0"
RANGE_SIZE=${RANGE_SIZE:-"7M"}

. $HT_HOME/bin/ht-env.sh

# Kill launcher if running & store pid of this launcher
if [ -f $LAUNCHER_PIDFILE ]; then
  kill -9 `cat $LAUNCHER_PIDFILE`
  rm -f $LAUNCHER_PIDFILE
fi
echo "$$" > $LAUNCHER_PIDFILE

# Kill RangeServer if running
if [ -f $PIDFILE ]; then
  kill -9 `cat $PIDFILE`
  rm -f $PIDFILE
fi

$HT_HOME/bin/Hypertable.RangeServer --verbose --pidfile=$PIDFILE \
    --Hypertable.RangeServer.Workers=330 \
    --Hypertable.RangeServer.Range.SplitSize=$RANGE_SIZE $@

# Exit if base run
if [ -z $1 ]; then
    \rm -f $LAUNCHER_PIDFILE
    exit
fi

echo ""
echo "!!!! CRASH ($@) !!!!"
echo ""

echo "RSML entries:"
$DUMP_METALOG $METALOG
echo "Range states:"
$DUMP_METALOG -s $METALOG

$HT_HOME/bin/Hypertable.RangeServer --pidfile=$PIDFILE --Hypertable.RangeServer.Workers=330 --verbose

\rm -f $LAUNCHER_PIDFILE
