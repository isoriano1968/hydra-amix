CFLAGS	= -O -D_KERNEL -DSVR40 -DSVR4 -I../../.. -I../../inc -I../aen

OBJ	= kdebug.o rkdb.o rkd.o en.o hooks.o

exp:    $(OBJ)
	ld -r -o exp $(OBJ)

clean:
	-rm -f $(OBJ) exp

kdebug.o: ../aen/aen.h ../aen/aenuser.h kdebug.h
rkdb.o: ../aen/aen.h ../aen/aenuser.h kdebug.h
rkd.o: ../aen/aen.h ../aen/aenuser.h kdebug.h
eno.o: ../aen/aen.h ../aen/aenuser.h kdebug.h ../aen/lance.h
