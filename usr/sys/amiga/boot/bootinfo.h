/*
 *	Copyright (C) 1991, Commodore Business Machines, Inc.
 *
 *	This file defines the interface between the boot code and
 *	the kernel.
 */

#ifndef NAUTO		/* Number of configuration slots */
#define NAUTO	16	/* Also in "amigarom.h" in kernel includes */
#endif

/*
 * Boot methods:
 *
 * A2620:
 *	D0 = 1
 *	D1 = Nothing
 *	Autoconfig table at 0x78080 (struct acon as per amigarom.h)
 *
 * EXEC0:
 *	D0 = 2
 *	D1 = Pointer to autoconfig table struct ConfigDev[16]
 *		(struct ConfigDev as per libraries/configvars.h)
 *
 * EXEC1:
 *	D0 = 3
 *	D1 = Pointer to struct bootinfo (below)
 *
 */

#define A2620 1				/* A2620/A2630 ROM */
#define EXEC0 2				/* Exec boot method 0 */
#define EXEC1 3				/* Exec boot method 1 */

/*
 *	The following structure defines the information passed to
 *	the kernel for boot method 3.  The BootData structure is
 *	defined in bootdata.h.  It is currently not clear whether
 *	ConfigDev and MemHeader will come from kernel/boot private
 *	include files, or standard AmigaDOS include files.
 */

struct bootinfo {
    struct ConfigDev autocon[NAUTO];	/* Autoconfig devices */
    struct MemHeader memory[NAUTO];	/* Memory regions */
    unsigned char keystates[16];	/* Keyboard state */
    struct BootData bootdata;		/* Info about text & data */
    unsigned char reserved[256];
};

