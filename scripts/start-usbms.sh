#!/bin/sh

# Failures are bad. Catch 'em.
set -e

# Export the internal (and external) storage over USBMS
# c.f., /usr/local/Kobo/udev/usb
# c.f., https://github.com/baskerville/plato/blob/master/scripts/usb-enable.sh

SCRIPT_NAME="$(basename "${0}")"

# If we're already in the middle of an USBMS session, something went wrong...
if lsmod | grep -q "g_file_storage" ; then
	logger -p "DAEMON.ERR" -t "${SCRIPT_NAME}[$$]" "Already in USBMS session?!"
	exit 1
fi

USB_VENDOR_ID="0x2237"
# NOTE: USB_PRODUCT_ID has already been taken care of by the C tool.
# Grab what we need from Nickel's version tag before we unmount onboard...
VERSION_TAG="/mnt/onboard/.kobo/version"
if [ -f "$KOBO_TAG" ] ; then
	# NOTE: The one baked into the block device *may* be longer on some devices, so use the same as Nickel to avoid issues with stuff that uses it to discriminate devices (i.e., Calibre)
	#       (c.f., http://trac.ak-team.com/trac/browser/niluje/Configs/trunk/Kindle/Kobo_Hacks/KoboStuff/src/usr/local/stuff/bin/usbnet-toggle.sh#L57)
	SERIAL_NUMBER="$(cut -f1 -d',' "${VERSION_TAG}")"
	FW_VERSION="$(cut -f3 -d',' "${VERSION_TAG}")"
else
	logger -p "DAEMON.WARNING" -t "${SCRIPT_NAME}[$$]" "No Nickel version tag?!"
	SERIAL_NUMBER="N000000000000"
	FW_VERSION="4.6.9995"
fi

# NOTE: The stock script is a bit wonky: when exporting, it does an ugly dynamic detection of vfat partitions, but it's hard-coded when remounting...
#       Follow Plato's lead, and hard-code in both cases.
DISK="/dev/mmcblk"
PARTITIONS="${DISK}0p3"

# Append the SD card if there's one
[ -e "${DISK}1p1" ] && PARTITIONS="${PARTITIONS},${DISK}1p1"

# Flush to disk, and drop FS caches
sync
echo 3 > "/proc/sys/vm/drop_caches"

# And now, unmount it
for mountpoint in sd onboard ; do
	DIR="/mnt/${mountpoint}"
	if grep -q "${DIR}" "/proc/mounts" ; then
		# NOTE: Unlike the stock script (which only does a lazy unmount) and Plato (which does both),
		#       we're extremely paranoid and will only try a proper umount.
		#       If it fails, we're done. We'll log the error, the usbms tool will print a warning for a bit, and KOReader will then shutdown the device.
		if ! umount "${DIR}" ; then
			logger -p "DAEMON.CRIT" -t "${SCRIPT_NAME}[$$]" "Failed to unmount ${mountpoint}, aborting!"
			exit 1
		fi
	fi
done

MODULES_PATH="/drivers/${PLATFORM}"
GADGETS_PATH="${MODULES_PATH}/usb/gadget"

if [ -e "${MODULES_PATH}/g_mass_storage.ko" ] ; then
	PARAMS="idVendor=${USB_VENDOR_ID} idProduct=${USB_PRODUCT_ID} iManufacturer=Kobo iProduct=eReader-${FW_VERSION} iSerialNumber=${SERIAL_NUMBER}"
	insmod "${MODULES_PATH}/g_mass_storage.ko" file="${PARTITIONS}" stall=1 removable=1 ${PARAMS}

else
	if [ "${PLATFORM}" = "mx6sll-ntx" ] || [ "${PLATFORM}" = "mx6ull-ntx" ] ; then
		PARAMS="idVendor=${USB_VENDOR_ID} idProduct=${USB_PRODUCT_ID} iManufacturer=Kobo iProduct=eReader-${FW_VERSION} iSerialNumber=${SERIAL_NUMBER}"
		insmod "${GADGETS_PATH}/configfs.ko"
		insmod "${GADGETS_PATH}/libcomposite.ko"
		insmod "${GADGETS_PATH}/usb_f_mass_storage.ko"
	else
		PARAMS="vendor=${USB_VENDOR_ID} product=${USB_PRODUCT_ID} vendor_id=Kobo product_id=eReader-${FW_VERSION} SN=${SERIAL_NUMBER}"
		# NOTE: arcotg_udc is builtin on Mk. 6, but old FW may have been shipping a broken module!
		if [ "${PLATFORM}" != "mx6sl-ntx" ] ; then
			insmod "${GADGETS_PATH}/arcotg_udc.ko"
			sleep 2
		fi
	fi

	insmod "${GADGETS_PATH}/g_file_storage.ko" file="${PARTITIONS}" stall=1 removable=1 ${PARAMS}
fi

# Let's keep the mysterious NTX sleep... Given our experience with Wi-Fi modules, it's probably there for a reason ;p.
sleep 1
