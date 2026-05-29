#
# Patchlevel: Makefile_1.1
#
CC =		/usr/public/bin/gcc
GCCFLAGS =	-fpcrel -fno-function-cse
SHELL =		/bin/sh
AMIGAINC =	-I/usr/amigados/include
OFLAG =		-O
CFLAGS =	$(OFLAG) $(AMIGAINC) $(DBUG) $(GCCFLAGS)
KERNEL =	need-KERNEL
#DBUG =		-DDBUG
#LIBDBUG =	-ldbug
SDBFLAG =	-g
BOOT1FILES =	boot1csu.o boot1.o libboot.a
BOOT2FILES =	boot2csu.o boot2.o relb.o decompressb.o libboot.a
IMAGEFILES =	boot1.boot boot2.iblk boot2.boot $(KERNEL).iblk $(KERNEL)
BOOTIMAGE =	$(KERNEL).bootimage

LIBOBJ =	Alert.o AllocMem.o BlinkLED.o \
		DisplayAlert.o DoIO.o FreeMem.o OpenLibrary.o \
		Supervisor.o copyit.o reloc.o bind.o strcpy.o strlen.o \
		chksum.o streq.o memset.o streqn.o memcpy.o sumbytes.o \
		message.o itoa.o Disable.o Enable.o Debug.o alloc.o FindConfigDev.o

RELTESTOBJ =	reltest.o relx.o streq.o streqn.o

all :		boottest.boot boot1.boot boot2.boot reltest makeiblk elf2brel

#  Boottest is a simple test that generates a boot image that should exactly
#  match the 1.3 Workbench boot blocks.

boottest.boot :	boottest elf2boot
		./elf2boot -s 1K <boottest >boottest.boot

boottest :	boottest.o
		$(LD) -dn -o boottest -M mapfile boottest.o

boottest.o :	boottest.s
		$(AS) boottest.s

#  The first level bootstrap.

boot1.boot :	boot1 elf2boot
		./elf2boot -s 1K <boot1 >boot1.boot

boot1 :		$(BOOT1FILES)
		$(LD) -dn -o boot1 -M mapfile $(BOOT1FILES)

boot1.o :	boot1.c infoblock.h

#  The second level bootstrap.

boot2.boot :	boot2 elf2boot
		./elf2boot <boot2 >boot2.boot

boot2 :		$(BOOT2FILES)
		$(LD) -dn -o boot2 -M mapfile $(BOOT2FILES)

boot2.o :	boot2.c bootdata.h bootinfo.h infoblock.h

boot2.iblk :	makeiblk boot2.boot
		./makeiblk boot2.boot $@

#  The makeiblk program is used to make the information blocks.
#  Elf2boot is used to extract the text and initialized data
#  sections from an ELF file.

makeiblk :	makeiblk.c bootdata.h infoblock.h decompressx.o
		$(CC) $(CFLAGS) -o $@ makeiblk.c decompressx.o

elf2boot.o :	elf2boot.c usermsg.h

usermsg.o :	usermsg.c usermsg.h

elf2boot :	elf2boot.o usermsg.o
		$(CC) $(LDFLAGS) -o $@ elf2boot.o usermsg.o

elf2brel :	elf2brel.o
		$(CC) $(LDFLAGS) -o $@ elf2brel.o $(LIBDBUG)

libboot.a :	$(LIBOBJ)
		$(AR) rv $@ $?

#  Boot images consist of the concatentation of their various
#  components.  We allow keeping several around for testing

$(BOOTIMAGE) :	$(IMAGEFILES)
		cat $(IMAGEFILES) >$@

$(KERNEL).iblk:	makeiblk $(KERNEL)
		./makeiblk $(KERNEL) $@

#  Make a boot floppy

floppy :	$(KERNEL) elf2brel makeiblk boot1.boot boot2.boot
		./elf2brel < $(KERNEL) | compress > TmP
		( cat boot1.boot; \
		  ./makeiblk boot2.boot; \
		  cat boot2.boot; \
		  ./makeiblk TmP; \
		  cat TmP ) | dd of=/dev/dsk/fd0
		rm TmP

#  Install $(KERNEL) in boot partition

bootpart :	$(KERNEL) makeiblk boot1.boot boot2.boot
		( cat boot1.boot; \
		  ./makeiblk boot2.boot; \
		  cat boot2.boot; \
		  ./makeiblk $(KERNEL); \
		  cat $(KERNEL) ) | dd of=$(BOOTPART)

#  Up to three floppies can be made from a bootimage, just say
#  "make floppy1" for example.

floppy1 :	$(BOOTIMAGE)
		dd if=$(BOOTIMAGE) of=/dev/dsk/fd0 count=1760

floppy2 :	$(BOOTIMAGE)
		dd if=$(BOOTIMAGE) of=/dev/dsk/fd0 skip=1760 count=1760

floppy3 :	$(BOOTIMAGE)
		dd if=$(BOOTIMAGE) of=/dev/dsk/fd0 skip=3520 count=1760

#  Some targets to clean up the work area.

clean :
		rm -f *.[oa] *~ core boot1.s boot2.s

clobber :	clean
		rm -f *.boot boottest boot? *.bootimage makeiblk
		rm -f *.iblk nohup.out reltest elf2brel elf2boot

#  Build a program to test the relocation code as a UNIX user process.
#  Includes the test driver (reltest) and the relocation module (relx).

reltest :	$(RELTESTOBJ)
		$(CC) $(LDFLAGS) $(SDBFLAG) -o $@ $(RELTESTOBJ) $(LIBDBUG)

reltest.o :	reltest.c bootinfo.h
		$(CC) $(CFLAGS) $(SDBFLAG) $(DBUG) -c reltest.c

relx.o :	rel.c bootinfo.h
		$(CC) $(CFLAGS) -DTEST $(SDBFLAG) $(DBUG) -c rel.c
		mv rel.o relx.o

relb.o :	rel.c bootinfo.h
		$(CC) $(CFLAGS) -S rel.c
		$(AS) rel.s
		mv rel.o relb.o
		rm rel.s

decompressx.o :	decompress.c
		$(CC) $(CFLAGS) -DNOTBOOT $(SDBFLAG) $(DBUG) -c decompress.c
		mv decompress.o decompressx.o

decompressb.o :	decompress.c
		$(CC) $(CFLAGS) -S decompress.c
		$(AS) decompress.s
		mv decompress.o decompressb.o
		rm decompress.s

