.c.o:
	$(CC) -DNODEBUG -c -O $(CFLAGS) -I../.. -I../inc $<

OBJ	= flop.o flopf.o flopf.dos.o floph.o

exp:    $(OBJ)
	ld -r -o exp $(OBJ)

$(OBJ):	hstyle.h flopf.h flopf.dos.h

clean:
	-rm -f $(OBJ) exp
