#!/bin/sh

# Failures are bad. Catch 'em.
set -e
if [ "${WITH_PIPEFAIL}" = "true" ]; then
    # shellcheck disable=SC2039
    set -o pipefail
fi

SCRIPT_NAME="$(basename "${0}")"

# Do something a wee bit more user-friendly with what fuser spits out...
for pid in $(fuser -m "${1}") ; do
	if [ -e "/proc/${pid}" ] ; then
		pretty_msg="${pid} -> $(cat "/proc/${pid}/comm")"
		echo "${pretty_msg}"
		logger -p "DAEMON.NOTICE" -t "${SCRIPT_NAME}[$$]" "${pretty_msg}"
	fi
done
