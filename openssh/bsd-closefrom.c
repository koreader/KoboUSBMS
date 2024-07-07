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
	// NOTE: glibc just brute-forces INT_MAX
	long maxfd = sysconf(_SC_OPEN_MAX);
	if (maxfd < 0) {
		maxfd = OPEN_MAX;
	}

	for (long fd = lowfd; fd < maxfd; fd++) {
		close((int) fd);
	}
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

	// NOTE: This is not actually based on the libbsd implementation, but on the CPython & glibc ones.
	//       c.f., https://sourceware.org/git/?p=glibc.git;a=commitdiff;h=607449506f197cc9514408908f41f22537a47a8c
	int dir_fd = open("/proc/self/fd", O_RDONLY | O_DIRECTORY);
	if (dir_fd == -1) {
		// Fall back to brute force closure
		return closefrom_fallback(lowfd);
	}

	char    buffer[1024];
	ssize_t bytes;
	while ((bytes = getdents64(dir_fd, buffer, sizeof(buffer))) > 0) {
		struct linux_dirent64* entry;
		for (off64_t offset = 0; offset < bytes; offset += entry->d_reclen) {
			entry = (struct linux_dirent64*) (buffer + offset);

			// entry->d_type != DT_LNK would also work, as procfs *should* support setting d_type
			if (entry->d_name[0] == '.') {
				continue;
			}

			int fd = 0;
			for (const char* s = entry->d_name; (unsigned int) (*s) - '0' < 10; s++) {
				fd = 10 * fd + (*s - '0');
			}

			if (fd < lowfd || fd == dir_fd) {
				continue;
			}

			close(fd);
		}
		// NOTE: At this point, the glibc implementation rewinds dir_fd if we closed something, "to obtain any possible kernel update".
		//       The commit mentions that getdents64 doesn't appear to return disjointed entries after a close (and I confirmed that),
		//       so this feels completely unnecessary to me in a single-threaded workflow.
		//       And even then, since we already read multiple entries per getdents64 call,
		//       in order to truly always get the latest data from the kernel,
		//       wouldn't we really need to rewind after *every* close, instead of only after every getdents64 call where we closed something?
	}
	close(dir_fd);
}
