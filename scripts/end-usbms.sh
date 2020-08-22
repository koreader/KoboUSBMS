#!/bin/sh

# Failures are bad. Catch 'em.
set -e
if [ "${WITH_PIPEFAIL}" = "true" ]; then
    # shellcheck disable=SC2039
    set -o pipefail
fi

# Remount internal (and external) storage after an USBMS session
# c.f., /usr/local/Kobo/udev/usb
# c.f., https://github.com/baskerville/plato/blob/master/scripts/usb-disable.sh

SCRIPT_NAME="$(basename "${0}")"

# If we're NOT in the middle of an USBMS session, something went wrong...
if ! lsmod | grep -q -e "g_file_storage" -e "g_mass_storage" ; then
	logger -p "DAEMON.ERR" -t "${SCRIPT_NAME}[$$]" "Not in an USBMS session?!"
	exit 1
fi

MODULES_PATH="/drivers/${PLATFORM}"
if [ -e "${MODULES_PATH}/g_mass_storage.ko" ] ; then
	rmmod g_mass_storage
else
	rmmod g_file_storage

	if [ "${PLATFORM}" = "mx6sll-ntx" ] || [ "${PLATFORM}" = "mx6ull-ntx" ] ; then
		rmmod usb_f_mass_storage
		rmmod libcomposite
		rmmod configfs
	else
		# NOTE: See start-usbms.sh for why we have to double-check this one...
		lsmod | grep -q "arcotg_udc" && rmmod arcotg_udc
	fi
fi

# Let's keep the mysterious NTX sleep... Given our experience with Wi-Fi modules, it's probably there for a reason ;p.
sleep 1

DISK="/dev/mmcblk"
PARTITION="${DISK}0p3"
MOUNT_ARGS="noatime,nodiratime,shortname=mixed,utf8"

FS_CORRUPT=0
# NOTE: Be a tad less heavy-handed than the stock script with the amount of fscks, but do abort if it's not recoverable...
dosfsck -a -w "${PARTITION}" || dosfsck -a -w "${PARTITION}" || FS_CORRUPT=1
if [ "${FS_CORRUPT}" -eq 1 ] ; then
	logger -p "DAEMON.CRIT" -t "${SCRIPT_NAME}[$$]" "Unrecoverable filesystem corruption on ${PARTITION}, aborting!"
	exit 1
fi
mount -t vfat -o "${MOUNT_ARGS}" "${PARTITION}" "/mnt/onboard"

# Handle the SD card now (again, not dealing with the dynamic detection nonsense).
PARTITION="${DISK}1p1"
[ -e "$PARTITION" ] && mount -t vfat -o "${MOUNT_ARGS}" "${PARTITION}" "/mnt/sd"
