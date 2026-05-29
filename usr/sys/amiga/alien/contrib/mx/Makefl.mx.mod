# jck additions to /usr/sys/amiga/driver/Makefile for MAX-125 mx driver
# (lines marked with "jck" added)

CFLAGS	= -O -D_KERNEL -DSVR40 -DSVR4 -I../.. -I../inc

OBJ	= \
	  acia.o\
	  amiga.o\
	  ben.o\
	  bb.o\
	  cl.o\
	  dummy.o\
	  hd.o\
	  jb.o\
	  machid.o\
	  par.o\
	  ram.o\
	  ql.o\
	  sl.o\
	  slip.o\
	  tiga.o \
	  audio.o \
	  mx/exp \			# jck MAX-125 mx driver
	  aen/exp # kdb/exp

exp :	$(OBJ)
	ld -r -o exp $(OBJ)

mx/exp :				# jck MAX-125 mx driver
	cd mx; $(MAKE)			# jck MAX-125 mx driver

aen/exp :
	cd aen; $(MAKE)

kdb/exp :
	cd kdb; $(MAKE)

clean :
	-rm -f $(OBJ) exp ql6502.h
	cd as65; $(MAKE) clean
	cd ql65; $(MAKE) clean
	cd aen; $(MAKE) clean
	cd kdb; $(MAKE) clean
	cd mx; $(MAKE) clean		# jck MAX-125 mx driver

ql.o : ql6502.h

ben.o : ben.h

ql6502.h :
	cd ql65; $(MAKE)
	ln ql65/ql6502.h ql6502.h
