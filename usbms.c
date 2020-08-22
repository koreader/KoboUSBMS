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

static void
    setup_usb_ids(const FBInkConfig* fbink_cfg)
{
	// Map device IDs to USB produc IDs, as we're going to need that in the scripts
	// c.f., https://github.com/kovidgoyal/calibre/blob/9d881ed2fcff219887579571f1bb48bdf41437d4/src/calibre/devices/kobo/driver.py#L1402-L1416
	// c.f., https://github.com/baskerville/plato/blob/d96e40737060b569ae875f37d6d741fd5ccc802c/contrib/plato.sh#L38-L56
	// NOTE: Keep 'em in FBInk order to make my life easier
	FBInkState state = { 0 };
	fbink_get_state(fbink_cfg, &state);

	uint32_t pid = 0xDEAD;
	switch (state.device_id) {
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
			PFLOG(LOG_WARNING, "Can't match device code (%hu) to an USB Product ID!", state.device_id);
			break;
	}

	// Push it to the env...
	char pid_str[7] = { 0 };
	snprintf(pid_str, sizeof(pid_str) - 1, "0x%X", pid);
	setenv("PRODUCT_ID", pid_str, 1);
}

int
    main(void)
{
	// So far, so good ;).
	int                    rv       = EXIT_SUCCESS;
	int                    pwd      = -1;
	int                    fbfd     = -1;
	struct uevent_listener listener = { 0 };
	listener.pfd.fd                 = -1;
	struct libevdev* dev            = NULL;
	int              evfd           = -1;

	// We'll be chatting exclusively over syslog, because duh.
	openlog("usbms", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);

	// Say hello
	LOG(LOG_INFO, "[PID: %ld] Initializing USBMS %s (%s)", (long) getpid(), USBMS_VERSION, USBMS_TIMESTAMP);

	// We'll want to jump to /, and only get back to our original PWD on exit...
	// c.f., man getcwd for the fchdir trick, as we can certainly spare the fd ;).
	// NOTE: O_PATH is Linux 2.6.39+ :(
	pwd = open(".", O_RDONLY | O_DIRECTORY | O_PATH | O_CLOEXEC);
	if (pwd == -1) {
		PFLOG(LOG_CRIT, "open: %m");
		rv = EXIT_FAILURE;
		goto cleanup;
	}
	if (chdir("/") == -1) {
		PFLOG(LOG_CRIT, "chdir: %m");
		rv = EXIT_FAILURE;
		goto cleanup;
	}

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
		rv = EXIT_FAILURE;
		goto cleanup;
	}
	if (fbink_init(fbfd, &fbink_cfg) == ERRCODE(EXIT_FAILURE)) {
		LOG(LOG_CRIT, "Failed to initialize FBInk, aborting . . .");
		rv = EXIT_FAILURE;
		goto cleanup;
	}
	LOG(LOG_INFO, "Initialized FBInk %s", fbink_version());

	// Setup libue
	int rc = -1;
	rc     = ue_init_listener(&listener);
	if (rc < 0) {
		LOG(LOG_CRIT, "Failed to initialize libue listener (%d)", rc);
		rv = EXIT_FAILURE;
		goto cleanup;
	}
	LOG(LOG_INFO, "Initialized libue %s", LIBUE_VERSION);

	// Setup libevdev
	evfd = open(NTX_KEYS_EVDEV, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
	if (evfd == -1) {
		PFLOG(LOG_CRIT, "open: %m");
		rv = EXIT_FAILURE;
		goto cleanup;
	}

	rc = libevdev_new_from_fd(evfd, &dev);
	if (rc < 0) {
		LOG(LOG_CRIT, "Failed to initialize libevdev (%s)", strerror(-rc));
		rv = EXIT_FAILURE;
		goto cleanup;
	}
	LOG(LOG_INFO,
	    "Initialized evdev version: %x for device '%s'",
	    libevdev_get_driver_version(dev),
	    libevdev_get_name(dev));

	// Now that FBInk has been initialized, setup the USB product ID for the current device
	setup_usb_ids(&fbink_cfg);

cleanup:
	closelog();

	fbink_close(fbfd);

	ue_destroy_listener(&listener);

	libevdev_free(dev);
	if (evfd != -1) {
		close(evfd);
	}

	if (pwd != -1) {
		if (fchdir(pwd) == -1) {
			// NOTE: That would be bad, probably failed to remount onboard?
			PFLOG(LOG_CRIT, "fchdir: %m");
			rv = EXIT_FAILURE;
		}
		close(pwd);
	}

	return rv;
}
