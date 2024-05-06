#!/bin/sh

# Failures are bad. Catch 'em.
set -ex
if [ "${WITH_PIPEFAIL}" = "true" ]; then
    # shellcheck disable=SC2039
    set -o pipefail
fi

# Remount internal (and external) storage after an USBMS session
# c.f., /usr/local/Kobo/udev/usb
# c.f., https://github.com/baskerville/plato/blob/master/scripts/usb-disable.sh

SCRIPT_NAME="$(basename "${0}")"

# On some devices/FW versions, some of the modules are builtins, so we can't just fire'n forget...
checked_rmmod() {
	if grep -q "^${1} " "/proc/modules" ; then
		rmmod "${1}"
	fi
}

DISK="/dev/mmcblk"

# NXP & Sunxi SoCs
legacy_usb() {
	# If we're NOT in the middle of an USBMS session, something went wrong...
	if ! grep -q -e "^g_file_storage " -e "^g_mass_storage " "/proc/modules" ; then
		logger -p "DAEMON.ERR" -t "${SCRIPT_NAME}[$$]" "Not in an USBMS session?!"
		exit 1
	fi

	MODULES_PATH="/drivers/${PLATFORM}"
	if [ -e "${MODULES_PATH}/g_mass_storage.ko" ] ; then
		rmmod "g_mass_storage"
	else
		rmmod "g_file_storage"

		if [ "${PLATFORM}" = "mx6sll-ntx" ] || [ "${PLATFORM}" = "mx6ull-ntx" ] ; then
			# Since FW 4.31.19086, these may be builtins...
			checked_rmmod "usb_f_mass_storage"
			checked_rmmod "libcomposite"
			checked_rmmod "configfs"
		else
			# NOTE: See start-usbms.sh for why we have to double-check this one...
			if [ "${PLATFORM}" != "mx6sl-ntx" ] ; then
				checked_rmmod "arcotg_udc"
			fi
		fi
	fi

	# Let's keep the mysterious NTX sleep... Given our experience with Wi-Fi modules, it's probably there for a reason ;p.
	sleep 1

	PARTITION="${DISK}0p3"
}

# MTK SoCs, via configfs
mtk_usb() {
	# If we're NOT in the middle of an USBMS session, something went wrong...
	if [ "$(cat /sys/kernel/config/usb_gadget/g1/UDC)" != "11211000.usb" ] ; then
		logger -p "DAEMON.ERR" -t "${SCRIPT_NAME}[$$]" "Not in an USBMS session?!"
		exit 1
	fi

	# Common
	mkdir -p /sys/kernel/config/usb_gadget/g1
	mkdir -p /sys/kernel/config/usb_gadget/g1/strings/0x409
	PARTITION="${DISK}0p12"

	# Disable the gadget
	echo "" > /sys/kernel/config/usb_gadget/g1/UDC

	# Unbind function from config
	rm /sys/kernel/config/usb_gadget/g1/configs/c.1/mass_storage.0
	# Remove the config's strings
	rmdir /sys/kernel/config/usb_gadget/g1/configs/c.1/strings/0x409
	# Remove the config
	rmdir /sys/kernel/config/usb_gadget/g1/configs/c.1
	# Remove the function
	rmdir /sys/kernel/config/usb_gadget/g1/functions/mass_storage.0
	# Remove the gadget's strings
	rmdir /sys/kernel/config/usb_gadget/g1/strings/0x409
	# And remove the gadget
	rmdir /sys/kernel/config/usb_gadget/g1
}

case "${PLATFORM}" in
	"mt8113t-ntx" )
		mtk_usb
	;;
	* )
		legacy_usb
	;;
esac

MOUNT_ARGS="noatime,nodiratime,shortname=mixed,utf8"

# NOTE: Be a tad less heavy-handed than the stock script with the amount of fscks, but do abort if it's not recoverable...
if ! dosfsck -a -w "${PARTITION}" ; then
	if ! dosfsck -a -w "${PARTITION}" ; then
		logger -p "DAEMON.CRIT" -t "${SCRIPT_NAME}[$$]" "Unrecoverable filesystem corruption on ${PARTITION}, aborting!"
		exit 1
	fi
fi
mount -t vfat -o "${MOUNT_ARGS}" "${PARTITION}" "/mnt/onboard"

# Handle the SD card now (again, not dealing with the dynamic detection nonsense).
PARTITION="${DISK}1p1"
if [ -e "${PARTITION}" ] ; then
	# NOTE: Mimic the stock script and never check the external SD card...
	#       While I'm not necessarily a fan of this approach,
	#       one of the benefits is that we avoid a potentially time consuming process for larger cards.
	mount -t vfat -o "${MOUNT_ARGS}" "${PARTITION}" "/mnt/sd"
fi
