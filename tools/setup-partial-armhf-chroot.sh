#!/bin/bash
#
# TODO: Rename this file without "armhf" when it's safe to do so.
#

set -e

name=${0}

usage() {
    echo "Usage: ${name} [options] mychroot-dir"
    echo "options:"
    echo "	-a arch	Select architecture, i.e. armhf, arm64, ppc... Default is armhf"
    echo "	-d dist	Select distribution, i.e. vivid, wily. Default is vivid"
    echo "	-r rep	Select an additional repository for bootstrap. Default is none"
    echo
    echo "please supply at least a directory to create partial chroot in. (eg, ./setup-partial-armhf-chroot.sh mychroot-dir)"
}

# Default to vivid as we don't seem to have any working wily devices right now.
# Also Jenkins expects this script to default to vivid (TODO: update CI?)
arch=armhf
dist=vivid
sourceid=0
repositories=
sources=

while getopts a:d:r:h opt; do
    case $opt in
        a)
            arch=$OPTARG
            ;;
        d)
            dist=$OPTARG
            ;;
        r)
            repositories="$repositories $OPTARG"
            ((++sourceid))
            sources="$sources source$sourceid"
            ;;
        :)
            echo "Option -$OPTARG requires an argument" 
            usage
            exit 1
            ;;
        h)
            usage
            exit 0
            ;;
        \?)
            echo "Invalid option: -$OPTARG" 
            usage
            exit 1
            ;;
    esac
done

shift $((OPTIND-1))

if [ -z ${1} ]; then
    usage
    exit 1
fi

directory=${1}
echo "creating phablet-compatible $arch partial chroot for mir compilation in directory ${directory}"

if [ ! -d ${directory} ]; then
    mkdir -p ${directory} 
fi

DEBCONTROL=$(pwd)/../debian/control

pushd ${directory} > /dev/null

# Empty dpkg status file, so that ALL dependencies are listed with dpkg-checkbuilddeps
echo "" > status

# Manual error code checking is needed for dpkg-checkbuilddeps
set +e

# Parse dependencies from debian/control
# dpkg-checkbuilddeps returns non-zero when dependencies are not met and the list is sent to stderr
builddeps=$(dpkg-checkbuilddeps -a ${arch} --admindir=. ${DEBCONTROL} 2>&1 )
if [ $? -eq 0 ] ; then
    exit 0 
fi
echo "${builddeps}"

# now turn exit on error option
set -e

# Sanitize dependencies list for submission to multistrap
# build-essential is not needed as we are cross-compiling
builddeps=$(echo ${builddeps} | sed -e 's/dpkg-checkbuilddeps://g' \
                                    -e 's/error://g' \
                                    -e 's/Unmet build dependencies://g' \
                                    -e 's/build-essential:native//g')
builddeps=$(echo ${builddeps} | sed 's/([^)]*)//g')
builddeps=$(echo ${builddeps} | sed -e 's/abi-compliance-checker//g')
builddeps=$(echo ${builddeps} | sed -e 's/multistrap//g')

case ${arch} in
    amd64 | i386 )
        source_url=http://archive.ubuntu.com/ubuntu
        ;;
    * )
        source_url=http://ports.ubuntu.com/ubuntu-ports
        ;;
esac

# Our chroot is missing the default public keys set up in a real install.
# So this will provide them and silence the warnings:
mkdir -p etc/apt
ln -fs /etc/apt/trusted.gpg etc/apt/trusted.gpg

echo "[General]
arch=${arch}
directory=${directory}
unpack=false
noauth=true
bootstrap=ubuntu-main ubuntu-universe ${sources}

[ubuntu-main]
packages=${builddeps}
source=${source_url}
suite=${dist} main

[ubuntu-universe]
packages=${builddeps}
source=${source_url}
suite=${dist} universe
" > mstrap.conf

sourceid=0
for x in ${repositories};
do
    ((++sourceid))
    echo "[source${sourceid}]
source=${x}
suite=${dist}
" >> mstrap.conf
done

# Fakeroot is required to stop the apt update command giving up
fakeroot multistrap -f mstrap.conf 

rm -f var/cache/apt/archives/lock

# Remove libc libraries that confuse the cross-compiler
rm -f var/cache/apt/archives/libc-dev*.deb
rm -f var/cache/apt/archives/libc6*.deb

for deb in var/cache/apt/archives/* ; do
    if [ ! -d ${deb} ] ; then
        echo "unpacking: ${deb}"
        dpkg -x ${deb} .
    fi
done

# Fix up symlinks which asssumed the usual root path
for broken_symlink in $(find . -name \*.so -type l -xtype l) ; do
    ln -sf $(pwd)$(readlink ${broken_symlink}) ${broken_symlink}
done

popd > /dev/null 
