O	= ml/exp console/exp alien/exp driver/exp floppy/exp kernel/exp

all :
	@for dir in $(O); do ( cd `dirname $$dir`; $(MAKE) ); done
	@$(MAKE) exp

$(O) :

exp : $(O)
	ld -r -o exp $(O)

clean :
	for dir in $(O); do ( cd `dirname $$dir`; $(MAKE) clean ); done
	rm -f exp
	cd config; rm -f *.o
	cd boot; $(MAKE) clobber
