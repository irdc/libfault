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

#ifndef _FAULT_H_
#define _FAULT_H_

enum {
	FAULT_BAD_ACCESS = 0
};

struct faultinfo {
	void		*fi_pc,
			*fi_sp,
			*fi_addr,
			*fi_ctx;
};

struct faultaction {
	int	(*fa_fun)(int flt, const struct faultinfo *, void *);
	void	*fa_arg;
};

int	 fault(int flt, const struct faultaction *act, struct faultaction *oact);

#endif /* _FAULT_H_ */
