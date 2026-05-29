/*
** Lance EtherNet driver.
**
** Copyright 1989, 1990, 1991, 1992 Commodore Business Machines
**				written by Keith Gabryelski
**
** Patchlevel: aen_1.1
*/

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
#include "sys/socket.h"
#include "sys/sockio.h"
#include "net/if.h"
#include "aenuser.h"
#include "aen.h"
#include "lance.h"
#include "../kdb/kdebug.h"

#ifdef DEBUG0
#include "amigahr.h"
void hexdump();
#endif /* DEBUG0 */

extern struct ifstats	*ifstats;	/* per-interface statistics for inet */

extern char *panicstr;
int (*aen_packet_hook)();
void aenautoconfig();

int aen_initialize();
static int aenopen(), aenclose(), aenwput(), aenwsrv(), count_aen_boards();
static void aenioctl();
static int aenproto(), setup_lance();
static void initialize_and_go(), receive_interrupt(), transmit_interrupt();
static void toss_packet_up_stream();

/*
**  stream data structure definitions
*/

static struct module_info aen_minfo =
{
    0x656E /*'en'*/, "aen", AEN_MIN, AEN_MAX, AEN_HIWAT, AEN_LOWAT,
};

static struct qinit aenrinit =
{
    NULL, NULL, aenopen, aenclose, NULL, &aen_minfo, NULL,
};

static struct qinit aenwinit =
{
    aenwput, aenwsrv, NULL, NULL, NULL, &aen_minfo, NULL,
};

struct streamtab aeninfo =
{
    &aenrinit, &aenwinit, NULL, NULL,
};

/*
** storage definitions
*/
aen_board_t aen_board[AEN_MAXBOARDS];

aen_autoconfig_t aen_autoconfig[AEN_MAXBOARDS];
int aen_number_of_boards;

/*
** aenopen -	Open lance EtherNet driver.
*/

/* ARGSUSED */
static int
aenopen(q, devp, flag, sflag, credp)
register queue_t *q;
dev_t *devp;
int flag, sflag;
cred_t *credp;
{
    register aen_t *aen;
    register int minor_device, board_index, sap_index;
    aen_board_t *board;

    /*
    ** We aren't a module and no support for clone openning.
    */
    if (sflag == MODOPEN || sflag == CLONEOPEN)
	return EINVAL;

    aenautoconfig();

    /*
    ** Minor devices are handled as:
    **	bits 3-0	Board index, 0x0=first board 0xF=last board.
    **	bits 7-4	Index into sap table with 0 being a psuedo-
    **			clone device to choose the next sap available.
    **			(1=first sap)
    */

    minor_device = getminor(*devp);
    board_index = minor_device & 0x0F;
    sap_index = (minor_device & 0xF0) >> 4;
    board = &aen_board[board_index];

    if (board_index >= AEN_MAXBOARDS)
	return ENODEV;		/* bad index into boards */

    if (board->aen_status.board_state != BOARD_RUNNING)
    {
	if (!aen_initialize(board_index))
	    return ENODEV;
    }

    if (board->aen_info.board_base == 0)
	return ENXIO;		/* Couldn't setup lance */

    if (sap_index == 0)
    {
	/*
	** Psuedo Clone device code.
	*/

	for (sap_index = 0, aen = board->aen;
	     sap_index < AEN_MAXDEV;
	     sap_index++, aen++)
	{
	    if (!aen->q)
		break;
	}
    }
    else
	--sap_index;

    if (sap_index >= AEN_MAXDEV)
	return ENOSPC;

    *devp = makedevice(getmajor(*devp), (((sap_index+1) << 4) | board_index));

    aen = &board->aen[sap_index];

    if (aen->q)
	return EBUSY;

    /*
    ** setup streams pointer
    */

    q->q_ptr = (char *)aen;
    WR(q)->q_ptr = (char *)aen;
    aen->q = q;
    aen->board_index = board_index;
    aen->flags = 0;

    if (!board->ifstats.ifs_name)	/* have we attached ourselves? */
    {
	    board->ifstats.ifs_name = "aen";
	    board->ifstats.ifs_mtu = AEN_MTU;
	    board->ifstats.ifs_unit = board_index;
	    board->ifstats.ifs_next = ifstats;
	    ifstats = &board->ifstats;
    }
    board->ifstats.ifs_active = 1;

    return 0;
}

/*
** aenclose - close lance EtherNet device
*/
static
aenclose(q)
register queue_t *q;
{
    int dev;
    register aen_t *aen = (aen_t *)q->q_ptr;
    register aen_board_t *board = &aen_board[aen->board_index];

    aen->q = 0;
    
    OTHERQ(q)->q_ptr = NULL;
    q->q_ptr = NULL;
    
    /*
    ** If we are the last dev, shutdown the board
    */
    for (dev = 0, aen = board->aen; dev < AEN_MAXDEV; ++dev, ++aen)
    {
	if (aen->q)
	    break;
    }

    if (dev >= AEN_MAXDEV)
    {
	STRLOG(0x656e/*'en'*/, 0, 1, SL_TRACE,
	       "en[%d] Close called HALTING LANCE\n",
	       aen->board_index, *board->aen_info.lance_data);
	/*
	** Halt lance.
	*/

	board->aen_status.board_state = BOARD_RESET;
	*board->aen_info.lance_addr = RAP_CSR0;		/* Register 0 */
	*board->aen_info.lance_data = CSR0_STOP;	/* Halt lance */

	board->ifstats.ifs_unit = 0;
	board->ifstats.ifs_active = 0;
	board->ifstats.ifs_ipackets = 0;
	board->ifstats.ifs_ierrors = 0;
	board->ifstats.ifs_opackets = 0;
	board->ifstats.ifs_oerrors = 0;
    }

    STRLOG(0x656E, 0, 1, SL_TRACE, "en[%d] Closed\n",
	   *board->aen_info.lance_data);

    return 0;
}

/*
** aenwput -- comment goes here
*/
static
aenwput(q, mp)
register queue_t *q;
register mblk_t *mp; 
{
    register int s;
    register aen_t *aen = (aen_t *)q->q_ptr;
    register aen_board_t *board = &aen_board[aen->board_index];

    s = splaen();

    STRLOG(0x656E, 0, 8, SL_TRACE, "en[%d] wput called\n", aen->board_index);

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
	(void) aenproto(q, mp);
	break;

    case M_IOCDATA:
    case M_IOCTL:
	aenioctl(q, mp);
	break;

    default:
	STRLOG(0x656E, 0, 2, SL_ERROR|SL_WARN, "aen%d: bad msg type %d",
	       aen->board_index, mp->b_datap->db_type);
	freemsg(mp);
	break;
    }
    splx(s);
}

