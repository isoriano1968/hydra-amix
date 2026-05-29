#include "sys/types.h"
#include "sys/tty.h"
#include "sys/jioctl.h"
#include "sys/xtproto.h"
#include "sys/xt.h"

#define XTCNT	3
#define LINKSIZE (sizeof(struct Link) + sizeof(struct Channel)*(MAXPCHAN-1))

int nxt_count ={XTCNT};
char nxtctl[LINKSIZE*XTCNT];
