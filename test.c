#include "fault.h"

#include <stdio.h>
#include <unistd.h>
#include <setjmp.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

#include <sys/mman.h>

#define nitems(arr)	(sizeof(arr) / sizeof((arr)[0]))

#if defined(__i386__) || (defined(__APPLE__) && defined(__MACH__))
# define C_(name)	#name
# define C(name)	C_(_ ## name)
#else
# define C(name)	#name
#endif

#if defined(__PIC__)
# define ENTRY(name)	".globl " C(name) "\n" C(name) ":"
#else
# define ENTRY(name)	C(name) ":"
#endif

#define BAD_ADDR	((void *) (ptrdiff_t) -1)

__attribute__ ((noinline)) void *
pc(void)
{
	return __builtin_return_address(0);
}

const void *segv_pc, *segv_addr;
sigjmp_buf env;

int
segv(const void *pc, const void *addr, void *arg)
{
	printf("segv: pc=%p, addr=%p\n", pc, addr);

	segv_pc = pc;
	segv_addr = addr;

	siglongjmp(env, 1);
}

static int
test_longjmp(void)
{
	if (!sigsetjmp(env, 1)) {
		fault(segv, NULL);
		(void) *(volatile char *) NULL;
	}

	return 0;
}

void
test_pc_fault(void);

static int
test_pc(void)
{
	if (!sigsetjmp(env, 1)) {
		fault(segv, NULL);

#if defined(__aarch64__)
		asm(
			"mov	x7, xzr\n"
			"bl	" C(test_pc_fault) "\n"
			".p2align 2\n"
ENTRY(test_pc_fault) "\n"
			"ldr	x7, [x7]\n"
			: : : "x7"
		);
#elif defined(__amd64__) || defined(__x86_64__)
		asm(
			"xorq	%%rax, %%rax\n"
ENTRY(test_pc_fault) "\n"
			"movq	(%%rax), %%rax\n"
			: : : "rax"
                );
#elif defined(__i386__)
		asm(
			"xorl	%%eax, %%eax\n"
ENTRY(test_pc_fault) "\n"
			"movl	(%%eax), %%eax\n"
			: : : "eax"
                );
#else
# error "Unsupported architecture"
#endif
	}

	return segv_pc == test_pc_fault ? 0 : -1;
}

static int
test_addr(void)
{
	if (!sigsetjmp(env, 1)) {
		fault(segv, NULL);
		(void) ((volatile char *) NULL)[0x10];
	}

	return segv_addr == (void *) 0x10 ? 0 : -1;
}

static int
test_write(void)
{
	void *addr;
	long page_size = sysconf(_SC_PAGESIZE);

	addr = mmap(NULL, page_size, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (addr == NULL) {
		perror("mmap");
		return -1;
	}

	if (!sigsetjmp(env, 1)) {
		fault(segv, NULL);
		*(volatile char *) addr = 42;
	}

	return segv_addr == addr ? 0 : -1;
}

static int
test_unmapped(void)
{
	void *addr;
	long page_size = sysconf(_SC_PAGESIZE);

	addr = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (addr == NULL) {
		perror("mmap");
		return -1;
	}

	if (munmap(addr, page_size) != 0) {
		perror("munmap");
		return -1;
	}

	if (!sigsetjmp(env, 1)) {
		fault(segv, NULL);
		(void) *(volatile char *) addr;
	}

	return segv_addr == addr ? 0 : -1;
}

static int
test_bad(void)
{
	if (!sigsetjmp(env, 1)) {
		fault(segv, NULL);
		(void) *(volatile int *) BAD_ADDR;
	}

	return segv_addr == BAD_ADDR ? 0 : -1;
}

static int
test_unaligned(void)
{
	if (!sigsetjmp(env, 1)) {
		fault(segv, NULL);
		(void) ((volatile char *) NULL)[1];
	}

	return segv_addr == (void *) 1 ? 0 : -1;
}

int
retry_segv(const void *pc, const void *addr, void *arg)
{
	printf("retry_segv: pc=%p, addr=%p\n", pc, addr);

	if (mprotect((void *) addr, sysconf(_SC_PAGESIZE), PROT_READ | PROT_WRITE) != 0) {
		perror("mprotect");
		exit(1);
	}

	return 1;
}

static int
test_retry(void)
{
	void *addr;

	addr = mmap(NULL, sysconf(_SC_PAGESIZE), PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (addr == NULL) {
		perror("mmap");
		return -1;
	}

	if (!sigsetjmp(env, 1)) {
		fault(retry_segv, NULL);
		*(volatile char *) addr = 42;
	}

	return *(volatile char *) addr == 42 ? 0 : -1;
}

static struct {
	const char	*name;
	int		(*fun)(void);
} tests[] = {
	{ "longjmp",	test_longjmp },
	{ "pc",		test_pc },
	{ "addr",	test_addr },
	{ "write",	test_write },
	{ "unmapped",	test_unmapped },
	{ "bad",	test_bad },
	{ "unaligned",	test_unaligned },
	{ "retry",	test_retry },
};

int
main(int argc, const char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s name\n\n", argv[0]);

		fprintf(stderr, "Available tests:\n");
		for (unsigned int i = 0; i < nitems(tests); i++)
			fprintf(stderr, "\t%s\n", tests[i].name);

		return 1;
	}

	for (unsigned int i = 0; i < nitems(tests); i++) {
		if (strcmp(tests[i].name, argv[1]) == 0) {
			if (tests[i].fun() != 0) {
				fprintf(stderr, "%s: test `%s` failed\n", argv[0], argv[1]);
				return 1;
			}

			printf("%s: test `%s' successful\n", argv[0], argv[1]);

			return 0;
		}
	}

	fprintf(stderr, "%s: unknown test `%s'\n", argv[0], argv[1]);

	return 1;
}