/*
** aenxmit - `xmit' the packet on the EtherNet board.
*/
static int
aenxmit(q, mp)
queue_t *q;
mblk_t *mp;
{
    register int i, j, n, buffer_length, sent_packet;
    register aen_t *aen = (aen_t *)q->q_ptr;
    aen_board_t *board = &aen_board[aen->board_index];
    aen_info_t *aen_info = &board->aen_info;
    volatile struct ringptr *txptr = aen_info->txptr;
    char *bufaddr;
    mblk_t *oldmp;

    i = aen_info->tx_buffer_index;

    /*
    ** <IMPLEMENT> Check for errors in tranmit buffers.
    */

    if (!(txptr[i].mode&TMD1_OWN))
    {
	dl_unitdata_req_t *dp = (dl_unitdata_req_t *)mp->b_rptr;
	unsigned char *ap;

	/*
	** The current buffer is free.
	*/

	bufaddr = (char *)aen_info->board_base + txptr[i].loaddr;

	/*
	** destination address
	*/
	ap = mp->b_rptr + dp->dl_dest_addr_offset;
	bcopy((caddr_t)ap, (caddr_t)bufaddr, sizeof(ether_addr_t));
	bufaddr += sizeof(ether_addr_t);

	/*
	** Source address
	*/
	bcopy((caddr_t)aen_info->paddress,
	      (caddr_t)bufaddr,
	      sizeof(ether_addr_t));
	bufaddr += sizeof(ether_addr_t);

	/*
	** EtherNet packet type.
	*/

	bcopy((caddr_t)&aen->sap, (caddr_t)bufaddr, sizeof(aen->sap));
	bufaddr += sizeof(aen->sap);
	
	buffer_length = ETH_MAXDATA;

	oldmp = mp;

	/*
	** Copy message block to lance buffer.
	*/

	for (mp = mp->b_cont; mp; mp = mp->b_cont)
	{
	    n = min(mp->b_wptr - mp->b_rptr, buffer_length);
	    ASSERT(n >= 0);
	    bcopy((caddr_t)mp->b_rptr, (caddr_t)bufaddr, n);
	    buffer_length -= n;
	    bufaddr += n;
	}

	/*
	** <IMPLEMENT>
	** pad packet to fix minimum spec.
	*/

	freemsg(oldmp);

	n = sizeof(ether_header_t) +
		max(ETH_MAXDATA-buffer_length, ETH_MINDATA);
	    
	txptr[i].length = -n;

#if 0
	{
	    unsigned char *ad =
		(unsigned char *) (aen_info->board_base + txptr[i].loaddr);

	    if (0x0806 == *((unsigned short *)(&ad[12])))
		printf("en[%d] Sending[%d]; len/type: %d/%x; From %x:%x:%x:%x:%x:%x To %x:%x:%x:%x:%x:%x\n",
		       aen->board_index, i, n, *((unsigned short *)(&ad[12])),
		       ad[6], ad[7], ad[8], ad[9], ad[10], ad[11], ad[0],
		       ad[1], ad[2], ad[3], ad[4], ad[5]);
	}
#endif

	txptr[i].mode = TMD1_ENP|TMD1_STP|TMD1_OWN;
	board->aen_status.packets_sent++;
	board->ifstats.ifs_opackets++;

	if (i < TXCOUNT-1)			/* end of ring ptr list? */
	    aen_info->tx_buffer_index++;
	else
	    aen_info->tx_buffer_index = 0;
	    
	STRLOG(0x656E, 0, 21, SL_TRACE,
	       "en[%d] queued: i=%d[%d], len=%d, mode=0x%x, next=0x%x\n",
	       aen->board_index, i, aen_info->tx_buffer_index,
	       -(txptr[i].length),
	       txptr[i].mode, txptr[aen_info->tx_buffer_index].mode);

#ifdef DEBUG1
	if (BUGGYBUTTON&DEBUG1)
	    hexdump((char *)(aen_info->board_base + txptr[i].loaddr),
		    (int)-(txptr[i].length), "Transmitting:\n");
#endif /* DEBUG1 */

	sent_packet = TRUE;
    }
    else
	sent_packet = FALSE;

    return sent_packet;
}
/*
** Dummy write service procedure for
** flow control only.
*/
static int aenwsrv() { return 0; }

