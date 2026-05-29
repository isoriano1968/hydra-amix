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
#include "aenuser.h"
#include "aen.h"
#include "kdebug.h"

int rkdopen(), rkdclose(), rkdrput(), rkdwput(), rkdwsrv();
void rkdioctl(), rkdiocdata(), rkdproto();

#define blklen(bp) (bp->b_wptr - bp->b_rptr)

/*
**  stream data structure definitions
*/

static struct module_info rkd_minfo =
{
    0x4147, "rkmod", AEN_MIN, AEN_MAX, AEN_HIWAT, AEN_LOWAT,
};

static struct qinit rkdrinit =
{
    rkdrput, NULL, rkdopen, rkdclose, NULL, &rkd_minfo, NULL,
};

static struct qinit rkdwinit =
{
    rkdwput, rkdwsrv, NULL, NULL, NULL, &rkd_minfo, NULL,
};

struct streamtab rkdinfo =
{
    &rkdrinit, &rkdwinit, NULL, NULL,
};

/*
** rkdopen -
*/

/* ARGSUSED */
static int
rkdopen(q, devp, flag, sflag, credp)
register queue_t *q;
dev_t *devp;
int flag, sflag;
cred_t *credp;
{
    rkunit.q = q;

    return 0;
}

/*
** rkdclose - close remote kernel
*/
static
rkdclose(q)
register queue_t *q;
{
    rkunit.q = (queue_t *)NULL;
}

/*
** rkdrput
*/
rkdrput(q, mp)
register queue_t *q;
register mblk_t *mp;
{
    if (!rkunit.rkopened)		/* Not opened, just an ack */
    {
	rkunit.rkopened = TRUE;
	freemsg(mp);
	wakeup((caddr_t)&rkunit);
	return;
    }

    rkunit.response = (kdebug_t *)kmem_zalloc(sizeof(kdebug_t), KM_NOSLEEP);

    if (!rkunit.response)
    {
	/*
	** Couldn't get memory for response.
	*/
	freemsg(mp);
	return;
    }

    bcopy((caddr_t)mp->b_cont->b_rptr,
	  (caddr_t)rkunit.response,
	  sizeof(kdebug_t));

    freemsg(mp);

    wakeup((caddr_t)&rkunit);
}

/*
** rkdwput -- comment goes here
*/
static
rkdwput(q, mp)
register queue_t *q;
register mblk_t *mp; 
{
    register int s;

    s = splrkd();

    switch (mp->b_datap->db_type)
    {
    case M_FLUSH:
	if (*mp->b_rptr & FLUSHW)
	{
	    flushq(q, FLUSHALL);
	    *mp->b_rptr &= ~FLUSHW;
	}

	if (*mp->b_rptr & FLUSHR) 
	    qreply(q, mp);
	else 
	    freemsg(mp);
	break;

    case M_PROTO:
    case M_PCPROTO:
	(void) rkdproto(q, mp);
	break;

    case M_IOCDATA:
	rkdiocdata(q, mp);
	break;

    case M_IOCTL:
	rkdioctl(q, mp);
	break;

    default:
	cmn_err(CE_WARN, "rk: bad msg type %d",	mp->b_datap->db_type);
	freemsg(mp);
	break;
    }
    splx(s);
}

static int
rkdwsrv()
{
    /*
    ** empty rkdwsrv for flow control only.
    */
    return 0;
}

static void
rkdiocdata(q, mp)
queue_t *q;
mblk_t *mp;
{
    struct iocblk *iocbp = (struct iocblk *)mp->b_rptr;

    /* For copyin/copyout failures, just free message. */

    if (((struct copyresp *)mp->b_rptr)->cp_rval)
    {
	freemsg(mp);
	return;
    }

    if (!((struct copyresp *)mp->b_rptr)->cp_private)
    {
	mp->b_datap->db_type = M_IOCACK;
	freemsg(unlinkb(mp));
	iocbp->ioc_count = 0;
	iocbp->ioc_rval = 0;
	iocbp->ioc_error = 0;
	putnext(RD(q), mp);
	return;
    }

    rkdioctl(q, mp);
}

