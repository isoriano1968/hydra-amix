/*
** RKDB:  Remote Kernel Debuggers.  This character device (usually
** /dev/rkdb c 45 0) is used on the debugger system to impersonate
** a /dev/proc filesystem and is the connection between the
** debugger (adb, sdb, or gdb?) and the debugged system.
*/

#include "sys/cmn_err.h"
#include "sys/types.h"
#include "sys/param.h"
#include "sys/signal.h"
#include "sys/errno.h"
#include "sys/psw.h"
#include "sys/pcb.h"
#include "sys/user.h"
#include "sys/stream.h"
#include "sys/stropts.h"
#include "sys/strlog.h"
#include "sys/log.h"
#include "sys/queue.h"
#include "sys/cio_defs.h"
#include "sys/sbd.h"
#include "sys/debug.h"
#include "sys/inline.h"
#include "sys/systm.h"
#include "sys/cred.h"
#include "sys/dlpi.h"
#include "sys/ddi.h"
#include "sys/procfs.h"
#include "sys/time.h"
#include "sys/kmem.h"
#include "sys/uio.h"
#include "aenuser.h"
#include "aen.h"
#include "lance.h"
#include "kdebug.h"

extern timestruc_t hrestime;
extern kdebug_t *send_kdb_packet();

/*
** Global stuff.
*/

rkunit_t rkunit;

int
rkopen(devp, mode, type, cr)
dev_t *devp;
int mode;
int type;
struct cred *cr;
{
    mblk_t *mp = allocb(sizeof(dl_bind_req_t), BPRI_MED);
    dl_bind_req_t *p;

    if (rkunit.rkopened)
	return 0;

    if (!rkunit.q)
	return ENXIO;

    rkunit.id = hrestime.tv_sec;

    /*
    ** Do the sap thing.
    */
    if (!mp)
	return ENOMEM;

    mp->b_datap->db_type = M_PROTO;

    p = (dl_bind_req_t *)mp->b_rptr;
    p->dl_primitive = DL_BIND_REQ;
    p->dl_sap = ETHERTYPE_KERNEL_DEBUG;
    mp->b_wptr += sizeof(dl_bind_req_t);
    putnext(WR(rkunit.q), mp);

    while (!rkunit.rkopened)
	sleep((caddr_t)&rkunit, RKPRI);

    return 0;
}

int
rkclose(dev, flag, type, cr)
dev_t dev;
int flag;
int type;
struct cred *cr;
{
    if (rkunit.rlc)
    {
    }
    rkunit.rkopened = FALSE;
    return 0;
}

int rkread(dev, uiop, cr)
dev_t dev;
struct uio *uiop;
struct cred *cr;
{
    kdebug_t request, *response;
    int error = 0;

    request.direction = KDB_DIR_SEND;
    request.kdb_command = KDB_READ;

    while (uiop->uio_resid && !error)
    {
	request.kdb_rdwr.seek = uiop->uio_offset;
	request.kdb_rdwr.count = uiop->uio_resid;

	if ((response = send_kdb_packet(&request)) == NULL)
	    return ETIMEDOUT;

	if (!(error = response->kdb_error))
	    if (uiomove(response->kdb_rdwr.data, response->kdb_rdwr.count,
			UIO_READ, uiop))
		error = EFAULT;

	kmem_free(response, sizeof(*response));
	rkunit.response = NULL;
    }

    return error;
}

int
rkwrite(dev, uiop, cr)
dev_t dev;
struct uio *uiop;
struct cred *cr;
{
    kdebug_t request, *response;
    int error = 0;

    request.direction = KDB_DIR_SEND;
    request.kdb_command = KDB_WRITE;

    while (uiop->uio_resid && !error)
    {
	request.kdb_rdwr.seek = uiop->uio_offset;
	request.kdb_rdwr.count = uiop->uio_resid;

	if (uiomove(request.kdb_rdwr.data,
		    min(sizeof(request.kdb_rdwr.data), request.kdb_rdwr.count),
		    UIO_WRITE, uiop))
	{
	    error = EFAULT;
	    break;
	}

	if ((response = send_kdb_packet(&request)) == NULL)
	    return ETIMEDOUT;

	error = response->kdb_error;

	kmem_free(response, sizeof(*response));
	rkunit.response = NULL;
    }

    return error;
}

