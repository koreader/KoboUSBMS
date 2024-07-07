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

#include "bsd-closefrom.h"

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

// We only care about symlinks
static int
    lnk_only(const struct dirent* d)
{
	return d->d_type == DT_LNK;
}

void
    bsd_closefrom(int lowfd)
{
#if defined(HAVE_CLOSE_RANGE)
	// NOTE: I long for the days we'll actually be able to use this... (Linux 5.9+ w/ glibc 2.34+)
	//       c.f., this fantastic recap of how messy achieving this can be: https://stackoverflow.com/a/918469
	if (close_range(lowfd, ~0U, 0) == 0) {
		return;
	}
#endif

	// NOTE: Unlike the original openssh/libbsd implementation, we use scandir so as not to disappear stuff *during* the walk.
	//       With a readdir loop, we'd risk missing stuff as procfs doesn't guarantee returning the next entry if we delete the current one,
	//       c.f., the gymnastics glibc's closefrom_fallback implementation has to deal with.
	//       This issue was reported in libbsd in https://bugs.freedesktop.org/show_bug.cgi?id=85663,
	//       and upstream libbsd now does something fancier than the openssh copy:
	//       https://cgit.freedesktop.org/libbsd/tree/src/closefrom.c
	//       sudo is still using a standard readdir loop: https://github.com/sudo-project/sudo/blob/main/lib/util/closefrom.c
	struct dirent** namelist;
	int             n = scandir("/proc/self/fd", &namelist, &lnk_only, versionsort);
	if (n == -1) {
		// Fall back to brute force closure
		return closefrom_fallback(lowfd);
	}

	while (n--) {
		char* endp;
		long  fd = strtol(namelist[n]->d_name, &endp, 10);
		if (namelist[n]->d_name != endp && *endp == '\0' && fd >= 0 && fd < INT_MAX && fd >= lowfd) {
			// Note that we'll get to iterate over the temporary fd opened by the initial scandir call.
			// It's gone now, though, so close will just harmlessly fail with EBADF on that one.
			close((int) fd);
		}

		free(namelist[n]);
	}
	free(namelist);
}
