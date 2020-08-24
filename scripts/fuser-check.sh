#!/bin/sh

# Failures are bad. Catch 'em.
set -e
if [ "${WITH_PIPEFAIL}" = "true" ]; then
    # shellcheck disable=SC2039
    set -o pipefail
fi

# Do something a wee bit more user-friendly with what fuser spits out...
for pid in $(fuser -m "/mnt/onboard") ; do
	if [ -e "/proc/${pid}" ] ; then
		echo "${pid} -> $(cat "/proc/${pid}/comm")"
	fi
done