int
rkioctl(dev, cmd, arg, mode, cr, rvalp)
dev_t dev;
int cmd;
int arg;
int mode;
struct cred *cr;
int *rvalp;
{
    kdebug_t *response, request;
    int error = 0, command;

    switch(cmd)
    {
    case KDB_SET_ETHERNET_ADDRESS:
	    /*
	    ** Sets the ethernet address to debug.  This is usually
	    ** done by the daemon process `kdb'
	    */
	if (copyin((caddr_t)arg,
		   (caddr_t)rkunit.dst_addr,
		   sizeof(ether_addr_t)))
	    return EFAULT;
	return 0;

    case PIOCSRLC:
	rkunit.rlc = 1;
	return 0;

    case PIOCRRLC:
	rkunit.rlc = 0;
	return 0;

    case PIOCRUN:		/* make process runnable */
	command = KDB_RUN;
	if (arg)
	{
	    prrun_t prrun;

	    if (copyin((caddr_t)arg, (caddr_t)&prrun, sizeof(prrun)))
		return EFAULT;

	    request.kdb_run.argp = TRUE;
	    request.kdb_run.flags = prrun.pr_flags;
	    request.kdb_run.vaddr = prrun.pr_vaddr;
	}
	else
	    request.kdb_run.argp = FALSE;
	break;

    case PIOCSTOP:		/* post STOP request and... */
	command = KDB_STOP;
	break;

    case PIOCWSTOP:		/* wait for process to STOP */
	command = KDB_WSTOP;
	break;

    case PIOCSTATUS:	/* get process status */
	command = KDB_STATUS;
	break;

    case PIOC:		/* echo back a nice response. */
	command = KDB_ECHO;
	break;

    case PIOCGFAULT:	/* get traced fault set */
	command = KDB_GFAULT;
	break;

    case PIOCSFAULT:	/* set traced fault set */
	command = KDB_SFAULT;
	break;

    case PIOCCFAULT:	/* clear current fault */
	command = KDB_CFAULT;
	break;

    case PIOCGREG:		/* get general registers */
	command = KDB_GREG;
	break;

    case PIOCSREG:		/* set general registers */
	if (copyin((caddr_t)arg,
		   (caddr_t)request.kdb_gregs,
		   sizeof(request.kdb_gregs)))
	    return EFAULT;
	command = KDB_SREG;
	break;

    case PIOCGFPREG:	/* get floating-point registers */
	command = KDB_GFPREG;
	break;

    case PIOCSFPREG:	/* set floating-point registers */
	if (copyin((caddr_t)arg,
		   (caddr_t)&request.kdb_fpregs,
		   sizeof(request.kdb_fpregs)))
	    return EFAULT;
	command = KDB_SFPREG;
	break;

    case PIOCNMAP:		/* get number of memory mappings */
	command = KDB_NMAP;
	break;

    case PIOCMAP:		/* get memory map information */
	command = KDB_NMAP;	/* NMAP is correct here. */
				/* because we always need to */
				/* find out how many there */
				/* are and get them one by one */
				/* later --ag */
	break;

    default:
	return EINVAL;
    }

    request.kdb_command = command;
    request.direction = KDB_DIR_SEND;

    if ((response = send_kdb_packet(&request)) == NULL)
	return ETIMEDOUT;

    if (!(error = response->kdb_error))
    {
	switch (cmd)
	{
	case PIOC:		/* echo back a nice response. */
	case PIOCSREG:		/* set general registers */
	case PIOCSFPREG:	/* set floating-point registers */
	case PIOCRUN:		/* make process runnable */
	    /* Everything is Ok if no error was returned. */
	    break;

	case PIOCSTOP:		/* post STOP request and... */
	case PIOCWSTOP:		/* wait for process to STOP */
	    request.direction = KDB_DIR_SEND;
	    request.kdb_command = KDB_STATUS;

	    kmem_free(response, sizeof(*response));

	    if ((response = send_kdb_packet(&request)) == NULL)
		return ETIMEDOUT;

	    if ((error = response->kdb_error))
		break;

	    /* Fall Through */
	case PIOCSTATUS:	/* get process status */
	{
	    prstatus_t prs;

	    bzero((caddr_t)&prs, sizeof(prs));

	    prs.pr_flags = response->kdb_status.flags;
	    prs.pr_why = response->kdb_status.why;
	    prs.pr_what = response->kdb_status.what;
	    prs.pr_info.si_signo = response->kdb_status.signo;
	    prs.pr_info.si_code = response->kdb_status.code;
	    prs.pr_instr = response->kdb_status.instr;
	    bcopy((caddr_t)response->kdb_status.reg,
		  (caddr_t)prs.pr_reg,
		  sizeof(prs.pr_reg));

	    if (copyout((caddr_t)&prs, (caddr_t)arg, sizeof(prs)))
		error = EFAULT;
	    break;
	}

	case PIOCNMAP:		/* get number of memory mappings */
	    if (copyout((caddr_t)&response->kdb_nmap.num,
			(caddr_t)arg,
			sizeof(response->kdb_nmap.num)))
		error = EFAULT;
	    break;

	case PIOCMAP:		/* get memory map information */
	{
	    int i, n = response->kdb_nmap.num;
	    prmap_t prmap;

	    for (i=0; i < n; i++)
	    {
		/* Free old response */
		kmem_free(response, sizeof(*response));

		/* Build a KDB_MAP pack and keep track of them */

		request.kdb_command = KDB_MAP;
		request.direction = KDB_DIR_SEND;
		request.kdb_map_request.index = i;

		if ((response = send_kdb_packet(&request)) == NULL)
		{
		    error = ETIMEDOUT;
		    break;
		}

		if ((error = response->kdb_error))
		    break;

		prmap.pr_vaddr = response->kdb_map_response.vaddr;
		prmap.pr_size = response->kdb_map_response.size;
		prmap.pr_off = response->kdb_map_response.off;
		prmap.pr_mflags = response->kdb_map_response.mflags;

		if (copyout((caddr_t)&prmap,
			    (caddr_t)(arg + (sizeof(prmap)*i)),
			    sizeof(prmap)))
		{
		    error = EFAULT;
		    break;
		}
	    }

	    break;
	}

	case PIOCGREG:		/* get general registers */
	if (copyin((caddr_t)response->kdb_gregs,
		   (caddr_t)arg,
		   sizeof(response->kdb_gregs)))
	    return EFAULT;
	    break;

	case PIOCGFPREG:	/* get floating-point registers */
	if (copyin((caddr_t)&response->kdb_fpregs,
		   (caddr_t)arg,
		   sizeof(response->kdb_fpregs)))
	    return EFAULT;
	    break;

	case PIOCGFAULT:	/* get traced fault set */
	case PIOCSFAULT:	/* set traced fault set */
	case PIOCCFAULT:	/* clear current fault */
	    /*  Fall through for now. */
	default:
	    error = EINVAL;
	}
    }

    if (response != NULL)
	kmem_free(response, sizeof(*response));

    rkunit.response = NULL;

    return error;
}
