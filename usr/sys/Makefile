SHELL	= /bin/sh
CFLAGS	= -DSYSV -D_KERNEL -I..

#
#	Pick one:
#LINKARGS = -dn -M $(PLATFORM)/config/$(MACHINE).mapfile
#CC	= gcc 
LINKARGS = -r # -z defs
CC	= gcc 

KERNEL	= unix
PLATFORM= amiga
MACHINE	= reloc
#BOOTDIR = $(PLATFORM)/boot
BOOTDIR	= $(ROOT)/stand
CONFIGFILE=$(ROOT)/stand/CONFIG

OBJS	= ml/exp $(PLATFORM)/exp io/exp os/exp fs/exp master.d/exp vm/exp \
	  exec/exp disp/exp ktli/exp klm/exp rpc/exp des/exp netinet/exp \
	  local/exp

force :
	rm -f $(KERNEL)
	$(MAKE) $(MACHINE)$(KERNEL) KERNEL=$(KERNEL) PLATFORM=$(PLATFORM) MACHINE=$(MACHINE) CC="$(CC)" CONFIGFILE=$(CONFIGFILE)

all: $(MACHINE)$(KERNEL)

$(PLATFORM)/config/$(KERNEL).o : $(CONFIGFILE)
	rm -f $(PLATFORM)/config/$(KERNEL).o
	cd $(PLATFORM)/config; $(MAKE) $(KERNEL).o CONFIGFILE=$(CONFIGFILE)

bootfloppy : force
	(KERNEL=`pwd`/$(MACHINE)$(KERNEL); cd $(BOOTDIR); $(MAKE) floppy KERNEL=$${KERNEL})

$(MACHINE)$(KERNEL) : $(KERNEL)
	@if nm -u $(KERNEL) | egrep -v '^$|Symbols from|etext|edata|end' ; then \
		echo "unresolved symbols" ; \
		exit 1; \
	else \
		exit 0; \
	fi
	[ ! -f $(MACHINE)$(KERNEL) ] || mv $(MACHINE)$(KERNEL) OLD$(MACHINE)$(KERNEL)
	ln $(KERNEL) $(MACHINE)$(KERNEL)

$(KERNEL) : $(PLATFORM)/config/$(KERNEL).o
	@echo Begin at `date`
	@echo CC=$(CC) PLATFORM=$(PLATFORM) MACHINE=$(MACHINE)
	@cd master.d; $(MAKE) CFLAGS="$(CFLAGS)" 
	@cd $(PLATFORM); $(MAKE) CFLAGS="$(CFLAGS)"
	@cd vm; $(MAKE) CFLAGS="$(CFLAGS)"
	@cd exec; $(MAKE) CFLAGS="$(CFLAGS)"
	@cd netinet; $(MAKE) CFLAGS="$(CFLAGS)"
	@cd os; $(MAKE) CFLAGS="$(CFLAGS)"
	@cd fs; $(MAKE) CFLAGS="$(CFLAGS)"
	@cd ml; $(MAKE) CFLAGS="$(CFLAGS)"
	@cd disp; $(MAKE) CFLAGS="$(CFLAGS)"
	@cd ktli; $(MAKE) CFLAGS="$(CFLAGS)"
	@cd klm; $(MAKE) CFLAGS="$(CFLAGS)"
	@cd rpc; $(MAKE) CFLAGS="$(CFLAGS)"
	@cd des; $(MAKE) CFLAGS="$(CFLAGS)"
	@cd io; $(MAKE) CFLAGS="$(CFLAGS)"
	@cd local; $(MAKE) CFLAGS="$(CFLAGS)"
	ld -o $@ $(LINKARGS) $(OBJS) $(PLATFORM)/config/$(KERNEL).o
	chmod +r $@
	@echo ""
	@if nm -u $@ | egrep -v '^$|Symbols from|etext|end|edata' ; then \
		echo "Unresolved Symbol(s)!"; \
		exit 1; \
	else \
		exit 0; \
	fi
	@echo Finish at `date`

clean :
	cd $(PLATFORM); $(MAKE) clean
	cd os; $(MAKE) clean
	cd io; $(MAKE) clean
	cd fs; $(MAKE) clean
	cd ml; $(MAKE) clean
	cd vm; $(MAKE) clean
	cd exec; $(MAKE) clean
	cd disp; $(MAKE) clean
	cd ktli; $(MAKE) clobber
	cd klm; $(MAKE) clobber
	cd rpc; $(MAKE) clobber
	cd des; $(MAKE) clean
	cd netinet; $(MAKE) clean
	cd master.d; $(MAKE) clean
	cd local; $(MAKE) clean

clean_exp :
	-cd $(PLATFORM)/config; $(MAKE) CONFIGFILE=$(CONFIGFILE) clean
	-cd $(PLATFORM); rm -f exp */exp
	-cd fs; rm -f exp
	-cd master.d; rm -f exp
	-cd local; rm -f exp

clobber : clean
	-rm -f $(KERNEL) $(MACHINE)$(KERNEL)

install : all
	[ ! -f $(ROOT)/stand/NEW$(MACHINE)$(KERNEL) ] || rm $(ROOT)/stand/NEW$(MACHINE)$(KERNEL)
	cp $(MACHINE)$(KERNEL) $(ROOT)/stand/NEW$(MACHINE)$(KERNEL)
	[ ! -f $(ROOT)/stand/unix ] || mv $(ROOT)/stand/unix $(ROOT)/stand/OLDunix
	cd $(ROOT)/stand; ln NEW$(MACHINE)$(KERNEL) unix; $(MAKE)

release :
	sh $(PLATFORM)/release/Install
