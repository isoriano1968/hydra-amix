/*
 *  Copyright (C) 1991, Commodore Business Machines, Inc.
 *  Patchlevel boot2.c_1.1
 *
 *  Second level bootstrap.  Receives a pointer to an AmigaDOS IOStdReq
 *  structure what has had the io_Offset field initialized to point to
 *  the first block of an ELF format kernel on the boot device.
 *
 *  Assumes that copying code placed in chip ram is safe from being
 *  overwritten.  Also makes some assumptions about the ELF kernel,
 *  including assuming that the loadable, merged text and data segment
 *  is the first program segment (the first program header table entry)
 *  for fully linked kernels.
 *
 *  Uses bootmethod 3 (EXEC1):
 *
 *	D0 = 3				// boot method 3
 *	D1 = (struct bootinfo *)	// pointer to boot information
 */

#pragma pack(2)
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/nodes.h>
#include <exec/libraries.h>
#include <exec/io.h>
#include <exec/execbase.h>
#include <devices/trackdisk.h>
#include <hardware/cia.h>
#include <libraries/configregs.h>
#include <libraries/configvars.h>
#include <libraries/expansion.h>
#include <libraries/expansionbase.h>
#pragma pack(4)

#define ABSEXECBASE ( (struct ExecBase **)4 )

#include <sys/elf.h>

#include "infoblock.h"
#include "bootdata.h"
#include "bootinfo.h"		/* NAUTO, etc */

extern void *reloc ();

#define NULL		0

#define min(a,b)	((a)<(b)?(a):(b))

#define TD_OFFSET_MASK	(TD_SECTOR-1)

/*
 *	Parameters to assembler "copyit" function.
 *
 *	WARNING:  If you change this structure, be sure to change the
 *	contants in copyit.s that reference the various structure
 *	members!
 */

struct ci {
    unsigned long ci_sp;	/* Space in chip mem for stack */
    unsigned long ci_entry;	/* Entry point in kernel */
    unsigned long ci_d0;	/* Value to place in d0 for kernel */
    unsigned long ci_d1;	/* Value to place in d1 for kernel */
    unsigned long ci_tbuf;	/* Source address of .text */
    unsigned long ci_tvaddr;	/* Destination address of .text */
    unsigned long ci_tsize;	/* Size of .text */
    unsigned long ci_rbuf;	/* Source address of .rodata */
    unsigned long ci_rvaddr;	/* Destination address of .rodata */
    unsigned long ci_rsize;	/* Size of .rodata */
    unsigned long ci_dbuf;	/* Source address of .data */
    unsigned long ci_dvaddr;	/* Destination address of .data */
    unsigned long ci_dsize;	/* Size of .data */
};


/*
 *	Because the boot code gets loaded and run at an arbitrary
 *	address, it must be position independent.  Thus static
 *	data, which is bound to a particular address by the linker,
 *	will be loaded at a different address than expected.  The
 *	reloc() function adjusts for this at runtime, adding or
 *	subtracting the appropriate amount from an address to generate
 *	the correct address of the data.  For simplicity, all of the
 *	static globals are gathered into a single structure and a
 *	macro is used to call the reloc function to generate the
 *	correct pointer to the global data.
 */

struct globals {
    struct IOStdReq *iob;
    unsigned long offset;	/* offset on boot dev where read starts */
    unsigned char *chipbuf;	/* Chip buffer for reads */
    unsigned long sum;		/* Checksum of kernel file */
    unsigned char *tmp;		/* Buffer for kernel file */
    unsigned long tmpsize;	/* Size of the kernel buffer */
    unsigned char *tmpptr;	/* Saved pointer for tmp copy */
} gbls = {
    NULL, 0L, NULL, 0L, NULL, NULL};

#define GBLS ((struct globals *)reloc(&gbls))

/*
 *	Message strings.  Uses ANSI string concatenation feature.
 */

static char volmsg[] = {
    "Load boot volume %d\n"
    "%M"
};

static char chksummsg[] = {
    "WARNING! Kernel file checksum mismatch.\n"
    "Expected %x, found %x.\n"
    "%M"
};

static char lzwerrmsg[] = {
    "WARNING! Kernel decompression overrun.\n"
    "Kernel may have been corrupted.\n"
    "%M"
};


/*
 *	If a floppy, turn off the motor.
 */

static void MotorOff ()
{
    GBLS -> iob -> io_Command = TD_MOTOR;
    GBLS -> iob -> io_Length = 0;
    if (DoIO (GBLS -> iob) != 0) {
	message (reloc ("Failed to turn drive motor off.\n%M"));
    }
}

/*
 *	A pointer to this function is passed to the decompression
 *	routine.  It is called whenever the decompression buffer
 *	becomes full, and needs to be flushed.  Before calling the
 *	decompression routine, the pointer to the ultimate destination
 *	of the decompressed data is set up, and we update it on
 *	each call as the data is transfered after decompression.
 */

