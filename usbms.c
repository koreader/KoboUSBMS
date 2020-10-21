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

#include "usbms.h"

// Wrapper function to use syslog as a libevdev log handler
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
	syslog(LOG_INFO, "libevdev: %s @ %s:%d (prio: %u)", func, file, line, priority);
	vsyslog(LOG_INFO, format, args);
}

static void
    setup_usb_ids(unsigned short int device_code)
{
	// Map device IDs to USB Product IDs, as we're going to need that in the scripts
	// c.f., https://github.com/kovidgoyal/calibre/blob/9d881ed2fcff219887579571f1bb48bdf41437d4/src/calibre/devices/kobo/driver.py#L1402-L1416
	// c.f., https://github.com/baskerville/plato/blob/d96e40737060b569ae875f37d6d741fd5ccc802c/contrib/plato.sh#L38-L56
	// NOTE: Keep 'em in FBInk order to make my life easier

	uint32_t pid = 0xDEAD;
	switch (device_code) {
		case 310U:    // Touch A/B (trilogy)
			pid = 0x4163;
			break;
		case 320U:    // Touch C (trilogy)
			pid = 0x4163;
			break;
		case 340U:    // Mini (pixie)
			pid = 0x4183;
			break;
		case 330U:    // Glo (kraken)
			pid = 0x4173;
			break;
		case 371U:    // Glo HD (alyssum)
			pid = 0x4223;
			break;
		case 372U:    // Touch 2.0 (pika)
			pid = 0x4224;
			break;
		case 360U:    // Aura (phoenix)
			pid = 0x4203;
			break;
		case 350U:    // Aura HD (dragon)
			pid = 0x4193;
			break;
		case 370U:    // Aura H2O (dahlia)
			pid = 0x4213;
			break;
		case 374U:    // Aura H2O² (snow)
			pid = 0x4227;
			break;
		case 378U:    // Aura H2O² r2 (snow)
			pid = 0x4227;
			break;
		case 373U:    // Aura ONE (daylight)
			pid = 0x4225;
			break;
		case 381U:    // Aura ONE LE (daylight)
			pid = 0x4225;
			break;
		case 375U:    // Aura SE (star)
			pid = 0x4226;
			break;
		case 379U:    // Aura SE r2 (star)
			pid = 0x4226;
			break;
		case 376U:    // Clara HD (nova)
			pid = 0x4228;
			break;
		case 377U:    // Forma (frost)
			pid = 0x4229;
			break;
		case 380U:    // Forma 32GB (frost)
			pid = 0x4229;
			break;
		case 384U:    // Libra H2O (storm)
			pid = 0x4232;
			break;
		case 382U:    // Nia (luna)
			pid = 0x4230;
			break;
		case 0U:
			pid = 0x4163;
			break;
		default:
			PFLOG(LOG_WARNING, "Can't match device code (%hu) to an USB Product ID!", device_code);
			break;
	}

	// Push it to the env...
	char pid_str[8] = { 0 };
	snprintf(pid_str, sizeof(pid_str) - 1U, "0x%04X", pid);
	PFLOG(LOG_NOTICE, "USB product ID: %s", pid_str);
	setenv("USB_PRODUCT_ID", pid_str, 1);
}

static bool
    is_usb_plugged(int ntxfd)
{
	// Check if we're plugged in...
	unsigned long ptr = 0U;
	int           rc  = ioctl(ntxfd, CM_USB_Plug_IN, &ptr);
	if (rc == -1) {
		PFLOG(LOG_WARNING, "Failed to query USB status (ioctl: %m)");
	}
	return !!ptr;
}

// Return a fancy battery icon given the charge percentage...
static const char*
    get_battery_icon(uint8_t charge)
{
	if (charge >= 100) {
		return "\uf578";
	} else if (charge >= 90) {
		return "\uf581";
	} else if (charge >= 80) {
		return "\uf580";
	} else if (charge >= 70) {
		return "\uf57f";
	} else if (charge >= 60) {
		return "\uf57e";
	} else if (charge >= 50) {
		return "\uf57d";
	} else if (charge >= 40) {
		return "\uf57c";
	} else if (charge >= 30) {
		return "\uf57b";
	} else if (charge >= 20) {
		return "\uf57a";
	} else if (charge >= 10) {
		return "\uf579";
	} else {
		return "\uf582";
	}
}

