/*
** Patchlevel: master.d:kernel.c_1.2
*/
#include "sys/types.h"
#include "sys/param.h"
#include "sys/psw.h"
#include "sys/sbd.h"
#include "sys/sysmacros.h"
#include "sys/pcb.h"
#include "sys/immu.h"
#include "sys/systm.h"
#include "sys/signal.h"
#include "sys/user.h"
#include "sys/mount.h"
#include "sys/proc.h"
#include "sys/var.h"
#include "sys/debug.h"
#include "sys/conf.h"
#include "sys/tuneable.h"
#include "sys/buf.h"
#include "sys/stream.h"
#include "sys/param.h"
#include "sys/file.h"
#include "sys/callo.h"
#include "sys/map.h"
#include "sys/fcntl.h"
#include "sys/flock.h"
#include "sys/sysinfo.h"
#include "sys/tty.h"
#include "sys/utsname.h"
#include "sys/resource.h"
#include "sys/exec.h"
#include "sys/class.h"

#include "kernel.h"

extern void fpuinit(),cinit(),binit(),inoinit(),vfsinit(),finit();
extern void strinit(),msginit(),seminit(),sadinit(),hydrainit();
extern void tclinit(),tcoinit(),tcooinit();
#ifdef KERNEL_DEBUGGER
extern void kdb_init_hooks();
#endif /* KERNEL_DEBUGGER */

void	(*init_tbl[])() = {
	fpuinit,
	cinit,
	binit,
	inoinit,
	vfsinit,
	finit,
	strinit,
	msginit,
	seminit,
	sadinit,
	tclinit,
	tcoinit,
	tcooinit,
#ifdef KERNEL_DEBUGGER
	kdb_init_hooks,
#endif /* KERNEL_DEBUGGER */
	hydrainit,
	0};

struct buf pbuf[NPBUF];
struct hbuf hbuf[NHBUF];
struct callo callout[NCALL];
struct map sptmap[SPTMAP];
char putbuf[PUTBUFSZ];
int putbufsz = {PUTBUFSZ};
char panicbuf[PANICBUFSZ];
int panicbufsz = {PANICBUFSZ};

int pages_pp_maximum = {PAGES_UNLOCK};

struct var v = {
	NBUF,
	NCALL,
	NPROC,
	0,
	0,
	MAXCLSYSPRI,
	NCLIST,
	MAXUP,
	NHBUF,
	NHBUF-1,
	NPBUF,
	SPTMAP,
	MAXPMEM,
	NAUTOUP,
	BUFHWM,
	XSDSEGS,
	XSDSLOTS
	};

struct rlimit rlimits[] = {
	SCPULIM,
	HCPULIM,
	SFSZLIM,
	HFSZLIM,
	SDATLIM,
	HDATLIM,
	SSTKLIM,
	HSTKLIM,
	SCORLIM,
	HCORLIM,
	SFNOLIM,
	HFNOLIM,
	SVMMLIM,
	HVMMLIM};

/* scheduler */
char sys_name[15] = {SYS_NAME};
char intcls[16] = {INITCLASS};
char *initclass = {intcls};

/* tuneable.h */
tune_t tune = {
	GPGSLO,
	GPGSHI,	/* was 0 ??? */
	0,
	0,
	0,
	0,
	0,
	MAXUMEM,
	FSFLUSHR,
	MINARMEM,
	MINASMEM,
	FLCKREC
	};

int exec_ncargs = ARG_MAX;

/* file and record locking */
struct flckinfo flckinfo
	= {0,0};
/* shared libraries */
struct shlbinfo shlbinfo
	= {SHLBMAX, 0, 0, 0};
/* uname */
struct utsname utsname
	={SYS,
	  NODE,
	  REL,
	  VER,
	  MACH};

/* sysinfo */
char architecture[]
	={ARCHITECTURE};
char hw_serial[] 
	={HW_SERIAL};
char hw_provider[] 
	={HW_PROVIDER};
