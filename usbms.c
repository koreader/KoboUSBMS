/*
	KoboUSBMS: USBMS helper for KOReader
	Copyright (C) 2020-2024 NiLuJe <ninuje@gmail.com>
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

#include "usbms.h"

// Wrapper function to use syslog as a libevdev log handler
// c.f., libevdev_dflt_log_func @ libevdev/libevdev.c
__attribute__((format(printf, 7, 0))) static void
    libevdev_to_syslog(const struct libevdev*     dev __attribute__((unused)),
		       enum libevdev_log_priority priority,
		       void*                      data __attribute__((unused)),
		       const char*                file,
		       int                        line,
		       const char*                func,
		       const char*                format,
		       va_list                    args)
{
	const char* prefix;
	switch (priority) {
		case LIBEVDEV_LOG_ERROR:
			prefix = "libevdev error";
			break;
		case LIBEVDEV_LOG_INFO:
			prefix = "libevdev info";
			break;
		case LIBEVDEV_LOG_DEBUG:
			prefix = "libevdev debug";
			break;
		default:
			prefix = "libevdev INVALID LOG PRIORITY";
			break;
	}

	if (priority == LIBEVDEV_LOG_DEBUG) {
		syslog(LOG_INFO, "%s in %s:%d:%s:", prefix, file, line, func);
	} else {
		syslog(LOG_INFO, "%s in %s:", prefix, func);
	}
	vsyslog(LOG_INFO, format, args);
}

static void
    setup_usb_ids(DEVICE_ID_T device_code)
{
	// Map device IDs to USB Product IDs, as we're going to need that in the scripts
	// c.f., https://github.com/kovidgoyal/calibre/blob/9d881ed2fcff219887579571f1bb48bdf41437d4/src/calibre/devices/kobo/driver.py#L1402-L1416
	// c.f., https://github.com/baskerville/plato/blob/d96e40737060b569ae875f37d6d741fd5ccc802c/contrib/plato.sh#L38-L56
	// NOTE: Keep 'em in FBInk order to make my life easier

	uint32_t pid = 0xDEAD;
	switch (device_code) {
		case DEVICE_KOBO_TOUCH_A:    // Touch A (trilogy)
		case DEVICE_KOBO_TOUCH_B:    // Touch B (trilogy)
		case DEVICE_KOBO_TOUCH_C:    // Touch C (trilogy)
			pid = 0x4163;
			break;
		case DEVICE_KOBO_MINI:    // Mini (pixie)
			pid = 0x4183;
			break;
		case DEVICE_KOBO_GLO:    // Glo (kraken)
			pid = 0x4173;
			break;
		case DEVICE_KOBO_GLO_HD:    // Glo HD (alyssum)
			pid = 0x4223;
			break;
		case DEVICE_KOBO_TOUCH_2:    // Touch 2.0 (pika)
			pid = 0x4224;
			break;
		case DEVICE_KOBO_AURA:    // Aura (phoenix)
			pid = 0x4203;
			break;
		case DEVICE_KOBO_AURA_HD:    // Aura HD (dragon)
			pid = 0x4193;
			break;
		case DEVICE_KOBO_AURA_H2O:    // Aura H2O (dahlia)
			pid = 0x4213;
			break;
		case DEVICE_KOBO_AURA_H2O_2:       // Aura H2O² (snow)
		case DEVICE_KOBO_AURA_H2O_2_R2:    // Aura H2O² r2 (snow)
			pid = 0x4227;
			break;
		case DEVICE_KOBO_AURA_ONE:       // Aura ONE (daylight)
		case DEVICE_KOBO_AURA_ONE_LE:    // Aura ONE LE (daylight)
			pid = 0x4225;
			break;
		case DEVICE_KOBO_AURA_SE:       // Aura SE (star)
		case DEVICE_KOBO_AURA_SE_R2:    // Aura SE r2 (star)
			pid = 0x4226;
			break;
		case DEVICE_KOBO_CLARA_HD:    // Clara HD (nova)
			pid = 0x4228;
			break;
		case DEVICE_KOBO_FORMA:         // Forma (frost)
		case DEVICE_KOBO_FORMA_32GB:    // Forma 32GB (frost)
			pid = 0x4229;
			break;
		case DEVICE_KOBO_LIBRA_H2O:    // Libra H2O (storm)
			pid = 0x4232;
			break;
		case DEVICE_KOBO_NIA:    // Nia (luna)
			pid = 0x4230;
			break;
		case DEVICE_KOBO_ELIPSA:    // Elipsa (europa)
			pid = 0x4233;
			break;
		case DEVICE_KOBO_LIBRA_2:    // Libra 2 (io)
			pid = 0x4234;
			break;
		case DEVICE_KOBO_SAGE:    // Sage (cadmus)
			pid = 0x4231;
			break;
		case DEVICE_KOBO_CLARA_2E:    // Clara 2E (goldfinch)
			pid = 0x4235;
			break;
		case DEVICE_KOBO_ELIPSA_2E:    // Elipsa 2E (condor)
			pid = 0x4236;
			break;
		case DEVICE_KOBO_LIBRA_COLOUR:      // Kobo Libra Colour (monza)
		case DEVICE_TOLINO_VISION_COLOR:    // Tolino Vision Color (monza)
		case DEVICE_KOBO_CLARA_BW:          // Kobo Clara B&W (spa)
		case DEVICE_TOLINO_SHINE_BW:        // Tolino Shine B&W (spa)
		case DEVICE_KOBO_CLARA_COLOUR:      // Kobo Clara Colour (spa)
		case DEVICE_TOLINO_SHINE_COLOR:     // Tolino Shine Color (spa)
			pid = 0x4237;
			break;
		case 0U:
			pid = 0x4163;
			break;
		default:
			PFLOG(LOG_WARNING, "Can't match device code (%hu) to a USB Product ID!", device_code);
			break;
	}

	// Push it to the env…
	char pid_str[8] = { 0 };
	snprintf(pid_str, sizeof(pid_str) - 1U, "0x%04X", pid);
	PFLOG(LOG_NOTICE, "USB product ID: %s", pid_str);
	setenv("USB_PRODUCT_ID", pid_str, 1);
}

static bool
    ioctl_is_usb_plugged(int ntxfd, bool foo __attribute__((unused)))
{
	// Check if we're plugged in…
	unsigned long ptr = 0U;
	int           rc  = ioctl(ntxfd, CM_USB_Plug_IN, &ptr);
	if (rc == -1) {
		PFLOG(LOG_WARNING, "Could not query USB status (ioctl: %m)");
	}
	return !!ptr;
}

static bool
    sysfs_is_usb_plugged(int foo __attribute__((unused)), bool log_status)
{
	bool is_plugged = false;

	FILE* f = fopen(BATT_STATUS_SYSFS, "re");
	if (f) {
		char   status[16] = { 0 };
		size_t size       = fread(status, sizeof(*status), sizeof(status) - 1U, f);
		fclose(f);
		if (size > 0) {
			// Strip trailing LF
			if (status[size - 1U] == '\n') {
				status[size - 1U] = '\0';
			}
			if (log_status) {
				LOG(LOG_DEBUG, "Battery status: %s", status);
			}
		} else {
			LOG(LOG_WARNING, "Could not read the battery status from sysfs!");
		}

		// c.f., power_supply_show_property @ drivers/power/supply/power_supply_sysfs.c
		//     & include/linux/power_supply.h
		// NOTE: Match the behavior of the NXP ntx_io ioctl (c.f., _Is_USB_plugged):
		//       false if discharging, true otherwise.
		// NOTE: The charger type check ought to then confirm that…
		if (strncmp(status, "Unknown", 7U) == 0U) {
			is_plugged = true;
		} else if (strncmp(status, "Charging", 8U) == 0U) {
			is_plugged = true;
		} else if (strncmp(status, "Discharging", 11U) == 0U) {
			is_plugged = false;
		} else if (strncmp(status, "Not charging", 12U) == 0U) {
			is_plugged = true;
		} else if (strncmp(status, "Full", 4U) == 0U) {
			is_plugged = true;
		}
	}

	return is_plugged;
}

// Check if the standalone USB-C controller thinks there's something plugged in
// c.f., drivers/input/misc/P15USB30216C.c
static int
    is_usbc_plugged(bool log_status)
{
	// Only found on a few Mk. 8 & Mk. 9 boards...
	if (!USBC_PLUG_SYSFS) {
		return -1;
	}

	FILE* f = fopen(USBC_PLUG_SYSFS, "re");
	if (!f) {
		return -1;
	}

	bool   is_plugged   = false;
	char   usbc_conn[8] = { 0 };
	size_t size         = fread(usbc_conn, sizeof(*usbc_conn), sizeof(usbc_conn) - 1U, f);
	fclose(f);
	f = NULL;
	if (size > 0) {
		// Strip trailing LF
		if (usbc_conn[size - 1U] == '\n') {
			usbc_conn[size - 1U] = '\0';
		}
	}

	// Should only ever be 0 or 1
	// NOTE: The i2c read exposes more information about what kind of device is on the other end of the cable,
	//       but that information is, sadly, lost to us (it *is* printed to dmesg, at least).
	if (usbc_conn[0] == '1') {
		is_plugged = true;
	}

	if (log_status) {
		LOG(LOG_DEBUG,
		    "Standalone USB-C controller cable detection: %s",
		    is_plugged ? "Connected" : "Disconnected");
	}

	return is_plugged;
}

// Return a fancy battery icon given the charge percentage…
static const char*
    get_battery_icon(uint8_t charge)
{
	if (charge >= 100) {
		return "\U000f0079";
	} else if (charge >= 90) {
		return "\U000f0082";
	} else if (charge >= 80) {
		return "\U000f0081";
	} else if (charge >= 70) {
		return "\U000f0080";
	} else if (charge >= 60) {
		return "\U000f007f";
	} else if (charge >= 50) {
		return "\U000f007e";
	} else if (charge >= 40) {
		return "\U000f007d";
	} else if (charge >= 30) {
		return "\U000f007c";
	} else if (charge >= 20) {
		return "\U000f007b";
	} else if (charge >= 10) {
		return "\U000f007a";
	} else {
		return "\U000f0083";
	}
}

// NOTE: Inspired from git's strtoul_ui @ git-compat-util.h
static int
    strtoul_hhu(const char* str, uint8_t* restrict result)
{
	// NOTE: We want to *reject* negative values (which strtoul does not)!
	if (strchr(str, '-')) {
		PFLOG(LOG_WARNING, "Passed a negative value (`%s`) to strtoul_hhu", str);
		return -EINVAL;
	}

	// Now that we know it's positive, we can go on with strtoul…
	char* endptr;
	errno                 = 0;    // To distinguish success/failure after call
	unsigned long int val = strtoul(str, &endptr, 10);

	if ((errno == ERANGE && val == ULONG_MAX) || (errno != 0 && val == 0)) {
		PFLOG(LOG_WARNING, "strtoul: %m");
		return -EINVAL;
	}

	// NOTE: It fact, always clamp to CHAR_MAX, since we may need to cast to a signed representation later.
	if (val > CHAR_MAX) {
		PFLOG(LOG_WARNING, "Passed a value larger than CHAR_MAX to strtoul_hhu, clamping it down to CHAR_MAX");
		val = CHAR_MAX;
	}

	if (endptr == str) {
		PFLOG(LOG_WARNING, "No digits were found in value `%s` assigned to a variable expecting an uint8_t", str);
		return -EINVAL;
	}

	// If we got here, strtoul() successfully parsed at least part of a number.
	// But we do want to enforce the fact that the input really was *only* an integer value.
	if (*endptr != '\0') {
		PFLOG(
		    LOG_WARNING,
		    "Found trailing characters (`%s`) behind value '%lu' assigned from string `%s` to a variable expecting an uint8_t",
		    endptr,
		    val,
		    str);
		return -EINVAL;
	}

	// Make sure there isn't a loss of precision on this arch when casting explicitly
	if ((uint8_t) val != val) {
		PFLOG(LOG_WARNING, "Loss of precision when casting value '%lu' to an uint8_t.", val);
		return -EINVAL;
	}

	*result = (uint8_t) val;
	return EXIT_SUCCESS;
}

// Pilfered from NickelMenu ;).
// c.f., https://github.com/pgaskin/NickelMenu/blob/85cd558715886069e70cbdcb1f9de43843e49e9f/src/util.h#L14-L23
static char*
    strtrim(char* s)
{
	if (!s) {
		return NULL;
	}
	char* a = s;
	char* b = s + strlen(s);
	for (; a < b && isspace((unsigned char) (*a)); a++) {
		;
	}
	for (; b > a && isspace((unsigned char) (*(b - 1))); b--) {
		;
	}
	*b = '\0';
	return a;
}

// Based on timespecsub from <bsd/sys/time.h>
static void
    timespec_delta(struct timespec* t2, struct timespec* t1, struct timespec* td)
{
	td->tv_sec  = t2->tv_sec - t1->tv_sec;
	td->tv_nsec = t2->tv_nsec - t1->tv_nsec;
	if (td->tv_nsec < 0) {
		td->tv_sec--;
		td->tv_nsec += 1000000000L;
	}
}

static time_t
    elapsed_time(struct timespec* t2, struct timespec* t1)
{
	// Compute the elapsed time between the two timestamps
	struct timespec td = { 0 };
	timespec_delta(t2, t1, &td);

	// We don't need nanosecond precision, just round up if necessary
	if (td.tv_nsec >= 500000000L) {
		td.tv_sec++;
		td.tv_nsec = 0;
	}

	return td.tv_sec;
}

// Yield for a bit on devices where we can't rely on MXCFB_WAIT_FOR_UPDATE_COMPLETE...
static int
    stub_wait_for_update_complete(int fbfd __attribute__((unused)), uint32_t marker __attribute__((unused)))
{
	// c.f., https://github.com/koreader/koreader-base/blob/21f4b974c7ab64a149075adc32318f87bf71dcdc/ffi/framebuffer_mxcfb.lua#L230-L235
	const struct timespec zzz = { 0L, 250 * 1000000L };    // 250ms
	return nanosleep(&zzz, NULL);
}

// Attempt to figure out the current frontlight intensity…
static uint8_t
    get_frontlight_intensity(void)
{
	// If all else fails, don't touch the FL by ensuring we return 0
	uint8_t intensity = 0U;

	// On Mk. 7, we can actually get it from sysfs, making our life far easier…
	FILE* f = fopen(FL_INTENSITY_SYSFS, "re");
	if (f) {
		char   fl_intensity[8] = { 0 };
		size_t size            = fread(fl_intensity, sizeof(*fl_intensity), sizeof(fl_intensity) - 1U, f);
		fclose(f);
		f = NULL;
		if (size > 0) {
			// Strip trailing LF
			if (fl_intensity[size - 1U] == '\n') {
				fl_intensity[size - 1U] = '\0';
			}
		}

		if (strtoul_hhu(fl_intensity, &intensity) < 0) {
			PFLOG(LOG_WARNING,
			      "Could not convert sysfs frontlight intensity value `%s` to an uint8_t!",
			      fl_intensity);
		} else {
			// We're good, don't bother trying to parse KOReader's settings!
			PFLOG(LOG_INFO, "sysfs says frontlight intensity is at %hhu%%", intensity);
			return intensity;
		}
	}

	const char* ko_dir = getenv("KOREADER_DIR");
	if (!ko_dir) {
		PFLOG(LOG_WARNING, "Unable to compute KOReader directory!");
		return intensity;
	}

	// Now, try to parse KOReader's settings…
	char ko_settings[PATH_MAX] = { 0 };
	snprintf(ko_settings, sizeof(ko_settings) - 1U, "%s/settings.reader.lua", ko_dir);
	f = fopen(ko_settings, "re");
	if (f) {
		bool    found_state     = false;
		bool    fl_state        = false;
		bool    found_intensity = false;
		uint8_t fl_intensity    = 0U;
		char*   line            = NULL;
		line                    = calloc(PIPE_BUF, sizeof(*line));
		if (!line) {
			PFLOG(LOG_ERR, "calloc: %m");
			fclose(f);
			return intensity;
		}
		while (fgets(line, PIPE_BUF, f)) {
			char* cur_line = line;
			if (strstr(cur_line, "[\"is_frontlight_on\"]")) {
				char* setting_key = strsep(&cur_line, "=");
				if (!setting_key) {
					PFLOG(LOG_WARNING,
					      "Could not parse 'is_frontline_on' in KOReader's settings (key)");
					continue;
				}

				char* setting_value = strsep(&cur_line, ",");
				if (!setting_value) {
					PFLOG(LOG_WARNING,
					      "Could not parse 'is_frontline_on' in KOReader's settings (value)");
					continue;
				}

				setting_value = strtrim(setting_value);

				if (strcmp(setting_value, "true") == 0) {
					found_state = true;
					fl_state    = true;
					PFLOG(LOG_INFO, "Frontlight is enabled in KOReader");
				} else if (strcmp(setting_value, "false") == 0) {
					found_state = true;
					fl_state    = false;
					PFLOG(LOG_INFO, "Frontlight is disabled in KOReader");
				} else {
					PFLOG(LOG_WARNING,
					      "Could not parse 'is_frontline_on' value! (`%s`)",
					      setting_value);
				}
			} else if (strstr(cur_line, "[\"frontlight_intensity\"]")) {
				char* setting_key = strsep(&cur_line, "=");
				if (!setting_key) {
					PFLOG(LOG_WARNING,
					      "Could not parse 'frontlight_intensity' in KOReader's settings (key)");
					continue;
				}

				char* setting_value = strsep(&cur_line, ",");
				if (!setting_value) {
					PFLOG(LOG_WARNING,
					      "Could not parse 'frontlight_intensity' in KOReader's settings (value)");
					continue;
				}

				setting_value = strtrim(setting_value);

				if (strtoul_hhu(setting_value, &fl_intensity) < 0) {
					PFLOG(LOG_WARNING,
					      "Could not convert KOReader frontlight intensity value `%s` to an uint8_t!",
					      setting_value);
				} else {
					found_intensity = true;
					PFLOG(LOG_INFO, "KOReader says frontlight intensity is at %hhu%%", fl_intensity);
				}
			}

			// If we've found & parsed both state & intensity, we're golden
			if (found_intensity && found_state) {
				// And if it is actually enabled, update the return value
				if (fl_state) {
					intensity = fl_intensity;
				}
				break;
			}
		}
		fclose(f);
		free(line);
	}

	return intensity;
}

// Fancy frontlight toggle :)
// Based on a PoC tested in https://github.com/koreader/koreader/pull/5421#discussion_r327812380
static void
    toggle_frontlight(bool state, uint8_t intensity, int ntxfd)
{
#define STEPS 20u
#define SLEEP 7u    // in ms
	// NOTE: The ioctl on newer devices actually blocks for noticeably longer than on older devices,
	//       c.f., https://github.com/koreader/koreader/blob/b40331085a565f99a95c27012b1aa3e71e3eb182/frontend/device/kobo/powerd.lua#L331-L332
	//       Here, we get away with this thanks to the larger amount of steps, combined with the fact that on newer devices,
	//       the ioctl won't block as long if it's called with the same requested intensity as the current intensity.
	const struct timespec zzz = { 0L, SLEEP * 1000000L };

	if (state == false) {
		// Ramp-down
		for (uint8_t i = 1U; i <= STEPS; i++) {
			int ptr = ifloorf(intensity - ((intensity / (float) STEPS) * i));
			int rc  = ioctl(ntxfd, CM_FRONT_LIGHT_SET, ptr);
			if (rc == -1) {
				PFLOG(LOG_WARNING, "Could not set frontlight intensity to %d%% (ioctl: %m)", ptr);
			}
			if (i < STEPS) {
				nanosleep(&zzz, NULL);
			}
		}
	} else {
		// Ramp-up
		for (uint8_t i = 1U; i <= STEPS; i++) {
			int ptr = iceilf(0U + ((intensity / (float) STEPS) * i));
			int rc  = ioctl(ntxfd, CM_FRONT_LIGHT_SET, ptr);
			if (rc == -1) {
				PFLOG(LOG_WARNING, "Could not set frontlight intensity to %d%% (ioctl: %m)", ptr);
			}
			if (i < STEPS) {
				nanosleep(&zzz, NULL);
			}
		}
	}
}

// Check if there's an auxiliary battery connected (e.g., the Sage's PowerCover).
static bool
    is_aux_battery_connected(void)
{
	FILE* f = fopen(CILIX_CONNECTED_SYSFS, "re");
	if (f) {
		char   cilix_conn[8] = { 0 };
		size_t size          = fread(cilix_conn, sizeof(*cilix_conn), sizeof(cilix_conn) - 1U, f);
		fclose(f);
		f = NULL;
		if (size > 0) {
			// Strip trailing LF
			if (cilix_conn[size - 1U] == '\n') {
				cilix_conn[size - 1U] = '\0';
			}
		}

		// Check if the PowerCover is currently connected
		if (cilix_conn[0] == '1') {
			return true;
		}
	}

	return false;
}

// We'll want to regularly update a display of the plug/charge status, and whether Wi-Fi is on or not
static void
    print_status(const USBMSContext* ctx)
{
	// Check if we're plugged in…
	bool usb_plugged = (*fxpIsUSBPlugged)(ctx->ntxfd, false);

	// Get the battery charge %
	uint8_t batt_perc = 0U;
	FILE*   f         = fopen(BATT_CAP_SYSFS, "re");
	if (f) {
		char   batt_charge[8] = { 0 };
		size_t size           = fread(batt_charge, sizeof(*batt_charge), sizeof(batt_charge) - 1U, f);
		fclose(f);
		if (size > 0) {
			// Strip trailing LF
			if (batt_charge[size - 1U] == '\n') {
				batt_charge[size - 1U] = '\0';
			}
		}

		if (strtoul_hhu(batt_charge, &batt_perc) < 0) {
			PFLOG(LOG_WARNING, "Could not convert battery charge value `%s` to an uint8_t!", batt_charge);
		}
	}

	// Check if there's a PowerCover
	bool    has_aux_battery = false;
	uint8_t aux_batt_perc   = 0U;
	if (ctx->fbink_state.device_id == DEVICE_KOBO_SAGE) {
		has_aux_battery = is_aux_battery_connected();

		if (has_aux_battery) {
			f = fopen(CILIX_BATT_CAP_SYSFS, "re");
			if (f) {
				char   cilix_charge[8] = { 0 };
				size_t size = fread(cilix_charge, sizeof(*cilix_charge), sizeof(cilix_charge) - 1U, f);
				fclose(f);
				if (size > 0) {
					// Strip trailing LF
					if (cilix_charge[size - 1U] == '\n') {
						cilix_charge[size - 1U] = '\0';
					}
				}

				if (strtoul_hhu(cilix_charge, &aux_batt_perc) < 0) {
					PFLOG(LOG_WARNING,
					      "Could not convert cilix charge value `%s` to an uint8_t!",
					      cilix_charge);
				}
			}
		}
	}

	// Check for Wi-Fi status
	// (c.f., https://github.com/koreader/koreader/blob/b5d33058761625111d176123121bcc881864a64e/frontend/device/kobo/device.lua#L451-L471)
	bool wifi_up            = false;
	char if_sysfs[PATH_MAX] = { 0 };
	snprintf(if_sysfs, sizeof(if_sysfs) - 1U, "/sys/class/net/%s/carrier", getenv("INTERFACE"));
	f = fopen(if_sysfs, "re");
	if (f) {
		char   carrier[8] = { 0 };
		size_t size       = fread(carrier, sizeof(*carrier), sizeof(carrier) - 1U, f);
		fclose(f);
		if (size > 0) {
			// Strip trailing LF
			if (carrier[size - 1U] == '\n') {
				carrier[size - 1U] = '\0';
			}
		}

		// If there's a carrier, Wi-Fi is up.
		if (carrier[0] == '1') {
			wifi_up = true;
		}
	}

	// Display the time
	time_t     t = time(NULL);
	struct tm  local_tm;
	struct tm* lt         = localtime_r(&t, &local_tm);
	char       sz_time[6] = { 0 };
	strftime(sz_time, sizeof(sz_time), "%H:%M", lt);

	if (has_aux_battery) {
		fbink_printf(ctx->fbfd,
			     &ctx->ot_cfg,
			     &ctx->fbink_cfg,
			     "%s • \uf017 %s • %s (%hhu%%) + %s (%hhu%%) • %s",
			     usb_plugged ? "\U000f06a5" : "\U000f06a6",
			     sz_time,
			     get_battery_icon(batt_perc),
			     batt_perc,
			     get_battery_icon(aux_batt_perc),
			     aux_batt_perc,
			     wifi_up ? "\U000f05a9" : "\U000f05aa");
	} else {
		fbink_printf(ctx->fbfd,
			     &ctx->ot_cfg,
			     &ctx->fbink_cfg,
			     "%s • \uf017 %s • %s (%hhu%%) • %s",
			     usb_plugged ? "\U000f06a5" : "\U000f06a6",
			     sz_time,
			     get_battery_icon(batt_perc),
			     batt_perc,
			     wifi_up ? "\U000f05a9" : "\U000f05aa");
	}
}

static void
    print_icon(const char* string, USBMSContext* ctx)
{
	ctx->fbink_cfg.is_halfway = true;
	fbink_print_ot(ctx->fbfd, string, &ctx->icon_cfg, &ctx->fbink_cfg, NULL);
	ctx->fbink_cfg.is_halfway = false;
}

static int
    print_msg(const char* string, USBMSContext* ctx)
{
	return fbink_print_ot(ctx->fbfd, string, &ctx->msg_cfg, &ctx->fbink_cfg, NULL);
}

static int
    print_countdown(time_t left, USBMSContext* ctx)
{
	const char* icon = NULL;
	switch (left % 3) {
		case 0:
			icon = "\U0000f251";
			break;
		case 1:
			icon = "\U0000f253";
			break;
		case 2:
			icon = "\U0000f252";
			break;
		default:
			icon = "\U0000f254";
			break;
	}
	return fbink_printf(ctx->fbfd, &ctx->countdown_cfg, &ctx->fbink_cfg, "%s %lld", icon, (long long int) left);
}

static int
    clear_countdown(USBMSContext* ctx)
{
	return fbink_print_ot(ctx->fbfd, " ", &ctx->countdown_cfg, &ctx->fbink_cfg, NULL);
}

// Poor man's grep in /proc/modules
__attribute((nonnull(1))) static bool
    is_module_loaded(const char* needle)
{
	FILE* f = fopen("/proc/modules", "re");
	if (f) {
		char   line[PIPE_BUF];
		size_t len = strlen(needle);
		while (fgets(line, sizeof(line), f)) {
			if (strncmp(line, needle, len) == 0) {
				fclose(f);
				return true;
			}
		}
		fclose(f);
	}

	return false;
}

// Parse an evdev event, looking for a power button press
static bool
    handle_evdev(struct libevdev* dev)
{
	int rc = 1;
	do {
		struct input_event ev;
		rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
		if (rc == LIBEVDEV_READ_STATUS_SYNC) {
			while (rc == LIBEVDEV_READ_STATUS_SYNC) {
				// NOTE: We're ignoring the sync delta events here.
				rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
			}
		} else if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
			// Check if it's a power button press (well, release, actually)
			if (libevdev_event_is_code(&ev, EV_KEY, KEY_POWER) == 1 && ev.value == 0) {
				return true;
			}
		}
	} while (rc == LIBEVDEV_READ_STATUS_SYNC || rc == LIBEVDEV_READ_STATUS_SUCCESS);
	if (rc != LIBEVDEV_READ_STATUS_SUCCESS && rc != -EAGAIN) {
		PFLOG(LOG_ERR, "Failed to handle input events: %s", strerror(-rc));
	}

	return false;
}

// Parse an evdev event, looking for a SW_DOCK switch
static int
    handle_usbc_evdev(struct libevdev* dev)
{
	int rc = 1;
	do {
		struct input_event ev;
		rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
		if (rc == LIBEVDEV_READ_STATUS_SYNC) {
			while (rc == LIBEVDEV_READ_STATUS_SYNC) {
				// NOTE: We're ignoring the sync delta events here.
				rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
			}
		} else if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
			// If it's a SW_DOCK switch, return the value
			if (libevdev_event_is_code(&ev, EV_SW, SW_DOCK) == 1) {
				LOG(LOG_NOTICE, "Caught a USB-C plug %s event", ev.value ? "in" : "out");
				return ev.value;
			}
		}
	} while (rc == LIBEVDEV_READ_STATUS_SYNC || rc == LIBEVDEV_READ_STATUS_SUCCESS);
	if (rc != LIBEVDEV_READ_STATUS_SUCCESS && rc != -EAGAIN) {
		PFLOG(LOG_ERR, "Failed to handle input events: %s", strerror(-rc));
	}

	return -1;
}

// Parse an uevent
static int
    handle_uevent(struct uevent_listener* l, struct uevent* uevp)
{
	ue_reset_event(uevp);
	ssize_t len = xread(l->pfd.fd, uevp->buf, sizeof(uevp->buf) - 1U);
	if (len == -1) {
		if (errno == ENOBUFS) {
			// NOTE: Unlike ue_wait_for_event, don't recover:
			//       since events were most likely lost, we'll consider this fatal
			PFLOG(LOG_WARNING, "uevent overrun!");
		}
		PFLOG(LOG_CRIT, "read: %m");
		return ERR_LISTENER_RECV;
	}
	char* end = uevp->buf + len;
	*end      = '\0';

	int rc = ue_parse_event_msg(uevp, (size_t) len);
	if (rc == EXIT_SUCCESS) {
		PFLOG(LOG_DEBUG, "uevent successfully parsed");
		return EXIT_SUCCESS;
	} else if (rc == ERR_PARSE_UDEV) {
		PFLOG(LOG_DEBUG, "skipped %zd bytes udev uevent: `%.*s`", len, (int) len, uevp->buf);
	} else if (rc == ERR_PARSE_INVALID_HDR) {
		PFLOG(LOG_DEBUG, "skipped %zd bytes malformed uevent: `%.*s`", len, (int) len, uevp->buf);
	} else {
		PFLOG(LOG_DEBUG, "skipped %zd bytes unsupported uevent: `%.*s`", len, (int) len, uevp->buf);
	}

	return EXIT_FAILURE;
}

int
    main(void)
{
	// So far, so good ;).
	int                    rv       = EXIT_SUCCESS;
	int                    pwd      = -1;
	char*                  abs_pwd  = NULL;
	bool                   is_CJK   = false;
	struct uevent_listener listener = { 0 };
	listener.pfd.fd                 = -1;
	struct libevdev* dev            = NULL;
	USBMSContext     ctx            = { 0 };
	int              evfd           = -1;
	int              clockfd        = -1;
	struct libevdev* usbc_dev       = NULL;
	int              usbc_fd        = -1;

	// Close any non-standard fds before we open any ourselves (this should be a NOP on sane launchers)
	bsd_closefrom(3);

	// We'll be chatting exclusively over syslog, because duh.
	openlog("usbms", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);

	// Say hello
	LOG(LOG_INFO, "Initializing USBMS %s (%s)", USBMS_VERSION, USBMS_TIMESTAMP);

	// Redirect stdin/stdout/stderr to /dev/null
	int fd = open("/dev/null", O_RDONLY);
	if (fd != -1) {
		dup2(fd, fileno(stdin));
		close(fd);
	} else {
		PFLOG(LOG_CRIT, "open(\"/dev/null\", O_RDONLY): %m");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}
	fd = open("/dev/null", O_RDWR);
	if (fd != -1) {
		dup2(fd, fileno(stdout));
		dup2(fd, fileno(stderr));
		close(fd);
	} else {
		PFLOG(LOG_CRIT, "open(\"/dev/null\", O_RDWR): %m");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}

	// We'll want to jump to /, and only get back to our original PWD on exit…
	// c.f., man getcwd for the fchdir trick, as we can certainly spare the fd ;).
	// NOTE: While using O_PATH would be nice, the flag itself is Linux 2.6.39+,
	//       but, more importantly, the resulting fd is only usable with fchdir since Linux 3.5+…
	//       And, of course, Mk. 6 devices are smack in that sweet spot: they run Linux 3.0.35,
	//       where O_PATH is supported, but not by fchdir ;).
	pwd = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (pwd == -1) {
		PFLOG(LOG_CRIT, "open(\".\"): %m");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}
	// We do need the pathname to load resources, though…
	abs_pwd = get_current_dir_name();
	if (chdir("/") == -1) {
		PFLOG(LOG_CRIT, "chdir(\"/\"): %m");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}
	char resource_path[PATH_MAX] = { 0 };

	// Make sure we have a klogd instance redirecting the kernel logs to syslog, so we get some context interleaved with our own logging.
	snprintf(resource_path, sizeof(resource_path) - 1U, "%s/scripts/launch-klogd.sh", abs_pwd);
	system(resource_path);

	// NOTE: The font we ship only covers LGC scripts. Blacklist a few languages where we know it won't work,
	//       based on KOReader's own language list (c.f., frontend/ui/language.lua).
	//       Because English is better than the replacement character ;p.
	//       We do jump through a few hoops to attempt to salvage CJK support…
	const char* lang = getenv("LANGUAGE");
	if (lang) {
		if (strncmp(lang, "he", 2U) == 0 || strncmp(lang, "ar", 2U) == 0 || strncmp(lang, "fa", 2U) == 0) {
			LOG(LOG_NOTICE, "Your language (%s) is unsupported (RTL), falling back to English", lang);
			setenv("LANGUAGE", "C", 1);
		} else if (strncmp(lang, "bn", 2U) == 0 || strncmp(lang, "hi", 2U) == 0) {
			LOG(LOG_NOTICE, "Your language (%s) is unsupported (!LGC), falling back to English", lang);
			setenv("LANGUAGE", "C", 1);
		} else if (strncmp(lang, "ja", 2U) == 0 || strncmp(lang, "ko", 2U) == 0 || strncmp(lang, "zh", 2U) == 0) {
			LOG(LOG_NOTICE, "Your language (%s) may be badly handled (CJK)!", lang);

			// If we don't actually have a translation ready, don't set the CJK flag, and fallback to English.
			snprintf(
			    resource_path, sizeof(resource_path) - 1U, "%s/l10n/%s/LC_MESSAGES/usbms.mo", abs_pwd, lang);
			if (access(resource_path, F_OK) == 0) {
				is_CJK = true;
			} else {
				LOG(LOG_NOTICE,
				    "Your CJK language (%s) hasn't been translated yet, falling back to English",
				    lang);
				setenv("LANGUAGE", "C", 1);
			}
		}
	}

	// NOTE: Setup gettext, with a rather nasty twist, because of Kobo's utter lack of sane locales setup:
	//       In order to translate stuff, gettext needs a valid setlocale call to a locale that *isn't* C or POSIX.
	//       Unfortunately, Kobo doesn't compile *any* locales…
	//       The minimal LC_* category we need for our translation is LC_MESSAGES.
	//       It requires SYS_LC_MESSAGES from the glibc (usually shipped in archive form on sane systems).
	//       So, we build one manually (via localedef) from the en_US definitions, and set-it up in a bogus custom locale,
	//       which we use for our LC_MESSAGES setlocale call.
	//       We of course ship it, and we enforce our own l10n directory as the global locale search path…
	//       Then, we can *finally* choose our translation language via the LANGUAGE env var…
	//       c.f., https://stackoverflow.com/q/36857863
	snprintf(resource_path, sizeof(resource_path) - 1U, "%s/l10n", abs_pwd);
	// We can't touch the rootfs, so, teach the glibc about our awful workaround…
	// (c.f., https://www.gnu.org/software/libc/manual/html_node/Locale-Names.html)
	setenv("LOCPATH", resource_path, 1);
	// Then, because gettext wants a setlocale call, let's give it one that's enough to get us translations,
	// and minimal enough that we don't have to ship a crazy amount of useless locale stuff…
	setlocale(LC_MESSAGES, "kobo");
	// And now stuff can more or less start looking like a classic gettext setup…
	bindtextdomain("usbms", resource_path);
	textdomain("usbms");
	// The whole locale shenanigan means more hand-holding is needed to enforce UTF-8…
	bind_textdomain_codeset("usbms", "UTF-8");

	// Setup FBInk
	ctx.fbink_cfg.row         = -5;
	ctx.fbink_cfg.is_centered = true;
	ctx.fbink_cfg.is_padded   = true;
	ctx.fbink_cfg.to_syslog   = true;
	// We'll want early errors to already go to syslog
	fbink_update_verbosity(&ctx.fbink_cfg);

	if ((ctx.fbfd = fbink_open()) == ERRCODE(EXIT_FAILURE)) {
		LOG(LOG_CRIT, "Could not open the framebuffer, aborting…");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}
	if (fbink_init(ctx.fbfd, &ctx.fbink_cfg) == ERRCODE(EXIT_FAILURE)) {
		LOG(LOG_CRIT, "Could not initialize FBInk, aborting…");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}
	LOG(LOG_INFO, "Initialized FBInk %s", fbink_version());

	// Now that FBInk has been initialized, setup the USB product ID for the current device
	fbink_get_state(&ctx.fbink_cfg, &ctx.fbink_state);
	setup_usb_ids(ctx.fbink_state.device_id);

	// Auto-detect the input device for the power button
	size_t            dev_count;
	size_t            matches = 0U;
	// Look for a power button that isn't featured by a touchscreen (because some panels have *extremely* weird caps...).
	FBInkInputDevice* devices = fbink_input_scan(INPUT_POWER_BUTTON, INPUT_TOUCHSCREEN, SCAN_ONLY, &dev_count);
	if (devices) {
		FBInkInputDevice* matched_device = NULL;
		for (FBInkInputDevice* device = devices; device < devices + dev_count; device++) {
			// We *should* only ever get one match, but let's be extra cautious...
			if (device->matched) {
				matches++;
				matched_device = device;
			}
			// Also handle the weird input device for the standalone USB-C controller found on some sunxi-era devices...
			if (device->type == INPUT_UNKNOWN && strcmp(device->name, "P15USB30216C") == 0) {
				USBC_EVDEV = strdup(device->path);
				LOG(LOG_INFO, "Found a standalone USB-C controller input device @ `%s`", USBC_EVDEV);
				// We need to poke at a dev_attr of this virtual input device, which means we need its number...
				char* p = NULL;
				int   n =
				    snprintf(p, 0, SUNXI_USBC_PLUG_SYSFS_FMT, device->path + strlen("/dev/input/event"));
				if (n >= 0) {
					size_t size = (size_t) n + 1U;
					p           = malloc(size);
					if (p) {
						n = snprintf(p,
							     size,
							     SUNXI_USBC_PLUG_SYSFS_FMT,
							     device->path + strlen("/dev/input/event"));
						if (n < 0) {
							free(p);
						} else {
							USBC_PLUG_SYSFS = p;
						}
					}
				}
				// Double check that we indeed have a sysfs entry at the computed path...
				if (USBC_PLUG_SYSFS) {
					if (access(USBC_PLUG_SYSFS, F_OK) == 0) {
						LOG(LOG_INFO,
						    "Found USB_PLUG sysfs entry for standalone USB-C controller @ `%s`",
						    USBC_PLUG_SYSFS);
					} else {
						LOG(LOG_WARNING,
						    "Unable to access USB_PLUG sysfs entry for standalone USB-C controller @ `%s`",
						    USBC_PLUG_SYSFS);
						// We'll have to do without...
						free(USBC_PLUG_SYSFS);
						USBC_PLUG_SYSFS = NULL;
					}
				} else {
					LOG(LOG_WARNING,
					    "Failed to compute USB_PLUG sysfs entry for standalone USB-C controller!");
				}
			}
		}
		if (matches > 1U) {
			// We pick the last one by virtue of event0 being fairly solidly set in stone as the main NTX key hub.
			// If there's a dedicated power button, it'll come later.
			LOG(LOG_WARNING,
			    "Found more that one potential match for the power button's input device, picking the last one…");
		}
		if (matched_device) {
			NTX_KEYS_EVDEV = strdup(matched_device->path);
		}
		free(devices);
	}
	if (matches == 0U) {
		LOG(LOG_WARNING, "Couldn't auto-detect the power button's input device, assuming event0…");
		NTX_KEYS_EVDEV = strdup("/dev/input/event0");
	}

	// And setup the sysfs paths & usb check based on the device…
	if (ctx.fbink_state.is_mtk) {
		BATT_CAP_SYSFS     = MTK_BATT_CAP_SYSFS;
		CHARGER_TYPE_SYSFS = MTK_CHARGER_TYPE_SYSFS;

		// The CM_USB_Plug_IN ioctl still doesn't look at the right power supplies...
		BATT_STATUS_SYSFS = MTK_BATT_STATUS_SYSFS;
		fxpIsUSBPlugged   = &sysfs_is_usb_plugged;
	} else if (ctx.fbink_state.is_sunxi) {
		BATT_CAP_SYSFS     = SUNXI_BATT_CAP_SYSFS;
		CHARGER_TYPE_SYSFS = SUNXI_CHARGER_TYPE_SYSFS;

		// The CM_USB_Plug_IN ioctl is currently unreliable (it pokes at the wrong power supply)…
		BATT_STATUS_SYSFS = SUNXI_BATT_STATUS_SYSFS;
		fxpIsUSBPlugged   = &sysfs_is_usb_plugged;

		// Enforce REAGL, since AUTO is not recommended on sunxi
		ctx.fbink_cfg.wfm_mode = WFM_REAGL;
	} else {
		// NOTE: Mk. 9 devices may have different hardware revisions with meaningful changes,
		//       and/or different hardware than earlier NTX boards, period;
		//       so we'll just auto-detect everything...

		// Using the new battery & charger sysfs paths
		if (access(SUNXI_BATT_CAP_SYSFS, F_OK) == 0) {
			BATT_CAP_SYSFS = SUNXI_BATT_CAP_SYSFS;

			// NOTE: I'm going to assume this means the ioctl is similarly broken...
			BATT_STATUS_SYSFS = SUNXI_BATT_STATUS_SYSFS;
			fxpIsUSBPlugged   = &sysfs_is_usb_plugged;
		} else {
			BATT_CAP_SYSFS  = NXP_BATT_CAP_SYSFS;
			fxpIsUSBPlugged = &ioctl_is_usb_plugged;
		}

		if (access(SUNXI_CHARGER_TYPE_SYSFS, F_OK) == 0) {
			CHARGER_TYPE_SYSFS = SUNXI_CHARGER_TYPE_SYSFS;
		} else if (access(STD_CHARGER_TYPE_SYSFS, F_OK) == 0) {
			CHARGER_TYPE_SYSFS = STD_CHARGER_TYPE_SYSFS;
		} else {
			CHARGER_TYPE_SYSFS = NXP_CHARGER_TYPE_SYSFS;
		}
	}
	// Check whether the device actually supports charger_type...
	if (access(CHARGER_TYPE_SYSFS, F_OK) != 0) {
		LOG(LOG_INFO,
		    "Unable to check charger type on your device (please report this issue if your device is actually newer than Mk. 7).");
		// Lets us quickly check whether this is supported later
		CHARGER_TYPE_SYSFS = NULL;
	}
	// Deal with devices where fbink_wait_for_complete may timeout...
	if (ctx.fbink_state.unreliable_wait_for) {
		fxpWaitForUpdateComplete = &stub_wait_for_update_complete;
	} else {
		fxpWaitForUpdateComplete = &fbink_wait_for_complete;
	}

	// Setup libue
	int rc = -1;
	rc     = ue_init_listener(&listener);
	if (rc < 0) {
		LOG(LOG_CRIT, "Could not initialize libue listener (%d)", rc);
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}
	LOG(LOG_INFO, "Initialized libue v%s", LIBUE_VERSION);

	// Setup libevdev
	evfd = open(NTX_KEYS_EVDEV, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
	if (evfd == -1) {
		PFLOG(LOG_CRIT, "open(NTX_KEYS_EVDEV): %m");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}

	dev = libevdev_new();
	libevdev_set_device_log_function(dev, &libevdev_to_syslog, LIBEVDEV_LOG_INFO, NULL);
	rc = libevdev_set_fd(dev, evfd);
	if (rc < 0) {
		LOG(LOG_CRIT, "Could not initialize libevdev (%s)", strerror(-rc));
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}
	// Check that nothing else has grabbed the input device, because that would prevent us from using it…
	if (libevdev_grab(dev, LIBEVDEV_GRAB) != 0) {
		LOG(LOG_CRIT,
		    "Cannot read input events because the input device is currently grabbed by something else!");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}
	// And we ourselves don't need to grab it, so, don't ;).
	libevdev_grab(dev, LIBEVDEV_UNGRAB);
	LOG(LOG_INFO, "Initialized libevdev v%s for device `%s`", LIBEVDEV_VERSION, libevdev_get_name(dev));

	// Ditto for the standalone USB-C controller, if any
	if (USBC_EVDEV) {
		usbc_fd = open(USBC_EVDEV, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
		if (usbc_fd == -1) {
			PFLOG(LOG_CRIT, "open(USBC_EVDEV): %m");
			rv = USBMS_EARLY_EXIT;
			goto cleanup;
		}

		usbc_dev = libevdev_new();
		libevdev_set_device_log_function(usbc_dev, &libevdev_to_syslog, LIBEVDEV_LOG_INFO, NULL);
		rc = libevdev_set_fd(usbc_dev, usbc_fd);
		if (rc < 0) {
			LOG(LOG_CRIT, "Could not initialize libevdev for USB-C controller (%s)", strerror(-rc));
			rv = USBMS_EARLY_EXIT;
			goto cleanup;
		}
		// Check that nothing else has grabbed the input device, because that would prevent us from using it…
		if (libevdev_grab(usbc_dev, LIBEVDEV_GRAB) != 0) {
			LOG(LOG_CRIT,
			    "Cannot read input events from USB-C controller because the input device is currently grabbed by something else!");
			rv = USBMS_EARLY_EXIT;
			goto cleanup;
		}
		// And we ourselves don't need to grab it, so, don't ;).
		libevdev_grab(usbc_dev, LIBEVDEV_UNGRAB);
		LOG(LOG_INFO, "Initialized libevdev v%s for device `%s`", LIBEVDEV_VERSION, libevdev_get_name(usbc_dev));
	}

	// Much like in KOReader's OTAManager, check if we can use pipefail in a roundabout way,
	// because old busybox ash versions will *abort* on set failures…
	rc = system("set -o pipefail 2>/dev/null");
	if (rc == EXIT_SUCCESS) {
		setenv("WITH_PIPEFAIL", "true", 1);
	} else {
		setenv("WITH_PIPEFAIL", "false", 1);
	}

	// Setup the fd for ntx_io ioctls
	ctx.ntxfd = open("/dev/ntx_io", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (ctx.ntxfd == -1) {
		PFLOG(LOG_CRIT, "open(\"/dev/ntx_io\"): %m");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}

	// Setup the timer for clock ticks
	clockfd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
	if (clockfd == -1) {
		PFLOG(LOG_CRIT, "timerfd_create: %m");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}
	// Arm it to get a tick on every minute, on the dot.
	struct timespec now_ts = { 0 };
	clock_gettime(CLOCK_REALTIME, &now_ts);
	struct itimerspec clock_timer;
	// NOTE: Setting an initial expiration time in the past is perfectly valid:
	//       the fd will just be marked as readable immediately
	//       (and compute an accurate amount of expirations, as if it had ticked all this time).
	// Here, the next minute on the dot is good enough for us,
	// so, just round the current timestamp up to the next multiple of 60.
	clock_timer.it_value.tv_sec     = (now_ts.tv_sec + 60 - 1) / 60 * 60;
	clock_timer.it_value.tv_nsec    = 0;
	// Tick every minute
	clock_timer.it_interval.tv_sec  = 60;
	clock_timer.it_interval.tv_nsec = 0;
	if (timerfd_settime(clockfd, TFD_TIMER_ABSTIME, &clock_timer, NULL) == -1) {
		PFLOG(LOG_CRIT, "timerfd_settime: %m");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}

	// Display our header
	ctx.fbink_cfg.no_refresh = true;
	fbink_cls(ctx.fbfd, &ctx.fbink_cfg, NULL, false);
	ctx.ot_cfg.margins.top = (short int) ctx.fbink_state.font_h;
	ctx.ot_cfg.size_px     = (unsigned short int) (ctx.fbink_state.font_h * 2U);
	snprintf(resource_path, sizeof(resource_path) - 1U, "%s/resources/fonts/CaskaydiaCove_NF.ttf", abs_pwd);
	if (fbink_add_ot_font_v2(resource_path, FNT_REGULAR, &ctx.icon_cfg) != EXIT_SUCCESS) {
		PFLOG(LOG_CRIT, "Could not load main font!");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}
	// NOTE: Minor hackery: instead of the custom LGC Nerdfont we ship, for CJK, use KOReader's own CJK font…
	//       (The only remotely CJK-ish NerdFont available is M+, and it's more J than CJK ;)).
	if (is_CJK) {
		snprintf(
		    resource_path, sizeof(resource_path) - 1U, "%s/resources/fonts/NotoSansCJKsc-Regular.otf", abs_pwd);
		if (fbink_add_ot_font_v2(resource_path, FNT_REGULAR, &ctx.msg_cfg) != EXIT_SUCCESS) {
			PFLOG(LOG_CRIT, "Could not load CJK font!");
			rv = USBMS_EARLY_EXIT;
			goto cleanup;
		}
		// The first ot_cfg print (title bar) actually requires CJK support, so point it at our CJK font…
		ctx.ot_cfg.font = ctx.msg_cfg.font;
	} else {
		// If we don't need CJK support, we simply use the main font everywhere
		ctx.ot_cfg.font  = ctx.icon_cfg.font;
		ctx.msg_cfg.font = ctx.icon_cfg.font;
	}
	fbink_print_ot(ctx.fbfd, _("USB Mass Storage"), &ctx.ot_cfg, &ctx.fbink_cfg, NULL);
	if (is_CJK) {
		// Back to the main font, as this will only be used for the status bar from now on
		ctx.ot_cfg.font = ctx.icon_cfg.font;
	}
	ctx.fbink_cfg.ignore_alpha  = true;
	ctx.fbink_cfg.halign        = CENTER;
	ctx.fbink_cfg.scaled_height = (short int) (ctx.fbink_state.screen_height / 10U);
	ctx.fbink_cfg.row           = 3;
	snprintf(resource_path, sizeof(resource_path) - 1U, "%s/resources/img/koreader.png", abs_pwd);
	fbink_print_image(ctx.fbfd, resource_path, 0, 0, &ctx.fbink_cfg);
	ctx.fbink_cfg.no_refresh  = false;
	ctx.fbink_cfg.is_flashing = true;
	fbink_refresh(ctx.fbfd, 0, 0, 0, 0, &ctx.fbink_cfg);
	ctx.fbink_cfg.is_flashing = false;

	// Display a minimal status bar on screen
	tzset();
	ctx.fbink_cfg.row      = -3;
	ctx.ot_cfg.size_px     = (unsigned short int) (ctx.fbink_state.font_h * 2.2f);
	ctx.ot_cfg.margins.top = (short int) -(ctx.fbink_state.font_h * 3U);
	ctx.ot_cfg.padding     = HORI_PADDING;
	print_status(&ctx);

	// Setup the center icon display
	ctx.icon_cfg.size_px = (unsigned short int) (ctx.fbink_state.font_h * 30U);
	ctx.icon_cfg.padding = HORI_PADDING;

	// The various lsmod checks will take a while, so, start with the initial cable status…
	bool usb_plugged = (*fxpIsUSBPlugged)(ctx.ntxfd, true);
	print_icon(usb_plugged ? "\U000f0201" : "\U000f0202", &ctx);

	// Setup the message area
	ctx.msg_cfg.size_px        = (unsigned short int) (ctx.fbink_state.font_h * 2U);
	ctx.fbink_cfg.row          = -14;
	ctx.msg_cfg.margins.top    = (short int) -(ctx.fbink_state.font_h * 14U);
	// We want enough space for 4 lines (+/- metrics shenanigans)
	ctx.msg_cfg.margins.bottom = (short int) (ctx.fbink_state.font_h * (14U - (4U * 2U) - 1U));
	ctx.msg_cfg.padding        = FULL_PADDING;

	// Setup the countdown area
	ctx.countdown_cfg.font        = ctx.ot_cfg.font;
	ctx.countdown_cfg.size_px     = (unsigned short int) (ctx.fbink_state.font_h * 2U);
	ctx.countdown_cfg.margins.top = (short int) -(ctx.fbink_state.font_h * 6U);
	ctx.countdown_cfg.padding     = HORI_PADDING;

	// And now, on to the fun stuff!
	bool need_early_abort = false;
	bool early_unmount    = false;
	// If we're in USBNet mode, abort!
	// (tearing it down properly is out of our scope, since we can't really know how the user enabled it in the first place).
	if (is_module_loaded("g_ether ")) {
		LOG(LOG_ERR, "Device is in USBNet mode, aborting");
		need_early_abort = true;

		print_icon("\U000f0200", &ctx);
		print_msg(    // @translators: First unicode codepoint is an icon, leave it as-is.
		    _("\uf071 Please disable USBNet manually!\nPress the power button to exit."),
		    &ctx);
	}

	// Same deal for USBSerial…
	if (is_module_loaded("g_serial ")) {
		LOG(LOG_ERR, "Device is in USBSerial mode, aborting");
		need_early_abort = true;

		// NOTE: There's also U+fb5b for a serial cable icon
		print_icon("\ue795", &ctx);
		print_msg(    // @translators: First unicode codepoint is an icon, leave it as-is.
		    _("\uf071 Please disable USBSerial manually!\nPress the power button to exit."),
		    &ctx);
	}

	// Try to cobble something together for configfs devices...
	if (access(KOBO_USB_GADGET_STATE_MTK, F_OK) == 0) {
		LOG(LOG_INFO, "Checking MTK USB gadget state");
		FILE* f = fopen(KOBO_USB_GADGET_STATE_MTK, "re");
		if (f) {
			// The longest possible string happens to be exactly 15 characters
			char   gadget_state[16] = { 0 };
			size_t size = fread(gadget_state, sizeof(*gadget_state), sizeof(gadget_state) - 1U, f);
			fclose(f);
			if (size > 0) {
				// Strip trailing LF (there shouldn't be any here)
				if (gadget_state[size - 1U] == '\n') {
					gadget_state[size - 1U] = '\0';
				}
			} else {
				LOG(LOG_WARNING, "Could not read the gadget type from sysfs!");
			}

			// c.f., usb_state_string @ drivers/usb/common/common.c
			if (strcmp(gadget_state, "not attached") != 0U) {
				LOG(LOG_ERR,
				    "Device already has a USB gadget attached to the UDC, aborting (current state: `%.*s`)",
				    (int) (sizeof(gadget_state) - 1U),
				    gadget_state);
				need_early_abort = true;

				print_icon("\U000f11f0", &ctx);
				print_msg(    // @translators: First unicode codepoint is an icon, leave it as-is.
				    _("\uf071 Please disable your custom USB gadget manually!\nPress the power button to exit."),
				    &ctx);
			}
		}
	}

	// NOTE: On the Sage, if the PowerCover is plugged, crossing the Cilix charge thresholds *may* reset the USB connection…
	//       This *will* break I/O, and there's a threshold at 99% which is *very* easy to trip on a recently charged device.
	//       To avoid any potential issues, abort if we're connected to the PowerCover.
	if (ctx.fbink_state.device_id == DEVICE_KOBO_SAGE) {
		if (is_aux_battery_connected()) {
			LOG(LOG_ERR, "Device is inside a PowerCover, aborting");
			need_early_abort = true;

			// NOTE: Shh, nobody can tell it's actually a sign-out icon... :D.
			print_icon("\uf426", &ctx);
			print_msg(    // @translators: First unicode codepoint is an icon, leave it as-is.
			    _("\uf071 Please take the device out of the PowerCover!\nPress the power button to exit."),
			    &ctx);
		}
	}

	// We'll want to check both the internal storage and the SD card, as both get exported.
	const USBMSPartition mount_points[] = {
		{ PARTITION_INTERNAL, "Internal",    KOBO_PARTITION,    KOBO_MOUNTPOINT },
		{ PARTITION_EXTERNAL, "External", KOBO_SD_PARTITION, KOBO_SD_MOUNTPOINT },
		{     PARTITION_NONE,       NULL,              NULL,               NULL }
	};
	for (size_t i = 0U; !need_early_abort && mount_points[i].name; i++) {
		// Check if the character device actually exists, because SD cards ;).
		if (mount_points[i].id != PARTITION_INTERNAL && access(mount_points[i].device, F_OK) != 0) {
			LOG(LOG_INFO, "%s storage device not available.", mount_points[i].name);
			continue;
		}

		// Wee bit of trickery with an obscure umount2 feature, to see if the mountpoint is currently busy,
		// without actually unmounting it for real…
		rc = umount2(mount_points[i].mountpoint, MNT_EXPIRE);
		if (rc != EXIT_SUCCESS) {
			if (errno == EAGAIN) {
				// That means we're good to go ;).
				LOG(LOG_INFO,
				    "%s storage partition wasn't busy, it's been successfully marked as expired.",
				    mount_points[i].name);
			} else if (errno == EBUSY) {
				LOG(LOG_WARNING, "%s storage partition is busy, can't export it!", mount_points[i].name);
				print_icon(mount_points[i].id == PARTITION_INTERNAL ? "\U000f02ca" : "\U000f07dc", &ctx);

				// Start a little bit higher than usual to leave us some room…
				ctx.fbink_cfg.row       = -16;
				ctx.msg_cfg.margins.top = (short int) -(ctx.fbink_state.font_h * 16U);
				rc = print_msg(    // @translators: First unicode codepoint is an icon, leave it as-is.
				    _("\uf071 Filesystem is busy! Offending processes:"),
				    &ctx);

				// And now, switch to a smaller font size when consuming the script's output…
				ctx.msg_cfg.padding              = HORI_PADDING;
				const unsigned short int size_px = ctx.msg_cfg.size_px;
				ctx.msg_cfg.size_px              = ctx.fbink_state.font_h;
				ctx.msg_cfg.margins.top          = (short int) rc;
				// Drop the bottom margin to allow stomping over the status bar…
				ctx.msg_cfg.margins.bottom       = 0;

				LOG(LOG_WARNING, "Listing all offending processes…");
				snprintf(resource_path,
					 sizeof(resource_path) - 1U,
					 "%s/scripts/fuser-check.sh '%s'",
					 abs_pwd,
					 mount_points[i].mountpoint);
				FILE* f = popen(resource_path, "re");
				if (f) {
					char line[PIPE_BUF];
					while (fgets(line, sizeof(line), f)) {
						rc                      = print_msg(line, &ctx);
						ctx.msg_cfg.margins.top = (short int) rc;
					}

					// Back to normal :)
					ctx.msg_cfg.size_px = size_px;

					rc = pclose(f);
					if (rc != EXIT_SUCCESS) {
						// Hu oh… Print a giant warning, and abort.
						LOG(LOG_CRIT, "The fuser script failed (%d)!", rc);
						print_icon("\uf06a", &ctx);
						rc = print_msg(
						    // @translators: First unicode codepoint is an icon, leave it as-is. fuser is a program name, leave it as-is.
						    _("\uf071 The fuser script failed!"),
						    &ctx);
						ctx.msg_cfg.margins.top = (short int) rc;
					}
				} else {
					// Hu oh… Print a giant warning, and abort.
					LOG(LOG_CRIT, "Could not run fuser script!");
					print_icon("\uf06a", &ctx);
					rc = print_msg(
					    // @translators: First unicode codepoint is an icon, leave it as-is.
					    _("\uf071 Could not run the fuser script!"),
					    &ctx);
					ctx.msg_cfg.margins.top = (short int) rc;
				}

				// We can still exit safely at that point
				print_msg(_("Press the power button to exit."), &ctx);
				need_early_abort = true;
				break;
			} else {
				PFLOG(LOG_CRIT, "umount2(\"%s\"): %m", mount_points[i].mountpoint);
				rv = USBMS_EARLY_EXIT;
				goto cleanup;
			}
		} else {
			// NOTE: This should never really happen…
			LOG(LOG_WARNING,
			    "%s storage partition has been unmounted early: it wasn't busy, and it was already marked as expired?!",
			    mount_points[i].name);
			early_unmount = true;
		}
	}

	// If we need an early abort because of USBNet/USBSerial or a busy mountpoint, do it now…
	if (need_early_abort) {
		LOG(LOG_INFO, "Waiting for a power button press…");
		struct pollfd pfds[2] = { 0 };
		nfds_t        nfds    = 2;
		// Input device
		pfds[0].fd            = evfd;
		pfds[0].events        = POLLIN;
		// Clock
		pfds[1].fd            = clockfd;
		pfds[1].events        = POLLIN;

		// Keep track of the time we've been polling
		struct timespec start_ts = { 0 };
		clock_gettime(CLOCK_MONOTONIC_RAW, &start_ts);

		while (true) {
			int poll_num = poll(pfds, nfds, 5 * 1000);

			if (poll_num == -1) {
				if (errno == EINTR) {
					continue;
				}
				PFLOG(LOG_CRIT, "poll: %m");
				rv = early_unmount ? EXIT_FAILURE : USBMS_EARLY_EXIT;
				goto cleanup;
			}

			if (poll_num > 0) {
				// Power button
				if (pfds[0].revents & POLLIN) {
					if (handle_evdev(dev)) {
						// Refresh the status bar
						print_status(&ctx);
						LOG(LOG_NOTICE, "Caught a power button release");
						if (early_unmount) {
							print_msg(
							    // @translators: First unicode codepoint is an icon, leave it as-is.
							    _("\uf071 The device will shut down in 30 sec."),
							    &ctx);
						} else {
							print_msg(
							    // @translators: First unicode codepoint is an icon, leave it as-is.
							    _("\uf05a KOReader will now restart…"),
							    &ctx);
						}
						(*fxpWaitForUpdateComplete)(ctx.fbfd, LAST_MARKER);
						break;
					}
				}

				// Clock
				if (pfds[1].revents & POLLIN) {
					// Refresh the status bar
					print_status(&ctx);
					// We don't actually care about the expiration count, so just read to clear the event
					uint64_t exp;
					read(clockfd, &exp, sizeof(exp));
				}
			}

			bool            done    = false;
			struct timespec poll_ts = { 0 };
			clock_gettime(CLOCK_MONOTONIC_RAW, &poll_ts);
			if (elapsed_time(&poll_ts, &start_ts) >= 30) {
				// We've been polling for more than 30 sec, we're done
				done = true;
			}

			// Give up afer 30 sec
			if (done) {
				LOG(LOG_NOTICE, "It's been 30 sec, giving up");
				if (early_unmount) {
					print_msg(
					    // @translators: First unicode codepoint is an icon, leave it as-is.
					    _("\uf05a Gave up after 30 sec.\nThe device will shut down in 30 sec."),
					    &ctx);
				} else {
					print_msg(
					    // @translators: First unicode codepoint is an icon, leave it as-is.
					    _("\uf05a Gave up after 30 sec.\nKOReader will now restart…"),
					    &ctx);
				}
				// Make sure this message will be visible…
				(*fxpWaitForUpdateComplete)(ctx.fbfd, LAST_MARKER);
				const struct timespec zzz = { 2L, 500000000L };
				nanosleep(&zzz, NULL);
				break;
			}
		}

		// NOTE: Not a hard failure, we can (usually) safely go back to whatever we were doing before.
		rv = early_unmount ? EXIT_FAILURE : USBMS_EARLY_EXIT;
		goto cleanup;
	}

	LOG(LOG_INFO, "Starting USBMS shenanigans");
	bool sleep_on_abort = true;
	// If we're not plugged in, wait for it (or abort early)
	usb_plugged         = (*fxpIsUSBPlugged)(ctx.ntxfd, true);
	// Check the standalone USB-C controller, too...
	int usb_c_plugged   = is_usbc_plugged(true);
	while (!usb_plugged) {
		print_msg(_("Waiting to be plugged in…\nOr, press the power button to exit."), &ctx);

		LOG(LOG_INFO, "Waiting for a plug in event or a power button press…");
		struct pollfd pfds[4] = { 0 };
		nfds_t        nfds    = 4;
		// Input device
		pfds[0].fd            = evfd;
		pfds[0].events        = POLLIN;
		// Uevent socket
		pfds[1].fd            = listener.pfd.fd;
		pfds[1].events        = listener.pfd.events;
		// USB-C input device (optional)
		pfds[2].fd            = usbc_fd;
		pfds[2].events        = POLLIN;
		// Clock
		pfds[3].fd            = clockfd;
		pfds[3].events        = POLLIN;

		// Keep track of the time we've been polling
		struct timespec start_ts = { 0 };
		clock_gettime(CLOCK_MONOTONIC_RAW, &start_ts);
		time_t time_spent_polling = 0;
		print_countdown(60, &ctx);

		struct uevent uev;
		while (true) {
			int poll_num = poll(pfds, nfds, 1000);

			if (poll_num == -1) {
				if (errno == EINTR) {
					continue;
				}
				PFLOG(LOG_CRIT, "poll: %m");
				rv = early_unmount ? EXIT_FAILURE : USBMS_EARLY_EXIT;
				goto cleanup;
			}

			if (poll_num > 0) {
				// Power button
				if (pfds[0].revents & POLLIN) {
					if (handle_evdev(dev)) {
						// Refresh the status bar
						print_status(&ctx);
						LOG(LOG_NOTICE, "Caught a power button release");
						// Clear the countdown, it may be halfway inside msg's margins
						clear_countdown(&ctx);
						if (early_unmount) {
							print_msg(
							    // @translators: First unicode codepoint is an icon, leave it as-is.
							    _("\uf071 The device will shut down in 30 sec."),
							    &ctx);
						} else {
							print_msg(
							    // @translators: First unicode codepoint is an icon, leave it as-is.
							    _("\uf05a KOReader will now restart…"),
							    &ctx);
						}
						need_early_abort = true;
						// That's a direct user interaction with an expected result, don't dawdle.
						sleep_on_abort   = false;
						break;
					}
				}

				// Uevent
				if (pfds[1].revents & POLLIN) {
					int ue_rc = handle_uevent(&listener, &uev);
					if (ue_rc == EXIT_SUCCESS) {
						// Now check if it's a plug in…
						if (uev.action == UEVENT_ACTION_ADD && uev.devpath &&
						    UE_STR_EQ(uev.devpath, KOBO_USB_DEVPATH_PLUG)) {
							// Refresh the status bar
							print_status(&ctx);
							LOG(LOG_WARNING,
							    "Caught a plug in event, but to a plain power source, not a USB host");
							// Clear the countdown, it may be halfway inside msg's margins
							clear_countdown(&ctx);
							if (early_unmount) {
								print_msg(
								    // @translators: First unicode codepoint is an icon, leave it as-is.
								    _("\uf071 The device was plugged into a plain power source, not a USB host!\nThe device will shut down in 30 sec."),
								    &ctx);
							} else {
								print_msg(
								    // @translators: First unicode codepoint is an icon, leave it as-is.
								    _("\uf071 The device was plugged into a plain power source, not a USB host!\nKOReader will now restart…"),
								    &ctx);
							}
							need_early_abort = true;
							break;
						} else if (uev.action == UEVENT_ACTION_ADD && uev.devpath &&
							   UE_STR_EQ(uev.devpath, KOBO_USB_DEVPATH_HOST)) {
							// Refresh the status bar
							print_status(&ctx);
							LOG(LOG_NOTICE, "Caught a plug in event (to a USB host)");
							break;
						} else if (uev.action == UEVENT_ACTION_CHANGE && uev.subsystem &&
							   UE_STR_EQ(uev.subsystem, "power_supply")) {
							// Refresh the status bar
							print_status(&ctx);
							// NOTE: Any meaningful change *should* be accompanied by the relevant usb_host/usb_plug event,
							//       this one is just for the status bar's sake.
							LOG(LOG_NOTICE, "Caught a discharge tick");
						}
					} else if (ue_rc == ERR_LISTENER_RECV) {
						// Assume handle_uevent read failures to be fatal
						rv = early_unmount ? EXIT_FAILURE : USBMS_EARLY_EXIT;
						goto cleanup;
					}
				}

				// Standalone USB-C controller
				if (pfds[2].revents & POLLIN) {
					// I don't trust this very much (even in optimal conditions, it *will* fire multiple times),
					// but we've seen some weird behavior on some host/device combos,
					// so, if that's the best we have at the end of the timeout, let the charger type detection figure it out...
					usb_c_plugged = handle_usbc_evdev(usbc_dev);
				}

				// Clock
				if (pfds[3].revents & POLLIN) {
					// Refresh the status bar
					print_status(&ctx);
					// We don't actually care about the expiration count, so just read to clear the event
					uint64_t exp;
					read(clockfd, &exp, sizeof(exp));
				}
			}

			bool            done    = false;
			struct timespec poll_ts = { 0 };
			clock_gettime(CLOCK_MONOTONIC_RAW, &poll_ts);
			time_t t = elapsed_time(&poll_ts, &start_ts);
			if (t != time_spent_polling) {
				// Refresh countdown
				time_t left = MAX(0, 60 - t);
				print_countdown(left, &ctx);
			}
			time_spent_polling = t;
			if (time_spent_polling >= 60) {
				// We've been polling for more than 60 sec, we're done
				done = true;

				// Despite the lack of plug in event, check what the kernel thinks the current situation is...
				usb_plugged = (*fxpIsUSBPlugged)(ctx.ntxfd, true);
				if (usb_plugged && CHARGER_TYPE_SYSFS) {
					// And in case it now looks plugged in, and we can verify that via a charger type check, keep going...
					LOG(LOG_WARNING,
					    "It's been 60 sec, and we failed to detect a proper plug in event, but the PMIC thinks we might be plugged in…");
					break;
				}

				// NOTE: In the same vein, on janky devices with a standalone USB-C controller,
				//       if we failed to detect a proper plug in event,
				//       but said controller thinks there's something at the other end of the cable,
				//       go ahead and let the charger type detection figure things out...
				//       c.f., https://github.com/koreader/koreader/issues/12128
				if (usb_c_plugged == 1 && CHARGER_TYPE_SYSFS) {
					// All the devices with said controller *should* support charger type detection, but let's be thorough...
					LOG(LOG_WARNING,
					    "It's been 60 sec, and we failed to detect a proper plug in event, but the USB-C controller thinks there's something at the other end of the cable…");
					break;
				}
			}

			// Give up afer 60 sec
			if (done) {
				LOG(LOG_NOTICE, "It's been 60 sec, giving up");
				// Clear the countdown, it may be halfway inside msg's margins
				clear_countdown(&ctx);
				if (early_unmount) {
					print_msg(
					    // @translators: First unicode codepoint is an icon, leave it as-is.
					    _("\uf05a Gave up after 60 sec.\nThe device will shut down in 30 sec."),
					    &ctx);
				} else {
					print_msg(
					    // @translators: First unicode codepoint is an icon, leave it as-is.
					    _("\uf05a Gave up after 60 sec.\nKOReader will now restart…"),
					    &ctx);
				}
				need_early_abort = true;
				break;
			}
		}

		// Double-check what the USB-C controller thinks is going on...
		is_usbc_plugged(true);

		// If we abort before plug in, we can (usually) still exit safely…
		if (need_early_abort) {
			// Make sure the final message will be visible…
			(*fxpWaitForUpdateComplete)(ctx.fbfd, LAST_MARKER);
			if (sleep_on_abort) {
				const struct timespec zzz = { 2L, 500000000L };
				nanosleep(&zzz, NULL);
			}
			rv = early_unmount ? EXIT_FAILURE : USBMS_EARLY_EXIT;
			goto cleanup;
		}

		// We've supposedly been plugged to a USB host...
		print_icon("\U000f051f", &ctx);
		// Let things settle for a while, and actually double-check that...
		// NOTE: We're being way more defensive than UIManager:_beforeCharging here,
		//       because we have less granularity and precision than drivers/input/misc/usb_plug.c...
		// NOTE: We're currently double-checking usb_host add events, but *NOT* usb_plug add events!
		//       So far, this hasn't been an issue, but do note that, on plug,
		//       the kernel ought to have better accuracy than the charger type check we'll do later...
		//       (Specifically, right now, it makes the right decision if the charger type is SDP OVRLIM,
		//       while that info is lost in the sysfs attribute).
		const struct timespec zzz = { 2L, 0L };
		nanosleep(&zzz, NULL);
		usb_plugged = (*fxpIsUSBPlugged)(ctx.ntxfd, true);
		if (usb_plugged) {
			// And that's our exit condition for the loop we're in
			LOG(LOG_NOTICE, "Device is now plugged in");
		} else {
			// That's... not good? :D. Back to another round of polling!
			LOG(LOG_WARNING,
			    "Device appears to still be unplugged despite the plug in event! Going back to square one.");
		}
		print_icon(usb_plugged ? "\U000f0201" : "\U000f0202", &ctx);
	}

	// NOTE: usb_plugged will be true if usbms was started while *already* plugged in,
	//       even if it's to a plain power source, and not a USB host…
	//       On some devices, POWER_SUPPLY_PROP_ONLINE is smart enough to be able to tell the difference,
	//       which means we can read it from sysfs (c.f., ricoh61x_batt_get_prop @ drivers/power/ricoh619-battery.c),
	//       but on older devices, it isn't, and the discrimination is *only* done during the plug in event…
	//       (c.f., drivers/input/misc/usb_plug.c).
	//       So, do what we can here, otherwise, the state may need to be tracked by the frontend,
	//       assuming it *also* got a chance to catch the event: i.e., it started *before* the plug in…
	// NOTE: Regardless, we want to double-check the charger type in *every* scenario...
	// NOTE: Unfortunately, the only platforms where we can do that appears to be Mk. 7+…
	if (CHARGER_TYPE_SYSFS) {
		LOG(LOG_INFO, "Checking charger type");
		FILE* f = fopen(CHARGER_TYPE_SYSFS, "re");
		if (f) {
			char   charger_type[16] = { 0 };
			size_t size = fread(charger_type, sizeof(*charger_type), sizeof(charger_type) - 1U, f);
			fclose(f);
			if (size > 0) {
				// Strip trailing LF
				if (charger_type[size - 1U] == '\n') {
					charger_type[size - 1U] = '\0';
				}
			} else {
				LOG(LOG_WARNING, "Could not read the charger type from sysfs!");
			}

			// c.f., charger_type_read @ drivers/power/ricoh619-battery.c
			if (strncmp(charger_type, "CDP", 3U) == 0U) {
				LOG(LOG_WARNING, "CDP (Charging Downstream Port) charger detected");
			} else if (strncmp(charger_type, "DCP", 3U) == 0U) {
				LOG(LOG_WARNING, "DCP (Dedicated Charging Port) charger detected");
				need_early_abort = true;
			} else if ((strncmp(charger_type, "SDP_PC", 6U) == 0U) || (strcmp(charger_type, "SDP") == 0U)) {
				// NOTE: The bd71827 PMIC found on some devices only reports "SDP" or "DCP"
				LOG(LOG_INFO, "SDP PC (Standard Downstream Port, 500mA) charger detected");
			} else if (strncmp(charger_type, "SDP_ADPT", 8U) == 0U) {
				LOG(LOG_WARNING, "SDP ADPT (Standard Downstream Port, 800mA) charger detected");
				// NOTE: ricoh619_charger_detect_ex @ drivers/mfd/ricoh619.c currently never returns that.
				need_early_abort = true;
			} else if (strncmp(charger_type, "SDP_OVRLIM", 8U) == 0U) {
				// NOTE: While it is supported in ricoh619_charger_detect,
				//       this will currently never make it to the sysfs attribute file,
				//       as it's currently commented out
				//       (c.f., charger_type_read @ drivers/power/supply/ricoh619-battery.c).
				LOG(LOG_WARNING, "SDP OVRLIM (Standard Downstream Port, > 500mA) charger detected");
			} else if (strncmp(charger_type, "NO", 2U) == 0U) {
				// NOTE: Is actually "NONE" on bd71827, where it possibly means unplugged?
				// NOTE: Despite being in a usb_plugged branch,
				//       this *may* happen if the device is fully charged.
				//       In which case,
				//       /sys/class/power_supply/mc13892_bat/status will *also* say "Not charging".
				//       I'm not double-checking capacity here, but it ought to be 100 in these cases ;).
				//       Thankfully, that's only true when connected to a SDP, which suits us just fine.
				//       If you hit 100% while connected to a DCP, while the rest of this note holds true,
				//       the charger type keeps saying DCP :).
				LOG(LOG_INFO, "No charger detected! Fully charged?");
			} else if (strncmp(charger_type, "DISABLE", 7U) == 0U) {
				// NOTE: We might want to let this one through?
				LOG(LOG_WARNING, "Charger is disabled!");
				need_early_abort = true;
			} else {
				LOG(LOG_ERR,
				    "Unknown charger type (`%.*s`)!",
				    (int) (sizeof(charger_type) - 1U),
				    charger_type);
				need_early_abort = true;
			}

			// NOTE: A CDP can enumerate. On older devices (<= Mk. 7),
			//       the discrimination between usb_plug and usb_host was only based on detecting an SDP PC
			//       in drivers/input/misc/usb_plug.c
			//       (c.f., ricoh619_charger_detect @ drivers/mfd/ricoh619.c).
			//       This changed on the Elipsa, which started allowing SDP OVRLIM,
			//       and on the Sage, which added CDP to the list (the Libra 2 followed suit).
			//       We do the most sensible thing here (i.e., continue if the port can enumerate),
			//       by following the latest code ;).
			// NOTE: SDP_CHARGER == SDP_PC_CHARGER != SDP_ADPT_CHARGER
			//       c.f., include/linux/power/ricoh619_battery.h
			if (need_early_abort) {
				LOG(LOG_ERR, "Charger type cannot enumerate, aborting");
				if (early_unmount) {
					print_msg(
					    // @translators: First unicode codepoint is an icon, leave it as-is.
					    _("\uf071 The device is plugged into a plain power source, not a USB host!\nThe device will shut down in 30 sec."),
					    &ctx);
				} else {
					print_msg(
					    // @translators: First unicode codepoint is an icon, leave it as-is.
					    _("\uf071 The device is plugged into a plain power source, not a USB host!\nKOReader will now restart…"),
					    &ctx);
				}

				// We still haven't switched to USBMS, so we can (usually) exit safely…
				// Make sure the final message will be visible…
				(*fxpWaitForUpdateComplete)(ctx.fbfd, LAST_MARKER);
				const struct timespec zzz = { 2L, 500000000L };
				nanosleep(&zzz, NULL);
				rv = early_unmount ? EXIT_FAILURE : USBMS_EARLY_EXIT;
				goto cleanup;
			}
		} else {
			// That... shouldn't really ever happen ;).
			LOG(LOG_WARNING, "Could not open the sysfs entry for charger type (%m)!");
		}
	}

	// We're plugged in, here comes the fun…
	LOG(LOG_INFO, "Starting USBMS session…");
	print_icon("\uf287", &ctx);
	print_msg(_("Starting USBMS session…"), &ctx);

	// NOTE: We need to figure out the frontlight intensity *before* we unmount onboard,
	//       because, on < Mk. 7 devices, we'll have to get that from KOReader's config file…
	uint8_t fl_intensity = get_frontlight_intensity();
	LOG(LOG_INFO, "Frontlight intensity is currently set to %hhu%%", fl_intensity);

	// Here goes nothing…
	snprintf(resource_path,
		 sizeof(resource_path) - 1U,
		 "%s/scripts/start-usbms.sh >/usr/local/KoboUSBMS.log 2>&1",
		 abs_pwd);
	rc = system(resource_path);
	if (rc != EXIT_SUCCESS) {
		// Hu oh… Print a giant warning, and abort. KOReader will shut down the device after a while.
		if (rc == -1) {
			LOG(LOG_CRIT, "Could not start the USBMS session (system: %m)!");
		} else {
			if (WIFEXITED(rc)) {
				LOG(LOG_CRIT,
				    "Could not start the USBMS session (script exited with status %d)!",
				    WEXITSTATUS(rc));
			} else if (WIFSIGNALED(rc)) {
				LOG(LOG_CRIT,
				    "Could not start the USBMS session (script was terminated by signal %s)!",
				    strsignal(WTERMSIG(rc)));
			}
			LOG(LOG_DEBUG, "Check `/usr/local/KoboUSBMS.log` for more details");
		}
		print_icon("\uf06a", &ctx);
		print_msg(
		    // @translators: First unicode codepoint is an icon, leave it as-is.
		    _("\uf071 Could not start the USBMS session!\nThe device will shut down in 90 sec."),
		    &ctx);

		rv = EXIT_FAILURE;
		goto cleanup;
	}

	// Now we're cooking with gas!
	LOG(LOG_INFO, "USBMS session in progress");
	// Switch to nightmode for the duration of the session, as a nod to the stock behavior ;).
	ctx.fbink_cfg.no_refresh = true;
	if (ctx.fbink_state.can_hw_invert) {
		ctx.fbink_cfg.is_nightmode = true;
	} else {
		// Fake it ;).
		ctx.fbink_cfg.is_inverted = true;
		fbink_invert_screen(ctx.fbfd, &ctx.fbink_cfg);
	}
	print_msg(_("USBMS session in progress.\nPlease eject your device safely before unplugging it."), &ctx);
	ctx.fbink_cfg.no_refresh = false;
	fbink_refresh(ctx.fbfd, 0, 0, 0, 0, &ctx.fbink_cfg);

	// And much like Nickel, gently turn the light off for the duration…
	if (fl_intensity != 0U) {
		LOG(LOG_INFO, "Turning frontlight off…");
		toggle_frontlight(false, fl_intensity, ctx.ntxfd);
	}

	// And now we just have to wait until an unplug…
	LOG(LOG_INFO, "Waiting for an eject or unplug event…");
	struct pollfd pfds[3] = { 0 };
	nfds_t        nfds    = 3;
	// Uevent socket
	pfds[0].fd            = listener.pfd.fd;
	pfds[0].events        = listener.pfd.events;
	// USB-C input device (optional)
	pfds[1].fd            = usbc_fd;
	pfds[1].events        = POLLIN;
	// Clock
	pfds[2].fd            = clockfd;
	pfds[2].events        = POLLIN;

	struct uevent uev;
	// NOTE: This is basically ue_wait_for_event, but with an extra polling on our clock timerfd,
	//       solely for the purpose of refreshing the status bar,
	//       because we don't necessarily get change events on power_supply on older devices
	//       (e.g., it happens on Mk. 7, but not on Mk. 5)…
	while (true) {
		int poll_num = poll(pfds, nfds, -1);

		if (poll_num == -1) {
			if (errno == EINTR) {
				continue;
			}
			PFLOG(LOG_CRIT, "poll: %m");
			rv = EXIT_FAILURE;
			goto cleanup;
		}

		if (poll_num > 0) {
			// Uevent
			if (pfds[0].revents & POLLIN) {
				int ue_rc = handle_uevent(&listener, &uev);
				if (ue_rc == EXIT_SUCCESS) {
					// Now check if it's an eject or an unplug…
					if (uev.action == UEVENT_ACTION_OFFLINE && uev.devpath &&
					    (UE_STR_EQ(uev.devpath, KOBO_USB_DEVPATH_FSL) ||
					     (uev.modalias && UE_STR_EQ(uev.modalias, KOBO_USB_MODALIAS_CI)) ||
					     UE_STR_EQ(uev.devpath, KOBO_USB_DEVPATH_UDC) ||
					     UE_STR_EQ(uev.devpath, KOBO_USB_DEVPATH_MTK))) {
						// Refresh the status bar
						print_status(&ctx);
						LOG(LOG_NOTICE, "Caught an eject event");
						break;
					} else if (uev.action == UEVENT_ACTION_REMOVE && uev.devpath &&
						   (UE_STR_EQ(uev.devpath, KOBO_USB_DEVPATH_PLUG) ||
						    UE_STR_EQ(uev.devpath, KOBO_USB_DEVPATH_HOST))) {
						// Refresh the status bar
						print_status(&ctx);
						LOG(LOG_NOTICE, "Caught an unplug event");
						break;
					} else if (uev.action == UEVENT_ACTION_CHANGE && uev.subsystem &&
						   UE_STR_EQ(uev.subsystem, "power_supply")) {
						// Refresh the status bar
						print_status(&ctx);
						LOG(LOG_NOTICE, "Caught a charge tick");
					}
				} else if (ue_rc == ERR_LISTENER_RECV) {
					// Assume handle_uevent read failures to be fatal
					rv = EXIT_FAILURE;
					goto cleanup;
				}
			}

			// Standalone USB-C controller
			if (pfds[1].revents & POLLIN) {
				handle_usbc_evdev(usbc_dev);
				// NOTE: Unlike with the plug in detection, this one is purely informal,
				//       I don't intend to *ever* trust it over uevent for anything...
			}

			// Clock
			if (pfds[2].revents & POLLIN) {
				// Refresh the status bar
				print_status(&ctx);
				// We don't actually care about the expiration count, so just read to clear the event
				uint64_t exp;
				read(clockfd, &exp, sizeof(exp));
			}
		}
	}
	// Remember the eject timestamp
	struct timespec eject_ts = { 0 };
	clock_gettime(CLOCK_REALTIME, &eject_ts);

	if (ctx.fbink_state.can_hw_invert) {
		ctx.fbink_cfg.is_nightmode = false;
		fbink_refresh(ctx.fbfd, 0, 0, 0, 0, &ctx.fbink_cfg);
	} else {
		ctx.fbink_cfg.is_inverted = false;
		fbink_invert_screen(ctx.fbfd, &ctx.fbink_cfg);
	}

	// Turn frontlight back on
	if (fl_intensity != 0U) {
		LOG(LOG_INFO, "Turning frontlight back on…");
		toggle_frontlight(true, fl_intensity, ctx.ntxfd);
	}

	// If ue_wait_for_event failed for some reason, abort with extreme prejudice…
	if (rc != EXIT_SUCCESS) {
		LOG(LOG_CRIT, "Could not detect an unlug event!");
		print_icon("\uf06a", &ctx);
		print_msg(
		    // @translators: First unicode codepoint is an icon, leave it as-is.
		    _("\uf071 Could not detect an unplug event!\nThe device will shut down in 90 sec."),
		    &ctx);

		rv = EXIT_FAILURE;
		goto cleanup;
	}

	// And now remount all the things!
	LOG(LOG_INFO, "Ending USBMS session…");
	print_icon("\U000f0553", &ctx);
	print_msg(_("Ending USBMS session…"), &ctx);

	// Nearly there…
	snprintf(
	    resource_path, sizeof(resource_path) - 1U, "%s/scripts/end-usbms.sh >/usr/local/KoboUSBMS.log 2>&1", abs_pwd);
	rc = system(resource_path);
	if (rc != EXIT_SUCCESS) {
		// Hu oh… Print a giant warning, and abort. KOReader will shut down the device after a while.
		if (rc == -1) {
			LOG(LOG_CRIT, "Could not end the USBMS session (system: %m)!");
		} else {
			if (WIFEXITED(rc)) {
				LOG(LOG_CRIT,
				    "Could not end the USBMS session (script exited with status %d)!",
				    WEXITSTATUS(rc));
			} else if (WIFSIGNALED(rc)) {
				LOG(LOG_CRIT,
				    "Could not end the USBMS session (script was terminated by signal %s)!",
				    strsignal(WTERMSIG(rc)));
			}
			LOG(LOG_DEBUG, "Check `/usr/local/KoboUSBMS.log` for more details");
		}
		print_icon("\uf06a", &ctx);
		print_msg(
		    // @translators: First Unicode codepoint is an icon, leave it as-is.
		    _("\uf071 Could not end the USBMS session!\nThe device will shut down in 90 sec."),
		    &ctx);

		rv = EXIT_FAILURE;
		goto cleanup;
	}

	// Handle date/time synchronization, like Nickel
	// c.f., https://www.mobileread.com/forums/showpost.php?p=4064358&postcount=9
	if (access(KOBO_TZ_FILE, F_OK) == 0) {
		LOG(LOG_INFO, "Checking timezone synchronization file…");
		FILE* f = fopen(KOBO_TZ_FILE, "re");
		if (f) {
			char   tzname[_POSIX_PATH_MAX * 2U] = { 0 };
			size_t size                         = fread(tzname, sizeof(*tzname), sizeof(tzname) - 1U, f);
			fclose(f);
			if (size > 0) {
				// Strip trailing LF, not that the Kobo app actually adds one ;).
				if (tzname[size - 1U] == '\n') {
					tzname[size - 1U] = '\0';
				}
			} else {
				LOG(LOG_WARNING, "Could not read timezone.conf!");
			}

			// Replace all occurences of a space by an underscore
			for (char* p = tzname; (p = strchr(p, ' ')) != NULL; *p = '_') {
				;
			}

			// Start by checking if we actually *can* use it…
			bool tz_available = false;
			snprintf(resource_path, sizeof(resource_path) - 1U, SYSTEM_TZPATH "/%s", tzname);
			if (access(resource_path, F_OK) != 0) {
				tz_available = false;
				// Try with the few extra symlinks Kobo maintains…
				snprintf(resource_path, sizeof(resource_path) - 1U, KOBO_TZPATH "/%s", tzname);
				if (access(resource_path, F_OK) != 0) {
					tz_available = false;
					LOG(LOG_WARNING, "Cannot use the timezone from timezone.conf: `%s`", tzname);
				} else {
					tz_available = true;
				}
			} else {
				tz_available = true;
			}
			if (tz_available) {
				// Then it's as easy as a symlink ;)
				unlink(SYSTEM_TZFILE);
				if (symlink(resource_path, SYSTEM_TZFILE) == -1) {
					LOG(LOG_WARNING, "Could not symlink the zoneinfo file for `%s`: %m", tzname);
					// Fallback to US/NYC
					symlink(SYSTEM_TZPATH "/America/New_York", SYSTEM_TZFILE);
					LOG(LOG_INFO, "Reset timezone to America/New_York");
				} else {
					LOG(LOG_INFO, "Updated timezone to: %s", tzname);
				}
			}
		}

		// We're done, get rid of it ;)
		unlink(KOBO_TZ_FILE);
	}

	if (access(KOBO_EPOCH_TS, F_OK) == 0) {
		LOG(LOG_INFO, "Checking date/time synchronization file…");
		FILE* f = fopen(KOBO_EPOCH_TS, "re");
		if (f) {
			char   epoch[32] = { 0 };
			size_t size      = fread(epoch, sizeof(*epoch), sizeof(epoch) - 1U, f);
			fclose(f);
			if (size > 0) {
				// Strip trailing LF, not that the Kobo app actually adds one ;).
				if (epoch[size - 1U] == '\n') {
					epoch[size - 1U] = '\0';
				}
			} else {
				LOG(LOG_WARNING, "Could not read epoch.conf!");
			}

			// c.f., busybox's date & hwclock applets
			tzset();
			struct timespec ts = { 0 };
			clock_gettime(CLOCK_REALTIME, &ts);

			// Compute the elapsed time since the actual eject
			time_t elapsed_sec = elapsed_time(&ts, &eject_ts);

			// Seed the timespec with the current time_t ts
			struct tm tm_time = { 0 };
			localtime_r(&ts.tv_sec, &tm_time);
			// We're on glibc, let strptime deal with it (barring that, strtol/stroll would do)
			if (strptime(epoch, "%s", &tm_time) == NULL) {
				LOG(LOG_WARNING, "Could not parse epoch.conf data: `%s`", epoch);
			} else {
				// Make a time_t out of the updated timespec
				time_t t = mktime(&tm_time);
				if (t == (time_t) -1L) {
					LOG(LOG_WARNING, "Invalid date from epoch.conf: `%s`", epoch);
				} else {
					// Account for the time difference between the eject,
					// (which is roughly when the ts was stored by the app),
					// and the moment when we actually started reading it,
					// because we're guaranteed to have lost a few seconds to fsck…
					ts.tv_sec  = t + elapsed_sec;
					ts.tv_nsec = 0;
					// And, finally, update the system clock
					clock_settime(CLOCK_REALTIME, &ts);

					// Update the rtc, too (which is in UTC)
					gmtime_r(&ts.tv_sec, &tm_time);
					int rtc = open("/dev/rtc0", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
					if (rtc == -1) {
						LOG(LOG_WARNING, "Could not open RTC: %m");
					} else {
						tm_time.tm_isdst = 0;
						if (ioctl(rtc, RTC_SET_TIME, &tm_time) == -1) {
							LOG(LOG_WARNING, "Could not set RTC time: %m");
						}
						close(rtc);
					}
					LOG(LOG_INFO,
					    "Updated date/time to epoch: %s (+ %jd)",
					    epoch,
					    (intmax_t) elapsed_sec);
				}
			}
		}

		// We're done, remove it to prevent Nickel from doing something stupid with it later ;).
		unlink(KOBO_EPOCH_TS);
	}

	// Whee!
	LOG(LOG_INFO, "Done :)");
	// NOTE: We batch the final screen, make it flash, and wait for completion of the refresh,
	//       all in order to make sure we won't lose a race with the refresh induced by KOReader's restart…
	//       (i.e., don't let it get optimized out by the EPDC).
	ctx.fbink_cfg.no_refresh = true;
	print_icon("\uf058", &ctx);
	print_msg(_("Done!\nKOReader will now restart…"), &ctx);
	// Refresh the status bar one last time
	print_status(&ctx);
	ctx.fbink_cfg.no_refresh  = false;
	ctx.fbink_cfg.is_flashing = true;
	fbink_refresh(ctx.fbfd, 0, 0, 0, 0, &ctx.fbink_cfg);
	ctx.fbink_cfg.is_flashing = false;
	(*fxpWaitForUpdateComplete)(ctx.fbfd, LAST_MARKER);

cleanup:
	LOG(LOG_INFO, "Bye!");

	fbink_free_ot_fonts_v2(&ctx.icon_cfg);
	if (is_CJK) {
		fbink_free_ot_fonts_v2(&ctx.msg_cfg);
		ctx.ot_cfg.font = NULL;
	} else {
		// We share the same font everywhere, so just avoid dangling pointers
		ctx.msg_cfg.font = NULL;
		ctx.ot_cfg.font  = NULL;
	}
	fbink_close(ctx.fbfd);

	ue_destroy_listener(&listener);

	libevdev_free(dev);
	if (evfd != -1) {
		close(evfd);
	}
	free(NTX_KEYS_EVDEV);
	libevdev_free(usbc_dev);
	if (usbc_fd != -1) {
		close(usbc_fd);
	}
	free(USBC_EVDEV);
	free(USBC_PLUG_SYSFS);

	if (ctx.ntxfd != -1) {
		close(ctx.ntxfd);
	}

	if (clockfd != -1) {
		close(clockfd);
	}

	if (pwd != -1) {
		if (fchdir(pwd) == -1) {
			// NOTE: That would be bad if we were launched from within the internal storage
			//       (i.e., that'd be a hint that it probably failed to remount properly).
			//       Which is why you should start this from within a tmpfs, like KOReader ;).
			PFLOG(LOG_CRIT, "fchdir(pwd): %m");
			rv = EXIT_FAILURE;
		}
		close(pwd);
	}
	free(abs_pwd);

	closelog();

	return rv;
}
