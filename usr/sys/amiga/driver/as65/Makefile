

# NCC is "native CC" -- maybe not the same as CC

.c.o:
	$${NCC-cc} $${NCFLAGS-"-O"} -c $<

OBJECTS	= main.o lex.o as65.o local.o symbol.o seg.o code.o expr.o croak.o

as65: $(OBJECTS)
	$${NCC-cc} $${NCFLAGS-"-O"} -s -o $@ $(OBJECTS) $${NLDFLAGS}
clean:
	rm -f *.o y.tab.h y.tab.c as65 as65.y

as65.o: as65.proto Optab
	sed <as65.proto 1,/INSERTION1/!d >as65.y
	sed <Optab -n '/^-/!s/\([^ 	]*\).*/%token		\1/p' >>as65.y
	sed <as65.proto /INSERTION1/,/INSERTION2/!d >>as65.y
	awk <Optab -f op.awk | sed 's/^/		| /' >>as65.y
	sed <as65.proto 1,/INSERTION2/d >>as65.y
	yacc -d as65.y
	$${NCC-cc} $${NCFLAGS-"-O"} -c y.tab.c
	rm y.tab.c as65.y
	mv y.tab.o $@
lex.o: as65.o lex.proto
	sed <lex.proto 1,/INSERTION1/!d >lex.c
	sed <Optab -f lex.sed >>lex.c
	sed <lex.proto 1,/INSERTION1/d >>lex.c
	$${NCC-cc} $${NCFLAGS-"-O"}  -c lex.c
	rm lex.c

as65.o:		as65.h expr.h
code.o:		as65.h expr.h seg.h
croak.o:	as65.h
expr.o:		as65.h expr.h
lex.o:		as65.h expr.h
local.o:	as65.h
main.o:		as65.h seg.h
seg.o:		as65.h seg.h code.h
symbol.o:	as65.h seg.h