char srpc_domain[] 
	={SRPC_DOMAIN};

/* Streams */
int nstrpush  ={NSTRPUSH};
int strmsgsz  ={STRMSGSZ};
int strctlsz  ={STRCTLSZ};
int strthresh ={STRTHRESH};

/* Multiple groups and chown(2) restrictions */
int rstchown  = { RSTCHOWN };
int ngroups_max  = { NGROUPS_MAX };

/* Specification of root file system */
char rootfstype[15] = {ROOTFSTYPE};

/* Directory name lookup cache */
int ncsize  = { NPROC+100 };

/* add overflow lists for sched and pageout */
struct buf pgoutbuf[PGOVERFLOW];
struct buf notpgoutbuf[NOTPGOVERFLOW];
int npgoutbuf = PGOVERFLOW;
int nnotpgoutbuf = NOTPGOVERFLOW;

/* XENIX Time variables for ftime system call */
int Timezone;
int Dstflag;

/* head of inet statistics list */
int ifstats;

extern void aciaaintr(), jbintr(), a2091intr(), a3091intr(), aenintr(), hydraintr();
void	(*int2_tbl[])() = {
	aciaaintr,		/* ACIAA (parallel/kbd) ISR */
	jbintr,			/* A2090 ISR */
	a2091intr,		/* A2091 ISR */
	a3091intr,		/* A3000 internal SCSI ISR */
	aenintr,		/* A2065 Ethernet ISR */
	hydraintr,		/* Hydra AmigaNet ISR */
	0};

extern void parinit();
void	(*io_init[])() = {
	parinit,
	0};

int	(*io_start[])() = {
	0};

int	(*io_halt[])() = {
	0};

extern void qlintr(), slpoll();
void	(*io_poll[])() = {
	qlintr,
	slpoll,
	0};


extern int nodev();
extern int nulldev();
#define notty   (struct tty *)(0)
#define nostr   (struct streamtab *)(0)
#define	ND	nodev


/*#define DEBUGGING_CONSOLE_DRIVER /* Make printf() go to serial port. */
/* Pointers to the functions the kernel will use for "console" character I/O */
/* Normally cogetc and coputc, but might be KGetChar and KPutChar. */
unsigned char cogetc(), KGetChar();
void coputc(), KPutChar();
#ifdef DEBUGGING_CONSOLE_DRIVER
unsigned char (*congetc)() = KGetChar;
void (*conputc)() = KPutChar;
#else
unsigned char (*congetc)() = cogetc;
void (*conputc)() = coputc;
#endif


extern int mmopen(), mmclose(), mmwrite(), mmread(), mmioctl(), mmmmap(), mmsegmap();
extern int syopen(),syread(),sywrite(),syioctl();
extern struct streamtab nsxtinfo, nxtinfo;
extern struct streamtab coinfo;
extern struct streamtab slinfo;
extern struct streamtab qlinfo;
extern bbopen(),bbclose(),bbread(),bbwrite(),bbioctl(),bbpoll();
extern paropen(),parclose(),parread(),parwrite(),parioctl(),parpoll();
extern amread(), amwrite(), amioctl(), ammmap();
extern tiopen(),ticlose(),tiread(),tiwrite(),tiioctl(),timmap();
extern resopen(),resclose(),resread(),reswrite(),resioctl(),resmmap();
extern benopen(),benread(),benwrite();
extern scropen(),scrclose(),scrread(),scrwrite(),scrioctl(),scrmmap(),scrpoll();
extern gsioctl();
#ifdef KERNEL_DEBUGGER
extern struct streamtab rkdinfo;
extern rkopen(),rkclose(),rkread(),rkwrite(),rkioctl();
#endif /* KERNEL_DEBUGGER */
extern struct streamtab audioinfo;
extern ctopen(),ctclose(),ctread(),ctwrite(),ctioctl();
extern fdopen(),fdclose(),fdread(),fdwrite(),fdioctl(),
	fdstrategy(),fdprint(),fdsize();
