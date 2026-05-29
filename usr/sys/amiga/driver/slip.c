/*
 * Streams SLIP driver.
 *
 * Actually two drivers:  a streams module which is pushed on a
 * tty device driver stream and implements the SL/IP protocol,
 * and a DLPI driver which creates an interface to be linked into
 * the network stream mux.
 *
 * Currently, the DLPI side is opened by slink(1) at system startup
 * time and always left open.  To do slattach, a module is pushed
 * on a tty device and an ioctl done on it to find out which slip
 * interface should be started up using ifconfig(1).
 */

#include "sys/types.h"
#include "sys/param.h"
#include "sys/signal.h"
#include "sys/errno.h"
#include "sys/stream.h"
#include "sys/stropts.h"
#include "sys/strlog.h"
#include "sys/log.h"
#include "sys/inline.h"
#include "sys/cred.h"

#include "sys/dlpi.h"
#include "sys/socket.h"
#include "sys/sockio.h"

#include "net/route.h"
#include "net/if.h"
#include "net/strioc.h"

#include "netinet/in.h"
#include "netinet/in_var.h"
#include "netinet/ip_str.h"

extern struct ifstats	*ifstats;	/* per-interface statistics for inet */

#include "slip.h"


#define SLIP_WHICH_INTERFACE 'slwh'

#define SLIP_DLPI_MID	'SD'		/* DLPI Module ID */
#define SLIP_TTY_MID	'ST'		/* TTY Module ID */

#define SLIP_MAXDEV	5		/* Maximum number of simultaneous */
					/* active SLIP interfaces. */
#define	SLIP_MIN	1
#define SLIP_MAX	1006		/* Is this some sort of standard? */
#define SLIP_HIWAT	2048
#define SLIP_LOWAT	(2*SLIP_HIWAT/3)

struct slipdev
{
    struct queue *dlpiq;		/* DLPI interface queue */
    struct queue *ttyq;			/* tty module queue */
    int state;
    struct ifstats stats;
    int if_flags;
    char ifname[20];
    mblk_t *curblk;			/* Input packet being assembled */
    int escaped;			/* Last character was ESCAPE */
};


/*
 *  DLPI stream data structure definitions
 */
static int dlpiopen(), dlpiclose();
static int dlpiwput(), dlpiwsrv();
static void do_dlpiioctl(), do_dlpiproto();
static int slipxmit();

static struct module_info dlpi_minfo =
{
    SLIP_DLPI_MID, "sld", SLIP_MIN, SLIP_MAX, SLIP_HIWAT, SLIP_LOWAT,
};

static struct qinit dlpirinit =
{
    NULL, NULL, dlpiopen, dlpiclose, NULL, &dlpi_minfo, NULL,
};

static struct qinit dlpiwinit =
{
    dlpiwput, dlpiwsrv, NULL, NULL, NULL, &dlpi_minfo, NULL,
};

struct streamtab sldinfo = { &dlpirinit, &dlpiwinit, NULL, NULL, };


/*
 *  TTY stream data structure definitions
 */
static int ttyopen(), ttyclose();
static int ttywput(), ttywsrv();
static int ttyrput(), ttyrsrv();
static void do_ttyioctl();

static struct module_info tty_minfo =
{
    SLIP_TTY_MID, "slipmod", SLIP_MIN, SLIP_MAX, SLIP_HIWAT, SLIP_LOWAT,
};

static struct qinit ttyrinit =
{
    ttyrput, ttyrsrv, ttyopen, ttyclose, NULL, &tty_minfo, NULL,
};

static struct qinit ttywinit =
{
    ttywput, ttywsrv, NULL, NULL, NULL, &tty_minfo, NULL,
};

struct streamtab sltinfo = { &ttyrinit, &ttywinit, NULL, NULL, };


/*
 * local storage definitions
 */

static struct slipdev sliptab[SLIP_MAXDEV];


/*
 * dlpiopen - Open SLIP DLPI interface.
 * Clone opens are not needed; the minor device will
 * be known already because it will have been determined
 * when the tty module was pushed, and multiple "sap"
 * devices (ala Ethernet) are not needed since IP is the
 * only protocol supported.
 */