/*
** aenioctl  -	process I/O control requests
*/
static void
aenioctl(q, mp)
queue_t *q;
mblk_t *mp;
{
    register struct iocblk *iocbp = (struct iocblk *)mp->b_rptr;
    register aen_t *aen = (aen_t *)q->q_ptr;
    register aen_board_t *board = &aen_board[aen->board_index];

    if (mp->b_datap->db_type == M_IOCDATA)
    {
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
    }

    STRLOG(0x656E, 0, 7, SL_TRACE,
	   "en[%d]: %s message, command 0x%x\n", aen->board_index,
	   (mp->b_datap->db_type == M_IOCDATA ? "IOCDATA" :
	    mp->b_datap->db_type == M_IOCTL ? "IOCTL" : "??"), iocbp->ioc_cmd);

    switch (iocbp->ioc_cmd)
    {
    case AEN_CLEAR_STATUS:
	board->aen_status.packets_sent = 0;
	board->aen_status.packets_received = 0;
	board->aen_status.allocbs_failed = 0;
	board->aen_status.couldnt_put = 0;
	board->aen_status.memory_error = 0;
	board->aen_status.miss_error = 0;
	board->aen_status.collision_error = 0;
	board->aen_status.babble_error = 0;
	board->aen_status.trailers = 0;
	board->aen_status.bad_start = 0;
	board->aen_status.buffer_error = 0;
	board->aen_status.overflow = 0;
	board->aen_status.framming = 0;
	board->aen_status.crc = 0;
	freemsg(mp->b_cont);
	break;

    case AEN_GET_STATUS:
    {
	aen_status_t *status;
	caddr_t arg = *(caddr_t *)mp->b_cont->b_rptr;

	freemsg(mp->b_cont);

	mp->b_cont = allocb(sizeof(aen_status_t), BPRI_MED);
	if (!mp->b_cont)
	{
	    mp->b_datap->db_type = M_IOCNAK;
	    freemsg(unlinkb(mp));
	    iocbp->ioc_count = 0;
	    iocbp->ioc_rval = 0;
	    iocbp->ioc_error = ENOMEM;
	    putnext(RD(q), mp);
	    return;
	}

	status = (aen_status_t *)mp->b_cont->b_rptr;
	mp->b_cont->b_wptr += sizeof(aen_status_t);

	bcopy((caddr_t)&board->aen_status, (caddr_t)status,
	      sizeof(aen_status_t));

	if (mp->b_datap->db_type == M_IOCTL && iocbp->ioc_count == TRANSPARENT)
	{
	    struct copyreq *creq = (struct copyreq *)mp->b_rptr;
	    mp->b_datap->db_type = M_COPYOUT;
	    creq->cq_addr = arg;
	    mp->b_wptr = mp->b_rptr + sizeof *creq;
	    mp->b_cont->b_wptr = mp->b_cont->b_rptr + sizeof(aen_status_t);
	    creq->cq_size = sizeof(aen_status_t);
	    creq->cq_flag = 0;
	    creq->cq_private = (mblk_t *)NULL;
	    putnext(RD(q), mp);
	    return;
	}

	break;
    }

    case AEN_GET_CONFIG:
    {
	aen_config_t *aen_config;
	caddr_t arg = *(caddr_t *)mp->b_cont->b_rptr;

	freemsg(mp->b_cont);

	mp->b_cont = allocb(sizeof(aen_config_t), BPRI_MED);
	if (!mp->b_cont)
	{
	    mp->b_datap->db_type = M_IOCNAK;
	    freemsg(unlinkb(mp));
	    iocbp->ioc_count = 0;
	    iocbp->ioc_rval = 0;
	    iocbp->ioc_error = ENOMEM;
	    putnext(RD(q), mp);
	    return;
	}

	aen_config = (aen_config_t *)mp->b_cont->b_rptr;
	mp->b_cont->b_wptr += sizeof(aen_config_t);

	aen_config->board_debug = board->aen_status.board_debug;
	aen_config->board_base = board->aen_info.board_base;
	aen_config->mode = board->aen_info.initblk->mode;
	aen_config->flags = aen->flags;
	bcopy((caddr_t)board->aen_info.paddress, (caddr_t)aen_config->paddress,
	      sizeof(board->aen_info.paddress));
	bcopy((caddr_t)board->aen_info.laddress, (caddr_t)aen_config->laddress,
	      sizeof(board->aen_info.laddress));

	if (mp->b_datap->db_type == M_IOCTL && iocbp->ioc_count == TRANSPARENT)
	{
	    struct copyreq *creq = (struct copyreq *)mp->b_rptr;
	    mp->b_datap->db_type = M_COPYOUT;
	    creq->cq_addr = arg;
	    mp->b_wptr = mp->b_rptr + sizeof *creq;
	    mp->b_cont->b_wptr = mp->b_cont->b_rptr + sizeof(aen_config_t);
	    creq->cq_size = sizeof(aen_config_t);
	    creq->cq_flag = 0;
	    creq->cq_private = (mblk_t *)NULL;
	    putnext(RD(q), mp);
	    return;
	}

	break;
    }

    case AEN_SET_CONFIG:
    {
	aen_config_t *aen_config;

	if (mp->b_datap->db_type == M_IOCTL && iocbp->ioc_count == TRANSPARENT)
	{
	    struct copyreq *creq = (struct copyreq *)mp->b_rptr;
	    mp->b_datap->db_type = M_COPYIN;
	    creq->cq_addr = *(caddr_t *)mp->b_cont->b_rptr;
	    mp->b_wptr = mp->b_rptr + sizeof(*creq);
	    creq->cq_size = sizeof(*aen_config);
	    creq->cq_flag = 0;
	    creq->cq_private = (mblk_t *)1;
	    putnext(RD(q), mp);
	    return;
	}
	else
	{
	    mblk_t *bp1;
	    register int i;

	    (void)pullupmsg(mp->b_cont, -1);

	    if (!mp->b_cont || blklen(mp->b_cont) != sizeof(*aen_config))
	    {
		iocbp->ioc_error = EINVAL;
		mp->b_datap->db_type = M_IOCNAK;
		iocbp->ioc_count = 0;
		putnext(RD(q), mp);
		break;
	    }

	    aen_config = (aen_config_t *)mp->b_cont->b_rptr;

	    board->aen_status.board_debug = aen_config->board_debug;

	    mp->b_datap->db_type = M_IOCACK;

	    aen->flags = aen_config->flags;

	    if (bcmp((caddr_t)board->aen_info.paddress,
		    (caddr_t)aen_config->paddress,
		     sizeof(board->aen_info.paddress)) ||
		bcmp((caddr_t)board->aen_info.laddress,
		     (caddr_t)aen_config->laddress,
		     sizeof(board->aen_info.laddress)) ||
		board->aen_info.initblk->mode != aen_config->mode)
	    {
		if (!setup_lance(aen->board_index, aen_config->paddress,
				 aen_config->laddress, aen_config->mode))
		    mp->b_datap->db_type = M_IOCNAK;
	    }

	    bp1 = unlinkb(mp);
	    if (bp1)
		freeb(bp1);
	    iocbp->ioc_count = 0;
	    putnext(RD(q), mp);

	    for (i=0; i < AEN_MAXDEV; i++)
	    {
		dl_bind_ack_t *bind_ackp;
		dl_info_ack_t *info_ackp;

		if (board->aen[i].sap && board->aen[i].q)
		{
		    if (!(mp=allocb(sizeof(*bind_ackp) +
				    sizeof(board->aen_info.paddress),
				    BPRI_MED)))
		    {
			/* <IMPLEMENT> BARF */
			return;
		    }

		    mp->b_datap->db_type = M_PCPROTO;
		    bind_ackp = (dl_bind_ack_t *)mp->b_wptr;
		    bind_ackp->dl_primitive = DL_BIND_ACK;
		    bind_ackp->dl_sap = aen->sap;
		    bind_ackp->dl_addr_length =
			sizeof(board->aen_info.paddress);
		    bind_ackp->dl_addr_offset = sizeof(*bind_ackp);
		    bind_ackp->dl_max_conind = 0;
		    bind_ackp->dl_growth = 0;
		    mp->b_wptr += sizeof(*bind_ackp);

		    bcopy((caddr_t)board->aen_info.paddress,
			  (caddr_t)mp->b_wptr,
			  sizeof(board->aen_info.paddress));

		    mp->b_wptr += sizeof(board->aen_info.paddress);

		    qreply(WR(board->aen[i].q), mp);

		    if (!(mp = allocb(sizeof(dl_info_ack_t) +
				      sizeof(board->aen_info.paddress),
				      BPRI_MED)))
		    {
			/* BARF HERE */
			return;
		    }

		    mp->b_datap->db_type = M_PCPROTO;
		    info_ackp = (dl_info_ack_t *)mp->b_wptr;
		    info_ackp->dl_primitive = DL_INFO_ACK;
		    info_ackp->dl_max_sdu = ETH_MAXDATA;
		    info_ackp->dl_min_sdu = 1;
		    info_ackp->dl_addr_length =
			sizeof(board->aen_info.paddress);
		    info_ackp->dl_mac_type = DL_ETHER;
		    info_ackp->dl_reserved = 0;
		    info_ackp->dl_current_state = aen->state;
		    info_ackp->dl_max_idu = ETH_MAXDATA;
		    info_ackp->dl_service_mode = DL_CLDLS;
		    info_ackp->dl_qos_length = 0;
		    info_ackp->dl_qos_offset = 0;
		    info_ackp->dl_qos_range_length = 0;
		    info_ackp->dl_qos_range_offset = 0;
		    info_ackp->dl_provider_style = DL_STYLE1;
		    info_ackp->dl_growth = 0;
		    if (aen->state == DL_IDLE)
		    {
			info_ackp->dl_addr_offset = sizeof(dl_info_ack_t);
			mp->b_wptr += sizeof(dl_info_ack_t);
			bcopy((caddr_t)board->aen_info.paddress,
			      (caddr_t)mp->b_wptr,
			      sizeof(board->aen_info.paddress));
			mp->b_wptr += sizeof(board->aen_info.paddress);
		    }
		    else
		    {
			info_ackp->dl_addr_offset = 0;
			mp->b_wptr += sizeof(dl_info_ack_t);
		    }

		    qreply(WR(board->aen[i].q), mp);
		}
	    }
	}
	break;
    }

    case AEN_NUMBER_OF_BOARDS:
    {
	caddr_t arg = *(caddr_t *)mp->b_cont->b_rptr;

	freemsg(mp->b_cont);

	mp->b_cont = allocb(sizeof(aen_number_of_boards), BPRI_MED);
	if (!mp->b_cont)
	{
	    mp->b_datap->db_type = M_IOCNAK;
	    freemsg(unlinkb(mp));
	    iocbp->ioc_count = 0;
	    iocbp->ioc_rval = 0;
	    iocbp->ioc_error = ENOMEM;
	    putnext(RD(q), mp);
	    return;
	}

	mp->b_cont->b_wptr += sizeof(int);

	*(int *)mp->b_cont->b_rptr = aen_number_of_boards;

	if (mp->b_datap->db_type == M_IOCTL && iocbp->ioc_count == TRANSPARENT)
	{
	    struct copyreq *creq = (struct copyreq *)mp->b_rptr;
	    mp->b_datap->db_type = M_COPYOUT;
	    creq->cq_addr = arg;
	    mp->b_wptr = mp->b_rptr + sizeof *creq;
	    mp->b_cont->b_wptr = mp->b_cont->b_rptr + sizeof(int);
	    creq->cq_size = sizeof(int);
	    creq->cq_flag = 0;
	    creq->cq_private = (mblk_t *)NULL;
	    putnext(RD(q), mp);
	    return;
	}

	break;
    }

    case SIOCSIFFLAGS:
    case SIOCGIFFLAGS:
	mp->b_datap->db_type = M_IOCACK;
	putnext(RD(q), mp);
	return;

    case IF_UNITSEL:			/* Handle these someday */
    default:
	mp->b_datap->db_type = M_IOCNAK;
	freemsg(unlinkb(mp));
	iocbp->ioc_count = 0;
	iocbp->ioc_rval = 0;
	iocbp->ioc_error = EINVAL;
	putnext(RD(q), mp);
	return;
    }

    mp->b_datap->db_type = M_IOCACK;
    freemsg(unlinkb(mp));
    iocbp->ioc_count = 0;
    iocbp->ioc_rval = 0;
    iocbp->ioc_error = 0;
    putnext(RD(q), mp);
    return;
}

