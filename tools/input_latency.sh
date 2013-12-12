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

echo "This script will start a Mir server and client and measure the latency "
echo "between input events sent between them. Please press some keys and move "
echo "your mouse! To quit, press Ctrl+C"
echo ""

if [ -x "./$server" ]; then
    bindir="."
elif [ -x "./bin/$server" ]; then
    bindir="./bin"
elif [ -x "./build/bin/$server" ]; then
    bindir="./build/bin"
fi

sudo env MIR_SERVER_INPUT_REPORT=log \
         MIR_CLIENT_INPUT_RECEIVER_REPORT=log \
     sh -c "(sleep 2 ; "$bindir/$client") &
            $bindir/$server" |
     awk '
BEGIN {
    client_time = 0
}

/input: Received event \(when, type, code, value\) from kernel: / {
    now = $1
    gsub(/[^0-9.]/, "", now)

    event_time = $12
    gsub(/[^0-9]/, "", event_time)

    server_time[event_time] = now 
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
    event_time = $2
    if (client_time && (event_time in server_time))
    {
        latency = client_time - server_time[event_time]
        print event_type " latency = " latency " seconds"
        delete server_time[event_time]

        sum[event_type] += latency
        sums[event_type]++
    }
}

/^}$/ {
    client_time = 0
}
'