static int dlpiopen(rq, devp, flag, sflag, credp)
register struct queue *rq;
dev_t *devp;
int flag, sflag;
cred_t *credp;
{
    register struct slipdev *slip;
    int mindev;

    if (sflag == MODOPEN)		/* Not a Module */
	return EINVAL;

    if (sflag == CLONEOPEN)
	return EINVAL;

    mindev = getminor(*devp);

    if ((mindev < 0) || (mindev >= SLIP_MAXDEV))
	return EINVAL;

    if (!rq->q_ptr)
    {
	slip = &sliptab[mindev];
	slip->if_flags = IFF_POINTOPOINT|IFF_NOTRAILERS|IFF_NOARP|IFF_RUNNING;
	if (!slip->stats.ifs_name) {
	    slip->stats.ifs_name = "sl";
	    slip->stats.ifs_unit = mindev;
	    slip->stats.ifs_next = ifstats;
	    ifstats = &slip->stats;
	}

	slip->stats.ifs_active = 1;
	slip->stats.ifs_mtu = SLIP_MAX;
	slip->stats.ifs_ipackets =
	    slip->stats.ifs_ierrors =
		slip->stats.ifs_opackets =
		    slip->stats.ifs_oerrors = 0;

	WR(rq)->q_ptr = rq->q_ptr = (caddr_t)slip;
	slip->dlpiq = rq;
    }

    return 0;
}


/*
 * dlpiclose - Disconnect a SLIP DLPI interface
 */
static int dlpiclose(rq)
register struct queue *rq;
{
    int dev;
    register struct slipdev *slip = (struct slipdev *)rq->q_ptr;

    slip->stats.ifs_active = 0;
    slip->stats.ifs_ipackets =
	slip->stats.ifs_ierrors =
	    slip->stats.ifs_opackets =
		slip->stats.ifs_oerrors = 0;

    slip->dlpiq = (struct queue *)0;

    OTHERQ(rq)->q_ptr = rq->q_ptr = (caddr_t)0;

    return 0;
}


/*
 * dlpiwput - Slip DLPI write put procedure
 */
static int dlpiwput(q, mp)
register struct queue *q;
register mblk_t *mp;
{
    register int s;

    s = splstr();
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
	do_dlpiproto(q, mp);
	break;

    case M_IOCTL:
	do_dlpiioctl(q, mp);
	break;

    default:
	printf("sl%d: funny msg type 0x%x",
	       (struct slipdev *)q->q_ptr-sliptab, mp->b_datap->db_type);
	freemsg(mp);
	break;
    }
    splx(s);
    return 0;
}


/*
 * Dummy write service procedure for flow control only.
 */
static int dlpiwsrv()
{
    return 0;
}


/*
 * dlpiioctl - Process slip DLPI ioctl requests
 */
