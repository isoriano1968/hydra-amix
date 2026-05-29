.c.o:
	$(CC) -c -O $(CFLAGS) -I../.. -I../inc $<

OBJ	= \
	  servant.o\
	  support.o

exp:    $(OBJ)
	ld -r -o exp $(OBJ)

clean:
	-rm -f $(OBJ) exp
