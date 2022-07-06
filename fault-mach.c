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

/*
 * See: Mac OS X Internals: A Systems Approach
 */

#include <pthread.h>

#include <mach/mach.h>
#include <mach/exc.h>
#include <mach/message.h>
#include <mach/mig.h>

#if defined(__aarch64__)
# define NATIVE_THREAD_STATE		ARM_THREAD_STATE
# define NATIVE_THREAD_STATE_COUNT	ARM_UNIFIED_THREAD_STATE_COUNT
typedef arm_unified_thread_state_t native_thread_state_t;

# define NATIVE_EXCEPTION_STATE		ARM_EXCEPTION_STATE64
# define NATIVE_EXCEPTION_STATE_COUNT	ARM_EXCEPTION_STATE64_COUNT
typedef arm_exception_state64_t native_exception_state_t;

# define PC(ts)		((uintptr_t) __darwin_arm_thread_state64_get_pc_fptr((ts).ts_64))
# define SET_PC(ts,pc)	__darwin_arm_thread_state64_set_pc_fptr((ts).ts_64, (pc))
# define SP(ts)		__darwin_arm_thread_state64_get_sp((ts).ts_64)
# define SET_SP(ts,sp)	__darwin_arm_thread_state64_set_sp((ts).ts_64, (sp))
# define LR(ts)		((uintptr_t) __darwin_arm_thread_state64_get_lr_fptr((ts).ts_64))
# define SET_LR(ts,lr)	__darwin_arm_thread_state64_set_lr_fptr((ts).ts_64, (lr))
# define ARG(ts,a)	((ts).ts_64.__x[(a)])
# define SET_ARG(ts,a,v) ((ts).ts_64.__x[(a)] = (v))

# define ADDR(es)	((void *) (es).__far)

# define STACK_ALIGN	16
#elif defined(__amd64__) || defined(__x86_64__)
# define NATIVE_THREAD_STATE		x86_THREAD_STATE
# define NATIVE_THREAD_STATE_COUNT	x86_THREAD_STATE_COUNT
typedef x86_thread_state_t native_thread_state_t;

# define NATIVE_EXCEPTION_STATE		x86_EXCEPTION_STATE
# define NATIVE_EXCEPTION_STATE_COUNT	x86_EXCEPTION_STATE_COUNT
typedef x86_exception_state_t native_exception_state_t;

# define PC(ts)		((ts).uts.ts64.__rip)
# define SET_PC(ts,pc)	((ts).uts.ts64.__rip = (uintptr_t) (pc))
# define SP(ts)		((ts).uts.ts64.__rsp)
# define SET_SP(ts,sp)	((ts).uts.ts64.__rsp = (uintptr_t) (sp))
# define ARG0(ts)	((ts).uts.ts64.__rdi)
# define ARG1(ts)	((ts).uts.ts64.__rsi)
# define ARG(ts,a)	ARG ## a(ts)
# define SET_ARG(ts,a,v) (ARG ## a(ts) = (v))

# define ADDR(es)	((void *) (es).ues.es64.__faultvaddr)

# define STACK_ALIGN	16
#endif

#define EXC_SEGV	2405

#if __MigPackStructs
# pragma pack(push, 4)
#endif

typedef struct {
	mach_msg_header_t		 Head;

	mach_msg_body_t			 msgh_body;
	mach_msg_port_descriptor_t	 thread;
	mach_msg_port_descriptor_t	 task;

	NDR_record_t			 NDR;
	exception_type_t		 exception;
	mach_msg_type_number_t		 codeCnt;
	int64_t				 code[2];
} exc_request_t;

typedef struct {
	mach_msg_header_t		 Head;
	NDR_record_t			 NDR;
	kern_return_t			 RetCode;
} exc_reply_t;

#if __MigPackStructs
# pragma pack(pop)
#endif

static int
hook_called = 0;

static void
push(native_thread_state_t *ts, const void *buf, size_t len)
{
	uintptr_t sp = SP(*ts) - len;
	memcpy((uint64_t *) sp, buf, len);
	SET_SP(*ts, sp);
}

void
trampoline_return(void);

static __attribute__ ((noreturn)) void
trampoline(native_thread_state_t *ts, native_exception_state_t *es)
{
	int ok = 0;
	if (curact.fa_fun != NULL && curact.fa_fun(
	    (const void *) PC(*ts), (const void *) ADDR(*es),
	    curact.fa_arg)) {
		ok = 1;
	}

#if defined(__aarch64__)
	asm (
		"mov	x0, %x0\n"
		"mov	x1, %x1\n"
		"mov	x7, xzr\n"
"_trampoline_return:\n"
		"ldr	x7, [x7]\n"
		: : "r" (ok), "r" (ts)
	);
#elif defined(__amd64__) || defined(__x86_64__)
	asm (
		"movq	%q0, %%rdi\n"
		"movq	%q1, %%rsi\n"
		"xorq	%%rax, %%rax\n"
"_trampoline_return:\n"
		"movq	(%%rax), %%r9\n"
		: : "q" (ok), "q" (ts)
	);
#endif

	for (;;);
}

