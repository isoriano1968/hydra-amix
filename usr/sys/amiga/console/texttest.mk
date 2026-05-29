OBJS = texttest.o font.o sunfont.o
ASFLAGS=-m
texttest : $(OBJS)
	$(CC) $(CFLAGS) -o texttest $(OBJS)
