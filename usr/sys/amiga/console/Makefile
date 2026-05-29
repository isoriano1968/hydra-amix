CFLAGS	= -O -I../.. -I../inc -D_KERNEL -DSVR40 $(DEFS)
ASFLAGS	= -m

OBJ	= \
	kio.o\
	c0.o\
	c1.o\
	c3.o\
	memory.o\
	scrdev.o\
	screen.o\
	usa1.kmap.o\
	font.o\
	topaz.o

exp :	$(OBJ)
	ld -r -o exp $(OBJ)

clean :
	-rm -f $(OBJ) exp
