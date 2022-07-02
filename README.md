# `libfault` - Portable fault handling in user space

Motivating example:

```
sigjmp_buf env;

int
handle_fault(const void *pc, const void *addr, void *arg)
{
    siglongjmp(env, 1);
}

/* ... */

if (!sigsetjmp(env, 1)) {
    fault(handle_fault, (void *) &arg);
    *(int *) NULL = 42;
}
```

This will, on all supported targets, transfer control to `handle_fault`, passing in the program counter and fault address. If the fault handler returns non-zero, the faulting instruction is retried; otherwise, control is passed to the system fault handler (which will usually generate a core dump and terminate the program).

If desired, `siglongjmp` can be used to jump out of the fault handler.

## Targets

| OS           | CPU      | Tested (version)        |
| ------------ | -------- | ----------------------- |
| DragonFlyBSD | x86_64   |                         |
| FreeBSD      | aarch64  |                         |
| FreeBSD      | i386     |                         |
| FreeBSD      | riscv    |                         |
| FreeBSD      | x86_64   |                         |
| OpenBSD      | aarch64  | :white_check_mark: 7.1  |
| OpenBSD      | i386     |                         |
| OpenBSD      | riscv    |                         |
| OpenBSD      | x86_64   |                         |
| Linux        | aarch64  |                         |
| Linux        | i386     |                         |
| Linux        | riscv    |                         |
| Linux        | x86_64   | :white_check_mark: 5.16 |
| MacOS        | aarch64  | :white_check_mark: 12.4 |
| MacOS        | x86_64   | :white_check_mark: 12.4 |
| NetBSD       | aarch64  |                         |
| NetBSD       | i386     |                         |
| NetBSD       | riscv    |                         |
| NetBSD       | x86_64   |                         |
