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

int
    main(void)
{
	// So far, so good ;).
	int rv = EXIT_SUCCESS;

	// We'll be chatting exclusively over syslog, because duh.
	openlog("usbms", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);

	// Say hello
	LOG(LOG_INFO,
	    "[PID: %ld] Initializing USBMS %s (%s) | FBInk %s | libue %s",
	    (long) getpid(),
	    USBMS_VERSION,
	    USBMS_TIMESTAMP,
	    fbink_version(),
	    LIBUE_VERSION);
	// TODO: libevdev_get_driver_version after init...

	// We'll want to jump to /, and only get back to our original PWD on exit...
	// c.f., man getcwd for the fchdir trick, as we can certainly spare the fd ;).
	// NOTE: O_PATH is Linux 2.6.39+ :(
	int pwd = open(".", O_DIRECTORY | O_PATH | O_CLOEXEC, O_RDONLY);
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

cleanup:
	closelog();

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
