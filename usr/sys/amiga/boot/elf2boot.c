/*
 *	Copyright (C) 1991, Commodore Business Machines, Inc.
 *
 *	Converts a special format SVR4 ELF executable into a boot image.
 *	The ELF executable is presumed to have been linked with a 'mapfile'
 *	that results in all of the text, data, and bss being linked into
 *	a single loadable program segment starting at virtual address 0xC,
 *	and as a sanity check, the ELF executable is checked to ensure that
 *	these conditions are satisfied.
 *
 *
 *	Assuming a 2 block boot image (1024 bytes), the boot image is
 *	laid out as follows:
 *
 *	File
 *	Offset		Value		Contents
 *	-----------	------------	-----------------------------------
 *
 *	0x000-0x003	0x444F5300	Bootblock magic number ('DOS\0')
 *	0x004-0x007	-		Checksum
 *	0x008-0x00C	0x00000370	Black magic?
 *	0x00C-0x***	-		Code section from ELF file
 *	0x***-0x***	-		Data section from ELF file
 *	0x***-0x3FC	-		Empty (unused space)
 *	0x3FC-0x3FF	0x03010401	Super kickstart magic number
 */

#include <stdio.h>
#include <sys/elf.h>

#include "usermsg.h"

char *whoami = "urk!";

static Elf32_Ehdr ElfHeader;		/* Copy of the ELF file header */
static Elf32_Phdr ProgHdr;		/* An ELF program header */

static int vflag;			/* -v flag (verbose) */
static int bootsize;			/* -s <size> (sizeof boot blocks) */
static unsigned char *bootblocks;	/* Boot image built here */

static unsigned long BootStart;		/* Virtual address of boot start */
static unsigned long BootLength;	/* Length of boot text and data */
static unsigned long BootOffset;	/* Offset of boot segment in ELF */


/*
 * Define some magic bootblock numbers.
 *
 * BBMAGICN is related to superkickstart.  An A3000 in a completely cold
 * boot from poweron will normally refuse to accept any floppy other than
 * a SuperKickstart floppy until the soft-loaded OS is running.  This magic
 * number bypasses that requirement.
 */

#define ENTRY_OFFSET	12		/* Entrypoint offset */
#define BBMAGIC0	0x444F5300	/* 1st longword magic number; 'DOS\0' */
#define BBMAGIC2	0x00000370	/* 3rd longword magic number; magic? */
#define BBMAGICN	0x03010401	/* last longword magic number */

static void Options ();
static void ReadElfHeader ();
static void ReadProgramHeader ();
static void ReadBootSegment ();
static void WriteBoot ();
static unsigned long AddWithCarry ();

main (argc, argv)
int argc;
char *argv[];
{
    Options (argc, argv);
    ReadElfHeader ();
    ReadProgramHeader ();
    ReadBootSegment ();
    WriteBoot ();
    exit (0);
}

/*
 *	Process command line options using the standard getopt().
 */

static void Options (argc, argv)
int argc;
char *argv[];
{
    int c;
    extern char *optarg;
    extern int getopt ();

    whoami = argv[0];
    while ((c = getopt (argc, argv, "vs:")) != EOF) {
	switch (c) {
	    case 's':
		bootsize = atoi (optarg);
		switch (optarg[strlen(optarg)-1]) {
		    case 'K':
		    case 'k':
			bootsize *= 1024;
			break;
		    case 'B':
		    case 'b':
			bootsize *= 512;
			break;
		}
		break;
	    case 'v':
		vflag++;
		break;
	}
    }
}

/*
 *	Read the ELF header and check the magic number to verify that we
 *	have a valid ELF file.
 */

static void ReadElfHeader ()
{
    if (fread ((char *) &ElfHeader, sizeof (ElfHeader), 1, stdin) != 1) {
	usermsg (MSG_ELFHDR);
    } else if (strncmp (ElfHeader.e_ident, ELFMAG, 4) != 0) {
	usermsg (MSG_ELFMAGIC);
    }
}

/*
 *	Check to make sure we have a program header table and that it is
 *	of the expected structure.  If the size of each member is not what
 *	we expect, then we are probably dealing with a later version of
 *	ELF and help.
 *
 */

static void ReadProgramHeader ()
{
    unsigned int phi;
    int oldbootsize;
    extern char *malloc ();

    if (ElfHeader.e_phoff == 0) {
	usermsg (MSG_MPHT);
    } else if (fseek (stdin, ElfHeader.e_phoff, 0) != 0) {
	usermsg (MSG_PHTSEEK);
    } else if (ElfHeader.e_phentsize != sizeof (ProgHdr)) {
	usermsg (MSG_BADPHT);
    } else if (fread ((char *) &ProgHdr, sizeof (ProgHdr), 1, stdin) != 1) {
	usermsg (MSG_PHTREAD);
    } else if (ProgHdr.p_type != PT_LOAD) {
	usermsg (MSG_PHTLOAD);
    } else {
	BootStart = ProgHdr.p_vaddr;
	BootLength = ProgHdr.p_memsz;
	BootOffset = ProgHdr.p_offset;
    }
    if (bootsize == 0) {
	bootsize = (ENTRY_OFFSET + BootLength + 4 + 1023) / 1024;
	bootsize *= 1024;
    } else if ((bootsize % 1024) != 0) {
	oldbootsize = bootsize;
	bootsize = (ENTRY_OFFSET + BootLength + 4 + 1023) / 1024;
	usermsg (MSG_BOOTSIZE, oldbootsize, bootsize);
    }
    if ((bootblocks = (unsigned char *) malloc (bootsize)) == NULL) {
	usermsg (MSG_MALLOC, bootsize);
    }
    (void) memset (bootblocks, 0, bootsize);
    usermsg (MSG_LAYOUT, "boot", BootStart, BootLength, BootLength);
    if ((ENTRY_OFFSET + BootLength + 4) > bootsize) {
	usermsg (MSG_TOOBIG, bootsize);
    }

}

static void ReadBootSegment ()
{
    if (fseek (stdin, BootOffset, 0) != 0) {
	usermsg (MSG_BOOTSEEK);
    }
    if (fread (bootblocks + ENTRY_OFFSET, BootLength, 1, stdin) != 1) {
	usermsg (MSG_BOOTREAD);
    }
}

static void WriteBoot ()
{
    unsigned long *p;
    unsigned long checksum = 0;

    ((unsigned long *) bootblocks)[0] = BBMAGIC0;
    ((unsigned long *) bootblocks)[2] = BBMAGIC2;
    ((unsigned long *) bootblocks)[(bootsize/sizeof(long))-1] = BBMAGICN;

    for (p = (unsigned long *) bootblocks ;
	 p < (unsigned long *) (bootblocks + bootsize) ;
	 ++p) {
	checksum = AddWithCarry (checksum, *p);
    }
    ((unsigned long *) bootblocks)[1] = ~checksum;
    if (fwrite (bootblocks, bootsize, 1, stdout) != 1) {
	usermsg (MSG_BOOTWRITE);
    }
}

/*
 *  Add two unsigned longs, and if overflow occurs, add the overflow
 *  back into the sum.  Since overflow will always be either 0 for
 *  no overflow, or 1 for overflow, and we can detect overflow by
 *  noticing that the result is less than either operand, we can
 *  do this portably in C rather than resorting to assembly code.
 */

static unsigned long AddWithCarry (unsigned long x, unsigned long y)
{
    unsigned long result;

    result = x + y;
    if ((result < x) || (result < y)) {
	result++;
    }
    return (result);
}