static kern_return_t
handle_segv(mach_port_t exception_port, mach_port_t thread,
    mach_port_t task, exception_type_t exception,
    mach_exception_data_t code, mach_msg_type_number_t codeCnt)
{
	kern_return_t kr;
	native_thread_state_t ts;
	native_exception_state_t es;
	mach_msg_type_number_t ts_count = NATIVE_THREAD_STATE_COUNT,
	    es_count = NATIVE_EXCEPTION_STATE_COUNT;

	kr = thread_get_state(thread, NATIVE_THREAD_STATE,
	    (void *) &ts, &ts_count);
	if (kr != KERN_SUCCESS)
		return kr;

	kr = thread_get_state(thread, NATIVE_EXCEPTION_STATE,
	    (void *) &es, &es_count);
	if (kr != KERN_SUCCESS)
		return kr;

	if ((void *) PC(ts) == trampoline_return) {
		ts = *(native_thread_state_t *) ARG(ts, 1);
	} else {
		push(&ts, (void *) &ts, sizeof(ts));
		SET_ARG(ts, 0, SP(ts));

		push(&ts, (void *) &es, sizeof(es));
		SET_ARG(ts, 1, SP(ts));

		SET_SP(ts, SP(ts) - (SP(ts) & STACK_ALIGN - 1));
#if defined(__amd64__) || defined(__x86_64__)
		/* on x86-64, calling a function pushes the return address
		 * onto the stack; take that into account here otherwise the
		 * stack becomes misaligned. */
		SET_SP(ts, SP(ts) - sizeof(void *));
#endif

		SET_PC(ts, trampoline);
	}

	return thread_set_state(thread, NATIVE_THREAD_STATE,
	    (void *) &ts, ts_count);
}

static boolean_t
demux(mach_msg_header_t *request, mach_msg_header_t *reply)
{
	reply->msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(request->msgh_bits), 0);
	reply->msgh_remote_port = request->msgh_remote_port;
	reply->msgh_local_port = MACH_PORT_NULL;
	reply->msgh_id = request->msgh_id + 100;
	reply->msgh_reserved = 0;

	if (request->msgh_id == EXC_SEGV) {
		exc_request_t *erequest = (exc_request_t *) request;
		exc_reply_t *ereply = (exc_reply_t *) reply;

		ereply->Head.msgh_size = (mach_msg_size_t) sizeof(*ereply);
		ereply->NDR = NDR_record;
		ereply->RetCode = handle_segv(
		    erequest->Head.msgh_remote_port,
		    erequest->thread.name, erequest->task.name,
		    erequest->exception, erequest->code,
		    erequest->codeCnt);

		return TRUE;
	} else {
		mig_reply_error_t *error = (mig_reply_error_t *) reply;

		error->Head.msgh_size = (mach_msg_size_t) sizeof(*error);
		error->NDR = NDR_record;
		error->RetCode = MIG_BAD_ID;

		return FALSE;
	}
}

static void *
handler_thread(void *arg)
{
	mach_port_t segv_port = (mach_port_t) (uintptr_t) arg;
	kern_return_t kr = KERN_SUCCESS;

	mach_msg_server(demux, sizeof(exc_request_t), segv_port,
	    MACH_MSG_TIMEOUT_NONE);

	return NULL;
}

static int
map_error(kern_return_t err)
{
	switch (err) {
	case KERN_SUCCESS:	return 0;
	case KERN_NO_SPACE:	errno = EMFILE;		break;
	case KERN_INVALID_CAPABILITY: errno = EINVAL;	break;
	case KERN_UREFS_OVERFLOW: errno = EOVERFLOW;	break;
	case KERN_NAME_EXISTS:
	case KERN_RIGHT_EXISTS:	errno = EEXIST;		break;
	default:		errno = ENOTSUP;	break;
	}

	return -1;
}

static int
hook_fault(void)
{
	kern_return_t kr = KERN_SUCCESS;
	mach_port_t self = mach_task_self(), segv_port;
	pthread_t thread;
	int err;

	if (hook_called)
		return hook_called < 0 ? -1 : 0;

	kr = mach_port_allocate(self, MACH_PORT_RIGHT_RECEIVE,
	    &segv_port);
	if (kr != KERN_SUCCESS)
		goto fail;

	kr = mach_port_insert_right(self, segv_port, segv_port,
	    MACH_MSG_TYPE_MAKE_SEND);
	if (kr != KERN_SUCCESS)
		goto fail;

	kr = task_set_exception_ports(self, EXC_MASK_BAD_ACCESS,
	    segv_port, EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES,
	    NATIVE_THREAD_STATE);
	if (kr != KERN_SUCCESS)
		goto fail;

	err = pthread_create(&thread, NULL, handler_thread,
	    (void *) (uintptr_t) segv_port);
	if (err != 0) {
		errno = -err;
		return hook_called = -1;
	}

	pthread_detach(thread);

	hook_called = 1;
	return 0;

fail:
	mach_port_deallocate(self, segv_port);

	hook_called = -1;
	return map_error(kr);
}

static int
unhook_fault(void)
{
	return 0;
}