/*
** aenproto - handle requests from IP
*/
static int
aenproto(q, mp)
queue_t *q;
mblk_t *mp;
{
    register union DL_primitives *p = (union DL_primitives *)mp->b_rptr;
    register aen_t *aen = (aen_t *)q->q_ptr;
    register aen_board_t *board = &aen_board[aen->board_index];

    STRLOG(0x656E, 0, 8, SL_TRACE, "en[%d] proto called (%d)\n",
	   aen->board_index, p->dl_primitive);

    switch (p->dl_primitive)
    {
    case DL_UNITDATA_REQ:
	if (q->q_first || !aenxmit(q, mp))
	    putq(q, mp);
	break;

    case DL_BIND_REQ:
	{
	    dl_bind_req_t *reqp = (dl_bind_req_t *)mp->b_rptr;
	    dl_bind_ack_t *ackp;

	    aen->sap = reqp->dl_sap;
	    aen->state = DL_IDLE;

	    STRLOG(0x656E, 0, 8, SL_TRACE, "en[%d] Setting sap to 0x%x\n",
		   aen->board_index, aen->sap);

	    /* bind o.k., return dl_bind_ack_t */

	    freemsg(mp);

	    if (!(mp=allocb(sizeof(*ackp)+sizeof(board->aen_info.paddress),
			    BPRI_MED)))
	    {
		/* <IMPLEMENT> BARF */
		return 1;
	    }

	    mp->b_datap->db_type = M_PCPROTO;
	    ackp = (dl_bind_ack_t *)mp->b_wptr;
	    ackp->dl_primitive = DL_BIND_ACK;
	    ackp->dl_sap = aen->sap;
	    ackp->dl_addr_length = sizeof(board->aen_info.paddress);
	    ackp->dl_addr_offset = sizeof(*ackp);
	    ackp->dl_max_conind = 0;
	    ackp->dl_growth = 0;
	    mp->b_wptr += sizeof(*ackp);

	    bcopy((caddr_t)board->aen_info.paddress, (caddr_t)mp->b_wptr,
		  sizeof(board->aen_info.paddress));

	    mp->b_wptr += sizeof(board->aen_info.paddress);

	    qreply(q, mp);

	    return 1;
	}

    case DL_INFO_REQ:
	{
	    dl_info_ack_t *ackp;

	    STRLOG(0x656E, 0, 8, SL_TRACE, "en[%d] Info Request, sap=0x%x\n",
		   aen->board_index, aen->sap);

	    freemsg(mp);

	    if (!(mp = allocb(sizeof(dl_info_ack_t) +
			      sizeof(board->aen_info.paddress), BPRI_MED)))
	    {
		/* BARF HERE */
		return(1);
	    }

	    mp->b_datap->db_type = M_PCPROTO;
	    ackp = (dl_info_ack_t *)mp->b_wptr;
	    ackp->dl_primitive = DL_INFO_ACK;
	    ackp->dl_max_sdu = ETH_MAXDATA;
	    ackp->dl_min_sdu = 1;
	    ackp->dl_addr_length = sizeof(board->aen_info.paddress);
	    ackp->dl_mac_type = DL_ETHER;
	    ackp->dl_reserved = 0;
	    ackp->dl_current_state = aen->state;
	    ackp->dl_max_idu = ETH_MAXDATA;
	    ackp->dl_service_mode = DL_CLDLS;
	    ackp->dl_qos_length = 0;
	    ackp->dl_qos_offset = 0;
	    ackp->dl_qos_range_length = 0;
	    ackp->dl_qos_range_offset = 0;
	    ackp->dl_provider_style = DL_STYLE1;
	    ackp->dl_growth = 0;
	    if (aen->state == DL_IDLE)
	    {
		ackp->dl_addr_offset = sizeof(dl_info_ack_t);
		mp->b_wptr += sizeof(dl_info_ack_t);
		bcopy((caddr_t)board->aen_info.paddress,
		      (caddr_t)mp->b_wptr,
		      sizeof(board->aen_info.paddress));
		mp->b_wptr += sizeof(board->aen_info.paddress);
	    }
	    else
	    {
		ackp->dl_addr_offset = 0;
		mp->b_wptr += sizeof(dl_info_ack_t);
	    }

	    qreply(q, mp);
	    return 1;
	}

    case DL_UNBIND_REQ:
	{
	    dl_ok_ack_t *okp;

	    STRLOG(0x656E, 0, 8, SL_TRACE, "en[%d] Unbind Request, sap=0x%x\n",
		   aen->board_index, aen->sap);

	    freemsg(mp);

	    /* unbind o.k., return dl_ok_ack_t */

	    if (!(mp = allocb(sizeof(dl_ok_ack_t), BPRI_MED)))
	    {
		/* BARF HERE */
		return 1;
	    }

	    okp = (dl_ok_ack_t *)mp->b_wptr;
	    mp->b_datap->db_type = M_PCPROTO;
	    okp->dl_primitive = DL_OK_ACK;
	    okp->dl_correct_primitive = DL_UNBIND_REQ;
	    mp->b_wptr += sizeof(dl_ok_ack_t);

	    aen->state = DL_UNBOUND;
	    aen->sap = 0;

	    qreply(q, mp);

	    return 1;
	}

    default:
	STRLOG(0x656E, 0, 2, SL_ERROR|SL_WARN,
	       "aenproto: unknown proto request: %d", (int)p->dl_primitive);
	freemsg(mp);
	return(1);
    }

    return 0;
}

