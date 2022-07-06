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

#include <signal.h>

#if defined(__OpenBSD__)
# define SIGNALS	{ SIGSEGV, SIGBUS }
# if defined(__aarch64__)
#  define PC(ctx)	((ctx)->sc_elr)
# elif defined(__amd64__) || defined(__x86_64__)
#  define PC(ctx)	((ctx)->sc_rip)
# elif defined(__i386__)
#  define PC(ctx)	((ctx)->sc_eip)
# elif defined(__riscv64)
#  define PC(ctx)	((ctx)->sc_sepc)
# endif
#elif defined(__NetBSD__)
# define SIGNALS	{ SIGSEGV, SIGBUS }
# define PC(ctx)	_UC_MACHINE_PC(ctx)
#elif defined(__FreeBSD__)
# define SIGNALS	{ SIGSEGV, SIGBUS }
# if defined(__aarch64__)
#  define PC(ctx)	((ctx)->uc_mcontext.mc_gpregs.gp_elr)
# elif defined(__amd64__) || defined(__x86_64__)
#  define PC(ctx)	((ctx)->uc_mcontext.mc_rip)
# elif defined(__i386__)
#  define PC(ctx)	((ctx)->uc_mcontext.mc_eip)
# elif defined(__riscv64)
#  define PC(ctx)	((ctx)->uc_mcontext.mc_gpregs.gp_sepc)
# endif
#elif defined(__DragonFly__)
# define SIGNALS	{ SIGSEGV, SIGBUS }
# if defined(__amd64__) || defined(__x86_64__)
#  define PC(ctx)	((ctx)->uc_mcontext.mc_rip)
# endif
#elif defined(__linux__)
# define SIGNALS	{ SIGSEGV, SIGBUS }
# define SC(ctx)	((struct sigcontext *) &(ctx)->uc_mcontext)
# if defined(__aarch64__)
#  define PC(ctx)	(SC(ctx)->pc)
# elif defined(__amd64__) || defined(__x86_64__)
#  define PC(ctx)	(SC(ctx)->rip)
# elif defined(__i386__)
#  define PC(ctx)	(SC(ctx)->eip)
# elif defined(__riscv64)
#  define PC(ctx)	(SC(ctx)->pc)
# endif
#elif defined(__APPLE__) && defined(__MACH__)
# define SIGNALS	{ SIGSEGV, SIGBUS }
# if defined(__aarch64__)
#  define PC(ctx)	((ctx)->uc_mcontext->__ss.__pc)
# elif defined(__amd64__) || defined(__x86_64__)
#  define PC(ctx)	((ctx)->uc_mcontext->__ss.__rip)
# endif
#endif

static void
handle_fault(int sig, siginfo_t *info, void *ctx)
{
	if (curact.fa_fun != NULL && curact.fa_fun(
	    (const void *) PC((ucontext_t *) ctx),
	    info->si_addr, curact.fa_arg)) {
		return;
	}
}

static int
hook_fault(void)
{
	static const int signals[] = SIGNALS;

	sigset_t mask;
	sigfillset(&mask);

	for (unsigned int i = 0;
	     i < sizeof(signals) / sizeof(signals[0]);
	     i++) {
		if (sigaction(signals[i], &(struct sigaction) {
			.sa_sigaction = handle_fault,
			.sa_mask = mask,
			.sa_flags = SA_SIGINFO
		    }, NULL) != 0)
			return -1;
	}

	return 0;
}

static int
unhook_fault(void)
{
	static const int signals[] = SIGNALS;

	for (unsigned int i = 0;
	     i < sizeof(signals) / sizeof(signals[0]);
	     i++) {
		sigaction(signals[i], &(struct sigaction) {
			.sa_handler = SIG_DFL
		    }, NULL);
	}

	return 0;
}
