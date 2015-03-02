#!/bin/sh

set -e

abi_vars="\
    MIRCLIENT_ABI \
    MIRCLIENT_DEBUG_EXTENSION_ABI \
    MIR_CLIENT_PLATFORM_ABI \
    MIRCOMMON_ABI \
    MIRPLATFORM_ABI \
    MIRPROTOBUF_ABI \
    MIRSERVER_ABI \
    MIR_SERVER_GRAPHICS_PLATFORM_ABI"

install_files="\
    libmirclient:MIRCLIENT_ABI \
    libmirclient-debug-extension:MIRCLIENT_DEBUG_EXTENSION_ABI \
    libmircommon:MIRCOMMON_ABI \
    libmirplatform:MIRPLATFORM_ABI \
    libmirprotobuf:MIRPROTOBUF_ABI \
    libmirserver:MIRSERVER_ABI \
    mir-client-platform-android:MIR_CLIENT_PLATFORM_ABI \
    mir-client-platform-mesa:MIR_CLIENT_PLATFORM_ABI \
    mir-platform-graphics-android:MIR_SERVER_GRAPHICS_PLATFORM_ABI \
    mir-platform-graphics-mesa:MIR_SERVER_GRAPHICS_PLATFORM_ABI"


get_abi_number()
{
    local abi_var=$1
    grep -R "set($abi_var" src/* | grep -o '[[:digit:]]\+'
}

populate_abi_variables()
{
    for abi_var in $abi_vars;
    do
        local tmp=$(get_abi_number $abi_var)
        if [ -z "$tmp" ];
        then
            echo "Failed to find ABI number for $abi_var"
            exit 1
        fi
        eval "$abi_var=$tmp"
    done
}

create_control_file()
{
    local cmd="sed"

    for abi_var in $abi_vars;
    do
        cmd="$cmd -e \"s/@$abi_var@/\${$abi_var}/\""
    done

    cmd="$cmd debian/control.in > debian/control"

    # Remove the debian/control symlink before writing to it
    rm debian/control
    eval "$cmd"
}

create_install_files()
{
    for f in $install_files;
    do
        local pkg=${f%%:*}
        local abi_var=${f##*:}
        local abi=$(eval "echo \$${abi_var}")
        sed "s/@$abi_var@/$abi/" \
            debian/${pkg}.install.in > debian/${pkg}${abi}.install
    done
}

populate_abi_variables
create_control_file
create_install_files