int copytotmp (unsigned char *addr, int nbytes)
{
    if ((GBLS -> tmpptr + nbytes) > (GBLS -> tmp + GBLS -> tmpsize)) {
	message (reloc (lzwerrmsg));
    }
    memcpy (GBLS -> tmpptr, addr, nbytes);
    GBLS -> tmpptr += nbytes;
    return (nbytes);
}

/*
 *  PARAMETERS
 *
 *	addr		pointer to ultimate read destination in fast mem
 *	nbytes		number of bytes to read from boot dev
 *
 *  We implement multivolume boot sets by assuming that we are on the
 *  first volume at the time of the first read error (currently true
 *  the way the boot code works and given the current structure of
 *  the boot floppies).  When a read error occurs, we turn off the
 *  drive motor, bump the volume count by 1 (is initially 1), and
 *  prompt for that volume as the next volume number.  We then force
 *  the boot device read offset back to zero to start reading from
 *  the beginning of the volume.
 *
 *  To avoid a noticable delay after reading a large file, we
 *  do part of the work of computing the file's checksum, as each
 *  block is read in.
 *
 */

static int readstuff (unsigned char *addr, int nbytes)
{
    int len;
    int volume = 1;
    int rtnval = nbytes;

    while (nbytes) {
	len = min (nbytes, TD_SECTOR - (GBLS -> offset & TD_OFFSET_MASK));
	GBLS -> iob -> io_Command = CMD_READ;
	GBLS -> iob -> io_Length = TD_SECTOR;
	GBLS -> iob -> io_Data = (APTR) GBLS -> chipbuf;
	GBLS -> iob -> io_Offset = (GBLS -> offset & ~TD_OFFSET_MASK);
	if (DoIO (GBLS -> iob) != 0) {
	    MotorOff ();
	    message (reloc (volmsg), ++volume);
	    GBLS -> offset = 0;
	} else {
	    (void) memcpy (addr, GBLS -> chipbuf + (GBLS -> offset & TD_OFFSET_MASK), len);
	    GBLS -> sum += sumbytes (addr, len);
	    GBLS -> offset += len;
	    addr += len;
	    nbytes -= len;
	}
    }
    return (rtnval - nbytes);
}


/*
 *	Main routine for level 2 boot.  Note that we are passed the
 *	same iob used by the level 1 boot, which has initialized the
 *	io_Offset to point to the next block on the boot device after
 *	the level 2 boot blocks, I.E. the kernel info block.
 */

unsigned long main (iob)
struct IOStdReq *iob;
{
    struct infoblock *infobuf;		/* Buffer for kernel info block */
    unsigned long infobase;		/* Offset of kernel info block */
    unsigned long filebase;		/* Offset of kernel on boot dev */
    struct ExpansionBase *ExpansionBase;
    struct bootinfo *bi;
    struct BootData *bd;
    struct ConfigDev *cdp;
    struct ConfigDev *acp;
    struct MemHeader *mhp;
    struct MemHeader *mnp;
    unsigned long temp;
    struct ci *ci;			/* Information for copyit() */
    Elf32_Addr bindaddr;		/* Base addr to reloc kernel to */
    int copycodesize;			/* Size of the copyit() code */
    void (*copycode)();			/* Place to copy copyit() to */
    extern void copyit ();		/* Start of copyit() */
    extern void copyitend ();		/* End of copyit() */

    GBLS -> iob = iob;
    ExpansionBase =
	(struct ExpansionBase *) OpenLibrary (reloc (EXPANSIONNAME), 0);
    if (ExpansionBase == NULL) {
	message (reloc ("Failed to open '%s'.\n%M"), reloc (EXPANSIONNAME));
    }

    /*
     *  Preserve the boot device offset of the kernel information block
     *  and the start of the kernel ELF file.
     */

    infobase = GBLS -> iob -> io_Offset;
    filebase = GBLS -> iob -> io_Offset + TD_SECTOR;

    /*
     *  Allocate some buffers in chip memory, including a reusable sector
     *  read buffer, a kernel info block buffer, and a buffer into which
     *  the copyit() function will be copied before being run.
     *
     *  Note that alloc does it's own error checking for low/no memory
     *  conditions.
     */

    GBLS -> chipbuf = (unsigned char *) alloc (TD_SECTOR, MEMF_CHIP);
    infobuf = (struct infoblock *) alloc (sizeof (*infobuf), MEMF_CHIP);
    copycodesize = (long) copyitend - (long) copyit;
    copycode = (void (*)()) alloc (copycodesize, MEMF_CHIP);

    /*
     *  Read the kernel info block from the boot device.
     */

    GBLS -> offset = infobase;
    readstuff ((unsigned char *) infobuf, TD_SECTOR);
    if (!streq (reloc ("IBLK"), infobuf -> ib_ident)) {
	message (reloc ("Failed to get info block (IBLK).\n%M"));
    }
	
    /*
     *  Allocate a buffer large enough to hold the entire kernel file
     *  and read it into the buffer.  Then turn off the boot device
     *  motor (if a floppy).
     */

