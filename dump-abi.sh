#!/bin/bash
set -e

BUILD_DIR=build-linux-x86
NUM_JOBS=$(( $(grep -c ^processor /proc/cpuinfo) + 1 ))
if [ ! -e ${BUILD_DIR} ]; then
    mkdir ${BUILD_DIR}
    ( cd ${BUILD_DIR} && cmake ..)
fi

pushd ${BUILD_DIR} > /dev/null
make -j${NUM_JOBS} abi-dump-base
popd > /dev/null

./cross-compile-chroot.sh abi-dump-base

if [ ! -e "abi_dumps" ]; then
    mkdir abi_dumps
fi

cp -R build-linux-x86/abi_dumps/* abi_dumps/
cp -R build-android-arm/abi_dumps/* abi_dumps/ 