static void do_dlpiioctl(q, mp)
struct queue *q;
mblk_t *mp;
{
    register struct slipdev *slip = (struct slipdev *)q->q_ptr;
    register struct iocblk *iocbp = (struct iocblk *)mp->b_rptr;
    struct ifreq *ifr;

    if ((mp->b_wptr - mp->b_rptr) < sizeof (struct iocblk))
    {
	mp->b_datap->db_type = M_IOCNAK;
	qreply(q, mp);
	return;
    }

#ifdef IF_RESTRICT_TO_SUSER
    if (!suser(iocbp->ioc_cr))
    {
	switch (iocbp->ioc_cmd)
	{
	case SIOCGIFNAME:
	case SIOCGIFFLAGS:
	case SIOCGIFNETMASK:
	case SIOCGIFMETRIC:
	    break;
	default:
	    mp->b_datap->db_type = M_IOCNAK;
	    iocbp->ioc_error = EPERM;
	    qreply(q, mp);
	    return;
	}
    }
#endif

    if (iocbp->ioc_count == TRANSPARENT)
    {
	mp->b_datap->db_type = M_IOCNAK;
	iocbp->ioc_error = EINVAL;
	qreply(q, mp);
	return;
    }

    ifr = (struct ifreq *)mp->b_cont->b_rptr;

    switch (iocbp->ioc_cmd)
    {

    case SIOCSIFNAME:
	strncpy(slip->ifname, ifr->ifr_name, sizeof slip->ifname);
	break;

    case SIOCSIFFLAGS:
	slip->if_flags = ifr->ifr_flags;
	break;

    case SIOCGIFFLAGS:
	break;

    case SIOCSIFADDR:
    case SIOCSIFNETMASK:
    case SIOCSIFMETRIC:
    case SIOCSIFBRDADDR:
    case SIOCSIFDSTADDR:

    case SIOCGIFADDR:
    case SIOCGIFNETMASK:
    case SIOCGIFMETRIC:
    case SIOCGIFBRDADDR:
    case SIOCGIFDSTADDR:

    case SIOCIFDETACH:
	/* Nothing need be done for these, since IP has */
	/* set up acceptable defaults where necessary. */
	break;

    default:
	printf("sl%d: NAKing ioctl cmd 0x%x\n",
	       slip-sliptab, iocbp->ioc_cmd);
	mp->b_datap->db_type = M_IOCNAK;
	qreply(q, mp);
	return;
    }

    mp->b_datap->db_type = M_IOCACK;
    ((struct iocblk_in *)iocbp)->ioc_ifflags = slip->if_flags;
    qreply(q, mp);
}


/*
 * dlpiproto - Handle DLPI requests from IP
 */
static void do_dlpiproto(q, mp)
struct queue *q;
mblk_t *mp;
{
    register union DL_primitives *p = (union DL_primitives *)mp->b_rptr;
    register struct slipdev *slip = (struct slipdev *)q->q_ptr;

    switch (p->dl_primitive)
    {
    case DL_UNITDATA_REQ:
	if (q->q_first || !slipxmit(slip, mp))
	    putq(q, mp);
	break;

    case DL_BIND_REQ:
	{
	    dl_bind_req_t *reqp = (dl_bind_req_t *)mp->b_rptr;
	    dl_bind_ack_t *ackp;

	    slip->state = DL_IDLE;

	    freemsg(mp);

	    if (!(mp=allocb(sizeof *ackp, BPRI_MED)))
	    {
		/* BARF */
		return;
	    }

	    mp->b_datap->db_type = M_PCPROTO;
	    ackp = (dl_bind_ack_t *)mp->b_wptr;
	    ackp->dl_primitive = DL_BIND_ACK;
	    ackp->dl_sap = 0;
	    ackp->dl_addr_length = 0;
	    ackp->dl_addr_offset = 0;
	    ackp->dl_max_conind = 0;
	    ackp->dl_growth = 0;
	    mp->b_wptr += sizeof *ackp;

	    qreply(q, mp);
	}
	break;

    case DL_UNBIND_REQ:
	slip->state = 0;
	break;

    case DL_INFO_REQ:
	{
	    dl_info_ack_t *ackp;

	    freemsg(mp);

	    if (!(mp = allocb(sizeof (dl_info_ack_t), BPRI_MED)))
	    {
		/* BARF */
		return;
	    }

	    mp->b_datap->db_type = M_PCPROTO;
	    ackp = (dl_info_ack_t *)mp->b_wptr;
	    ackp->dl_primitive = DL_INFO_ACK;
	    ackp->dl_max_sdu = SLIP_MAX;
	    ackp->dl_min_sdu = SLIP_MIN;
	    ackp->dl_addr_length = 0;
	    ackp->dl_mac_type = DL_ETHER;
	    ackp->dl_reserved = 0;
	    ackp->dl_current_state = slip->state;
	    ackp->dl_max_idu = SLIP_MAX;
	    ackp->dl_service_mode = DL_CLDLS;
	    ackp->dl_qos_length = 0;
	    ackp->dl_qos_offset = 0;
	    ackp->dl_qos_range_length = 0;
	    ackp->dl_qos_range_offset = 0;
	    ackp->dl_provider_style = DL_STYLE1;
	    ackp->dl_growth = 0;
	    ackp->dl_addr_offset = 0;
	    mp->b_wptr += sizeof (dl_info_ack_t);

	    qreply(q, mp);
	}
	break;

    default:
	printf("slipproto: unknown proto request: 0x%x", p->dl_primitive);
	freemsg(mp);
	break;
    }
}


