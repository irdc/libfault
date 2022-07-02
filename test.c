#include "fault.h"

#include <stdio.h>
#include <unistd.h>
#include <setjmp.h>

const void *segv_pc, *segv_addr;
sigjmp_buf env;

int
segv(const void *pc, const void *addr, void *arg)
{
	static int count = 0;

	write(1, "segv\n", 5);
	printf("%p\n", pc);
	segv_pc = pc;
	segv_addr = addr;

	if (++count == 5)
		siglongjmp(env, 1);

	return 1;
}

void *
pc(void)
{
	return __builtin_return_address(0);
}

int
main(int argc, const char *argv[])
{
	printf("%p\n", pc());

	if (!sigsetjmp(env, 1)) {
		fault(segv, NULL);
		((char *) NULL)[0x13] = 42;
	}

	printf("%p %p\n", segv_pc, segv_addr);

	return 0;
}