extern hdopen(),hdclose(),hdread(),hdwrite(),hdioctl(),
	hdstrategy(),hdprint(),hdsize();
extern ddopen(),ddclose(),ddread(),ddwrite(),ddioctl(),
	ddstrategy(),ddprint(),ddsize();
extern dumopen(),dumclose(),dumstrategy(),dumprint(),dumsize();
extern ramopen(),ramclose(),ramstrategy(),ramprint(),ramsize();
extern miread();
extern struct streamtab aeninfo;
extern struct streamtab hydrainfo;
extern struct streamtab sldinfo;
extern struct streamtab loopinfo;

extern struct streamtab ipinfo, tcpinfo, udpinfo, ripinfo, icmpinfo, arpinfo;
extern struct streamtab tcoinfo, tcooinfo, tclinfo;
extern struct streamtab sadinfo;
extern struct streamtab ptminfo, ptsinfo;

extern struct streamtab timinfo;
extern struct streamtab trwinfo;
extern struct streamtab clninfo;
extern struct streamtab	loginfo;
extern struct streamtab spinfo;
extern struct streamtab mapinfo;
extern int prfopen(), prfclose(), prfread(),prfwrite(),prfioctl();

static int oldflag[1] = {D_OLD,};
static int nullflag[1] = {0,};

struct bdevsw bdevsw[] = {
/* open close strategy print size xpoll xhalt flag*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*0*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*1*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*2*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*3*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*4*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*5*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*6*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*7*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*8*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*9*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*10*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*11*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*12*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*13*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*14*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*15*/
fdopen, fdclose, fdstrategy, fdprint, fdsize, ND, ND, nullflag,	/*16 = fd*/
hdopen, hdclose, hdstrategy, hdprint, hdsize, ND, ND, nullflag,	/*17 = hd*/
ddopen, ddclose, ddstrategy, ddprint, ddsize, ND, ND, nullflag,	/*18 = dd*/
dumopen, dumclose, dumstrategy, dumprint, dumsize, ND, ND, nullflag, /*19 = dummydisk*/
ramopen, ramclose, ramstrategy, ramprint, ramsize, ND, ND, nullflag, /*20 = ramdisk*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*21*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*22*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*23*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*24*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*25*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*26*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*27*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*28*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*29*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*30*/
ND,ND,ND,ND,ND,ND,ND,nullflag,	/*31*/
};
#define BDEVSIZE (sizeof(bdevsw)/sizeof(bdevsw[0]))
struct bdevsw shadowbsw[BDEVSIZE];
int bdevcnt = BDEVSIZE;

