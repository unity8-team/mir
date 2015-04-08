#
# Example usage for an unconfined app 'appname'. This provides no protection
# or configuration.
# $ aa-easyprof --template=unconfined \
#               --profile-name=com.example.appname \
#               "/usr/share/appname/**"
#
###ENDUSAGE###
# vim:syntax=apparmor

#include <tunables/global>

# Define vars with unconfined since autopilot rules may reference them
###VAR###

# TODO: when v3 userspace lands, use:
# ###PROFILEATTACH### (unconfined) {}

# v2 compatible wildly permissive profile
###PROFILEATTACH### (attach_disconnected) {
  capability,
  network,
  / rwkl,
  /** rwlkm,
  /** pix,

  mount,
  remount,
  umount,
  dbus,
  signal,
  ptrace,
  unix,
}