    if (infobuf -> ib_fullsize > 0) {
	GBLS -> tmpsize = infobuf -> ib_fullsize;
    } else {
	GBLS -> tmpsize = infobuf -> ib_size;
    }
    GBLS -> tmp = (unsigned char *) alloc (GBLS -> tmpsize, MEMF_FAST);
    GBLS -> sum = 0;
    GBLS -> offset = filebase;
    if (infobuf -> ib_fullsize > 0) {
	GBLS -> tmpptr = GBLS -> tmp;
	decompress (reloc (readstuff), reloc (copytotmp), infobuf -> ib_size);
    } else {
	readstuff (GBLS -> tmp, GBLS -> tmpsize);
    }
    MotorOff ();

    /*
     *	Finish computing the SVR4 "sum" compatible checksum of the kernel
     *	file and warn the user if it does not match the value expected,
     *	as recorded in the kernel info block.  See the routine chksum.c
     *	for details of the algorithm.
     */

    temp = (GBLS -> sum >> 16) + (GBLS -> sum & 0xFFFF);
    GBLS -> sum = (temp >> 16) + (temp & 0xFFFF);
    if (GBLS -> sum != infobuf -> ib_chksum) {
	message (reloc (chksummsg), infobuf -> ib_chksum, GBLS -> sum);
    }

    /*
     *  Allocate space in chip memory to hold the bootinfo structure, and
     *	then go through the various lists and initialize bootinfo.
     */

    bi = (struct bootinfo *) alloc (sizeof (*bi), MEMF_CLEAR | MEMF_CHIP);

    acp = bi -> autocon;
/*    for (cdp = (struct ConfigDev *) ExpansionBase -> BoardList.lh_Head ;
	 cdp -> cd_Node.ln_Succ ;
	 cdp = (struct ConfigDev *) cdp -> cd_Node.ln_Succ) {
	*acp++ = *cdp;
    }
*/
      for (cdp = (struct ConfigDev *) FindConfigDev(NULL,-1L,-1L);
	  cdp != NULL;
	  cdp = (struct ConfigDev *)FindConfigDev(cdp,-1L,-1L) ) {
	  *acp++ = *cdp;
	  }
    mhp = bi -> memory;
    for (mnp = (struct MemHeader *) (*ABSEXECBASE) -> MemList.lh_Head;
	 mnp -> mh_Node.ln_Succ;
	 mnp = (struct MemHeader *) mnp -> mh_Node.ln_Succ) {
	*mhp++ = *mnp;
    }

    /*
     *  Check to see if a specific binding address has been specified
     *  in the info block.  A value of 0xFFFFFFFF indicates that the
     *  boot code is to do automatic binding (the default) by scanning
     *  the system's memory list and picking an appropriate relocation
     *  address.
     */

    if ((bindaddr = infobuf -> ib_bind) == 0xFFFFFFFF) {
	bindaddr = bind ();
    }

    /*
     *  Do the relocation.  Complain if we don't find any .text, .rodata,
     *  or .data.
     */

    bd = &(bi -> bootdata);
    rel (GBLS -> tmp, bindaddr, bd);
    if (bd -> bd_tsize + bd -> bd_rsize + bd -> bd_dsize == 0) {
	message (reloc ("No .text, .rodata, or .data!\n%M"));
    }

    /*
     *  Initialize the ci structure in chip memory, by reusing the
     *  "chipbuf" buffer we used for doing device I/O.  Note that here
     *  is where is allocate space for a stack to be used by copyit(),
     *  since the stack must be in chip memory, and point to the top
     *  word of the 16 word stack.
     *
     */

    ci		     = (struct ci *) GBLS -> chipbuf;
    ci -> ci_sp	     = (unsigned long) alloc (64, MEMF_CHIP) + 60;
    ci -> ci_entry   = bd -> bd_entry;
    ci -> ci_d0      = EXEC1;
    ci -> ci_d1      = (unsigned long) bi;
    ci -> ci_tbuf    = (unsigned long) (GBLS -> tmp + bd -> bd_toffset);
    ci -> ci_tvaddr  = bd -> bd_tvaddr;
    ci -> ci_tsize   = bd -> bd_tsize;
    ci -> ci_rbuf    = (unsigned long) (GBLS -> tmp + bd -> bd_roffset);
    ci -> ci_rvaddr  = bd -> bd_rvaddr;
    ci -> ci_rsize   = bd -> bd_rsize;
    ci -> ci_dbuf    = (unsigned long) (GBLS -> tmp + bd -> bd_doffset);
    ci -> ci_dvaddr  = bd -> bd_dvaddr;
    ci -> ci_dsize   = bd -> bd_dsize;

    /*
     *  Copy the copyit() code from wherever it is currently located
     *  to a buffer in chip memory.  This is the final code that runs
     *  before the kernel takes over, and must be protected from being
     *  overwritten by itself while doing the shuffling of kernel
     *  text and data sections.  It is presumed that placing it in
     *  chip memory satisfies this requirement.  Once the code is copied
     *  then call it in supervisor mode.  We never expect to return
     *  from Supervisor().
     */

    memcpy (copycode, reloc (copyit), copycodesize);
    Supervisor (copycode, ci);
}
