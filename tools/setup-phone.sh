#! /bin/bash
rm lib/libmirclient.so.0
phablet-flash ubuntu-system --no-backup --channel=devel-proposed
adb shell mount -o remount,rw /
adb shell touch /userdata/.writable_image
phablet-network
adb shell apt-get install mir-demos
adb shell sudo -i -u phablet stop unity8
../tools/install_on_android.sh .

#adb shell
#cd tmp/mirtest/
#export LD_LIBRARY_PATH=/tmp/mirtest/

#adb shell sudo -i -u phablet
#sudo apt-add-repository ppa:autopilot/ppa
#sudo apt-get update &&  sudo apt-get install libautopilot-qt python-autopilot 

