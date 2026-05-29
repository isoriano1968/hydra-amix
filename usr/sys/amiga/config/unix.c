#ident	"@(#)unix.c	1.3"

#include "sys/types.h"
#include "sys/sysmacros.h"
#include "sys/param.h"
#include "vm/bootconf.h"

dev_t    rootdev = ROOTDEV;			/* device of the root */
dev_t    dumpdev = DUMPDEV;			/* device to dump on */
int      char_special = CHAR_SPECIAL;		/* true if cdevsw[] device */
int      do_sysdump = DO_SYSDUMP;		/* true for sysdump */
int      no_interactive = NO_INTERACTIVE;	/* true for autoboot */

struct bootobj swapfile =
{
	"",
	SWAPDEV,
	0,
	SWPLO,
	NSWAP,
	0,
};