// NOTE: Inspired from git's strtoul_ui @ git-compat-util.h
static int
    strtoul_hhu(const char* str, uint8_t* restrict result)
{
	// NOTE: We want to *reject* negative values (which strtoul does not)!
	if (strchr(str, '-')) {
		PFLOG(LOG_WARNING, "Passed a negative value (%s) to strtoul_hhu", str);
		return -EINVAL;
	}

	// Now that we know it's positive, we can go on with strtoul...
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
		PFLOG(LOG_WARNING, "No digits were found in value '%s' assigned to a variable expecting an uint8_t", str);
		return -EINVAL;
	}

	// If we got here, strtoul() successfully parsed at least part of a number.
	// But we do want to enforce the fact that the input really was *only* an integer value.
	if (*endptr != '\0') {
		PFLOG(
		    LOG_WARNING,
		    "Found trailing characters (%s) behind value '%lu' assigned from string '%s' to a variable expecting an uint8_t",
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
	for (; a < b && isspace((unsigned char) (*a)); a++)
		;
	for (; b > a && isspace((unsigned char) (*(b - 1))); b--)
		;
	*b = '\0';
	return a;
}

// Attempt to figure out the current frontlight intensity...
static uint8_t
    get_frontlight_intensity(void)
{
	// If all else fails, don't touch the FL by ensuring we return 0
	uint8_t intensity = 0U;

	// On Mk. 7, we can actually get it from sysfs, making our life far easier...
	FILE* f = fopen(FL_INTENSITY_SYSFS, "re");
	if (f) {
		char   fl_intensity[8] = { 0 };
		size_t size            = fread(fl_intensity, sizeof(*fl_intensity), sizeof(fl_intensity), f);
		if (size > 0) {
			// NUL terminate
			fl_intensity[size - 1U] = '\0';
			// Strip trailing LF
			if (fl_intensity[size - 2U] == '\n') {
				fl_intensity[size - 2U] = '\0';
			}
		}
		fclose(f);
		f = NULL;

		if (strtoul_hhu(fl_intensity, &intensity) < 0) {
			PFLOG(LOG_WARNING,
			      "Failed to convert sysfs frontlight intensity value '%s' to an uint8_t!",
			      fl_intensity);
		} else {
			// We're good, don't bother trying to parse KOReader's settings!
			return intensity;
		}
	}

	char* ko_dir = getenv("KOREADER_DIR");
	if (!ko_dir) {
		PFLOG(LOG_WARNING, "Unable to compute KOReader directory!");
		return intensity;
	}

	// Now, try to parse KOReader's settings...
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
					      "Failed to parse `is_frontline_on` in KOReader's settings (key)");
					continue;
				}

				char* setting_value = strsep(&cur_line, ",");
				if (!setting_value) {
					PFLOG(LOG_WARNING,
					      "Failed to parse `is_frontline_on` in KOReader's settings (value)");
					continue;
				}

				setting_value = strtrim(setting_value);

				if (strcmp(setting_value, "true") == 0) {
					found_state = true;
					fl_state    = true;
					PFLOG(LOG_INFO, "Frontlight was enabled in KOReader");
				} else if (strcmp(setting_value, "false") == 0) {
					found_state = true;
					fl_state    = false;
					PFLOG(LOG_INFO, "Frontlight was disabled in KOReader");
				} else {
					PFLOG(LOG_WARNING,
					      "Failed to parse `is_frontline_on` value! (`%s`)",
					      setting_value);
				}
			} else if (strstr(cur_line, "[\"frontlight_intensity\"]")) {
				char* setting_key = strsep(&cur_line, "=");
				if (!setting_key) {
					PFLOG(LOG_WARNING,
					      "Failed to parse `frontlight_intensity` in KOReader's settings (key)");
					continue;
				}

				char* setting_value = strsep(&cur_line, ",");
				if (!setting_value) {
					PFLOG(LOG_WARNING,
					      "Failed to parse `frontlight_intensity` in KOReader's settings (value)");
					continue;
				}

				setting_value = strtrim(setting_value);

				if (strtoul_hhu(setting_value, &fl_intensity) < 0) {
					PFLOG(LOG_WARNING,
					      "Failed to convert KOReader frontlight intensity value '%s' to an uint8_t!",
					      setting_value);
				} else {
					found_intensity = true;
					PFLOG(LOG_INFO, "Frontlight intensity was at %hhu%% in KOReader", fl_intensity);
				}
			}

			// If we've found & parsed both state & intensity, we're golden
			if (found_intensity && found_state) {
				// And if it was actually enabled, update the return value
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
				PFLOG(LOG_WARNING, "Failed to set frontlight intensity to %d%% (ioctl: %m)", ptr);
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
				PFLOG(LOG_WARNING, "Failed to set frontlight intensity to %d%% (ioctl: %m)", ptr);
			}
			if (i < STEPS) {
				nanosleep(&zzz, NULL);
			}
		}
	}
}

