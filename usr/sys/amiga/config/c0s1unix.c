#include "sys/types.h"
#include "sys/sysmacros.h"
#include "sys/param.h"
#include "vm/bootconf.h"

#define FD0 makedevice(16, 0)
#define C0D0S0 makedevice(18, 0)
#define C0D0S1 makedevice(18, 16)
#define C2D0S0 makedevice(18, 2)
#define C2D0S1 makedevice(18, 18)
#define C6D0S0 makedevice(18, 6)
#define C6D0S1 makedevice(18, 22)

dev_t    rootdev = C0D0S1;		/* device of the root */

struct bootobj swapfile =
{
	"",
	"/dev/dsk/c0d0s2",
	0,
	0,
	20479,				/* 10 megabytes - 1 block */
	0,
};
