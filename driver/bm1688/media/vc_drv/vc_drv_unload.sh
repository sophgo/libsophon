#!/bin/sh
module="vpu"
device="vpu"

# invoke rmmod with all	arguments we got
/sbin/rmmod $module $* || exit 1

# Remove stale nodes
rm -f /dev/${device}