// We'll want to regularly update a display of the plug/charge status, and whether Wi-Fi is on or not
static void
    print_status(int fbfd, const FBInkConfig* fbink_cfg, const FBInkOTConfig* ot_cfg, int ntxfd)
{
	// Check if we're plugged in...
	bool usb_plugged = is_usb_plugged(ntxfd);

	// Get the battery charge %
	uint8_t batt_perc = 0U;
	FILE*   f         = fopen(BATT_CAP_SYSFS, "re");
	if (f) {
		char   batt_charge[8] = { 0 };
		size_t size           = fread(batt_charge, sizeof(*batt_charge), sizeof(batt_charge), f);
		if (size > 0) {
			// NUL terminate
			batt_charge[size - 1U] = '\0';
			// Strip trailing LF
			if (batt_charge[size - 2U] == '\n') {
				batt_charge[size - 2U] = '\0';
			}
		}
		fclose(f);

		if (strtoul_hhu(batt_charge, &batt_perc) < 0) {
			PFLOG(LOG_WARNING, "Failed to convert battery charge value '%s' to an uint8_t!", batt_charge);
		}
	}

	// Check for Wi-Fi status
	// (c.f., https://github.com/koreader/koreader/blob/b5d33058761625111d176123121bcc881864a64e/frontend/device/kobo/device.lua#L451-L471)
	bool wifi_up            = false;
	char if_sysfs[PATH_MAX] = { 0 };
	snprintf(if_sysfs, sizeof(if_sysfs) - 1U, "/sys/class/net/%s/carrier", getenv("INTERFACE"));
	f = fopen(if_sysfs, "re");
	if (f) {
		char   carrier[8];
		size_t size = fread(carrier, sizeof(*carrier), sizeof(carrier), f);
		if (size > 0) {
			// NUL terminate
			carrier[size - 1U] = '\0';
		}
		fclose(f);

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

	fbink_printf(fbfd,
		     ot_cfg,
		     fbink_cfg,
		     "%s • \uf017 %s • %s (%hhu%%) • %s",
		     usb_plugged ? "\ufba3" : "\ufba4",
		     sz_time,
		     get_battery_icon(batt_perc),
		     batt_perc,
		     wifi_up ? "\ufaa8" : "\ufaa9");
}

static void
    print_icon(int fbfd, const char* string, FBInkConfig* fbink_cfg, const FBInkOTConfig* ot_cfg)
{
	fbink_cfg->is_halfway = true;
	fbink_print_ot(fbfd, string, ot_cfg, fbink_cfg, NULL);
	fbink_cfg->is_halfway = false;
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

// Parse an uevent
static int
    handle_uevent(struct uevent_listener* l, struct uevent* uevp)
{
	ue_reset_event(uevp);
	ssize_t len = recv(l->pfd.fd, uevp->buf, sizeof(uevp->buf), MSG_DONTWAIT);
	if (len == -1) {
		PFLOG(LOG_CRIT, "recv: %m");
		return ERR_LISTENER_RECV;
	}
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
	int                    fbfd     = -1;
	FBInkOTConfig          ot_cfg   = { 0 };
	FBInkOTConfig          icon_cfg = { 0 };
	FBInkOTConfig          msg_cfg  = { 0 };
	struct uevent_listener listener = { 0 };
	listener.pfd.fd                 = -1;
	struct libevdev* dev            = NULL;
	int              evfd           = -1;
	int              ntxfd          = -1;

	// We'll be chatting exclusively over syslog, because duh.
	openlog("usbms", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);

	// Say hello
	LOG(LOG_INFO, "Initializing USBMS %s (%s)", USBMS_VERSION, USBMS_TIMESTAMP);

	// We'll want to jump to /, and only get back to our original PWD on exit...
	// c.f., man getcwd for the fchdir trick, as we can certainly spare the fd ;).
	// NOTE: O_PATH is Linux 2.6.39+ :(
	pwd = open(".", O_RDONLY | O_DIRECTORY | O_PATH | O_CLOEXEC);
	if (pwd == -1) {
		PFLOG(LOG_CRIT, "open: %m");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}
	// We do need the pathname to load resources, though...
	abs_pwd = get_current_dir_name();
	if (chdir("/") == -1) {
		PFLOG(LOG_CRIT, "chdir: %m");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}
	char resource_path[PATH_MAX] = { 0 };

	// NOTE: The font we ship only covers LGC scripts. Blacklist a few languages where we know it won't work,
	//       based on KOReader's own language list (c.f., frontend/ui/language.lua).
	//       Because English is better than the replacement character ;p.
	//       We do jump through a few hoops to attempt to salvage CJK support...
	const char* lang = getenv("LANGUAGE");
	if (lang) {
		if (strncmp(lang, "he", 2U) == 0 || strncmp(lang, "ar", 2U) == 0 || strncmp(lang, "fa", 2U) == 0) {
			LOG(LOG_NOTICE, "Your language (%s) is unsupported (RTL), falling back to English", lang);
			setenv("LANGUAGE", "C", 1);
		} else if (strncmp(lang, "bn", 2U) == 0) {
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
	//       Unfortunately, Kobo doesn't compile *any* locales...
	//       The minimal LC_* category we need for our translation is LC_MESSAGES.
	//       It requires SYS_LC_MESSAGES from the glibc (usually shipped in archive form on sane systems).
	//       So, we build one manually (via localedef) from the en_US definitions, and set-it up in a bogus custom locale,
	//       which we use for our LC_MESSAGES setlocale call.
	//       We of course ship it, and we enforce our own l10n directory as the global locale search path...
	//       Then, we can *finally* choose our translation language via the LANGUAGE env var...
	//       c.f., https://stackoverflow.com/q/36857863
	snprintf(resource_path, sizeof(resource_path) - 1U, "%s/l10n", abs_pwd);
	// We can't touch the rootfs, so, teach the glibc about our awful workaround...
	// (c.f., https://www.gnu.org/software/libc/manual/html_node/Locale-Names.html)
	setenv("LOCPATH", resource_path, 1);
	// Then, because gettext wants a setlocale call, let's give it one that's enough to get us translations,
	// and minimal enough that we don't have to ship a crazy amount of useless locale stuff...
	setlocale(LC_MESSAGES, "kobo");
	// And now stuff can more or less start looking like a classic gettext setup...
	bindtextdomain("usbms", resource_path);
	textdomain("usbms");
	// The whole locale shenanigan means more hand-holding is needed to enforce UTF-8...
	bind_textdomain_codeset("usbms", "UTF-8");

	// Setup FBInk
	FBInkConfig fbink_cfg = { 0 };
	fbink_cfg.row         = -5;
	fbink_cfg.is_centered = true;
	fbink_cfg.is_padded   = true;
	fbink_cfg.to_syslog   = true;
	// We'll want early errors to already go to syslog
	fbink_update_verbosity(&fbink_cfg);

	if ((fbfd = fbink_open()) == ERRCODE(EXIT_FAILURE)) {
		LOG(LOG_CRIT, "Failed to open the framebuffer, aborting . . .");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}
	if (fbink_init(fbfd, &fbink_cfg) == ERRCODE(EXIT_FAILURE)) {
		LOG(LOG_CRIT, "Failed to initialize FBInk, aborting . . .");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}
	LOG(LOG_INFO, "Initialized FBInk %s", fbink_version());

	// Setup libue
	int rc = -1;
	rc     = ue_init_listener(&listener);
	if (rc < 0) {
		LOG(LOG_CRIT, "Failed to initialize libue listener (%d)", rc);
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}
	LOG(LOG_INFO, "Initialized libue v%s", LIBUE_VERSION);

	// Setup libevdev
	evfd = open(NTX_KEYS_EVDEV, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
	if (evfd == -1) {
		PFLOG(LOG_CRIT, "open: %m");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}

	dev = libevdev_new();
	libevdev_set_device_log_function(dev, &libevdev_to_syslog, LIBEVDEV_LOG_INFO, NULL);
	rc = libevdev_set_fd(dev, evfd);
	if (rc < 0) {
		LOG(LOG_CRIT, "Failed to initialize libevdev (%s)", strerror(-rc));
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}
	// Check that nothing else has grabbed the input device, because that would prevent us from using it...
	if (libevdev_grab(dev, LIBEVDEV_GRAB) != 0) {
		LOG(LOG_CRIT,
		    "Cannot read input events because the input device is currently grabbed by something else!");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}
	// And we ourselves don't need to grab it, so, don't ;).
	libevdev_grab(dev, LIBEVDEV_UNGRAB);
	LOG(LOG_INFO, "Initialized libevdev v%s for device '%s'", LIBEVDEV_VERSION, libevdev_get_name(dev));

	// Now that FBInk has been initialized, setup the USB product ID for the current device
	FBInkState fbink_state = { 0 };
	fbink_get_state(&fbink_cfg, &fbink_state);
	setup_usb_ids(fbink_state.device_id);

	// Much like in KOReader's OTAManager, check if we can use pipefail in a roundabout way,
	// because old busybox ash versions will *abort* on set failures...
	rc = system("set -o pipefail 2>/dev/null");
	if (rc == EXIT_SUCCESS) {
		setenv("WITH_PIPEFAIL", "true", 1);
	} else {
		setenv("WITH_PIPEFAIL", "false", 1);
	}

	// Setup the fd for ntx_io ioctls
	ntxfd = open("/dev/ntx_io", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (ntxfd == -1) {
		PFLOG(LOG_CRIT, "open: %m");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}

	// Display our header
	fbink_cfg.no_refresh = true;
	fbink_cls(fbfd, &fbink_cfg, NULL);
	ot_cfg.margins.top = (short int) fbink_state.font_h;
	ot_cfg.size_px     = (unsigned short int) (fbink_state.font_h * 2U);
	snprintf(resource_path, sizeof(resource_path) - 1U, "%s/resources/fonts/CaskaydiaCove_NF.ttf", abs_pwd);
	if (fbink_add_ot_font_v2(resource_path, FNT_REGULAR, &icon_cfg) != EXIT_SUCCESS) {
		PFLOG(LOG_CRIT, "Failed to load main font!");
		rv = USBMS_EARLY_EXIT;
		goto cleanup;
	}
	// NOTE: Minor hackery: instead of the custom LGC Nerdfont we ship, for CJK, use KOReader's own CJK font...
	//       (The only remotely CJK-ish NerdFont available is M+, and it's more J than CJK ;)).
	if (is_CJK) {
		snprintf(
		    resource_path, sizeof(resource_path) - 1U, "%s/resources/fonts/NotoSansCJKsc-Regular.otf", abs_pwd);
		if (fbink_add_ot_font_v2(resource_path, FNT_REGULAR, &msg_cfg) != EXIT_SUCCESS) {
			PFLOG(LOG_CRIT, "Failed to load CJK font!");
			rv = USBMS_EARLY_EXIT;
			goto cleanup;
		}
		// The first ot_cfg print (title bar) actually requires CJK support, so point it at our CJK font...
		ot_cfg.font = msg_cfg.font;
	} else {
		// If we don't need CJK support, we simply use the main font everywhere
		ot_cfg.font  = icon_cfg.font;
		msg_cfg.font = icon_cfg.font;
	}
	fbink_print_ot(fbfd, _("USB Mass Storage"), &ot_cfg, &fbink_cfg, NULL);
	if (is_CJK) {
		// Back to the main font, as this will only be used for the status bar from now on
		ot_cfg.font = icon_cfg.font;
	}
	fbink_cfg.ignore_alpha  = true;
	fbink_cfg.halign        = CENTER;
	fbink_cfg.scaled_height = (short int) (fbink_state.screen_height / 10U);
	fbink_cfg.row           = 3;
	snprintf(resource_path, sizeof(resource_path) - 1U, "%s/resources/img/koreader.png", abs_pwd);
	fbink_print_image(fbfd, resource_path, 0, 0, &fbink_cfg);
	fbink_cfg.no_refresh  = false;
	fbink_cfg.is_flashing = true;
	fbink_refresh(fbfd, 0, 0, 0, 0, &fbink_cfg);
	fbink_cfg.is_flashing = false;

	// Display a minimal status bar on screen
	tzset();
	fbink_cfg.row      = -3;
	ot_cfg.margins.top = (short int) -(fbink_state.font_h * 3U);
	ot_cfg.padding     = HORI_PADDING;
	print_status(fbfd, &fbink_cfg, &ot_cfg, ntxfd);

	// Setup the center icon display
	icon_cfg.size_px = (unsigned short int) (fbink_state.font_h * 30U);
	icon_cfg.padding = HORI_PADDING;

	// The various lsmod checks will take a while, so, start with the initial cable status...
	bool usb_plugged = is_usb_plugged(ntxfd);
	print_icon(fbfd, usb_plugged ? "\uf700" : "\uf701", &fbink_cfg, &icon_cfg);

	// Setup the message area
	msg_cfg.size_px     = ot_cfg.size_px;
	msg_cfg.padding     = icon_cfg.padding;
	fbink_cfg.row       = -14;
	msg_cfg.margins.top = (short int) -(fbink_state.font_h * 14U);
	// We want enough space for 4 lines (+/- metrics shenanigans)
	msg_cfg.margins.bottom = (short int) (fbink_state.font_h * (14U - (4U * 2U) - 1U));
	msg_cfg.padding        = FULL_PADDING;

	// And now, on to the fun stuff!
	bool need_early_abort = false;
	bool early_unmount    = false;
	// If we're in USBNet mode, abort!
	// (tearing it down properly is out of our scope, since we can't really know how the user enabled it in the first place).
	if (is_module_loaded("g_ether")) {
		LOG(LOG_ERR, "Device is in USBNet mode, aborting");
		need_early_abort = true;

		print_icon(fbfd, "\uf6ff", &fbink_cfg, &icon_cfg);
		fbink_print_ot(fbfd,
			       // @translators: First unicode codepoint is an icon, leave it as-is.
			       _("\uf071 Please disable USBNet manually!\nPress the power button to exit."),
			       &msg_cfg,
			       &fbink_cfg,
			       NULL);
	}

	// Same deal for USBSerial...
	if (is_module_loaded("g_serial")) {
		LOG(LOG_ERR, "Device is in USBSerial mode, aborting");
		need_early_abort = true;

		// NOTE: There's also U+fb5b for a serial cable icon
		print_icon(fbfd, "\ue795", &fbink_cfg, &icon_cfg);
		fbink_print_ot(fbfd,
			       // @translators: First unicode codepoint is an icon, leave it as-is.
			       _("\uf071 Please disable USBSerial manually!\nPress the power button to exit."),
			       &msg_cfg,
			       &fbink_cfg,
			       NULL);
	}

	// Wee bit of trickery with an obscure umount2 feature, to see if the mountpoint is currently busy,
	// without actually unmounting it for real...
	rc = umount2(KOBO_MOUNTPOINT, MNT_EXPIRE);
	if (rc != EXIT_SUCCESS) {
		if (errno == EAGAIN) {
			// That means we're good to go ;).
			LOG(LOG_INFO,
			    "Internal storage partition wasn't busy, it's been successfully marked as expired.");
		} else if (errno == EBUSY) {
			LOG(LOG_WARNING, "Internal storage partition is busy, can't export it!");
			print_icon(fbfd, "\uf7c9", &fbink_cfg, &icon_cfg);

			// Start a little bit higher than usual to leave us some room...
			fbink_cfg.row       = -16;
			msg_cfg.margins.top = (short int) -(fbink_state.font_h * 16U);
			rc                  = fbink_print_ot(fbfd,
                                            // @translators: First unicode codepoint is an icon, leave it as-is.
                                            _("\uf071 Filesystem is busy! Offending processes:"),
                                            &msg_cfg,
                                            &fbink_cfg,
                                            NULL);

			// And now, switch to a smaller font size when consuming the script's output...
			msg_cfg.padding     = HORI_PADDING;
			msg_cfg.size_px     = fbink_state.font_h;
			msg_cfg.margins.top = (short int) rc;
			// Drop the bottom margin to allow stomping over the status bar...
			msg_cfg.margins.bottom = 0;

			LOG(LOG_WARNING, "Listing offending processes...");
			snprintf(resource_path, sizeof(resource_path) - 1U, "%s/scripts/fuser-check.sh", abs_pwd);
			FILE* f = popen(resource_path, "re");
			if (f) {
				char line[PIPE_BUF];
				while (fgets(line, sizeof(line), f)) {
					rc                  = fbink_print_ot(fbfd, line, &msg_cfg, &fbink_cfg, NULL);
					msg_cfg.margins.top = (short int) rc;
				}

				// Back to normal :)
				msg_cfg.size_px = ot_cfg.size_px;

				rc = pclose(f);
				if (rc != EXIT_SUCCESS) {
					// Hu oh... Print a giant warning, and abort.
					LOG(LOG_CRIT, "The fuser script failed (%d)!", rc);
					print_icon(fbfd, "\uf06a", &fbink_cfg, &icon_cfg);
					rc = fbink_print_ot(
					    fbfd,
					    // @translators: First unicode codepoint is an icon, leave it as-is. fuser is a program name, leave it as-is.
					    _("\uf071 The fuser script failed!"),
					    &msg_cfg,
					    &fbink_cfg,
					    NULL);
					msg_cfg.margins.top = (short int) rc;
				}
			} else {
				// Hu oh... Print a giant warning, and abort.
				LOG(LOG_CRIT, "Failed to run fuser script!");
				print_icon(fbfd, "\uf06a", &fbink_cfg, &icon_cfg);
				rc                  = fbink_print_ot(fbfd,
                                                    // @translators: First unicode codepoint is an icon, leave it as-is.
                                                    _("\uf071 Failed to run the fuser script!"),
                                                    &msg_cfg,
                                                    &fbink_cfg,
                                                    NULL);
				msg_cfg.margins.top = (short int) rc;
			}

			// We can still exit safely at that point
			fbink_print_ot(fbfd, _("Press the power button to exit."), &msg_cfg, &fbink_cfg, NULL);
			need_early_abort = true;
		} else {
			PFLOG(LOG_CRIT, "umount2: %m");
			rv = USBMS_EARLY_EXIT;
			goto cleanup;
		}
	} else {
		// NOTE: This should never really happen...
		LOG(LOG_WARNING,
		    "Internal storage partition has been unmounted early: it wasn't busy, and it was already marked as expired?!");
		early_unmount = true;
	}

	// If we need an early abort because of USBNet/USBSerial or a busy mountpoint, do it now...
	if (need_early_abort) {
		LOG(LOG_INFO, "Waiting for a power button press . . .");
		struct pollfd pfd = { 0 };
		pfd.fd            = evfd;
		pfd.events        = POLLIN;

		size_t retry = 0U;
		while (true) {
			int poll_num = poll(&pfd, 1, 5 * 1000);

			// Refresh the status bar
			print_status(fbfd, &fbink_cfg, &ot_cfg, ntxfd);

			if (poll_num == -1) {
				if (errno == EINTR) {
					continue;
				}
				PFLOG(LOG_CRIT, "poll: %m");
				rv = early_unmount ? EXIT_FAILURE : USBMS_EARLY_EXIT;
				goto cleanup;
			}

			if (poll_num > 0) {
				if (pfd.revents & POLLIN) {
					if (handle_evdev(dev)) {
						LOG(LOG_NOTICE, "Caught a power button release");
						if (early_unmount) {
							fbink_print_ot(
							    fbfd,
							    // @translators: First unicode codepoint is an icon, leave it as-is.
							    _("\uf071 The device will shutdown in 30 sec."),
							    &msg_cfg,
							    &fbink_cfg,
							    NULL);
						} else {
							fbink_print_ot(
							    fbfd,
							    // @translators: First unicode codepoint is an icon, leave it as-is.
							    _("\uf05a KOReader will now restart…"),
							    &msg_cfg,
							    &fbink_cfg,
							    NULL);
						}
						fbink_wait_for_complete(fbfd, LAST_MARKER);
						break;
					}
				}
			}

			if (poll_num == 0) {
				// Timed out, increase the retry counter
				retry++;
			}

			// Give up afer 30 sec
			if (retry >= 6) {
				LOG(LOG_NOTICE, "It's been 30 sec, giving up");
				if (early_unmount) {
					fbink_print_ot(
					    fbfd,
					    // @translators: First unicode codepoint is an icon, leave it as-is.
					    _("\uf05a Gave up after 30 sec.\nThe device will shutdown in 30 sec."),
					    &msg_cfg,
					    &fbink_cfg,
					    NULL);
				} else {
					fbink_print_ot(
					    fbfd,
					    // @translators: First unicode codepoint is an icon, leave it as-is.
					    _("\uf05a Gave up after 30 sec.\nKOReader will now restart…"),
					    &msg_cfg,
					    &fbink_cfg,
					    NULL);
				}
				// Make sure this message will be visible...
				fbink_wait_for_complete(fbfd, LAST_MARKER);
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
	usb_plugged = is_usb_plugged(ntxfd);
	if (!usb_plugged) {
		fbink_print_ot(fbfd,
			       _("Waiting to be plugged in…\nOr, press the power button to exit."),
			       &msg_cfg,
			       &fbink_cfg,
			       NULL);

		LOG(LOG_INFO, "Waiting for a plug in event or a power button press . . .");
		struct pollfd pfds[2] = { 0 };
		nfds_t        nfds    = 2;
		// Input device
		pfds[0].fd     = evfd;
		pfds[0].events = POLLIN;
		// Uevent socket
		pfds[1].fd     = listener.pfd.fd;
		pfds[1].events = listener.pfd.events;

		struct uevent uev;
		size_t        retry = 0U;
		while (true) {
			int poll_num = poll(pfds, nfds, 5 * 1000);

			// Refresh the status bar
			print_status(fbfd, &fbink_cfg, &ot_cfg, ntxfd);

			if (poll_num == -1) {
				if (errno == EINTR) {
					continue;
				}
				PFLOG(LOG_CRIT, "poll: %m");
				rv = early_unmount ? EXIT_FAILURE : USBMS_EARLY_EXIT;
				goto cleanup;
			}

			if (poll_num > 0) {
				if (pfds[0].revents & POLLIN) {
					if (handle_evdev(dev)) {
						LOG(LOG_NOTICE, "Caught a power button release");
						if (early_unmount) {
							fbink_print_ot(
							    fbfd,
							    // @translators: First unicode codepoint is an icon, leave it as-is.
							    _("\uf071 The device will shutdown in 30 sec."),
							    &msg_cfg,
							    &fbink_cfg,
							    NULL);
						} else {
							fbink_print_ot(
							    fbfd,
							    // @translators: First unicode codepoint is an icon, leave it as-is.
							    _("\uf05a KOReader will now restart…"),
							    &msg_cfg,
							    &fbink_cfg,
							    NULL);
						}
						need_early_abort = true;
						// That's a direct user interaction with an expected result, don't dawdle.
						sleep_on_abort = false;
						break;
					}
				}

				if (pfds[1].revents & POLLIN) {
					if (handle_uevent(&listener, &uev) == EXIT_SUCCESS) {
						// Now check if it's a plug in...
						if (uev.action == UEVENT_ACTION_ADD && uev.devpath &&
						    UE_STR_EQ(uev.devpath, KOBO_USB_DEVPATH_PLUG)) {
							LOG(LOG_WARNING,
							    "Caught a plug in event, but to a plain power source, not a USB host");
							if (early_unmount) {
								fbink_print_ot(
								    fbfd,
								    // @translators: First unicode codepoint is an icon, leave it as-is.
								    _("\uf071 The device was plugged into a plain power source, not a USB host!\nThe device will shutdown in 30 sec."),
								    &msg_cfg,
								    &fbink_cfg,
								    NULL);
							} else {
								fbink_print_ot(
								    fbfd,
								    // @translators: First unicode codepoint is an icon, leave it as-is.
								    _("\uf071 The device was plugged into a plain power source, not a USB host!\nKOReader will now restart…"),
								    &msg_cfg,
								    &fbink_cfg,
								    NULL);
							}
							need_early_abort = true;
							break;
						} else if (uev.action == UEVENT_ACTION_ADD && uev.devpath &&
							   UE_STR_EQ(uev.devpath, KOBO_USB_DEVPATH_HOST)) {
							LOG(LOG_NOTICE, "Caught a plug in event (to a USB host)");
							break;
						}
					}
				}
			}

			if (poll_num == 0) {
				// Timed out, increase the retry counter
				retry++;
			}

			// Give up afer 90 sec
			if (retry >= 18) {
				LOG(LOG_NOTICE, "It's been 90 sec, giving up");
				if (early_unmount) {
					fbink_print_ot(
					    fbfd,
					    // @translators: First unicode codepoint is an icon, leave it as-is.
					    _("\uf05a Gave up after 90 sec.\nThe device will shutdown in 30 sec."),
					    &msg_cfg,
					    &fbink_cfg,
					    NULL);
				} else {
					fbink_print_ot(
					    fbfd,
					    // @translators: First unicode codepoint is an icon, leave it as-is.
					    _("\uf05a Gave up after 90 sec.\nKOReader will now restart…"),
					    &msg_cfg,
					    &fbink_cfg,
					    NULL);
				}
				need_early_abort = true;
				break;
			}
		}
	} else {
		// NOTE: usb_plugged will be true if usbms was started while *already* plugged in,
		//       even if it's to a plain power source, and not a USB host...
		//       On some devices, POWER_SUPPLY_PROP_ONLINE is smart enough to be able to tell the difference,
		//       which means we can read it from sysfs (c.f., ricoh61x_batt_get_prop @ drivers/power/ricoh619-battery.c),
		//       but on older devices, it isn't, and the discrimination is *only* done during the plug in event...
		//       (c.f., drivers/input/misc/usb_plug.c).
		//       So, do what we can here, otherwise, the state may need to be tracked by the frontend,
		//       assuming it *also* got a chance to catch the event: i.e., it started *before* the plug in...
		// NOTE: Unfortunately, the only platform where we can do that appears to be Mk. 7...
		if (access(CHARGER_TYPE_SYSFS, F_OK) == 0) {
			LOG(LOG_INFO, "Checking charger type");
			FILE* f = fopen(CHARGER_TYPE_SYSFS, "re");
			if (f) {
				char   charger_type[16] = { 0 };
				size_t size = fread(charger_type, sizeof(*charger_type), sizeof(charger_type), f);
				if (size > 0) {
					// NUL terminate
					charger_type[size - 1U] = '\0';
					// Strip trailing LF
					if (charger_type[size - 2U] == '\n') {
						charger_type[size - 2U] = '\0';
					}
				} else {
					LOG(LOG_WARNING, "Failed to read the charger type from sysfs!");
				}
				fclose(f);

				// c.f., charger_type_read @ drivers/power/ricoh619-battery.c
				if (strncmp(charger_type, "CDP", 3U) == 0U) {
					LOG(LOG_WARNING, "CDP (Charging Downstream Port) charger detected");
					need_early_abort = true;
				} else if (strncmp(charger_type, "DCP", 3U) == 0U) {
					LOG(LOG_WARNING, "DCP (Dedicated Charging Port) charger detected");
					need_early_abort = true;
				} else if (strncmp(charger_type, "SDP_PC", 6U) == 0U) {
					// That's the only one we can go through with ;)
					LOG(LOG_INFO, "SDP PC (Standard Downstream Port, 500mA) charger detected");
				} else if (strncmp(charger_type, "SDP_ADPT", 8U) == 0U) {
					LOG(LOG_WARNING, "SDP ADPT (Standard Downstream Port, 800mA) charger detected");
					need_early_abort = true;
				} else if (strncmp(charger_type, "NO", 2U) == 0U) {
					// NOTE: Despite being in a usb_plugged branch,
					//       this *may* happen if the device is fully charged.
					//       In which case,
					//       /sys/class/power_supply/mc13892_bat/status will *also* say "Not charging".
					//       I'm not double-checking capacity here, but it ought to be 100 in these cases ;).
					LOG(LOG_INFO, "No charger detected! Fully charged?");
				} else if (strncmp(charger_type, "DISABLE", 7U) == 0U) {
					LOG(LOG_WARNING, "Charger is disabled!");
					need_early_abort = true;
				} else {
					LOG(LOG_ERR, "Unknown charger type!");
					need_early_abort = true;
				}

				// While a CDP could technically enumerate,
				// the discrimination between usb_plug and usb_host is only based on detecting an SDP PC
				// in drivers/input/misc/usb_plug.c, so, do the same thing here.
				// (c.f., ricoh619_charger_detect @ drivers/mfd/ricoh619.c)
				// NOTE: SDP_CHARGER == SDP_PC_CHARGER != SDP_ADPT_CHARGER
				//       c.f., include/linux/power/ricoh619_battery.h
				if (need_early_abort) {
					LOG(LOG_ERR, "Charger type is not SDP PC, aborting");
					if (early_unmount) {
						fbink_print_ot(
						    fbfd,
						    // @translators: First unicode codepoint is an icon, leave it as-is.
						    _("\uf071 The device is plugged into a plain power source, not a USB host!\nThe device will shutdown in 30 sec."),
						    &msg_cfg,
						    &fbink_cfg,
						    NULL);
					} else {
						fbink_print_ot(
						    fbfd,
						    // @translators: First unicode codepoint is an icon, leave it as-is.
						    _("\uf071 The device is plugged into a plain power source, not a USB host!\nKOReader will now restart…"),
						    &msg_cfg,
						    &fbink_cfg,
						    NULL);
					}
				}
			} else {
				LOG(LOG_WARNING, "Failed to open the sysfs entry for charger type (%m)!");
			}
		} else {
			LOG(LOG_INFO, "Device generation is older than Mk. 7, can't check charger type!");
		}
	}

	// If we aborted before plug in, we can (usually) still exit safely...
	if (need_early_abort) {
		// Make sure the final message will be visible...
		fbink_wait_for_complete(fbfd, LAST_MARKER);
		if (sleep_on_abort) {
			const struct timespec zzz = { 2L, 500000000L };
			nanosleep(&zzz, NULL);
		}
		rv = early_unmount ? EXIT_FAILURE : USBMS_EARLY_EXIT;
		goto cleanup;
	}

	// We're plugged in, here comes the fun...
	LOG(LOG_INFO, "Starting USBMS session...");
	print_icon(fbfd, "\uf287", &fbink_cfg, &icon_cfg);
	fbink_print_ot(fbfd, _("Starting USBMS session…"), &msg_cfg, &fbink_cfg, NULL);

	// NOTE: We need to figure out the frontlight intensity *before* we unmount onboard,
	//       because, on < Mk. 7 devices, we'll have to get that from KOReader's config file...
	uint8_t fl_intensity = get_frontlight_intensity();

	// Here goes nothing...
	snprintf(resource_path, sizeof(resource_path) - 1U, "%s/scripts/start-usbms.sh", abs_pwd);
	rc = system(resource_path);
	if (rc != EXIT_SUCCESS) {
		// Hu oh... Print a giant warning, and abort. KOReader will shutdown the device after a while.
		LOG(LOG_CRIT, "Failed to start the USBMS session (%d)!", rc);
		print_icon(fbfd, "\uf06a", &fbink_cfg, &icon_cfg);
		fbink_print_ot(fbfd,
			       // @translators: First unicode codepoint is an icon, leave it as-is.
			       _("\uf071 Failed to start the USBMS session!\nThe device will shutdown in 90 sec."),
			       &msg_cfg,
			       &fbink_cfg,
			       NULL);

		rv = EXIT_FAILURE;
		goto cleanup;
	}

	// Now we're cooking with gas!
	LOG(LOG_INFO, "USBMS session in progress");
	// Switch to nightmode for the duration of the session, as a nod to the stock behavior ;).
	fbink_cfg.is_nightmode = true;
	fbink_cfg.no_refresh   = true;
	fbink_print_ot(fbfd,
		       _("USBMS session in progress.\nPlease eject your device safely before unplugging it."),
		       &msg_cfg,
		       &fbink_cfg,
		       NULL);
	// Refresh the status bar
	print_status(fbfd, &fbink_cfg, &ot_cfg, ntxfd);
	fbink_cfg.no_refresh = false;
	fbink_refresh(fbfd, 0, 0, 0, 0, &fbink_cfg);

	// And much like Nickel, gently turn the light off for the duration...
	LOG(LOG_INFO, "Frontlight was set to %hhu%%", fl_intensity);
	if (fl_intensity != 0U) {
		LOG(LOG_INFO, "Turning frontlight off...");
		toggle_frontlight(false, fl_intensity, ntxfd);
	}

	// And now we just have to wait until an unplug...
	LOG(LOG_INFO, "Waiting for an eject or unplug event . . .");
	struct pollfd pfd = { 0 };
	pfd.fd            = listener.pfd.fd;
	pfd.events        = listener.pfd.events;

	struct uevent uev;
	// NOTE: This is basically ue_wait_for_event, but with a 45s timeout,
	//       solely for the purpose of refreshing the status bar,
	//       because we don't necessarily get change events on power_supply on older devices
	//       (e.g., it happens on Mk. 7, but not on Mk. 5)...
	while (true) {
		int poll_num = poll(&pfd, 1, 45 * 1000);

		if (poll_num == -1) {
			if (errno == EINTR) {
				continue;
			}
			PFLOG(LOG_CRIT, "poll: %m");
			rv = EXIT_FAILURE;
			goto cleanup;
		}

		if (poll_num > 0) {
			if (pfd.revents & POLLIN) {
				if (handle_uevent(&listener, &uev) == EXIT_SUCCESS) {
					// Now check if it's an eject or an unplug...
					if (uev.action == UEVENT_ACTION_OFFLINE && uev.devpath &&
					    (UE_STR_EQ(uev.devpath, KOBO_USB_DEVPATH_FSL) ||
					     (uev.modalias && UE_STR_EQ(uev.modalias, KOBO_USB_MODALIAS_CI)) ||
					     UE_STR_EQ(uev.devpath, KOBO_USB_DEVPATH_UDC))) {
						LOG(LOG_NOTICE, "Caught an eject event");
						break;
					} else if (uev.action == UEVENT_ACTION_REMOVE && uev.devpath &&
						   (UE_STR_EQ(uev.devpath, KOBO_USB_DEVPATH_PLUG) ||
						    UE_STR_EQ(uev.devpath, KOBO_USB_DEVPATH_HOST))) {
						LOG(LOG_NOTICE, "Caught an unplug event");
						break;
					} else if (uev.action == UEVENT_ACTION_CHANGE && uev.subsystem &&
						   UE_STR_EQ(uev.subsystem, "power_supply")) {
						LOG(LOG_NOTICE, "Caught a charge tick");
						// On devices where we do get those events, refresh the status bar.
						// (c.f., NOTE above).
						print_status(fbfd, &fbink_cfg, &ot_cfg, ntxfd);
					}
				}
			}
		}

		if (poll_num == 0) {
			// Refresh the status bar on every timeout
			print_status(fbfd, &fbink_cfg, &ot_cfg, ntxfd);
		}
	}
	fbink_cfg.is_nightmode = false;
	fbink_refresh(fbfd, 0, 0, 0, 0, &fbink_cfg);

	// Turn frontlight back on
	if (fl_intensity != 0U) {
		LOG(LOG_INFO, "Turning frontlight back on...");
		toggle_frontlight(true, fl_intensity, ntxfd);
	}

	// If ue_wait_for_event failed for some reason, abort with extreme prejudice...
	if (rc != EXIT_SUCCESS) {
		LOG(LOG_CRIT, "Failed to detect an unlug event!");
		print_icon(fbfd, "\uf06a", &fbink_cfg, &icon_cfg);
		fbink_print_ot(fbfd,
			       // @translators: First unicode codepoint is an icon, leave it as-is.
			       _("\uf071 Failed to detect an unplug event!\nThe device will shutdown in 90 sec."),
			       &msg_cfg,
			       &fbink_cfg,
			       NULL);

		rv = EXIT_FAILURE;
		goto cleanup;
	}

	// And now remount all the things!
	LOG(LOG_INFO, "Ending USBMS session...");
	print_icon(fbfd, "\ufa52", &fbink_cfg, &icon_cfg);
	fbink_print_ot(fbfd, _("Ending USBMS session…"), &msg_cfg, &fbink_cfg, NULL);
	// Refresh the status bar
	print_status(fbfd, &fbink_cfg, &ot_cfg, ntxfd);

	// Nearly there...
	snprintf(resource_path, sizeof(resource_path) - 1U, "%s/scripts/end-usbms.sh", abs_pwd);
	rc = system(resource_path);
	if (rc != EXIT_SUCCESS) {
		// Hu oh... Print a giant warning, and abort. KOReader will shutdown the device after a while.
		LOG(LOG_CRIT, "Failed to end the USBMS session (%d)!", rc);
		print_icon(fbfd, "\uf06a", &fbink_cfg, &icon_cfg);
		fbink_print_ot(fbfd,
			       // @translators: First unicode codepoint is an icon, leave it as-is.
			       _("\uf071 Failed to end the USBMS session!\nThe device will shutdown in 90 sec."),
			       &msg_cfg,
			       &fbink_cfg,
			       NULL);

		rv = EXIT_FAILURE;
		goto cleanup;
	}

	// Whee!
	LOG(LOG_INFO, "Done :)");
	// NOTE: We batch the final screen, make it flash, and wait for completion of the refresh,
	//       all in order to make sure we won't lose a race with the refresh induced by KOReader's restart...
	//       (i.e., don't let it get optimized out by the EPDC).
	fbink_cfg.no_refresh = true;
	print_icon(fbfd, "\uf058", &fbink_cfg, &icon_cfg);
	fbink_print_ot(fbfd, _("Done!\nKOReader will now restart…"), &msg_cfg, &fbink_cfg, NULL);
	// Refresh the status bar
	print_status(fbfd, &fbink_cfg, &ot_cfg, ntxfd);
	fbink_cfg.no_refresh  = false;
	fbink_cfg.is_flashing = true;
	fbink_refresh(fbfd, 0, 0, 0, 0, &fbink_cfg);
	fbink_cfg.is_flashing = false;
	fbink_wait_for_complete(fbfd, LAST_MARKER);

cleanup:
	LOG(LOG_INFO, "Bye!");

	fbink_free_ot_fonts_v2(&icon_cfg);
	if (is_CJK) {
		fbink_free_ot_fonts_v2(&msg_cfg);
		ot_cfg.font = NULL;
	} else {
		// We share the same font everywhere, so just avoid dangling pointers
		msg_cfg.font = NULL;
		ot_cfg.font  = NULL;
	}
	fbink_close(fbfd);

	ue_destroy_listener(&listener);

	libevdev_free(dev);
	if (evfd != -1) {
		close(evfd);
	}

	if (ntxfd != -1) {
		close(ntxfd);
	}

	if (pwd != -1) {
		if (fchdir(pwd) == -1) {
			// NOTE: That would be bad if we were launched from within the internal storage
			//       (i.e., that'd be a hint that it probably failed to remount properly).
			//       Which is why you should start this from within a tmpfs, like KOReader ;).
			PFLOG(LOG_CRIT, "fchdir: %m");
			rv = EXIT_FAILURE;
		}
		close(pwd);
	}
	free(abs_pwd);

	closelog();

	return rv;
}