/*
** Our interrupt routine.
*/
aenintr()
{
    register unsigned short status;
    int board_index;

    if (panicstr)			/* System panic? */
	return;

    for (board_index = 0; board_index < AEN_MAXBOARDS; ++board_index)
    {
	aen_board_t *board = &aen_board[board_index];

	/*
	** If this board is active check to see if the interrupt was
	** from it.
	*/

	if (board->aen_status.board_state == BOARD_RUNNING)
	{
	    *board->aen_info.lance_addr = RAP_CSR0; /* Status register 0 */
	    status = *board->aen_info.lance_data;
    
	    if (status&CSR0_INTR)
	    {
		*board->aen_info.lance_data = status;

		STRLOG(0x656E, 0, 6, SL_TRACE, "en[%d] INT; status = %x\n",
		       board_index, status);

		/*
		** Handle receive interrupts.
		*/

		if (status&CSR0_RINT)
		{
		    STRLOG(0x656E, 0, 4, SL_TRACE, "en[%d] RINT\n",
			   board_index);
		    receive_interrupt(board_index);
		}
		/*
		** Handle transmit interrupts.
		*/
		if (status&CSR0_TINT)
		{
		    transmit_interrupt(board_index);
		}
	    }

	    /*
	    ** Keep track of any errors that occurred.
	    */

	    if (status&CSR0_ERR)
	    {
		*board->aen_info.lance_data = status;

		if (status&CSR0_MERR)
		    ++board->aen_status.memory_error;

		if (status&CSR0_MISS)
		    ++board->aen_status.miss_error;

		if (status&CSR0_CERR)
		    ++board->aen_status.collision_error;

		if (status&CSR0_BABL)
		    ++board->aen_status.babble_error;

		board->ifstats.ifs_ierrors++;
		if (status&CSR0_CERR)
		    board->ifstats.ifs_collisions++;
	    }

	}
	else if (board->aen_status.board_state == BOARD_IN_INIT_CODE)
	{
	    initialize_and_go(board_index);
	}
    }
}

static void
receive_interrupt(board_index)
int board_index;
{
    int i, j, count;
    aen_board_t *board = &aen_board[board_index];
    aen_info_t *aen_info = &board->aen_info;
    volatile struct ringptr *rxptr = aen_info->rxptr;
    static char rxbuff[MAX_BUFFER_LENGTH];
    int rxtotsize;
    char *rb;

    board->aen_status.packets_received++;
    board->ifstats.ifs_ipackets;

    i = aen_info->rx_buffer_index;

    while (!(rxptr[i].mode&RMD1_OWN))
    {
	/*
	** Make sure we start with START packet.
	*/

	if (!rxptr[i].mode&RMD1_STP)
	{
	    /*
	    ** mark buffer as lance owned.
	    */

	    rxptr[i++].mode = RMD1_OWN,
	    i %= RXCOUNT;
	    aen_info->rx_buffer_index=i;
	    board->aen_status.bad_start++;
	    continue;
	}

	/*
	** Check to see we have a full packet
	** <CHECK> What happens if we receive another start packet
	** <CHECK> Before receiving an end packet?  Can this ever
	** <CHECK> Occur?
	*/

	for (j=i; !(rxptr[j].mode&RMD1_OWN); j++, j %= RXCOUNT)
	{
	    if (rxptr[j].mode&RMD1_ENP)
		break;
	}

	if (rxptr[j].mode&RMD1_OWN || !(rxptr[j].mode&RMD1_ENP))
	{
	    /*
	    ** A Full packet has not been received yet.
	    */
	    break;
	}

	j++;
	j %= RXCOUNT;

	/*
	** i points to the first packet.
	** j points to the last packet + 1.
	**
	** Copy Received packet to temporary ethernet buffer.
	**
	** keep track of errors.
	*/

	for (rb = rxbuff,rxtotsize=0;
	     i != j;
	     rxptr[i++].mode = RMD1_OWN, i %= RXCOUNT,
	     aen_info->rx_buffer_index=i)
	{
	    if (rxptr[i].mode&RMD1_ERR)
	    {
		rb = NULL;
		if (rxptr[i].mode&RMD1_BUFF)
		{
		    /* buffer error */
		    board->aen_status.buffer_error++;
		}
		if (!rxptr[i].mode&RMD1_ENP && rxptr[i].mode&RMD1_OFLO)
		{
		    /* Overflow error */
		    board->aen_status.overflow++;
		}
		else if (!rxptr[i].mode&RMD1_OFLO)
		{
		    if (rxptr[i].mode&RMD1_FRAM)
			board->aen_status.framming++;
		    if (rxptr[i].mode&RMD1_CRC)
			board->aen_status.crc++;
		}
		continue;
	    }

	    if (rb)
	    {
		if (rxptr[i].mode & RMD1_ENP)
		    count = (int)rxptr[i].count - rxtotsize;
		else
		    count = (int)(-rxptr[i].length);

		rxtotsize += count;
		    
		/* If someone is hosing around sending extra long packets
		** we'll get rid of them here
		** change made by Rick Schaeffer 4/3/92
		*/
		if (rxtotsize > MAX_BUFFER_LENGTH) {
#ifdef	DEBUG
		    printf("Tossing oversize packet, %d bytes\n", rxtotsize);
#endif
		    board->aen_status.buffer_error++;
		    rb = NULL;
		    continue;
		}
		bcopy((caddr_t) (aen_info->board_base + rxptr[i].loaddr),
		      (caddr_t)rb,
		      count);

		rb += count;

		if ((rxptr[i].mode&RMD1_ENP))
		{
		    count = (int)rxptr[i].count;
		}

	    }
	}

	if (!rb)
	{
	    /*
	    ** Some packet error occured.  Try next packet.
	    */
	    continue;
	}
	if (aen_packet_hook)
	{
	    if (aen_packet_hook((ether_header_t *)rxbuff, board_index))
		continue;
	}

	/*
	** Toss packets up stream according to the ethernet type
	** (which is called SAP for our interface purposes)
	*/
	toss_packet_up_stream(rxbuff, board_index, count);
    }
}

