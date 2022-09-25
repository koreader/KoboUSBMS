/*
	KoboUSBMS: USBMS helper for KOReader
	Copyright (C) 2020-2022 NiLuJe <ninuje@gmail.com>
	SPDX-License-Identifier: GPL-3.0-or-later

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef __USBMS_H
#define __USBMS_H

// Because we're pretty much Linux-bound ;).
#ifndef _GNU_SOURCE
#	define _GNU_SOURCE
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/limits.h>
#include <linux/rtc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

// Like FBInk, handle math shenanigans...
#include <math.h>
#ifdef __clang__
#	if __has_builtin(__builtin_ceilf)
#		define iceilf(x) ((int) (__builtin_ceilf(x)))
#	endif
#	if __has_builtin(__builtin_floorf)
#		define ifloorf(x) ((int) (__builtin_floorf(x)))
#	endif
#else
#	if __STDC_VERSION__ >= 199901L
#		define iceilf(x)  __builtin_iceilf(x)
#		define ifloorf(x) __builtin_ifloorf(x)
#	endif
#endif

// I18n
#include <libintl.h>
#include <locale.h>

#include "FBInk/fbink.h"
#include "libue/libue.h"
#include "openssh/atomicio.h"
#include <libevdev/libevdev.h>

// Fallback version tag...
#ifndef USBMS_VERSION
#	define USBMS_VERSION "v1.3.2"
#endif
// Fallback timestamp...
#ifndef USBMS_TIMESTAMP
#	define USBMS_TIMESTAMP __TIMESTAMP__
#endif

// Apparently the libevdev version string isn't available anywhere, so, fake it
#define LIBEVDEV_VERSION "1.13.0"

// Gettext
#define _(String) gettext(String)

// Logging helpers
#define LOG(prio, fmt, ...) ({ syslog(prio, fmt, ##__VA_ARGS__); })

// Same, but with __PRETTY_FUNCTION__:__LINE__ right before fmt
#define PFLOG(prio, fmt, ...) ({ LOG(prio, "[%s:%d] " fmt, __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__); })

// FBInk always returns negative error codes
#define ERRCODE(e) (-(e))

// We use a specific exit code for early aborts, in order to be able to know whether onboard is usable or not after a failure...
#define USBMS_EARLY_EXIT 86

// c.f., https://github.com/koreader/koreader-base/blob/master/input/input-kobo.h
#define KOBO_USB_DEVPATH_PLUG "/devices/platform/usb_plug"    // Plugged into a plain power source
#define KOBO_USB_DEVPATH_HOST "/devices/platform/usb_host"    // Plugged into a computer
// c.f., /lib/udev/rules.d/kobo.rules
#define KOBO_USB_DEVPATH_FSL  "/devices/platform/fsl-usb2-udc"                  // OK
#define KOBO_USB_MODALIAS_CI  "platform:ci_hdrc"                                // OK
#define KOBO_USB_DEVPATH_UDC  "/devices/platform/soc/5100000.udc-controller"    // OK

// Those had been set in stone so far...
#define NXP_NTX_KEYS_EVDEV        "/dev/input/event0"
#define NXP_TOUCHPAD_EVDEV        "/dev/input/event1"
// ... but sunxi & SMP changed that ;).
#define SUNXI_NTX_KEYS_EVDEV      "/dev/input/by-path/platform-ntx_event0-event"
// NOTE: Beware! It's on bus 0 on the Elipsa & Sage, but on bus 1 on the Libra 2!
#define ELAN_BUS0_TOUCHPAD_EVDEV  "/dev/input/by-path/platform-0-0010-event"
#define ELAN_BUS1_TOUCHPAD_EVDEV  "/dev/input/by-path/platform-1-0010-event"
// ... and a new hardware revision of the Libra 2 made it even more fun!
#define BD71828_POWERBUTTON_EVDEV "/dev/input/by-path/platform-bd71828-pwrkey-event"
const char* NTX_KEYS_EVDEV = NULL;
const char* TOUCHPAD_EVDEV = NULL;
#define NXP_BATT_CAP_SYSFS    "/sys/class/power_supply/mc13892_bat/capacity"
#define SUNXI_BATT_CAP_SYSFS  "/sys/class/power_supply/battery/capacity"
#define CILIX_CONNECTED_SYSFS "/sys/class/misc/cilix/cilix_conn"
#define CILIX_BATT_CAP_SYSFS  "/sys/class/misc/cilix/cilix_bat_capacity"
const char* BATT_CAP_SYSFS = NULL;
// NOTE: On sunxi, the CM_USB_Plug_IN ioctl is currently broken (it's poking at "mc13892_bat" instead of "battery"),
//       so, rely on sysfs ourselves instead...
#define SUNXI_BATT_STATUS_SYSFS "/sys/class/power_supply/battery/status"
bool (*fxpIsUSBPlugged)(int) = NULL;
// These, on the other hand, are only available on Mk. 7+
#define NXP_CHARGER_TYPE_SYSFS   "/sys/class/power_supply/mc13892_charger/device/charger_type"
#define SUNXI_CHARGER_TYPE_SYSFS "/sys/class/power_supply/charger/device/charger_type"
// For ref., on mainline w/ @akemnade's driver: /sys/class/power_supply/rn5t618-usb/usb_type
const char* CHARGER_TYPE_SYSFS = NULL;
#define FL_INTENSITY_SYSFS "/sys/class/backlight/mxc_msp430.0/actual_brightness"

// Internal storage
#define KOBO_PARTITION     "/dev/mmcblk0p3"
#define KOBO_MOUNTPOINT    "/mnt/onboard"
// External storage
#define KOBO_SD_PARTITION  "/dev/mmcblk1p1"
#define KOBO_SD_MOUNTPOINT "/mnt/sd"
// The timestamp file that the Kobo app creates, which Nickel uses to resync date/time after USBMS sessions
#define KOBO_EPOCH_TS      KOBO_MOUNTPOINT "/.kobo/epoch.conf"
// Same idea, but for the timezone
#define KOBO_TZ_FILE       KOBO_MOUNTPOINT "/.kobo/timezone.conf"
#define SYSTEM_TZPATH      "/etc/zoneinfo"
#define KOBO_TZPATH        "/etc/zoneinfo-kobo"
#define SYSTEM_TZFILE      "/etc/localtime"

// List of exportable partitions
typedef enum
{
	PARTITION_NONE     = -1,
	PARTITION_INTERNAL = 0,
	PARTITION_EXTERNAL
} PARTITION_ID_E;

typedef struct
{
	PARTITION_ID_E id;
	const char*    name;
	const char*    device;
	const char*    mountpoint;
} USBMSPartition;

typedef struct
{
	FBInkConfig   fbink_cfg;
	FBInkOTConfig ot_cfg;
	FBInkOTConfig icon_cfg;
	FBInkOTConfig msg_cfg;
	FBInkState    fbink_state;
	int           fbfd;
	int           ntxfd;
} USBMSContext;

// c.f., arch/arm/mach-imx/imx_ntx_io.c or arch/arm/mach-sunxi/sunxi_ntx_io.c in a Kobo kernel
#define CM_USB_Plug_IN        108
#define CM_CHARGE_STATUS      204    // Mapped to CM_USB_Plug_IN on Mk. 7+...
#define CM_GET_BATTERY_STATUS 206
#define CM_FRONT_LIGHT_SET    241

#endif    // __USBMS_H
