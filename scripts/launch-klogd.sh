#/bin/sh

SCRIPT_NAME="$(basename "${0}")"

# Prefer KoboStuff's binutils build
export PATH="/usr/local/niluje/usbnet/sbin:/usr/local/niluje/usbnet/usr/sbin:/usr/local/niluje/usbnet/bin:/usr/local/niluje/usbnet/usr/bin:${PATH}"

if ! command -v klogd > /dev/null ; then
	logger -p "DAEMON.WARNING" -t "${SCRIPT_NAME}[$$]" "klogd is not available on this device"
	exit 0
fi

if pkill -0 klog ; then
	logger -p "DAEMON.NOTICE" -t "${SCRIPT_NAME}[$$]" "klogd is already running"
else
	klogd
	logger -p "DAEMON.NOTICE" -t "${SCRIPT_NAME}[$$]" "launched klogd"
fi
