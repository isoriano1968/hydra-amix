#include "sys/param.h"
#include "sys/types.h"
#include "sys/psw.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/sysinfo.h"
#include "sys/cred.h"
#include "sys/callo.h"
#include "sys/fs/s5dir.h"
#include "sys/sit.h"
#include "sys/signal.h"
#include "sys/vnode.h"
#include "sys/evecb.h"
#include "sys/hrtcntl.h"
#include "sys/dl.h"
#include "sys/errno.h"
#include "sys/priocntl.h"
#include "sys/map.h"
#include "sys/file.h"
#include "sys/proc.h"
#include "sys/procset.h"
#include "sys/events.h"
#include "sys/evsys.h"
#include "sys/hrtsys.h"

#define HRTIME	 50
#define HRVTIME  50

long timer_resolution = { HZ };
timer_t hrtimes[HRTIME];
int hrtimes_size = {HRTIME};
timer_t itimes[HRVTIME];
int itimes_size = {HRVTIME};
