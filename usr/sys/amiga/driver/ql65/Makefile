

# NCC is "native CC" -- maybe not the same as CC

.c.o:
	$${NCC-cc} $${NCFLAGS-"-O"} -c $<

.c:
	$${NCC-cc} $${NCFLAGS-"-O"} $< -o $@ $${NLDFLAGS}

NCFLAGS	= -O -I../../..
LDFLAGS	= -s


ql6502.h: startup.m4 dev.m4 ql6502.m4 crunch xtoc ../as65/as65
	m4 -s ql6502.m4 | ../as65/as65 | ./crunch | ./xtoc ql6502code >$@

../as65/as65:
	cd ../as65; $(MAKE)

clean:
	rm -f ql6502.h crunch xtoc
