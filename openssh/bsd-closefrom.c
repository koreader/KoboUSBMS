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

// Because we're pretty much Linux-bound ;).
#ifndef _GNU_SOURCE
#	define _GNU_SOURCE
#endif

#include <fts.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

// Mimic scandir's alphasort
static int
    fts_natsort(const FTSENT** a, const FTSENT** b)
{
	return strverscmp((*a)->fts_name, (*b)->fts_name);
}

static void
    bsd_closefrom(int lowfd)
{
// NOTE: I long for the days we'll actually be able to use this... (Linux 5.9+ w/ glibc 2.34+)
//       c.f., this fantastic recap of how messy achieving this can be: https://stackoverflow.com/a/918469
#if HAVE_CLOSE_RANGE
	if (close_range(lowfd, ~0U, 0) == 0) {
		return;
	}
#endif

	// NOTE: We use fts because it caches the whole thing *first*.
	//       With a readdir loop, we'd risk missing stuff as procfs doesn't guarantee returning the next entry if we delete the current one,
	//       c.f., the gymnastics glibc's closefrom_fallback implementation has to deal with.
	//       This issue was reported in libbsd in https://bugs.freedesktop.org/show_bug.cgi?id=85663, and libbsd now does something fancier:
	//       https://cgit.freedesktop.org/libbsd/tree/src/closefrom.c
	//       sudo is still using a standard readdir loop: https://github.com/sudo-project/sudo/blob/main/lib/util/closefrom.c
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
#pragma clang diagnostic ignored "-Wincompatible-pointer-types-discards-qualifiers"
	char* const fdpath[] = { "/proc/self/fd", NULL };
#pragma GCC diagnostic pop
	FTS* restrict ftsp;
	if ((ftsp = fts_open(fdpath, FTS_PHYSICAL | FTS_XDEV, &fts_natsort)) == NULL) {
		// fall back to brute force closure
		return closefrom_fallback(lowfd);
	}
	// Initialize ftsp with as many toplevel entries as possible.
	FTSENT* restrict chp;
	chp = fts_children(ftsp, 0);
	if (chp == NULL) {
		// No files to traverse! Unlikely in this context...
		fts_close(ftsp);
		return closefrom_fallback(lowfd);
	}
	FTSENT* restrict p;
	while ((p = fts_read(ftsp)) != NULL) {
		char* endp;
		long  fd;
		switch (p->fts_info) {
			case FTS_SL:
				fd = strtol(p->fts_name, &endp, 10);
				if (p->fts_name != endp && *endp == '\0' && fd >= 0 && fd < INT_MAX && fd >= lowfd &&
				    fd != ftsp->fts_rfd) {
					// NOTE: We'll eventually hit a temporary fd caught during the initial walk,
					//       it was created by fts itself, but is now gone,
					//       so close will harmlessly fail with EBADF on that one.
					close((int) fd);
				}
				break;
			default:
				break;
		}
	}
	fts_close(ftsp);
}
