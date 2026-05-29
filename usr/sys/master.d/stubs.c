#include "sys/psw.h"
#include "sys/types.h"
#include "sys/immu.h"
#include "sys/sys3b.h"
#include "sys/stream.h"

/* STUBS --- routines that are not supported, or not wanted in the kernel */
/* If you define a stub here, be sure to also change                      */
/* io/Makefile to not compile and link the actual driver		  */
/* This way of doing sysgen's will eventually go away in favor of	  */
/* doing a real auto-configure.						  */

/*#define	SHMSTUB		/* shared memory */
/*#define	SEMSTUB		/* semaphores */
/*#define	MSGSTUB		/* ipc messages */
/*#define	XTSTUB		/* xt driver */
/*#define	NFSSTUB		/* NFS */
/*#define	SVVSSTUB	/* SVVS stream test drivers */
/*#define	ICDSTUB		/* In-Core Disk */
/*#define	RFSSTUB		/* remote file server */
#define	MISCSTUB	/* leftovers from 3B2 port */
#define	EVENTSTUB	/* Events */
#define ASYNCSTUB	/* Async I/O */

/* SHM stubs */
#ifdef SHMSTUB
shmexec(){}
shmexit(){}
shmseg(){ return 0;}
shmsys(){ noreach();}
shminit(){}
shmfork(){}
shmslp(){return 1;}
shmtext(){}
#endif

/* SEM stubs */
#ifdef SEMSTUB
semexit(){}
semsys(){noreach();}
seminit(){}
#endif

/* MSG stubs */
#ifdef MSGSTUB
msgsys(){noreach();}
msginit(){}
#endif

/* XT stubs */
#ifdef XTSTUB
xtin(){}
xtout(){}
xtvtproc(){}
errlayer(){}
#endif

/* RFS stubs */
#ifdef RFSSTUB
rfubyte(){ noreach(); }
rfuword(){ noreach(); }
rsubyte(){ noreach(); }
rsuword(){ noreach(); }
rcopyin(){ noreach(); }
rcopyout(){ noreach(); }
rfsys(){nopkg();}
rf_clock(){}
rfc_disable_msg(){}
rf_stime(){}
vtord(){ return 0;}
rf_ustat(){nopkg();}
/*
rf_putcap(){}
riget(){}
remote_call(){}
rnamei1(){}
rnamei3(){}
rnamei4(){}
rnamei0(){}
rnamei2(){}
remio(){}
unremio(){}
rem_date(){}
advfs(){nopkg();}
unadvfs(){nopkg();}
rmount(){}
rumount(){}
rfstart(){nopkg();}
rfstop(){nopkg();}
del_sndd(){}
remfileop(){}
*/
#endif

#ifdef NFSSTUB
nfssys(){nopkg();}
#endif

/* MISC stubs */
#ifdef MISCSTUB
mtcrchk(){}             /* see os/prf.c */
/*debuginit(){}*/
emsetmap(){}
emgetmap(){}
emunmap(){}
/*_des_crypt(){return(0);}*/
ssinvalidate(){}
/*rtodc(){}*/
#endif

hdeexit(){}			/* os/exit.c */
dmainit(){}			/* os/main.c */
s3blookup(){ return(0); }	/* debug/misc.c */
#ifdef USE_U3B2_STUFF				/* AMIX */
struct s3bsym  sys3bsym;	/* generated symbol table	 */
struct s3bconf sys3bconfig;	/* generated configuration table */
struct s3bboot sys3bboot;	/* generated boot program name	 */
#endif /* USE_U3B2_STUFF */			/* AMIX */

/* Drivers that SVVS wants */
#ifdef SVVSSTUB
struct streamtab loinfo;
struct streamtab tivinfo;
struct streamtab tidinfo;
struct streamtab tmxinfo;
struct streamtab lmrinfo;
struct streamtab lmeinfo;
struct streamtab lmtinfo;
struct streamtab lmbinfo;
#endif

#ifdef EVENTSTUB
ev_checkq(){}
ev_config(){return(0);}
ev_dr_post(){nopkg();}
ev_eqdtovp(){nopkg();}
ev_evsys(){nopkg();}
ev_evtrapret(){nopkg();}
ev_exec(){}
ev_exit(){}
ev_fork(){}
ev_gotsig(){}
ev_intr_restart(){return(0);}
ev_istrap(){return(0);}
ev_kev_post(){nopkg();}
ev_kev_free(){}
ev_mem_alloc(){nopkg();}
ev_mem_free(){}
ev_newpri(){}
ev_signal(){}
ev_stream_post(){nopkg();}
ev_traptousr(){}
#endif

#ifdef ASYNCSTUB
aio_config(){return(0);}
aioinit(){}
aiodaemon(){}
aiodmn_spawn(){}
async(){}
async_cancel(){}
stop_aio(){}
#endif

#ifdef ICDSTUB
icddevinit(){}
#endif

#ifdef OLD_TCP_STUB
struct streamtab loinfo;
struct streamtab if37info;
struct streamtab nninfo;
struct streamtab ipinfo;
struct streamtab tpiminfo;
#endif
