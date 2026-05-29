CFLAGS	= -O -D_KERNEL -DSVR40 -DSVR4 -I../../.. -I../../inc $(KDB)

OBJ	= mx.o

exp:	$(OBJ)
	ld -r -o exp $(OBJ)

clean:
	-rm -f $(OBJ) exp

