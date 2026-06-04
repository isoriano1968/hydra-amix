CC=gcc
CFLAGS=-g
LIBS=

all: hya

hya: hya.o
	$(CC) $(CFLAGS) hya.o -o hya $(LIBS)

clean:
	rm -f *.o hya core

clobber: clean

hya.o: hydrauser.h

install: hya
	cpset hya $(ROOT)/usr/amiga/bin 755 amiga amiga
