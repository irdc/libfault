CFLAGS	= -O2
SRCS	= fault.c
OBJS	= $(SRCS:.c=.o)

all: libfault.a test tests

tests: test
	./test longjmp
	./test pc
	./test sp
	./test addr
	./test write
	./test unmapped
	./test bad
	./test unaligned
	./test retry

test: test.o libfault.a
	$(CC) $(CFLAGS) -o $@ test.o libfault.a

libfault.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

clean:
	rm -f test test.o $(OBJS) libfault.a