/*
 * slipxmit - Try to send a packet on the SLIP tty
 */
static int slipxmit(slip, dlpimp)
register struct slipdev *slip;
mblk_t *dlpimp;
{
    mblk_t *bp, *outmp;

    if (!canput(WR(slip->ttyq)))
	return 0;

    /* 2*SLIP_MAX is big enough for any packet within the MTU. */
    if (!(outmp=allocb(2*SLIP_MAX, BPRI_MED)))
    {
	++slip->stats.ifs_oerrors;
	return 0;
    }
    outmp->b_datap->db_type = M_DATA;

#define putc(ch) (*outmp->b_wptr++ = (ch))
    /* Copy input to output, escaping special characters */
    for ( bp=dlpimp->b_cont ; bp ; bp=bp->b_cont )
	while (bp->b_wptr > bp->b_rptr)
	{
	    unsigned char c = *bp->b_rptr++;

	    switch (c)
	    {
	    case FRAME_ESCAPE:
		putc(FRAME_ESCAPE);
		putc(TRANSPOSED_FRAME_ESCAPE);
		break;

	    case FRAME_END:
		putc(FRAME_ESCAPE);
		putc(TRANSPOSED_FRAME_END);
		break;

	    default:
		putc(c);
		break;
	    }
	}
    putc(FRAME_END);
#undef putc

    ++slip->stats.ifs_opackets;
    putnext(WR(slip->ttyq), outmp);
    freemsg(dlpimp);

    return 1;
}


/*
 * ttyopen - Open a SLIP tty module
 */
static int ttyopen(rq, devp, flag, sflag, credp)
struct queue *rq;
dev_t *devp;
int flag, sflag;
cred_t *credp;
{
    register struct slipdev *slip;

    if (sflag != MODOPEN)
	return EINVAL;

    if (!(slip = (struct slipdev *)rq->q_ptr))
    {
	for ( slip=sliptab ; slip->ttyq ; ++slip )
	    if (slip >= sliptab+SLIP_MAXDEV)
		return ENXIO;

	slip->ttyq = rq;
	WR(rq)->q_ptr = rq->q_ptr = (caddr_t)slip;
	slip->escaped = 0;
    }

    return 0;
}


/*
 * ttyclose - Close a SLIP tty module
 */
static int ttyclose(rq, oflag, credp)
struct queue *rq;
int oflag;
cred_t *credp;
{
    register struct slipdev *slip = (struct slipdev *)rq->q_ptr;

    slip->ttyq = (struct queue *)0;
    if (slip->curblk)
    {
	freeb(slip->curblk);
	slip->curblk = (mblk_t *)0;
    }

    return 0;
}


/*
 * Dummy write service procedure for flow control only.
 */
static int ttywsrv()
{
    return 0;
}


/*
 * ttywput - SLIP tty write service procedure
 */
static int ttywput(q, mp)
struct queue *q;
mblk_t *mp;
{
    register struct console *cp = (struct console *)q->q_ptr;

    switch (mp->b_datap->db_type)
    {
    case M_DATA:
	/* Discard any data written to the tty. */
	freemsg(mp);
	break;

    case M_IOCTL:
    case M_IOCDATA:
	do_ttyioctl(q, mp);
	break;

    case M_FLUSH:
	switch (mp->b_rptr[0])
	{
	case FLUSHRW:
	    mp->b_rptr[0] = FLUSHR;
	case FLUSHR:
	    putnext(RD(q), mp);
	    break;
	case FLUSHW:
	    freemsg(mp);
	    break;
	}
	break;

    case M_CTL:
	mp->b_datap->db_type = M_IOCNAK;
	putnext(RD(q), mp);
	break;

    case M_BREAK:
    case M_START:
    case M_STOP:
    case M_DELAY:
    case M_STARTI:
    case M_STOPI:
    case M_READ:
	freemsg(mp);
	break;

    default:
	printf("wput: got an unknown message 0x%x\n",
	       mp->b_datap->db_type);
	freemsg(mp);
	break;
    }

    return 0;
}


