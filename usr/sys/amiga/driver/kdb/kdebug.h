#include "sys/psw.h"
#include "sys/pcb.h"
#include "sys/regset.h"

#define	ETHERTYPE_KERNEL_DEBUG	0x6167		/* Kernel debug packets */

#define RKPRI		26
#define splrkd()	spl3()

/*
** Handle these on the debugger side.
**
** PIOCSRLC	-- set run-on-last-close flag
** PIOCRRLC	-- reset run-on-last-close flag
*/
#define KDB				(0x4147<<16)
#define KDB_ECHO			(KDB|0)
#define KDB_SET_ETHERNET_ADDRESS	(KDB|1)
#define KDB_READ			(KDB|2)
#define KDB_WRITE			(KDB|3)
#define KDB_STOP			(KDB|4)
#define KDB_WSTOP			(KDB|5)
#define KDB_STATUS			(KDB|6)
#define KDB_RUN				(KDB|7)
#define KDB_GFAULT			(KDB|8)
#define KDB_SFAULT			(KDB|9)
#define KDB_CFAULT			(KDB|10)
#define KDB_GREG			(KDB|11)
#define KDB_SREG			(KDB|12)
#define KDB_GFPREG			(KDB|13)
#define KDB_SFPREG			(KDB|14)
#define KDB_NMAP			(KDB|15)
#define KDB_MAP				(KDB|16)

typedef struct
{
    int			seek;
    int			count;
    char		data[4];
} kdb_rdwr_t;

typedef struct
{
    int			flags;
    int			why;
    int			what;
    int			signo;
    int			code;
    long		instr;
    gregset_t		reg;
} kdb_status_t;

typedef struct
{
    int		argp;		/* Is flags valid? */
    long	flags;		/* Flags */
    caddr_t	vaddr;		/* Virtual address at which to resume */
} kdb_run_t;

typedef struct
{
    union
    {
	int		     index;
	struct
	{
	    caddr_t	     vaddr;
	    unsigned long    size;
	    off_t	     off;
	    long	     mflags;
	} st1;
    } u3;
} kdb_map_t;

typedef struct
{
    int		num;
} kdb_nmap_t;

typedef gregset_t	kdb_gregs_t;

typedef fpregset_t	kdb_fpregs_t;

typedef struct
{
    int			direction;
    unsigned int	transmit_id;
    union
    {
	int		command;
	int		error;
    } u1;
    union
    {
	kdb_rdwr_t	rdwr;
	kdb_run_t	run;
	kdb_status_t	status;
	kdb_map_t	map;
	kdb_nmap_t	nmap;
	kdb_gregs_t	gregs;
	kdb_fpregs_t	fpregs;
    } u2;
} kdebug_t;

/*
** Directions.
*/
#define KDB_DIR_SEND			0
#define KDB_DIR_REPLY			1

/*
** short cuts.
*/
#define kdb_command	u1.command
#define kdb_error	u1.error

#define kdb_rdwr		u2.rdwr
#define kdb_run			u2.run
#define kdb_status		u2.status
#define kdb_nmap		u2.nmap
#define kdb_map_request		u2.map.u3
#define kdb_map_response	u2.map.u3.st1
#define kdb_gregs		u2.gregs
#define kdb_fpregs		u2.fpregs

typedef struct
{
    queue_t		*q;
    int			rkopened;
    kdebug_t		*response;
    ether_addr_t	dst_addr;
    int			id;
    int			rlc;	/* run on last close */
    queue_t		*plinkedq;
} rkunit_t;

typedef struct
{
    ether_addr_t	dst_addr;
    kdebug_t		last_response;
    int			device;
    int			flags;
    int			why;
    int			what;
    int			signo;
    int			code;
    pcb_t		*context;
    int			wstop_id;
    int			last_transmit_id;
    unsigned short	*pc;
    int			ipl;
    int			tracing;
} rkdb_t;

extern rkunit_t rkunit;
extern rkdb_t rkdb;
