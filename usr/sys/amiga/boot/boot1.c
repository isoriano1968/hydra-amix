/*
 *	Copyright (C) 1991, Commodore Business Machines, Inc.
 *
 *	First level bootstrap, which is limited to the first two
 *	blocks of the boot device.  Since we need more space than
 *	that to do everything, we load in a second level bootstrap
 *	that can be as large as necessary (up to limits of memory
 *	and boot device size).
 *
 *	Note that the pointer to the I/O request structure is
 *	passed on to the second level bootstrap, so it can find
 *	the next boot info block and the kernel file.
 *	
 */

#pragma pack(2)
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/nodes.h>
#include <exec/libraries.h>
#include <exec/io.h>
#include <exec/execbase.h>
#include <exec/alerts.h>
#include <devices/trackdisk.h>
#pragma pack(4)

#include <sys/elf.h>
#include "infoblock.h"

/*
 * This is a workaround for a bug in the A3000 "SuperKickstart"
 * scheme.  When you turn on an A3000, it gives you a choice of
 * loading the OS from hard disk or floppy.  However, before it
 * does this, it loads and executes the Unix bootstrap (that's the
 * bug).  So, the below code detects that the system really wants
 * to superkickstart rather than boot Unix, and "returns" to an
 * adjusted address which will continue the superkickstart process.
 * Totally disgusting.
 */

#define KICKSTARTKLUDGE 1

#if KICKSTARTKLUDGE
#pragma pack(2)
#include <libraries/expansion.h>
#include <libraries/expansionbase.h>
#define ALERT_EXPB	0x45585042	/* 'EXPB' */
#pragma pack(4)
#endif

#ifndef NULL
#define NULL		0		/* Standard NULL definition */
#endif

#define STARTOFFSET	1024		/* Secondary bootstrap @ blks 2 & up */
#define ENTRY_OFFSET	12		/* Must match value in elf2boot */

#define ALERT_BDIO	0x4244494F	/* 'BDIO' */
#define ALERT_IBLK	0x49424C4B	/* 'IBLK' */
#define ALERT_CSUM	0x4353554D	/* 'CSUM' */
#define ALERT_AMEM	0x414D454D	/* 'AMEM' */

extern void *AllocMem ();
extern int DoIO ();
extern int Alert ();
extern char *reloc ();

/*
 *	Read iob->io_Length bytes from the boot device, starting
 *	at offset iob->io_Offset, into a buffer allocated in CHIP
 *	memory, and return a pointer to the buffer if successful,
 *	and NULL if not.  The io_Offset is updated if the read
 *	is successful, so that the next read starts where the
 *	previous left off.  Note that io_Length is overwritten
 *	by the attempt to turn off the boot device motor, assuming
 *	it is a floppy.
 */

static unsigned char *readblocks (struct IOStdReq *iob)
{
    unsigned char *p;

    p = (unsigned char *) AllocMem (iob -> io_Length, MEMF_CHIP);
    if (p == NULL) {
	Alert (ALERT_AMEM | AT_DeadEnd, &(iob -> io_Length));
    } else {
	iob -> io_Command = CMD_READ;
	iob -> io_Data = (APTR) p;
	if (DoIO (iob) != 0) {
	    Alert (ALERT_BDIO | AT_DeadEnd, &(iob -> io_Length));
	} else {
	    iob -> io_Offset += iob -> io_Length;
	}
	iob -> io_Command = TD_MOTOR;
	iob -> io_Length = 0;
	(void) DoIO (iob);
    }
    return (p);
}

int main (iob, retaddrp)
struct IOStdReq *iob;
unsigned long *retaddrp;
{
    unsigned char *p;
    int (*boot1)();
    struct infoblock *infobuf;		/* Buffer for kernel info block */
    unsigned long sum;			/* Checksum of second bootstrap file */

    /*
     *	Pre-1.4 strap doesn't give a valid io_Offset so assume zero
     */

    if ((*(struct ExecBase **)4) -> LibNode.lib_Version < 36) {
	iob->io_Offset = 0;
    }

#if KICKSTARTKLUDGE

    {
	extern void *OpenLibrary ();
	struct ExpansionBase *ExpansionBase =
	    (struct ExpansionBase *) OpenLibrary (reloc (EXPANSIONNAME), 0);
	if (ExpansionBase == NULL) {
	    Alert (ALERT_EXPB | AT_DeadEnd, &ExpansionBase);
	}
	if (ExpansionBase -> Flags & 0x30) {
	    *retaddrp += 14;
	    return (0);
	}
    }

#endif	/* KICKSTARTKLUDGE */

    /*
     *	Read an info block from the boot device.  Since the AmigaDOS boot
     *	does not update the io_Offset field, we have to do it here ourselves.
     */

    iob -> io_Offset += STARTOFFSET;
    iob -> io_Length = sizeof (struct infoblock);
    infobuf = (struct infoblock *) readblocks (iob);

    if (*(long *)(infobuf -> ib_ident) != ALERT_IBLK) {
	Alert (ALERT_IBLK | AT_DeadEnd, infobuf -> ib_ident);
    }
	
    /*
     *	Read the secondary bootstrap from the boot device.
     */

    iob -> io_Length = infobuf -> ib_size;
    p = readblocks (iob);

    /*
     *  Compute the SVR4 "sum" compatible checksum of the secondary
     *  bootstrap and warn the user if it does not match the value expected,
     *  as recorded in the info block.
     */

    if ((sum = chksum (p, infobuf -> ib_size)) != infobuf -> ib_chksum) {
	Alert (ALERT_CSUM | AT_DeadEnd, &sum);
    }

    /*
     *	Jump to the second level bootstrap.  If we ever return, then
     *	pass the return value back to the caller.
     */

    boot1 = (int (*)()) (p + ENTRY_OFFSET);
    return ((*boot1) (iob));
}
