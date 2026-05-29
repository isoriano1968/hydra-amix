#
#ident	"%W%"
#
include $(CONFIGFILE)

O = c0s1unix.o c6s1unix.o fd0unix.o unix.o

CFLAGS = -DSYSV -D_KERNEL -I../.. -DROOTDEV='$(ROOTDEV)' \
	-DSWAPDEV='"$(SWAPPART)"' -DNSWAP='$(SWAPSIZE)'    \
	-DSWPLO=0 \
	-DDUMPDEV='$(DUMPDEV)' \
	-DCHAR_SPECIAL='$(CHAR_SPECIAL)' \
	-DDO_SYSDUMP='$(DO_SYSDUMP)' \
	-DNO_INTERACTIVE='$(NO_INTERACTIVE)'

$(O) : $(CONFIGFILE)


clean:
	rm -f *.o

