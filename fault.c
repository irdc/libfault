/*
 * Copyright (c) 2022 Willemijn Coene
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "fault.h"
#include <stddef.h>
#include <errno.h>

static struct faultaction
curact = { 0 };

#if defined(__OpenBSD__) || \
    defined(__NetBSD__) || \
    defined(__FreeBSD__) || \
    defined(__DragonFly__) || \
    defined(__linux__)
# include "fault-posix.c"
#elif defined(__APPLE__) && defined(__MACH__)
# include "fault-mach.c"
#else
# error "What kind of platform is this?"
#endif

int
fault(int flt, const struct faultaction *act, struct faultaction *oact)
{
	if (flt != FAULT_BAD_ACCESS) {
		errno = EINVAL;
		return -1;
	}

	if (oact != NULL)
		*oact = curact;

	if (act != NULL) {
		int res;

		if (act->fa_fun != NULL && curact.fa_fun == NULL)
			res = hook_fault();
		else if (act->fa_fun == NULL && curact.fa_fun != NULL)
			res = unhook_fault();
		if (res < 0)
			return res;

		curact = *act;
	}

	return 0;
}
