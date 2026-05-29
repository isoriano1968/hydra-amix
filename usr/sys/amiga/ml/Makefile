
OBJS = ttrap.o vec.o syms.o


exp: $(OBJS)
	ld -r -o exp $(OBJS)

clean:
	rm -f exp $(OBJS)
