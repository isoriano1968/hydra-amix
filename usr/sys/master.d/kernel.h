
#define SYS  "UNIX_System_V"
#define NODE  "unix"
#define REL    "4.0"
#define VER    "?"
#define MACH   "Amiga"

#define ARCHITECTURE "m68k"
#define HW_SERIAL "0"
#define HW_PROVIDER "Amiga"
#define SRPC_DOMAIN "rpcdomain"


/* The following entries form the tunable parameter table. */


#define NBUF   100
#define NCALL   60
#define NPROC   200
#define NCLIST   50	/* 0 default */

/* The following stuff is for streams. */

#define NSTRPUSH   9
#define STRCTLSZ   1024

# define	PGOVERFLOW	16	/* 8 <= N <= 128 */
# define	NOTPGOVERFLOW	16	/* 8 <= N <= 128 */

/* strmsgsz is the size for the maximum streams message a user can create. */
/* for Release 4.0, a value of zero will indicate no upper bound.  This */
/* parameter will disappear entirely in the next release. */

#define STRMSGSZ   0


/* strthresh is a cap to the amount of dynamic memory that the */
/* streams subsystem can consume.  Once passed, pushes, opens of */
/* streams devices, and writes will fail for non-root processes. */
/* strthresh is in bytes; a value of 0 means no limit */
/* */
/* the default set below is 2M; this value should be reset based */
/* on the amount of memory available on the machine and the */
/* machine's configuration */

#define STRTHRESH	2097152

/* maxup is the maximum number of processes per user */

#define MAXUP   100

/* hashbuf must be a power of 2 */

#define NHBUF   64
#define NPBUF   20

/* max nubmer of active file/record locks system-wide */

#define FLCKREC   300

/* Shared Libraries:  Maximum number of libraries that can be */
/*                    attached to a process at one time. */

#define SHLBMAX   2

/* Delay for delayed writes */

#define NAUTOUP   60

/* default per process resource limits (set to -1 for infinite limits) */
/* -1 is an infinite limit */
/* S prefix is for soft limits, H prefix is for hard limits */
/* CPULIM - maximum combined user and system time in seconds */
/* FSZLIM - maximum file size in bytes */
/* DATLIM - maximum writeable mapped memory (swap space) in bytes */
/* STKLIM - maximum size of current stack in bytes */
/* CORLIM - maximum size of core file in bytes */
/* FNOLIM - maximum number of file descriptors */
/* VMMLIM - maximum amount of simulaneously mapped virtual memory in bytes */

#define SCPULIM   0x7fffffff
#define HCPULIM   0x7fffffff
#define SFSZLIM   0x10000000
#define HFSZLIM   0x7fffffff
#define SDATLIM   0x1000000
#define HDATLIM   0x7fffffff
#define SSTKLIM   0x1000000
#define HSTKLIM   0x7fffffff
#define SCORLIM   0x100000
#define HCORLIM   0x7fffffff
#define SFNOLIM   0x40
#define HFNOLIM   0x400
#define SVMMLIM   0x1000000
#define HVMMLIM   0x7fffffff

/* added for paging */

#define SPTMAP   100
#define MAXPMEM   0
#define GPGSLO   25
#define GPGSHI   40	/* Not in AT&T anymore? */
#define MAXUMEM   8192
#define FSFLUSHR   1
#define MINARMEM   25
#define MINASMEM   25
#define PUTBUFSZ   2000
#define PANICBUFSZ   2000	/* space for panic backtrace */
#define PAGES_UNLOCK   200

/* Multiple groups and chown(2) restrictions */

#define RSTCHOWN   0
#define NGROUPS_MAX   16

/* Specification of root fstype */

#define ROOTFSTYPE   ""			/* Empty means try them all */

/* Scheduler Tunables */

#define MAXCLSYSPRI   99
#define SYS_NAME   "SYS"
#define INITCLASS   "TS"

/* XENIX Tunables for shared data */
#define XSDSEGS   25
#define XSDSLOTS   3

/* Buffer cache tunables */
#define BUFHWM   200
#define ARG_MAX   51200