static void
rkdioctl(q, mp)
queue_t *q;
mblk_t *mp;
{
    register struct iocblk *iocbp = (struct iocblk *)mp->b_rptr;

    switch (iocbp->ioc_cmd)
    {
#if 0
    case I_PLINK:
    case I_LINK:
	if (rkunit.plinkedq)
	{
	    mp->b_datap->db_type = M_IOCNAK;
	    iocbp->ioc_error = EBUSY;
	}
	else
	{
	    struct linkblk	*lp;
	    dl_bind_req_t	*bindr;
	    mblk_t		*nbp;
printf("PLINK Called\n");

	    lp = (struct linkblk *) mp->b_cont->b_rptr;

	    rkunit.plinkedq = lp->l_qbot;

	    if ((nbp = allocb(sizeof(dl_bind_req_t), BPRI_HI)) == NULL)
	    {
		iocbp->ioc_error = ENOSR;
		mp->b_datap->db_type = M_IOCNAK;
		break;
	    }

	    /*
	    ** Do the sap thing.
	    */
	    
	    nbp->b_datap->db_type = M_PROTO;
	    nbp->b_wptr += sizeof(dl_bind_req_t);
	    bindr = (dl_bind_req_t *) nbp->b_rptr;
	    bindr->dl_primitive = DL_BIND_REQ;
	    bindr->dl_sap = ETHERTYPE_KERNEL_DEBUG;
	    putnext(rkunit.plinkedq, nbp);
	    mp->b_datap->db_type = M_IOCACK;
	    iocbp->ioc_error = 0;
	    qreply(q, mp);
	    return;
	}
	break;

    case I_PUNLINK:
    case I_UNLINK:
	if (rkunit.plinkedq)
	{
	    /* <IMPLEMENT> unbind request goes here */
	    rkunit.plinkedq = 0;
	    mp->b_datap->db_type = M_IOCACK;
	    iocbp->ioc_error = 0;
	    qreply(q, mp);
	    return;
	}
	else
	{
	    mp->b_datap->db_type = M_IOCNAK;
	    iocbp->ioc_error = EINVAL;
	}
	break;
#endif
    case KDB_SET_ETHERNET_ADDRESS:
	if (mp->b_datap->db_type == M_IOCTL && iocbp->ioc_count == TRANSPARENT)
	{
	    struct copyreq *creq = (struct copyreq *)mp->b_rptr;

	    mp->b_datap->db_type = M_COPYIN;
	    creq->cq_addr =  *(caddr_t *)mp->b_cont->b_rptr; 
	    mp->b_wptr = mp->b_rptr + sizeof(*creq);
	    creq->cq_size = sizeof(ether_addr_t);
	    creq->cq_flag = 0;
	    creq->cq_private = (mblk_t *)1;
	    putnext(RD(q), mp);
	    return;
	}

	if (blklen(mp->b_cont) < sizeof(ether_addr_t))
	    mp->b_datap->db_type = M_IOCNAK;
	else
	{
	    bcopy((caddr_t)mp->b_cont->b_rptr,
		  (caddr_t)rkunit.dst_addr,
		  sizeof(ether_addr_t));
	    mp->b_datap->db_type = M_IOCACK;
	}

	iocbp->ioc_error = 0;
	break;

    default:
	mp->b_datap->db_type = M_IOCNAK;
	iocbp->ioc_error = EINVAL;
	break;
    }

    iocbp->ioc_count = 0;
    iocbp->ioc_rval = 0;
    putnext(RD(q), mp);
    return;
}

static void
rkdproto(q, mp)
queue_t *q;
mblk_t *mp;
{
    register union DL_primitives *p = (union DL_primitives *)mp->b_rptr;

    switch (p->dl_primitive)
    {
#if 0
    case DL_BIND_ACK:
	break;
#endif

    default:
	cmn_err(CE_WARN, "rkdproto: unknown proto request: %d",
		(int)p->dl_primitive);
    }

    freemsg(mp);
    return;
}

kdebug_t *
send_kdb_packet(request)
kdebug_t *request;
{
    mblk_t *mp;
    queue_t *q = WR(rkunit.q);
    int bmesslen = sizeof(union DL_primitives) + (2 * sizeof(ether_addr_t));
    int tid;

    if (canput(q) && (mp = allocb(bmesslen, BPRI_MED)) != NULL &&
	(mp->b_cont = allocb(sizeof(*request), BPRI_MED)) != NULL)
    {
	register dl_unitdata_ind_t *dp = (dl_unitdata_ind_t *)mp->b_wptr;
	register unchar *p;
		
	dp->dl_primitive = DL_UNITDATA_REQ;
	dp->dl_dest_addr_length = sizeof(ether_addr_t);
	dp->dl_dest_addr_offset = sizeof(union DL_primitives);
	dp->dl_src_addr_length = sizeof(ether_addr_t);
	dp->dl_src_addr_offset =
	    sizeof(union DL_primitives) + sizeof(ether_addr_t);
		
	/* fill in destination address */
	p = mp->b_wptr + dp->dl_dest_addr_offset;
	bcopy((caddr_t)rkunit.dst_addr, (caddr_t)p, sizeof(ether_addr_t));

	dp->dl_reserved = 0;
	mp->b_wptr +=
	    sizeof(union DL_primitives) + (2 * sizeof(ether_addr_t));
		
	mp->b_datap->db_type = M_PROTO;
		
	/*
	** add transmit id to request;
	*/
	request->transmit_id = rkunit.id++;
	request->direction = KDB_DIR_SEND;

	/*
	** blt message.
	*/
	bcopy((caddr_t)request, (caddr_t)mp->b_cont->b_wptr, sizeof(*request));
	mp->b_cont->b_wptr += sizeof(*request);
	mp->b_cont->b_datap->db_type = M_DATA;
    }
    else
    {
	request->kdb_error = ENOMEM;
	return request;
    }

    while (1)
    {
	rkunit.response = NULL;
	do
	{
	    mblk_t *mp2 = copymsg(mp);
	    
	    putnext(q, mp);
	    mp = mp2;

	    tid = timeout(wakeup, (caddr_t)&rkunit, HZ);
	    sleep((caddr_t)&rkunit, RKPRI);
	    untimeout(tid);

	    if (!rkunit.response && !mp)
		    return NULL;
	} while (!rkunit.response);

	if (rkunit.response->transmit_id == request->transmit_id)
	{
	    freemsg(mp);
	    break;
	}

	kmem_free(rkunit.response, sizeof(kdebug_t));
    }

    return rkunit.response;
}

