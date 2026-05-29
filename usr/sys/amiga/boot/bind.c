/*
 *  Copyright (C) 1991, Commodore Business Machines, Inc.
 *  Patchlevel: bind.c_1.1
 *
 *  Scan the system memory list and pick a suitable address as the
 *  kernel start address.  Note that the space taken up by the MemHeader
 *  itself is "missing" from the list, i.e., if it says memory starts at
 *  0x280020, it really starts at 0x280000.  The missing 0x20 bytes are
 *  where the MemHeader itself is stored.  Also note that the exception
 *  vectors have been deleted from the chip memory (not that it matters),
 *  and of course remember that you can't load the kernel into chip memory
 *  anyway.
 */

#pragma pack(2)
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
/* #include <exec/execname.h> */
#include <exec/libraries.h>
#include <stdio.h>
#pragma pack(4)

#define ABSEXECBASE ( (struct ExecBase **)4 )

unsigned long bind ()
{
    struct MemHeader *node;
    unsigned long base = 0;
    unsigned long maxsize = 0;
    unsigned long size;

    for (node = (struct MemHeader *) (*ABSEXECBASE) -> MemList.lh_Head;
	 node -> mh_Node.ln_Succ;
	 node = (struct MemHeader *) node -> mh_Node.ln_Succ)
    {
	if (!(node -> mh_Attributes & MEMF_CHIP)) {
    	    size = (unsigned long) (node -> mh_Upper);
	    size -= (unsigned long) (node -> mh_Lower) & ~0xFF;
	    if (size > maxsize) {
		maxsize = size;
		base = (unsigned long) (node -> mh_Lower) & ~0xFF;
	    }
	}
    }
    return (base);
}