static void
toss_packet_up_stream(packet, board_index, count)
char *packet;
int board_index, count;
{
    int j, length, trailn;
    unsigned short sap;
    aen_board_t *board = &aen_board[board_index];
    aen_info_t *aen_info = &board->aen_info;
    volatile struct ringptr *rxptr = aen_info->rxptr;
    aen_t *aen = board->aen;
    static unsigned char broadcast_address[6] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    };
    ether_header_t *packet_header = (ether_header_t *)packet;

    sap = packet_header->ether_type;

    /* Dude! I'm seeing trails.... */
    if (sap >= ETHERTYPE_TRAIL &&
	sap <= (unsigned short)(ETHERTYPE_TRAIL+ETHERTYPE_NTRAILER))
    {
	/*
	** Ethernet trailers are types (inclusive)
	**
	**  ETHERTYPE_TRAIL to (ETHERTYPE_TRAIL+ETHERTYPE_NTRAILER)
	**
	** where ((Ether->et_Type - ETHERTYPE_TRAIL) * 512) is the
	** number of bytes of data in this packet followed by the
	** actual Ethernet TYPE followed by a revised length
	** (including the new EtherNet TYPE and LENGTH) then the
	** variable header (eg: ip, arp, or ?).
	*/

	length = (sap - ETHERTYPE_TRAIL) * 512;
	trailn = (*((unsigned short *)
		    &packet[sizeof(ether_header_t) + length + ETH_TYPE_SIZE]) -
		  ETH_TYPE_SIZE - ETH_LENGTH_SIZE);

	sap = *((unsigned short *)&packet[sizeof(ether_header_t)+length]);
	board->aen_status.trailers++;
    }
    else
    {
	length = count - ETH_CRC_LEN;
	trailn = 0;
    }

#ifdef DEBUG2
    if (BUGGYBUTTON&DEBUG2)
    {
	unsigned char *ad = (unsigned char *)packet;

	if (!(ad[0] == 0xFF && ad[1] == 0xFF &&
	      ad[2] == 0xFF && ad[3] == 0xFF &&
	      ad[4] == 0xFF && ad[5] == 0xFF))
	{
	    hexdump((char *)packet, length + trailn, "Receiving Packet:\n");

	    if (trailn)
		hexdump((char *) &packet[sizeof(ether_header_t) + length +
					 ETH_TYPE_SIZE + ETH_LENGTH_SIZE],
			trailn - ETH_TYPE_SIZE - ETH_LENGTH_SIZE,
			"Receiving Trailer:\n");
	}
    }
#endif /* DEBUG2 */

    for (j=0; j < AEN_MAXDEV; j++, aen++)
    {
	mblk_t *mp = (mblk_t *)NULL;

	if (!aen->q)
	    continue;

	if (aen->flags&AEN_RAW_DATA)
	{
	    if (canput(aen->q->q_next))
	    {
		if ((mp = allocb(sizeof(aen_status_t), BPRI_MED)) != NULL &&
		    (mp->b_cont = allocb(count, BPRI_MED)) != NULL)
		{
		    mp->b_datap->db_type = M_PROTO;
		    bcopy((caddr_t)&board->aen_status,
			  (caddr_t)mp->b_wptr, sizeof(board->aen_status));
		    mp->b_wptr +=  sizeof(board->aen_status);

		    mp->b_cont->b_datap->db_type = M_DATA;
		    bcopy((caddr_t)packet, (caddr_t)mp->b_cont->b_wptr, count);
		    mp->b_cont->b_wptr += count;
		    putnext(aen->q, mp);
		}
		else
		{
		    board->aen_status.allocbs_failed++;
		    if (mp)
			freemsg(mp);
		}
	    }
	    else
		board->aen_status.couldnt_put++;
	}
	else if (aen->sap == sap)
	{
	    if (aen_info->initblk->mode&MODE_PROM)
	    {
		/*
		** The board is in promiscious mode but this stream
		** is not so check the destination address against
		** ours.
		*/

		if (bcmp((char *)packet_header->ether_dhost,
			 (char *)aen_info->paddress,
			 sizeof(ether_addr_t)) &&
		    bcmp((char *)packet_header->ether_dhost,
			 (char *)broadcast_address,
			 sizeof(broadcast_address)))
		    continue;
	    }

	    if (canput(aen->q->q_next))
	    {
		if ((mp=allocb(sizeof(union DL_primitives) +
			       (2 * sizeof(ether_addr_t)),
			       BPRI_MED)) != NULL &&
		    (mp->b_cont = allocb(length + trailn, BPRI_MED)) != NULL)
		{
		    register dl_unitdata_ind_t
			*dp = (dl_unitdata_ind_t *)mp->b_wptr;

		    dp->dl_primitive = DL_UNITDATA_IND;
		    dp->dl_dest_addr_length = sizeof(ether_addr_t);
		    dp->dl_dest_addr_offset = sizeof(union DL_primitives);
		    dp->dl_src_addr_length = sizeof(ether_addr_t);
		    dp->dl_src_addr_offset = sizeof(union DL_primitives) +
			sizeof(ether_addr_t);

		    /*
		    ** Fill in destination address in dlpi header
		    */
		    bcopy((caddr_t)&packet_header->ether_dhost,
			  (caddr_t)mp->b_wptr + dp->dl_dest_addr_offset,
			  sizeof(ether_addr_t));

		    /* fill in source address address */
		    bcopy((caddr_t)&packet_header->ether_shost,
			  (caddr_t)mp->b_wptr + dp->dl_src_addr_offset,
			  sizeof(ether_addr_t));

		    dp->dl_reserved = 0;
		    mp->b_wptr += sizeof(union DL_primitives) +
			(2 * sizeof(ether_addr_t));

		    mp->b_datap->db_type = M_PROTO;

		    if (trailn)
		    {
			bcopy((caddr_t)
			      &packet[sizeof(ether_header_t) + length +
				      ETH_TYPE_SIZE + ETH_LENGTH_SIZE],
			      (caddr_t)mp->b_cont->b_wptr,
			      trailn);

			mp->b_cont->b_wptr += trailn;
		    }

		    bcopy((caddr_t)&packet[sizeof(ether_header_t)],
			  (caddr_t)mp->b_cont->b_wptr,
			  length);

		    mp->b_cont->b_wptr += length;
		    mp->b_cont->b_datap->db_type = M_DATA;

		    putnext(aen->q, mp);
		}
		else
		{
		    board->aen_status.allocbs_failed++;
		    if (mp)
			freemsg(mp);
		}
	    }
	    else
		board->aen_status.couldnt_put++;
	}
    }
}

static void
transmit_interrupt(board_index)
int board_index;
{
    aen_t *aen = aen_board[board_index].aen;
    int n;
    mblk_t *mp = (mblk_t *)NULL;

    for (n = 0; n < AEN_MAXDEV; n++, aen++)
    {
	if (aen->q)
	{
	    queue_t *wrq = WR(aen->q);

	    mp = getq(wrq);

	    if (mp && !aenxmit(wrq, mp))
	    {
		putbq(wrq, mp);
		break;			/* No more xmitting for now */
	    }
	}
    }
}

