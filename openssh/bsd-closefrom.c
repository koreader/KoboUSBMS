/*
 * Copyright (c) 2004-2005 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

// NOTE: https://github.com/openssh/openssh-portable/blob/master/openbsd-compat/bsd-closefrom.c

#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef OPEN_MAX
#	define OPEN_MAX 256
#endif

/*
 * Close all file descriptors greater than or equal to lowfd.
 */
static void
    closefrom_fallback(int lowfd)
{
	/*
	 * Fall back on sysconf(). We avoid checking
	 * resource limits since it is possible to open a file descriptor
	 * and then drop the rlimit such that it is below the open fd.
	 */
	long maxfd = sysconf(_SC_OPEN_MAX);
	if (maxfd < 0) {
		maxfd = OPEN_MAX;
	}

	for (long fd = lowfd; fd < maxfd; fd++) {
		close((int) fd);
	}
}

static void
    closefrom(int lowfd)
{
// NOTE: I long for the days we'll actually be able to use this... (Linux 5.9+ w/ glibc 2.34+)
//       c.f., this fantastic recap of how messy achieving this can be: https://stackoverflow.com/a/918469
#ifdef close_range
	if (close_range(lowfd, ~0U, 0) == 0) {
		return;
	}
#endif

	DIR* dirp;
	/* Check for a /proc/self/fd directory. */
	if ((dirp = opendir("/proc/self/fd")) != NULL) {
		struct dirent* dent;
		while ((dent = readdir(dirp)) != NULL) {
			char* endp;
			long  fd = strtol(dent->d_name, &endp, 10);
			if (dent->d_name != endp && *endp == '\0' && fd >= 0 && fd < INT_MAX && fd >= lowfd &&
			    fd != dirfd(dirp)) {
				// NOTE: It's unclear to me whether the procfs implementation of readdir makes this safe...
				//       libbsd upstream did not think so (https://bugs.freedesktop.org/show_bug.cgi?id=85663),
				//       but that bug doesn't exactly back up that statement with any kind of data...
				//       (Conversely, Apple *explicitly*, at least at one point, said that this is unsafe on HFS:
				//       https://web.archive.org/web/20220122122948/https://support.apple.com/kb/TA21420?locale=en_US).
				// TL;DR: libbsd upstream does somethinf fancier: https://cgit.freedesktop.org/libbsd/tree/src/closefrom.c
				//        But sudo still does something similar: https://github.com/sudo-project/sudo/blob/main/lib/util/closefrom.c
				close((int) fd);
			}
		}
		closedir(dirp);
		return;
	}

	/* /proc/self/fd strategy failed, fall back to brute force closure */
	closefrom_fallback(lowfd);
}