#define RETURN(type, rval, error) \
	do { \
	    mp->b_datap->db_type = type; \
	    freemsg(unlinkb(mp)); \
	    iocbp->ioc_count = 0; \
	    iocbp->ioc_rval = rval; \
	    iocbp->ioc_error = error; \
	    putnext(RD(q), mp); \
	    return; \
	} while (0)


static void do_ttyioctl(q, mp)
struct queue *q;
mblk_t *mp;
{
    register struct slipdev *slip = (struct slipdev *)q->q_ptr;
    struct iocblk *iocbp = (struct iocblk *)mp->b_rptr;

    switch (iocbp->ioc_cmd)
    {
    case SLIP_WHICH_INTERFACE:
	RETURN(M_IOCACK, slip-sliptab, 0);

    default:
	RETURN(M_IOCNAK, 0, EINVAL);
    }
}


/*
 * Dummy read service procedure for flow control only.
 */
static int ttyrsrv()
{
    return 0;
}


static int ttyrput(q, inmp)
struct queue *q;
mblk_t *inmp;
{
    register struct slipdev *slip = (struct slipdev *)q->q_ptr;

    switch (inmp->b_datap->db_type)
    {
    case M_DATA:
	{
	    mblk_t *outmp = slip->curblk;
	    register unsigned char *p = inmp->b_rptr;

	    while (p < inmp->b_wptr)
	    {
		unsigned char c = *p++;

		switch (c)
		{
		case FRAME_END:
		    if (outmp &&
			(outmp->b_wptr > outmp->b_rptr) &&
			canput(slip->dlpiq->q_next))
		    {
			mblk_t *dlpimp;
			register dl_unitdata_ind_t *udp;
			dlpimp = allocb(sizeof *udp, BPRI_MED);
			if (dlpimp)
			{
			    udp = (dl_unitdata_ind_t *)dlpimp->b_wptr;
			    udp->dl_primitive = DL_UNITDATA_IND;
			    udp->dl_dest_addr_length = 0;
			    udp->dl_dest_addr_offset = sizeof *udp;
			    udp->dl_src_addr_length = 0;
			    udp->dl_src_addr_offset = sizeof *udp;
			    udp->dl_reserved = 0;

			    dlpimp->b_wptr += sizeof *udp;
			    dlpimp->b_datap->db_type = M_PROTO;
			    dlpimp->b_cont = outmp;
			    ++slip->stats.ifs_ipackets;
			    putnext(slip->dlpiq, dlpimp);
			}
			else
			    freemsg(outmp);
			slip->curblk = outmp = 0;
		    }
		    break;

		case FRAME_ESCAPE:
		    slip->escaped = 1;
		    break;

		default:
		    if (slip->escaped)
		    {
			slip->escaped = 0;

			switch (c)
			{
			case TRANSPOSED_FRAME_ESCAPE:
			    c = FRAME_ESCAPE;
			    break;

			case TRANSPOSED_FRAME_END:
			    c = FRAME_END;
			    break;
			}
		    }

		    /* if packet is too large, drop it and start over */
		    if (!outmp)
			if (outmp=allocb(SLIP_MAX, BPRI_MED))
			    slip->curblk = outmp;
			else
			{
			    ++slip->stats.ifs_ierrors;
			    break;
			}
		    else
			if (outmp->b_wptr >= outmp->b_datap->db_lim)
			{
			    ++slip->stats.ifs_ierrors;
			    outmp->b_wptr = outmp->b_rptr =
				outmp->b_datap->db_base;
			}

		    *outmp->b_wptr++ = c;
		}
	    }
	}
	freemsg(inmp);
	break;
    default:
	/* Should handle M_HANGUP, etc. */
	/* ??? */
	putnext(q, inmp);
	break;
    }

    return 0;
}
