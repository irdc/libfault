CFLAGS	= -O2
SRCS	= test.c fault.c
OBJS	= $(SRCS:.c=.o)

test: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

clean:
	rm -f test $(OBJS)
