#!/bin/sh
# raise-receive-buffer-limit <settings file> [<limit file>]
#
# Applies the Core's network receive buffer limit at once, so an install does
# not wait for the next system start, and says in plain words what it did.
# It only ever raises: a limit already at or above the one in the settings
# file is left as it is.
#
# <settings file> is the sysctl.d file the Core ships
# (/usr/lib/sysctl.d/10-nereusd.conf, from
# packaging/nereusd-receive-buffer.conf). The number is read from it, so one
# file states it. <limit file> is where the kernel keeps the limit in force,
# /proc/sys/net/core/rmem_max unless given; the tests give a temporary file.
#
# Run as root by the Core's installers: the .deb postinst and the Rock and Pi
# stage installers. It exits 0 whenever the settings file is sound, raised or
# not. A limit that cannot be changed now must not fail an install: the
# settings file still applies it at the next system start.
set -eu

conf=${1:?settings file required}
limit_file=${2:-/proc/sys/net/core/rmem_max}
key=net.core.rmem_max

wanted=$(sed -n 's/^[[:space:]]*net\.core\.rmem_max[[:space:]]*=[[:space:]]*\([0-9][0-9]*\)[[:space:]]*$/\1/p' "$conf" | tail -n 1)
if [ -z "$wanted" ]; then
    echo "$conf sets no $key; nothing done" >&2
    exit 2
fi

current=$(cat "$limit_file" 2>/dev/null) || current=
case "$current" in
    ''|*[!0-9]*)
        echo "Network receive buffer limit ($key): the limit in force could not be read. $conf sets it to $wanted bytes at the next system start."
        exit 0
        ;;
esac

if [ "$current" -ge "$wanted" ]; then
    echo "Network receive buffer limit ($key) is $current bytes, already at least the $wanted the Core asks for. Left unchanged."
    exit 0
fi

if { echo "$wanted" > "$limit_file"; } 2>/dev/null; then
    echo "Raised the network receive buffer limit ($key) from $current to $wanted bytes, the size the Core asks for on the socket its radio sends to, so the system can hold the radio's packets while the Core is briefly held up. $conf keeps it at every system start; removing that file undoes it from the next start."
else
    echo "Network receive buffer limit ($key): it could not be raised from $current to $wanted bytes now. $conf sets it at the next system start."
fi
exit 0
