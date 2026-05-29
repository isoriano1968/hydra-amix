/*
*#ident	"@(#)fs:master.d/rfs	1.11"
*
* RFS
*
*FLAG   #VEC    PREFIX  SOFT    #DEV    IPL     DEPENDENCIES/VARIABLES
oxj     -       rf_      -       -       -      
*/
#include "sys/list.h"
#include "sys/types.h"
#include "sys/param.h"
#include "sys/errno.h"
#include "sys/signal.h"
#include "sys/stream.h"
#include "sys/nserve.h"
#include "sys/rf_cirmgr.h"
#include "sys/user.h"
#include "sys/vnode.h"
#include "sys/vfs.h"
#include "sys/rf_messg.h"
#include "sys/rf_comm.h"
#include "sys/mount.h"
#include "sys/buf.h"

#define NAMERFS  "rfs"
#define NSRMOUNT	 20
#define MAXGDP		 24
#define NRCVD		 150
#define NRDUSER		 250
#define NSNDD		 150
#define MINSERVE	 3
#define MAXSERVE	 6
#define RCACHETIME	 10
#define	RF_MAXKMEM	 0

char rf_name[12] ={NAMERFS};
int nsrmount ={ NSRMOUNT };
int nrcvd  ={ NRCVD };
int nsndd ={ NSNDD };
int maxgdp = { MAXGDP };
int minserve  ={ MINSERVE };
int maxserve  ={ MAXSERVE };
int nrduser   ={ NRDUSER };
int rc_time ={ RCACHETIME };
int rf_maxkmem ={ RF_MAXKMEM };
