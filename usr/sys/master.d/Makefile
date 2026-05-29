#
# The files *.c in this directory are constructed from the original
# master.d files that are supposed to be run through mkboot.  For
# the initial version of the kernel these files are made by hand
# conversion.  Eventually, the auto-configure code will be converted
# to M68020.
#

.c.o:
	$(CC) -c -O $(CFLAGS) $<

OBJS =  \
	arp.o \
	disp.o \
	filesys.o \
	gentty.o \
	hrt.o \
	icd.o \
	ip.o \
	kernel.o \
	kmacct.o \
	krpc.o \
	llcloop.o \
	log.o \
	msg.o \
	opts.o \
	prf.o \
	ptem.o \
	ptm.o \
	rt.o \
	sad.o \
	sem.o \
	shm.o \
	sockmod.o \
	sp.o \
	sxt.o \
	tcp.o \
	timod.o \
	tirdwr.o \
	ts.o \
	udp.o \
	xt.o \
	stubs.o

exp:    $(OBJS)
	ld -r -o exp $(OBJS)

kernel.o:	kernel.h

sem.o:	kernel.h

filesys.o:	filesys.c s5.c bfs.c ufs.c proc.c fifofs.c xnamfs.c specfs.c rfs.c namefs.c

clean:
	-rm -f $(OBJS) exp

