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
#  define SP(ctx)	((ctx)->sc_sp)
# elif defined(__amd64__) || defined(__x86_64__)
#  define PC(ctx)	((ctx)->sc_rip)
#  define SP(ctx)	((ctx)->sc_rsp)
# elif defined(__i386__)
#  define PC(ctx)	((ctx)->sc_eip)
#  define SP(ctx)	((ctx)->sc_esp)
# elif defined(__riscv64) || (defined(__riscv) && __riscv_xlen == 64)
#  define PC(ctx)	((ctx)->sc_sepc)
#  define SP(ctx)	((ctx)->sc_sp)
# endif
#elif defined(__NetBSD__)
# define SIGNALS	{ SIGSEGV, SIGBUS }
# define PC(ctx)	_UC_MACHINE_PC(ctx)
# define SP(ctx)	_UC_MACHINE_SP(ctx)
#elif defined(__FreeBSD__)
# define SIGNALS	{ SIGSEGV, SIGBUS }
# if defined(__aarch64__)
#  define PC(ctx)	((ctx)->uc_mcontext.mc_gpregs.gp_elr)
#  define SP(ctx)	((ctx)->uc_mcontext.mc_gpregs.gp_sp)
# elif defined(__amd64__) || defined(__x86_64__)
#  define PC(ctx)	((ctx)->uc_mcontext.mc_rip)
#  define SP(ctx)	((ctx)->uc_mcontext.mc_rsp)
# elif defined(__i386__)
#  define PC(ctx)	((ctx)->uc_mcontext.mc_eip)
#  define SP(ctx)	((ctx)->uc_mcontext.mc_esp)
# elif defined(__riscv64) || (defined(__riscv) && __riscv_xlen == 64)
#  define PC(ctx)	((ctx)->uc_mcontext.mc_gpregs.gp_sepc)
#  define SP(ctx)	((ctx)->uc_mcontext.mc_gpregs.gp_sp)
# endif
#elif defined(__DragonFly__)
# define SIGNALS	{ SIGSEGV, SIGBUS }
# if defined(__amd64__) || defined(__x86_64__)
#  define PC(ctx)	((ctx)->uc_mcontext.mc_rip)
#  define SP(ctx)	((ctx)->uc_mcontext.mc_rsp)
# endif
#elif defined(__linux__)
# define SIGNALS	{ SIGSEGV, SIGBUS }
# define SC(ctx)	((struct sigcontext *) &(ctx)->uc_mcontext)
# if defined(__aarch64__)
#  define PC(ctx)	(SC(ctx)->pc)
#  define SP(ctx)	(SC(ctx)->sp)
# elif defined(__amd64__) || defined(__x86_64__)
#  define PC(ctx)	(SC(ctx)->rip)
#  define SP(ctx)	(SC(ctx)->rsp)
# elif defined(__i386__)
#  define PC(ctx)	(SC(ctx)->eip)
#  define SP(ctx)	(SC(ctx)->esp)
# elif defined(__riscv64) || (defined(__riscv) && __riscv_xlen == 64)
#  define PC(ctx)	(SC(ctx)->pc)
#  define SP(ctx)	(SC(ctx)->sp)
# endif
#elif defined(__APPLE__) && defined(__MACH__)
# define SIGNALS	{ SIGSEGV, SIGBUS }
# if defined(__aarch64__)
#  define PC(ctx)	((ctx)->uc_mcontext->__ss.__pc)
#  define SP(ctx)	((ctx)->uc_mcontext->__ss.__sp)
# elif defined(__amd64__) || defined(__x86_64__)
#  define PC(ctx)	((ctx)->uc_mcontext->__ss.__rip)
#  define SP(ctx)	((ctx)->uc_mcontext->__ss.__rsp)
# endif
#endif

static void
handle_fault(int sig, siginfo_t *info, void *ctx)
{
	if (curact.fa_fun != NULL && curact.fa_fun(
	    FAULT_BAD_ACCESS, &(struct faultinfo) {
		.fi_pc = (void *) PC((ucontext_t *) ctx),
		.fi_sp = (void *) SP((ucontext_t *) ctx),
		.fi_addr = info->si_addr,
		.fi_ctx = ctx
	    }, curact.fa_arg)) {
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