/*
** get_auto_serial_number() - get the serial number from board pointed
** to by BASE (the base address of the board [gotten from autocon()])
** and use it as the physical EtherNet address (with some modifications).
** The first 3 octets of the EtherNet address are passed in.  They are
** assigned by IEEE for each manufacturer.
**
** octets 0-2 are set to 008010 (the IEEE manufacturer's code for Commodore).
** octets 0-2 are set to 00009F (the IEEE manufacturer's code for Amerstar).
**
** octets 3-5 are set to the corresponding byte of the serial number.
**
*/

#define SERIAL_NUMBER_OFFSET_FROM_BASE	0x18

static
void
get_auto_serial_number(base, physical_ethernet_address, octet0, octet1, octet2)
long base;
unsigned short physical_ethernet_address[3];
unsigned char octet0, octet1, octet2;
{
    register unsigned short *ptr;

    ptr = (unsigned short *)(base + SERIAL_NUMBER_OFFSET_FROM_BASE);

#define addr ((unsigned char *)physical_ethernet_address)

    addr[0] = octet0;
    addr[1] = octet1;
    addr[2] = octet2;
    addr[3] = (((~ptr[2]&0xF000) >>  8) | ((~ptr[3]&0xF000) >> 12));
    addr[4] = (((~ptr[4]&0xF000) >>  8) | ((~ptr[5]&0xF000) >> 12));
    addr[5] = (((~ptr[6]&0xF000) >>  8) | ((~ptr[7]&0xF000) >> 12));

#undef addr
}

void
aenautoconfig()
{
    register long y;
    register int i, w, x, z, hit;
    long dummy;
    static int aenautoconfigured;

    if (aenautoconfigured)
	return;

    aenautoconfigured = TRUE;

    /*
    ** autocon(mannum, which, base_address, memory)
    ** mannum = The manufactures number.
    ** which = Which board to return (ie if which==0 return the first board
    **         with the mannum equal to MANNUM
    ** base_address = Where does this thing start.
    ** memory = How much memory does it use.
    */

    for (i=0; i < AEN_MAXBOARDS; ++i)
	if (autocon(AEN_BOARD, i, &aen_autoconfig[i].address, &dummy))
	    aen_autoconfig[i].type = AEN_BOARD;
	else
	    break;

    for (z=0; i+z < AEN_MAXBOARDS; ++z)
	if (autocon(AMBOARD, z, &aen_autoconfig[i+z].address, &dummy))
	    aen_autoconfig[i+z].type = AMBOARD;
	else
	    break;

    i += z;

    aen_number_of_boards = i;

    /*
    ** Sort the entries.
    */

    for (z=0; z < i-1; ++z)
    {
	int hit=FALSE;

	for (x=0; x < i-1; ++x)
	    if (aen_autoconfig[x].address > aen_autoconfig[x+1].address)
	    {
		hit = TRUE;

		y = aen_autoconfig[x].address;
		aen_autoconfig[x].address = aen_autoconfig[x+1].address;
		aen_autoconfig[x+1].address = y;

		w = aen_autoconfig[x].type;
		aen_autoconfig[x].type = aen_autoconfig[x+1].type;
		aen_autoconfig[x+1].type = w;
	    }

	if (!hit)
	    break;
    }
}

/*
** Initialize lance.
*/
int
aen_initialize(board_index)
int board_index;
{
    int n;
    aen_t *aen;
    unsigned char paddress[6];	/* Physical address */
    unsigned char laddress[8];	/* Logical address */
    register aen_board_t *board = &aen_board[board_index];

    bzero((caddr_t)laddress, sizeof(laddress));

#ifdef DEBUG0
    board->aen_status.board_debug = DEFAULT_DEBUG;
#endif /* DEBUG0 */

    if ((board->aen_info.board_base=aen_autoconfig[board_index].address) != 0)
    {
	if (aen_autoconfig[board_index].type == AMBOARD)
	    get_auto_serial_number(board->aen_info.board_base,
				   paddress,
				   (unsigned char)0x00,
				   (unsigned char)0x00,
				   (unsigned char)0x9F);
	else
	    get_auto_serial_number(board->aen_info.board_base,
				   paddress,
				   (unsigned char)0x00,
				   (unsigned char)0x80,
				   (unsigned char)0x10);
    }
    else
    {
	STRLOG(0x656E, 0, 14, SL_TRACE, "aen: No ethernet board for index %d",
	       board_index);
	return 0;
    }

    /*
    ** Initialize our aen structure.
    */

    for (n=0, aen = board->aen; n < AEN_MAXDEV; ++n, ++aen)
    {
	aen->q = (queue_t *)NULL;
	aen->sap = 0;
	aen->state = DL_UNBOUND;
    }

    return setup_lance(board_index, paddress, laddress, MODE);
}

