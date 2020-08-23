/*
	KoboUSBMS: USBMS helper for KOReader
	Copyright (C) 2020 NiLuJe <ninuje@gmail.com>
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

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "FBInk/fbink.h"
#include "libue/libue.h"
#include <libevdev/libevdev.h>

// Fallback version tag...
#ifndef USBMS_VERSION
#	define USBMS_VERSION "v0.9.0"
#endif
// Fallback timestamp...
#ifndef USBMS_TIMESTAMP
#	define USBMS_TIMESTAMP __TIMESTAMP__
#endif

// Apparently the libevdev version string isn't available anywhere, so, fake it
#define LIBEVDEV_VERSION "1.9.1"

// Logging helpers
#define LOG(prio, fmt, ...) ({ syslog(prio, fmt, ##__VA_ARGS__); })

// Same, but with __PRETTY_FUNCTION__:__LINE__ right before fmt
#define PFLOG(prio, fmt, ...) ({ LOG(prio, "[%s:%d] " fmt, __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__); })

// FBInk always returns negative error codes
#define ERRCODE(e) (-(e))

// c.f., https://github.com/koreader/koreader-base/blob/master/input/input-kobo.h
#define KOBO_USB_DEVPATH_PLUG "/devices/platform/usb_plug"
#define KOBO_USB_DEVPATH_HOST "/devices/platform/usb_host"

// So far, those have thankfully been set in stone
#define NTX_KEYS_EVDEV "/dev/input/event0"
#define TOUCHPAD_EVDEV "/dev/input/event1"

// c.f., arch/arm/mach-imx/imx_ntx_io.c in a Kobo kernel
#define CM_USB_Plug_IN        108
#define CM_CHARGE_STATUS      204    // Mapped to CM_USB_Plug_IN on the Forma...
#define CM_GET_BATTERY_STATUS 206

#endif    // __USBMS_H
