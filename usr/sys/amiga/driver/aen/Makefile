CFLAGS	= -O -D_KERNEL -DSVR40 -DSVR4 -I../../.. -I../../inc $(KDB)

OBJ	= aen.o

exp:	$(OBJ)
	ld -r -o exp $(OBJ)

clean:
	-rm -f $(OBJ) exp

aen.o : aen.h lance.h aenuser.h ../kdb/kdebug.h
