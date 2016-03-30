#!/usr/bin/env bash

HT_HOME=${INSTALL_DIR:-"$HOME/hypertable/current"}
SCRIPT_DIR=`dirname $0`

trap "$HT_HOME/bin/ht-stop-servers.sh" EXIT

$HT_HOME/bin/ht-start-test-servers.sh --clear --no-thriftbroker --Hypertable.RangeServer.Maintenance.Interval 100

cat $SCRIPT_DIR/hql-cellcache-delete-issue.hql | $HT_HOME/bin/ht shell --test-mode >& hql-cellcache-delete-issue.output

diff $SCRIPT_DIR/hql-cellcache-delete-issue.golden hql-cellcache-delete-issue.output
if [ $? -ne 0 ]; then
    echo "error: diff returned non-zero, exiting..."
    exit 1
fi

exit 0