struct cdevsw cdevsw[] = {
/* open close read write ioctl 
		mmap segmap poll xpoll xhalt ttys stream flag */
ND,ND,ND,ND,ND,
		ND,ND,ND,ND,ND,notty,&coinfo,nullflag,		/*0=console*/
prfopen,prfclose,prfread,prfwrite,prfioctl,
		ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*1=prf*/
syopen,nulldev,syread,sywrite,syioctl,
		ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*2=tty*/
mmopen,mmclose,mmread,mmwrite,mmioctl,
		mmmmap,mmsegmap,ND,ND,ND,notty,nostr,nullflag,	/*3=mem*/
bbopen,bbclose,bbread,ND,ND,
		ND,ND,bbpoll,ND,ND,notty,nostr,nullflag,	/*4=bb*/
ND,ND,ND,ND,ND,
		ND,ND,ND,ND,ND,notty,&slinfo,nullflag,		/*5=sl*/
nulldev,nulldev,amread,amwrite,amioctl,
		ammmap,ND,ND,ND,ND,notty,nostr,nullflag,	/*6=amiga*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*7=win?*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&nxtinfo,oldflag,		/*8=xt*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&nsxtinfo,oldflag,		/*9=sxt*/
scropen,scrclose,scrread,scrwrite,scrioctl,
		scrmmap,ND,scrpoll,ND,ND,notty,nostr,nullflag,	/*10=screen*/
nulldev,nulldev,ND,ND,gsioctl,ND,ND,ND,ND,ND,notty,nostr,nullflag, /*11=scsi*/
nulldev,nulldev,miread,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag, /*12=machid*/
ND,ND,ND,ND,ND,
		ND,ND,ND,ND,ND,notty,&qlinfo,nullflag,		/*13=ql*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&ptsinfo,oldflag,		/*14=pts*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&ptminfo,oldflag,		/*15=ptmx*/
ctopen,ctclose,ctread,ctwrite,ND,
		ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*16=ct*/
fdopen,fdclose,fdread,fdwrite,ND,
		ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*17=fd*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&aeninfo,nullflag,		/*18=aen*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&sldinfo,nullflag,		/*19=slip dlpi*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&loopinfo,oldflag,		/*20=loop*/
paropen,parclose,parread,parwrite,parioctl,
		ND,ND,parpoll,ND,ND,notty,nostr,nullflag,	/*21=par*/
tiopen,ticlose,tiread,tiwrite,tiioctl,
		timmap,ND,ND,ND,ND,notty,nostr,nullflag,	/*22=tiga*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&timinfo,nullflag,		/*23=timod*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&trwinfo,oldflag,		/*24=tirdwr*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&loginfo,nullflag,		/*25=log*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&spinfo,oldflag,		/*26=sp*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&clninfo,nullflag,		/*27=clone*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&tcoinfo,oldflag,		/*28=ticots*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&tcooinfo,oldflag,		/*29=ticotsord*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&tclinfo,oldflag,		/*30=ticlts*/
resopen,resclose,resread,reswrite,resioctl,
		    resmmap,ND,ND,ND,ND,notty,nostr,nullflag,	/*31=res*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&ipinfo,oldflag,		/*32=ip*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&tcpinfo,oldflag,		/*33=tcp*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&udpinfo,oldflag,		/*34=udp*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&ripinfo,oldflag,		/*35=rawip*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&icmpinfo,oldflag,		/*36=icmp*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&arpinfo,oldflag,		/*37=arp*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*38=fd*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*39=hd*/
ddopen,ddclose,ddread,ddwrite,ddioctl,
		ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*40=dd*/
benopen,nulldev,benread,benwrite,ND,
		ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*41=ben*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*42*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*43*/
#ifdef KERNEL_DEBUGGER
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&rkdinfo,nullflag,		/*44=rkd*/
rkopen,rkclose,rkread,rkwrite,rkioctl,
		ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*45=kdebug*/
#else /* !KERNEL_DEBUGGER */
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*44=rkd*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*45=kdebug*/
#endif /* !KERNEL_DEBUGGER */
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&audioinfo,nullflag,	/*46=audio*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&hydrainfo,nullflag,		/*47=hydra*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*48*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*49*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&sadinfo,nullflag,		/*50=sad*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*51*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*52*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*53*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*54*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*55*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*56*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*57*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*58*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*59*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*60*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*61*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*62*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*63*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*64*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*65*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*66*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*67*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*68*/
ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*69*/
};
#define	CDEVSIZE (sizeof(cdevsw)/sizeof(cdevsw[0]))
struct cdevsw shadowcsw[CDEVSIZE];
int cdevcnt = CDEVSIZE;


extern short coffmagic,elfmagic,intpmagic;
extern int coffexec(),elfexec(),intpexec();
extern int coffcore(),elfcore(),intpcore();

struct execsw execsw[] = {
	{ &coffmagic,	coffexec,  coffcore, },
	{ &elfmagic,	elfexec,   elfcore, },
	{ &intpmagic,	intpexec,  nodev, },
	};

int nexectype = (sizeof(execsw)/sizeof(execsw[0]));


extern void ts_init(),rt_init(),sys_init();

struct class class[] = {
	"SYS",	sys_init,	0,
	"TS",	ts_init,	0,
	"RT",	rt_init,	0
	};

int nclass = (sizeof(class)/sizeof(class[0]));

