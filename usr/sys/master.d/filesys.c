#include "sys/conf.h"
#include "sys/types.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/sysinfo.h"
#include "sys/vfs.h"
#include "sys/vnode.h"

#include "s5.c"
#include "ufs.c"
#include "bfs.c"
#include "proc.c"
#include "fifofs.c"
#include "xnamfs.c"
#include "specfs.c"
#include "rfs.c"
#include "namefs.c"
#include "fd.c"
#include "nfs.c"

extern struct vfsops vfs_strayops;
extern int s5init(),ufsinit(),nfsinit(),rf_init(),xnaminit(),bfinit(),prinit();
extern int specinit(),fifoinit(),nameinit(),fdinit(),nfsinit();

struct vfssw vfssw[] = {
	{ "BADVFS",	0,	&vfs_strayops,	0},
	{ NAMES5,	s5init,		0,	0},
	{ NAMEUFS,	ufsinit,	0,	0},
	{ NAMEFIFO,	fifoinit,	0,	0},
#define INCLUDE_RFS
#ifdef INCLUDE_RFS
	{ NAMERFS,	rf_init,	0,	0},
#endif
	{ NAMEXNAM,	xnaminit,	0,	0},
	{ NAMEBF,	bfinit,		0,	0},
	{ NAMEPR,	prinit,		0,	0},
	{ NAMESPEC,	specinit,	0,	0},
	{ NAMENM,	nameinit,	0,	0},
	{ NAMEFD,	fdinit,		0,	0},
	{ NAMENFS,	nfsinit,	0,	0},
	};

int nfstype = (sizeof(vfssw)/sizeof(vfssw[0]));

vnode_t *rootvp = NULL;

int nullflag[1] = {0,};
int oldflag[1] = {D_OLD,};

extern struct streamtab modtab;
extern struct streamtab ldtrinfo;
extern struct streamtab ttcoinfo;
extern struct streamtab pteminfo;
extern struct streamtab pcktinfo;
extern struct streamtab timinfo;
extern struct streamtab trwinfo;
#if 0
extern struct streamtab lmrinfo;
extern struct streamtab lmeinfo;
extern struct streamtab lmtinfo;
extern struct streamtab lmbinfo;
#endif
extern struct streamtab sockinfo;
extern struct streamtab appinfo;
extern struct streamtab sltinfo;
extern struct streamtab conninfo;
#ifdef KERNEL_DEBUGGER
extern struct streamtab rkdinfo;
#endif /* KERNEL_DEBUGGER */

struct fmodsw fmodsw[] = {
/* name         streamtab       flags */
{ "module",	&modtab,	oldflag, },
{ "ldterm",	&ldtrinfo,	nullflag, },
{ "sockmod",	&sockinfo,	nullflag, },
{ "app",	&appinfo,	oldflag, },
{ "ttcompat",	&ttcoinfo,	oldflag, },
{ "timod",	&timinfo,	nullflag, },
{ "tirdwr",	&trwinfo,	oldflag, },
{ "ptem",	&pteminfo,	oldflag, },
{ "pckt",	&pcktinfo,	oldflag, },
{ "slipmod",	&sltinfo,	nullflag, },
{ "connld",	&conninfo,	nullflag, },
#ifdef KERNEL_DEBUGGER
{ "rkd",	&rkdinfo,	nullflag, },
#endif /* KERNEL_DEBUGGER */
#if 0
{ "lmodr", 	&lmrinfo,	?flag, },
{ "lmode", 	&lmeinfo,	?flag, },
{ "lmodt",	&lmtinfo,	?flag, },
{ "lmodb",	&lmbinfo,	?flag, },
#endif
};
int fmodcnt = (sizeof(fmodsw)/sizeof(fmodsw[0]));
