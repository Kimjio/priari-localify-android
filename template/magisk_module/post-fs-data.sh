#!/system/bin/sh
while read -r line; do echo "${line}" | grep jp.co.cygames.priconnegrandmasters | awk '{print $2}' | xargs umount -l; done </proc/mounts
