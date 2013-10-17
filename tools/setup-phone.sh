#!/bin/bash
# Setup script for getting phone to a convenient state for development work
#
phablet-flash ubuntu-system --no-backup --channel=devel-proposed
adb shell mount -o remount,rw /
adb shell touch /userdata/.writable_image
phablet-network
adb shell apt-get install -y mir-demos gdb

# You now can do stuff like this:
# tools/install_on_android.sh

# Or stuff like this:
# $ adb shell
# # cd tmp/mirtest/
# # export LD_LIBRARY_PATH=/tmp/mirtest/
# # ./acceptance-tests

# Or stuff like this:
# $ adb shell 
# # apt-add-repository ppa:autopilot/ppa
# # apt-get update 
# # apt-get install libautopilot-qt python-autopilot 

# Or even:
# $ adb shell 
# # sudo -i -u phablet
# $ stop unity8
# # gdb unity8
