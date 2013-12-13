#!/bin/sh
#
# Copyright © 2013 Canonical Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
#

server="mir_demo_server_shell"
client="mir_demo_client_egltriangle"

echo "This script will start a Mir server and client to measure the latency "
echo "of input events sent between them. Please press some keys and move "
echo "your mouse! To quit press Ctrl+C."
echo ""

if [ -x "./$server" ]; then
    bindir="."
elif [ -x "./bin/$server" ]; then
    bindir="./bin"
elif [ -x "./build/bin/$server" ]; then
    bindir="./build/bin"
fi

#
# Note the client is started and backgrounded first. This is so we can keep
# the server in the foreground to catch SIGINT and ensure all processes will
# be shut down correctly.
#
sudo env MIR_SERVER_INPUT_REPORT=log \
         MIR_CLIENT_INPUT_RECEIVER_REPORT=log \
     sh -c "(sleep 2 ; "$bindir/$client") &
            $bindir/$server" |
     awk '
BEGIN {
    client_time = 0
    event_type = "UnknownEventType"
}

/input: Received event \(when, type, code, value\) from kernel: / {
    now = $1
    gsub(/[^0-9.]/, "", now)

    event_id = $12
    gsub(/[^0-9]/, "", event_id)

    if (!(event_id in server_time))
    {
        server_time[event_id] = now 
    }
}

/input-receiver: Received event:/ {
    client_time = $1
    gsub(/[^0-9.]/, "", client_time)
}

/^Mir.*Event ?{$/ {
    event_type = $1
    sub(/{$/, "", event_type)
}

/^  event_time: / {
    event_id = $2
    if (client_time && (event_id in server_time))
    {
        latency = client_time - server_time[event_id]
        print event_type " latency = " latency " seconds"
        delete server_time[event_id]
    }
}

/^}$/ {
    client_time = 0
}
'
