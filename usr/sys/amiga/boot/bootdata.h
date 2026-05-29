/*
 *	Copyright (C) 1991, Commodore Business Machines, Inc.
 *      Patchlevel: bootdata.h_1.1
 */

/*
 *	This structure is used by the boot code, and is part of the
 *	boot/kernel interface.
 */

struct BootData {
    unsigned long bd_entry;		/* start address */
    unsigned long bd_tvaddr;		/* text start virtual address */
    unsigned long bd_toffset;		/* text segment offset */
    unsigned long bd_tsize;		/* text segment size */
    unsigned long bd_rvaddr;		/* rodata start virtual address */
    unsigned long bd_roffset;		/* rodata segment offset */
    unsigned long bd_rsize;		/* rodata segment size */
    unsigned long bd_dvaddr;		/* data start virtual address */
    unsigned long bd_doffset;		/* data segment offset */
    unsigned long bd_dsize;		/* data segment size */
};

