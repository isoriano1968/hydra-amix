# Makefile for local kernel modules
# Put your device drivers (either .o or .c) in this directory
# and add to OBJS below.


.c.o:
	$(CC) -c -O $(CFLAGS) -DSVR40 $<


OBJS = empty.o res.o


exp: $(OBJS)
	ld -r -o exp $(OBJS)

clean:
	rm -f $(OBJS) exp

