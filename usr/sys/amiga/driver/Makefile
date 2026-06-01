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
	  aen/exp \
	  hydra/exp # kdb/exp

exp :	$(OBJ)
	ld -r -o exp $(OBJ)

aen/exp :
	cd aen; $(MAKE)

hydra/exp :
	cd hydra; $(MAKE)

kdb/exp :
	cd kdb; $(MAKE)

clean :
	-rm -f $(OBJ) exp ql6502.h
	cd as65; $(MAKE) clean
	cd ql65; $(MAKE) clean
	cd aen; $(MAKE) clean
	cd hydra; $(MAKE) clean
	cd kdb; $(MAKE) clean

ql.o : ql6502.h

ben.o : ben.h

ql6502.h :
	cd ql65; $(MAKE)
	ln ql65/ql6502.h ql6502.h
