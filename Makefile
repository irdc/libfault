CFLAGS	= -O2
SRCS	= fault.c
OBJS	= $(SRCS:.c=.o)

all: libfault.a test

test: test.c libfault.a
	$(CC) $(CFLAGS) -o $@ test.c libfault.a

libfault.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

clean:
	rm -f test $(OBJS)
