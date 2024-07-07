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

#ifndef _BSD_CLOSEFROM_H
#define _BSD_CLOSEFROM_H

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Not in any public headers, c.f., getdents(2)
struct linux_dirent64
{
	ino64_t            d_ino;
	off64_t            d_off;
	unsigned short int d_reclen; /* Length of this linux_dirent */
	unsigned char      d_type;
	char               d_name[256]; /* Filename (null-terminated) */
};

#ifndef OPEN_MAX
#	define OPEN_MAX 256
#endif

// Because our target platforms are too old to support either close_range or the glibc closefrom fallback.
void bsd_closefrom(int lowfd);

#endif    // _BSD_CLOSEFROM_H