/* ARGSUSED */
static int
setup_lance(board_index, ethernet_address, logical_address, mode)
int board_index;
unsigned char ethernet_address[6];		/* Physical address */
unsigned char logical_address[8];		/* Logical address mask */
unsigned short mode;
{
    unsigned short ptr;
    register int i, s;
    register aen_board_t *board = &aen_board[board_index];

    STRLOG(0x656E, 0, 14, SL_TRACE, "en[%d] Address: %x:%x:%x:%x:%x:%x\n",
	   board_index, ethernet_address[0], ethernet_address[1],
	   ethernet_address[2], ethernet_address[3], ethernet_address[4],
	   ethernet_address[5]);

    bcopy((caddr_t)ethernet_address, (caddr_t)board->aen_info.paddress,
	  sizeof(board->aen_info.paddress));

    board->aen_info.lance_base =
	(unsigned short *)(board->aen_info.board_base + 0x4000);
    board->aen_info.lance_data =
	(unsigned short *)(board->aen_info.lance_base + 0);
    board->aen_info.lance_addr =
	(unsigned short *)(board->aen_info.lance_base + 1);
    board->aen_info.lance_buff =
	(void *)(board->aen_info.board_base + 0x8000);

    board->aen_info.initblk =
	(struct initblk *)board->aen_info.lance_buff;

    /*
    ** Halt lance.
    */

    board->aen_status.board_state = BOARD_RESET;
    *board->aen_info.lance_addr = RAP_CSR0; /* Register 0 */
    *board->aen_info.lance_data = CSR0_STOP; /* Halt lance */

    /*
    ** Set byte swapping for 68k
    */

    *board->aen_info.lance_addr = RAP_CSR3; /* Register 3 */
    *board->aen_info.lance_data = CSR3_BSWP; /* do swapping */

    /*
    ** Set address for lance init block.
    */

    *board->aen_info.lance_addr = RAP_CSR1; /* Register 1 */
    *board->aen_info.lance_data =
	(unsigned short)(board->aen_info.initblk); /* low addr */
    *board->aen_info.lance_addr = RAP_CSR2; /* Register 2 */
    *board->aen_info.lance_data = (unsigned short)0; /* ignored high addr */

    /*
    ** Initialize lance init block.
    */

    board->aen_info.initblk->mode = mode; /* I'm not a slut. */

    board->aen_info.initblk->padr[0] = board->aen_info.paddress[1];
    board->aen_info.initblk->padr[1] = board->aen_info.paddress[0];
    board->aen_info.initblk->padr[2] = board->aen_info.paddress[3];
    board->aen_info.initblk->padr[3] = board->aen_info.paddress[2];
    board->aen_info.initblk->padr[4] = board->aen_info.paddress[5];
    board->aen_info.initblk->padr[5] = board->aen_info.paddress[4];

    bcopy((caddr_t)logical_address, (caddr_t)board->aen_info.initblk->ladrf,
	  sizeof(board->aen_info.initblk->ladrf));
    board->aen_info.rxptr = (struct ringptr *)(board->aen_info.initblk + 1);
    board->aen_info.txptr =
	(struct ringptr *)(board->aen_info.rxptr + RXCOUNT);
    board->aen_info.initblk->rxring[0] =
	(ulong)board->aen_info.rxptr - board->aen_info.board_base;
    board->aen_info.initblk->rxring[1] = RXRLEN<<13; /* top three bits */
    board->aen_info.initblk->txring[0] =
	(ulong)board->aen_info.txptr - board->aen_info.board_base;
    board->aen_info.initblk->txring[1] = TXRLEN<<13; /* top three bits */

    /*
    ** Clear all ring descriptor blocks.
    */

    /* Start at first pointers */
    board->aen_info.rx_buffer_index = board->aen_info.tx_buffer_index = 0;

    ptr = (unsigned short)
	((board->aen_info.txptr + TXCOUNT)-board->aen_info.board_base);
    for (i=0; i < RXCOUNT; ++i, ptr += RX_BUFFER_LENGTH) /* Rx ring buff */
    {
	board->aen_info.rxptr[i].loaddr = ptr;
	board->aen_info.rxptr[i].hiaddr = 0; /* Assume address<64k */
	board->aen_info.rxptr[i].length = -RX_BUFFER_LENGTH;
	board->aen_info.rxptr[i].count = 0; /* actually status */
	board->aen_info.rxptr[i].mode = RMD1_OWN;
    }

    for (i=0; i < TXCOUNT; ++i, ptr += TX_BUFFER_LENGTH) /* Tx ring buff */
    {
	board->aen_info.txptr[i].loaddr = ptr;
	board->aen_info.txptr[i].hiaddr = 0; /* Assume address<64k */
	board->aen_info.txptr[i].length = -TX_BUFFER_LENGTH;
	board->aen_info.txptr[i].count = 0;		/* actually status */
	board->aen_info.txptr[i].mode = 0;
    }

    /*
    ** Initialize lance.
    */

    STRLOG(0x656E, 0, 15, SL_TRACE, "en[%d] In Init CODE.\n", board_index);

    board->aen_status.board_state = BOARD_IN_INIT_CODE;

    *board->aen_info.lance_addr = RAP_CSR0; /* Register 0 */
    *board->aen_info.lance_data = CSR0_INIT|CSR0_INEA; /* Initialize lance */

    /*
    ** Wait for lance to initialize.
    */

    s = splaen();
    while (board->aen_status.board_state == BOARD_IN_INIT_CODE)
	sleep((caddr_t)&board->aen_status.board_state, AEN_INIT_PRI);
    splx(s);

    STRLOG(0x656E, 0, 15, SL_TRACE, "en[%d] Woke up.\n", board_index);

    if (board->aen_status.board_state != BOARD_RUNNING)
    {
	STRLOG(0x656E, 0, 2, SL_ERROR|SL_WARN|SL_NOTIFY,
	       "aen%d: Couldn't initialize ethernet board @0x%x data=0x%x\n",
	       board_index, board->aen_info.board_base,
	       *board->aen_info.lance_data);
	return 0;
    }

    *board->aen_info.lance_data = CSR0_STRT|CSR0_INEA; /* Start Lance */

    return 1;
}

static void
initialize_and_go(board_index)
int board_index;
{
    register aen_board_t *board = &aen_board[board_index];
    register unsigned short status;

    *board->aen_info.lance_addr = RAP_CSR0; /* Status register 0 */
	
    status = *board->aen_info.lance_data;
	
    if (status&CSR0_INTR)
    {
	*board->aen_info.lance_data = status;
	    
	if (status&CSR0_IDON)
	{
	    /*
	    ** Wakeup process waiting for Lance to initialize.
	    */

	    wakeup((caddr_t)&board->aen_status.board_state);
	    board->aen_status.board_state = BOARD_RUNNING;
	    board->aen_status.packets_sent = 0;
	    board->aen_status.packets_received = 0;
	    board->aen_status.allocbs_failed = 0;
	    board->aen_status.couldnt_put = 0;
	    board->aen_status.memory_error = 0;
	    board->aen_status.miss_error = 0;
	    board->aen_status.collision_error = 0;
	    board->aen_status.babble_error = 0;
	    board->aen_status.trailers = 0;
	    board->aen_status.bad_start = 0;
	    board->aen_status.buffer_error = 0;
	    board->aen_status.overflow = 0;
	    board->aen_status.framming = 0;
	    board->aen_status.crc = 0;
	}

	STRLOG(0x656E, 0, 1, SL_TRACE, "en[%d] Init; status = %x/%x\n",
	       board_index, status, *board->aen_info.lance_data);
    }
    else
    {
	STRLOG(0x656E, 0, 1, SL_TRACE,
	       "en[%d] INTR while Initing Lance; status = %x/%x\n",
	       board_index, status, *board->aen_info.lance_data);
    }
}

#ifdef DEBUG0
void
hexdump(buf, length, header)
char *buf, *header;
int length;
{
    int i;
    unsigned int hd_address = 0;
    char hd_text[17], *hdp, hd_addrtext[4], *hda;
    static char hd_spaces[]="                                                ";

    printf("%s", header);

    while (length)
    {
	hda = hd_addrtext;

	if (hd_address < 0x1000)
	    *hda++ = '0';

	if (hd_address < 0x100)
	    *hda++ = '0';

	if (hd_address < 0x10)
	    *hda++ = '0';

	*hda = '\0';
	printf("%s%x: ", hd_addrtext, hd_address);

	hdp = hd_text;
	for (i=0; i < 16 && length; ++i, --length, ++hd_address)
	{
	    register unsigned char c = *buf++;

	    *hdp++ = ((c < 20) || ((c > 0x7F) && (c < 0xa0))) ? '.' : c;

	    if (c < 0x10)
		printf("0%x ", c);
	    else
		printf("%x ", c);
	}

	if (i < 16)
	    printf("%s", &hd_spaces[(16*3) - ((16-i)*3)]);

	*hdp = '\0';
	printf("%s\n", hd_text);
    }
}
#endif /* DEBUG0 */
