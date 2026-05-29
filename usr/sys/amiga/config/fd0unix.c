#include "sys/types.h"
#include "sys/sysmacros.h"
#include "sys/param.h"
#include "vm/bootconf.h"

#define FD0 makedevice(16, 0)
#define FD0P makedevice(16, 8)		/* fd0 with prompt */
#define C0D0S0 makedevice(18, 0)
#define C0D0S1 makedevice(18, 16)
#define C2D0S0 makedevice(18, 2)
#define C2D0S1 makedevice(18, 18)

dev_t    rootdev = FD0P;		/* device of the root */

struct bootobj swapfile =
{
	"",
	"/dev/fakeswap",		/* Fake swap area (dummydisk) */
	0,
	0,
	20479,				/* 10 megabytes - 1 block */
	0,
};
